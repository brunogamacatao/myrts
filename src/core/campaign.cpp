#include "campaign.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <set>

namespace myrts {

namespace {
/// `{"gold": 2000, ...}`: whole numbers >= 0 (which ids exist is checked when the match is built).
std::map<std::string, int> parse_amounts(const nlohmann::json& j) {
  std::map<std::string, int> out;
  for (const auto& [id, n] : j.items()) {
    if (!n.is_number() || n.get<double>() < 0) throw std::runtime_error("stock '" + id + "' must be a whole number >= 0");
    out[id] = static_cast<int>(n.get<double>());
  }
  return out;
}

void read_ai(const nlohmann::json& a, AiConfig& ai) {
  ai.start_delay = a.value("start_delay", ai.start_delay);
  ai.first_wave = a.value("first_wave", ai.first_wave);
  ai.wave_growth = a.value("wave_growth", ai.wave_growth);
  ai.desired_workers = a.value("desired_workers", ai.desired_workers);
  ai.desired_tankers = a.value("desired_tankers", ai.desired_tankers);
  ai.think_interval = a.value("think_interval", ai.think_interval);
  ai.farm_margin = a.value("farm_margin", ai.farm_margin);
}

Objective parse_objective(const nlohmann::json& j, bool victory) {
  static const std::set<std::string> victory_types = {"destroy_all", "destroy_building", "survive", "collect", "build"};
  static const std::set<std::string> defeat_types = {"lose_building", "time_limit"};
  Objective o;
  o.type = j.at("type").get<std::string>();
  if (!(victory ? victory_types : defeat_types).contains(o.type))
    throw std::runtime_error(std::string("unknown ") + (victory ? "victory" : "defeat") + " objective '" + o.type + "'");
  o.text = j.value("text", std::string{});
  o.building = j.value("building", std::string{});
  o.resource = j.value("resource", std::string{});
  o.amount = j.value("amount", j.value("count", 1));
  o.seconds = j.value("seconds", 0.0);
  const bool needs_building = o.type == "destroy_building" || o.type == "build" || o.type == "lose_building";
  if (needs_building && o.building.empty()) throw std::runtime_error("objective '" + o.type + "' needs a building");
  if (o.type == "collect" && (o.resource.empty() || o.amount < 1)) throw std::runtime_error("objective 'collect' needs a resource and an amount");
  if ((o.type == "survive" || o.type == "time_limit") && o.seconds <= 0) throw std::runtime_error("objective '" + o.type + "' needs seconds");
  return o;
}

Objectives parse_objectives(const nlohmann::json& j) {
  Objectives out;
  if (j.contains("victory"))
    for (const auto& o : j.at("victory")) out.victory.push_back(parse_objective(o, true));
  if (j.contains("defeat"))
    for (const auto& o : j.at("defeat")) out.defeat.push_back(parse_objective(o, false));
  return out;
}

std::map<std::string, AiConfig> parse_profiles(std::string_view text) {
  std::map<std::string, AiConfig> out;
  if (text.empty()) return out;
  const nlohmann::json doc = nlohmann::json::parse(text);
  for (const auto& [name, body] : doc.at("profiles").items()) {
    AiConfig ai;
    read_ai(body, ai);
    out[name] = ai;
  }
  return out;
}
}  // namespace


std::expected<Campaign, std::string> Campaign::parse(std::string_view json_text, std::string_view profiles_json) {
  using json = nlohmann::json;
  Campaign campaign;
  try {
    const auto profiles = parse_profiles(profiles_json);
    const json doc = json::parse(json_text);
    for (const json& l : doc.at("levels")) {
      Level level;
      level.name = l.at("name").get<std::string>();
      level.map = l.at("map").get<std::string>();
      level.music = l.value("music", std::string{});
      level.chapter = l.value("chapter", std::string{});
      level.kind = l.value("kind", level.kind);
      if (level.kind != "land" && level.kind != "air" && level.kind != "naval") throw std::runtime_error("unknown kind '" + level.kind + "'");
      level.player_faction = l.value("player_faction", level.player_faction);
      level.enemy_faction = l.value("enemy_faction", level.enemy_faction);
      level.briefing = l.value("briefing", std::string{});
      if (l.contains("hints")) level.hints = l.at("hints").get<std::vector<std::string>>();
      if (l.contains("allow")) level.allow = l.at("allow").get<std::vector<std::string>>();
      if (l.contains("player_stock")) {
        const json& s = l.at("player_stock");
        level.player_stock = parse_amounts(s);
      }
      if (l.contains("ai_profile")) {
        level.ai_profile = l.at("ai_profile").get<std::string>();
        const auto it = profiles.find(level.ai_profile);
        if (it == profiles.end()) throw std::runtime_error("level '" + level.name + "': unknown ai_profile '" + level.ai_profile + "'");
        level.ai = it->second;
      }
      if (l.contains("ai")) read_ai(l.at("ai"), level.ai);
      if (l.contains("objectives")) level.objectives = parse_objectives(l.at("objectives"));
      if (l.contains("more_opponents"))
        for (const json& o : l.at("more_opponents")) {
          OpponentSpec spec;
          spec.faction = o.value("faction", std::string{});
          if (o.contains("ai_profile")) {
            const auto it = profiles.find(o.at("ai_profile").get<std::string>());
            if (it == profiles.end()) throw std::runtime_error("unknown ai_profile '" + o.at("ai_profile").get<std::string>() + "'");
            spec.ai = it->second;
          }
          if (o.contains("ai")) read_ai(o.at("ai"), spec.ai);
          if (o.contains("stock")) spec.stock = parse_amounts(o.at("stock"));
          spec.workers = o.value("workers", spec.workers);
          level.more_opponents.push_back(std::move(spec));
        }
      if (l.contains("enemy_stock")) {
        const json& s = l.at("enemy_stock");
        level.enemy_stock = parse_amounts(s);
      }
      level.enemy_workers = l.value("enemy_workers", level.enemy_workers);
      campaign.levels_.push_back(std::move(level));
    }
  } catch (const std::exception& e) {
    return std::unexpected(std::string("invalid campaign: ") + e.what());
  }
  if (campaign.levels_.empty()) return std::unexpected("the campaign has no levels");
  return campaign;
}

Level with_default_sides(Level level, const Registry& registry) {
  const auto factions = registry.factions();
  if (level.player_faction.empty() && !factions.empty()) level.player_faction = factions[0].id;
  if (level.enemy_faction.empty() && !factions.empty()) level.enemy_faction = factions[factions.size() > 1 ? 1 : 0].id;
  return level;
}

Level play_as(const Level& original, const Registry& registry, std::string_view faction) {
  const Level level = with_default_sides(original, registry);
  if (faction.empty() || faction == level.player_faction) return level;
  const FactionDef* chosen = registry.faction(faction);
  if (!chosen) return level;
  Level out = level;
  out.enemy_faction = level.player_faction;
  out.player_faction = chosen->id;
  const auto it = chosen->translations.find(level.player_faction);
  if (it == chosen->translations.end()) return out;
  const Translation& t = it->second;
  out.allow.clear();
  for (const std::string& id : level.allow) {
    const auto to = t.ids.find(id);
    const std::vector<std::string> mapped = to == t.ids.end() ? std::vector<std::string>{id} : to->second;
    for (const std::string& m : mapped)
      if (std::ranges::find(out.allow, m) == out.allow.end()) out.allow.push_back(m);
  }
  out.briefing = translate_text(level.briefing, t);
  for (std::string& hint : out.hints) hint = translate_text(hint, t);
  return out;
}

std::expected<Campaign, std::string> Campaign::load_file(const std::string& path, const std::string& profiles_path) {
  std::ifstream f(path);
  if (!f) return std::unexpected("cannot open " + path);
  const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  std::string profiles;
  if (!profiles_path.empty()) {
    if (std::ifstream p(profiles_path); p) profiles.assign(std::istreambuf_iterator<char>(p), std::istreambuf_iterator<char>());
  }
  return parse(text, profiles);
}

Campaign Campaign::single(std::string map) {
  Campaign c;
  Level level;
  level.name = "Skirmish";
  level.map = std::move(map);
  c.levels_.push_back(std::move(level));
  return c;
}

}  // namespace myrts
