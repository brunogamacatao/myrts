#include "pathfinding.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

namespace myrts {
namespace {

constexpr double kSqrt2 = 1.41421356237309504880;

constexpr std::array<Point, 8> kDirections{{
    {1, 0}, {-1, 0}, {0, 1}, {0, -1},    // E, W, S, N
    {1, 1}, {-1, 1}, {1, -1}, {-1, -1},  // SE, SW, NE, NW
}};

// Octile distance: exact for 8-way movement with costs 1 and sqrt(2).
double heuristic(Point a, Point b) {
  const int dx = std::abs(a.x - b.x);
  const int dy = std::abs(a.y - b.y);
  return (dx + dy) + (kSqrt2 - 2.0) * std::min(dx, dy);
}

struct OpenNode {
  double f;
  std::size_t index;
  bool operator>(const OpenNode& o) const { return f > o.f; }
};

}  // namespace

std::deque<Point> a_star(const Grid& grid, Point start, Point goal, const Grid* extra_blocked) {
  const auto blocked = [&](Point p) {
    return grid.is_blocked(p) || (extra_blocked != nullptr && extra_blocked->is_blocked(p));
  };

  if (grid.is_blocked(start) || blocked(goal)) return {};
  if (start == goal) return {start};

  const std::size_t cells = static_cast<std::size_t>(grid.width()) * static_cast<std::size_t>(grid.height());
  constexpr double kInf = std::numeric_limits<double>::infinity();
  constexpr std::size_t kNone = std::numeric_limits<std::size_t>::max();

  std::vector<double> g(cells, kInf);
  std::vector<std::size_t> parent(cells, kNone);
  std::vector<bool> closed(cells, false);

  std::priority_queue<OpenNode, std::vector<OpenNode>, std::greater<>> open;

  const auto to_point = [&](std::size_t i) {
    return Point{static_cast<int>(i % static_cast<std::size_t>(grid.width())),
                 static_cast<int>(i / static_cast<std::size_t>(grid.width()))};
  };

  const std::size_t start_i = grid.index(start);
  const std::size_t goal_i = grid.index(goal);
  g[start_i] = 0.0;
  open.push({heuristic(start, goal), start_i});

  while (!open.empty()) {
    const auto [f, ci] = open.top();
    open.pop();
    if (closed[ci]) continue;  // stale queue entry
    closed[ci] = true;

    if (ci == goal_i) {
      std::deque<Point> path;
      for (std::size_t i = goal_i; i != kNone; i = parent[i]) path.push_front(to_point(i));
      return path;
    }

    const Point current = to_point(ci);
    for (const Point d : kDirections) {
      const Point nb{current.x + d.x, current.y + d.y};
      if (blocked(nb)) continue;

      const bool diagonal = d.x != 0 && d.y != 0;
      if (diagonal && (blocked({current.x + d.x, current.y}) || blocked({current.x, current.y + d.y}))) {
        continue;  // no corner cutting
      }

      const std::size_t ni = grid.index(nb);
      if (closed[ni]) continue;

      const double tentative = g[ci] + (diagonal ? kSqrt2 : 1.0);
      if (tentative < g[ni]) {
        g[ni] = tentative;
        parent[ni] = ci;
        open.push({tentative + heuristic(nb, goal), ni});
      }
    }
  }
  return {};
}

}  // namespace myrts
