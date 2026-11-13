#include <catch2/catch_test_macros.hpp>
#include <array>

#include "core/rules.hpp"
#include "core/world.hpp"

using namespace myrts;

namespace {

constexpr const char* kUnits = R"({"units":[
  {"id":"peon","name":"Peon","hp":30,"move_speed":96,"sprite":"s","commands":["move"],
   "worker":{"lumber_per_trip":10,"gold_per_trip":100,"chop_seconds":1.0,"mine_seconds":1.0},
   "builds":["hall","farm"]},
  {"id":"grunt","name":"Grunt","hp":60,"move_speed":96,"sprite":"s"},
  {"id":"dragon","name":"Dragon","hp":120,"move_speed":96,"move_class":"air","sprite":"s"}]})";
constexpr const char* kBuildings = R"({"buildings":[
  {"id":"hall","name":"Hall","size":4,"cost":{"gold":1200,"lumber":800},"build_seconds":10,
   "is_depot":true,"sprite":"h"},
  {"id":"farm","name":"Farm","size":2,"cost":{"gold":500,"lumber":250},"build_seconds":4,"sprite":"f"}]})";

const Registry& registry() {
  static const Registry r = [] {
    auto parsed = Registry::parse(kUnits, kBuildings, R"({"commands":[{"id":"move"}]})");
    if (!parsed) FAIL("test data is invalid: " << parsed.error());
    return std::move(*parsed);
  }();
  return r;
}

struct Fixture {
  World world{WorldConfig{}, Grid(24, 14)};
  PlayerId me = world.add_player("me", "orcs", Stock{2000, 1000, 0});
  const UnitDef& peon = *registry().unit("peon");
  const UnitDef& grunt = *registry().unit("grunt");
  const UnitDef& dragon = *registry().unit("dragon");
  const BuildingDef& hall = *registry().building("hall");
  const BuildingDef& farm = *registry().building("farm");

  std::vector<Event> seen;
  Fixture() { world.events().subscribe([this](const Event& e) { seen.push_back(e); }); }

  EntityId spawn(const UnitDef& d, Point p) {
    const EntityId id = world.spawn_unit(d, me, p);
    REQUIRE(id.valid());
    return id;
  }
  void run(double seconds, double dt = 0.05) {
    for (double t = 0; t < seconds; t += dt) world.update(dt);
  }
  template <class E>
  int count() const {
    int n = 0;
    for (const auto& e : seen) n += std::holds_alternative<E>(e) ? 1 : 0;
    return n;
  }
  const Unit& unit(EntityId id) const { return *world.units().get(id); }
};

}  // namespace

TEST_CASE("A unit walks to its target and stops there") {
  Fixture f;
  const auto u = f.spawn(f.peon, {1, 1});
  const std::array ids{u};
  REQUIRE(f.world.order_move(ids, {10, 8}).ok);
  f.run(15);
  CHECK(f.unit(u).tile == Point{10, 8});
  CHECK(f.unit(u).is_idle());
}

TEST_CASE("Lumber is chopped, delivered to the owner and the tree disappears when empty") {
  Fixture f;
  f.world.add_building(f.hall, f.me, {1, 1}, true);
  f.world.add_tree({12, 6}, 20);  // two trips
  REQUIRE(f.world.grid(MoveClass::Land).is_blocked({12, 6}));
  const auto u = f.spawn(f.peon, {6, 6});
  const std::array ids{u};

  REQUIRE(f.world.order_harvest(ids, {12, 6}).ok);
  f.run(60);

  CHECK(f.world.player(f.me).stock.lumber == 1000 + 20);
  CHECK_FALSE(f.world.trees().has_tree({12, 6}));
  CHECK(f.world.grid(MoveClass::Land).is_walkable({12, 6}));  // felled: passable again
  CHECK(f.count<ev::TreeFelled>() == 1);
  CHECK(f.count<ev::Delivered>() == 2);
}

TEST_CASE("Gold is mined from a mine that runs dry and unblocks its tiles") {
  Fixture f;
  f.world.add_building(f.hall, f.me, {1, 1}, true);
  const int mine = f.world.add_mine({15, 6}, 3, 150);  // 100 + 50
  const auto u = f.spawn(f.peon, {8, 6});
  const std::array ids{u};

  REQUIRE(f.world.order_harvest(ids, {16, 7}).ok);  // any tile of the mine
  f.run(80);

  CHECK(f.world.player(f.me).stock.gold == 2000 + 150);
  CHECK(f.world.mines()[static_cast<std::size_t>(mine)].site.depleted());
  CHECK(f.world.mine_at({16, 7}) == -1);
  CHECK(f.world.grid(MoveClass::Land).is_walkable({16, 7}));
  CHECK(f.count<ev::MineDepleted>() == 1);
  CHECK_FALSE(f.unit(u).hidden);
}

TEST_CASE("Harvesting needs a depot and a worker") {
  Fixture f;
  f.world.add_tree({12, 6}, 100);
  const auto peon = f.spawn(f.peon, {6, 6});
  const auto grunt = f.spawn(f.grunt, {6, 8});
  const std::array p{peon};
  const std::array g{grunt};

  const auto no_depot = f.world.order_harvest(p, {12, 6});
  CHECK_FALSE(no_depot.ok);
  CHECK(no_depot.message.find("Great Hall") != std::string::npos);

  f.world.add_building(f.hall, f.me, {1, 1}, true);
  CHECK_FALSE(f.world.order_harvest(g, {12, 6}).ok);  // grunts cannot gather
  CHECK_FALSE(f.world.order_harvest(p, {3, 12}).ok);  // nothing to harvest there
}

TEST_CASE("Building pays up front, locks the builder until done, then becomes a depot") {
  Fixture f;
  const auto u = f.spawn(f.peon, {3, 3});
  const std::array ids{u};

  REQUIRE(f.world.order_build(f.me, ids, f.farm, {10, 6}).ok);
  CHECK(f.world.player(f.me).stock == Stock{1500, 750, 0});  // charged at once
  const EntityId site = f.world.building_at({10, 6});
  REQUIRE(site.valid());
  CHECK_FALSE(f.world.buildings().get(site)->complete);
  CHECK(f.world.grid(MoveClass::Land).is_blocked({10, 6}));

  f.run(1.0);  // on its way
  const auto refused = f.world.order_move(ids, {20, 12});
  CHECK_FALSE(refused.ok);
  CHECK(refused.message.find("Builders") != std::string::npos);
  CHECK_FALSE(f.world.order_stop(ids).message.size());  // stop is silently ignored too
  CHECK(f.unit(u).locked());

  f.run(20);
  CHECK(f.world.buildings().get(site)->complete);
  CHECK(f.count<ev::BuildingCompleted>() == 1);
  CHECK_FALSE(f.unit(u).locked());
  CHECK(f.world.order_move(ids, {20, 12}).ok);  // free again

  // A finished Great Hall becomes a place to deliver loads.
  CHECK(f.world.depot_tiles(f.me).empty());
  f.world.add_building(f.hall, f.me, {1, 8}, true);
  CHECK(f.world.depot_tiles(f.me).size() == 16);
}

TEST_CASE("Building is refused (and costs nothing) when it cannot be placed or paid") {
  Fixture f;
  const auto u = f.spawn(f.peon, {3, 3});
  const auto other = f.spawn(f.grunt, {10, 6});
  const std::array ids{u};
  const auto before = f.world.player(f.me).stock;

  CHECK_FALSE(f.world.order_build(f.me, ids, f.hall, {22, 12}).ok);  // sticks out of the map
  const auto in_way = f.world.order_build(f.me, ids, f.farm, {10, 6});
  CHECK(in_way.message == "A unit is in the way");
  (void)other;

  f.world.player(f.me).stock = {100, 0, 0};
  CHECK_FALSE(f.world.order_build(f.me, ids, f.farm, {15, 6}).ok);  // too poor
  CHECK(f.world.player(f.me).stock == Stock{100, 0, 0});
  CHECK_FALSE(f.world.building_at({15, 6}).valid());
  (void)before;

  const std::array grunt_only{other};
  f.world.player(f.me).stock = {5000, 5000, 0};
  CHECK_FALSE(f.world.order_build(f.me, grunt_only, f.farm, {15, 6}).ok);  // grunts cannot build
}

TEST_CASE("An abandoned site can be resumed by another peon") {
  Fixture f;
  const auto first = f.spawn(f.peon, {3, 3});
  const std::array a{first};
  REQUIRE(f.world.order_build(f.me, a, f.farm, {10, 6}).ok);
  const EntityId site = f.world.building_at({10, 6});

  f.world.kill_unit(first);  // the builder dies
  CHECK_FALSE(f.world.buildings().get(site)->builder.valid());
  CHECK(f.count<ev::UnitDied>() == 0);  // events are delivered by the next update...
  f.run(0.05);
  CHECK(f.count<ev::UnitDied>() == 1);  // ...never in the middle of a state change

  const auto second = f.spawn(f.peon, {3, 3});
  const std::array b{second};
  REQUIRE(f.world.order_resume_build(b, site).ok);
  f.run(20);
  CHECK(f.world.buildings().get(site)->complete);
}

TEST_CASE("Air units cross obstacles that stop land units") {
  Grid land(24, 14);
  for (int y = 0; y < 14; ++y) land.set_blocked({10, y});  // a wall across the whole map
  World world{WorldConfig{}, land};
  const PlayerId me = world.add_player("me", "orcs", {});
  const UnitDef& dragon = *registry().unit("dragon");
  const UnitDef& grunt = *registry().unit("grunt");

  const EntityId d = world.spawn_unit(dragon, me, {2, 5});
  const EntityId g = world.spawn_unit(grunt, me, {2, 7});
  REQUIRE((d.valid() && g.valid()));
  const std::array dd{d};
  const std::array gg{g};
  world.order_move(dd, {18, 5});
  world.order_move(gg, {18, 7});
  for (int i = 0; i < 400; ++i) world.update(0.05);

  CHECK(world.units().get(d)->tile == Point{18, 5});  // flew over the wall
  CHECK(world.units().get(g)->tile.x < 10);           // the grunt is still on its side
}

TEST_CASE("Air and land units do not collide with each other") {
  Fixture f;
  const auto land = f.spawn(f.grunt, {5, 5});
  const EntityId air = f.world.spawn_unit(f.dragon, f.me, {5, 5});  // same tile, other layer
  CHECK(air.valid());
  (void)land;
  CHECK_FALSE(f.world.spawn_unit(f.grunt, f.me, {5, 5}).valid());   // two land units cannot
}

TEST_CASE("Killing a unit releases everything it held") {
  Fixture f;
  f.world.add_building(f.hall, f.me, {1, 1}, true);
  const int mine = f.world.add_mine({15, 6}, 3, 1000);
  const auto u = f.spawn(f.peon, {13, 6});
  const std::array ids{u};
  f.world.order_harvest(ids, {15, 6});
  f.run(0.6);  // arrived (1 tile) and went in; the load takes 1 s
  REQUIRE(f.world.mines()[static_cast<std::size_t>(mine)].busy);  // it is inside

  f.world.kill_unit(u);
  CHECK_FALSE(f.world.mines()[static_cast<std::size_t>(mine)].busy);
  CHECK_FALSE(f.world.units().alive(u));
  CHECK(f.world.spawn_unit(f.peon, f.me, {13, 6}).valid());  // its tile is free again
}

TEST_CASE("Victory and defeat follow who has anything left") {
  Fixture f;
  CHECK(evaluate_outcome(f.world, f.me) == Outcome::Defeat);

  const auto mine = f.spawn(f.grunt, {2, 2});
  CHECK_FALSE(evaluate_outcome(f.world, f.me).has_value());  // alone: sandbox, no outcome

  const PlayerId enemy = f.world.add_player("enemy", "undead", {});
  const EntityId foe = f.world.spawn_unit(f.grunt, enemy, {8, 8});
  CHECK_FALSE(evaluate_outcome(f.world, f.me).has_value());  // fight is on

  f.world.kill_unit(foe);
  CHECK(evaluate_outcome(f.world, f.me) == Outcome::Victory);
  f.world.kill_unit(mine);
  CHECK(evaluate_outcome(f.world, f.me) == Outcome::Defeat);
}

TEST_CASE("Clicking in the middle of a forest, or between trees, still starts chopping") {
  Fixture f;
  f.world.add_building(f.hall, f.me, {1, 1}, true);
  for (int y = 4; y <= 10; ++y)
    for (int x = 12; x <= 18; ++x) f.world.add_tree({x, y}, 10);  // a solid 7x7 wood
  const auto u = f.spawn(f.peon, {7, 7});
  const std::array ids{u};

  REQUIRE(f.world.order_harvest(ids, {15, 7}).ok);  // deep inside: no free side of its own
  f.run(60);
  CHECK(f.world.player(f.me).stock.lumber > 1000);

  const auto v = f.spawn(f.peon, {7, 8});
  const std::array vids{v};
  f.world.add_tree({22, 2}, 10);
  CHECK(f.world.order_harvest(vids, {21, 2}).ok);       // one tile off a tree: snaps to it
  CHECK_FALSE(f.world.order_harvest(vids, {22, 12}).ok);  // nothing nearby: still refused
}

TEST_CASE("Moving to a walled-off spot goes as near as possible instead of staying put") {
  Fixture f;
  for (int y = 0; y < 14; ++y) f.world.add_tree({16, y}, 10);  // a wall of trees across the map
  const auto u = f.spawn(f.grunt, {4, 6});
  const std::array ids{u};
  REQUIRE(f.world.order_move(ids, {20, 6}).ok);
  f.run(10);
  CHECK(f.unit(u).tile.x == 15);
}
