#pragma once

#include <array>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "building.hpp"

namespace myrts {

/// Which terrain a unit can use. Each class has its own blocked-tile grid and tile occupancy.
/// @see World for how each class gets its own blocked grid.
enum class MoveClass { Land, Water, Air };

/// Where a building may stand. A shipyard needs the shore, an oil platform stands on the water.
enum class Placement {
  Land,   ///< on walkable ground (the default)
  Coast,  ///< on walkable ground with open water next to it, so the ships it builds can leave
  Water   ///< on sailable water (oil platforms)
};

enum class Targeting {
  None,      // acts immediately (Stop)
  Ground,    // next click picks a tile (Move)
  Resource,  // next click picks a tree or a mine (Harvest)
  Menu       // opens a sub-menu (Build)
};

struct BuildingDef;

struct UnitStats {
  int armor = 0;       ///< subtracted from each hit taken (a hit does at least 1)
  int damage_min = 0;  ///< lower bound of the damage roll
  int damage_max = 0;  ///< upper bound; 0 means the unit cannot attack
  int range = 0;       ///< attack reach in tiles; 1 = melee
  int sight = 0;       ///< how far (tiles) it notices enemies
  int speed = 0;       ///< display value only; movement uses UnitDef::move_speed
};

/// Optional capability: this unit can gather resources.
/// How a worker gathers one resource: what it carries per trip and how long the work takes.
struct GatherSpec {
  int per_trip = 0;  ///< 0: this worker cannot gather the resource
  double seconds = 0.0;
};
struct WorkerDef {
  std::array<GatherSpec, kMaxResources> gather{};  ///< by ResourceId
  [[nodiscard]] bool can_gather(ResourceId r) const { return r >= 0 && r < kMaxResources && gather[static_cast<std::size_t>(r)].per_trip > 0; }
  [[nodiscard]] const GatherSpec& spec(ResourceId r) const { return gather[static_cast<std::size_t>(r)]; }
};

/// Static data shared by every unit of a kind. Loaded from assets/data/units.json.
struct UnitDef {
  std::string id;
  std::string name;
  std::string faction;
  int level = 1;
  int max_hp = 1;
  double move_speed = 64;  // pixels per second
  MoveClass move_class = MoveClass::Land;
  UnitStats stats;
  Cost cost;
  double train_seconds = 0;
  int food = 1;                          // food this unit eats while alive (or waits in a training queue)
  double attack_seconds = 1.5;           // time between two strikes
  double siege_multiplier = 1.0;         // damage multiplier against buildings (catapults, ballistas)
  int capacity = 0;                      // land units a ship of this kind can carry (0: it is not a transport)
  bool mechanical = false;               // workers can repair it (catapults, ballistas)
  bool aggressive = true;                // attacks enemies it sees on its own (workers do not)
  int icon = -1;                         // index in the icon atlas (assets/ui/icons.png), -1 for none
  std::string sprite;                    // path of the sprite definition (json)
  std::string projectile;                // what a ranged strike looks like in flight ("arrow", "boulder"...), empty for none
  std::vector<std::string> commands;     // command ids offered in the panel
  std::optional<WorkerDef> worker;       // gathers resources
  std::vector<std::string> builds;       // building ids this unit can construct
  std::vector<std::string> requirements;     // building ids the player must own (finished) before training this unit
  std::vector<const BuildingDef*> required_defs;  // the same, resolved by the Registry

  [[nodiscard]] bool can_attack() const { return stats.damage_max > 0; }
};

/// Static data shared by every building of a kind. Loaded from assets/data/buildings.json.
struct BuildingDef {
  std::string id;
  std::string name;
  std::string faction;
  int size = 1;  // footprint in tiles (square)
  Cost cost;
  double build_seconds = 1.0;
  int hp = 400;
  int armor = 0;
  int food = 0;           // food this building provides (farms, town halls)
  std::array<bool, kMaxResources> accepts{};  // resources workers can deliver here, by ResourceId (halls, shipyards...)
  [[nodiscard]] bool accepts_resource(ResourceId r) const { return r >= 0 && r < kMaxResources && accepts[static_cast<std::size_t>(r)]; }
  [[nodiscard]] bool is_depot() const { for (const bool a : accepts) if (a) return true; return false; }
  Placement placement = Placement::Land;  // where it can be built
  int icon = -1;                    // index in the icon atlas
  std::string sprite;               // finished building (png)
  std::string sprite_construction;  // while being built (png)
  std::vector<std::string> trains;  // unit ids it can train
  std::vector<std::string> requirements;  // building ids the player must own (finished) before building this
  std::vector<const BuildingDef*> required_defs;  // the same, resolved by the Registry
};

/// What a ranged strike looks like in flight (an entry of projectiles.json). `shape` names how the front end draws it
/// ("arrow", "bolt", "axe", "boulder", "fire", "magic", "coil"); the flight itself is described by the numbers.
struct ProjectileDef {
  std::string id;
  std::string shape;
  float speed = 400;       ///< pixels per second
  float arc_max = 0;       ///< highest the missile rises above a straight line, in pixels (0: flies straight)
  float arc_ratio = 0.08f; ///< ...but never more than this share of the distance
  double min_seconds = 0.12;  ///< flight time limits
  double max_seconds = 1.2;
};

/// A button of the command panel. The behaviour behind it is registered by the game.
/// The orders the engine knows how to carry out (see World::issue_order). A command of commands.json names one in
/// `order`; a game builds its own buttons out of them.
inline constexpr std::string_view kBuiltinOrders[] = {"move", "stop", "attack", "repair", "harvest", "unload",
                                                      "board", "resume_build", "patrol", "hold", "build"};

/// "When the player right-clicks X with units selected, give command Y": the first matching rule wins.
/// `when` lists conditions that must all hold (empty: always): "enemy" (an enemy unit or building), "resource" (a tree
/// or a mine), "own_transport" (own transport, with land units selected), "own_damaged" (own damaged building or
/// machine, with workers selected), "unfinished_building" (own building under construction).
struct ContextRule {
  std::vector<std::string> when;
  std::string command;
};
inline constexpr std::string_view kContextConditions[] = {"enemy", "resource", "own_transport", "own_damaged",
                                                          "unfinished_building"};

struct CommandDef {
  std::string id;
  std::string order;   ///< which builtin order it gives (default: the command's own id)
  std::string prompt;  ///< shown while waiting for the target click ("" for the default of its targeting)
  std::string label;
  std::string icon;  ///< key into the UI's glyph table (fallback when there is no icon in the atlas)
  /// Icon atlas indices by faction; "default" applies to factions not listed (the two sides have their own).
  std::unordered_map<std::string, int> icons;
  /// The icon for a faction's units: its own, else the default, else -1.
  [[nodiscard]] int icon_for(const std::string& faction) const {
    if (const auto it = icons.find(faction); it != icons.end()) return it->second;
    if (const auto it = icons.find("default"); it != icons.end()) return it->second;
    return -1;
  }
  Targeting targeting = Targeting::None;
  char hotkey = 0;  ///< upper-case letter, 0 for none
  bool implemented = true;
};

/// How to read a campaign written for another faction as this one: the unit and building ids to use instead
/// (`orc_peon` becomes `alliance_peasant`; an id may become several, or none) and the words of the briefings and hints.
struct Translation {
  std::unordered_map<std::string, std::vector<std::string>> ids;
  std::vector<std::pair<std::string, std::string>> terms;  ///< the longest matching source wins; the text is read once
};

/// Rewrites `text` with `translation.terms` in one pass, so a replacement is never replaced again.
[[nodiscard]] std::string translate_text(std::string_view text, const Translation& translation);

/// A playable side: what its players start with. Loaded from assets/data/factions.json.
struct FactionDef {
  std::string id;                              ///< matches `faction` of its units and buildings ("orcs", "alliance")
  std::string name;                            ///< shown to the player ("Horde")
  std::string worker;                          ///< unit id of the starting workers
  std::string town_hall;                       ///< building id every player of this side starts with
  std::vector<std::string> base_buildings;     ///< production buildings a computer player starts with, around its hall
  /// By the faction a campaign level was written for: how to play that level as this faction.
  std::unordered_map<std::string, Translation> translations;
};

/// All game data. Immutable after loading; units and buildings refer to each other by id and
/// every reference is checked when the registry is built.
class Registry {
 public:
  Registry() = default;
  Registry(const Registry&) = delete;
  Registry& operator=(const Registry&) = delete;
  Registry(Registry&&) = default;
  Registry& operator=(Registry&&) = default;

  /// Parses the JSON documents and cross-checks every reference (`builds`, `trains`, `commands`, factions...).
  /// `factions_json` may be empty for data that defines no playable sides (most unit tests).
  /// @return the registry, or a message naming the first problem found.
  static std::expected<Registry, std::string> parse(std::string_view units_json, std::string_view buildings_json,
                                                    std::string_view commands_json, std::string_view factions_json = {},
                                                    std::string_view resources_json = {}, std::string_view projectiles_json = {});
  /// Reads units.json, buildings.json, commands.json, factions.json and (optionally) resources.json and projectiles.json from `directory`.
  static std::expected<Registry, std::string> load_directory(const std::string& directory);

  /// The game's resources (the three of the original game when it defines none).
  [[nodiscard]] const std::shared_ptr<const ResourceTable>& resources() const { return resources_; }
  /// The right-click rules, in order (the game's own, or the default behaviour when commands.json has none).
  [[nodiscard]] const std::vector<ContextRule>& context_rules() const { return context_rules_; }
  /// nullptr when the game defines no projectile with that id.
  [[nodiscard]] const ProjectileDef* projectile(std::string_view id) const;
  [[nodiscard]] std::span<const ProjectileDef> projectiles() const { return projectiles_; }
  [[nodiscard]] const UnitDef* unit(std::string_view id) const;
  [[nodiscard]] const BuildingDef* building(std::string_view id) const;
  [[nodiscard]] const CommandDef* command(std::string_view id) const;
  [[nodiscard]] const FactionDef* faction(std::string_view id) const;

  [[nodiscard]] std::span<const UnitDef> units() const { return units_; }
  [[nodiscard]] std::span<const BuildingDef> buildings() const { return buildings_; }
  [[nodiscard]] std::span<const CommandDef> commands() const { return commands_; }
  [[nodiscard]] std::span<const FactionDef> factions() const { return factions_; }

 private:
  std::shared_ptr<const ResourceTable> resources_ = ResourceTable::legacy();
  std::vector<ProjectileDef> projectiles_;
  std::vector<ContextRule> context_rules_;
  std::vector<UnitDef> units_;
  std::vector<BuildingDef> buildings_;
  std::vector<CommandDef> commands_;
  std::vector<FactionDef> factions_;
};

}  // namespace myrts
