#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "core/pathfinding.hpp"

using myrts::Grid;
using myrts::Point;
using myrts::a_star;

namespace {
double path_cost(const std::deque<Point>& path) {
  double cost = 0;
  for (std::size_t i = 1; i < path.size(); ++i) {
    const bool diag = path[i].x != path[i - 1].x && path[i].y != path[i - 1].y;
    cost += diag ? std::sqrt(2.0) : 1.0;
  }
  return cost;
}
}  // namespace

TEST_CASE("Point comparison", "[point]") {
  REQUIRE(Point{1, 2} == Point{1, 2});
  REQUIRE(Point{1, 2} != Point{2, 1});
  REQUIRE(Point{1, 2} < Point{1, 3});
  REQUIRE(Point{0, 9} < Point{1, 0});
}

TEST_CASE("Grid bounds and blocking", "[grid]") {
  Grid grid(3, 2);
  REQUIRE(grid.is_walkable({2, 1}));
  REQUIRE(grid.is_blocked({3, 0}));
  REQUIRE(grid.is_blocked({-1, 0}));
  grid.set_blocked({1, 1});
  REQUIRE(grid.is_blocked({1, 1}));
  grid.set_blocked({1, 1}, false);
  REQUIRE(grid.is_walkable({1, 1}));
}

TEST_CASE("A* on open grid", "[astar]") {
  Grid grid(10, 10);

  SECTION("start equals goal") {
    auto path = a_star(grid, {3, 3}, {3, 3});
    REQUIRE(path.size() == 1);
  }
  SECTION("straight line") {
    auto path = a_star(grid, {0, 0}, {5, 0});
    REQUIRE(path.size() == 6);
    REQUIRE(path.front() == Point{0, 0});
    REQUIRE(path.back() == Point{5, 0});
  }
  SECTION("diagonal is preferred") {
    auto path = a_star(grid, {0, 0}, {4, 4});
    REQUIRE(path.size() == 5);
  }
}

TEST_CASE("A* respects obstacles", "[astar]") {
  Grid grid(5, 5);
  for (int y = 0; y < 4; ++y) grid.set_blocked({2, y});  // wall with a gap at y=4

  auto path = a_star(grid, {0, 0}, {4, 0});
  REQUIRE_FALSE(path.empty());
  for (auto p : path) REQUIRE(grid.is_walkable(p));
  REQUIRE(path.back() == Point{4, 0});

  grid.set_blocked({2, 4});
  REQUIRE(a_star(grid, {0, 0}, {4, 0}).empty());
}

TEST_CASE("A* does not cut corners", "[astar]") {
  Grid grid(2, 2);
  grid.set_blocked({1, 0});
  grid.set_blocked({0, 1});
  REQUIRE(a_star(grid, {0, 0}, {1, 1}).empty());
}

TEST_CASE("A* invalid endpoints", "[astar]") {
  Grid grid(4, 4);
  grid.set_blocked({3, 3});
  REQUIRE(a_star(grid, {0, 0}, {3, 3}).empty());   // blocked goal
  REQUIRE(a_star(grid, {0, 0}, {9, 9}).empty());   // out of bounds goal
  REQUIRE(a_star(grid, {-1, 0}, {1, 1}).empty());  // out of bounds start
  REQUIRE(a_star(Grid{}, {0, 0}, {0, 0}).empty()); // empty grid
}

TEST_CASE("A* finds an optimal path", "[astar]") {
  Grid grid(6, 6);
  grid.set_blocked({1, 1});
  grid.set_blocked({2, 1});
  grid.set_blocked({3, 1});
  auto path = a_star(grid, {0, 0}, {5, 5});
  REQUIRE_FALSE(path.empty());
  // Optimal: 5 diagonal steps is blocked by the wall; allow only slight detour.
  REQUIRE(path_cost(path) <= 5 * std::sqrt(2.0) + 2.0);
  // Steps are always between adjacent cells.
  for (std::size_t i = 1; i < path.size(); ++i) {
    REQUIRE(std::abs(path[i].x - path[i - 1].x) <= 1);
    REQUIRE(std::abs(path[i].y - path[i - 1].y) <= 1);
  }
}
