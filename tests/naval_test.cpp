#include <catch2/catch_test_macros.hpp>
#include <array>

#include "core/ai.hpp"
#include "core/world.hpp"

using namespace myrts;

// A coast: land for x < 10, sea for x >= 10.
namespace {

constexpr const char* kUnits = R"({"units":[
  {"id":"tanker","name":"Tanker","hp":90,"move_speed":96,"move_class":"water","sprite":"s",
   "cost":{"gold":50},"train_seconds":5,"worker":{"oil_per_trip":100,"oil_seconds":1.0}},
  {"id":"peon","name":"Peon","hp":30,"move_speed":96,"sprite":"s",
   "worker":{"gold_per_trip":100,"mine_seconds":1.0},"builds":["shipyard"]}]})";
constexpr const char* kBuildings = R"({"buildings":[
  {"id":"hall","name":"Hall","size":3,"build_seconds":10,"is_depot":true,"food":10,"sprite":"h"},
  {"id":"shipyard","name":"Shipyard","size":3,"build_seconds":10,"placement":"coast","accepts_oil":true,"cost":{"gold":100},"trains":["tanker"],"sprite":"y"},
  {"id":"platform","name":"Platform","size":3,"build_seconds":10,"placement":"water","sprite":"p"}]})";

const Registry& registry() {
  static const Registry r = [] {
    auto parsed = Registry::parse(kUnits, kBuildings, R"({"commands":[]})");
    if (!parsed) FAIL("test data is invalid: " << parsed.error());
    return std::move(*parsed);
  }();
  return r;
}

World make_world(WorldConfig config = {}) {
  Grid land(24, 10), water(24, 10);
  for (int y = 0; y < 10; ++y)
    for (int x = 0; x < 24; ++x) (x < 10 ? water : land).set_blocked({x, y});
  return World(config, std::move(land), std::move(water));
}

}  // namespace

TEST_CASE("placement and oil flags are read from the data, and a bad placement is refused") {
  CHECK(registry().building("shipyard")->placement == Placement::Coast);
  CHECK(registry().building("shipyard")->accepts_oil);
  CHECK(registry().building("platform")->placement == Placement::Water);
  CHECK(registry().building("hall")->placement == Placement::Land);
  CHECK(registry().unit("tanker")->worker->oil_per_trip == 100);
  CHECK_FALSE(Registry::parse(kUnits, R"({"buildings":[{"id":"x","name":"X","size":1,"build_seconds":1,"placement":"sky","sprite":"s"}]})",
                              R"({"commands":[]})").has_value());
}

TEST_CASE("a coastal building needs the shore, a platform needs the sea, a hall needs land") {
  World w = make_world();
  const BuildingDef& shipyard = *registry().building("shipyard");
  const BuildingDef& platform = *registry().building("platform");
  const BuildingDef& hall = *registry().building("hall");

  CHECK(w.placement_status(shipyard, {7, 3}) == PlaceStatus::Ok);       // touches the sea at x = 10
  CHECK(w.placement_status(shipyard, {2, 3}) == PlaceStatus::Blocked);  // inland
  CHECK(w.placement_status(shipyard, {12, 3}) == PlaceStatus::Blocked); // on the water itself

  CHECK(w.placement_status(platform, {14, 3}) == PlaceStatus::Ok);
  CHECK(w.placement_status(platform, {2, 3}) == PlaceStatus::Blocked);
  CHECK(w.placement_status(platform, {9, 3}) == PlaceStatus::Blocked);  // straddles the shore

  CHECK(w.placement_status(hall, {2, 3}) == PlaceStatus::Ok);
  CHECK(w.placement_status(hall, {12, 3}) == PlaceStatus::Blocked);
}

TEST_CASE("a platform blocks the water, not the land") {
  World w = make_world();
  const PlayerId me = w.add_player("me", "orcs", Stock{});
  const BuildingDef& platform = *registry().building("platform");
  w.add_building(platform, me, {14, 3}, true);
  CHECK(w.grid(MoveClass::Water).is_blocked({15, 4}));
  CHECK(w.placement_status(platform, {14, 3}) == PlaceStatus::Blocked);
  CHECK(w.grid(MoveClass::Land).is_blocked({15, 4}));  // (the land layer never had sea: it stays closed)
}

TEST_CASE("tankers pump oil from a patch at sea and deliver it to a shipyard") {
  World w = make_world();
  const PlayerId me = w.add_player("me", "orcs", Stock{});
  w.add_building(*registry().building("shipyard"), me, {7, 3}, true);
  const int patch = w.add_mine({15, 3}, 3, 250, ResourceKind::Oil);
  CHECK(w.grid(MoveClass::Water).is_blocked({16, 4}));
  CHECK_FALSE(w.grid(MoveClass::Land).is_walkable({16, 4}));  // it was never land

  const EntityId tanker = w.spawn_unit(*registry().unit("tanker"), me, {12, 4});
  REQUIRE(tanker.valid());
  const std::array ids{tanker};
  REQUIRE(w.order_harvest(ids, {15, 3}).ok);
  for (double t = 0; t < 40 && w.player(me).stock.oil < 100; t += 0.05) w.update(0.05);
  CHECK(w.player(me).stock.oil == 100);
  CHECK(w.player(me).stock.gold == 0);  // oil is not gold
  CHECK_FALSE(w.units().get(tanker)->hidden);  // tankers stay in sight

  for (double t = 0; t < 80 && !w.mines()[static_cast<std::size_t>(patch)].site.depleted(); t += 0.05) w.update(0.05);
  CHECK(w.mines()[static_cast<std::size_t>(patch)].site.depleted());
  CHECK(w.player(me).stock.oil >= 200);
  CHECK(w.grid(MoveClass::Water).is_walkable({16, 4}));  // the sea opens up again
}

TEST_CASE("oil needs a shipyard, and land workers cannot reach an oil patch") {
  World w = make_world();
  const PlayerId me = w.add_player("me", "orcs", Stock{});
  w.add_building(*registry().building("hall"), me, {1, 1}, true);  // a hall takes gold and lumber, not oil
  w.add_mine({15, 3}, 3, 250, ResourceKind::Oil);
  const EntityId tanker = w.spawn_unit(*registry().unit("tanker"), me, {12, 4});
  const std::array t{tanker};
  const Result no_yard = w.order_harvest(t, {15, 3});
  CHECK_FALSE(no_yard.ok);
  CHECK(no_yard.message.contains("Shipyard"));

  w.add_building(*registry().building("shipyard"), me, {7, 3}, true);
  const EntityId peon = w.spawn_unit(*registry().unit("peon"), me, {4, 6});
  const std::array p{peon};
  CHECK_FALSE(w.order_harvest(p, {15, 3}).ok);  // the patch is out of a peon's reach
}

TEST_CASE("the AI builds a shipyard when the map has oil, trains tankers and keeps them pumping") {
  WorldConfig config;
  config.build_speed = 10;
  config.train_speed = 10;
  World w = make_world(config);
  const PlayerId me = w.add_player("ai", "orcs", Stock{2000, 500, 0});
  w.add_building(*registry().building("hall"), me, {1, 1}, true);
  w.spawn_unit(*registry().unit("peon"), me, {5, 6});
  w.add_mine({15, 3}, 3, 5000, ResourceKind::Oil);

  AiConfig cfg;
  cfg.desired_workers = 1;
  cfg.desired_tankers = 2;
  Ai ai(w, registry(), me, cfg);
  for (double t = 0; t < 120; t += 0.1) {
    ai.update(0.1);
    w.update(0.1);
  }

  int shipyards = 0, tankers = 0;
  w.buildings().for_each([&](const Building& b) { shipyards += (b.owner == me && b.def->id == "shipyard" && b.complete) ? 1 : 0; });
  w.units().for_each([&](const Unit& u) { tankers += (u.owner == me && u.def->id == "tanker") ? 1 : 0; });
  CHECK(shipyards == 1);  // one, not one per think
  CHECK(tankers == 2);
  CHECK(w.player(me).stock.oil > 0);  // the tankers are working
}

TEST_CASE("on a map without oil the AI builds no shipyard") {
  WorldConfig config;
  config.build_speed = 10;
  World w = make_world(config);
  const PlayerId me = w.add_player("ai", "orcs", Stock{2000, 500, 0});
  w.add_building(*registry().building("hall"), me, {1, 1}, true);
  w.spawn_unit(*registry().unit("peon"), me, {5, 6});
  Ai ai(w, registry(), me);
  for (double t = 0; t < 60; t += 0.1) {
    ai.update(0.1);
    w.update(0.1);
  }
  w.buildings().for_each([&](const Building& b) { CHECK(b.def->id != "shipyard"); });
}
