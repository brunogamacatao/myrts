#include "world.hpp"

#include <algorithm>
#include <cmath>

#include "pathfinding.hpp"

namespace myrts {

namespace {
// How long a unit waits for a tile held by another unit before looking for a way around.
constexpr double kRepathDelay = 0.25;
}  // namespace

const char* facing_name(Facing f) {
  switch (f) {
    case Facing::Up: return "up";
    case Facing::UpRight: return "up_right";
    case Facing::Right: return "right";
    case Facing::DownRight: return "down_right";
    case Facing::Down: return "down";
    case Facing::DownLeft: return "down_left";
    case Facing::Left: return "left";
    case Facing::UpLeft: return "up_left";
  }
  return "down";
}

bool Unit::working() const {
  if (const auto* h = std::get_if<order::Harvest>(&order))
    return h->state == order::Harvest::State::Working && !h->gold;
  if (const auto* b = std::get_if<order::Build>(&order)) return b->state == order::Build::State::Building;
  return false;
}

// ---- setup --------------------------------------------------------------------

World::World(WorldConfig config, Grid land_blocked, Grid water_blocked) : config_(config), rng_(config.seed) {
  const int w = land_blocked.width();
  const int h = land_blocked.height();
  if (water_blocked.empty()) {  // no water on this map: nothing is sailable
    water_blocked = Grid(w, h);
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) water_blocked.set_blocked({x, y});
  }
  layer(MoveClass::Land) = {std::move(land_blocked), Occupancy(w, h)};
  layer(MoveClass::Water) = {std::move(water_blocked), Occupancy(w, h)};
  layer(MoveClass::Air) = {Grid(w, h), Occupancy(w, h)};
}

World::World(WorldConfig config, TileMap terrain) : World(config, terrain.land_blocked(), terrain.water_blocked()) {
  for (const Point p : terrain.tree_tiles()) trees_.add(p, config_.lumber_per_tree);
  terrain_ = std::move(terrain);
}

PlayerId World::add_player(std::string name, std::string faction, Stock stock) {
  players_.push_back({std::move(name), std::move(faction), stock});
  return static_cast<PlayerId>(players_.size()) - 1;
}

EntityId World::spawn_unit(const UnitDef& def, PlayerId owner, Point tile) {
  Layer& L = layer(def.move_class);
  if (L.blocked.is_blocked(tile)) return kNoEntity;
  const int ts = config_.tile_size;
  const EntityId id = units_.insert([&](EntityId new_id) {
    Unit u;
    u.id = new_id;
    u.def = &def;
    u.owner = owner;
    u.hp = def.max_hp;
    u.tile = tile;
    u.x = static_cast<float>(tile.x * ts + ts / 2);
    u.y = static_cast<float>(tile.y * ts + ts / 2);
    return u;
  });
  if (!L.occupancy.try_reserve(tile, static_cast<int>(id.index))) {
    units_.erase(id);
    return kNoEntity;
  }
  events_.publish(ev::UnitSpawned{id});
  return id;
}

EntityId World::add_building(const BuildingDef& def, PlayerId owner, Point origin, bool complete) {
  const EntityId id = buildings_.insert([&](EntityId new_id) {
    Building b;
    b.id = new_id;
    b.def = &def;
    b.owner = owner;
    b.origin = origin;
    b.complete = complete;
    b.progress = complete ? def.build_seconds : 0.0;
    b.hp = def.hp;
    return b;
  });
  for (const Point p : footprint(origin, def.size)) layer(MoveClass::Land).blocked.set_blocked(p);
  events_.publish(ev::BuildingPlaced{id, owner});
  if (complete) events_.publish(ev::BuildingCompleted{id, owner});
  return id;
}

void World::add_tree(Point tile, int lumber) {
  trees_.add(tile, lumber);
  layer(MoveClass::Land).blocked.set_blocked(tile);
}

int World::add_mine(Point origin, int size, int gold) {
  mines_.push_back({GoldMine(origin, size, gold), false});
  for (const Point p : mines_.back().site.tiles()) layer(MoveClass::Land).blocked.set_blocked(p);
  return static_cast<int>(mines_.size()) - 1;
}

// ---- queries --------------------------------------------------------------------

PlaceStatus World::placement_status(const BuildingDef& def, Point origin) const {
  if (!can_place(land().blocked, origin, def.size)) return PlaceStatus::Blocked;
  for (const Point p : footprint(origin, def.size))
    if (land().occupancy.owner(p) != Occupancy::kNone) return PlaceStatus::UnitInWay;
  return PlaceStatus::Ok;
}

EntityId World::building_at(Point tile) const {
  EntityId found;
  buildings_.for_each([&](const Building& b) {
    if (found.valid()) return;
    const int dx = tile.x - b.origin.x;
    const int dy = tile.y - b.origin.y;
    if (dx >= 0 && dy >= 0 && dx < b.def->size && dy < b.def->size) found = b.id;
  });
  return found;
}

EntityId World::unit_at(Point tile) const {
  EntityId found;
  units_.for_each([&](const Unit& u) {
    if (found.valid() || u.hidden) return;
    if (u.tile == tile || (u.moving && u.next == tile)) found = u.id;
  });
  return found;
}

int World::mine_at(Point tile) const {
  for (std::size_t i = 0; i < mines_.size(); ++i) {
    if (mines_[i].site.depleted()) continue;
    const auto tiles = mines_[i].site.tiles();
    if (std::ranges::find(tiles, tile) != tiles.end()) return static_cast<int>(i);
  }
  return -1;
}

std::vector<Point> World::depot_tiles(PlayerId owner) const {
  std::vector<Point> out;
  buildings_.for_each([&](const Building& b) {
    if (b.owner != owner || !b.complete || !b.def->is_depot) return;
    const auto tiles = footprint(b.origin, b.def->size);
    out.insert(out.end(), tiles.begin(), tiles.end());
  });
  return out;
}

int World::food_cap(PlayerId owner) const {
  int cap = 0;
  buildings_.for_each([&](const Building& b) { cap += (b.owner == owner && b.complete) ? b.def->food : 0; });
  return cap;
}

int World::food_used(PlayerId owner) const {
  int used = 0;
  units_.for_each([&](const Unit& u) { used += u.owner == owner ? u.def->food : 0; });
  buildings_.for_each([&](const Building& b) {
    if (b.owner != owner) return;
    for (const TrainItem& q : b.queue) used += q.unit->food;
  });
  return used;
}

bool World::has_assets(PlayerId owner) const {
  bool any = false;
  units_.for_each([&](const Unit& u) { any = any || u.owner == owner; });
  buildings_.for_each([&](const Building& b) { any = any || b.owner == owner; });
  return any;
}

// ---- movement ---------------------------------------------------------------------

void World::face(Unit& u, float dx, float dy) {
  const bool down = dy > 0.1f, up = dy < -0.1f, right = dx > 0.1f, left = dx < -0.1f;
  if (down && right) u.facing = Facing::DownRight;
  else if (down && left) u.facing = Facing::DownLeft;
  else if (up && right) u.facing = Facing::UpRight;
  else if (up && left) u.facing = Facing::UpLeft;
  else if (down) u.facing = Facing::Down;
  else if (up) u.facing = Facing::Up;
  else if (right) u.facing = Facing::Right;
  else if (left) u.facing = Facing::Left;
}

void World::face_tile(Unit& u, Point target) {
  face(u, static_cast<float>(target.x - u.tile.x), static_cast<float>(target.y - u.tile.y));
}

void World::stop_unit(Unit& u) {
  u.path.clear();
  u.blocked_time = 0.0;
}

void World::send_along(Unit& u, std::deque<Point> path) {
  if (!path.empty() && path.front() == u.anchor()) path.pop_front();
  u.path = std::move(path);
  if (!u.path.empty()) u.goal = u.path.back();
  u.blocked_time = 0.0;
}

void World::begin_step(Unit& u, Point next) {
  u.next = next;
  u.moving = true;
  u.blocked_time = 0.0;
}

void World::try_advance(Unit& u, double dt) {
  if (u.moving || u.path.empty()) return;
  Layer& L = layer_of(u);

  const Point next = u.path.front();
  if (L.occupancy.try_reserve(next, occ_id(u))) {
    u.path.pop_front();
    begin_step(u, next);
    return;
  }

  u.blocked_time += dt;
  if (u.blocked_time < kRepathDelay) return;

  // Another unit is in the way: look for a route around every tile held by others.
  const Grid others = L.occupancy.blocked_for(occ_id(u));
  auto detour = a_star(L.blocked, u.tile, u.goal, &others);
  if (detour.empty()) {
    u.path.clear();  // no way through; stay put
  } else {
    detour.pop_front();  // current tile
    u.path = std::move(detour);
  }
  u.blocked_time = 0.0;
}

void World::step_unit(Unit& u, double dt) {
  try_advance(u, dt);
  if (!u.moving) return;

  const int ts = config_.tile_size;
  const float tx = static_cast<float>(u.next.x * ts + ts / 2);
  const float ty = static_cast<float>(u.next.y * ts + ts / 2);
  const float dx = tx - u.x;
  const float dy = ty - u.y;
  const float dist = std::hypot(dx, dy);
  const float step = static_cast<float>(u.def->move_speed * dt);

  face(u, dx, dy);
  if (dist <= step) {
    u.x = tx;
    u.y = ty;
    layer_of(u).occupancy.release(u.tile, occ_id(u));
    u.tile = u.next;
    u.moving = false;
    try_advance(u, 0.0);  // chain into the next step without an idle frame
  } else {
    u.x += dx / dist * step;
    u.y += dy / dist * step;
  }
}

// ---- simulation ----------------------------------------------------------------------

void World::update(double dt) {
  for (const EntityId id : units_.ids())
    if (Unit* u = units_.get(id)) update_order(*u, dt);
  for (const EntityId id : units_.ids())
    if (Unit* u = units_.get(id)) step_unit(*u, dt);
  update_training(dt);
  events_.dispatch();
}

void World::kill_unit(EntityId id) {
  Unit* u = units_.get(id);
  if (!u) return;
  cancel_order(*u);
  Layer& L = layer_of(*u);
  L.occupancy.release(u->tile, occ_id(*u));
  L.occupancy.release(u->next, occ_id(*u));
  events_.publish(ev::UnitDied{id, u->owner, u->def, u->x, u->y, u->facing});
  units_.erase(id);
}

}  // namespace myrts
