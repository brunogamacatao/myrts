#pragma once

#include <deque>
#include <expected>
#include <memory>
#include <string>
#include <vector>

#include "core/geometry.hpp"
#include "core/grid.hpp"
#include "core/occupancy.hpp"
#include "sprite.hpp"

enum class Command { Move, Stop, Attack, Repair, Harvest, Build };

struct UnitStats {
  int armor = 0;
  int damage_min = 0;
  int damage_max = 0;
  int range = 0;
  int sight = 0;
  int speed = 0;
};

/// Static data shared by all units of a kind (loaded from assets/units/*.json).
struct UnitType {
  std::string name;
  int level = 1;
  int max_hp = 1;
  double move_speed = 64;  // pixels per second
  UnitStats stats;
  std::vector<Command> commands;
  std::shared_ptr<SpriteDef> sprite;
};

std::expected<std::shared_ptr<const UnitType>, std::string> load_unit_type(const std::string& filename);

constexpr bool is_implemented(Command c) { return c == Command::Move || c == Command::Stop; }
const char* command_label(Command c);

class Unit {
 public:
  Unit(int id, std::shared_ptr<const UnitType> type, myrts::Point tile, int tile_size,
       myrts::Occupancy& occupancy);

  [[nodiscard]] int id() const { return id_; }
  [[nodiscard]] const UnitType& type() const { return *type_; }
  [[nodiscard]] myrts::Point tile() const { return tile_; }
  /// Tile the unit will occupy once its current step ends (where new paths must start).
  [[nodiscard]] myrts::Point anchor() const { return moving_ ? next_ : tile_; }
  [[nodiscard]] float x() const { return x_; }
  [[nodiscard]] float y() const { return y_; }
  [[nodiscard]] int hp() const { return hp_; }
  [[nodiscard]] bool is_moving() const { return moving_ || !path_.empty(); }
  [[nodiscard]] myrts::Rect box() const;
  [[nodiscard]] const AnimationState& animation() const { return anim_; }

  bool selected = false;

  /// `path` must start at anchor(); the unit finishes its current step, then follows it.
  void set_path(std::deque<myrts::Point> path);
  /// Stops after the current step.
  void stop();

  void update(double dt, const myrts::Grid& grid, myrts::Occupancy& occupancy);

 private:
  void try_advance(double dt, const myrts::Grid& grid, myrts::Occupancy& occupancy);
  void begin_step(myrts::Point next);
  void face(float dx, float dy);

  int id_;
  std::shared_ptr<const UnitType> type_;
  int tile_size_;
  AnimationState anim_;
  std::string facing_ = "down";
  int hp_;

  float x_, y_;  // world position of the unit's centre, in pixels
  myrts::Point tile_;
  myrts::Point next_{};
  bool moving_ = false;
  std::deque<myrts::Point> path_;  // tiles still to visit after the current step
  myrts::Point goal_{};
  double blocked_time_ = 0.0;
};
