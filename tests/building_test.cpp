#include <catch2/catch_test_macros.hpp>

#include "core/building.hpp"

using namespace myrts;

TEST_CASE("pay only succeeds when every resource is covered") {
  Stock s{1000, 300, 0};
  CHECK_FALSE(pay(s, Cost{500, 400, 0}));  // not enough lumber
  CHECK(s == Stock{1000, 300, 0});         // untouched
  CHECK(pay(s, Cost{500, 250, 0}));
  CHECK(s == Stock{500, 50, 0});
  CHECK_FALSE(can_afford(s, Cost{0, 0, 1}));
}

TEST_CASE("footprint lists size*size tiles") {
  const auto f = footprint({2, 3}, 3);
  CHECK(f.size() == 9);
  CHECK(f.front() == Point{2, 3});
  CHECK(f.back() == Point{4, 5});
}

TEST_CASE("can_place rejects blocked or out-of-bounds footprints") {
  Grid g(10, 10);
  CHECK(can_place(g, {0, 0}, 4));
  CHECK(can_place(g, {6, 6}, 4));
  CHECK_FALSE(can_place(g, {7, 6}, 4));  // sticks out of the map
  g.set_blocked({2, 2});
  CHECK_FALSE(can_place(g, {0, 0}, 4));
  CHECK(can_place(g, {3, 3}, 2));
}

TEST_CASE("ResourceSite is exhausted gradually") {
  ResourceSite m({1, 1}, 3, 250);
  CHECK(m.tiles().size() == 9);
  CHECK(m.take(100) == 100);
  CHECK(m.take(100) == 100);
  CHECK_FALSE(m.depleted());
  CHECK(m.take(100) == 50);
  CHECK(m.depleted());
  CHECK(m.take(100) == 0);
}
