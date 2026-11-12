#include <catch2/catch_test_macros.hpp>

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
  CHECK(r->unit("orc_peon")->builds.size() == 3);
}
