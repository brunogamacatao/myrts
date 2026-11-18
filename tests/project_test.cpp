#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iterator>

#include "core/package_check.hpp"
#include "editor_model/project.hpp"

using namespace myrts;
namespace fs = std::filesystem;

namespace {
const fs::path kGame = fs::path(MYRTS_SOURCE_DIR) / "games" / "warcraft-like";

struct TempDir {
  fs::path path;
  explicit TempDir(const std::string& name) : path(fs::temp_directory_path() / ("myrts_editor_" + name)) {
    fs::remove_all(path);
  }
  ~TempDir() { std::error_code ec; fs::remove_all(path, ec); }
};

std::string read_all(const fs::path& file) {
  std::ifstream in(file);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}
}  // namespace

TEST_CASE("check_package reports a good package clean and names what is wrong in a broken one") {
  const PackageReport good = check_package(kGame);
  CHECK(good.ok());
  CHECK_FALSE(good.info.empty());
  const PackageReport none = check_package(fs::temp_directory_path() / "myrts_no_such_game");
  CHECK_FALSE(none.ok());
}

TEST_CASE("a project holds the game's JSON documents, and saving without edits writes nothing") {
  auto project = editor::Project::open(kGame);
  REQUIRE(project.has_value());
  CHECK(project->report().ok());
  CHECK_FALSE(project->dirty());
  const auto names = project->document_names();
  for (const char* expected : {"game.json", "data/units.json", "data/buildings.json", "data/campaign.json", "data/resources.json", "strings/pt.json"})
    CHECK(std::ranges::find(names, expected) != names.end());
  CHECK(project->document("data/units.json")->contains("units"));
  CHECK(project->document("nope.json") == nullptr);
  CHECK(project->edit("nope.json") == nullptr);
}

TEST_CASE("saving a copy of every document gives back the same JSON, with keys in the same order") {
  TempDir tmp("roundtrip");
  auto project = editor::Project::open(kGame);
  REQUIRE(project.has_value());
  REQUIRE(project->save_copy(tmp.path).has_value());
  for (const std::string& name : project->document_names()) {
    INFO(name);
    CHECK(nlohmann::ordered_json::parse(read_all(tmp.path / name)) == *project->document(name));
  }
}

TEST_CASE("an edit marks the document changed; save writes it and the package reloads with the change") {
  TempDir tmp("edit");
  auto project = editor::Project::create_from(kGame, tmp.path, "My Test Game");
  REQUIRE(project.has_value());
  CHECK(project->title() == "My Test Game");
  CHECK(project->package().id == "my-test-game");
  CHECK_FALSE(project->dirty());

  auto* units = project->edit("data/units.json");
  REQUIRE(units != nullptr);
  CHECK(project->dirty("data/units.json"));
  CHECK(project->dirty());
  (*units)["units"][0]["hp"] = 77;
  REQUIRE(project->save().has_value());
  CHECK_FALSE(project->dirty());

  auto reopened = editor::Project::open(tmp.path);
  REQUIRE(reopened.has_value());
  CHECK((*reopened->document("data/units.json"))["units"][0]["hp"] == 77);
  CHECK(reopened->title() == "My Test Game");
  CHECK(reopened->report().ok());
}

TEST_CASE("an edit that breaks the game shows up as a problem after revalidating") {
  TempDir tmp("broken");
  auto project = editor::Project::create_from(kGame, tmp.path, "Broken");
  REQUIRE(project.has_value());
  (*project->edit("data/units.json"))["units"][0]["sprite"] = "sprites/does_not_exist.json";
  REQUIRE(project->save().has_value());
  project->revalidate();
  CHECK_FALSE(project->report().ok());
}

TEST_CASE("a new project needs a template and a free folder") {
  TempDir tmp("exists");
  fs::create_directories(tmp.path);
  CHECK_FALSE(editor::Project::create_from(kGame, tmp.path, "x").has_value());
  CHECK_FALSE(editor::Project::create_from(fs::temp_directory_path() / "myrts_no_template", tmp.path / "sub", "x").has_value());
}

TEST_CASE("discarding changes reads the documents from disk again") {
  TempDir tmp("discard");
  auto project = editor::Project::create_from(kGame, tmp.path, "Discard");
  REQUIRE(project.has_value());
  const auto before = (*project->document("data/units.json"))["units"][0]["hp"];
  (*project->edit("data/units.json"))["units"][0]["hp"] = 12345;
  project->discard_changes();
  CHECK_FALSE(project->dirty());
  CHECK((*project->document("data/units.json"))["units"][0]["hp"] == before);
}
