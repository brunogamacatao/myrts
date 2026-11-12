#include "ai.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace myrts {

namespace {

double dist(Point a, Point b) { return std::hypot(static_cast<double>(a.x - b.x), static_cast<double>(a.y - b.y)); }

bool is_free(const Unit& u) { return std::holds_alternative<order::Idle>(u.order) && u.is_idle() && !u.hidden; }

}  // namespace

void Ai::update(double dt) {
  timer_ -= dt;
  if (timer_ > 0) return;
  timer_ = cfg_.think_interval;
  think();
}

void Ai::think() {
  manage_workers();
  manage_production();
  manage_army();
}

/// Idle workers go to the nearest gold mine, or to the nearest tree once the gold is gone.
void Ai::manage_workers() {
  std::vector<EntityId> idle;
  world_.units().for_each([&](const Unit& u) {
    if (u.owner == me_ && u.def->worker && is_free(u) && !u.locked()) idle.push_back(u.id);
  });

  for (const EntityId id : idle) {
    const Unit* u = world_.units().get(id);
    if (!u) continue;

    Point best{};
    double best_d = std::numeric_limits<double>::max();
    for (const Mine& m : world_.mines()) {
      if (m.site.depleted()) continue;
      const double d = dist(u->tile, m.site.origin());
      if (d < best_d) {
        best_d = d;
        best = m.site.origin();
      }
    }
    if (best_d == std::numeric_limits<double>::max()) {
      for (const Point p : world_.trees().positions()) {
        const double d = dist(u->tile, p);
        if (d < best_d) {
          best_d = d;
          best = p;
        }
      }
    }
    if (best_d == std::numeric_limits<double>::max()) continue;
    const std::array one{id};
    world_.order_harvest(one, best);
  }
}

/// Every finished building with an empty queue trains one useful unit it is able to afford.
void Ai::manage_production() {
  int workers = 0;
  world_.units().for_each([&](const Unit& u) { workers += (u.owner == me_ && u.def->worker) ? 1 : 0; });
  world_.buildings().for_each([&](const Building& b) {
    for (const TrainItem& q : b.queue) workers += (b.owner == me_ && q.unit->worker) ? 1 : 0;
  });

  for (const EntityId id : world_.buildings().ids()) {
    const Building* b = world_.buildings().get(id);
    if (!b || b->owner != me_ || !b->complete || !b->queue.empty()) continue;

    for (const std::string& unit_id : b->def->trains) {
      const UnitDef* def = registry_.unit(unit_id);
      if (!def) continue;
      const bool wanted = def->worker ? workers < cfg_.desired_workers : (def->aggressive && def->can_attack());
      if (!wanted) continue;
      if (world_.order_train(me_, id, *def).ok) {
        workers += def->worker ? 1 : 0;
        break;
      }
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
