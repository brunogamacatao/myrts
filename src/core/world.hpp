#pragma once

#include <array>
#include <expected>
#include <nlohmann/json_fwd.hpp>
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
#include "fog.hpp"
#include "events.hpp"
#include "facing.hpp"
#include "grid.hpp"
#include "harvest.hpp"
#include "occupancy.hpp"
#include "tilemap.hpp"

namespace myrts {

/// What a worker is carrying back to a depot.
enum class Cargo { None, Lumber, Gold, Oil };


// ---- orders -----------------------------------------------------------------
/// @name Orders
/// What a unit is busy with. Moving is not an order: a unit simply follows its path. To teach units
// a new activity (attack, repair, patrol...) add an alternative here and a handler in World::update_order.

namespace order {

/// Nothing to do. Idle fighters may pick a fight by themselves (see World::auto_acquire).
struct Idle {};

/// Walk to a tree or gold mine, work, carry the load to a depot, repeat.
struct Harvest {
  enum class State { ToResource, Working, ToDepot };
  State state = State::ToResource;  ///< current leg of the work cycle
  ResourceKind kind = ResourceKind::Lumber;  ///< what is being gathered
  /// Gold and oil come from a site on the map (see World::mines()), lumber from a tree.
  [[nodiscard]] bool from_site() const { return kind != ResourceKind::Lumber; }
  Point target{};                   ///< a tree tile, or any tile of the mine
  int mine = -1;                    ///< index into World::mines() when gathering from a site
  double timer = 0.0;               ///< work progress, or delay before re-planning a blocked route
};

/// Walk to a construction site and work on it until it is finished.
struct Build {
  enum class State { ToSite, Building };
  State state = State::ToSite;  ///< walking there, or hammering
  EntityId building;            ///< the construction site
  double timer = 0.0;           ///< delay before re-planning a blocked route
};

/// What an attack is aimed at.
struct Target {
  enum class Kind { Unit, Building };
  Kind kind = Kind::Unit;  ///< which store `id` refers to
  EntityId id;
};

/// Chase a target and strike it until it is gone. `resume` is where an attack-move was heading.
struct Attack {
  Target target;
  std::optional<Point> resume;  ///< where to carry on to afterwards (set when interrupting an attack-move)
  double retry = 0.0;           ///< time since the route to a moving target was last planned
};

/// Walk to a point and fight whatever enemy comes into sight on the way.
struct AttackMove {
  Point dest{};       ///< final destination
  double scan = 0.0;  ///< time since the surroundings were last scanned for enemies
};

/// Walk to a damaged building (or machine) of one's own side and mend it, which costs gold and lumber.
struct Repair {
  enum class State { ToTarget, Repairing };
  State state = State::ToTarget;
  Target target;
  double timer = 0.0;       ///< delay before re-planning a blocked route
  double healed = 0.0;      ///< hit points mended but not yet added (fractions)
  double owe_gold = 0.0;    ///< the cost of what was mended, paid in whole pieces
  double owe_lumber = 0.0;
};

}  // namespace order

/// @}

/// The activity of a unit: exactly one alternative of `order::*` at a time.
using Order = std::variant<order::Idle, order::Harvest, order::Build, order::Attack, order::AttackMove, order::Repair>;

// ---- entities ---------------------------------------------------------------

/// A living unit: its static data (`def`), position, path and what it is doing.
struct Unit {
  EntityId id;
  const UnitDef* def = nullptr;  ///< static data shared by all units of this kind
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
  double progress = 0.0;  ///< seconds of training done
};

/// A building, under construction or finished. Static data is in `def`.
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

/// A gold mine or oil patch on the map plus whether a worker is currently inside it.
struct Mine {
  ResourceSite site;
  bool busy = false;  ///< a worker is inside (gold only: one at a time; oil can be pumped by several)
  [[nodiscard]] ResourceKind kind() const { return site.kind(); }
};

/// A participant of the match (human or computer) and their resources.
struct Player {
  std::string name;
  std::string faction;
  Stock stock;
};

/// Tunables of a World. The `*_speed` fields exist for testing (they divide durations).
struct WorldConfig {
  int tile_size = 32;
  double build_speed = 1.0;   // construction time divisor (testing)
  double retry_delay = 0.5;   // seconds before re-planning a blocked route
  int lumber_per_tree = 100;
  double train_speed = 1.0;   // training time divisor (testing)
  int max_queue = 5;
  double scan_interval = 0.3; // how often idle fighters look for enemies
  bool fog = true;            // false: every player sees the whole map
  unsigned seed = 1;          // damage rolls are reproducible for a given seed
};

/// Outcome of an order. `message` is meant for the player.
struct Result {
  bool ok = true;
  std::string message;
  static Result success() { return {}; }
  static Result fail(std::string m) { return {false, std::move(m)}; }
};

/// Whether a building can be placed: `Blocked` by terrain/other buildings, or `UnitInWay` (temporary)
enum class PlaceStatus { Ok, Blocked, UnitInWay };

/// The whole simulation: terrain, units, buildings, resources and the orders that drive them.
/// It knows nothing about graphics, input or the UI, so it can be tested headless.
class World {
 public:
  /// `land_blocked` marks tiles land units cannot enter; `water_blocked` (same size) tiles water
  /// units cannot enter. Air units are only limited by the map edge.
  World(WorldConfig config, Grid land_blocked, Grid water_blocked = {});
  /// A world over real terrain: blocked tiles come from the map's movement rules, every tree tile holds wood, and
  /// felling trees edits the terrain (see TileMap::clear_trees).
  World(WorldConfig config, TileMap terrain);

  // ---- setup
  /// Adds a player and returns their id (ids are assigned in order: 0, 1, ...).
  PlayerId add_player(std::string name, std::string faction, Stock stock);
  /// Returns kNoEntity if `tile` is blocked or already occupied.
  EntityId spawn_unit(const UnitDef& def, PlayerId owner, Point tile);
  /// Places a building. With `complete == false` it starts as a construction site.
  EntityId add_building(const BuildingDef& def, PlayerId owner, Point origin, bool complete);
  /// Adds a tree holding `lumber` units of wood.
  void add_tree(Point tile, int lumber);
  /// Adds a gold mine or oil patch (a `size` x `size` obstacle: on land for gold, on water for oil) and returns its
  /// index in mines().
  int add_mine(Point origin, int size, int amount, ResourceKind kind = ResourceKind::Gold);

  // ---- orders (what the player asks for)
  /// Cancels the orders and movement of the given units (builders stay locked).
  Result order_stop(std::span<const EntityId> units);
  /// Moves a group to `target`; each unit gets its own destination tile (see assign_destinations).
  Result order_move(std::span<const EntityId> units, Point target);
  /// Workers go gather the tree or mine on `tile` and keep cycling between it and a depot.
  Result order_harvest(std::span<const EntityId> units, Point tile);
  /// Pays for and starts a building; the nearest free unit among `candidates` goes to build it.
  /// Workers go and repair the damaged building (or catapult, ballista...) of their own side on `tile`. Mending costs a quarter
  /// of the price over a full repair, paid as it goes; it stops when the target is whole, or the money runs out.
  Result order_repair(std::span<const EntityId> units, Point tile);
  Result order_build(PlayerId owner, std::span<const EntityId> candidates, const BuildingDef& def, Point origin);
  /// Sends a worker back to finish an unfinished building.
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

  /// Removes a unit immediately, releasing its tiles, and publishes ev::UnitDied.
  void kill_unit(EntityId id);

  /// Advances the simulation by `dt` seconds (orders, movement, training) and then delivers the queued events.
  void update(double dt);

  // ---- queries
  [[nodiscard]] const WorldConfig& config() const { return config_; }
  [[nodiscard]] int width() const { return land().blocked.width(); }
  [[nodiscard]] int height() const { return land().blocked.height(); }
  [[nodiscard]] const Grid& grid(MoveClass c) const { return layer(c).blocked; }
  [[nodiscard]] PlaceStatus placement_status(const BuildingDef& def, Point origin) const;
  /// The building covering `tile`, or kNoEntity.
  [[nodiscard]] EntityId building_at(Point tile) const;
  [[nodiscard]] EntityId unit_at(Point tile) const;  // visible unit standing on (or stepping onto) the tile
  [[nodiscard]] static bool is_enemy(PlayerId a, PlayerId b) { return a != b && a != kNoPlayer && b != kNoPlayer; }
  [[nodiscard]] int mine_at(Point tile) const;  // -1 if none (depleted mines do not count)
  /// Tiles of the owner's finished depots that accept `kind` (halls for gold and lumber, shipyards for oil).
  [[nodiscard]] std::vector<Point> depot_tiles(PlayerId owner, ResourceKind kind = ResourceKind::Gold) const;
  /// True while the player still has any unit or building (used by the victory rules).
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
  /// The tree nearest to `from` within `radius` tiles (square), if any.
  [[nodiscard]] std::optional<Point> nearest_tree(Point from, int radius) const;
  /// The trees touching `start`, directly or through other trees (at most `limit`), `start` first.
  [[nodiscard]] std::vector<Point> forest_around(Point start, std::size_t limit) const;
  /// The terrain, if this world was built over a TileMap.
  [[nodiscard]] const TileMap* terrain() const { return terrain_ ? &*terrain_ : nullptr; }
  [[nodiscard]] EventBus& events() { return events_; }

  // ---- saved games (savegame.cpp)
  /// Everything that changes during a match, as JSON: stocks, remaining trees and mines, buildings (with their training
  /// queues), units (with what they are doing) and what each player has explored. The map itself is not saved: a match is
  /// restored by building the same level again and calling load_state().
  [[nodiscard]] nlohmann::json save_state() const;
  /// Replaces the units, buildings and resources of this (freshly built) world with the saved ones. Orders that were under
  /// way are resumed by planning their routes again. Returns a message (before changing anything) if the map size, the players or a unit or building id do not fit.
  std::expected<void, std::string> load_state(const nlohmann::json& state, const Registry& registry);

  // ---- fog of war
  /// What `player` knows of the map. Refreshed by update() a few times a second, and by refresh_fog().
  [[nodiscard]] const FogOfWar& fog(PlayerId player) const { return fogs_.at(static_cast<std::size_t>(player)); }
  [[nodiscard]] FogOfWar& fog(PlayerId player) { return fogs_.at(static_cast<std::size_t>(player)); }
  /// Recomputes who sees what now (units and buildings reveal the tiles around them).
  void refresh_fog();
  /// How far (in tiles) a building sees.
  [[nodiscard]] static int building_sight(const BuildingDef& def) { return 3 + def.size; }

 private:
  struct Layer {
    Grid blocked;
    Occupancy occupancy;
  };
  [[nodiscard]] Layer& layer(MoveClass c) { return layers_[static_cast<std::size_t>(c)]; }
  [[nodiscard]] const Layer& layer(MoveClass c) const { return layers_[static_cast<std::size_t>(c)]; }
  [[nodiscard]] Layer& layer_of(const Unit& u) { return layer(u.def->move_class); }
  [[nodiscard]] const Layer& land() const { return layer(MoveClass::Land); }
  /// The layer a building occupies: platforms stand on the water, everything else on land.
  [[nodiscard]] Layer& layer_of(const BuildingDef& d) { return layer(d.placement == Placement::Water ? MoveClass::Water : MoveClass::Land); }
  [[nodiscard]] const Layer& layer_of(const BuildingDef& d) const { return layer(d.placement == Placement::Water ? MoveClass::Water : MoveClass::Land); }
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
  void update_repair(Unit& u, order::Repair& r, double dt);
  bool start_repair(Unit& u, const order::Target& target);
  void cancel_order(Unit& u);
  [[nodiscard]] std::optional<ApproachPath> approach(const Unit& u, std::span<const Point> targets) const;
  bool start_gathering(Unit& u, ResourceKind kind, Point target);
  bool start_gathering_nearest(Unit& u, ResourceKind kind);
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
  std::optional<TileMap> terrain_;
  std::vector<Player> players_;
  std::vector<FogOfWar> fogs_;  // one per player
  double fog_timer_ = 0.0;
  EventBus events_;
  std::mt19937 rng_;
};

}  // namespace myrts
