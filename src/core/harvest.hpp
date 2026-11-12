#pragma once

#include <deque>
#include <map>
#include <optional>
#include <span>
#include <vector>

#include "grid.hpp"

namespace myrts {

/// Lumber left in each tree tile of the map.
class TreeField {
 public:
  void add(Point p, int lumber);
  [[nodiscard]] bool has_tree(Point p) const { return trees_.contains(p); }
  [[nodiscard]] int remaining(Point p) const;
  /// Removes up to `amount` lumber from the tree at `p` and returns what was taken.
  /// The tree disappears once it is empty.
  int take(Point p, int amount);
  [[nodiscard]] std::vector<Point> positions() const;
  [[nodiscard]] bool empty() const { return trees_.empty(); }

 private:
  std::map<Point, int> trees_;
};

struct ApproachPath {
  Point target;            // the tree / building tile that will be worked on
  std::deque<Point> path;  // from `from` to a walkable tile next to `target`
};

/// Shortest path from `from` to a walkable tile adjacent (8-neighbourhood, without
/// cutting corners of blocked tiles) to any tile in `targets`. The targets themselves
/// may be blocked. Returns nullopt if none is reachable.
[[nodiscard]] std::optional<ApproachPath> path_to_adjacent(const Grid& grid, Point from,
                                                           std::span<const Point> targets,
                                                           const Grid* extra_blocked = nullptr);

/// True when `a` and `b` touch (Chebyshev distance 1) and the move between them is not
/// diagonal across a blocked corner.
[[nodiscard]] bool is_adjacent(Point a, Point b);

}  // namespace myrts
