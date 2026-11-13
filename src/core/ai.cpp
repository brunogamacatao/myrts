#include "ai.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>

namespace myrts {

namespace {

double dist(Point a, Point b) { return std::hypot(static_cast<double>(a.x - b.x), static_cast<double>(a.y - b.y)); }

bool is_water_worker(const UnitDef& d) { return d.worker && d.move_class == MoveClass::Water; }

bool has_oil(const World& world) {
  return std::ranges::any_of(world.mines(), [](const Mine& m) { return m.kind() == ResourceKind::Oil && !m.site.depleted(); });
}

bool is_free(const Unit& u) { return std::holds_alternative<order::Idle>(u.order) && u.is_idle() && !u.hidden; }

}  // namespace

void Ai::update(double dt) {
  elapsed_ += dt;
  timer_ -= dt;
  if (timer_ > 0) return;
  timer_ = cfg_.think_interval;
  think();
}

void Ai::think() {
  manage_workers();
  manage_tankers();
  manage_construction();
  manage_production();
  manage_army();
}

namespace {

/// The farm a worker of this player can build: provides food and is not a depot.
const BuildingDef* farm_def(const Registry& registry, const UnitDef& worker) {
  for (const std::string& id : worker.builds) {
    const BuildingDef* b = registry.building(id);
    if (b && b->food > 0 && !b->is_depot) return b;
  }
  return nullptr;
}

}  // namespace

/// Workers mine the nearest gold; when a farm is needed and there is no lumber, one of them cuts
/// wood instead, and goes back to gold once there is plenty.
void Ai::manage_workers() {
  std::vector<EntityId> workers;
  int lumberjacks = 0;
  const UnitDef* any_worker = nullptr;
  world_.units().for_each([&](const Unit& u) {
    if (u.owner != me_ || !u.def->worker || is_water_worker(*u.def) || u.locked()) return;
    workers.push_back(u.id);
    any_worker = u.def;
    if (const auto* h = std::get_if<order::Harvest>(&u.order)) lumberjacks += h->kind == ResourceKind::Lumber ? 1 : 0;
  });
  if (workers.empty()) return;

  const BuildingDef* farm = farm_def(registry_, *any_worker);
  const int lumber = world_.player(me_).stock.lumber;
  const bool want_wood = farm && lumber < farm->cost.lumber;
  const bool too_much_wood = farm && lumber >= 3 * std::max(1, farm->cost.lumber);

  const auto nearest_tree = [&](Point from) -> std::optional<Point> {
    std::optional<Point> best;
    double best_d = std::numeric_limits<double>::max();
    for (const Point p : world_.trees().positions())
      if (const double d = dist(from, p); d < best_d) {
        best_d = d;
        best = p;
      }
    return best;
  };
  const auto nearest_mine = [&](Point from) -> std::optional<Point> {
    std::optional<Point> best;
    double best_d = std::numeric_limits<double>::max();
    for (const Mine& m : world_.mines())
      if (!m.site.depleted() && m.kind() == ResourceKind::Gold)
        if (const double d = dist(from, m.site.origin()); d < best_d) {
          best_d = d;
          best = m.site.origin();
        }
    return best;
  };

  // Put one worker on wood when needed (preferably one that is idle), or back on gold when swimming in it.
  if (want_wood && lumberjacks == 0) {
    EntityId pick = workers.front();
    for (const EntityId id : workers)
      if (is_free(*world_.units().get(id))) pick = id;
    if (const auto tree = nearest_tree(world_.units().get(pick)->tile)) {
      const std::array one{pick};
      if (world_.order_harvest(one, *tree).ok) ++lumberjacks;
    }
  } else if (too_much_wood && lumberjacks > 0) {
    for (const EntityId id : workers) {
      const Unit* u = world_.units().get(id);
      const auto* h = std::get_if<order::Harvest>(&u->order);
      if (h && h->kind == ResourceKind::Lumber && u->carrying == 0)
        if (const auto mine = nearest_mine(u->tile)) {
          const std::array one{id};
          world_.order_harvest(one, *mine);
          break;
        }
    }
  }

  // Everyone else who is idle goes to gold (or to wood when the gold has run out).
  for (const EntityId id : workers) {
    const Unit* u = world_.units().get(id);
    if (!u || !is_free(*u)) continue;
    auto target = nearest_mine(u->tile);
    if (!target) target = nearest_tree(u->tile);
    if (!target) continue;
    const std::array one{id};
    world_.order_harvest(one, *target);
  }
}

/// Tankers pump the nearest oil patch and bring the oil to a shipyard; they wait while there is no oil depot yet.
void Ai::manage_tankers() {
  if (world_.depot_tiles(me_, ResourceKind::Oil).empty()) return;
  world_.units().for_each([&](const Unit& u) {
    if (u.owner != me_ || !is_water_worker(*u.def) || !is_free(u)) return;
    std::optional<Point> best;
    double best_d = std::numeric_limits<double>::max();
    for (const Mine& m : world_.mines())
      if (m.kind() == ResourceKind::Oil && !m.site.depleted())
        if (const double d = dist(u.tile, m.site.origin()); d < best_d) {
          best_d = d;
          best = m.site.origin();
        }
    if (!best) return;
    const std::array one{u.id};
    world_.order_harvest(one, *best);
  });
}

/// One building at a time, on free ground near the first depot: a farm whenever food is about to run out, or else a
/// coastal oil depot (shipyard) when the map has oil to pump and the player has none yet.
void Ai::manage_construction() {
  bool building = false;
  bool oil_depot = false;  // built or being built
  Point centre{};
  bool have_depot = false;
  world_.buildings().for_each([&](const Building& b) {
    if (b.owner != me_) return;
    building = building || !b.complete;
    oil_depot = oil_depot || b.def->accepts_oil;
    if (b.complete && b.def->is_depot && !have_depot) {
      centre = {b.origin.x + b.def->size / 2, b.origin.y + b.def->size / 2};
      have_depot = true;
    }
  });
  if (building || !have_depot) return;

  EntityId builder;
  const UnitDef* worker_def = nullptr;
  world_.units().for_each([&](const Unit& u) {
    if (u.owner != me_ || !u.def->worker || is_water_worker(*u.def) || u.locked() || builder.valid()) return;
    builder = u.id;
    worker_def = u.def;
  });
  if (!builder.valid()) return;

  const BuildingDef* want = nullptr;
  if (world_.food_cap(me_) - world_.food_used(me_) <= cfg_.farm_margin) want = farm_def(registry_, *worker_def);
  if (!want && !oil_depot && has_oil(world_)) {
    for (const std::string& id : worker_def->builds) {
      const BuildingDef* b = registry_.building(id);
      if (b && b->accepts_oil && b->placement == Placement::Coast) want = b;
    }
  }
  if (!want || !can_afford(world_.player(me_).stock, want->cost)) return;

  // The free spot closest to the depot. Land buildings keep a gap of one tile around them so units can pass; a
  // shipyard has to find a shore, which may be further away.
  const int reach = want->placement == Placement::Land ? 10 : 30;
  std::optional<Point> spot;
  double best_d = std::numeric_limits<double>::max();
  for (int y = centre.y - reach; y <= centre.y + reach; ++y)
    for (int x = centre.x - reach; x <= centre.x + reach; ++x) {
      const Point origin{x, y};
      if (world_.placement_status(*want, origin) != PlaceStatus::Ok) continue;
      if (want->placement == Placement::Land && !can_place(world_.grid(MoveClass::Land), {x - 1, y - 1}, want->size + 2))
        continue;
      const double d = dist(centre, origin);
      if (d < best_d) {
        best_d = d;
        spot = origin;
      }
    }
  if (!spot) return;
  world_.order_build(me_, std::array{builder}, *want, *spot);
}

/// Every finished building with an empty queue trains the useful unit it has the fewest of, so the army
/// ends up varied (footmen and archers, knights, gryphons...). Buildings wait until they can afford it.
void Ai::manage_production() {
  std::unordered_map<std::string, int> have;  // living + queued, by unit id
  int workers = 0, tankers = 0;
  const auto count = [&](const UnitDef& d) {
    if (is_water_worker(d)) ++tankers;
    else if (d.worker) ++workers;
  };
  world_.units().for_each([&](const Unit& u) {
    if (u.owner != me_) return;
    ++have[u.def->id];
    count(*u.def);
  });
  world_.buildings().for_each([&](const Building& b) {
    if (b.owner != me_) return;
    for (const TrainItem& q : b.queue) {
      ++have[q.unit->id];
      count(*q.unit);
    }
  });

  for (const EntityId id : world_.buildings().ids()) {
    const Building* b = world_.buildings().get(id);
    if (!b || b->owner != me_ || !b->complete || !b->queue.empty()) continue;

    const UnitDef* pick = nullptr;
    for (const std::string& unit_id : b->def->trains) {
      const UnitDef* def = registry_.unit(unit_id);
      if (!def) continue;
      const bool wanted = is_water_worker(*def) ? (tankers < cfg_.desired_tankers && has_oil(world_) &&
                                                   !world_.depot_tiles(me_, ResourceKind::Oil).empty())
                          : def->worker  ? workers < cfg_.desired_workers
                                         : (def->aggressive && def->can_attack());
      if (!wanted) continue;
      if (!pick || have[def->id] < have[pick->id]) pick = def;
    }
    if (pick && world_.order_train(me_, id, *pick).ok) {
      ++have[pick->id];
      count(*pick);
    }
  }
}

/// When enough fighters are standing around, send them all against the nearest enemy building.
void Ai::manage_army() {
  std::vector<EntityId> ready;
  Point centre{};
  world_.units().for_each([&](const Unit& u) {
    if (u.owner != me_ || !u.def->aggressive || !u.def->can_attack() || !is_free(u)) return;
    ready.push_back(u.id);
    centre.x += u.tile.x;
    centre.y += u.tile.y;
  });
  if (elapsed_ < cfg_.start_delay) return;
  const int needed = cfg_.first_wave + cfg_.wave_growth * waves_;
  if (ready.empty() || static_cast<int>(ready.size()) < needed) return;
  centre = {centre.x / static_cast<int>(ready.size()), centre.y / static_cast<int>(ready.size())};

  std::optional<Point> target;
  double best_d = std::numeric_limits<double>::max();
  world_.buildings().for_each([&](const Building& b) {
    if (!World::is_enemy(me_, b.owner)) return;
    const double d = dist(centre, b.origin);
    if (d < best_d) {
      best_d = d;
      target = b.origin;
    }
  });
  if (!target) {  // no structures left: hunt the remaining units
    world_.units().for_each([&](const Unit& u) {
      if (!World::is_enemy(me_, u.owner)) return;
      const double d = dist(centre, u.tile);
      if (d < best_d) {
        best_d = d;
        target = u.tile;
      }
    });
  }
  if (!target) return;

  if (world_.order_attack_move(ready, *target).ok) ++waves_;
}

}  // namespace myrts
