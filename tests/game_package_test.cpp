#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

#include "core/game_package.hpp"

using namespace myrts;
namespace fs = std::filesystem;

TEST_CASE("the shipped game package loads and its files exist", "[package]") {
  const fs::path root = fs::path(MYRTS_SOURCE_DIR) / "games" / "warcraft-like";
  const auto pkg = GamePackage::load(root);
  REQUIRE(pkg);
  CHECK(pkg->id == "warcraft-like");
  for (const auto& file : {pkg->campaign_file, pkg->tilesets_file, pkg->sound_table_file, pkg->credits_file,
                           pkg->fallback_map, pkg->music_intro, pkg->music_menu, pkg->music_victory, pkg->music_defeat})
    CHECK(fs::exists(pkg->path(file)));
  CHECK(fs::is_directory(pkg->path(pkg->data_dir)));
  CHECK(fs::is_directory(pkg->path(pkg->sounds_dir)));
  CHECK(fs::is_directory(pkg->path(pkg->cursors_dir)));
}

TEST_CASE("a missing or newer manifest is reported", "[package]") {
  const auto missing = GamePackage::load(fs::path(MYRTS_SOURCE_DIR) / "does-not-exist");
  REQUIRE_FALSE(missing);
  CHECK(missing.error().find("game.json") != std::string::npos);

  const fs::path dir = fs::temp_directory_path() / "myrts_pkg_test";
  fs::create_directories(dir);
  std::ofstream(dir / "game.json") << R"({"format_version": 99})";
  CHECK_FALSE(GamePackage::load(dir));
  std::ofstream(dir / "game.json") << R"({"title": 3})";
  const auto bad = GamePackage::load(dir);
  REQUIRE_FALSE(bad);
  CHECK(bad.error().find("title") != std::string::npos);
  fs::remove_all(dir);
}

TEST_CASE("without a manifest the defaults apply and paths resolve against the root", "[package]") {
  const auto pkg = GamePackage::defaults("/some/game");
  CHECK(pkg.path("data/units.json") == "/some/game/data/units.json");
  CHECK(pkg.path("").empty());
}
