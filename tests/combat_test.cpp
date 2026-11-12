#include <catch2/catch_test_macros.hpp>
#include <array>

#include "core/ai.hpp"
#include "core/rules.hpp"
#include "core/world.hpp"

using namespace myrts;

namespace {

// Fixed damage (min == max) keeps the fights deterministic.
constexpr const char* kUnits = R"({"units":[
  {"id":"peon","name":"Peon","hp":10,"move_speed":96,"sprite":"s","commands":["move"],
   "stats":{"damage_min":5,"damage_max":5,"range":1,"sight":4},"cost":{"gold":400},"train_seconds":10,
   "worker":{"lumber_per_trip":10,"gold_per_trip":100,"chop_seconds":1.0,"mine_seconds":1.0},"builds":["hut","farm"]},
  {"id":"grunt","name":"Grunt","hp":20,"move_speed":96,"sprite":"s","attack_seconds":1.0,
   "stats":{"armor":0,"damage_min":5,"damage_max":5,"range":1,"sight":5},"cost":{"gold":600},"train_seconds":20},
  {"id":"footman","name":"Footman","hp":20,"move_speed":96,"sprite":"s","attack_seconds":1.0,
   "stats":{"armor":2,"damage_min":5,"damage_max":5,"range":1,"sight":5},"cost":{"gold":600},"train_seconds":20},
  {"id":"archer","name":"Archer","hp":10,"move_speed":96,"sprite":"s","attack_seconds":1.0,
   "stats":{"armor":0,"damage_min":3,"damage_max":3,"range":4,"sight":6},"cost":{"gold":500}},
  {"id":"catapult","name":"Catapult","hp":40,"move_speed":48,"sprite":"s","attack_seconds":1.0,"siege_multiplier":3.0,
   "stats":{"armor":0,"damage_min":4,"damage_max":4,"range":6,"sight":6},"cost":{"gold":900},"train_seconds":20,"food":2},
  {"id":"dragon","name":"Dragon","hp":40,"move_speed":120,"move_class":"air","sprite":"s","attack_seconds":1.0,
   "stats":{"armor":0,"damage_min":6,"damage_max":6,"range":3,"sight":6},"cost":{"gold":1500},"train_seconds":20,"food":3},
  {"id":"brute","name":"Brute","hp":30,"move_speed":96,"sprite":"s","attack_seconds":1.0,
   "stats":{"armor":0,"damage_min":1,"damage_max":9,"range":1,"sight":5},"cost":{"gold":500}}]})";
constexpr const char* kBuildings = R"({"buildings":[
  {"id":"hall","name":"Hall","size":4,"hp":100,"food":50,"cost":{"gold":1200},"build_seconds":10,"is_depot":true,
   "sprite":"h","trains":["peon"]},
  {"id":"barracks","name":"Barracks","size":3,"hp":100,"armor":1,"food":50,"cost":{"gold":700},"build_seconds":10,
   "sprite":"b","trains":["grunt","footman"]},
  {"id":"bunker","name":"Bunker","size":3,"hp":100,"food":2,"cost":{"gold":300},"build_seconds":10,
   "sprite":"k","trains":["grunt"]},
  {"id":"farm","name":"Farm","size":2,"hp":40,"food":4,"cost":{"gold":100,"lumber":20},"build_seconds":2,"sprite":"f"},
  {"id":"smallhall","name":"Small Hall","size":4,"hp":100,"food":2,"cost":{"gold":1200},"build_seconds":10,
   "is_depot":true,"sprite":"h","trains":["peon"]},
  {"id":"hut","name":"Hut","size":2,"hp":12,"cost":{"gold":100},"build_seconds":4,"sprite":"x"}]})";

const Registry& registry() {
  static const Registry r = [] {
    auto parsed = Registry::parse(kUnits, kBuildings, R"({"commands":[{"id":"move"}]})");
    if (!parsed) FAIL("test data is invalid: " << parsed.error());
    return std::move(*parsed);
  }();
  return r;
}

struct Fixture {
  explicit Fixture(WorldConfig cfg = {}, int w = 30, int h = 14) : world(cfg, Grid(w, h)) {
    world.events().subscribe([this](const Event& e) { seen.push_back(e); });
  }
  World world;
  PlayerId me = world.add_player("me", "orcs", Stock{2000, 1000, 0});
  PlayerId foe = world.add_player("foe", "alliance", Stock{2000, 1000, 0});
  std::vector<Event> seen;

  const UnitDef& def(const char* id) { return *registry().unit(id); }
  const BuildingDef& bdef(const char* id) { return *registry().building(id); }
  EntityId spawn(const char* id, Point p, PlayerId owner) {
    const EntityId e = world.spawn_unit(def(id), owner, p);
    REQUIRE(e.valid());
    return e;
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
  const Unit* unit(EntityId id) const { return world.units().get(id); }
};

}  // namespace

TEST_CASE("Armor reduces damage, and a hit always does at least 1") {
  Fixture f;
  const auto grunt = f.spawn("grunt", {5, 5}, f.me);
  const auto footman = f.spawn("footman", {6, 5}, f.foe);
  (void)footman;
  f.world.order_attack(std::array{grunt}, {6, 5});
  f.run(0.2);
  CHECK(f.unit(footman)->hp == 20 - 3);  // 5 - armor 2

  Fixture g;
  const auto weak = g.spawn("archer", {5, 5}, g.me);  // 3 damage
  const auto tank = g.spawn("footman", {6, 5}, g.foe);
  g.world.order_attack(std::array{weak}, {6, 5});
  g.run(0.2);
  CHECK(g.unit(tank)->hp == 20 - 1);  // 3 - armor 2
}

TEST_CASE("Two fighters that meet fight to the death; the better armored one wins") {
  Fixture f;
  const auto grunt = f.spawn("grunt", {5, 5}, f.me);
  const auto footman = f.spawn("footman", {6, 5}, f.foe);
  f.run(20);  // both are aggressive and see each other

  CHECK_FALSE(f.world.units().alive(grunt));  // takes 4 hits to die, deals 3 per hit
  REQUIRE(f.world.units().alive(footman));
  CHECK(f.unit(footman)->hp > 0);
  CHECK(f.count<ev::UnitDied>() == 1);
  CHECK(f.count<ev::Attacked>() >= 4);
  CHECK(std::holds_alternative<order::Idle>(f.unit(footman)->order));  // back to idle afterwards
}

TEST_CASE("An ordered attack chases a distant target") {
  Fixture f;
  const auto grunt = f.spawn("grunt", {2, 5}, f.me);
  const auto peon = f.spawn("peon", {22, 5}, f.foe);  // far outside its sight
  REQUIRE(f.world.order_attack(std::array{grunt}, {22, 5}).ok);
  f.run(20);
  CHECK_FALSE(f.world.units().alive(peon));
  CHECK(f.world.units().alive(grunt));
  CHECK(std::holds_alternative<order::Idle>(f.unit(grunt)->order));
}

TEST_CASE("Idle fighters engage what they see; workers and far-away enemies are left alone") {
  Fixture f;
  const auto grunt = f.spawn("grunt", {2, 5}, f.me);
  const auto far_peon = f.spawn("peon", {15, 5}, f.foe);  // 13 tiles: out of sight
  f.run(3);
  CHECK(f.unit(far_peon)->hp == 10);
  CHECK(std::holds_alternative<order::Idle>(f.unit(grunt)->order));

  Fixture g;
  const auto peon = g.spawn("peon", {5, 5}, g.me);  // not aggressive
  const auto enemy = g.spawn("peon", {6, 5}, g.foe);
  g.run(3);
  CHECK(g.unit(enemy)->hp == 10);  // neither side picked a fight
  CHECK(g.unit(peon)->hp == 10);

  Fixture h;
  const auto brute = h.spawn("grunt", {5, 5}, h.me);
  const auto prey = h.spawn("peon", {9, 5}, h.foe);  // 4 tiles: in sight
  h.run(10);
  CHECK_FALSE(h.world.units().alive(prey));
  CHECK(h.world.units().alive(brute));
}

TEST_CASE("An armed unit that is hit while idle fights back, even a worker") {
  Fixture f;
  const auto archer = f.spawn("archer", {5, 5}, f.foe);
  const auto peon = f.spawn("peon", {9, 5}, f.me);  // 4 tiles away: inside the archer's range
  REQUIRE(f.world.order_attack(std::array{archer}, {9, 5}).ok);
  f.run(1.2);
  REQUIRE(f.world.units().alive(peon));
  CHECK(f.unit(peon)->hp < 10);  // it was hit...
  CHECK(std::holds_alternative<order::Attack>(f.unit(peon)->order));  // ...and turned on its attacker
  f.run(10);
  CHECK_FALSE(f.world.units().alive(archer));  // 10 hp, 5 damage per peon hit
}

TEST_CASE("Ranged units strike without closing in") {
  Fixture f;
  const auto archer = f.spawn("archer", {5, 5}, f.me);
  const auto target = f.spawn("peon", {9, 5}, f.foe);  // 4 tiles away = range
  f.world.order_attack(std::array{archer}, {9, 5});
  f.run(1.5);
  CHECK(f.unit(archer)->tile == Point{5, 5});  // never moved
  CHECK(f.unit(target)->hp < 10);
}

TEST_CASE("Attack-move fights what it meets and then carries on") {
  Fixture f;
  const auto grunt = f.spawn("grunt", {1, 5}, f.me);
  const auto bystander = f.spawn("peon", {14, 5}, f.foe);
  REQUIRE(f.world.order_attack_move(std::array{grunt}, {26, 5}).ok);
  f.run(30);
  CHECK_FALSE(f.world.units().alive(bystander));  // dealt with on the way
  CHECK(f.unit(grunt)->tile.x >= 24);              // and kept going
  CHECK(std::holds_alternative<order::Idle>(f.unit(grunt)->order));
}

TEST_CASE("Units cannot be ordered to attack when they have no weapon, and builders stay locked") {
  Fixture f;
  f.world.add_building(f.bdef("hall"), f.me, {1, 1}, true);
  const auto brute = f.spawn("peon", {10, 8}, f.me);
  CHECK(f.world.order_attack(std::array{brute}, {20, 8}).ok);  // peons can fight when told to

  const auto builder = f.spawn("peon", {8, 8}, f.me);
  REQUIRE(f.world.order_build(f.me, std::array{builder}, f.bdef("hut"), {14, 10}).ok);
  f.run(0.5);
  CHECK_FALSE(f.world.order_attack(std::array{builder}, {20, 8}).ok);  // locked while building
}

TEST_CASE("Buildings can be attacked and destroyed, freeing their ground") {
  Fixture f;
  const EntityId hut = f.world.add_building(f.bdef("hut"), f.foe, {12, 5}, true);
  const auto grunt = f.spawn("grunt", {5, 6}, f.me);
  REQUIRE(f.world.order_attack(std::array{grunt}, {12, 5}).ok);
  CHECK(f.world.grid(MoveClass::Land).is_blocked({12, 5}));
  f.run(15);

  CHECK_FALSE(f.world.buildings().alive(hut));  // 12 hp: three hits of 5
  CHECK(f.world.grid(MoveClass::Land).is_walkable({12, 5}));
  CHECK(f.world.grid(MoveClass::Land).is_walkable({13, 6}));
  CHECK(f.count<ev::BuildingDestroyed>() == 1);
  CHECK_FALSE(f.world.building_at({12, 5}).valid());
  CHECK(std::holds_alternative<order::Idle>(f.unit(grunt)->order));
}

TEST_CASE("A construction site can be destroyed; its builder is freed") {
  Fixture f;
  f.world.add_building(f.bdef("hall"), f.me, {1, 1}, true);
  const auto builder = f.spawn("peon", {8, 8}, f.me);
  REQUIRE(f.world.order_build(f.me, std::array{builder}, f.bdef("hut"), {14, 10}).ok);
  const EntityId site = f.world.building_at({14, 10});
  f.run(1.5);
  REQUIRE(f.unit(builder) != nullptr);
  REQUIRE(f.unit(builder)->locked());

  // An enemy arrives and is told to tear the site down (left alone, it would rather kill the builder first).
  const auto raider = f.spawn("grunt", {18, 10}, f.foe);
  REQUIRE(f.world.order_attack(std::array{raider}, {14, 10}).ok);
  for (int i = 0; i < 400 && f.world.buildings().alive(site); ++i) f.run(0.05);  // until the site falls
  CHECK_FALSE(f.world.buildings().alive(site));
  REQUIRE(f.unit(builder) != nullptr);  // the builder is still alive at that moment...
  CHECK_FALSE(f.unit(builder)->locked());  // ...and free again
  (void)raider;
}

TEST_CASE("Training: pay, wait, spawn next to the building, refund on cancel") {
  WorldConfig cfg;
  Fixture f(cfg);
  const EntityId barracks = f.world.add_building(f.bdef("barracks"), f.me, {5, 5}, true);
  const UnitDef& grunt = f.def("grunt");

  REQUIRE(f.world.order_train(f.me, barracks, grunt).ok);
  CHECK(f.world.player(f.me).stock.gold == 2000 - 600);

  f.run(19);
  CHECK(f.world.units().size() == 0);  // 20 s needed
  f.run(2);
  REQUIRE(f.world.units().size() == 1);
  Point spawned{};
  f.world.units().for_each([&](const Unit& u) { spawned = u.tile; });
  const auto cheb = std::max(std::abs(spawned.x - 6), std::abs(spawned.y - 6));
  CHECK(cheb == 2);  // on the ring right around the 3x3 footprint (centre 6,6)
  CHECK(f.world.grid(MoveClass::Land).is_walkable(spawned));

  REQUIRE(f.world.order_train(f.me, barracks, f.def("footman")).ok);
  CHECK(f.world.player(f.me).stock.gold == 2000 - 600 - 600);
  REQUIRE(f.world.order_cancel_train(f.me, barracks, 0).ok);
  CHECK(f.world.player(f.me).stock.gold == 2000 - 600);  // refunded
  CHECK_FALSE(f.world.order_cancel_train(f.me, barracks, 0).ok);
}

TEST_CASE("Training is refused when it should be") {
  Fixture f;
  const EntityId barracks = f.world.add_building(f.bdef("barracks"), f.me, {5, 5}, true);
  const EntityId unfinished = f.world.add_building(f.bdef("barracks"), f.me, {20, 5}, false);

  CHECK_FALSE(f.world.order_train(f.me, unfinished, f.def("grunt")).ok);    // not built yet
  CHECK_FALSE(f.world.order_train(f.foe, barracks, f.def("grunt")).ok);     // somebody else's
  CHECK_FALSE(f.world.order_train(f.me, barracks, f.def("peon")).ok);       // not trained here
  CHECK_FALSE(f.world.order_train(f.me, barracks, f.def("archer")).ok);

  f.world.player(f.me).stock.gold = 500;
  CHECK_FALSE(f.world.order_train(f.me, barracks, f.def("grunt")).ok);      // too poor
  CHECK(f.world.player(f.me).stock.gold == 500);

  f.world.player(f.me).stock.gold = 100000;
  for (int i = 0; i < 5; ++i) CHECK(f.world.order_train(f.me, barracks, f.def("grunt")).ok);
  const auto full = f.world.order_train(f.me, barracks, f.def("grunt"));
  CHECK_FALSE(full.ok);
  CHECK(full.message.find("full") != std::string::npos);
}

TEST_CASE("Idle fighters kill a builder they can see before touching its site") {
  Fixture f;
  f.world.add_building(f.bdef("hall"), f.me, {1, 1}, true);
  const auto builder = f.spawn("peon", {8, 8}, f.me);
  REQUIRE(f.world.order_build(f.me, std::array{builder}, f.bdef("hut"), {14, 10}).ok);
  const EntityId site = f.world.building_at({14, 10});
  f.run(1.5);
  f.spawn("grunt", {18, 10}, f.foe);
  f.run(15);
  CHECK_FALSE(f.world.units().alive(builder));                 // units come first...
  CHECK_FALSE(f.world.buildings().alive(site));                // ...then the building
  CHECK(f.count<ev::UnitDied>() == 1);
}

TEST_CASE("Siege units hit buildings much harder than units") {
  Fixture f;
  const EntityId hall = f.world.add_building(f.bdef("hall"), f.foe, {12, 4}, true);  // 100 hp
  const auto catapult = f.spawn("catapult", {6, 6}, f.me);
  const auto victim = f.spawn("grunt", {20, 10}, f.foe);
  (void)victim;
  f.world.order_attack(std::array{catapult}, {12, 4});
  f.run(0.5);  // one strike from range 6: 4 x 3.0 = 12 against a building
  CHECK(f.world.buildings().get(hall)->hp == 100 - 12);

  Fixture g;
  const auto siege = g.spawn("catapult", {6, 6}, g.me);
  const auto prey = g.spawn("footman", {10, 6}, g.foe);  // armor 2
  g.world.order_attack(std::array{siege}, {10, 6});
  g.run(0.5);
  CHECK(g.unit(prey)->hp == 20 - 2);  // 4 - armor 2: no multiplier against units
}

TEST_CASE("Flying units cross land obstacles and can be hit by ground fighters") {
  Grid land(24, 10);
  for (int y = 0; y < 10; ++y) land.set_blocked({10, y});  // a wall splitting the map
  World world{WorldConfig{}, land};
  const PlayerId me = world.add_player("me", "orcs", {});
  const PlayerId foe = world.add_player("foe", "alliance", {});
  const EntityId dragon = world.spawn_unit(*registry().unit("dragon"), me, {2, 5});
  const EntityId archer = world.spawn_unit(*registry().unit("archer"), foe, {16, 5});
  REQUIRE((dragon.valid() && archer.valid()));
  world.order_attack(std::array{dragon}, {16, 5});
  for (int i = 0; i < 300; ++i) world.update(0.05);  // 15 s
  CHECK_FALSE(world.units().alive(archer));  // it flew over the wall and killed it
  REQUIRE(world.units().alive(dragon));
  CHECK(world.units().get(dragon)->tile.x > 10);
}

TEST_CASE("The computer player trains a varied army, not just the first unit of a building") {
  WorldConfig cfg;
  cfg.train_speed = 40;
  Fixture f(cfg, 40, 14);
  f.world.add_building(f.bdef("hall"), f.foe, {28, 5}, true);   // food 50
  f.world.add_building(f.bdef("barracks"), f.foe, {20, 1}, true);  // grunt, footman
  f.world.player(f.foe).stock = {100000, 0, 0};
  f.spawn("peon", {26, 6}, f.foe);
  f.world.add_mine({33, 8}, 3, 100000);

  AiConfig ai_cfg;
  ai_cfg.think_interval = 0.5;
  ai_cfg.start_delay = 1e9;  // never attack: just watch what it builds
  Ai ai(f.world, registry(), f.foe, ai_cfg);
  for (int i = 0; i < 1200; ++i) {  // 60 s
    ai.update(0.05);
    f.world.update(0.05);
  }
  int grunts = 0, footmen = 0;
  f.world.units().for_each([&](const Unit& u) {
    grunts += u.def->id == "grunt" ? 1 : 0;
    footmen += u.def->id == "footman" ? 1 : 0;
  });
  CHECK(grunts >= 2);
  CHECK(footmen >= 2);
  CHECK(std::abs(grunts - footmen) <= 1);  // it alternates
}

TEST_CASE("Food limits how many units a player can have") {
  Fixture f;
  const EntityId bunker = f.world.add_building(f.bdef("bunker"), f.me, {5, 5}, true);  // food 2
  f.world.player(f.me).stock.gold = 100000;
  CHECK(f.world.food_cap(f.me) == 2);
  CHECK(f.world.food_used(f.me) == 0);

  CHECK(f.world.order_train(f.me, bunker, f.def("grunt")).ok);
  CHECK(f.world.order_train(f.me, bunker, f.def("grunt")).ok);
  CHECK(f.world.food_used(f.me) == 2);  // queued units already count
  const auto refused = f.world.order_train(f.me, bunker, f.def("grunt"));
  CHECK_FALSE(refused.ok);
  CHECK(refused.message.find("food") != std::string::npos);

  // A finished farm adds room; an unfinished one does not.
  f.world.add_building(f.bdef("farm"), f.me, {12, 5}, false);
  CHECK(f.world.food_cap(f.me) == 2);
  f.world.add_building(f.bdef("farm"), f.me, {16, 5}, true);
  CHECK(f.world.food_cap(f.me) == 6);
  CHECK(f.world.order_train(f.me, bunker, f.def("grunt")).ok);

  // Cancelling frees the food again; so does a unit dying.
  CHECK(f.world.order_cancel_train(f.me, bunker, 0).ok);
  CHECK(f.world.food_used(f.me) == 2);
  const auto grunt = f.spawn("grunt", {20, 5}, f.me);
  CHECK(f.world.food_used(f.me) == 3);
  f.world.kill_unit(grunt);
  CHECK(f.world.food_used(f.me) == 2);
}

TEST_CASE("Units leave the building for its rally point; workers sent to a mine start mining") {
  WorldConfig cfg;
  cfg.train_speed = 50;
  Fixture f(cfg);
  f.world.add_building(f.bdef("hall"), f.me, {2, 2}, true);
  f.world.add_mine({20, 8}, 3, 1000);
  const EntityId barracks = f.world.add_building(f.bdef("barracks"), f.me, {8, 8}, true);
  f.world.player(f.me).stock.gold = 5000;

  CHECK_FALSE(f.world.order_rally(f.foe, barracks, {15, 3}).ok);  // not yours
  REQUIRE(f.world.order_rally(f.me, barracks, {15, 3}).ok);
  REQUIRE(f.world.order_train(f.me, barracks, f.def("grunt")).ok);
  f.run(5);
  Point at{};
  f.world.units().for_each([&](const Unit& u) { at = u.tile; });
  CHECK(std::max(std::abs(at.x - 15), std::abs(at.y - 3)) <= 1);  // walked to the rally point

  // A worker rallied onto the mine goes to work by itself.
  const EntityId hall = f.world.building_at({2, 2});
  REQUIRE(f.world.order_rally(f.me, hall, {20, 8}).ok);
  REQUIRE(f.world.order_train(f.me, hall, f.def("peon")).ok);
  f.run(30);
  CHECK(f.world.player(f.me).stock.gold > 5000 - 600 - 400);  // paid for both units, then earned some back
}

TEST_CASE("A unit's death says where and facing which way, for the corpse") {
  Fixture f;
  const auto grunt = f.spawn("grunt", {5, 5}, f.me);
  const auto prey = f.spawn("peon", {6, 5}, f.foe);
  f.world.order_attack(std::array{grunt}, {6, 5});
  f.run(3);
  REQUIRE(f.count<ev::UnitDied>() == 1);
  for (const auto& e : f.seen)
    if (const auto* d = std::get_if<ev::UnitDied>(&e)) {
      CHECK(d->unit == prey);
      CHECK(d->owner == f.foe);
      CHECK(d->def == &f.def("peon"));
      CHECK(d->x == 6 * 32 + 16);
      CHECK(d->y == 5 * 32 + 16);
    }
}

TEST_CASE("Destroying everything the other player owns wins the game") {
  Fixture f;
  const auto grunt = f.spawn("grunt", {5, 5}, f.me);
  const auto peon = f.spawn("peon", {7, 5}, f.foe);
  (void)peon;
  CHECK_FALSE(evaluate_outcome(f.world, f.me).has_value());
  f.run(10);
  CHECK(evaluate_outcome(f.world, f.me) == Outcome::Victory);
  CHECK(evaluate_outcome(f.world, f.foe) == Outcome::Defeat);
  (void)grunt;
}

TEST_CASE("The same seed gives the same fight") {
  const auto fight = [](unsigned seed) {
    WorldConfig cfg;
    cfg.seed = seed;
    Fixture f(cfg);
    const auto a = f.spawn("brute", {5, 5}, f.me);
    const auto b = f.spawn("brute", {6, 5}, f.foe);
    f.run(4);
    return std::pair{f.world.units().alive(a) ? f.unit(a)->hp : -1, f.world.units().alive(b) ? f.unit(b)->hp : -1};
  };
  CHECK(fight(7) == fight(7));
  bool differs = false;
  for (unsigned s = 1; s < 8 && !differs; ++s) differs = fight(s) != fight(100 + s);
  CHECK(differs);  // damage really is random
}

TEST_CASE("A computer player builds a farm when food runs out, cutting wood for it first") {
  Fixture f(WorldConfig{}, 40, 14);
  f.world.add_building(f.bdef("smallhall"), f.foe, {20, 5}, true);  // food 2
  f.world.add_mine({14, 1}, 3, 100000);
  for (int y = 9; y <= 11; ++y) f.world.add_tree({30, y}, 100);
  f.world.player(f.foe).stock = {1000, 0, 0};                     // no lumber at all
  f.spawn("peon", {18, 4}, f.foe);
  f.spawn("peon", {18, 6}, f.foe);                                // 2 units: the food is exactly used up
  CHECK(f.world.food_used(f.foe) == f.world.food_cap(f.foe));

  AiConfig cfg;
  cfg.think_interval = 0.5;
  Ai ai(f.world, registry(), f.foe, cfg);
  for (int i = 0; i < 2400; ++i) {  // 120 s
    ai.update(0.05);
    f.world.update(0.05);
  }

  int farms = 0;
  f.world.buildings().for_each([&](const Building& b) { farms += (b.def->id == "farm" && b.complete) ? 1 : 0; });
  CHECK(farms >= 1);
  CHECK(f.world.food_cap(f.foe) >= 6);
}

TEST_CASE("The computer player holds its first attack until the start delay is over") {
  WorldConfig cfg;
  cfg.train_speed = 50;
  Fixture f(cfg, 40, 14);
  f.world.add_building(f.bdef("hall"), f.me, {2, 5}, true);
  f.world.add_building(f.bdef("hall"), f.foe, {30, 5}, true);
  f.world.add_building(f.bdef("barracks"), f.foe, {30, 10}, true);
  f.world.add_mine({24, 1}, 3, 100000);
  f.world.player(f.foe).stock = {6000, 0, 0};
  f.spawn("peon", {28, 4}, f.foe);

  AiConfig ai_cfg;
  ai_cfg.think_interval = 0.5;
  ai_cfg.first_wave = 2;
  ai_cfg.start_delay = 60;
  Ai ai(f.world, registry(), f.foe, ai_cfg);
  for (int i = 0; i < 1000; ++i) {  // 50 s: enough fighters, but too early
    ai.update(0.05);
    f.world.update(0.05);
  }
  int fighters = 0;
  f.world.units().for_each([&](const Unit& u) { fighters += (u.owner == f.foe && !u.def->worker) ? 1 : 0; });
  CHECK(fighters >= 2);
  CHECK(ai.waves_sent() == 0);

  for (int i = 0; i < 400; ++i) {  // past 60 s
    ai.update(0.05);
    f.world.update(0.05);
  }
  CHECK(ai.waves_sent() >= 1);
}

TEST_CASE("A computer player mines, trains fighters and attacks in waves") {
  WorldConfig cfg;
  cfg.train_speed = 25;
  Fixture f(cfg, 40, 14);
  // The player: a hall with a couple of defenders.
  f.world.add_building(f.bdef("hall"), f.me, {2, 5}, true);
  f.world.player(f.me).stock = {};
  // The computer player: hall, barracks, mine and two peons.
  f.world.add_building(f.bdef("hall"), f.foe, {30, 5}, true);
  f.world.add_building(f.bdef("barracks"), f.foe, {30, 10}, true);
  f.world.add_mine({24, 1}, 3, 100000);
  f.world.player(f.foe).stock = {1200, 0, 0};
  f.spawn("peon", {28, 4}, f.foe);
  f.spawn("peon", {28, 6}, f.foe);

  AiConfig cfg_ai;
  cfg_ai.think_interval = 0.5;
  cfg_ai.desired_workers = 3;
  cfg_ai.first_wave = 3;
  cfg_ai.wave_growth = 1;
  cfg_ai.start_delay = 0;
  Ai ai(f.world, registry(), f.foe, cfg_ai);
  int hall_hp_min = 100;
  for (int i = 0; i < 4000; ++i) {  // 200 s
    ai.update(0.05);
    f.world.update(0.05);
    if (const auto id = f.world.building_at({2, 5}); id.valid())
      hall_hp_min = std::min(hall_hp_min, f.world.buildings().get(id)->hp);
    else
      hall_hp_min = 0;
  }

  int fighters = 0;
  f.world.units().for_each([&](const Unit& u) { fighters += (u.owner == f.foe && u.def->can_attack() && !u.def->worker) ? 1 : 0; });
  CHECK(fighters > 0);                  // it trained some
  CHECK(ai.waves_sent() >= 1);          // and sent them
  CHECK(hall_hp_min < 100);             // the player's hall was hit
  CHECK(f.world.player(f.foe).stock.gold > 0);  // its peons delivered gold
}
