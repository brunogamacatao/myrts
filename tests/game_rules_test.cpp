#include <catch2/catch_test_macros.hpp>
#include <filesystem>

#include "core/game_rules.hpp"
#include "core/match_setup.hpp"

using namespace myrts;
namespace fs = std::filesystem;

namespace {
const fs::path kPackage = fs::path(MYRTS_SOURCE_DIR) / "games" / "warcraft-like";
}

TEST_CASE("the shipped rules.json spells out exactly the built-in defaults", "[rules]") {
  const auto r = GameRules::load_file((kPackage / "data" / "rules.json").string());
  REQUIRE(r);
  const GameRules d;
  CHECK(r->lumber_per_tree == d.lumber_per_tree);
  CHECK(r->max_queue == d.max_queue);
  CHECK(r->scan_interval == d.scan_interval);
  CHECK(r->retry_delay == d.retry_delay);
  CHECK(r->min_damage == d.min_damage);
  CHECK(r->attack_flash == d.attack_flash);
  CHECK(r->repair_cost_share == d.repair_cost_share);
  CHECK(r->repair_min_seconds == d.repair_min_seconds);
  CHECK(r->min_unit_sight == d.min_unit_sight);
  CHECK(r->building_sight_base == d.building_sight_base);
  CHECK(r->initial_workers == d.initial_workers);
  CHECK(r->mine_size == d.mine_size);
  CHECK(r->gold_per_value_unit == d.gold_per_value_unit);
  CHECK(r->default_mine_gold == d.default_mine_gold);
  CHECK(r->ai_farm_margin == d.ai_farm_margin);
  CHECK(r->corpse_seconds == d.corpse_seconds);
  CHECK(r->corpse_fade_seconds == d.corpse_fade_seconds);
  CHECK(r->max_corpses == d.max_corpses);
  CHECK(r->air_lift == d.air_lift);
}

TEST_CASE("rules: partial files keep the defaults; typos and bad values are reported", "[rules]") {
  const auto partial = GameRules::parse(R"({"match":{"initial_workers":3}})");
  REQUIRE(partial);
  CHECK(partial->initial_workers == 3);
  CHECK(partial->mine_size == 3);

  const auto typo = GameRules::parse(R"({"match":{"initial_worker":3}})");
  REQUIRE_FALSE(typo);
  CHECK(typo.error().find("match.initial_worker") != std::string::npos);
  CHECK_FALSE(GameRules::parse(R"({"combats":{}})"));
  CHECK_FALSE(GameRules::parse(R"({"simulation":{"max_queue":0}})"));
  CHECK_FALSE(GameRules::parse(R"({"repair":{"cost_share":"a lot"}})"));
  CHECK_FALSE(GameRules::parse("not json"));
}

TEST_CASE("the rules change the match that is built", "[rules]") {
  const auto registry = Registry::load_directory((kPackage / "data").string());
  const auto campaign = Campaign::load_file((kPackage / "data" / "campaign.json").string());
  const auto tilesets = TileSets::load_file((kPackage / "data" / "tilesets.json").string());
  REQUIRE((registry && campaign && tilesets));
  const auto shared = std::make_shared<const TileSets>(*tilesets);

  const auto workers_of = [&](const GameRules& rules, PlayerId* local_out = nullptr) {
    MatchOptions o;
    o.assets_dir = kPackage.string();
    o.world.set_rules(rules);
    auto m = build_match(*registry, shared, campaign->level(0), o);
    REQUIRE(m);
    int n = 0, gold = 0;
    m->world->units().for_each([&](const Unit& u) { n += (u.owner == m->local && u.def->worker) ? 1 : 0; });
    for (const auto& mine : m->world->mines()) gold += mine.site.remaining();
    if (local_out) *local_out = m->local;
    return std::pair{n, gold};
  };

  GameRules base;
  const auto [base_workers, base_gold] = workers_of(base);
  CHECK(base_workers == 8);

  GameRules few = base;
  few.initial_workers = 3;
  few.gold_per_value_unit = base.gold_per_value_unit * 2;
  few.default_mine_gold = base.default_mine_gold * 2;
  const auto [few_workers, few_gold] = workers_of(few);
  CHECK(few_workers == 3);
  CHECK(few_gold == base_gold * 2);
}
