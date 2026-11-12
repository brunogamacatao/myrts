#include <catch2/catch_test_macros.hpp>

#include "core/occupancy.hpp"

using myrts::Occupancy;
using myrts::Point;

TEST_CASE("Occupancy reservation", "[occupancy]") {
  Occupancy occ(4, 4);
  REQUIRE(occ.owner({1, 1}) == Occupancy::kNone);
  REQUIRE(occ.try_reserve({1, 1}, 7));
  REQUIRE(occ.try_reserve({1, 1}, 7));   // idempotent for the owner
  REQUIRE_FALSE(occ.try_reserve({1, 1}, 8));
  REQUIRE(occ.owner({1, 1}) == 7);

  occ.release({1, 1}, 8);  // not the owner: ignored
  REQUIRE(occ.owner({1, 1}) == 7);
  occ.release({1, 1}, 7);
  REQUIRE(occ.try_reserve({1, 1}, 8));
}

TEST_CASE("Occupancy bounds", "[occupancy]") {
  Occupancy occ(2, 2);
  REQUIRE_FALSE(occ.try_reserve({2, 0}, 1));
  REQUIRE_FALSE(occ.can_reserve({-1, 0}, 1));
  REQUIRE(occ.owner({5, 5}) == Occupancy::kNone);
}

TEST_CASE("Occupancy blocked grids", "[occupancy]") {
  Occupancy occ(3, 3);
  occ.try_reserve({0, 0}, 1);
  occ.try_reserve({1, 0}, 1);
  occ.try_reserve({2, 2}, 2);

  const auto for_one = occ.blocked_for(1);
  REQUIRE(for_one.is_walkable({0, 0}));
  REQUIRE(for_one.is_walkable({1, 0}));
  REQUIRE(for_one.is_blocked({2, 2}));

  const std::vector<int> both{1, 2};
  const auto none = occ.blocked_except(both);
  for (int y = 0; y < 3; ++y)
    for (int x = 0; x < 3; ++x) REQUIRE(none.is_walkable({x, y}));
}
