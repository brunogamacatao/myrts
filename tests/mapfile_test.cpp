#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

#include "core/mapfile.hpp"
#include "core/match_setup.hpp"

using namespace myrts;
namespace fs = std::filesystem;

namespace {
const fs::path kRoot = fs::path(MYRTS_SOURCE_DIR) / "games" / "warcraft-like";

struct Game {
  Registry registry = *Registry::load_directory((kRoot / "data").string());
  Campaign campaign = *Campaign::load_file((kRoot / "data" / "campaign.json").string());
  std::shared_ptr<const TileSets> tilesets = std::make_shared<const TileSets>(*TileSets::load_file((kRoot / "data" / "tilesets.json").string()));
  GameRules rules;
};
}  // namespace

TEST_CASE("a Warcraft II map converts to the native format and back without losing anything", "[map]") {
  const Game g;
  const auto original = load_map((kRoot / g.campaign.level(0).map).string(), *g.tilesets, *g.registry.resources(), g.rules);
  REQUIRE(original);
  REQUIRE_FALSE(original->tiles.empty());
  CHECK(original->starts.size() >= 2);
  CHECK_FALSE(original->mines.empty());
  for (const auto& m : original->mines) CHECK(g.registry.resources()->index(m.resource) != kNoResource);

  const std::string text = write_mrmap(*original);
  const auto again = parse_mrmap(text, *g.tilesets);
  REQUIRE(again);
  CHECK(again->width == original->width);
  CHECK(again->height == original->height);
  CHECK(again->tileset == original->tileset);
  CHECK(again->tiles == original->tiles);
  CHECK(again->movement == original->movement);
  REQUIRE(again->starts.size() == original->starts.size());
  for (std::size_t i = 0; i < again->starts.size(); ++i) {
    CHECK(again->starts[i].owner == original->starts[i].owner);
    CHECK(again->starts[i].at == original->starts[i].at);
    CHECK(again->starts[i].hall == original->starts[i].hall);
  }
  REQUIRE(again->mines.size() == original->mines.size());
  for (std::size_t i = 0; i < again->mines.size(); ++i) {
    CHECK(again->mines[i].at == original->mines[i].at);
    CHECK(again->mines[i].resource == original->mines[i].resource);
    CHECK(again->mines[i].amount == original->mines[i].amount);
  }
}

TEST_CASE("a native map plays exactly like the .pud it was converted from", "[map]") {
  const Game g;
  const fs::path dir = fs::temp_directory_path() / "myrts_mapfile_test";
  fs::create_directories(dir);
  const Level& base = g.campaign.level(0);
  const auto converted = load_map((kRoot / base.map).string(), *g.tilesets, *g.registry.resources(), g.rules);
  REQUIRE(converted);
  std::ofstream(dir / "copy.mrmap") << write_mrmap(*converted);

  Level native = base;
  native.map = "copy.mrmap";
  MatchOptions from_pud, from_native;
  from_pud.assets_dir = kRoot.string();
  from_native.assets_dir = dir.string();
  const auto a = build_match(g.registry, g.tilesets, base, from_pud);
  const auto b = build_match(g.registry, g.tilesets, native, from_native);
  REQUIRE(a);
  REQUIRE(b);
  CHECK(a->world->width() == b->world->width());
  CHECK(a->local_hall == b->local_hall);
  CHECK(a->enemy_hall == b->enemy_hall);
  REQUIRE(a->world->mines().size() == b->world->mines().size());
  for (std::size_t i = 0; i < a->world->mines().size(); ++i) {
    CHECK(a->world->mines()[i].site.origin() == b->world->mines()[i].site.origin());
    CHECK(a->world->mines()[i].site.remaining() == b->world->mines()[i].site.remaining());
  }
  CHECK(a->world->trees().positions() == b->world->trees().positions());
  fs::remove_all(dir);
}

TEST_CASE("bad native maps are refused with a message", "[map]") {
  const Game g;
  const auto valid = *load_map((kRoot / g.campaign.level(0).map).string(), *g.tilesets, *g.registry.resources(), g.rules);
  const auto edit = [&](auto change) {
    auto m = valid;
    change(m);
    return parse_mrmap(write_mrmap(m), *g.tilesets);
  };
  CHECK(edit([](MapFile&) {}));
  const auto wrong_tileset = edit([](MapFile& m) { m.tileset = "lava"; });
  REQUIRE_FALSE(wrong_tileset);
  CHECK(wrong_tileset.error().contains("lava"));
  CHECK_FALSE(edit([](MapFile& m) { m.width += 1; }));  // tile data no longer matches the size
  CHECK_FALSE(edit([](MapFile& m) { m.starts.push_back({3, {m.width + 5, 0}, std::nullopt}); }));
  CHECK_FALSE(parse_mrmap("{", *g.tilesets));
  CHECK_FALSE(parse_mrmap(R"({"tileset":"forest","width":2,"height":2,"tiles":"!!!"})", *g.tilesets));
  CHECK_FALSE(load_map("/does/not/exist.mrmap", *g.tilesets, *g.registry.resources(), g.rules));
}

TEST_CASE("a game may have any number of tile sets", "[map]") {
  const auto one = TileSets::parse(R"({"eras":[{"name":"only","image":"x.png","tiles":[[16,0,0]]}]})");
  REQUIRE(one);
  CHECK(one->count() == 1);
  CHECK(one->find("only") == Era::Forest);
  CHECK_FALSE(one->find("forest").has_value());
  CHECK_FALSE(one->has(Era::Winter));
  CHECK_FALSE(TileSets::parse(R"({"eras":[]})"));
}
