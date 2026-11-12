#include "harvest.hpp"

#include <algorithm>
#include <cstdlib>

#include "pathfinding.hpp"

namespace myrts {

void TreeField::add(Point p, int lumber) {
  if (lumber > 0) trees_[p] = lumber;
}

int TreeField::remaining(Point p) const {
  const auto it = trees_.find(p);
  return it == trees_.end() ? 0 : it->second;
}

int TreeField::take(Point p, int amount) {
  const auto it = trees_.find(p);
  if (it == trees_.end() || amount <= 0) return 0;
  const int taken = std::min(amount, it->second);
  it->second -= taken;
  if (it->second <= 0) trees_.erase(it);
  return taken;
}

std::vector<Point> TreeField::positions() const {
  std::vector<Point> out;
  out.reserve(trees_.size());
  for (const auto& [p, _] : trees_) out.push_back(p);
  return out;
}

bool is_adjacent(Point a, Point b) {
  const int dx = std::abs(a.x - b.x);
  const int dy = std::abs(a.y - b.y);
  return a != b && dx <= 1 && dy <= 1;
}

namespace {
int dist2(Point a, Point b) {
  const int dx = a.x - b.x;
  const int dy = a.y - b.y;
  return dx * dx + dy * dy;
}
}  // namespace

std::optional<ApproachPath> path_to_adjacent(const Grid& grid, Point from,
                                             std::span<const Point> targets,
                                             const Grid* extra_blocked) {
  // Candidate (target, standing tile) pairs, nearest first so we usually A* only once or twice.
  struct Candidate {
    Point target;
    Point stand;
    int d;
  };
  std::vector<Candidate> candidates;
  for (const Point t : targets) {
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) continue;
        const Point s{t.x + dx, t.y + dy};
        if (grid.is_blocked(s)) continue;
        if (extra_blocked && s != from && extra_blocked->is_blocked(s)) continue;
        // Diagonal contact must not cut a blocked corner (the target tile itself is exempt).
        if (dx != 0 && dy != 0) {
          const Point c1{t.x + dx, t.y};
          const Point c2{t.x, t.y + dy};
          if (grid.is_blocked(c1) && grid.is_blocked(c2)) continue;
        }
        candidates.push_back({t, s, dist2(from, s)});
      }
    }
  }
  std::ranges::sort(candidates, {}, &Candidate::d);

  constexpr std::size_t kMaxSearches = 12;
  std::size_t searches = 0;
  for (const auto& c : candidates) {
    if (c.stand == from) return ApproachPath{c.target, {from}};
    if (searches++ >= kMaxSearches) break;
    auto path = a_star(grid, from, c.stand, extra_blocked);
    if (!path.empty()) return ApproachPath{c.target, std::move(path)};
  }
  return std::nullopt;
}

}  // namespace myrts
