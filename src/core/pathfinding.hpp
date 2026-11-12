#pragma once

#include <deque>

#include "grid.hpp"

namespace myrts {

/// A* over an 8-connected grid (octile heuristic). Diagonal moves are not
/// allowed to cut corners of blocked cells.
///
/// `extra_blocked` (optional, same dimensions as `grid`) marks additional
/// cells to avoid, e.g. tiles currently occupied by other units. The start
/// cell is never considered blocked by it.
///
/// Returns the path including both `start` and `goal`, or an empty path if
/// either endpoint is blocked/out of bounds or no route exists.
[[nodiscard]] std::deque<Point> a_star(const Grid& grid, Point start, Point goal,
                                       const Grid* extra_blocked = nullptr);

}  // namespace myrts
