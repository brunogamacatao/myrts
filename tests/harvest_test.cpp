#include <catch2/catch_test_macros.hpp>
#include <array>

#include "core/harvest.hpp"

using namespace myrts;

TEST_CASE("TreeField takes lumber and removes empty trees") {
  TreeField f;
  f.add({2, 2}, 100);
  REQUIRE(f.has_tree({2, 2}));
  CHECK(f.take({2, 2}, 30) == 30);
  CHECK(f.remaining({2, 2}) == 70);
  CHECK(f.take({2, 2}, 100) == 70);
  CHECK_FALSE(f.has_tree({2, 2}));
  CHECK(f.take({2, 2}, 10) == 0);
  CHECK(f.empty());
}

TEST_CASE("path_to_adjacent reaches a tile next to a blocked tree") {
  Grid g(8, 8);
  g.set_blocked({5, 4});
  const std::array targets{Point{5, 4}};
  const auto r = path_to_adjacent(g, {0, 4}, targets);
  REQUIRE(r.has_value());
  CHECK(r->target == Point{5, 4});
  CHECK(is_adjacent(r->path.back(), {5, 4}));
  CHECK(g.is_walkable(r->path.back()));
  CHECK(r->path.front() == Point{0, 4});
}

TEST_CASE("path_to_adjacent returns the start when already adjacent") {
  Grid g(8, 8);
  g.set_blocked({3, 3});
  const std::array targets{Point{3, 3}};
  const auto r = path_to_adjacent(g, {2, 3}, targets);
  REQUIRE(r.has_value());
  CHECK(r->path.size() == 1);
}

TEST_CASE("path_to_adjacent picks the nearest of several trees") {
  Grid g(12, 3);
  g.set_blocked({2, 1});
  g.set_blocked({10, 1});
  const std::array targets{Point{10, 1}, Point{2, 1}};
  const auto r = path_to_adjacent(g, {0, 0}, targets);
  REQUIRE(r.has_value());
  CHECK(r->target == Point{2, 1});
}

TEST_CASE("path_to_adjacent fails when the tree is enclosed") {
  Grid g(5, 5);
  for (int y = 1; y <= 3; ++y)
    for (int x = 1; x <= 3; ++x) g.set_blocked({x, y});
  const std::array targets{Point{2, 2}};
  CHECK_FALSE(path_to_adjacent(g, {0, 0}, targets).has_value());
}

TEST_CASE("path_to_adjacent ignores tiles held by other units") {
  Grid g(5, 3);
  g.set_blocked({4, 1});
  Grid busy(5, 3);
  busy.set_blocked({3, 1});
  busy.set_blocked({3, 0});
  busy.set_blocked({3, 2});
  const std::array targets{Point{4, 1}};
  CHECK_FALSE(path_to_adjacent(g, {0, 1}, targets, &busy).has_value());
}
