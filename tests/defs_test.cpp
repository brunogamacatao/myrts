#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>

#include "core/defs.hpp"

using namespace myrts;

namespace {
constexpr const char* kUnits = R"({"units":[
  {"id":"peon","name":"Peon","hp":30,"sprite":"s.json","commands":["move"],
   "worker":{"gold_per_trip":50},"builds":["hall"],"cost":{"gold":400}},
  {"id":"dragon","name":"Dragon","hp":120,"sprite":"d.json","move_class":"air","move_speed":120}]})";
constexpr const char* kBuildings = R"({"buildings":[
  {"id":"hall","name":"Hall","size":4,"cost":{"gold":1200,"lumber":800},"build_seconds":255,
   "is_depot":true,"sprite":"h.png","sprite_construction":"hc.png","trains":["peon"]}]})";
constexpr const char* kCommands = R"({"commands":[
  {"id":"move","label":"Move","hotkey":"m","targeting":"ground"}]})";
}  // namespace

TEST_CASE("Registry parses units, buildings and commands") {
  const auto r = Registry::parse(kUnits, kBuildings, kCommands);
  REQUIRE(r);

  const UnitDef* peon = r->unit("peon");
  REQUIRE(peon);
  CHECK(peon->max_hp == 30);
  CHECK(peon->move_class == MoveClass::Land);  // default
  CHECK(peon->cost == Cost{400, 0, 0});
  REQUIRE(peon->worker);
  CHECK(peon->worker->gold_per_trip == 50);
  CHECK(peon->worker->lumber_per_trip == 10);  // default kept
  CHECK(r->unit("dragon")->move_class == MoveClass::Air);

  const BuildingDef* hall = r->building("hall");
  REQUIRE(hall);
  CHECK(hall->cost == Cost{1200, 800, 0});
  CHECK(hall->build_seconds == 255.0);
  CHECK(hall->is_depot);
  CHECK(hall->sprite_construction == "hc.png");

  const CommandDef* move = r->command("move");
  REQUIRE(move);
  CHECK(move->hotkey == 'M');  // normalised to upper case
  CHECK(move->targeting == Targeting::Ground);

  CHECK(r->unit("nobody") == nullptr);
}

TEST_CASE("Registry rejects dangling references and bad data") {
  const auto bad_build = Registry::parse(
      R"({"units":[{"id":"p","name":"P","hp":1,"sprite":"s","worker":{},"builds":["nope"]}]})",
      R"({"buildings":[]})", R"({"commands":[]})");
  REQUIRE_FALSE(bad_build);
  CHECK(bad_build.error().find("unknown building 'nope'") != std::string::npos);

  const auto bad_cmd = Registry::parse(R"({"units":[{"id":"p","name":"P","hp":1,"sprite":"s","commands":["x"]}]})",
                                       R"({"buildings":[]})", R"({"commands":[]})");
  CHECK_FALSE(bad_cmd);

  const auto bad_class = Registry::parse(
      R"({"units":[{"id":"p","name":"P","hp":1,"sprite":"s","move_class":"swim"}]})", R"({"buildings":[]})",
      R"({"commands":[]})");
  CHECK_FALSE(bad_class);

  const auto no_worker = Registry::parse(
      R"({"units":[{"id":"p","name":"P","hp":1,"sprite":"s","builds":["b"]}]})",
      R"({"buildings":[{"id":"b","name":"B","size":2,"build_seconds":1,"sprite":"x"}]})", R"({"commands":[]})");
  CHECK_FALSE(no_worker);

  const auto dup = Registry::parse(
      R"({"units":[{"id":"p","name":"P","hp":1,"sprite":"s"},{"id":"p","name":"Q","hp":1,"sprite":"s"}]})",
      R"({"buildings":[]})", R"({"commands":[]})");
  REQUIRE_FALSE(dup);
  CHECK(dup.error().find("duplicate id 'p'") != std::string::npos);

  const auto missing = Registry::parse(R"({"units":[{"id":"p"}]})", R"({"buildings":[]})", R"({"commands":[]})");
  CHECK_FALSE(missing);

  const auto garbage = Registry::parse("not json", kBuildings, kCommands);
  CHECK_FALSE(garbage);
}

TEST_CASE("The shipped game data is consistent") {
  // Tests run from the build directory; the data lives next to the sources.
  const auto r = Registry::load_directory(MYRTS_SOURCE_DIR "/assets/data");
  REQUIRE(r);
  const BuildingDef* hall = r->building("great_hall");
  const BuildingDef* farm = r->building("pig_farm");
  const BuildingDef* barracks = r->building("barracks");
  REQUIRE((hall && farm && barracks));
  CHECK(hall->cost == Cost{1200, 800, 0});
  CHECK(hall->build_seconds == 255.0);
  CHECK(farm->cost == Cost{500, 250, 0});
  CHECK(farm->build_seconds == 100.0);
  CHECK(barracks->cost == Cost{700, 450, 0});
  CHECK(barracks->build_seconds == 200.0);
  REQUIRE(r->unit("orc_peon"));
  CHECK(r->unit("orc_peon")->builds.size() == 7);
}

TEST_CASE("Every sprite named by the shipped data exists and can animate") {
  namespace fs = std::filesystem;
  const fs::path assets = fs::path(MYRTS_SOURCE_DIR) / "assets";
  const auto r = Registry::load_directory((assets / "data").string());
  REQUIRE(r);

  for (const auto& u : r->units()) {
    INFO("unit " << u.id);
    const fs::path file = assets / u.sprite;
    REQUIRE(fs::exists(file));
    std::ifstream in(file);
    const auto sprite = nlohmann::json::parse(in);
    CHECK(fs::exists(assets / sprite.at("spritesheet").get<std::string>()));

    std::set<std::string> names;
    for (const auto& a : sprite.at("animations")) {
      names.insert(a.at("name").get<std::string>());
      CHECK_FALSE(a.at("frames").empty());
    }
    // The game always asks for these; every direction must exist (left ones are mirrored copies).
    for (const char* dir : {"up", "up_right", "right", "down_right", "down", "down_left", "left", "up_left"}) {
      CHECK(names.contains(std::string("idle_") + dir));
      CHECK(names.contains(std::string("walk_") + dir));
      if (u.can_attack()) {
        const bool has_attack = names.contains(std::string("attack_") + dir);
        const bool has_walk = names.contains(std::string("walk_") + dir);  // fallback when there is no attack art
        CHECK((has_attack || has_walk));
      }
    }
  }
  for (const auto& b : r->buildings()) {
    INFO("building " << b.id);
    CHECK(fs::exists(assets / b.sprite));
    CHECK(fs::exists(assets / b.sprite_construction));
  }
}

TEST_CASE("Every unit in the shipped data can actually be obtained") {
  const auto r = Registry::load_directory(MYRTS_SOURCE_DIR "/assets/data");
  REQUIRE(r);
  std::set<std::string> trainable;
  for (const auto& b : r->buildings())
    for (const auto& u : b.trains) trainable.insert(u);
  // Starting units are placed by the game; everything else has to come out of some building.
  const std::set<std::string> starting{"orc_peon", "alliance_peasant"};
  for (const auto& u : r->units()) {
    INFO("unit " << u.id);
    CHECK((trainable.contains(u.id) || starting.contains(u.id)));
  }
  // ...and every building can be built by somebody of its faction.
  std::set<std::string> buildable;
  for (const auto& u : r->units())
    for (const auto& b : u.builds) buildable.insert(b);
  for (const auto& b : r->buildings()) {
    INFO("building " << b.id);
    CHECK(buildable.contains(b.id));
  }
}

TEST_CASE("Every unit, building and command in the shipped data has an icon in the atlas") {
  namespace fs = std::filesystem;
  const fs::path assets = fs::path(MYRTS_SOURCE_DIR) / "assets";
  const auto r = Registry::load_directory((assets / "data").string());
  REQUIRE(r);
  constexpr int kIcons = 198;  // icons in assets/ui/icons.png
  for (const auto& u : r->units()) {
    INFO("unit " << u.id);
    CHECK(u.icon >= 0);
    CHECK(u.icon < kIcons);
  }
  for (const auto& b : r->buildings()) {
    INFO("building " << b.id);
    CHECK(b.icon >= 0);
    CHECK(b.icon < kIcons);
  }
  for (const auto& c : r->commands()) {
    INFO("command " << c.id);
    for (const char* faction : {"orcs", "alliance"}) {
      CHECK(c.icon_for(faction) >= 0);
      CHECK(c.icon_for(faction) < kIcons);
    }
  }
  const CommandDef* move = r->command("move");
  REQUIRE(move);
  CHECK(move->icon_for("orcs") != move->icon_for("alliance"));  // each side has its own arrow
  CHECK(move->icon_for("trolls") == move->icon_for("alliance"));  // unknown sides fall back to the default

  for (const char* file : {"ui/icons.png", "ui/gold.png", "ui/lumber.png", "ui/oil.png", "ui/food.png", "ui/panel.png",
                           "ui/minimap_frame.png", "ui/topbar.png", "cursors/cursors.json", "cursors/pointer.png"})
    CHECK(fs::exists(assets / file));
  for (const char* era : {"forest", "winter", "wasteland", "swamp"}) {
    CHECK(fs::exists(assets / "buildings" / (std::string("gold_mine_") + era + ".png")));
    CHECK(fs::exists(assets / "buildings" / (std::string("gold_mine_active_") + era + ".png")));
  }
}
