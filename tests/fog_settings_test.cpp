#include <catch2/catch_test_macros.hpp>

#include "core/campaign.hpp"
#include "core/game_rules.hpp"
#include "core/world.hpp"

using namespace myrts;

namespace {
constexpr const char* kUnits = R"({"units":[{"id":"scout","name":"Scout","hp":5,"move_speed":96,"sprite":"s","stats":{"sight":4}}]})";
const Registry& registry() {
  static const Registry r = *Registry::parse(kUnits, R"({"buildings":[]})", R"({"commands":[]})");
  return r;
}
/// A world with one scout at (10,10) on a 30x30 map; `fog` is the JSON of the settings to apply.
struct Fixture {
  explicit Fixture(const char* fog = "") {
    WorldConfig cfg;
    cfg.fog_settings = *FogSettings::with({}, fog);
    world = std::make_unique<World>(cfg, Grid(30, 30));
    me = world->add_player("me", "x", Stock{});
    world->spawn_unit(*registry().unit("scout"), me, {10, 10});
    world->refresh_fog();
  }
  std::unique_ptr<World> world;
  PlayerId me = 0;
  [[nodiscard]] Seen seen(int x, int y) const { return world->fog(me).at({x, y}); }
};
}  // namespace

TEST_CASE("the default fog is the original one", "[fog]") {
  Fixture f;
  CHECK(f.seen(10, 10) == Seen::Visible);
  CHECK(f.seen(14, 10) == Seen::Visible);   // sight 4
  CHECK(f.seen(16, 10) == Seen::Unknown);
  CHECK(f.seen(25, 25) == Seen::Unknown);
}

TEST_CASE("fog settings: switching it off, revealing the map, not remembering", "[fog]") {
  Fixture off(R"({"enabled":false})");
  CHECK(off.seen(25, 25) == Seen::Visible);

  Fixture revealed(R"({"reveal_map":true})");
  CHECK(revealed.seen(25, 25) == Seen::Explored);  // known, but not in sight
  CHECK(revealed.seen(10, 10) == Seen::Visible);

  Fixture forgetful(R"({"remember":false})");
  const EntityId scout = forgetful.world->unit_at({10, 10});
  REQUIRE(forgetful.world->order_move(std::array{scout}, {22, 10}).ok);
  for (int i = 0; i < 20 * 12; ++i) forgetful.world->update(0.05);
  CHECK(forgetful.seen(22, 10) == Seen::Visible);
  CHECK(forgetful.seen(6, 10) == Seen::Unknown);   // left behind: black again

  Fixture remembering;
  const EntityId s2 = remembering.world->unit_at({10, 10});
  REQUIRE(remembering.world->order_move(std::array{s2}, {22, 10}).ok);
  for (int i = 0; i < 20 * 12; ++i) remembering.world->update(0.05);
  CHECK(remembering.seen(6, 10) == Seen::Explored);
}

TEST_CASE("fog settings: sight scale and bonus, minimum sight", "[fog]") {
  Fixture wide(R"({"sight_scale":2.0})");
  CHECK(wide.seen(18, 10) == Seen::Visible);   // 4 * 2 = 8
  CHECK(wide.seen(20, 10) == Seen::Unknown);
  Fixture bonus(R"({"sight_bonus":3})");
  CHECK(bonus.seen(17, 10) == Seen::Visible);  // 4 + 3
  Fixture blind(R"({"sight_scale":0,"min_unit_sight":0})");
  CHECK(blind.seen(11, 10) == Seen::Unknown);
}

TEST_CASE("fog settings are validated, layered and read from rules and levels", "[fog]") {
  CHECK_FALSE(FogSettings::with({}, R"({"enabld":true})"));
  CHECK_FALSE(FogSettings::with({}, R"({"sight_scale":-1})"));
  CHECK_FALSE(FogSettings::with({}, R"({"unknown_color":[0,0]})"));
  CHECK_FALSE(FogSettings::with({}, R"({"unknown_color":[0,0,300]})"));
  CHECK_FALSE(FogSettings::with({}, R"({"enabled":"yes"})"));
  const auto tinted = FogSettings::with({}, R"({"unknown_color":[10,20,30],"explored_opacity":0.2,"edge_softness":2,"minimap":false})");
  REQUIRE(tinted);
  CHECK(tinted->unknown_color == std::array<int, 3>{10, 20, 30});
  CHECK(tinted->explored_opacity == 0.2);
  CHECK_FALSE(tinted->minimap);

  const auto rules = GameRules::parse(R"({"fog":{"sight_bonus":1}})");
  REQUIRE(rules);
  CHECK(rules->fog.sight_bonus == 1);
  const auto level = FogSettings::with(rules->fog, R"({"remember":false})");   // a level layers on the game's defaults
  REQUIRE(level);
  CHECK(level->sight_bonus == 1);
  CHECK_FALSE(level->remember);
  CHECK_FALSE(GameRules::parse(R"({"fog":{"nope":1}})"));

  const auto c = Campaign::parse(R"({"levels":[{"name":"a","map":"m","fog":{"reveal_map":true}}]})");
  REQUIRE(c);
  CHECK(c->level(0).fog_json.contains("reveal_map"));
  CHECK_FALSE(Campaign::parse(R"({"levels":[{"name":"a","map":"m","fog":{"typo":1}}]})"));
}
