#include <catch2/catch_test_macros.hpp>
#include <array>
#include <filesystem>

#include "core/match_setup.hpp"
#include "core/rules.hpp"
#include "core/scenario.hpp"
#include "test_resources.hpp"

using namespace myrts;
namespace fs = std::filesystem;

namespace {

constexpr const char* kUnits = R"({"units":[{"id":"grunt","name":"Grunt","hp":10,"move_speed":96,"sprite":"s","attack_seconds":1.0,
   "stats":{"damage_min":500,"damage_max":500,"range":1,"sight":3}}]})";
constexpr const char* kBuildings = R"({"buildings":[
  {"id":"hall","name":"Hall","size":3,"hp":50,"build_seconds":1,"sprite":"s"},
  {"id":"tower","name":"Tower","size":2,"hp":50,"build_seconds":1,"sprite":"s"}]})";

const Registry& registry() {
  static const Registry r = *Registry::parse(kUnits, kBuildings, R"({"commands":[]})");
  return r;
}

struct Fixture {
  World world{WorldConfig{}, Grid(30, 12)};
  PlayerId me = world.add_player("me", "a", Stock{});
  PlayerId foe = world.add_player("foe", "b", Stock{});
  Fixture() {
    world.add_building(*registry().building("hall"), me, {1, 1}, true);
    world.add_building(*registry().building("hall"), foe, {20, 1}, true);
  }
  void run(double s) {
    for (double t = 0; t < s; t += 0.1) world.update(0.1);
  }
};

/// Has `owner`'s grunt walk up to the building standing on `tile` and knock it down.
void demolish(World& world, PlayerId attacker, Point tile) {
  const Point next_to{tile.x - 1, tile.y};
  const EntityId grunt = world.spawn_unit(*registry().unit("grunt"), attacker, next_to);
  REQUIRE(grunt.valid());
  const std::array ids{grunt};
  REQUIRE(world.order_attack(ids, tile).ok);
  for (int i = 0; i < 80; ++i) world.update(0.1);
}

Objective make(const char* type) {
  Objective o;
  o.type = type;
  return o;
}

}  // namespace

TEST_CASE("without objectives the match is won by destroying everything of the others", "[objectives]") {
  Fixture f;
  CHECK_FALSE(evaluate_outcome(f.world, f.me).has_value());
  demolish(f.world, f.me, {20, 1});
  CHECK(evaluate_outcome(f.world, f.me) == Outcome::Victory);
  CHECK(evaluate_outcome(f.world, f.foe) == Outcome::Defeat);
}

TEST_CASE("victory objectives must all hold", "[objectives]") {
  Fixture f;
  Objectives goals;
  Objective survive = make("survive");
  survive.seconds = 5;
  Objective collect = make("collect");
  collect.resource = "gold";
  collect.amount = 100;
  goals.victory = {survive, collect};

  f.run(6);
  CHECK_FALSE(evaluate_outcome(f.world, f.me, goals).has_value());  // survived, but no gold
  f.world.player(f.me).stock[kGold] = 100;
  CHECK(evaluate_outcome(f.world, f.me, goals) == Outcome::Victory);  // the enemy still stands: not needed
}

TEST_CASE("build and destroy_building objectives", "[objectives]") {
  Fixture f;
  Objectives goals;
  Objective build = make("build");
  build.building = "tower";
  build.amount = 2;
  goals.victory = {build};
  f.world.add_building(*registry().building("tower"), f.me, {6, 1}, true);
  CHECK_FALSE(evaluate_outcome(f.world, f.me, goals).has_value());
  f.world.add_building(*registry().building("tower"), f.me, {10, 1}, false);  // unfinished does not count
  CHECK_FALSE(evaluate_outcome(f.world, f.me, goals).has_value());
  f.world.add_building(*registry().building("tower"), f.me, {14, 1}, true);
  CHECK(evaluate_outcome(f.world, f.me, goals) == Outcome::Victory);

  Fixture g;
  Objectives kill;
  Objective target = make("destroy_building");
  target.building = "tower";
  kill.victory = {target};
  const EntityId tower = g.world.add_building(*registry().building("tower"), g.foe, {24, 5}, true);
  CHECK_FALSE(evaluate_outcome(g.world, g.me, kill).has_value());
  (void)tower;
  demolish(g.world, g.me, {24, 5});
  CHECK(evaluate_outcome(g.world, g.me, kill) == Outcome::Victory);  // the enemy hall still stands
}

TEST_CASE("defeat objectives: a time limit and losing a key building", "[objectives]") {
  Fixture f;
  Objectives goals;
  Objective limit = make("time_limit");
  limit.seconds = 4;
  goals.defeat = {limit};
  CHECK_FALSE(evaluate_outcome(f.world, f.me, goals).has_value());
  f.run(5);
  CHECK(evaluate_outcome(f.world, f.me, goals) == Outcome::Defeat);

  Fixture g;
  Objectives keep;
  Objective tower = make("lose_building");
  tower.building = "tower";
  keep.defeat = {tower};
  const EntityId id = g.world.add_building(*registry().building("tower"), g.me, {6, 1}, true);
  CHECK_FALSE(evaluate_outcome(g.world, g.me, keep).has_value());
  (void)id;
  demolish(g.world, g.foe, {6, 1});
  CHECK(evaluate_outcome(g.world, g.me, keep) == Outcome::Defeat);
}

TEST_CASE("campaign objectives and AI profiles are parsed and validated", "[objectives]") {
  const char* profiles = R"({"profiles":{"calm":{"start_delay":500,"first_wave":9},"rush":{"start_delay":10}}})";
  const auto c = Campaign::parse(R"({"levels":[
    {"name":"a","map":"m","ai_profile":"calm","ai":{"first_wave":4},
     "objectives":{"victory":[{"type":"survive","seconds":60,"text":"Hold out"},{"type":"collect","resource":"gold","amount":500}],
                   "defeat":[{"type":"lose_building","building":"hall"}]},
     "more_opponents":[{"ai_profile":"rush","workers":3,"stock":{"gold":10}}]}]})", profiles);
  REQUIRE(c);
  const Level& l = c->level(0);
  CHECK(l.ai.start_delay == 500);  // from the profile
  CHECK(l.ai.first_wave == 4);     // the level overrides one value
  CHECK(l.ai_profile == "calm");
  REQUIRE(l.objectives.victory.size() == 2);
  CHECK(l.objectives.victory[0].text == "Hold out");
  CHECK(l.objectives.defeat[0].building == "hall");
  REQUIRE(l.more_opponents.size() == 1);
  CHECK(l.more_opponents[0].ai.start_delay == 10);
  CHECK(l.more_opponents[0].workers == 3);

  CHECK_FALSE(Campaign::parse(R"({"levels":[{"name":"a","map":"m","ai_profile":"nope"}]})", profiles));
  CHECK_FALSE(Campaign::parse(R"({"levels":[{"name":"a","map":"m","objectives":{"victory":[{"type":"dance"}]}}]})"));
  CHECK_FALSE(Campaign::parse(R"({"levels":[{"name":"a","map":"m","objectives":{"victory":[{"type":"survive"}]}}]})"));
  CHECK_FALSE(Campaign::parse(R"({"levels":[{"name":"a","map":"m","objectives":{"defeat":[{"type":"survive","seconds":3}]}}]})"));
}

TEST_CASE("a level with more opponents gets one player and one AI each, on separate start locations", "[objectives]") {
  const fs::path root = fs::path(MYRTS_SOURCE_DIR) / "games" / "warcraft-like";
  const auto registry = Registry::load_directory((root / "data").string());
  auto campaign = Campaign::load_file((root / "data" / "campaign.json").string(), (root / "data" / "ai_profiles.json").string());
  const auto tilesets = TileSets::load_file((root / "data" / "tilesets.json").string());
  REQUIRE((registry && campaign && tilesets));
  const auto shared = std::make_shared<const TileSets>(*tilesets);

  // Find a shipped map with at least three start locations.
  for (const Level& base : campaign->levels()) {
    const auto pud = load_pud((root / base.map).string());
    REQUIRE(pud);
    if (scenario_from(*pud).starts.size() < 3) continue;
    Level level = base;
    level.more_opponents.push_back({});
    level.objectives.victory = {make("survive")};
    level.objectives.victory[0].seconds = 30;
    MatchOptions o;
    o.assets_dir = root.string();
    const auto m = build_match(*registry, shared, level, o);
    REQUIRE(m);
    CHECK(m->world->player_count() == 3);
    CHECK(m->ais.size() == 2);
    REQUIRE(m->more_opponents.size() == 1);
    CHECK(m->world->has_assets(m->more_opponents[0]));
    CHECK(m->objectives.victory.size() == 1);

    level.objectives.victory[0] = make("build");  // names no building
    level.objectives.victory[0].building = "no_such_building";
    CHECK_FALSE(build_match(*registry, shared, level, o));
    return;
  }
  SUCCEED("no shipped map has three start locations");
}
