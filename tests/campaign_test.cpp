#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <filesystem>

#include "core/campaign.hpp"
#include "core/defs.hpp"
#include "core/pathfinding.hpp"
#include "core/pud.hpp"
#include "core/scenario.hpp"
#include "core/tilemap.hpp"
#include "core/tileset.hpp"

using namespace myrts;
namespace fs = std::filesystem;

TEST_CASE("a campaign is read in order and ends after the last level") {
  const auto c = Campaign::parse(R"({"levels":[
    {"name":"One","map":"a.pud","ai":{"first_wave":7},"enemy_stock":{"gold":5},"enemy_workers":4},
    {"name":"Two","map":"b.pud"}]})");
  REQUIRE(c.has_value());
  CHECK(c->size() == 2);
  CHECK(c->level(0).ai.first_wave == 7);
  CHECK(c->level(0).enemy_stock.gold == 5);
  CHECK(c->level(0).enemy_workers == 4);
  CHECK(c->level(1).ai.first_wave == AiConfig{}.first_wave);  // defaults where the level says nothing
  CHECK(c->next(0) == 1);
  CHECK_FALSE(c->next(1).has_value());
  CHECK(c->level(99).name == "Two");  // out of range stays on the last level
}

TEST_CASE("an invalid campaign is rejected") {
  CHECK_FALSE(Campaign::parse(R"({"levels":[]})").has_value());
  CHECK_FALSE(Campaign::parse(R"({"levels":[{"name":"x"}]})").has_value());
  CHECK_FALSE(Campaign::parse("nope").has_value());
}

TEST_CASE("every level of the shipped campaign loads, and its bases connect by land exactly when it is a land level") {
  const fs::path assets = fs::path(MYRTS_SOURCE_DIR) / "assets";
  const auto campaign = Campaign::load_file((assets / "data" / "campaign.json").string());
  REQUIRE(campaign.has_value());
  const auto tilesets = TileSets::load_file((assets / "data" / "tilesets.json").string());
  REQUIRE(tilesets.has_value());
  const auto shared = std::make_shared<const TileSets>(*tilesets);

  for (const Level& level : campaign->levels()) {
    INFO(level.name);
    const auto pud = load_pud((assets / level.map).string());
    REQUIRE(pud.has_value());
    const auto map = TileMap::from_pud(*pud, shared);
    REQUIRE(map.has_value());
    const Scenario scenario = scenario_from(*pud);
    REQUIRE(scenario.starts.size() >= 2);
    const auto far = scenario.farthest_from(0);
    REQUIRE(far.has_value());
    Grid land = map->land_blocked();
    for (const Point t : map->tree_tiles()) land.set_blocked(t, false);  // forests can be chopped through
    const Point a = scenario.starts[0].start, b = scenario.starts[*far].start;
    const auto path = a_star_closest(land, a, b);
    REQUIRE_FALSE(path.empty());
    const Point end = path.back();
    INFO("a=" << a.x << "," << a.y << " b=" << b.x << "," << b.y << " end=" << end.x << "," << end.y << " blockedB=" << land.is_blocked(b));
    const bool connected = std::abs(end.x - b.x) + std::abs(end.y - b.y) <= 4;  // the walk gets (next to) the enemy's start
    CHECK(connected == (level.kind == "land"));  // an "air" level is exactly one where walking cannot get there
  }
}

TEST_CASE("a level only allows units and buildings that exist, and air levels allow flyers") {
  const fs::path assets = fs::path(MYRTS_SOURCE_DIR) / "assets";
  const auto campaign = Campaign::load_file((assets / "data" / "campaign.json").string());
  const auto registry = Registry::load_directory((assets / "data").string());
  REQUIRE(campaign.has_value());
  REQUIRE(registry.has_value());
  for (const Level& level : campaign->levels()) {
    INFO(level.name);
    for (const auto& id : level.allow) CHECK((registry->unit(id) != nullptr || registry->building(id) != nullptr));
    if (level.kind == "air" && !level.allow.empty())
      CHECK(std::ranges::find(level.allow, "orc_dragon") != level.allow.end());
    CHECK_FALSE(level.chapter.empty());
  }
}
