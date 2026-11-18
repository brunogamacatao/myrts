#include "game_rules.hpp"

#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <set>

namespace myrts {

namespace {
using nlohmann::json;

/// Reads `group.key` into `out` when present; checks it is a number of the right sign.
template <typename T>
void read(const json& doc, const char* group, const char* key, T& out, T min_allowed) {
  if (!doc.contains(group)) return;
  const json& g = doc.at(group);
  if (!g.contains(key)) return;
  if (!g.at(key).is_number()) throw std::runtime_error(std::string(group) + "." + key + " must be a number");
  const T v = g.at(key).get<T>();
  if (v < min_allowed)
    throw std::runtime_error(std::string(group) + "." + key + " must be at least " + std::to_string(min_allowed));
  out = v;
}

void reject_unknown(const json& doc) {
  static const std::set<std::string> groups = {"simulation", "combat", "repair", "vision", "match", "ai", "presentation", "fog"};
  static const std::set<std::string> keys[] = {
      {"lumber_per_tree", "max_queue", "scan_interval", "retry_delay"},
      {"min_damage", "attack_flash"},
      {"cost_share", "min_seconds"},
      {"min_unit_sight", "building_sight_base"},
      {"initial_workers", "mine_size", "gold_per_value_unit", "default_mine_gold"},
      {"farm_margin"},
      {"corpse_seconds", "corpse_fade_seconds", "max_corpses", "air_lift"}};
  static const char* names[] = {"simulation", "combat", "repair", "vision", "match", "ai", "presentation"};
  for (const auto& [k, v] : doc.items()) {
    if (k == "format_version") continue;
    if (!groups.contains(k)) throw std::runtime_error("unknown section \"" + k + "\"");
    if (k == "fog") continue;  // checked key by key by FogSettings
    for (std::size_t i = 0; i < 7; ++i) {
      if (k != names[i]) continue;
      for (const auto& [sub, _] : v.items())
        if (!keys[i].contains(sub)) throw std::runtime_error("unknown key \"" + k + "." + sub + "\"");
    }
  }
}
}  // namespace

std::expected<GameRules, std::string> GameRules::parse(const std::string& text) {
  try {
    const json doc = json::parse(text);
    reject_unknown(doc);
    GameRules r;
    read(doc, "simulation", "lumber_per_tree", r.lumber_per_tree, 1);
    read(doc, "simulation", "max_queue", r.max_queue, 1);
    read(doc, "simulation", "scan_interval", r.scan_interval, 0.01);
    read(doc, "simulation", "retry_delay", r.retry_delay, 0.01);
    read(doc, "combat", "min_damage", r.min_damage, 0);
    read(doc, "combat", "attack_flash", r.attack_flash, 0.0);
    read(doc, "repair", "cost_share", r.repair_cost_share, 0.0);
    read(doc, "repair", "min_seconds", r.repair_min_seconds, 0.01);
    read(doc, "vision", "min_unit_sight", r.min_unit_sight, 0);
    read(doc, "vision", "building_sight_base", r.building_sight_base, 0);
    read(doc, "match", "initial_workers", r.initial_workers, 0);
    read(doc, "match", "mine_size", r.mine_size, 1);
    read(doc, "match", "gold_per_value_unit", r.gold_per_value_unit, 1);
    read(doc, "match", "default_mine_gold", r.default_mine_gold, 1);
    read(doc, "ai", "farm_margin", r.ai_farm_margin, 0);
    read(doc, "presentation", "corpse_seconds", r.corpse_seconds, 0.0);
    read(doc, "presentation", "corpse_fade_seconds", r.corpse_fade_seconds, 0.01);
    read(doc, "presentation", "max_corpses", r.max_corpses, 0);
    read(doc, "presentation", "air_lift", r.air_lift, 0.0f);
    if (doc.contains("fog")) {
      const auto fog = FogSettings::with(r.fog, doc.at("fog").dump());
      if (!fog) throw std::runtime_error(fog.error());
      r.fog = *fog;
    }
    return r;
  } catch (const std::exception& e) {
    return std::unexpected(std::string("rules: ") + e.what());
  }
}

std::expected<GameRules, std::string> GameRules::load_file(const std::string& path) {
  std::ifstream f(path);
  if (!f) return std::unexpected("cannot open " + path);
  const auto r = parse(std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()));
  if (!r) return std::unexpected(path + ": " + r.error());
  return r;
}

}  // namespace myrts
