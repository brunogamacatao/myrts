#include "formation.hpp"

#include <algorithm>
#include <cstdlib>
#include <tuple>

namespace myrts {
namespace {

long dist2(Point a, Point b) {
  const long dx = a.x - b.x;
  const long dy = a.y - b.y;
  return dx * dx + dy * dy;
}

// Nearest walkable cell to `target` (searching outwards in square rings).
std::optional<Point> nearest_walkable(const Grid& grid, Point target) {
  if (grid.is_walkable(target)) return target;
  const int max_radius = std::max(grid.width(), grid.height());
  for (int r = 1; r <= max_radius; ++r) {
    std::optional<Point> best;
    for (int dy = -r; dy <= r; ++dy) {
      for (int dx = -r; dx <= r; ++dx) {
        if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
        const Point p{target.x + dx, target.y + dy};
        if (grid.is_walkable(p) && (!best || dist2(p, target) < dist2(*best, target))) best = p;
      }
    }
    if (best) return best;
  }
  return std::nullopt;
}

}  // namespace

std::vector<std::optional<Point>> assign_destinations(const Grid& grid, const Grid* occupied,
                                                      std::span<const Point> units, Point target) {
  std::vector<std::optional<Point>> result(units.size());
  if (units.empty() || grid.empty()) return result;

  const auto center = nearest_walkable(grid, target);
  if (!center) return result;

  const auto is_free = [&](Point p) { return !occupied || occupied->is_walkable(p); };

  // Breadth-first flood over walkable cells, one full level at a time, until enough free slots exist.
  std::vector<Point> slots;
  std::vector<bool> seen(static_cast<std::size_t>(grid.width()) * static_cast<std::size_t>(grid.height()), false);
  std::vector<Point> level{*center};
  seen[grid.index(*center)] = true;
  while (!level.empty() && slots.size() < units.size()) {
    std::vector<Point> next;
    for (const Point p : level) {
      if (is_free(p)) slots.push_back(p);
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          const Point n{p.x + dx, p.y + dy};
          if (grid.is_blocked(n) || seen[grid.index(n)]) continue;
          seen[grid.index(n)] = true;
          next.push_back(n);
        }
      }
    }
    level = std::move(next);
  }

  // Keep the slots closest to the centre (ties broken deterministically).
  std::ranges::sort(slots, [&](Point a, Point b) {
    return std::tuple(dist2(a, *center), a.y, a.x) < std::tuple(dist2(b, *center), b.y, b.x);
  });
  if (slots.size() > units.size()) slots.resize(units.size());

  // Greedy matching by increasing unit-slot distance.
  struct Pair {
    long d;
    std::size_t unit, slot;
  };
  std::vector<Pair> pairs;
  pairs.reserve(units.size() * slots.size());
  for (std::size_t u = 0; u < units.size(); ++u) {
    for (std::size_t s = 0; s < slots.size(); ++s) pairs.push_back({dist2(units[u], slots[s]), u, s});
  }
  std::ranges::sort(pairs, [](const Pair& a, const Pair& b) {
    return std::tuple(a.d, a.unit, a.slot) < std::tuple(b.d, b.unit, b.slot);
  });

  std::vector<bool> slot_used(slots.size(), false);
  for (const Pair& p : pairs) {
    if (result[p.unit] || slot_used[p.slot]) continue;
    result[p.unit] = slots[p.slot];
    slot_used[p.slot] = true;
  }
  return result;
}

}  // namespace myrts
