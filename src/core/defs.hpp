#pragma once

#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "building.hpp"

namespace myrts {

/// Which terrain a unit can use. Each class has its own blocked-tile grid and tile occupancy.
enum class MoveClass { Land, Water, Air };

enum class Targeting {
  None,      // acts immediately (Stop)
  Ground,    // next click picks a tile (Move)
  Resource,  // next click picks a tree or a mine (Harvest)
  Menu       // opens a sub-menu (Build)
};

struct UnitStats {
  int armor = 0;
  int damage_min = 0;
  int damage_max = 0;
  int range = 0;
  int sight = 0;
  int speed = 0;
};

/// Optional capability: this unit can gather resources.
struct WorkerDef {
  int lumber_per_trip = 10;
  int gold_per_trip = 100;
  double chop_seconds = 3.0;
  double mine_seconds = 2.5;
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
  bool aggressive = true;                // attacks enemies it sees on its own (workers do not)
  std::string sprite;                    // path of the sprite definition (json)
  std::vector<std::string> commands;     // command ids offered in the panel
  std::optional<WorkerDef> worker;       // gathers resources
  std::vector<std::string> builds;       // building ids this unit can construct

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
  bool is_depot = false;  // accepts delivered gold and lumber
  std::string sprite;               // finished building (png)
  std::string sprite_construction;  // while being built (png)
  std::vector<std::string> trains;  // unit ids it can train
};

/// A button of the command panel. The behaviour behind it is registered by the game.
struct CommandDef {
  std::string id;
  std::string label;
  std::string icon;  // key into the UI's glyph table
  Targeting targeting = Targeting::None;
  char hotkey = 0;  // upper-case letter, 0 for none
  bool implemented = true;
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

  static std::expected<Registry, std::string> parse(std::string_view units_json, std::string_view buildings_json,
                                                    std::string_view commands_json);
  /// Reads units.json, buildings.json and commands.json from `directory`.
  static std::expected<Registry, std::string> load_directory(const std::string& directory);

  [[nodiscard]] const UnitDef* unit(std::string_view id) const;
  [[nodiscard]] const BuildingDef* building(std::string_view id) const;
  [[nodiscard]] const CommandDef* command(std::string_view id) const;

  [[nodiscard]] std::span<const UnitDef> units() const { return units_; }
  [[nodiscard]] std::span<const BuildingDef> buildings() const { return buildings_; }
  [[nodiscard]] std::span<const CommandDef> commands() const { return commands_; }

 private:
  std::vector<UnitDef> units_;
  std::vector<BuildingDef> buildings_;
  std::vector<CommandDef> commands_;
};

}  // namespace myrts
