#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>

#include "core/defs.hpp"
#include "editor_model/collection.hpp"
#include "editor_model/schema.hpp"

using namespace myrts;
using namespace myrts::editor;
namespace fs = std::filesystem;

namespace {
const fs::path kGame = fs::path(MYRTS_SOURCE_DIR) / "games" / "warcraft-like";

struct TempGame {
  fs::path path;
  Project project;
  explicit TempGame(const std::string& name) : path(fs::temp_directory_path() / ("myrts_coll_" + name)), project(make(path)) {}
  ~TempGame() { std::error_code ec; fs::remove_all(path, ec); }
  static Project make(const fs::path& dir) {
    fs::remove_all(dir);
    auto p = Project::create_from(kGame, dir, "Collections");
    REQUIRE(p.has_value());
    return std::move(*p);
  }
};
}  // namespace

TEST_CASE("units and buildings are found by id; new ids are made unique") {
  TempGame g("find");
  const Collection units = units_of(g.project);
  CHECK(collection::count(g.project, units) == 23);
  CHECK(collection::find(g.project, units, "orc_grunt").has_value());
  CHECK_FALSE(collection::find(g.project, units, "nope").has_value());
  CHECK(collection::unique_id(g.project, units, "fresh") == "fresh");
  CHECK(collection::unique_id(g.project, units, "orc_grunt") == "orc_grunt_2");
}

TEST_CASE("a character can be added, duplicated and deleted, each step undoable, and the game still loads") {
  TempGame g("crud");
  Project& p = g.project;
  const Collection units = units_of(p);
  const std::size_t n = collection::count(p, units);
  const std::size_t grunt = *collection::find(p, units, "orc_grunt");

  const std::size_t copy = collection::duplicate(p, units, grunt);
  CHECK(copy == grunt + 1);
  CHECK(collection::id(p, units, copy) == "orc_grunt_copy");
  CHECK(collection::item(p, units, copy)->at("name") == "Grunt (copy)");
  (void)p.set(units.doc, Project::Json::json_pointer("/units/" + std::to_string(copy) + "/hp"), 99);
  p.seal();
  CHECK(collection::remove(p, units, copy));
  CHECK(collection::count(p, units) == n);
  REQUIRE(p.save().has_value());
  CHECK(Registry::load_directory((g.path / "data").string()).has_value());

  while (p.can_undo()) p.undo();
  CHECK(collection::count(p, units) == n);
  CHECK_FALSE(collection::find(p, units, "orc_grunt_copy").has_value());
}

TEST_CASE("references list every use of an id, not its own definition") {
  TempGame g("refs");
  const auto refs = collection::references(g.project, "orc_grunt");
  CHECK_FALSE(refs.empty());
  const bool in_buildings = std::ranges::any_of(refs, [](const std::string& r) { return r.starts_with("data/buildings.json"); });
  CHECK(in_buildings);  // the Barracks trains it
  const bool in_campaign = std::ranges::any_of(refs, [](const std::string& r) { return r.starts_with("data/campaign.json"); });
  CHECK(in_campaign);   // a level allows it
  CHECK(std::ranges::none_of(refs, [](const std::string& r) { return r.ends_with("/id"); }));
  CHECK(collection::references(g.project, "no_such_thing").empty());
}

TEST_CASE("renaming an id updates every reference in one undoable step, and the game loads with the new id") {
  TempGame g("rename");
  Project& p = g.project;
  const Collection units = units_of(p);
  const std::size_t i = *collection::find(p, units, "orc_grunt");
  const auto before = collection::references(p, "orc_grunt").size();

  CHECK_FALSE(collection::rename_id(p, units, i, "").has_value());
  CHECK_FALSE(collection::rename_id(p, units, i, "has space").has_value());
  CHECK_FALSE(collection::rename_id(p, units, i, "orc_peon").has_value());  // taken
  CHECK_FALSE(p.can_undo());

  REQUIRE(collection::rename_id(p, units, i, "orc_warrior").has_value());
  CHECK(collection::id(p, units, i) == "orc_warrior");
  CHECK(collection::references(p, "orc_grunt").empty());
  CHECK(collection::references(p, "orc_warrior").size() == before);
  REQUIRE(p.save().has_value());
  p.revalidate();
  CHECK(p.report().ok());  // sound table, factions, campaign and buildings all follow the new id

  CHECK(p.undo());
  CHECK(collection::id(p, units, i) == "orc_grunt");
  CHECK(collection::references(p, "orc_warrior").empty());
  CHECK(collection::rename_id(p, units, i, "orc_grunt").has_value());  // same id: nothing to do
}

TEST_CASE("the unit and building schemas name only keys the game reads, and a thing made from them loads") {
  TempGame g("schema");
  Project& p = g.project;
  // Every field with a fixed list of options offers only values the game accepts.
  for (const Field& f : unit_schema())
    if (f.key == "move_class") CHECK(f.options == std::vector<std::string>{"land", "water", "air"});
  for (const Field& f : building_schema())
    if (f.key == "placement") CHECK(f.options == std::vector<std::string>{"land", "coast", "water"});

  const Collection units = units_of(p), buildings = buildings_of(p);
  collection::add(p, units, {{"id", "test_unit"}, {"name", "Test"}, {"faction", "orcs"}, {"hp", 10}, {"sprite", "sprites/orcs_grunt.json"}});
  collection::add(p, buildings, {{"id", "test_hut"}, {"name", "Hut"}, {"faction", "orcs"}, {"size", 2}, {"build_seconds", 10}, {"sprite", "buildings/barracks.png"}});
  p.set(buildings.doc, Project::Json::json_pointer("/buildings/" + std::to_string(*collection::find(p, buildings, "test_hut")) + "/trains"), Project::Json::array({"test_unit"}));
  REQUIRE(p.save().has_value());
  const auto registry = Registry::load_directory((g.path / "data").string());
  REQUIRE(registry.has_value());
  CHECK(registry->unit("test_unit") != nullptr);
  CHECK(registry->building("test_hut")->trains == std::vector<std::string>{"test_unit"});

  CHECK(reference_options(p, "projectiles").size() == 7);
  CHECK(reference_options(p, "commands").size() >= 11);
}
