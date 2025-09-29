#include <queue>
#include <unordered_map>
#include <cmath>
#include <algorithm>

#include "pathfinding.hpp"

// Hash function for Point (to use in unordered_map / unordered_set)
struct PointHash {
  std::size_t operator()(const Point& p) const noexcept {
    return std::hash<int>()(p.x * 73856093 ^ p.y * 19349663);
  }
};

// Movement directions (8-way)
const std::vector<Point> directions = {
  {1, 0}, {-1, 0}, {0, 1}, {0, -1},   // N, S, E, W
  {1, 1}, {-1, 1}, {1, -1}, {-1, -1}  // NE, NW, SE, SW
};

// Heuristic: Octile distance (works for 8-direction grids)
inline double heuristic(const Point& a, const Point& b) {
  int dx = std::abs(a.x - b.x);
  int dy = std::abs(a.y - b.y);
  return (dx + dy) + (std::sqrt(2.0) - 2.0) * std::min(dx, dy);
}

std::deque<Point> a_star(const std::vector<std::vector<int>>& grid, const Point& start, const Point& goal) {
  using Node = std::pair<double, Point>; // (f_score, point)

  struct NodeCompare {
    bool operator()(const Node& a, const Node& b) const {
      return a.first > b.first; // min-heap by f
    }
  };

  std::priority_queue<Node, std::vector<Node>, NodeCompare> open_set;
  std::unordered_map<Point, Point, PointHash> came_from;
  std::unordered_map<Point, double, PointHash> g_score;

  g_score[start] = 0.0;
  open_set.push({heuristic(start, goal), start});

  const int rows = (int)grid.size();
  const int cols = rows ? (int)grid[0].size() : 0;

  const double SQRT2 = std::sqrt(2.0);

  while (!open_set.empty()) {
    auto [f_curr, current] = open_set.top();
    open_set.pop();

    auto itg = g_score.find(current);
    if (itg != g_score.end()) {
      double expected_f = itg->second + heuristic(current, goal);
      if (f_curr > expected_f + 1e-9) continue; // stale entry
    }

    if (current == goal) {
      // Reconstruct path in a deque
      std::deque<Point> path;
      Point p = goal;
      path.push_front(p);
      while (!(p == start)) {
        auto it = came_from.find(p);
        if (it == came_from.end()) { path.clear(); break; } // safety
        p = it->second;
        path.push_front(p);
      }
      return path;
    }

    for (const auto& d : directions) {
      Point nb{ current.x + d.x, current.y + d.y };

      if (nb.x < 0 || nb.y < 0 || nb.x >= cols || nb.y >= rows) continue;
      if (grid[nb.y][nb.x] != 0) continue; // blocked

      double step_cost = (d.x == 0 || d.y == 0) ? 1.0 : SQRT2;
      double tentative_g = g_score[current] + step_cost;

      if (!g_score.count(nb) || tentative_g < g_score[nb]) {
        came_from[nb] = current;
        g_score[nb] = tentative_g;
        double f_score = tentative_g + heuristic(nb, goal);
        open_set.push({f_score, nb});
      }
    }
  }

  return {}; // no path
}