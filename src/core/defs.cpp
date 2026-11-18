#include "defs.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <set>

namespace myrts {

namespace {

using json = nlohmann::json;

/// `{"gold": 100, "lumber": 20}`: every key must be a resource of the game.
Cost parse_cost(const json& j, const ResourceTable& table) {
  Cost c;
  for (const auto& [key, value] : j.items()) {
    const ResourceId r = table.index(key);
    if (r == kNoResource) throw std::runtime_error("cost: unknown resource '" + key + "'");
    if (!value.is_number_integer() || value.get<int>() < 0) throw std::runtime_error("cost: '" + key + "' must be a whole number >= 0");
    c[r] = value.get<int>();
  }
  return c;
}

std::vector<std::string> parse_strings(const json& j, const char* key) {
  std::vector<std::string> out;
  if (j.contains(key))
    for (const auto& v : j.at(key)) out.push_back(v.get<std::string>());
  return out;
}

MoveClass parse_move_class(const std::string& s) {
  if (s == "land") return MoveClass::Land;
  if (s == "water") return MoveClass::Water;
  if (s == "air") return MoveClass::Air;
  throw std::runtime_error("unknown move_class '" + s + "' (expected land, water or air)");
}

Targeting parse_targeting(const std::string& s) {
  if (s == "none") return Targeting::None;
  if (s == "ground") return Targeting::Ground;
  if (s == "resource") return Targeting::Resource;
  if (s == "menu") return Targeting::Menu;
  throw std::runtime_error("unknown targeting '" + s + "'");
}

Placement parse_placement(const std::string& s) {
  if (s == "land") return Placement::Land;
  if (s == "coast") return Placement::Coast;
  if (s == "water") return Placement::Water;
  throw std::runtime_error("unknown placement '" + s + "' (expected land, coast or water)");
}

UnitDef parse_unit(const json& j, const ResourceTable& table) {
  UnitDef u;
  u.id = j.at("id").get<std::string>();
  u.name = j.at("name").get<std::string>();
  u.faction = j.value("faction", std::string{});
  u.level = j.value("level", 1);
  u.max_hp = j.at("hp").get<int>();
  u.move_speed = j.value("move_speed", 64.0);
  u.move_class = parse_move_class(j.value("move_class", std::string("land")));
  if (j.contains("stats")) {
    const auto& s = j.at("stats");
    u.stats = {s.value("armor", 0), s.value("damage_min", 0), s.value("damage_max", 0),
               s.value("range", 0), s.value("sight", 0),      s.value("speed", 0)};
  }
  if (j.contains("cost")) u.cost = parse_cost(j.at("cost"), table);
  u.train_seconds = j.value("train_seconds", 0.0);
  u.food = j.value("food", u.food);
  u.icon = j.value("icon", -1);
  if (u.food < 0) throw std::runtime_error("unit '" + u.id + "': food must be >= 0");
  u.attack_seconds = j.value("attack_seconds", u.attack_seconds);
  u.siege_multiplier = j.value("siege_multiplier", u.siege_multiplier);
  if (u.attack_seconds <= 0) throw std::runtime_error("unit '" + u.id + "': attack_seconds must be > 0");
  u.sprite = j.at("sprite").get<std::string>();
  u.projectile = j.value("projectile", std::string{});
  u.mechanical = j.value("mechanical", false);
  u.capacity = j.value("capacity", 0);
  u.commands = parse_strings(j, "commands");
  u.builds = parse_strings(j, "builds");
  u.requirements = parse_strings(j, "requires");
  if (j.contains("worker")) {
    const auto& w = j.at("worker");
    WorkerDef def;
    if (w.contains("gather"))
      for (const auto& [key, g] : w.at("gather").items()) {
        const ResourceId r = table.index(key);
        if (r == kNoResource) throw std::runtime_error("unit '" + u.id + "': gathers unknown resource '" + key + "'");
        GatherSpec spec{g.value("per_trip", 100), g.value("seconds", 2.5)};
        if (spec.per_trip < 1 || spec.seconds < 0) throw std::runtime_error("unit '" + u.id + "': bad gather values for '" + key + "'");
        def.gather[static_cast<std::size_t>(r)] = spec;
      }
    u.worker = def;
  }
  u.aggressive = j.value("aggressive", !u.worker.has_value());  // workers keep working unless told to fight
  return u;
}

BuildingDef parse_building(const json& j, const ResourceTable& table) {
  BuildingDef b;
  b.id = j.at("id").get<std::string>();
  b.name = j.at("name").get<std::string>();
  b.faction = j.value("faction", std::string{});
  b.size = j.at("size").get<int>();
  if (b.size < 1) throw std::runtime_error("building '" + b.id + "': size must be >= 1");
  b.cost = parse_cost(j.value("cost", json::object()), table);
  b.build_seconds = j.at("build_seconds").get<double>();
  b.hp = j.value("hp", b.hp);
  b.armor = j.value("armor", 0);
  b.food = j.value("food", 0);
  b.icon = j.value("icon", -1);
  if (b.hp < 1) throw std::runtime_error("building '" + b.id + "': hp must be >= 1");
  for (const std::string& id : parse_strings(j, "accepts")) {
    const ResourceId r = table.index(id);
    if (r == kNoResource) throw std::runtime_error("building '" + b.id + "': accepts unknown resource '" + id + "'");
    b.accepts[static_cast<std::size_t>(r)] = true;
  }
  b.placement = parse_placement(j.value("placement", std::string("land")));
  b.sprite = j.at("sprite").get<std::string>();
  b.sprite_construction = j.value("sprite_construction", b.sprite);
  b.trains = parse_strings(j, "trains");
  b.requirements = parse_strings(j, "requires");
  return b;
}

CommandDef parse_command(const json& j) {
  CommandDef c;
  c.id = j.at("id").get<std::string>();
  c.label = j.value("label", c.id);
  c.order = j.value("order", c.id);
  c.prompt = j.value("prompt", std::string{});
  if (std::ranges::find(kBuiltinOrders, c.order) == std::end(kBuiltinOrders))
    throw std::runtime_error("command '" + c.id + "': unknown order '" + c.order + "'");
  c.icon = j.value("icon", c.id);
  c.targeting = parse_targeting(j.value("targeting", std::string("none")));
  const std::string hotkey = j.value("hotkey", std::string{});
  c.hotkey = hotkey.empty() ? '\0' : static_cast<char>(std::toupper(static_cast<unsigned char>(hotkey[0])));
  c.implemented = j.value("implemented", true);
  if (j.contains("icons"))
    for (const auto& [faction, index] : j.at("icons").items()) c.icons[faction] = index.get<int>();
  return c;
}

FactionDef parse_faction(const json& j) {
  FactionDef f;
  f.id = j.at("id").get<std::string>();
  f.name = j.value("name", f.id);
  f.worker = j.at("worker").get<std::string>();
  f.town_hall = j.at("town_hall").get<std::string>();
  f.base_buildings = parse_strings(j, "base_buildings");
  if (j.contains("translations"))
    for (const auto& [from, t] : j.at("translations").items()) {
      Translation tr;
      if (t.contains("ids"))
        for (const auto& [id, to] : t.at("ids").items())
          tr.ids[id] = to.is_array() ? to.get<std::vector<std::string>>() : std::vector<std::string>{to.get<std::string>()};
      if (t.contains("terms"))
        for (const auto& pair : t.at("terms")) tr.terms.emplace_back(pair.at(0).get<std::string>(), pair.at(1).get<std::string>());
      f.translations[from] = std::move(tr);
    }
  return f;
}

/// The right-click rules of commands.json ("context_orders"); without them, the behaviour the game always had.
std::vector<ContextRule> parse_context_rules(std::string_view commands_json) {
  const json doc = json::parse(commands_json);
  std::vector<ContextRule> rules;
  if (!doc.contains("context_orders"))
    return {{{"enemy"}, "attack"},         {{"resource"}, "harvest"},          {{"own_transport"}, "board"},
            {{"own_damaged"}, "repair"},   {{"unfinished_building"}, "resume_build"}, {{}, "move"}};
  for (const auto& r : doc.at("context_orders")) {
    ContextRule rule;
    rule.command = r.at("command").get<std::string>();
    rule.when = parse_strings(r, "when");
    for (const auto& w : rule.when)
      if (std::ranges::find(kContextConditions, w) == std::end(kContextConditions))
        throw std::runtime_error("context_orders: unknown condition '" + w + "'");
    rules.push_back(std::move(rule));
  }
  return rules;
}

constexpr const char* kShapes[] = {"arrow", "bolt", "axe", "boulder", "fire", "magic", "coil"};

ProjectileDef parse_projectile(const json& j) {
  ProjectileDef p;
  p.id = j.at("id").get<std::string>();
  p.shape = j.value("shape", p.id);
  if (std::ranges::find(kShapes, p.shape) == std::end(kShapes))
    throw std::runtime_error("projectile '" + p.id + "': unknown shape '" + p.shape + "'");
  p.speed = j.value("speed", p.speed);
  // Thrown things arc: boulders a lot, arrows a little; fire and magic fly straight unless the data says otherwise.
  const bool boulder = p.shape == "boulder";
  const bool thrown = boulder || p.shape == "arrow" || p.shape == "bolt" || p.shape == "axe";
  p.arc_max = j.value("arc_max", boulder ? 46.0f : thrown ? 12.0f : 0.0f);
  p.arc_ratio = j.value("arc_ratio", boulder ? 0.35f : 0.08f);
  p.min_seconds = j.value("min_seconds", p.min_seconds);
  p.max_seconds = j.value("max_seconds", p.max_seconds);
  if (p.speed <= 0) throw std::runtime_error("projectile '" + p.id + "': speed must be > 0");
  if (p.min_seconds < 0 || p.max_seconds < p.min_seconds) throw std::runtime_error("projectile '" + p.id + "': bad flight time limits");
  return p;
}

/// The seven missiles of the original game, used when a game ships no projectiles.json.
constexpr const char* kLegacyProjectiles = R"({"projectiles":[
 {"id":"arrow","speed":460},{"id":"bolt","speed":520},{"id":"axe","speed":320},{"id":"boulder","speed":280},
 {"id":"fire","speed":340},{"id":"magic","speed":400},{"id":"coil","speed":360}]})";

template <class T, class Parse>
std::vector<T> parse_list(std::string_view text, const char* key, Parse parse) {
  const json doc = json::parse(text);
  std::vector<T> out;
  for (const auto& item : doc.at(key)) out.push_back(parse(item));
  return out;
}

template <class T>
std::string find_duplicate(const std::vector<T>& items) {
  std::set<std::string> seen;
  for (const auto& i : items)
    if (!seen.insert(i.id).second) return i.id;
  return {};
}

}  // namespace

std::string translate_text(std::string_view text, const Translation& translation) {
  std::string out;
  for (std::size_t i = 0; i < text.size();) {
    const std::pair<std::string, std::string>* best = nullptr;
    for (const auto& term : translation.terms)
      if (!term.first.empty() && text.substr(i).starts_with(term.first) && (!best || term.first.size() > best->first.size()))
        best = &term;
    if (best) {
      out += best->second;
      i += best->first.size();
    } else {
      out += text[i++];
    }
  }
  return out;
}

std::expected<Registry, std::string> Registry::parse(std::string_view units_json, std::string_view buildings_json,
                                                     std::string_view commands_json, std::string_view factions_json,
                                                     std::string_view resources_json, std::string_view projectiles_json) {
  Registry r;
  if (!resources_json.empty()) {
    auto table = ResourceTable::parse(resources_json);
    if (!table) return std::unexpected("invalid game data: " + table.error());
    r.resources_ = std::make_shared<const ResourceTable>(std::move(*table));
  }
  const ResourceTable& table = *r.resources_;
  try {
    r.units_ = parse_list<UnitDef>(units_json, "units", [&](const json& j) { return parse_unit(j, table); });
    r.buildings_ = parse_list<BuildingDef>(buildings_json, "buildings", [&](const json& j) { return parse_building(j, table); });
    r.commands_ = parse_list<CommandDef>(commands_json, "commands", parse_command);
    r.context_rules_ = parse_context_rules(commands_json);
    r.projectiles_ = parse_list<ProjectileDef>(projectiles_json.empty() ? std::string_view(kLegacyProjectiles) : projectiles_json, "projectiles", parse_projectile);
    if (!factions_json.empty()) r.factions_ = parse_list<FactionDef>(factions_json, "factions", parse_faction);
  } catch (const std::exception& e) {
    return std::unexpected(std::string("invalid game data: ") + e.what());
  }

  for (const auto& dup : {find_duplicate(r.units_), find_duplicate(r.buildings_), find_duplicate(r.commands_), find_duplicate(r.factions_), find_duplicate(r.projectiles_)})
    if (!dup.empty()) return std::unexpected("invalid game data: duplicate id '" + dup + "'");

  for (const auto& rule : r.context_rules_)
    if (!r.command(rule.command) && json::parse(commands_json).contains("context_orders"))
      return std::unexpected("context_orders: unknown command '" + rule.command + "'");
  // Every reference must resolve, so a typo fails at startup rather than mid-game.
  for (const auto& u : r.units_) {
    for (const auto& c : u.commands)
      if (!r.command(c)) return std::unexpected("unit '" + u.id + "' uses unknown command '" + c + "'");
    if (!u.projectile.empty() && !r.projectile(u.projectile))
      return std::unexpected("unit '" + u.id + "' uses unknown projectile '" + u.projectile + "'");
    for (const auto& b : u.builds)
      if (!r.building(b)) return std::unexpected("unit '" + u.id + "' builds unknown building '" + b + "'");
    for (const auto& q : u.requirements)
      if (!r.building(q)) return std::unexpected("unit '" + u.id + "' requires unknown building '" + q + "'");
    if (!u.builds.empty() && !u.worker)
      return std::unexpected("unit '" + u.id + "' can build but has no worker block");
  }
  for (const auto& b : r.buildings_)
    for (const auto& q : b.requirements)
      if (!r.building(q)) return std::unexpected("building '" + b.id + "' requires unknown building '" + q + "'");
  for (const auto& b : r.buildings_)
    for (const auto& t : b.trains)
      if (!r.unit(t)) return std::unexpected("building '" + b.id + "' trains unknown unit '" + t + "'");
  for (const auto& f : r.factions_) {
    const UnitDef* worker = r.unit(f.worker);
    if (!worker || !worker->worker)
      return std::unexpected("faction '" + f.id + "': '" + f.worker + "' is not a worker unit");
    if (!r.building(f.town_hall)) return std::unexpected("faction '" + f.id + "' has unknown town hall '" + f.town_hall + "'");
    for (const auto& b : f.base_buildings)
      if (!r.building(b)) return std::unexpected("faction '" + f.id + "' has unknown base building '" + b + "'");
    for (const auto& [from, t] : f.translations) {
      if (!r.faction(from)) return std::unexpected("faction '" + f.id + "' translates from unknown faction '" + from + "'");
      for (const auto& [id, to] : t.ids) {
        const auto known = [&](const std::string& x) { return r.unit(x) || r.building(x); };
        if (!known(id)) return std::unexpected("faction '" + f.id + "' translates unknown id '" + id + "'");
        for (const auto& x : to)
          if (!known(x)) return std::unexpected("faction '" + f.id + "' translates '" + id + "' to unknown id '" + x + "'");
      }
    }
  }
  // Resolve the requirements to definitions (the vectors are not resized from here on, so the pointers stay valid).
  for (auto& u : r.units_)
    for (const auto& q : u.requirements) u.required_defs.push_back(r.building(q));
  for (auto& b : r.buildings_)
    for (const auto& q : b.requirements) b.required_defs.push_back(r.building(q));
  return r;
}

std::expected<Registry, std::string> Registry::load_directory(const std::string& directory) {
  const auto read = [&](const char* name) -> std::expected<std::string, std::string> {
    std::ifstream f(directory + "/" + name);
    if (!f) return std::unexpected("cannot open " + directory + "/" + name);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  };
  const auto units = read("units.json");
  if (!units) return std::unexpected(units.error());
  const auto buildings = read("buildings.json");
  if (!buildings) return std::unexpected(buildings.error());
  const auto commands = read("commands.json");
  if (!commands) return std::unexpected(commands.error());
  const auto factions = read("factions.json");
  if (!factions) return std::unexpected(factions.error());
  // resources.json is optional: without it the game has the three resources of the original.
  std::string resources;
  if (std::filesystem::exists(directory + "/resources.json")) {
    const auto text = read("resources.json");
    if (!text) return std::unexpected(text.error());
    resources = *text;
  }
  std::string projectiles;
  if (std::filesystem::exists(directory + "/projectiles.json")) {
    const auto text = read("projectiles.json");
    if (!text) return std::unexpected(text.error());
    projectiles = *text;
  }
  return parse(*units, *buildings, *commands, *factions, resources, projectiles);
}

const ProjectileDef* Registry::projectile(std::string_view id) const {
  const auto it = std::ranges::find(projectiles_, id, &ProjectileDef::id);
  return it == projectiles_.end() ? nullptr : &*it;
}
const UnitDef* Registry::unit(std::string_view id) const {
  const auto it = std::ranges::find(units_, id, &UnitDef::id);
  return it == units_.end() ? nullptr : &*it;
}
const BuildingDef* Registry::building(std::string_view id) const {
  const auto it = std::ranges::find(buildings_, id, &BuildingDef::id);
  return it == buildings_.end() ? nullptr : &*it;
}
const FactionDef* Registry::faction(std::string_view id) const {
  const auto it = std::ranges::find(factions_, id, &FactionDef::id);
  return it == factions_.end() ? nullptr : &*it;
}
const CommandDef* Registry::command(std::string_view id) const {
  const auto it = std::ranges::find(commands_, id, &CommandDef::id);
  return it == commands_.end() ? nullptr : &*it;
}

}  // namespace myrts
