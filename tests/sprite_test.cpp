#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "core/sprite_doc.hpp"
#include "editor_model/sprite.hpp"

using namespace myrts;
using namespace myrts::editor;
namespace fs = std::filesystem;

namespace {
const fs::path kGame = fs::path(MYRTS_SOURCE_DIR) / "games" / "warcraft-like";
const std::string kGrunt = "sprites/orcs_grunt.json";

struct TempGame {
  fs::path path;
  Project project;
  explicit TempGame(const std::string& name) : path(fs::temp_directory_path() / ("myrts_sprite_" + name)), project(make(path)) {}
  ~TempGame() { std::error_code ec; fs::remove_all(path, ec); }
  static Project make(const fs::path& dir) {
    fs::remove_all(dir);
    auto p = Project::create_from(kGame, dir, "Sprites");
    REQUIRE(p.has_value());
    return std::move(*p);
  }
};
}  // namespace

TEST_CASE("every sprite definition of the shipped game is sound") {
  TempGame g("shipped");
  const auto files = sprite::list(g.project);
  CHECK(files.size() >= 20);
  for (const std::string& file : files) {
    INFO(file);
    REQUIRE(g.project.open_document(file));
    CHECK(sprite_problem(*g.project.document(file)).empty());
  }
}

TEST_CASE("sprite_problem names what is wrong") {
  using J = nlohmann::json;
  const J good = {{"spritesheet", "a.png"}, {"default_animation", "idle"},
                  {"animations", J::array({J{{"name", "idle"}, {"fps", 5}, {"frames", J::array({J{{"x", 0}, {"y", 0}, {"w", 8}, {"h", 8}}})}}})}};
  CHECK(sprite_problem(good).empty());
  J bad = good;
  bad["animations"][0]["fps"] = 0;
  CHECK(sprite_problem(bad).contains("fps"));
  bad = good;
  bad["default_animation"] = "nope";
  CHECK(sprite_problem(bad).contains("default_animation"));
  bad = good;
  bad["animations"][0]["frames"] = J::array();
  CHECK(sprite_problem(bad).contains("no frames"));
  bad = good;
  bad["animations"][0]["frames"][0]["w"] = 0;
  CHECK(sprite_problem(bad).contains("above 0"));
  bad = good;
  bad["animations"].push_back(bad["animations"][0]);
  CHECK(sprite_problem(bad).contains("two animations"));
  CHECK_FALSE(sprite_problem(J::array()).empty());
}

TEST_CASE("animations and frames can be added, moved, renamed and removed, and undone") {
  TempGame g("ops");
  Project& p = g.project;
  REQUIRE(p.open_document(kGrunt));
  const std::size_t n = sprite::animation_count(p, kGrunt);
  const std::string first = sprite::animation_name(p, kGrunt, 0);

  const std::size_t added = sprite::add_animation(p, kGrunt, "idle_up");  // that name exists: made unique
  CHECK(added == n);
  CHECK(sprite::animation_name(p, kGrunt, added) == "idle_up_2");
  CHECK(sprite::frames(p, kGrunt, added).size() == 1);
  CHECK(sprite::add_frame(p, kGrunt, added, {10, 20, 30, 40}) == 1);
  CHECK(sprite::frames(p, kGrunt, added)[1] == Rect{10, 20, 30, 40});
  CHECK(sprite::move_frame(p, kGrunt, added, 1, 0));
  CHECK(sprite::frames(p, kGrunt, added)[0] == Rect{10, 20, 30, 40});
  CHECK(sprite::remove_frame(p, kGrunt, added, 0));
  CHECK(sprite::frames(p, kGrunt, added).size() == 1);
  CHECK_FALSE(sprite::remove_frame(p, kGrunt, added, 5));
  CHECK(sprite_problem(*p.document(kGrunt)).empty());

  CHECK(sprite::duplicate_animation(p, kGrunt, added) == added + 1);
  CHECK(sprite::animation_name(p, kGrunt, added + 1) == "idle_up_2_copy");
  CHECK(sprite::remove_animation(p, kGrunt, added + 1));

  while (p.can_undo()) p.undo();
  CHECK(sprite::animation_count(p, kGrunt) == n);
  CHECK_FALSE(p.dirty());
}

TEST_CASE("renaming an animation keeps the default and the states pointing at it; removing the default picks another") {
  TempGame g("rename");
  Project& p = g.project;
  REQUIRE(p.open_document(kGrunt));
  p.set(kGrunt, Project::Json::json_pointer("/states"), Project::Json{{"attack", Project::Json::array({"attack_{dir}", "walk_up"})}});
  const std::string def = (*p.document(kGrunt))["default_animation"];
  std::size_t index = 0;
  for (std::size_t i = 0; i < sprite::animation_count(p, kGrunt); ++i)
    if (sprite::animation_name(p, kGrunt, i) == def) index = i;

  CHECK_FALSE(sprite::rename_animation(p, kGrunt, index, "").has_value());
  CHECK_FALSE(sprite::rename_animation(p, kGrunt, index, "walk_up").has_value());  // taken
  REQUIRE(sprite::rename_animation(p, kGrunt, index, "standing").has_value());
  CHECK((*p.document(kGrunt))["default_animation"] == "standing");
  REQUIRE(sprite::rename_animation(p, kGrunt, sprite::animation_count(p, kGrunt) - 1, "x").has_value());
  CHECK(sprite_problem(*p.document(kGrunt)).empty());
  for (std::size_t i = 0; i < sprite::animation_count(p, kGrunt); ++i)
    if (sprite::animation_name(p, kGrunt, i) == "walk_up") REQUIRE(sprite::rename_animation(p, kGrunt, i, "walking").has_value());
  CHECK((*p.document(kGrunt))["states"]["attack"][1] == "walking");

  const std::size_t standing = index;
  CHECK(sprite::remove_animation(p, kGrunt, standing));
  CHECK((*p.document(kGrunt))["default_animation"] != "standing");
  CHECK(sprite_problem(*p.document(kGrunt)).empty());
}

TEST_CASE("grid_cells lists the cells of a grid in reading order") {
  const auto cells = sprite::grid_cells(10, 20, 32, 16, 3, 2, 1);
  REQUIRE(cells.size() == 6);
  CHECK(cells[0] == Rect{10, 20, 32, 16});
  CHECK(cells[2] == Rect{10 + 2 * 33, 20, 32, 16});
  CHECK(cells[3] == Rect{10, 20 + 17, 32, 16});
  CHECK(sprite::grid_cells(0, 0, 8, 8, 0, 3).empty());

  TempGame g("grid");
  REQUIRE(g.project.open_document(kGrunt));
  CHECK(sprite::set_frames(g.project, kGrunt, 0, cells));
  CHECK(sprite::frames(g.project, kGrunt, 0).size() == 6);
}

TEST_CASE("a new sprite imports its picture and starts a valid definition that saves, reloads and is checked by the package") {
  TempGame g("create");
  Project& p = g.project;
  const fs::path picture = g.path / "sprites" / "orcs_grunt.png";  // any existing picture stands in for the artist's
  const fs::path outside = fs::temp_directory_path() / "myrts_sprite_outside.png";
  fs::copy_file(picture, outside, fs::copy_options::overwrite_existing);

  CHECK_FALSE(sprite::create(p, outside, "", 10, 10).has_value());
  CHECK_FALSE(sprite::create(p, outside, "bad name", 10, 10).has_value());
  CHECK_FALSE(sprite::create(p, outside, "orcs_grunt", 10, 10).has_value());  // taken
  const auto made = sprite::create(p, outside, "hero", 38, 43);
  REQUIRE(made.has_value());
  CHECK(*made == "sprites/hero.json");
  CHECK(fs::exists(g.path / "sprites" / "hero.png"));
  CHECK(p.dirty());
  CHECK(sprite_problem(*p.document(*made)).empty());
  REQUIRE(p.save().has_value());
  CHECK(fs::exists(g.path / "sprites" / "hero.json"));
  const auto list = sprite::list(p);
  CHECK(std::ranges::find(list, "sprites/hero.json") != list.end());

  // A unit using it passes the package check; breaking the sprite is reported.
  const std::string units = "data/units.json";
  p.set(units, Project::Json::json_pointer("/units/0/sprite"), "sprites/hero.json");
  REQUIRE(p.save().has_value());
  p.revalidate();
  CHECK(p.report().ok());
  REQUIRE(p.open_document(*made));
  p.set(*made, Project::Json::json_pointer("/default_animation"), "nope");
  REQUIRE(p.save().has_value());
  p.revalidate();
  CHECK_FALSE(p.report().ok());
  std::error_code ec;
  fs::remove(outside, ec);
}
