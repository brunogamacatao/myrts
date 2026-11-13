#include <set>
#include <algorithm>

#include "pathfinding.hpp"
#include "formation.hpp"
#include "world.hpp"

namespace myrts {

namespace {
Cargo cargo_of(ResourceKind kind) {
  return kind == ResourceKind::Gold ? Cargo::Gold : kind == ResourceKind::Oil ? Cargo::Oil : Cargo::Lumber;
}
}  // namespace


namespace {

template <class... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};

bool adjacent_to_any(Point p, std::span<const Point> tiles) {
  return std::ranges::any_of(tiles, [&](Point t) { return is_adjacent(p, t); });
}

bool contains(const std::vector<std::string>& v, const std::string& s) { return std::ranges::find(v, s) != v.end(); }

bool can_build(const Unit& u, const BuildingDef& def) {
  return u.def->worker && contains(u.def->builds, def.id);
}

}  // namespace

void World::notice(PlayerId to, std::string text) { events_.publish(ev::Notice{to, std::move(text)}); }

void World::free_units(std::span<const EntityId> in, std::vector<EntityId>& out) const {
  for (const EntityId id : in) {
    const Unit* u = units_.get(id);
    if (u && !u->locked()) out.push_back(id);
  }
}

/// Route to a tile next to one of `targets`, avoiding other units when possible. If they hem the
/// unit in, plan through them anyway: movement re-plans around whoever is still in the way.
std::optional<ApproachPath> World::approach(const Unit& u, std::span<const Point> targets) const {
  const Layer& L = layer(u.def->move_class);
  const Grid others = L.occupancy.blocked_for(occ_id(u));
  auto route = path_to_adjacent(L.blocked, u.anchor(), targets, &others);
  if (!route) route = path_to_adjacent(L.blocked, u.anchor(), targets);
  return route;
}

void World::cancel_order(Unit& u) {
  if (const auto* h = std::get_if<order::Harvest>(&u.order)) {
    if (h->kind == ResourceKind::Gold && h->mine >= 0 && h->state == order::Harvest::State::Working)
      mines_[static_cast<std::size_t>(h->mine)].busy = false;
  } else if (const auto* b = std::get_if<order::Build>(&u.order)) {
    if (Building* bld = buildings_.get(b->building); bld && bld->builder == u.id) bld->builder = kNoEntity;
  }
  u.order = order::Idle{};
  u.hidden = false;
}

// ---- player orders --------------------------------------------------------------------

Result World::order_stop(std::span<const EntityId> ids) {
  std::vector<EntityId> free;
  free_units(ids, free);
  for (const EntityId id : free) {
    Unit& u = *units_.get(id);
    cancel_order(u);
    stop_unit(u);
  }
  return Result::success();
}

/// Plans paths that bring `movers` (all free to move) to distinct tiles around `target`. Returns
/// each unit's destination, aligned with `movers` (nullopt: no place for it).
std::vector<std::optional<Point>> World::move_group(const std::vector<EntityId>& movers, Point target) {
  std::vector<std::optional<Point>> result(movers.size());
  for (const MoveClass cls : {MoveClass::Land, MoveClass::Water, MoveClass::Air}) {
    std::vector<std::size_t> group;  // indices into movers
    for (std::size_t i = 0; i < movers.size(); ++i)
      if (units_.get(movers[i])->def->move_class == cls) group.push_back(i);
    if (group.empty()) continue;

    Layer& L = layer(cls);
    std::vector<Point> anchors;
    std::vector<int> occ;
    for (const std::size_t i : group) {
      anchors.push_back(units_.get(movers[i])->anchor());
      occ.push_back(occ_id(*units_.get(movers[i])));
    }
    // Tiles held by units outside the group are obstacles; the group's own tiles are not.
    const Grid occupied = L.occupancy.blocked_except(occ);
    const auto destinations = assign_destinations(L.blocked, &occupied, anchors, target);
    for (std::size_t k = 0; k < group.size(); ++k) {
      Unit& u = *units_.get(movers[group[k]]);
      if (!destinations[k]) {
        stop_unit(u);
        continue;
      }
      // Plan over the terrain alone: other units move, so walls of them must not cause long detours (a unit that finds
      // someone in its way re-plans locally). If the spot cannot be reached, go as close as the terrain allows.
      auto path = a_star(L.blocked, anchors[k], *destinations[k]);
      if (path.empty()) path = a_star_closest(L.blocked, anchors[k], *destinations[k]);
      send_along(u, std::move(path));
      result[group[k]] = destinations[k];
    }
  }
  return result;
}

Result World::order_move(std::span<const EntityId> ids, Point target) {
  if (ids.empty()) return Result::success();
  std::vector<EntityId> movers;
  free_units(ids, movers);
  if (movers.empty()) return Result::fail("Builders cannot be interrupted");
  for (const EntityId id : movers) cancel_order(*units_.get(id));
  move_group(movers, target);
  return Result::success();
}

Result World::order_harvest(std::span<const EntityId> ids, Point tile) {
  if (ids.empty()) return Result::success();
  const int site = mine_at(tile);
  const ResourceKind kind = site >= 0 ? mines_[static_cast<std::size_t>(site)].kind() : ResourceKind::Lumber;
  if (site < 0 && !trees_.has_tree(tile)) {
    // Clicking beside or between trees picks the nearest one within a couple of tiles.
    const auto near = nearest_tree(tile, 2);
    if (!near) return Result::fail("Select a tree or a gold mine");
    return order_harvest(ids, *near);
  }

  std::vector<EntityId> free;
  free_units(ids, free);
  if (free.empty()) return Result::fail("Builders cannot be interrupted");

  bool any_worker = false, no_depot = false, unreachable = false;
  for (const EntityId id : free) {
    Unit& u = *units_.get(id);
    if (!u.def->worker) continue;
    any_worker = true;
    if (depot_tiles(u.owner, kind).empty()) {
      no_depot = true;
      continue;
    }
    cancel_order(u);
    stop_unit(u);
    const Cargo wanted = cargo_of(kind);
    if (u.carrying > 0 && u.cargo != wanted) {  // drops what it was carrying
      u.carrying = 0;
      u.cargo = Cargo::None;
    }
    if (!start_gathering(u, kind, tile)) unreachable = true;
  }
  if (!any_worker) return Result::fail("These units cannot gather resources");
  if (no_depot)
    return Result::fail(kind == ResourceKind::Oil ? "You need a Shipyard to deliver oil" : "You need a Great Hall or Town Hall to deliver resources");
  if (unreachable)
    return Result::fail(kind == ResourceKind::Gold ? "Cannot reach that gold mine"
                        : kind == ResourceKind::Oil ? "Cannot reach that oil patch" : "Cannot reach that tree");
  return Result::success();
}

Result World::order_build(PlayerId owner, std::span<const EntityId> candidates, const BuildingDef& def,
                          Point origin) {
  std::vector<EntityId> free;
  free_units(candidates, free);
  std::erase_if(free, [&](EntityId id) { return !can_build(*units_.get(id), def); });
  if (free.empty()) return Result::fail("The selected workers are busy or cannot build that");

  switch (placement_status(def, origin)) {
    case PlaceStatus::Blocked: return Result::fail("Cannot build there");
    case PlaceStatus::UnitInWay: return Result::fail("A unit is in the way");
    case PlaceStatus::Ok: break;
  }
  if (!pay(player(owner).stock, def.cost)) return Result::fail("Not enough resources for a " + def.name);

  const EntityId site = add_building(def, owner, origin, false);

  // The candidate with the shortest unobstructed walk builds it (others may be hemmed in).
  const auto tiles = footprint(origin, def.size);
  EntityId best = free.front();
  std::size_t best_len = static_cast<std::size_t>(-1);
  for (const EntityId id : free) {
    const Unit& u = *units_.get(id);
    const Layer& L = layer(u.def->move_class);
    const Grid others = L.occupancy.blocked_for(occ_id(u));
    const auto route = path_to_adjacent(L.blocked, u.anchor(), tiles, &others);
    if (route && route->path.size() < best_len) {
      best_len = route->path.size();
      best = id;
    }
  }
  Unit& builder = *units_.get(best);
  cancel_order(builder);
  stop_unit(builder);
  if (!start_building(builder, site)) return Result::fail("The builder cannot reach the construction site");
  return Result::success();
}

Result World::order_resume_build(std::span<const EntityId> ids, EntityId building) {
  Building* b = buildings_.get(building);
  if (!b || b->complete) return Result::success();
  if (b->builder.valid() && units_.alive(b->builder)) return Result::fail("Someone is already building it");

  std::vector<EntityId> free;
  free_units(ids, free);
  std::erase_if(free, [&](EntityId id) { return !can_build(*units_.get(id), *b->def); });
  if (free.empty()) return Result::fail("Builders cannot be interrupted");

  Unit& u = *units_.get(free.front());
  cancel_order(u);
  stop_unit(u);
  if (!start_building(u, building)) return Result::fail("Cannot reach the construction site");
  return Result::success();
}

// ---- gathering --------------------------------------------------------------------------

std::optional<Point> World::nearest_tree(Point from, int radius) const {
  std::optional<Point> best;
  int best_d = 0;
  for (int dy = -radius; dy <= radius; ++dy)
    for (int dx = -radius; dx <= radius; ++dx) {
      const Point p{from.x + dx, from.y + dy};
      const int d = dx * dx + dy * dy;
      if (trees_.has_tree(p) && (!best || d < best_d)) {
        best = p;
        best_d = d;
      }
    }
  return best;
}

/// The trees connected to `start` (touching, diagonals included), the clicked one first; at most `limit`.
std::vector<Point> World::forest_around(Point start, std::size_t limit) const {
  std::vector<Point> found{start};
  std::set<std::pair<int, int>> seen{{start.x, start.y}};
  for (std::size_t i = 0; i < found.size() && found.size() < limit; ++i) {
    const Point p = found[i];
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        const Point n{p.x + dx, p.y + dy};
        if (trees_.has_tree(n) && seen.insert({n.x, n.y}).second) found.push_back(n);
      }
  }
  return found;
}

bool World::start_gathering(Unit& u, ResourceKind kind, Point target) {
  std::vector<Point> targets;
  int mine = -1;
  if (kind != ResourceKind::Lumber) {
    mine = mine_at(target);
    if (mine < 0 || mines_[static_cast<std::size_t>(mine)].kind() != kind) return false;
    targets = mines_[static_cast<std::size_t>(mine)].site.tiles();
  } else {
    if (!trees_.has_tree(target)) return false;
    targets = forest_around(target, 400);  // trees inside the forest have no free side: the edge ones are reached
  }
  auto route = approach(u, targets);
  if (!route) return false;
  send_along(u, std::move(route->path));
  u.order = order::Harvest{order::Harvest::State::ToResource, kind, kind != ResourceKind::Lumber ? target : route->target, mine, 0.0};
  return true;
}

bool World::start_gathering_nearest(Unit& u, ResourceKind kind) {
  std::vector<Point> targets;
  if (kind != ResourceKind::Lumber) {
    for (const auto& m : mines_) {
      if (m.site.depleted() || m.kind() != kind) continue;
      const auto tiles = m.site.tiles();
      targets.insert(targets.end(), tiles.begin(), tiles.end());
    }
  } else {
    targets = trees_.positions();
  }
  auto route = approach(u, targets);
  if (!route) return false;
  send_along(u, std::move(route->path));
  u.order = order::Harvest{order::Harvest::State::ToResource, kind, route->target,
                           kind != ResourceKind::Lumber ? mine_at(route->target) : -1, 0.0};
  return true;
}

bool World::send_to_depot(Unit& u) {
  const auto* harvest = std::get_if<order::Harvest>(&u.order);
  const auto depots = depot_tiles(u.owner, harvest ? harvest->kind : ResourceKind::Gold);
  auto route = approach(u, depots);
  if (!route) return false;
  send_along(u, std::move(route->path));
  if (auto* h = std::get_if<order::Harvest>(&u.order)) {
    h->state = order::Harvest::State::ToDepot;
    h->timer = 0.0;
  }
  return true;
}

void World::fell_tree(Point tile) {
  Layer& L = layer(MoveClass::Land);
  if (!terrain_) {  // a world without real terrain: the tile just becomes free
    L.blocked.set_blocked(tile, false);
    events_.publish(ev::TreeFelled{tile});
    return;
  }

  // The tile's trees turn to grass and the tiles around it are redrawn to match; some of them may have lost their
  // last tree, which frees them too.
  const std::vector<Point> changed = terrain_->clear_trees(tile);
  for (const Point p : changed) {
    if (trees_.has_tree(p) && !terrain_->is_tree(p)) trees_.remove(p);
    const bool covered = building_at(p).valid() || mine_at(p) >= 0;
    L.blocked.set_blocked(p, covered || !land_passable(terrain_->movement(p)));
  }
  L.blocked.set_blocked(tile, building_at(tile).valid() || !land_passable(terrain_->movement(tile)));
  events_.publish(ev::TreeFelled{tile});
  if (!changed.empty()) events_.publish(ev::TerrainChanged{changed});
}

void World::exhaust_mine(int index) {
  const Mine& mine = mines_[static_cast<std::size_t>(index)];
  Layer& L = layer(mine.kind() == ResourceKind::Oil ? MoveClass::Water : MoveClass::Land);
  for (const Point p : mine.site.tiles()) L.blocked.set_blocked(p, false);
  events_.publish(ev::MineDepleted{index});
}

void World::update_order(Unit& u, double dt) {
  if (u.attack_flash > 0.0) u.attack_flash -= dt;
  std::visit(Overloaded{[&](order::Idle&) { auto_acquire(u, dt); },
                        [&](order::Harvest& h) { update_harvest(u, h, dt); },
                        [&](order::Build& b) { update_build(u, b, dt); },
                        [&](order::Attack& a) { update_attack(u, a, dt); },
                        [&](order::AttackMove& m) { update_attack_move(u, m, dt); }},
             u.order);
}

// NOTE: failing or re-planning replaces `u.order`, which destroys `h`. Every path below returns
// right after such a call and never touches `h` again.
void World::update_harvest(Unit& u, order::Harvest& h, double dt) {
  using State = order::Harvest::State;
  const WorkerDef worker = u.def->worker.value_or(WorkerDef{});
  const ResourceKind kind = h.kind;
  const bool from_site = h.from_site();
  const Point target = h.target;

  const auto fail = [&](const std::string& msg) {
    notice(u.owner, msg);
    cancel_order(u);
  };
  const auto retry = [&] {
    const bool alive = from_site ? mine_at(target) >= 0 : trees_.has_tree(target);
    const bool ok = alive ? start_gathering(u, kind, target) : start_gathering_nearest(u, kind);
    if (!ok)
      fail(kind == ResourceKind::Gold ? "There is no gold left to mine"
           : kind == ResourceKind::Oil ? "There is no oil left to pump" : "There are no more trees");
  };

  switch (h.state) {
    case State::ToResource: {
      if (!u.is_idle()) return;
      if (from_site) {
        const int mine = mine_at(target);
        if (mine < 0) return retry();
        Mine& m = mines_[static_cast<std::size_t>(mine)];
        const auto tiles = m.site.tiles();
        if (adjacent_to_any(u.tile, tiles)) {
          const bool goes_inside = kind == ResourceKind::Gold;  // workers disappear into a mine; tankers stay in sight
          if (goes_inside && m.busy) return;  // wait for the peon inside to come out
          m.busy = goes_inside;
          h.mine = mine;
          h.state = State::Working;
          h.timer = 0.0;
          u.hidden = goes_inside;
          return;
        }
      } else if (trees_.has_tree(target) && is_adjacent(u.tile, target)) {
        h.state = State::Working;
        h.timer = 0.0;
        face_tile(u, target);
        return;
      }
      // Arrived without reaching the resource (blocked route, or someone else used it up).
      if ((h.timer += dt) >= config_.retry_delay) {
        h.timer = 0.0;
        retry();
      }
      return;
    }
    case State::Working: {
      h.timer += dt;
      if (from_site) {
        const bool oil = kind == ResourceKind::Oil;
        if (h.timer < (oil ? worker.oil_seconds : worker.mine_seconds)) return;
        Mine& m = mines_[static_cast<std::size_t>(h.mine)];
        const int got = m.site.take(oil ? worker.oil_per_trip : worker.gold_per_trip);
        m.busy = false;
        u.hidden = false;
        u.cargo = cargo_of(kind);
        u.carrying = got;
        if (m.site.depleted()) {
          exhaust_mine(h.mine);
          notice(u.owner, oil ? "The oil patch is exhausted" : "The gold mine is exhausted");
        }
      } else {
        if (h.timer < worker.chop_seconds) return;
        const int got = trees_.take(target, worker.lumber_per_trip);
        if (got > 0 && !trees_.has_tree(target)) fell_tree(target);
        u.cargo = Cargo::Lumber;
        u.carrying = got;
      }
      if (u.carrying > 0) {
        if (!send_to_depot(u)) fail("Cannot reach a hall to deliver to");
      } else {
        retry();
      }
      return;
    }
    case State::ToDepot: {
      if (!u.is_idle()) return;
      const auto depots = depot_tiles(u.owner, kind);
      if (adjacent_to_any(u.tile, depots)) {
        Stock amount;
        (u.cargo == Cargo::Gold ? amount.gold : u.cargo == Cargo::Oil ? amount.oil : amount.lumber) = u.carrying;
        Stock& stock = player(u.owner).stock;
        stock.gold += amount.gold;
        stock.lumber += amount.lumber;
        stock.oil += amount.oil;
        events_.publish(ev::Delivered{u.owner, amount});
        u.carrying = 0;
        u.cargo = Cargo::None;
        return retry();
      }
      if ((h.timer += dt) >= config_.retry_delay && !send_to_depot(u)) fail("Cannot reach a hall to deliver to");
      return;
    }
  }
}

// ---- construction -------------------------------------------------------------------------

bool World::start_building(Unit& u, EntityId building) {
  Building* b = buildings_.get(building);
  if (!b) return false;
  const auto tiles = footprint(b->origin, b->def->size);
  auto route = approach(u, tiles);
  if (!route) return false;
  send_along(u, std::move(route->path));
  b->builder = u.id;
  u.order = order::Build{order::Build::State::ToSite, building, 0.0};
  return true;
}

void World::update_build(Unit& u, order::Build& o, double dt) {
  Building* b = buildings_.get(o.building);
  if (!b) {  // the site is gone
    cancel_order(u);
    return;
  }
  const auto tiles = footprint(b->origin, b->def->size);

  if (o.state == order::Build::State::ToSite) {
    if (!u.is_idle()) return;
    if (adjacent_to_any(u.tile, tiles)) {
      o.state = order::Build::State::Building;
      const auto it = std::ranges::find_if(tiles, [&](Point t) { return is_adjacent(u.tile, t); });
      face_tile(u, *it);
      return;
    }
    if ((o.timer += dt) >= config_.retry_delay) {
      const EntityId site = o.building;
      cancel_order(u);
      if (!start_building(u, site)) notice(u.owner, "Cannot reach the construction site");
    }
    return;
  }

  b->progress += dt * config_.build_speed;
  if (b->progress < b->def->build_seconds) return;
  b->progress = b->def->build_seconds;
  b->complete = true;
  const EntityId id = b->id;
  const PlayerId owner = b->owner;
  const std::string name = b->def->name;
  cancel_order(u);  // frees the builder slot and the unit
  events_.publish(ev::BuildingCompleted{id, owner});
  notice(owner, name + " completed");
}

}  // namespace myrts
