#include <catch2/catch_test_macros.hpp>

#include "core/building.hpp"

using namespace myrts;

TEST_CASE("Building specs match the design table") {
  const auto hall = building_spec(BuildingKind::GreatHall);
  CHECK(hall.cost == Cost{1200, 800, 0});
  CHECK(hall.build_seconds == 255.0);
  CHECK(hall.is_depot);

  const auto farm = building_spec(BuildingKind::PigFarm);
  CHECK(farm.cost == Cost{500, 250, 0});
  CHECK(farm.build_seconds == 100.0);
  CHECK_FALSE(farm.is_depot);

  const auto barracks = building_spec(BuildingKind::Barracks);
  CHECK(barracks.cost == Cost{700, 450, 0});
  CHECK(barracks.build_seconds == 200.0);
}

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

TEST_CASE("GoldMine is exhausted gradually") {
  GoldMine m({1, 1}, 3, 250);
  CHECK(m.tiles().size() == 9);
  CHECK(m.take(100) == 100);
  CHECK(m.take(100) == 100);
  CHECK_FALSE(m.depleted());
  CHECK(m.take(100) == 50);
  CHECK(m.depleted());
  CHECK(m.take(100) == 0);
}
