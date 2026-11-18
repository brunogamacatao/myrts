#include "match_setup.hpp"

#include "mapfile.hpp"

#include <cmath>

#include "formation.hpp"
#include "pud.hpp"
#include "scenario.hpp"

namespace myrts {

namespace {

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
                                              const Level& unresolved_level, const MatchOptions& options) {
  const Level level = with_default_sides(unresolved_level, registry);
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

  // The map: a native .mrmap or a Warcraft II .pud (converted on load). Its terrain, start locations and mines define the match.
  const std::string map_path = options.assets_dir.empty() ? level.map : options.assets_dir + "/" + level.map;
  const ResourceTable& resources = *registry.resources();
  const auto map = load_map(map_path, *tilesets, resources, options.world.rules);
  if (!map) return std::unexpected(map.error());
  auto terrain = tilemap_from(*map, std::move(tilesets));
  if (!terrain) return std::unexpected(terrain.error());
  const Scenario scenario = scenario_from(*map);
  if (scenario.starts.size() < 2) return std::unexpected("this map has fewer than two start locations");

  WorldConfig world_config = options.world;
  const auto fog = FogSettings::with(options.world.rules.fog, level.fog_json);
  if (!fog) return std::unexpected("level '" + level.name + "': " + fog.error());
  world_config.fog_settings = *fog;
  m.fog = *fog;
  world_config.resources = registry.resources();  // the world plays by the registry's resources
  m.world = std::make_unique<World>(world_config, std::move(*terrain));
  World& world = *m.world;
  for (const MineSpec& mine : scenario.mines) {
    const ResourceId kind = resources.index(mine.source);  // a mine of a resource the game does not have is left out
    if (kind != kNoResource) world.add_mine(mine.origin, options.world.rules.mine_size, mine.gold, kind);
  }
  const auto player_stock = resources.stock_from(level.player_stock);
  const auto enemy_stock = resources.stock_from(level.enemy_stock);
  if (!player_stock) return std::unexpected("level '" + level.name + "': player_stock: " + player_stock.error());
  if (!enemy_stock) return std::unexpected("level '" + level.name + "': enemy_stock: " + enemy_stock.error());

  // Two players: the first start location is the human's, the opponent takes the one farthest away.
  const StartSlot& ours = scenario.starts[0];
  const StartSlot& theirs = scenario.starts[*scenario.farthest_from(0)];
  m.local = world.add_player("Player", m.local_faction->id, *player_stock);
  m.enemy = world.add_player(m.enemy_faction->name, m.enemy_faction->id, *enemy_stock);

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
  spawn_workers(world, *our_worker, m.local, m.local_hall, *our_hall, options.world.rules.initial_workers);
  spawn_workers(world, *their_worker, m.enemy, m.enemy_hall, *their_hall, level.enemy_workers);

  const auto configured = [&](AiConfig ai) {  // the level's own farm margin wins over the game's default
    if (ai.farm_margin == AiConfig{}.farm_margin) ai.farm_margin = options.world.rules.ai_farm_margin;
    return ai;
  };
  m.ais.push_back(std::make_unique<Ai>(world, registry, m.enemy, configured(level.ai)));

  // More computer players: each takes the free start location farthest from every base already placed.
  std::vector<std::size_t> used{0, *scenario.farthest_from(0)};
  for (const OpponentSpec& spec : level.more_opponents) {
    const FactionDef* faction = registry.faction(spec.faction.empty() ? m.enemy_faction->id : spec.faction);
    if (!faction) return std::unexpected("level '" + level.name + "': unknown faction '" + spec.faction + "' of an opponent");
    const UnitDef* worker = registry.unit(faction->worker);
    const BuildingDef* hall = registry.building(faction->town_hall);
    if (!worker || !hall) return std::unexpected("game data is missing the starting units or buildings of '" + faction->id + "'");
    const auto stock = resources.stock_from(spec.stock);
    if (!stock) return std::unexpected("level '" + level.name + "': opponent stock: " + stock.error());

    std::optional<std::size_t> best;
    double best_d = -1;
    for (std::size_t i = 0; i < scenario.starts.size(); ++i) {
      if (std::ranges::find(used, i) != used.end()) continue;
      double nearest = 1e18;
      for (const std::size_t u : used) {
        const double dx = scenario.starts[i].start.x - scenario.starts[u].start.x, dy = scenario.starts[i].start.y - scenario.starts[u].start.y;
        nearest = std::min(nearest, dx * dx + dy * dy);
      }
      if (nearest > best_d) {
        best_d = nearest;
        best = i;
      }
    }
    if (!best) return std::unexpected("level '" + level.name + "': the map has no free start location for another opponent");
    used.push_back(*best);

    const PlayerId id = world.add_player(faction->name, faction->id, *stock);
    const auto at = place_hall(world, *hall, scenario.starts[*best], id);
    if (!at) return std::unexpected(at.error());
    for (const std::string& b : faction->base_buildings)
      if (const BuildingDef* def = registry.building(b))
        if (const auto spot = find_spot(world, *def, *at)) world.add_building(*def, id, *spot, true);
    spawn_workers(world, *worker, id, *at, *hall, spec.workers);
    m.ais.push_back(std::make_unique<Ai>(world, registry, id, configured(spec.ai)));
    m.more_opponents.push_back(id);
  }
  m.objectives = level.objectives;
  for (const Objective& o : level.objectives.victory) {  // objectives may only name what the game has
    if (!o.building.empty() && !registry.building(o.building)) return std::unexpected("level '" + level.name + "': objective names unknown building '" + o.building + "'");
    if (!o.resource.empty() && resources.index(o.resource) == kNoResource) return std::unexpected("level '" + level.name + "': objective names unknown resource '" + o.resource + "'");
  }
  for (const Objective& o : level.objectives.defeat)
    if (!o.building.empty() && !registry.building(o.building)) return std::unexpected("level '" + level.name + "': objective names unknown building '" + o.building + "'");
  return m;
}

}  // namespace myrts
