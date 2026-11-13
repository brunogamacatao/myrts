#include "defs.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <set>

namespace myrts {

namespace {

using json = nlohmann::json;

Cost parse_cost(const json& j) {
  return {j.value("gold", 0), j.value("lumber", 0), j.value("oil", 0)};
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

UnitDef parse_unit(const json& j) {
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
  if (j.contains("cost")) u.cost = parse_cost(j.at("cost"));
  u.train_seconds = j.value("train_seconds", 0.0);
  u.food = j.value("food", u.food);
  u.icon = j.value("icon", -1);
  if (u.food < 0) throw std::runtime_error("unit '" + u.id + "': food must be >= 0");
  u.attack_seconds = j.value("attack_seconds", u.attack_seconds);
  u.siege_multiplier = j.value("siege_multiplier", u.siege_multiplier);
  if (u.attack_seconds <= 0) throw std::runtime_error("unit '" + u.id + "': attack_seconds must be > 0");
  u.sprite = j.at("sprite").get<std::string>();
  u.projectile = j.value("projectile", std::string{});
  u.commands = parse_strings(j, "commands");
  u.builds = parse_strings(j, "builds");
  if (j.contains("worker")) {
    const auto& w = j.at("worker");
    WorkerDef def;
    def.lumber_per_trip = w.value("lumber_per_trip", def.lumber_per_trip);
    def.gold_per_trip = w.value("gold_per_trip", def.gold_per_trip);
    def.chop_seconds = w.value("chop_seconds", def.chop_seconds);
    def.mine_seconds = w.value("mine_seconds", def.mine_seconds);
    u.worker = def;
  }
  u.aggressive = j.value("aggressive", !u.worker.has_value());  // workers keep working unless told to fight
  return u;
}

BuildingDef parse_building(const json& j) {
  BuildingDef b;
  b.id = j.at("id").get<std::string>();
  b.name = j.at("name").get<std::string>();
  b.faction = j.value("faction", std::string{});
  b.size = j.at("size").get<int>();
  if (b.size < 1) throw std::runtime_error("building '" + b.id + "': size must be >= 1");
  b.cost = parse_cost(j.value("cost", json::object()));
  b.build_seconds = j.at("build_seconds").get<double>();
  b.hp = j.value("hp", b.hp);
  b.armor = j.value("armor", 0);
  b.food = j.value("food", 0);
  b.icon = j.value("icon", -1);
  if (b.hp < 1) throw std::runtime_error("building '" + b.id + "': hp must be >= 1");
  b.is_depot = j.value("is_depot", false);
  b.sprite = j.at("sprite").get<std::string>();
  b.sprite_construction = j.value("sprite_construction", b.sprite);
  b.trains = parse_strings(j, "trains");
  return b;
}

CommandDef parse_command(const json& j) {
  CommandDef c;
  c.id = j.at("id").get<std::string>();
  c.label = j.value("label", c.id);
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
  return f;
}

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

std::expected<Registry, std::string> Registry::parse(std::string_view units_json, std::string_view buildings_json,
                                                     std::string_view commands_json, std::string_view factions_json) {
  Registry r;
  try {
    r.units_ = parse_list<UnitDef>(units_json, "units", parse_unit);
    r.buildings_ = parse_list<BuildingDef>(buildings_json, "buildings", parse_building);
    r.commands_ = parse_list<CommandDef>(commands_json, "commands", parse_command);
    if (!factions_json.empty()) r.factions_ = parse_list<FactionDef>(factions_json, "factions", parse_faction);
  } catch (const std::exception& e) {
    return std::unexpected(std::string("invalid game data: ") + e.what());
  }

  for (const auto& dup : {find_duplicate(r.units_), find_duplicate(r.buildings_), find_duplicate(r.commands_), find_duplicate(r.factions_)})
    if (!dup.empty()) return std::unexpected("invalid game data: duplicate id '" + dup + "'");

  // Every reference must resolve, so a typo fails at startup rather than mid-game.
  for (const auto& u : r.units_) {
    for (const auto& c : u.commands)
      if (!r.command(c)) return std::unexpected("unit '" + u.id + "' uses unknown command '" + c + "'");
    for (const auto& b : u.builds)
      if (!r.building(b)) return std::unexpected("unit '" + u.id + "' builds unknown building '" + b + "'");
    if (!u.builds.empty() && !u.worker)
      return std::unexpected("unit '" + u.id + "' can build but has no worker block");
  }
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
  }
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
  return parse(*units, *buildings, *commands, *factions);
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
