#include <catch2/catch_test_macros.hpp>

#include "core/fog.hpp"
#include "core/world.hpp"

using namespace myrts;

TEST_CASE("a fog starts black, reveals discs and remembers what was seen") {
  FogOfWar fog(20, 20);
  CHECK(fog.at({5, 5}) == Seen::Unknown);
  fog.begin();
  fog.reveal({5, 5}, 3);
  fog.end();
  CHECK(fog.visible({5, 5}));
  CHECK(fog.visible({7, 5}));
  CHECK_FALSE(fog.explored({10, 5}));
  CHECK_FALSE(fog.explored({0, 0}));
  const auto seen_version = fog.version();

  fog.begin();  // nobody sees that spot any more
  fog.reveal({15, 15}, 2);
  fog.end();
  CHECK(fog.at({5, 5}) == Seen::Explored);  // still known
  CHECK(fog.visible({15, 15}));
  CHECK(fog.version() > seen_version);  // something new was discovered

  const auto v = fog.version();
  fog.begin();
  fog.reveal({15, 15}, 2);
  fog.end();
  CHECK(fog.version() == v);  // nothing new: no redraw needed
}

TEST_CASE("reveal clips at the map edge and any_visible looks at a whole footprint") {
  FogOfWar fog(10, 10);
  fog.reveal({0, 0}, 4);
  CHECK(fog.visible({3, 0}));
  CHECK(fog.any_visible({3, 3}, 4));  // a 4x4 square that touches the disc
  CHECK_FALSE(fog.any_visible({7, 7}, 3));
  CHECK_FALSE(fog.visible({-1, 0}));
}

TEST_CASE("the explored area survives encoding and rejects a map of another size") {
  FogOfWar a(16, 16);
  a.begin();
  a.reveal({4, 4}, 3);
  a.reveal({12, 3}, 2);
  a.end();
  const std::string text = a.encode();
  FogOfWar b(16, 16);
  REQUIRE(b.decode(text));
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 16; ++x) CHECK(b.explored({x, y}) == a.explored({x, y}));
  CHECK(b.at({4, 4}) == Seen::Explored);  // saved sight is only "explored"
  FogOfWar small(8, 8);
  CHECK_FALSE(small.decode(text));
  CHECK_FALSE(b.decode("12X"));
}

TEST_CASE("a world reveals around each side's units and can play without fog") {
  WorldConfig cfg;
  World w(cfg, Grid(40, 40));
  const PlayerId a = w.add_player("a", "x", {});
  const PlayerId b = w.add_player("b", "y", {});
  static const char* units = R"({"units":[{"id":"u","name":"U","hp":10,"move_speed":96,"sprite":"s","stats":{"sight":5}}]})";
  const auto reg = Registry::parse(units, R"({"buildings":[]})", R"({"commands":[]})");
  REQUIRE(reg.has_value());
  REQUIRE(w.spawn_unit(*reg->unit("u"), a, {5, 5}).valid());
  REQUIRE(w.spawn_unit(*reg->unit("u"), b, {30, 30}).valid());
  w.refresh_fog();
  CHECK(w.fog(a).visible({8, 5}));
  CHECK_FALSE(w.fog(a).explored({30, 30}));  // the other side is out of sight
  CHECK(w.fog(b).visible({30, 30}));
  CHECK_FALSE(w.fog(b).visible({5, 5}));

  WorldConfig open;
  open.fog = false;
  World clear(open, Grid(20, 20));
  const PlayerId p = clear.add_player("p", "x", {});
  CHECK(clear.fog(p).visible({19, 19}));
}
