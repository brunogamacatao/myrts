#include "match_setup.hpp"

#include <cmath>

#include "formation.hpp"
#include "pud.hpp"
#include "scenario.hpp"

namespace myrts {

namespace {

constexpr int kMineSize = 3;
constexpr int kInitialWorkers = 8;

/// The free spot closest to `near` where `def` fits, leaving a one-tile gap around it so units can walk past.
std::optional<Point> find_spot(const World& world, const BuildingDef& def, Point near) {
  std::optional<Point> best;
  double best_d = 1e9;
  for (int y = near.y - 14; y <= near.y + 14; ++y) {
    for (int x = near.x - 14; x <= near.x + 14; ++x) {
      const Point origin{x, y};
      if (world.placement_status(def, origin) != PlaceStatus::Ok) continue;
      if (!can_place(world.grid(MoveClass::Land), {x - 1, y - 1}, def.size + 2)) continue;
      const double d = std::hypot(static_cast<double>(x - near.x), static_cast<double>(y - near.y));
      if (d < best_d) {
        best_d = d;
        best = origin;
      }
    }
  }
  return best;
}

/// A player's hall goes where the map put one, or at the nearest free spot to the start location.
std::expected<Point, std::string> place_hall(World& world, const BuildingDef& def, const StartSlot& slot, PlayerId owner) {
  std::optional<Point> at;
  if (slot.hall && world.placement_status(def, *slot.hall) == PlaceStatus::Ok) at = slot.hall;
  if (!at) at = find_spot(world, def, slot.start);
  if (!at) return std::unexpected("no room for a " + def.name + " near the start location");
  world.add_building(def, owner, *at, true);
  return *at;
}

/// Starting workers stand around their hall.
void spawn_workers(World& world, const UnitDef& def, PlayerId owner, Point hall_origin, const BuildingDef& hall_def, int count) {
  const Point around{hall_origin.x + hall_def.size / 2, hall_origin.y + hall_def.size};
  const std::vector<Point> wanted(static_cast<std::size_t>(count), around);
  for (const auto& spot : assign_destinations(world.grid(MoveClass::Land), nullptr, wanted, around))
    if (spot) world.spawn_unit(def, owner, *spot);
}

}  // namespace

std::expected<Match, std::string> build_match(const Registry& registry, std::shared_ptr<const TileSets> tilesets,
                                              const Level& level, const MatchOptions& options) {
  Match m;
  m.local_faction = registry.faction(level.player_faction);
  m.enemy_faction = registry.faction(level.enemy_faction);
  if (!m.local_faction) return std::unexpected("level '" + level.name + "': unknown faction '" + level.player_faction + "'");
  if (!m.enemy_faction) return std::unexpected("level '" + level.name + "': unknown faction '" + level.enemy_faction + "'");
  const UnitDef* our_worker = registry.unit(m.local_faction->worker);
  const UnitDef* their_worker = registry.unit(m.enemy_faction->worker);
  const BuildingDef* our_hall = registry.building(m.local_faction->town_hall);
  const BuildingDef* their_hall = registry.building(m.enemy_faction->town_hall);
  if (!our_worker || !their_worker || !our_hall || !their_hall)
    return std::unexpected("game data is missing the starting units or buildings (assets/data)");
  m.local_hall_def = our_hall;

  // The map: a Warcraft II .pud. Its terrain, start locations and gold mines define the match.
  const std::string map_path = options.assets_dir.empty() ? level.map : options.assets_dir + "/" + level.map;
  auto pud = load_pud(map_path);
  if (!pud) return std::unexpected(pud.error());
  auto terrain = TileMap::from_pud(*pud, std::move(tilesets));
  if (!terrain) return std::unexpected(terrain.error());
  const Scenario scenario = scenario_from(*pud);
  if (scenario.starts.size() < 2) return std::unexpected("this map has fewer than two start locations");

  m.world = std::make_unique<World>(options.world, std::move(*terrain));
  World& world = *m.world;
  for (const MineSpec& mine : scenario.mines) world.add_mine(mine.origin, kMineSize, mine.gold, mine.kind);

  // Two players: the first start location is the human's, the opponent takes the one farthest away.
  const StartSlot& ours = scenario.starts[0];
  const StartSlot& theirs = scenario.starts[*scenario.farthest_from(0)];
  m.local = world.add_player("Player", m.local_faction->id, level.player_stock);
  m.enemy = world.add_player(m.enemy_faction->name, m.enemy_faction->id, level.enemy_stock);

  const auto local_at = place_hall(world, *our_hall, ours, m.local);
  if (!local_at) return std::unexpected(local_at.error());
  const auto enemy_at = place_hall(world, *their_hall, theirs, m.enemy);
  if (!enemy_at) return std::unexpected(enemy_at.error());
  m.local_hall = *local_at;
  m.enemy_hall = *enemy_at;

  // The opponent's production buildings, next to its town hall.
  for (const std::string& id : m.enemy_faction->base_buildings) {
    const BuildingDef* def = registry.building(id);
    if (!def) continue;
    if (const auto at = find_spot(world, *def, m.enemy_hall)) world.add_building(*def, m.enemy, *at, true);
  }
  spawn_workers(world, *our_worker, m.local, m.local_hall, *our_hall, kInitialWorkers);
  spawn_workers(world, *their_worker, m.enemy, m.enemy_hall, *their_hall, level.enemy_workers);

  m.ais.push_back(std::make_unique<Ai>(world, registry, m.enemy, level.ai));
  return m;
}

}  // namespace myrts
