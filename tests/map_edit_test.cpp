#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <random>

#include "core/campaign.hpp"
#include "core/defs.hpp"
#include "editor_model/map_edit.hpp"

using namespace myrts;
using namespace myrts::editor;
namespace fs = std::filesystem;

namespace {
const fs::path kRoot = fs::path(MYRTS_SOURCE_DIR) / "games" / "warcraft-like";

std::shared_ptr<const TileSets> tilesets() {
  static const auto sets = std::make_shared<const TileSets>(*TileSets::load_file((kRoot / "data" / "tilesets.json").string()));
  return sets;
}

MapFile load(const char* name) {
  const auto registry = Registry::load_directory((kRoot / "data").string());
  const auto map = load_map((kRoot / "maps" / name).string(), *tilesets(), *registry->resources(), GameRules{});
  REQUIRE(map.has_value());
  return *map;
}

/// Every tile exists in the tile set and agrees with the vertices around it.
void check_consistent(const MapEdit& e) {
  for (int y = 0; y < e.height(); ++y)
    for (int x = 0; x < e.width(); ++x) {
      const std::uint16_t code = e.map().tiles[static_cast<std::size_t>(y * e.width() + x)];
      REQUIRE(e.tileset().has(code));
      const auto q = decompose(code);
      if (!q || is_wall(q->tl)) continue;
      const Quad want{e.vertex(x, y), e.vertex(x + 1, y), e.vertex(x, y + 1), e.vertex(x + 1, y + 1)};
      INFO("tile " << x << "," << y);
      REQUIRE(*q == want);
    }
}
}  // namespace

TEST_CASE("a new map is all one terrain, with two starts, and the tile set can draw it") {
  const auto e = MapEdit::create(tilesets(), "forest", 32, 32, Terrain::GrassLight, "Fresh");
  REQUIRE(e.has_value());
  CHECK(e->width() == 32);
  CHECK(e->map().tiles.size() == 32u * 32u);
  CHECK(e->map().starts.size() == 2);
  CHECK(e->vertex(5, 5) == Terrain::GrassLight);
  check_consistent(*e);
  CHECK_FALSE(MapEdit::create(tilesets(), "forest", 4, 4, Terrain::GrassLight, "x").has_value());
  CHECK_FALSE(MapEdit::create(tilesets(), "moon", 32, 32, Terrain::GrassLight, "x").has_value());
  CHECK_FALSE(MapEdit::create(tilesets(), "forest", 32, 32, Terrain::HumanWall, "x").has_value());
}

TEST_CASE("a real map's vertices agree with its tiles") {
  auto e = MapEdit::from_map(load("Skirmish BNE.pud"), tilesets());
  REQUIRE(e.has_value());
  check_consistent(*e);
}

TEST_CASE("painting water in grass makes a coast, tiles stay drawable, and the stroke undoes exactly") {
  auto e = MapEdit::create(tilesets(), "forest", 32, 32, Terrain::GrassLight, "Lake");
  REQUIRE(e.has_value());
  const auto before = e->map().tiles;
  e->begin_stroke();
  REQUIRE(e->paint(16, 16, 3, Terrain::WaterLight));
  REQUIRE(e->paint(18, 16, 3, Terrain::WaterLight));
  e->end_stroke();
  CHECK(e->vertex(16, 16) == Terrain::WaterLight);
  CHECK(e->vertex(18, 16) == Terrain::WaterLight);
  // Away from the water it is still grass; between them there is dirt (the coast).
  CHECK(e->vertex(2, 2) == Terrain::GrassLight);
  bool coast = false;
  for (int y = 10; y < 24; ++y)
    for (int x = 10; x < 26; ++x) coast = coast || e->vertex(x, y) == Terrain::DirtLight;
  CHECK(coast);
  check_consistent(*e);
  CHECK(e->dirty());
  CHECK(e->map().tiles != before);

  CHECK(e->undo());
  CHECK(e->map().tiles == before);  // one stroke, one undo
  CHECK_FALSE(e->dirty());
  check_consistent(*e);
  CHECK(e->redo());
  CHECK(e->vertex(16, 16) == Terrain::WaterLight);
  check_consistent(*e);
}

TEST_CASE("any terrain can be painted anywhere: random strokes always leave a drawable, consistent map") {
  auto e = MapEdit::create(tilesets(), "forest", 40, 40, Terrain::GrassLight, "Chaos");
  REQUIRE(e.has_value());
  std::mt19937 rng(12345);
  const std::array<Terrain, 8> palette{Terrain::WaterLight, Terrain::WaterDark, Terrain::DirtLight, Terrain::DirtDark,
                                       Terrain::GrassLight, Terrain::GrassDark, Terrain::Trees, Terrain::Rocks};
  int painted = 0;
  for (int i = 0; i < 150; ++i) {
    const int x = static_cast<int>(rng() % 41), y = static_cast<int>(rng() % 41), size = 1 + static_cast<int>(rng() % 4);
    e->begin_stroke();
    if (e->paint(x, y, size, palette[rng() % palette.size()])) ++painted;
    e->end_stroke();
  }
  CHECK(painted == 150);  // the fixing never gives up
  check_consistent(*e);
  const auto tilemap = tilemap_from(e->map(), tilesets());
  REQUIRE(tilemap.has_value());
  CHECK(tilemap->missing_tiles() == 0);
  while (e->can_undo()) e->undo();
  check_consistent(*e);
  for (int y = 0; y <= 40; ++y)
    for (int x = 0; x <= 40; ++x) REQUIRE(e->vertex(x, y) == Terrain::GrassLight);
}

TEST_CASE("painting on a real map keeps it consistent, and trees and movement follow") {
  auto e = MapEdit::from_map(load("Skirmish BNE.pud"), tilesets());
  REQUIRE(e.has_value());
  const int cx = e->width() / 2, cy = e->height() / 2;
  e->begin_stroke();
  REQUIRE(e->paint(cx, cy, 4, Terrain::WaterLight));
  e->end_stroke();
  check_consistent(*e);
  const auto tilemap = tilemap_from(e->map(), tilesets());
  REQUIRE(tilemap.has_value());
  CHECK(tilemap->missing_tiles() == 0);
  CHECK_FALSE(tilemap->land_blocked().is_walkable({cx, cy}));  // water stops land units
  CHECK(tilemap->water_blocked().is_walkable({cx, cy}));
  e->begin_stroke();
  REQUIRE(e->paint(cx, cy, 4, Terrain::Trees));
  e->end_stroke();
  CHECK(tilemap_from(e->map(), tilesets())->is_tree({cx, cy}));
  CHECK(e->undo());
  CHECK(e->undo());
  CHECK_FALSE(e->dirty());
}

TEST_CASE("a saved map reads back the same, and its terrain still edits") {
  auto e = MapEdit::create(tilesets(), "forest", 24, 24, Terrain::DirtLight, "Roundtrip");
  REQUIRE(e.has_value());
  e->paint(12, 12, 3, Terrain::Trees);
  const auto parsed = parse_mrmap(e->to_mrmap(), *tilesets());
  REQUIRE(parsed.has_value());
  CHECK(parsed->tiles == e->map().tiles);
  auto again = MapEdit::from_map(*parsed, tilesets());
  REQUIRE(again.has_value());
  CHECK(again->vertex(12, 12) == Terrain::Trees);
  check_consistent(*again);
  e->mark_saved();
  CHECK_FALSE(e->dirty());
  e->paint(3, 3, 1, Terrain::WaterDark);
  CHECK(e->dirty());
}

TEST_CASE("starts and mines can be added, moved, changed and removed, and every change undone") {
  auto e = MapEdit::create(tilesets(), "forest", 32, 32, Terrain::GrassLight, "Objects");
  REQUIRE(e.has_value());
  const auto starts = e->map().starts;
  CHECK_FALSE(e->dirty());

  const std::size_t s = e->add_start(2, {5, 5});
  CHECK(e->map().starts.size() == 3);
  CHECK(e->move_start(s, {6, 7}));
  CHECK(e->move_start(s, {7, 7}));  // the same object dragged on: one step
  CHECK(e->map().starts[s].at == Point{7, 7});
  CHECK(e->set_start(s, 3, Point{8, 8}));
  CHECK(e->map().starts[s].owner == 3);
  CHECK(e->map().starts[s].hall == Point{8, 8});
  const std::size_t m = e->add_mine({10, 10}, "gold", 1000);
  for (int amount = 1100; amount <= 1500; amount += 100) e->set_mine(m, "gold", amount);  // typing in the field
  CHECK(e->map().mines[m].amount == 1500);
  CHECK_FALSE(e->move_start(99, {1, 1}));
  CHECK_FALSE(e->set_mine(99, "gold", 1));
  CHECK(e->remove_start(s));
  CHECK(e->dirty());

  CHECK(e->undo());  // the removed start is back
  CHECK(e->map().starts.size() == 3);
  CHECK(e->undo());  // the mine's amount went back in one step
  CHECK(e->map().mines[m].amount == 1000);
  CHECK(e->undo());  // the mine is gone
  CHECK(e->map().mines.empty());
  CHECK(e->undo());  // the start's owner and hall
  CHECK(e->map().starts[s].hall == std::nullopt);
  CHECK(e->undo());  // its move
  CHECK(e->map().starts[s].at == Point{5, 5});
  CHECK(e->undo());
  CHECK(e->map().starts.size() == 2);
  CHECK_FALSE(e->can_undo());
  CHECK_FALSE(e->dirty());
  CHECK(e->redo());
  CHECK(e->map().starts.size() == 3);
}

TEST_CASE("every map of the shipped campaign has no problems for the level that uses it") {
  const auto registry = Registry::load_directory((kRoot / "data").string());
  const auto campaign = Campaign::load_file((kRoot / "data" / "campaign.json").string());
  REQUIRE(registry.has_value());
  REQUIRE(campaign.has_value());
  for (const Level& level : campaign->levels()) {
    INFO(level.name);
    auto e = MapEdit::from_map(*load_map((kRoot / level.map).string(), *tilesets(), *registry->resources(), GameRules{}), tilesets());
    REQUIRE(e.has_value());
    const auto problems = e->problems(*registry->resources(), level.kind);
    for (const auto& p : problems) INFO(p);
    CHECK(problems.empty());
  }
}

TEST_CASE("problems names what would stop a level") {
  const auto registry = Registry::load_directory((kRoot / "data").string());
  auto e = MapEdit::create(tilesets(), "forest", 32, 32, Terrain::GrassLight, "Checks");
  REQUIRE(e.has_value());
  const ResourceTable& resources = *registry->resources();
  CHECK(e->problems(resources, "land").empty());

  e->add_mine({10, 10}, "unobtainium", 100);
  CHECK(e->problems(resources, "land").size() == 1);
  e->undo();
  e->add_mine({10, 10}, "gold", 0);
  CHECK(e->problems(resources, "land").size() == 1);  // holds nothing
  e->undo();
  e->add_mine({10, 10}, "oil", 100);  // oil needs the sea
  CHECK(e->problems(resources, "land").size() == 1);
  e->undo();
  e->add_mine({10, 10}, "gold", 100);
  e->add_mine({11, 11}, "gold", 100);  // overlaps
  CHECK(e->problems(resources, "land").size() == 1);
  e->undo();
  e->undo();

  e->remove_start(1);
  CHECK(e->problems(resources, "land").size() == 1);  // no opponent
  e->undo();
  e->add_start(0, {2, 2});
  CHECK(e->problems(resources, "land").size() == 1);  // two starts for player 1
  e->undo();

  // A lake across the middle cuts the map in two: fine for an air level, not for a land one.
  e->paint(16, 0, 1, Terrain::WaterLight);
  for (int y = 0; y <= 32; ++y) e->paint(16, y, 3, Terrain::WaterLight);
  CHECK_FALSE(e->problems(resources, "land").empty());
  CHECK(e->problems(resources, "air").empty());
}
