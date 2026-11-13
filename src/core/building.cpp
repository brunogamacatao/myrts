#include "building.hpp"

#include <algorithm>

namespace myrts {

std::vector<Point> footprint(Point origin, int size) {
  std::vector<Point> out;
  out.reserve(static_cast<std::size_t>(size) * static_cast<std::size_t>(size));
  for (int dy = 0; dy < size; ++dy)
    for (int dx = 0; dx < size; ++dx) out.push_back({origin.x + dx, origin.y + dy});
  return out;
}

bool can_place(const Grid& grid, Point origin, int size) {
  const auto tiles = footprint(origin, size);
  return std::ranges::all_of(tiles, [&](Point p) { return grid.is_walkable(p); });
}

int ResourceSite::take(int amount) {
  if (amount <= 0) return 0;
  const int taken = std::min(amount, remaining_);
  remaining_ -= taken;
  return taken;
}

}  // namespace myrts
