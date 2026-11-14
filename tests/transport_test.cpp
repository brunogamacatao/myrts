#include <catch2/catch_test_macros.hpp>
#include <array>
#include <nlohmann/json.hpp>

#include "core/world.hpp"

using namespace myrts;

// A coast: land for x < 10, sea for x >= 10.
namespace {

constexpr const char* kUnits = R"({"units":[
  {"id":"ship","name":"Transport","hp":90,"move_speed":96,"move_class":"water","sprite":"s","capacity":2,"commands":["move"]},
  {"id":"soldier","name":"Soldier","hp":30,"move_speed":96,"sprite":"s","stats":{"damage_min":2,"damage_max":3,"range":1}},
  {"id":"dragon","name":"Dragon","hp":30,"move_speed":96,"move_class":"air","sprite":"s"}]})";

const Registry& registry() {
  static const Registry r = [] {
    auto parsed = Registry::parse(kUnits, R"({"buildings":[]})", R"({"commands":[{"id":"move"}]})");
    if (!parsed) FAIL("test data is invalid: " << parsed.error());
    return std::move(*parsed);
  }();
  return r;
}

struct Coast {
  World world{[] {
    Grid land(24, 12), water(24, 12);
    for (int y = 0; y < 12; ++y)
      for (int x = 0; x < 24; ++x) (x < 10 ? water : land).set_blocked({x, y});
    return World(WorldConfig{}, std::move(land), std::move(water));
  }()};
  PlayerId me = world.add_player("me", "x", Stock{});
  const UnitDef& ship = *registry().unit("ship");
  const UnitDef& soldier = *registry().unit("soldier");

  EntityId spawn(const UnitDef& d, Point p) {
    const EntityId id = world.spawn_unit(d, me, p);
    REQUIRE(id.valid());
    return id;
  }
  void run(double seconds, double dt = 0.05) {
    for (double t = 0; t < seconds; t += dt) world.update(dt);
  }
  const Unit& unit(EntityId id) const { return *world.units().get(id); }
};

}  // namespace

TEST_CASE("land units walk to a transport at the shore and go aboard, up to its capacity") {
  Coast c;
  const EntityId ship = c.spawn(c.ship, {10, 5});
  const EntityId a = c.spawn(c.soldier, {5, 5}), b = c.spawn(c.soldier, {5, 6}), d = c.spawn(c.soldier, {5, 4});
  const std::array all{a, b, d};
  REQUIRE(c.world.order_board(all, ship).ok);
  c.run(10);

  CHECK(c.unit(ship).passengers.size() == 2);  // the capacity
  int aboard = 0;
  for (const EntityId id : all) aboard += c.unit(id).aboard.valid() ? 1 : 0;
  CHECK(aboard == 2);
  for (const EntityId id : all)
    if (c.unit(id).aboard.valid()) {
      CHECK(c.unit(id).hidden);
      CHECK(c.unit(id).aboard == ship);
      CHECK_FALSE(c.world.unit_at(c.unit(id).tile) == id);  // not standing on the map any more
    }
  CHECK_FALSE(c.world.order_board(std::array{d}, ship).ok);  // full now
}

TEST_CASE("only own land units can board, and only a transport can be boarded") {
  Coast c;
  const EntityId ship = c.spawn(c.ship, {10, 5});
  const EntityId s = c.spawn(c.soldier, {5, 5});
  const EntityId dragon = c.spawn(*registry().unit("dragon"), {6, 6});
  CHECK_FALSE(c.world.order_board(std::array{dragon}, ship).ok);  // it flies
  CHECK_FALSE(c.world.order_board(std::array{s}, dragon).ok);     // that is no transport
}

TEST_CASE("a transport sails to a shore and lands its passengers there") {
  Coast c;
  const EntityId ship = c.spawn(c.ship, {12, 2});
  const EntityId a = c.spawn(c.soldier, {7, 8}), b = c.spawn(c.soldier, {7, 9});
  // They get aboard where the ship is waiting...
  const EntityId near = c.spawn(c.ship, {10, 8});
  REQUIRE(c.world.order_board(std::array{a, b}, near).ok);
  c.run(8);
  REQUIRE(c.unit(near).passengers.size() == 2);

  REQUIRE_FALSE(c.world.order_unload(std::array{near}, {12, 3}).ok);  // that is sea, not a shore
  REQUIRE(c.world.order_unload(std::array{near}, {7, 3}).ok);
  c.run(15);
  CHECK(c.unit(near).passengers.empty());
  for (const EntityId id : {a, b}) {
    const Unit& u = c.unit(id);
    CHECK_FALSE(u.aboard.valid());
    CHECK_FALSE(u.hidden);
    CHECK(c.world.grid(MoveClass::Land).is_walkable(u.tile));
    CHECK(u.tile.y <= 6);  // they came ashore near the target, not at the first ship's berth
  }
  (void)ship;
  CHECK(std::holds_alternative<order::Idle>(c.unit(near).order));
}

TEST_CASE("an empty transport has nobody to unload, and the cargo dies with the ship") {
  Coast c;
  const EntityId ship = c.spawn(c.ship, {10, 5});
  CHECK(c.world.order_unload(std::array{ship}, {8, 5}).ok);  // nothing happens, nothing breaks
  const EntityId a = c.spawn(c.soldier, {7, 5});
  REQUIRE(c.world.order_board(std::array{a}, ship).ok);
  c.run(6);
  REQUIRE(c.unit(ship).passengers.size() == 1);

  c.world.kill_unit(ship);
  CHECK(c.world.units().get(a) == nullptr);
  CHECK(c.world.units().get(ship) == nullptr);
}

TEST_CASE("a saved game keeps who is aboard which ship") {
  Coast c;
  const EntityId ship = c.spawn(c.ship, {10, 5});
  const EntityId a = c.spawn(c.soldier, {7, 5});
  REQUIRE(c.world.order_board(std::array{a}, ship).ok);
  c.run(6);
  REQUIRE(c.unit(ship).passengers.size() == 1);
  const nlohmann::json state = nlohmann::json::parse(c.world.save_state().dump());

  Coast d;
  const auto loaded = d.world.load_state(state, registry());
  REQUIRE(loaded.has_value());
  int ships = 0, aboard = 0;
  d.world.units().for_each([&](const Unit& u) {
    ships += u.def->capacity > 0 ? 1 : 0;
    aboard += u.aboard.valid() ? 1 : 0;
    if (u.def->capacity > 0) CHECK(u.passengers.size() == 1);
  });
  CHECK(ships == 1);
  CHECK(aboard == 1);
}
