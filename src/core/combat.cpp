#include <algorithm>
#include <array>
#include <cmath>

#include "pathfinding.hpp"
#include "world.hpp"

namespace myrts {

namespace {

int chebyshev(Point a, Point b) { return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)); }

double distance(Point a, Point b) {
  return std::hypot(static_cast<double>(a.x - b.x), static_cast<double>(a.y - b.y));
}

bool trains(const BuildingDef& b, const UnitDef& u) { return std::ranges::find(b.trains, u.id) != b.trains.end(); }

}  // namespace

// ---- player orders ---------------------------------------------------------------------------

Result World::order_attack(std::span<const EntityId> ids, Point tile) {
  if (ids.empty()) return Result::success();
  std::vector<EntityId> fighters;
  free_units(ids, fighters);
  std::erase_if(fighters, [&](EntityId id) { return !units_.get(id)->def->can_attack(); });
  if (fighters.empty()) return Result::fail("These units cannot attack");

  const PlayerId owner = units_.get(fighters.front())->owner;
  order::Target target;
  if (const EntityId e = unit_at(tile); e.valid() && is_enemy(owner, units_.get(e)->owner)) {
    target = {order::Target::Kind::Unit, e};
  } else if (const EntityId b = building_at(tile); b.valid() && is_enemy(owner, buildings_.get(b)->owner)) {
    target = {order::Target::Kind::Building, b};
  } else {
    return order_attack_move(fighters, tile);  // nothing to hit there: advance and fight on the way
  }

  for (const EntityId id : fighters) {
    Unit& u = *units_.get(id);
    cancel_order(u);
    stop_unit(u);
    begin_attack(u, target, std::nullopt);
  }
  return Result::success();
}

Result World::order_attack_move(std::span<const EntityId> ids, Point tile) {
  if (ids.empty()) return Result::success();
  std::vector<EntityId> fighters;
  free_units(ids, fighters);
  std::erase_if(fighters, [&](EntityId id) { return !units_.get(id)->def->can_attack(); });
  if (fighters.empty()) return Result::fail("These units cannot attack");

  for (const EntityId id : fighters) cancel_order(*units_.get(id));
  const auto destinations = move_group(fighters, tile);
  for (std::size_t i = 0; i < fighters.size(); ++i)
    if (destinations[i]) units_.get(fighters[i])->order = order::AttackMove{*destinations[i], 0.0};
  return Result::success();
}

Result World::order_train(PlayerId owner, EntityId building, const UnitDef& unit) {
  Building* b = buildings_.get(building);
  if (!b || b->owner != owner || !b->complete) return Result::fail("That building cannot train units yet");
  if (!trains(*b->def, unit)) return Result::fail(b->def->name + " cannot train a " + unit.name);
  if (b->queue.size() >= static_cast<std::size_t>(config_.max_queue)) return Result::fail("The training queue is full");
  if (food_used(owner) + unit.food > food_cap(owner)) return Result::fail("Not enough food: build more farms");
  if (!pay(player(owner).stock, unit.cost)) return Result::fail("Not enough resources for a " + unit.name);
  b->queue.push_back({&unit, 0.0});
  return Result::success();
}

Result World::order_rally(PlayerId owner, EntityId building, Point tile) {
  Building* b = buildings_.get(building);
  if (!b || b->owner != owner) return Result::fail("Select one of your buildings");
  if (b->def->trains.empty()) return Result::fail("This building does not train units");
  if (!layer(MoveClass::Land).blocked.contains(tile)) return Result::fail("Cannot rally there");
  b->rally = tile;
  return Result::success();
}

Result World::order_cancel_train(PlayerId owner, EntityId building, std::size_t index) {
  Building* b = buildings_.get(building);
  if (!b || b->owner != owner || index >= b->queue.size()) return Result::fail("Nothing to cancel");
  const Stock refund = b->queue[index].unit->cost;
  Stock& stock = player(owner).stock;
  stock.gold += refund.gold;
  stock.lumber += refund.lumber;
  stock.oil += refund.oil;
  b->queue.erase(b->queue.begin() + static_cast<std::ptrdiff_t>(index));
  return Result::success();
}

// ---- targets -------------------------------------------------------------------------------------

bool World::target_tiles(const order::Target& t, std::vector<Point>& out) const {
  out.clear();
  if (t.kind == order::Target::Kind::Unit) {
    const Unit* u = units_.get(t.id);
    if (!u) return false;
    out.push_back(u->tile);
    return true;
  }
  const Building* b = buildings_.get(t.id);
  if (!b) return false;
  out = footprint(b->origin, b->def->size);
  return true;
}

/// The closest enemy in sight: units first (they hit back), then buildings.
std::optional<order::Target> World::find_target(const Unit& u) const {
  const double sight = std::max(1, u.def->stats.sight);

  std::optional<order::Target> best;
  double best_d = sight + 1;
  units_.for_each([&](const Unit& o) {
    if (!is_enemy(u.owner, o.owner) || o.hidden) return;
    const double d = distance(u.tile, o.tile);
    if (d <= sight && d < best_d) {
      best_d = d;
      best = order::Target{order::Target::Kind::Unit, o.id};
    }
  });
  if (best) return best;

  buildings_.for_each([&](const Building& b) {
    if (!is_enemy(u.owner, b.owner)) return;
    for (const Point p : footprint(b.origin, b.def->size)) {
      const double d = distance(u.tile, p);
      if (d <= sight && d < best_d) {
        best_d = d;
        best = order::Target{order::Target::Kind::Building, b.id};
      }
    }
  });
  return best;
}

void World::begin_attack(Unit& u, const order::Target& target, std::optional<Point> resume) {
  u.order = order::Attack{target, resume, config_.retry_delay};  // plan the first route at once
}

void World::end_attack(Unit& u, std::optional<Point> resume) {
  u.order = order::Idle{};
  if (!resume) return;
  // An attack-move carries on to where it was going.
  const Layer& L = layer(u.def->move_class);
  const Grid others = L.occupancy.blocked_for(occ_id(u));
  auto path = a_star(L.blocked, u.anchor(), *resume, &others);
  if (path.empty()) return;
  send_along(u, std::move(path));
  u.order = order::AttackMove{*resume, 0.0};
}

// ---- per-tick behaviour -------------------------------------------------------------------------

void World::auto_acquire(Unit& u, double dt) {
  if (!u.def->aggressive || !u.def->can_attack() || !u.is_idle()) return;
  u.scan_timer -= dt;
  if (u.scan_timer > 0) return;
  u.scan_timer = config_.scan_interval;
  if (const auto target = find_target(u)) begin_attack(u, *target, std::nullopt);
}

void World::update_attack_move(Unit& u, order::AttackMove& m, double dt) {
  m.scan -= dt;
  if (m.scan <= 0) {
    m.scan = config_.scan_interval;
    if (const auto target = find_target(u)) {
      begin_attack(u, *target, m.dest);  // `m` is gone after this
      return;
    }
  }
  if (u.is_idle()) u.order = order::Idle{};  // arrived (or could not go on)
}

// NOTE: end_attack() / begin_attack() replace `u.order` and destroy `a`; nothing touches `a` after them.
void World::update_attack(Unit& u, order::Attack& a, double dt) {
  const order::Target target = a.target;
  const std::optional<Point> resume = a.resume;

  std::vector<Point> tiles;
  if (!target_tiles(target, tiles)) return end_attack(u, resume);  // already dead

  const int range = std::max(1, u.def->stats.range);
  Point nearest = tiles.front();
  int d = chebyshev(u.tile, nearest);
  for (const Point p : tiles) {
    if (const int dd = chebyshev(u.tile, p); dd < d) {
      d = dd;
      nearest = p;
    }
  }

  if (d <= range) {
    stop_unit(u);
    if (u.moving) return;  // finish the step first
    face_tile(u, nearest);
    u.cooldown -= dt;
    if (u.cooldown <= 0) {
      u.cooldown = u.def->attack_seconds;
      strike(u, target);
    }
    return;
  }

  a.retry += dt;
  if (u.is_idle() || a.retry >= config_.retry_delay) {
    a.retry = 0.0;
    auto route = approach(u, tiles);
    if (!route) return end_attack(u, resume);  // unreachable
    send_along(u, std::move(route->path));
  }
}

void World::strike(Unit& attacker, const order::Target& target) {
  const UnitStats& s = attacker.def->stats;
  std::uniform_int_distribution<int> roll(s.damage_min, std::max(s.damage_min, s.damage_max));
  const int raw = roll(rng_);
  attacker.attack_flash = 0.45;

  if (target.kind == order::Target::Kind::Unit) {
    Unit* t = units_.get(target.id);
    if (!t) return;
    const int dmg = std::max(1, raw - t->def->stats.armor);
    t->hp -= dmg;
    events_.publish(ev::Attacked{attacker.id, target.id, false, dmg});
    if (t->hp <= 0) {
      kill_unit(target.id);
    } else if (t->def->can_attack() && std::holds_alternative<order::Idle>(t->order)) {  // anyone armed fights back
      begin_attack(*t, {order::Target::Kind::Unit, attacker.id}, std::nullopt);  // fight back
    }
    return;
  }

  Building* b = buildings_.get(target.id);
  if (!b) return;
  const int dmg = std::max(1, raw - b->def->armor);
  b->hp -= dmg;
  events_.publish(ev::Attacked{attacker.id, target.id, true, dmg});
  if (b->hp <= 0) destroy_building(target.id);
}

void World::destroy_building(EntityId id) {
  Building* b = buildings_.get(id);
  if (!b) return;
  for (const Point p : footprint(b->origin, b->def->size)) layer(MoveClass::Land).blocked.set_blocked(p, false);
  if (Unit* builder = units_.get(b->builder)) cancel_order(*builder);
  const PlayerId owner = b->owner;
  const std::string name = b->def->name;
  buildings_.erase(id);
  events_.publish(ev::BuildingDestroyed{id, owner});
  notice(owner, "Your " + name + " was destroyed");
}

// ---- training ---------------------------------------------------------------------------------------

std::optional<Point> World::free_spawn_tile(const Building& b, MoveClass cls) const {
  const Layer& L = layer(cls);
  const int size = b.def->size;
  for (int r = 1; r <= 4; ++r) {  // rings around the footprint, nearest first
    for (int y = b.origin.y - r; y < b.origin.y + size + r; ++y) {
      for (int x = b.origin.x - r; x < b.origin.x + size + r; ++x) {
        const bool on_ring = x == b.origin.x - r || x == b.origin.x + size + r - 1 || y == b.origin.y - r ||
                             y == b.origin.y + size + r - 1;
        if (!on_ring) continue;
        const Point p{x, y};
        if (L.blocked.is_walkable(p) && L.occupancy.owner(p) == Occupancy::kNone) return p;
      }
    }
  }
  return std::nullopt;
}

void World::update_training(double dt) {
  for (const EntityId id : buildings_.ids()) {
    Building* b = buildings_.get(id);
    if (!b || !b->complete || b->queue.empty()) continue;
    TrainItem& item = b->queue.front();
    item.progress += dt * config_.train_speed;
    if (item.progress < item.unit->train_seconds) continue;

    const UnitDef& def = *item.unit;
    const auto tile = free_spawn_tile(*b, def.move_class);
    if (!tile) {  // no room around the building: the unit waits inside
      item.progress = def.train_seconds;
      continue;
    }
    const PlayerId owner = b->owner;
    const std::optional<Point> rally = b->rally;
    b->queue.erase(b->queue.begin());
    const EntityId spawned = spawn_unit(def, owner, *tile);
    notice(owner, def.name + " ready");

    // Head for the rally point; workers sent to a tree or a mine start gathering.
    if (spawned.valid() && rally) {
      const std::array one{spawned};
      if (def.worker && (trees_.has_tree(*rally) || mine_at(*rally) >= 0))
        order_harvest(one, *rally);
      else
        order_move(one, *rally);
    }
  }
}

}  // namespace myrts
