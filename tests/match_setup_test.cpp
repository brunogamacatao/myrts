#include <catch2/catch_test_macros.hpp>
#include <filesystem>

#include "core/match_setup.hpp"

using namespace myrts;
namespace fs = std::filesystem;

namespace {

struct Data {
  fs::path assets = fs::path(MYRTS_SOURCE_DIR) / "assets";
  Registry registry = *Registry::load_directory((assets / "data").string());
  Campaign campaign = *Campaign::load_file((assets / "data" / "campaign.json").string());
  std::shared_ptr<const TileSets> tilesets =
      std::make_shared<const TileSets>(*TileSets::load_file((assets / "data" / "tilesets.json").string()));
  MatchOptions options() const {
    MatchOptions o;
    o.assets_dir = assets.string();
    return o;
  }
};

int count_units(const World& w, PlayerId owner, bool workers) {
  int n = 0;
  w.units().for_each([&](const Unit& u) { n += (u.owner == owner && u.def->worker.has_value() == workers) ? 1 : 0; });
  return n;
}

int count_buildings(const World& w, PlayerId owner) {
  int n = 0;
  w.buildings().for_each([&](const Building& b) { n += b.owner == owner ? 1 : 0; });
  return n;
}

}  // namespace

TEST_CASE("every shipped level builds a match: halls, workers, one AI for the opponent") {
  const Data d;
  for (const Level& level : d.campaign.levels()) {
    INFO(level.name);
    const auto m = build_match(d.registry, d.tilesets, level, d.options());
    REQUIRE(m.has_value());
    const World& w = *m->world;
    CHECK(w.player_count() == 2);
    CHECK(w.player(m->local).faction == level.player_faction);
    CHECK(w.player(m->enemy).faction == level.enemy_faction);
    CHECK(w.building_at(m->local_hall).valid());
    CHECK(w.building_at(m->enemy_hall).valid());
    CHECK(count_units(w, m->local, true) > 0);
    CHECK(count_units(w, m->enemy, true) > 0);
    CHECK(count_buildings(w, m->enemy) >= 1);  // its hall (and base buildings when there is room)
    CHECK(m->ais.size() == 1);
    CHECK_FALSE(w.mines().empty());
    CHECK(w.player(m->local).stock.gold == level.player_stock.gold);
  }
}

TEST_CASE("the sides of a level can be swapped: the human may command the alliance") {
  const Data d;
  Level level = d.campaign.level(0);
  level.player_faction = "alliance";
  level.enemy_faction = "orcs";
  const auto m = build_match(d.registry, d.tilesets, level, d.options());
  REQUIRE(m.has_value());
  const World& w = *m->world;
  CHECK(m->local_faction->id == "alliance");
  CHECK(w.building_at(m->local_hall).valid());
  CHECK(w.buildings().get(w.building_at(m->local_hall))->def->id == "alliance_town_hall");
  w.units().for_each([&](const Unit& u) {
    if (u.owner == m->local) CHECK(u.def->faction == "alliance");
    if (u.owner == m->enemy) CHECK(u.def->faction == "orcs");
  });
}

TEST_CASE("a level with an unknown faction or a missing map is refused with a message") {
  const Data d;
  Level level = d.campaign.level(0);
  level.enemy_faction = "elves";
  const auto bad_faction = build_match(d.registry, d.tilesets, level, d.options());
  REQUIRE_FALSE(bad_faction.has_value());
  CHECK(bad_faction.error().contains("elves"));

  level = d.campaign.level(0);
  level.map = "maps/does-not-exist.pud";
  CHECK_FALSE(build_match(d.registry, d.tilesets, level, d.options()).has_value());
}
