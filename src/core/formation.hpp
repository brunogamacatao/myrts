#pragma once

#include <optional>
#include <span>
#include <vector>

#include "grid.hpp"

namespace myrts {

/// Chooses a distinct destination tile for each unit of a group moving to `target`.
///
/// Picks the free, walkable, connected tiles closest to `target` (if `target`
/// itself is unusable, the nearest walkable tile is used as the centre) and
/// pairs them with units greedily by distance, which keeps the formation
/// compact and avoids needless crossing. `occupied` (optional) marks tiles
/// that must not be chosen. Units left without a slot get `std::nullopt`.
[[nodiscard]] std::vector<std::optional<Point>> assign_destinations(const Grid& grid, const Grid* occupied,
                                                                    std::span<const Point> units,
                                                                    Point target);

}  // namespace myrts
