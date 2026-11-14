#include <algorithm>
#include <cmath>

#include "pathfinding.hpp"
#include "world.hpp"

namespace myrts {

namespace {

int cheb(Point a, Point b) { return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)); }

}  // namespace

// ---- boarding -----------------------------------------------------------------------------------

void World::put_aboard(Unit& p, Unit& ship) {
  Layer& L = layer_of(p);
  L.occupancy.release(p.tile, occ_id(p));
  L.occupancy.release(p.next, occ_id(p));
  p.path.clear();
  p.moving = false;
  p.order = order::Idle{};
  p.aboard = ship.id;
  p.hidden = true;
  p.x = ship.x;
  p.y = ship.y;
  ship.passengers.push_back(p.id);
  events_.publish(ev::UnitBoarded{p.id, ship.id});
}

EntityId World::spawn_aboard(const UnitDef& def, PlayerId owner, EntityId transport) {
  Unit* ship = units_.get(transport);
  if (!ship || def.move_class != MoveClass::Land || static_cast<int>(ship->passengers.size()) >= ship->def->capacity)
    return kNoEntity;
  const Point ship_tile = ship->tile;
  const float ship_x = ship->x, ship_y = ship->y;
  const EntityId id = units_.insert([&](EntityId new_id) {
    Unit u;
    u.id = new_id;
    u.def = &def;
    u.owner = owner;
    u.hp = def.max_hp;
    u.tile = ship_tile;
    u.next = ship_tile;
    u.x = ship_x;
    u.y = ship_y;
    u.aboard = transport;
    u.hidden = true;
    return u;
  });
  if (Unit* again = units_.get(transport)) again->passengers.push_back(id);  // (the store may have grown: look the ship up again)
  return id;
}

Result World::order_board(std::span<const EntityId> ids, EntityId transport) {
  const Unit* ship = units_.get(transport);
  if (!ship || ship->def->capacity <= 0) return Result::fail("That is not a transport");
  std::vector<EntityId> free;
  free_units(ids, free);
  std::erase_if(free, [&](EntityId id) {
    const Unit& u = *units_.get(id);
    return u.owner != ship->owner || u.def->move_class != MoveClass::Land || id == transport;
  });
  if (free.empty()) return Result::fail("Only your own land units can board a transport");
  if (static_cast<int>(ship->passengers.size()) >= ship->def->capacity) return Result::fail("The transport is full");

  bool unreachable = false;
  for (const EntityId id : free) {
    Unit& u = *units_.get(id);
    cancel_order(u);
    stop_unit(u);
    const std::array<Point, 1> where{ship->anchor()};
    auto route = approach(u, where);
    if (!route) {
      unreachable = true;
      continue;
    }
    send_along(u, std::move(route->path));
    u.order = order::Board{transport, 0.0};
  }
  return unreachable ? Result::fail("Cannot reach the transport (it must be at a shore)") : Result::success();
}

void World::update_board(Unit& u, order::Board& b, double dt) {
  Unit* ship = units_.get(b.transport);
  if (!ship) return cancel_order(u);
  if (cheb(u.tile, ship->tile) <= 1 && !u.moving) {
    if (static_cast<int>(ship->passengers.size()) >= ship->def->capacity) {
      notice(u.owner, "The transport is full");
      return cancel_order(u);
    }
    put_aboard(u, *ship);
    return;
  }
  const bool arrived = u.is_idle();
  if (arrived || (b.timer += dt) >= config_.retry_delay) {  // the ship may have moved on: plan again
    b.timer = 0.0;
    const std::array<Point, 1> where{ship->anchor()};
    auto route = approach(u, where);
    if (!route) {
      notice(u.owner, "Cannot reach the transport");
      return cancel_order(u);
    }
    send_along(u, std::move(route->path));
  }
}

// ---- unloading ----------------------------------------------------------------------------------

Result World::order_unload(std::span<const EntityId> ids, Point shore) {
  if (!layer(MoveClass::Land).blocked.is_walkable(shore)) return Result::fail("Choose open ground on the shore to land the troops");
  bool any = false, unreachable = false;
  for (const EntityId id : ids) {
    Unit* ship = units_.get(id);
    if (!ship || ship->def->capacity <= 0 || ship->aboard.valid()) continue;
    any = true;
    if (ship->passengers.empty()) {
      notice(ship->owner, "Nobody is aboard");
      continue;
    }
    cancel_order(*ship);
    stop_unit(*ship);
    // The water tile beside the shore that is nearest to the ship.
    const Grid& sea = layer(MoveClass::Water).blocked;
    std::optional<Point> berth;
    int best = 1 << 30;
    for (int dy = -3; dy <= 3; ++dy)
      for (int dx = -3; dx <= 3; ++dx) {
        const Point p{shore.x + dx, shore.y + dy};
        if (!sea.is_walkable(p)) continue;
        const int d = cheb(p, ship->anchor()) * 8 + cheb(p, shore);
        if (d < best) best = d, berth = p;
      }
    if (!berth) {
      unreachable = true;
      continue;
    }
    send_along(*ship, a_star(sea, ship->anchor(), *berth));
    ship->order = order::Unload{shore, 0.0};
  }
  if (!any) return Result::fail("Select a transport");
  return unreachable ? Result::fail("The ship cannot get close to that shore") : Result::success();
}

void World::update_unload(Unit& u, order::Unload& o, double dt) {
  (void)dt;
  if (!u.is_idle()) return;  // still sailing
  const Point shore = o.shore;
  land_passengers(u, shore);
  cancel_order(u);
}

/// Puts the passengers on free land tiles around the ship (the ones nearest `near_tile` first).
void World::land_passengers(Unit& ship, Point near_tile) {
  Layer& land = layer(MoveClass::Land);
  std::vector<Point> spots;
  for (int dy = -2; dy <= 2; ++dy)
    for (int dx = -2; dx <= 2; ++dx) {
      const Point p{ship.tile.x + dx, ship.tile.y + dy};
      if (land.blocked.is_walkable(p) && land.occupancy.owner(p) == Occupancy::kNone) spots.push_back(p);
    }
  std::ranges::sort(spots, [&](Point a, Point b) { return cheb(a, near_tile) * 100 + cheb(a, ship.tile) < cheb(b, near_tile) * 100 + cheb(b, ship.tile); });

  int landed = 0;
  for (const EntityId pid : std::vector<EntityId>(ship.passengers)) {
    Unit* p = units_.get(pid);
    if (!p || spots.empty()) break;
    const Point at = spots.front();
    spots.erase(spots.begin());
    if (!land.occupancy.try_reserve(at, occ_id(*p))) continue;
    p->aboard = kNoEntity;
    p->hidden = false;
    p->tile = at;
    p->next = at;
    p->x = static_cast<float>(at.x * config_.tile_size + config_.tile_size / 2);
    p->y = static_cast<float>(at.y * config_.tile_size + config_.tile_size / 2);
    std::erase(ship.passengers, pid);
    events_.publish(ev::UnitLanded{pid, ship.id});
    ++landed;
  }
  if (landed == 0) notice(ship.owner, "There is no room to land here");
  else if (!ship.passengers.empty()) notice(ship.owner, "Some troops stayed aboard: no room on the shore");
}

}  // namespace myrts
