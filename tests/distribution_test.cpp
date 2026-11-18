#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "core/campaign.hpp"
#include "core/match_setup.hpp"
#include "core/package_check.hpp"
#include "editor_model/export.hpp"
#include "editor_model/json_schema.hpp"

using namespace myrts;
using namespace myrts::editor;
namespace fs = std::filesystem;

namespace {
const fs::path kGame = fs::path(MYRTS_SOURCE_DIR) / "games" / "warcraft-like";

struct Temp {
  fs::path path;
  explicit Temp(const std::string& name) : path(fs::temp_directory_path() / ("myrts_dist_" + name)) { fs::remove_all(path); }
  ~Temp() { std::error_code ec; fs::remove_all(path, ec); }
};
}  // namespace

TEST_CASE("an exported game is a folder with the program named after the game and the package beside it") {
  Temp tmp("export");
  auto project = Project::create_from(kGame, tmp.path / "src", "My Export");
  REQUIRE(project.has_value());
  const fs::path program = tmp.path / "myrts";
  std::ofstream(program) << "#!/bin/sh\n";

  const auto folder = export_game(*project, program, tmp.path / "out");
  REQUIRE(folder.has_value());
  CHECK(*folder == tmp.path / "out" / "my-export");
  CHECK(fs::exists(*folder / "my-export"));
  CHECK(fs::exists(*folder / "game" / "game.json"));
  CHECK(fs::exists(*folder / "game" / "data" / "units.json"));
  CHECK(fs::exists(*folder / "game" / "sprites" / "orcs_grunt.png"));
  CHECK(Project::open(*folder / "game").has_value());

  // Exporting again replaces the folder.
  std::ofstream(*folder / "stale.txt") << "x";
  REQUIRE(export_game(*project, program, tmp.path / "out").has_value());
  CHECK_FALSE(fs::exists(*folder / "stale.txt"));
}

TEST_CASE("export refuses unsaved changes, a game with problems and a missing program") {
  Temp tmp("refuse");
  auto project = Project::create_from(kGame, tmp.path / "src", "Refuse");
  REQUIRE(project.has_value());
  const fs::path program = tmp.path / "myrts";
  std::ofstream(program) << "x";

  CHECK_FALSE(export_game(*project, tmp.path / "missing", tmp.path / "out").has_value());
  project->set("data/units.json", Project::Json::json_pointer("/units/0/hp"), 5);
  CHECK_FALSE(export_game(*project, program, tmp.path / "out").has_value());  // not saved
  REQUIRE(project->save().has_value());
  project->set("data/units.json", Project::Json::json_pointer("/units/0/sprite"), "sprites/nope.json");
  REQUIRE(project->save().has_value());
  const auto refused = export_game(*project, program, tmp.path / "out");
  REQUIRE_FALSE(refused.has_value());
  CHECK(refused.error().contains("problems"));
  CHECK_FALSE(fs::exists(tmp.path / "out"));
}

TEST_CASE("the JSON Schemas describe the editor's fields, and the shipped data fits them") {
  const auto schemas = all_json_schemas();
  CHECK(schemas.size() >= 10);
  for (const auto& [name, schema] : schemas) {
    INFO(name);
    CHECK(schema["type"] == "object");
    CHECK(schema.contains("properties"));
  }
  const auto unit = json_schema(unit_schema(), "unit", true);
  CHECK(unit["properties"]["hp"]["type"] == "integer");
  CHECK(unit["properties"]["hp"]["minimum"] == 1);
  CHECK(unit["properties"]["move_class"]["enum"] == nlohmann::json::array({"land", "water", "air"}));
  CHECK(unit["properties"]["stats"]["properties"]["range"]["type"] == "integer");
  CHECK(unit["required"] == nlohmann::json::array({"id"}));
  const auto level = json_schema(level_schema(), "level");
  CHECK(level["properties"]["objectives"]["properties"]["victory"]["type"] == "array");

  // Every key of every unit and building of the shipped game is in the schema (nothing is missing from the editor).
  auto project = Project::open(kGame);
  REQUIRE(project.has_value());
  const auto check_keys = [&](const char* file, const char* array, const nlohmann::json& schema) {
    for (const auto& item : (*project->document(file))[array])
      for (const auto& [key, value] : item.items()) {
        INFO(file << " " << item["id"] << " has the key " << key);
        CHECK(schema["properties"].contains(key));
      }
  };
  check_keys("data/units.json", "units", unit);
  check_keys("data/buildings.json", "buildings", json_schema(building_schema(), "building", true));
  check_keys("data/resources.json", "resources", json_schema(resource_schema(), "resource", true));
  check_keys("data/projectiles.json", "projectiles", json_schema(projectile_schema(), "projectile", true));
  check_keys("data/commands.json", "commands", json_schema(command_schema(), "command", true));
  const auto level_schema_json = json_schema(level_schema(), "level");
  for (const auto& l : (*project->document("data/campaign.json"))["levels"])
    for (const auto& [key, value] : l.items()) CHECK(level_schema_json["properties"].contains(key));
}

TEST_CASE("the minimal template is a valid game that starts a match and runs") {
  const fs::path templ = fs::path(MYRTS_SOURCE_DIR) / "templates" / "minimal-game";
  const PackageReport report = check_package(templ);
  for (const auto& p : report.problems) INFO(p);
  CHECK(report.ok());

  const auto registry = Registry::load_directory((templ / "data").string());
  const auto campaign = Campaign::load_file((templ / "data" / "campaign.json").string());
  const auto sets = TileSets::load_file((templ / "data" / "tilesets.json").string());
  REQUIRE(registry.has_value());
  REQUIRE(campaign.has_value());
  REQUIRE(sets.has_value());
  MatchOptions options;
  options.assets_dir = templ.string();
  auto match = build_match(*registry, std::make_shared<const TileSets>(*sets), with_default_sides(campaign->level(0), *registry), options);
  REQUIRE(match.has_value());
  for (int i = 0; i < 600; ++i) match->world->update(0.1);  // a minute of game time passes without trouble
  CHECK(match->world->elapsed() > 59.0);
}

TEST_CASE("a game started from the template can be edited, saved and exported") {
  Temp tmp("template");
  const fs::path templ = fs::path(MYRTS_SOURCE_DIR) / "templates" / "minimal-game";
  auto project = Project::create_from(templ, tmp.path / "mine", "My Own Game");
  REQUIRE(project.has_value());
  CHECK(project->package().id == "my-own-game");
  CHECK(project->report().ok());
  const fs::path program = tmp.path / "myrts";
  std::ofstream(program) << "x";
  const auto folder = export_game(*project, program, tmp.path / "out");
  REQUIRE(folder.has_value());
  CHECK(fs::exists(*folder / "game" / "maps" / "field.mrmap"));
}
