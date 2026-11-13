#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <filesystem>
#include <map>
#include <set>

#include "core/pud.hpp"
#include "core/scenario.hpp"
#include "core/terrain.hpp"
#include "core/tilemap.hpp"
#include "core/tileset.hpp"
#include "core/world.hpp"

using namespace myrts;
namespace fs = std::filesystem;

namespace {

const fs::path kAssets = fs::path(MYRTS_SOURCE_DIR) / "assets";

std::shared_ptr<const TileSets> tilesets() {
  static const auto sets = [] {
    auto loaded = TileSets::load_file((kAssets / "data" / "tilesets.json").string());
    if (!loaded) FAIL("cannot load tilesets: " << loaded.error());
    return std::make_shared<const TileSets>(std::move(*loaded));
  }();
  return sets;
}

std::vector<fs::path> sample_maps() {
  std::vector<fs::path> out;
  for (const auto& e : fs::directory_iterator(kAssets / "maps"))
    if (e.path().extension() == ".pud") out.push_back(e.path());
  std::ranges::sort(out);
  return out;
}

PudMap load(const fs::path& p) {
  auto pud = load_pud(p.string());
  if (!pud) FAIL("cannot load " << p << ": " << pud.error());
  return std::move(*pud);
}

bool is_wall_code(std::uint16_t code) {
  const std::uint16_t g = code & 0x0f00;
  const std::uint16_t s = code & 0x0ff0;
  return g == 0x0800 || g == 0x0900 || (g == 0 && s >= 0x0090 && s <= 0x00c0);
}

}  // namespace

TEST_CASE("Terrain: quadrants and codes are inverse of each other") {
  // A solid tile.
  const auto trees = decompose(0x0071);
  REQUIRE(trees);
  CHECK(*trees == Quad{Terrain::Trees, Terrain::Trees, Terrain::Trees, Terrain::Trees});
  CHECK(compose(*trees) == 0x0070);

  // A boundary tile: trees in the bottom-right corner of a grass tile (group 0x0700, mask 8 -> 7 << 4).
  const auto corner = decompose(0x0770);
  REQUIRE(corner);
  CHECK(*corner == Quad{Terrain::GrassLight, Terrain::GrassLight, Terrain::GrassLight, Terrain::Trees});
  CHECK(compose(*corner) == 0x0770);

  // Every one of the 14 masks of every pair survives a round trip.
  for (std::uint16_t group = 0x0100; group <= 0x0700; group += 0x0100)
    for (std::uint16_t mask = 0; mask <= 13; ++mask) {
      const std::uint16_t code = static_cast<std::uint16_t>(group | (mask << 4));
      const auto q = decompose(code);
      REQUIRE(q);
      CHECK(compose(*q) == code);
    }
}

TEST_CASE("Terrain: impossible combinations are refused") {
  CHECK_FALSE(compose({Terrain::WaterLight, Terrain::GrassLight, Terrain::GrassLight, Terrain::GrassLight}));  // no coast
  CHECK_FALSE(compose({Terrain::Trees, Terrain::GrassLight, Terrain::DirtLight, Terrain::GrassLight}));       // three terrains
  CHECK_FALSE(compose({Terrain::Trees, Terrain::DirtLight, Terrain::DirtLight, Terrain::DirtLight}));         // trees need grass
  CHECK_FALSE(decompose(0x0ff0));
  CHECK_FALSE(decompose(0x07f0));  // mask 15 would be a solid tile
}

TEST_CASE("Terrain: movement rules") {
  using T = Terrain;
  CHECK(movement_of({T::GrassLight, T::GrassLight, T::GrassLight, T::GrassLight}) == movement::kLand);
  CHECK(movement_of({T::DirtLight, T::DirtLight, T::GrassLight, T::DirtLight}) == movement::kDirt);
  CHECK(movement_of({T::GrassLight, T::GrassLight, T::GrassLight, T::Trees}) == movement::kForest);  // one tree quadrant blocks
  CHECK(movement_of({T::WaterLight, T::WaterLight, T::WaterLight, T::WaterLight}) == movement::kWater);
  CHECK(movement_of({T::WaterLight, T::DirtLight, T::DirtLight, T::DirtLight}) == movement::kCoastCorner);
  CHECK(movement_of({T::WaterLight, T::WaterLight, T::DirtLight, T::DirtLight}) == movement::kCoast);
  CHECK(movement_of({T::WaterLight, T::DirtLight, T::DirtLight, T::WaterLight}) == movement::kCoastCorner);  // checkerboard
  CHECK(land_passable(movement::kLand));
  CHECK_FALSE(land_passable(movement::kForest));
  CHECK_FALSE(land_passable(movement::kWater));
  CHECK(sailable(movement::kWater));
  CHECK_FALSE(sailable(movement::kCoast));
}

TEST_CASE("Tilesets: every era knows the tiles it needs") {
  const auto sets = tilesets();
  for (const Era era : {Era::Forest, Era::Winter, Era::Wasteland, Era::Swamp}) {
    const TileSet& ts = sets->era(era);
    INFO(ts.name());
    CHECK(ts.size() > 350);
    CHECK(fs::exists(kAssets / ts.image()));
    // Every group of tiles the autotiler can ask for has at least one drawing.
    for (const std::uint16_t group : {0x0010, 0x0020, 0x0030, 0x0040, 0x0050, 0x0060, 0x0070, 0x0080}) CHECK(ts.pick(static_cast<std::uint16_t>(group), 1));
    for (std::uint16_t g = 0x0100; g <= 0x0700; g += 0x0100)
      for (std::uint16_t mask = 0; mask <= 13; ++mask) {
        INFO("group " << g << " mask " << mask);
        CHECK(ts.pick(static_cast<std::uint16_t>(g | (mask << 4)), 1));
      }
  }
  // Plain ground never picks a decorated variation.
  const TileSet& forest = sets->era(Era::Forest);
  for (std::uint32_t seed = 0; seed < 200; ++seed) CHECK((*forest.pick(0x0050, seed) & 0xf) <= 2);
}

TEST_CASE("Maps: every tile of every sample map is understood") {
  const auto sets = tilesets();
  const auto maps = sample_maps();
  REQUIRE(maps.size() >= 5);
  for (const auto& path : maps) {
    INFO(path.filename());
    const PudMap pud = load(path);
    const TileSet& ts = sets->era(pud.era);

    std::size_t unknown = 0, not_drawable = 0, round_trip_failures = 0;
    for (const auto code : pud.tiles) {
      if (!ts.has(code)) ++not_drawable;
      if (is_wall_code(code)) continue;
      const auto q = decompose(code);
      if (!q) {
        ++unknown;
        continue;
      }
      if (compose(*q) != (code & 0xfff0)) ++round_trip_failures;
    }
    CHECK(not_drawable == 0);
    CHECK(unknown == 0);
    CHECK(round_trip_failures == 0);

    const auto map = TileMap::from_pud(pud, sets);
    CHECK(map.has_value());
  }
}

TEST_CASE("Maps: neighbouring tiles agree about the terrain at their shared corners") {
  // This is what makes the scheme work: each vertex of the tile grid has ONE terrain, seen by up to four tiles.
  const auto sets = tilesets();
  std::size_t checked = 0, disagreements = 0;
  for (const auto& path : sample_maps()) {
    const PudMap pud = load(path);
    const TileMap map = *TileMap::from_pud(pud, sets);
    std::map<std::pair<int, int>, Terrain> vertex;
    for (int y = 0; y < map.height(); ++y)
      for (int x = 0; x < map.width(); ++x) {
        if (is_wall_code(map.tile({x, y}))) continue;
        const auto q = map.quad({x, y});
        REQUIRE(q);
        const std::pair<int, Terrain> corners[] = {{0, q->tl}, {1, q->tr}, {2, q->bl}, {3, q->br}};
        for (const auto& [i, terrain] : corners) {
          const std::pair<int, int> v{x + (i & 1), y + (i >> 1)};
          const auto [it, inserted] = vertex.emplace(v, terrain);
          ++checked;
          if (!inserted && it->second != terrain) ++disagreements;
        }
      }
  }
  CHECK(checked > 10000);
  CHECK(disagreements == 0);  // in the sample maps it holds for every single corner
}

TEST_CASE("Maps: the movement rules reproduce the movement data stored in the maps") {
  const auto sets = tilesets();
  std::size_t total = 0, same = 0;
  std::map<std::pair<std::uint16_t, std::uint16_t>, int> mismatches;  // (stored, ours) -> count
  for (const auto& path : sample_maps()) {
    const PudMap pud = load(path);
    REQUIRE(pud.movement.size() == pud.tiles.size());
    for (std::size_t i = 0; i < pud.tiles.size(); ++i) {
      if (is_wall_code(pud.tiles[i])) continue;
      const auto q = decompose(pud.tiles[i]);
      REQUIRE(q);
      ++total;
      const std::uint16_t ours = movement_of(*q);
      if (ours == pud.movement[i]) {
        ++same;
      } else {
        ++mismatches[{pud.movement[i], ours}];
      }
    }
  }
  INFO("tiles checked: " << total);
  for (const auto& [pair, n] : mismatches) INFO("stored 0x" << std::hex << pair.first << " ours 0x" << pair.second << " x" << std::dec << n);
  CHECK(total > 10000);
  CHECK(same * 10000 >= total * 9995);  // all but a few hand-edited tiles
}

TEST_CASE("Maps: what land units can walk on agrees with the stored data") {
  const auto sets = tilesets();
  std::size_t total = 0, disagree = 0;
  for (const auto& path : sample_maps()) {
    const PudMap pud = load(path);
    for (std::size_t i = 0; i < pud.tiles.size(); ++i) {
      if (is_wall_code(pud.tiles[i])) continue;
      ++total;
      const bool stored = land_passable(pud.movement[i]);
      const bool ours = land_passable(movement_of(*decompose(pud.tiles[i])));
      disagree += stored != ours ? 1 : 0;
    }
  }
  CHECK(total > 10000);
  CHECK(disagree == 0);
}

TEST_CASE("The autotiler rebuilds a map from its terrain") {
  const auto sets = tilesets();
  for (const auto& path : sample_maps()) {
    INFO(path.filename());
    const PudMap pud = load(path);
    const TileMap original = *TileMap::from_pud(pud, sets);

    // Read the terrain at each vertex back out of the tiles (each vertex from the tile below-right of it where it can).
    const int w = original.width(), h = original.height();
    std::vector<Terrain> vertex(static_cast<std::size_t>((w + 1) * (h + 1)), Terrain::GrassLight);
    std::vector<bool> known(vertex.size(), false);
    bool has_walls = false;
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        if (is_wall_code(original.tile({x, y}))) {
          has_walls = true;
          continue;
        }
        const auto q = *original.quad({x, y});
        const Terrain corners[] = {q.tl, q.tr, q.bl, q.br};
        for (int i = 0; i < 4; ++i) {
          const auto idx = static_cast<std::size_t>((y + (i >> 1)) * (w + 1) + x + (i & 1));
          vertex[idx] = corners[i];
          known[idx] = true;
        }
      }
    if (has_walls) continue;  // walls are not composed by the autotiler

    const auto rebuilt = TileMap::from_terrain(sets, original.era(), w, h, [&](int vx, int vy) {
      return vertex[static_cast<std::size_t>(vy * (w + 1) + vx)];
    });
    REQUIRE(rebuilt);

    std::size_t different_group = 0;
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        const auto a = original.tile({x, y});
        const auto b = rebuilt->tile({x, y});
        if ((a & 0xfff0) != (b & 0xfff0)) ++different_group;
        CHECK(rebuilt->tileset().has(b));
      }
    // The same look everywhere (only the variation number may differ, and a few hand-edited tiles).
    CHECK(different_group * 200 < static_cast<std::size_t>(w * h));
  }
}

namespace {

/// Number of vertices where two tiles disagree about the terrain (0 for a well-formed map), and tiles whose code
/// the tileset cannot draw.
struct Consistency {
  std::size_t disagreements = 0;
  std::size_t undrawable = 0;
  std::size_t movement_wrong = 0;
};

Consistency check(const TileMap& map) {
  Consistency c;
  std::map<std::pair<int, int>, Terrain> vertex;
  for (int y = 0; y < map.height(); ++y)
    for (int x = 0; x < map.width(); ++x) {
      const Point p{x, y};
      if (!map.tileset().has(map.tile(p))) ++c.undrawable;
      if (is_wall_code(map.tile(p))) continue;
      const auto q = map.quad(p);
      if (!q) continue;
      if (map.movement(p) != movement_of(*q)) ++c.movement_wrong;
      const Terrain corners[] = {q->tl, q->tr, q->bl, q->br};
      for (int i = 0; i < 4; ++i) {
        const auto [it, inserted] = vertex.emplace(std::pair{x + (i & 1), y + (i >> 1)}, corners[i]);
        if (!inserted && it->second != corners[i]) ++c.disagreements;
      }
    }
  return c;
}

}  // namespace

TEST_CASE("Felling a tree redraws the forest edge instead of leaving a square hole") {
  const auto sets = tilesets();
  // 14x14 grass with a forest covering vertices 4..10 (so the trees are solid inside and ragged at the rim).
  const auto terrain = [](int vx, int vy) {
    return (vx >= 4 && vx <= 10 && vy >= 4 && vy <= 10) ? Terrain::Trees : Terrain::GrassLight;
  };
  auto built = TileMap::from_terrain(sets, Era::Forest, 14, 14, terrain, 1);
  REQUIRE(built);
  TileMap map = std::move(*built);
  REQUIRE(check(map).disagreements == 0);

  // The rim tiles are boundary tiles (grass/trees group 0x07xx); the middle is solid forest (0x007x).
  CHECK((map.tile({3, 3}) & 0x0f00) == 0x0700);
  CHECK((map.tile({7, 7}) & 0x0ff0) == 0x0070);
  CHECK((map.tile({1, 1}) & 0x0ff0) == 0x0050);
  const auto trees_before = map.tree_tiles().size();

  // Fell a solid tree in the middle.
  const auto changed = map.clear_trees({7, 7});
  CHECK_FALSE(changed.empty());
  CHECK(changed.size() <= 9);  // only the tile and its neighbours are touched
  for (const Point p : changed) {
    CHECK(std::abs(p.x - 7) <= 1);
    CHECK(std::abs(p.y - 7) <= 1);
  }
  // The felled tile is grass now, its neighbours became boundary tiles pointing at the new clearing.
  CHECK((map.tile({7, 7}) & 0x0ff0) == 0x0050);
  CHECK_FALSE(map.is_tree({7, 7}));
  CHECK(land_passable(map.movement({7, 7})));
  for (const Point p : {Point{6, 7}, Point{8, 7}, Point{7, 6}, Point{7, 8}, Point{6, 6}, Point{8, 8}}) {
    CHECK((map.tile(p) & 0x0f00) == 0x0700);  // ragged edge, not a solid block
    CHECK(map.is_tree(p));
  }
  CHECK(map.tree_tiles().size() == trees_before - 1);

  const auto c = check(map);
  CHECK(c.disagreements == 0);
  CHECK(c.undrawable == 0);
  CHECK(c.movement_wrong == 0);

  // Felling something that is not a tree changes nothing.
  CHECK(map.clear_trees({1, 1}).empty());
  CHECK(map.clear_trees({7, 7}).empty());
}

TEST_CASE("Cutting down a whole forest keeps the map consistent at every step") {
  const auto sets = tilesets();
  const auto terrain = [](int vx, int vy) {
    return ((vx - 8) * (vx - 8) + (vy - 8) * (vy - 8) <= 30) ? Terrain::Trees : Terrain::GrassLight;
  };
  auto built = TileMap::from_terrain(sets, Era::Forest, 16, 16, terrain, 5);
  REQUIRE(built);
  TileMap map = std::move(*built);
  REQUIRE_FALSE(map.tree_tiles().empty());

  int fells = 0;
  for (;;) {
    const auto trees = map.tree_tiles();
    if (trees.empty()) break;
    const auto before = trees.size();
    map.clear_trees(trees.front());
    ++fells;
    REQUIRE(map.tree_tiles().size() < before);  // every felling makes progress
    const auto c = check(map);
    REQUIRE(c.disagreements == 0);
    REQUIRE(c.undrawable == 0);
    REQUIRE(c.movement_wrong == 0);
    REQUIRE(fells < 1000);
  }
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 16; ++x) CHECK((map.tile({x, y}) & 0x0ff0) == 0x0050);  // nothing but grass left
}

TEST_CASE("Felling every tree of the real maps keeps them well-formed") {
  const auto sets = tilesets();
  for (const char* name : {"Cramped BNE.pud", "Gold Rush BNE.pud", "Skirmish BNE.pud"}) {
    INFO(name);
    TileMap map = *TileMap::from_pud(load(kAssets / "maps" / name), sets);
    const auto start = map.tree_tiles().size();
    REQUIRE(start > 100);

    std::size_t felled = 0;
    for (const Point p : map.tree_tiles()) {
      if (!map.is_tree(p)) continue;  // an earlier felling already cleared it
      map.clear_trees(p);
      ++felled;
    }
    CHECK(felled > 0);
    CHECK(map.tree_tiles().empty());
    const auto c = check(map);
    CHECK(c.disagreements == 0);
    CHECK(c.undrawable == 0);
    CHECK(c.movement_wrong == 0);
  }
}

// ---- the World over real terrain ---------------------------------------------------------------

namespace {

constexpr const char* kUnits = R"({"units":[
  {"id":"peon","name":"Peon","hp":30,"move_speed":96,"sprite":"s","commands":["move"],
   "worker":{"lumber_per_trip":10,"gold_per_trip":100,"chop_seconds":0.5,"mine_seconds":1.0},"builds":["hall"]}]})";
constexpr const char* kBuildings = R"({"buildings":[
  {"id":"hall","name":"Hall","size":4,"hp":100,"food":50,"cost":{"gold":1},"build_seconds":10,"is_depot":true,"sprite":"h"}]})";

const Registry& registry() {
  static const Registry r = [] {
    auto parsed = Registry::parse(kUnits, kBuildings, R"({"commands":[{"id":"move"}]})");
    if (!parsed) FAIL(parsed.error());
    return std::move(*parsed);
  }();
  return r;
}

}  // namespace

TEST_CASE("A world over a real map blocks what the map blocks and holds wood in every tree tile") {
  const auto sets = tilesets();
  const TileMap map = *TileMap::from_pud(load(kAssets / "maps" / "Cramped BNE.pud"), sets);
  const auto trees = map.tree_tiles();
  REQUIRE(trees.size() > 300);

  World world{WorldConfig{}, map};
  REQUIRE(world.terrain() != nullptr);
  CHECK(world.width() == 32);
  CHECK(world.height() == 32);
  CHECK(world.trees().positions().size() == trees.size());
  for (const Point p : trees) {
    CHECK(world.trees().has_tree(p));
    CHECK(world.grid(MoveClass::Land).is_blocked(p));
  }
  CHECK(world.grid(MoveClass::Land).is_walkable({8, 29}));  // the open ground of a start area
  CHECK(world.grid(MoveClass::Land).is_blocked({0, 0}) == !land_passable(map.movement({0, 0})));
}

TEST_CASE("A peon cutting a real forest leaves a clearing with proper edges") {
  const auto sets = tilesets();
  World world{WorldConfig{}, *TileMap::from_pud(load(kAssets / "maps" / "Cramped BNE.pud"), sets)};
  const PlayerId me = world.add_player("me", "orcs", {});
  const BuildingDef& hall = *registry().building("hall");
  const UnitDef& peon_def = *registry().unit("peon");
  world.add_building(hall, me, {7, 28}, true);

  int felled = 0, changed_events = 0;
  std::vector<Point> redrawn;
  world.events().subscribe([&](const Event& e) {
    on_event<ev::TreeFelled>(e, [&](const ev::TreeFelled&) { ++felled; });
    on_event<ev::TerrainChanged>(e, [&](const ev::TerrainChanged& c) {
      ++changed_events;
      redrawn.insert(redrawn.end(), c.tiles.begin(), c.tiles.end());
    });
  });

  const Point tree{4, 26};
  REQUIRE(world.terrain()->is_tree(tree));
  const std::uint16_t code_before = world.terrain()->tile(tree);
  const EntityId peon = world.spawn_unit(peon_def, me, {6, 27});
  REQUIRE(peon.valid());
  REQUIRE(world.order_harvest(std::array{peon}, tree).ok);

  for (int i = 0; i < 4000 && felled == 0; ++i) world.update(0.05);  // up to 200 s

  REQUIRE(felled >= 1);
  CHECK(changed_events >= 1);
  CHECK_FALSE(redrawn.empty());
  CHECK(world.player(me).stock.lumber == 90);  // a tree holds 100; the last 10 are still in the peon's hands
  CHECK_FALSE(world.trees().has_tree(tree));
  CHECK_FALSE(world.terrain()->is_tree(tree));
  CHECK(world.terrain()->tile(tree) != code_before);
  CHECK(world.grid(MoveClass::Land).is_walkable(tree));

  // Every redrawn tile is one that touches the felled one, and the terrain is still well-formed.
  for (const Point p : redrawn) {
    CHECK(std::abs(p.x - tree.x) <= 1);
    CHECK(std::abs(p.y - tree.y) <= 1);
  }
  const auto c = check(*world.terrain());
  CHECK(c.disagreements == 0);
  CHECK(c.undrawable == 0);
  CHECK(c.movement_wrong == 0);

  // The collision grid still matches the terrain everywhere (no building or mine overlaps trees here).
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 32; ++x) {
      const Point p{x, y};
      if (world.building_at(p).valid()) continue;
      CHECK(world.grid(MoveClass::Land).is_blocked(p) == !land_passable(world.terrain()->movement(p)));
      CHECK(world.trees().has_tree(p) == world.terrain()->is_tree(p));
    }
}

TEST_CASE("A map's scenario: who starts where, and what gold there is") {
  const Scenario cramped = scenario_from(load(kAssets / "maps" / "Cramped BNE.pud"));
  REQUIRE(cramped.starts.size() == 2);
  CHECK(cramped.starts[0].owner == 0);
  CHECK(cramped.starts[0].start == Point{7, 28});
  CHECK(cramped.starts[0].hall == Point{6, 26});  // the map placed the town hall
  CHECK(cramped.starts[1].start == Point{24, 3});
  CHECK(cramped.starts[1].hall == Point{22, 2});
  REQUIRE(cramped.mines.size() == 4);
  CHECK(cramped.mines[0].gold % 2500 == 0);
  CHECK(cramped.mines[0].gold == 14 * 2500);

  CHECK(cramped.farthest_from(0) == 1);
  CHECK(cramped.farthest_from(1) == 0);
  CHECK_FALSE(cramped.farthest_from(5).has_value());

  // On every sample map the start slots are inside the map, and a four-player map picks a far away opponent.
  for (const auto& path : sample_maps()) {
    INFO(path.filename());
    const PudMap pud = load(path);
    const Scenario s = scenario_from(pud);
    for (const auto& slot : s.starts) {
      CHECK(slot.start.x >= 0);
      CHECK(slot.start.y >= 0);
      CHECK(slot.start.x < pud.width);
      CHECK(slot.start.y < pud.height);
    }
  }
  const Scenario gold_rush = scenario_from(load(kAssets / "maps" / "Gold Rush BNE.pud"));
  REQUIRE(gold_rush.starts.size() == 4);
  CHECK(gold_rush.mines.size() > 20);
  const auto foe = gold_rush.farthest_from(0);
  REQUIRE(foe);
  CHECK(foe != 0);
}
