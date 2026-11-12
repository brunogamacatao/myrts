#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "core/geometry.hpp"
#include "core/selection.hpp"

using Catch::Matchers::WithinAbs;
using namespace myrts;

TEST_CASE("Rect from corners normalizes any drag direction", "[geometry]") {
  const Rect a = Rect::from_corners(10, 20, 4, 8);
  REQUIRE(a.x == 4);
  REQUIRE(a.y == 8);
  REQUIRE(a.w == 6);
  REQUIRE(a.h == 12);
  REQUIRE(a.contains(4, 8));
  REQUIRE_FALSE(a.contains(10, 20));  // right/bottom edges are exclusive
}

TEST_CASE("Rect intersection", "[geometry]") {
  const Rect r{0, 0, 10, 10};
  REQUIRE(r.intersects({5, 5, 10, 10}));
  REQUIRE_FALSE(r.intersects({10, 0, 5, 5}));  // touching edges don't overlap
  REQUIRE(r.intersects({-5, -5, 6, 6}));
}

TEST_CASE("Camera clamps to the world", "[camera]") {
  Camera cam{0, 0, 640, 576, 800, 640};
  cam.move(1000, 1000);
  REQUIRE(cam.x == 160);
  REQUIRE(cam.y == 64);
  cam.move(-5000, -5000);
  REQUIRE(cam.x == 0);
  REQUIRE(cam.y == 0);

  Camera small{50, 50, 640, 576, 300, 300};  // world smaller than view
  small.clamp();
  REQUIRE(small.x == 0);
  REQUIRE(small.y == 0);
}

TEST_CASE("Camera conversions round trip", "[camera]") {
  Camera cam{100, 40, 640, 576, 800, 640};
  const Vec2 w = cam.to_world(30, 20);
  REQUIRE(w.x == 130);
  REQUIRE(w.y == 60);
  const Vec2 v = cam.to_view(w.x, w.y);
  REQUIRE(v.x == 30);
  REQUIRE(v.y == 20);
}

TEST_CASE("Camera centers on a point and respects limits", "[camera]") {
  Camera cam{0, 0, 640, 576, 800, 640};
  cam.center_on(400, 320);
  REQUIRE(cam.x == 80);
  REQUIRE(cam.y == 32);
  cam.center_on(0, 0);
  REQUIRE(cam.x == 0);
  REQUIRE(cam.y == 0);
}

TEST_CASE("Minimap mapping", "[minimap]") {
  const Minimap mm{800, 640, 128, 128};
  REQUIRE_THAT(mm.scale(), WithinAbs(0.16, 1e-6));
  REQUIRE_THAT(mm.width(), WithinAbs(128, 1e-4));
  REQUIRE_THAT(mm.height(), WithinAbs(102.4, 1e-4));
  REQUIRE_THAT(mm.offset_y(), WithinAbs(12.8, 1e-4));

  const Vec2 m = mm.world_to_mini(400, 320);
  REQUIRE_THAT(m.x, WithinAbs(64, 1e-4));
  REQUIRE_THAT(m.y, WithinAbs(64, 1e-4));
  const Vec2 w = mm.mini_to_world(m.x, m.y);
  REQUIRE_THAT(w.x, WithinAbs(400, 1e-2));
  REQUIRE_THAT(w.y, WithinAbs(320, 1e-2));

  const Vec2 edge = mm.mini_to_world(-50, 500);  // outside: clamped
  REQUIRE(edge.x == 0);
  REQUIRE(edge.y == 640);
}

TEST_CASE("Selection by point and by rectangle", "[selection]") {
  const std::vector<SelectBox> items{{1, {0, 0, 32, 32}}, {2, {16, 16, 32, 32}}, {3, {100, 100, 32, 32}}};

  REQUIRE(pick_at(items, 20, 20) == 2);  // overlapping: topmost (last) wins
  REQUIRE(pick_at(items, 5, 5) == 1);
  REQUIRE(pick_at(items, 60, 60) == std::nullopt);

  const auto hits = pick_in(items, Rect::from_corners(40, 40, -10, -10));  // dragged up-left
  REQUIRE(hits == std::vector<int>{1, 2});
  REQUIRE(pick_in(items, {200, 200, 10, 10}).empty());
}
