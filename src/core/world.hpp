#pragma once

#include <array>
#include <deque>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "building.hpp"
#include "defs.hpp"
#include "entity_store.hpp"
#include "events.hpp"
#include "facing.hpp"
#include "grid.hpp"
#include "harvest.hpp"
#include "occupancy.hpp"

namespace myrts {

enum class Cargo { None, Lumber, Gold };


// ---- orders -----------------------------------------------------------------
// What a unit is busy with. Moving is not an order: a unit simply follows its path. To teach units
// a new activity (attack, repair, patrol...) add an alternative here and a handler in World::update_order.

namespace order {

struct Idle {};

/// Walk to a tree or gold mine, work, carry the load to a depot, repeat.
struct Harvest {
  enum class State { ToResource, Working, ToDepot };
  State state = State::ToResource;
  bool gold = false;
  Point target{};      // a tree tile, or any tile of the mine
  int mine = -1;       // index of the mine when gold
  double timer = 0.0;  // work progress, or delay before re-planning a blocked route
};

/// Walk to a construction site and work on it until it is finished.
struct Build {
  enum class State { ToSite, Building };
  State state = State::ToSite;
  EntityId building;
  double timer = 0.0;
};

/// What an attack is aimed at.
struct Target {
  enum class Kind { Unit, Building };
  Kind kind = Kind::Unit;
  EntityId id;
};

/// Chase a target and strike it until it is gone. `resume` is where an attack-move was heading.
struct Attack {
  Target target;
  std::optional<Point> resume;
  double retry = 0.0;  // time since the route to a moving target was last planned
};

/// Walk to a point and fight whatever enemy comes into sight on the way.
struct AttackMove {
  Point dest{};
  double scan = 0.0;
};

}  // namespace order

using Order = std::variant<order::Idle, order::Harvest, order::Build, order::Attack, order::AttackMove>;

// ---- entities ---------------------------------------------------------------

struct Unit {
  EntityId id;
  const UnitDef* def = nullptr;
  PlayerId owner = kNoPlayer;
  int hp = 1;

  // movement (positions in pixels, tiles in map coordinates)
  float x = 0, y = 0;  // centre of the unit
  Point tile{};
  Point next{};
  bool moving = false;
  std::deque<Point> path;  // tiles still to visit after the current step
  Point goal{};
  double blocked_time = 0.0;
  Facing facing = Facing::Down;

  // work
  Order order;
  double cooldown = 0.0;      // time until the next strike
  double scan_timer = 0.0;    // time until an idle unit looks around again
  double attack_flash = 0.0;  // > 0 while the strike animation should play
  Cargo cargo = Cargo::None;
  int carrying = 0;
  bool hidden = false;  // inside a gold mine

  /// Tile the unit will occupy once its current step ends (where new paths must start).
  [[nodiscard]] Point anchor() const { return moving ? next : tile; }
  [[nodiscard]] bool is_idle() const { return !moving && path.empty(); }
  [[nodiscard]] bool is_moving() const { return moving || !path.empty(); }
  /// A builder cannot be given other orders until its construction is finished.
  [[nodiscard]] bool locked() const { return std::holds_alternative<order::Build>(order); }
  /// Hammering or chopping (selects the work animation).
  [[nodiscard]] bool working() const;
  [[nodiscard]] bool attacking() const { return attack_flash > 0.0; }
};

/// A unit waiting to be trained in a building.
struct TrainItem {
  const UnitDef* unit = nullptr;
  double progress = 0.0;  // seconds
};

struct Building {
  EntityId id;
  const BuildingDef* def = nullptr;
  PlayerId owner = kNoPlayer;
  Point origin{};
  double progress = 0.0;  // seconds of work done
  bool complete = false;
  EntityId builder;  // unit currently working on it
  int hp = 1;
  std::vector<TrainItem> queue;  // the front one is being trained
  std::optional<Point> rally;    // where new units go after leaving the building
};

struct Mine {
  GoldMine site;
  bool busy = false;  // a peon is inside
};

struct Player {
  std::string name;
  std::string faction;
  Stock stock;
};

struct WorldConfig {
  int tile_size = 32;
  double build_speed = 1.0;   // construction time divisor (testing)
  double retry_delay = 0.5;   // seconds before re-planning a blocked route
  int lumber_per_tree = 100;
  double train_speed = 1.0;   // training time divisor (testing)
  int max_queue = 5;
  double scan_interval = 0.3; // how often idle fighters look for enemies
  unsigned seed = 1;          // damage rolls are reproducible for a given seed
};

/// Outcome of an order. `message` is meant for the player.
struct Result {
  bool ok = true;
  std::string message;
  static Result success() { return {}; }
  static Result fail(std::string m) { return {false, std::move(m)}; }
};

enum class PlaceStatus { Ok, Blocked, UnitInWay };

/// The whole simulation: terrain, units, buildings, resources and the orders that drive them.
/// It knows nothing about graphics, input or the UI, so it can be tested headless.
class World {
 public:
  /// `land_blocked` marks tiles land units cannot enter; `water_blocked` (same size) tiles water
  /// units cannot enter. Air units are only limited by the map edge.
  World(WorldConfig config, Grid land_blocked, Grid water_blocked = {});

  // ---- setup
  PlayerId add_player(std::string name, std::string faction, Stock stock);
  /// Returns kNoEntity if `tile` is blocked or already occupied.
  EntityId spawn_unit(const UnitDef& def, PlayerId owner, Point tile);
  EntityId add_building(const BuildingDef& def, PlayerId owner, Point origin, bool complete);
  void add_tree(Point tile, int lumber);
  int add_mine(Point origin, int size, int gold);

  // ---- orders (what the player asks for)
  Result order_stop(std::span<const EntityId> units);
  Result order_move(std::span<const EntityId> units, Point target);
  Result order_harvest(std::span<const EntityId> units, Point tile);
  /// Pays for and starts a building; the nearest free unit among `candidates` goes to build it.
  Result order_build(PlayerId owner, std::span<const EntityId> candidates, const BuildingDef& def, Point origin);
  Result order_resume_build(std::span<const EntityId> units, EntityId building);
  /// Attack the enemy unit or building on `tile`; on open ground, attack-move there.
  Result order_attack(std::span<const EntityId> units, Point tile);
  /// Walk to `tile` and engage any enemy met on the way.
  Result order_attack_move(std::span<const EntityId> units, Point tile);
  Result order_train(PlayerId owner, EntityId building, const UnitDef& unit);
  /// Where units trained in `building` head for (gold mines and trees put workers to work).
  Result order_rally(PlayerId owner, EntityId building, Point tile);
  /// Takes the `index`-th queued unit out of the queue and refunds its price.
  Result order_cancel_train(PlayerId owner, EntityId building, std::size_t index);

  void kill_unit(EntityId id);

  /// Advances the simulation and then delivers the queued events.
  void update(double dt);

  // ---- queries
  [[nodiscard]] const WorldConfig& config() const { return config_; }
  [[nodiscard]] int width() const { return land().blocked.width(); }
  [[nodiscard]] int height() const { return land().blocked.height(); }
  [[nodiscard]] const Grid& grid(MoveClass c) const { return layer(c).blocked; }
  [[nodiscard]] PlaceStatus placement_status(const BuildingDef& def, Point origin) const;
  [[nodiscard]] EntityId building_at(Point tile) const;
  [[nodiscard]] EntityId unit_at(Point tile) const;  // visible unit standing on (or stepping onto) the tile
  [[nodiscard]] static bool is_enemy(PlayerId a, PlayerId b) { return a != b && a != kNoPlayer && b != kNoPlayer; }
  [[nodiscard]] int mine_at(Point tile) const;  // -1 if none (depleted mines do not count)
  [[nodiscard]] std::vector<Point> depot_tiles(PlayerId owner) const;
  [[nodiscard]] bool has_assets(PlayerId owner) const;
  /// Food provided by finished buildings.
  [[nodiscard]] int food_cap(PlayerId owner) const;
  /// Food eaten by living units plus those waiting in training queues.
  [[nodiscard]] int food_used(PlayerId owner) const;
  [[nodiscard]] std::size_t player_count() const { return players_.size(); }

  [[nodiscard]] Player& player(PlayerId id) { return players_.at(static_cast<std::size_t>(id)); }
  [[nodiscard]] const Player& player(PlayerId id) const { return players_.at(static_cast<std::size_t>(id)); }
  [[nodiscard]] EntityStore<Building>& buildings() { return buildings_; }
  [[nodiscard]] EntityStore<Unit>& units() { return units_; }
  [[nodiscard]] const EntityStore<Unit>& units() const { return units_; }
  [[nodiscard]] const EntityStore<Building>& buildings() const { return buildings_; }
  [[nodiscard]] const std::vector<Mine>& mines() const { return mines_; }
  [[nodiscard]] const TreeField& trees() const { return trees_; }
  [[nodiscard]] EventBus& events() { return events_; }

 private:
  struct Layer {
    Grid blocked;
    Occupancy occupancy;
  };
  [[nodiscard]] Layer& layer(MoveClass c) { return layers_[static_cast<std::size_t>(c)]; }
  [[nodiscard]] const Layer& layer(MoveClass c) const { return layers_[static_cast<std::size_t>(c)]; }
  [[nodiscard]] Layer& layer_of(const Unit& u) { return layer(u.def->move_class); }
  [[nodiscard]] const Layer& land() const { return layer(MoveClass::Land); }
  static int occ_id(const Unit& u) { return static_cast<int>(u.id.index); }

  // movement (world.cpp)
  void step_unit(Unit& u, double dt);
  void try_advance(Unit& u, double dt);
  void begin_step(Unit& u, Point next);
  static void face(Unit& u, float dx, float dy);
  static void face_tile(Unit& u, Point target);
  void stop_unit(Unit& u);
  void send_along(Unit& u, std::deque<Point> path);

  // orders (orders.cpp)
  void update_order(Unit& u, double dt);
  void update_harvest(Unit& u, order::Harvest& h, double dt);
  void update_build(Unit& u, order::Build& b, double dt);
  void cancel_order(Unit& u);
  [[nodiscard]] std::optional<ApproachPath> approach(const Unit& u, std::span<const Point> targets) const;
  bool start_gathering(Unit& u, bool gold, Point target);
  bool start_gathering_nearest(Unit& u, bool gold);
  bool send_to_depot(Unit& u);
  bool start_building(Unit& u, EntityId building);
  void notice(PlayerId to, std::string text);
  void fell_tree(Point tile);
  void exhaust_mine(int index);
  void free_units(std::span<const EntityId> in, std::vector<EntityId>& out) const;
  std::vector<std::optional<Point>> move_group(const std::vector<EntityId>& movers, Point target);

  // combat and training (combat.cpp)
  void update_attack(Unit& u, order::Attack& a, double dt);
  void update_attack_move(Unit& u, order::AttackMove& m, double dt);
  void auto_acquire(Unit& u, double dt);
  [[nodiscard]] std::optional<order::Target> find_target(const Unit& u) const;
  [[nodiscard]] bool target_tiles(const order::Target& t, std::vector<Point>& out) const;
  void begin_attack(Unit& u, const order::Target& target, std::optional<Point> resume);
  void end_attack(Unit& u, std::optional<Point> resume);
  void strike(Unit& attacker, const order::Target& target);
  void destroy_building(EntityId id);
  void update_training(double dt);
  [[nodiscard]] std::optional<Point> free_spawn_tile(const Building& b, MoveClass cls) const;

  WorldConfig config_;
  std::array<Layer, 3> layers_;
  EntityStore<Unit> units_;
  EntityStore<Building> buildings_;
  std::vector<Mine> mines_;
  TreeField trees_;
  std::vector<Player> players_;
  EventBus events_;
  std::mt19937 rng_;
};

}  // namespace myrts
