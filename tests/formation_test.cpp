#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdlib>
#include <set>
#include <vector>

#include "core/formation.hpp"

using myrts::Grid;
using myrts::Point;
using myrts::assign_destinations;

namespace {
std::set<std::pair<int, int>> unique_dests(const std::vector<std::optional<Point>>& d) {
  std::set<std::pair<int, int>> s;
  for (const auto& p : d) {
    if (p) s.insert({p->x, p->y});
  }
  return s;
}
}  // namespace

TEST_CASE("Destinations are distinct and walkable", "[formation]") {
  Grid grid(10, 10);
  grid.set_blocked({5, 5});
  const std::vector<Point> units{{0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1}, {0, 2}, {1, 2}};

  const auto dests = assign_destinations(grid, nullptr, units, {5, 5});  // target is blocked
  REQUIRE(dests.size() == units.size());
  for (const auto& d : dests) {
    REQUIRE(d.has_value());
    REQUIRE(grid.is_walkable(*d));
  }
  REQUIRE(unique_dests(dests).size() == units.size());
}

TEST_CASE("Single unit goes straight to a walkable target", "[formation]") {
  Grid grid(5, 5);
  const auto dests = assign_destinations(grid, nullptr, std::vector<Point>{{0, 0}}, {3, 2});
  REQUIRE(dests.size() == 1);
  REQUIRE(*dests[0] == Point{3, 2});
}

TEST_CASE("Destinations stay close to the target", "[formation]") {
  Grid grid(20, 20);
  const std::vector<Point> units(9, Point{0, 0});
  const auto dests = assign_destinations(grid, nullptr, units, {10, 10});
  for (const auto& d : dests) {
    REQUIRE(std::abs(d->x - 10) <= 1);  // 9 units fit in the 3x3 block around the target
    REQUIRE(std::abs(d->y - 10) <= 1);
  }
}

TEST_CASE("Occupied tiles are skipped", "[formation]") {
  Grid grid(5, 5);
  Grid occupied(5, 5);
  occupied.set_blocked({2, 2});
  const auto dests = assign_destinations(grid, &occupied, std::vector<Point>{{0, 0}, {0, 1}}, {2, 2});
  REQUIRE(unique_dests(dests).size() == 2);
  for (const auto& d : dests) REQUIRE(*d != Point{2, 2});
}

TEST_CASE("Units without a slot get no destination", "[formation]") {
  Grid grid(2, 1);  // only two tiles
  const auto dests = assign_destinations(grid, nullptr, std::vector<Point>{{0, 0}, {1, 0}, {0, 0}}, {0, 0});
  REQUIRE(unique_dests(dests).size() == 2);
  REQUIRE(std::ranges::count_if(dests, [](const auto& d) { return !d.has_value(); }) == 1);
}

TEST_CASE("Unreachable tiles are not chosen", "[formation]") {
  Grid grid(5, 1);
  grid.set_blocked({2, 0});  // splits the row; right side unreachable from the target
  const auto dests = assign_destinations(grid, nullptr, std::vector<Point>{{0, 0}, {1, 0}, {0, 0}}, {0, 0});
  for (const auto& d : dests) {
    if (d) REQUIRE(d->x < 2);
  }
}

TEST_CASE("Degenerate inputs", "[formation]") {
  REQUIRE(assign_destinations(Grid{}, nullptr, std::vector<Point>{{0, 0}}, {0, 0})[0] == std::nullopt);
  REQUIRE(assign_destinations(Grid(3, 3), nullptr, {}, {1, 1}).empty());
}
