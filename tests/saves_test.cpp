#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>

#include "core/match_setup.hpp"
#include "core/saves.hpp"

using namespace myrts;
namespace fs = std::filesystem;

TEST_CASE("base64 round-trips every length and rejects bad text") {
  for (std::size_t n = 0; n < 12; ++n) {
    std::vector<std::uint8_t> bytes(n);
    for (std::size_t i = 0; i < n; ++i) bytes[i] = static_cast<std::uint8_t>(i * 37 + 11);
    const auto back = base64_decode(base64_encode(bytes));
    REQUIRE(back.has_value());
    CHECK(*back == bytes);
  }
  const std::vector<std::uint8_t> hello{'h', 'e', 'l', 'l', 'o'};
  CHECK(base64_encode(hello) == "aGVsbG8=");
  CHECK_FALSE(base64_decode("abc").has_value());
  CHECK_FALSE(base64_decode("ab!d").has_value());
  CHECK_FALSE(base64_decode("a=bc").has_value());
}

TEST_CASE("a save file keeps four slots, empty ones as null") {
  SaveFile file;
  SaveSlot s;
  s.name = "Level 2";
  s.level = 1;
  s.faction = "orcs";
  s.date = "2026-11-14 10:00";
  s.thumbnail = "AAAA";
  s.state = R"({"units":[1,2,3]})";
  file.set(2, s);
  const auto back = SaveFile::parse(file.to_json());
  REQUIRE(back.has_value());
  CHECK_FALSE(back->slot(0).has_value());
  REQUIRE(back->slot(2).has_value());
  CHECK(back->slot(2)->name == "Level 2");
  CHECK(back->slot(2)->level == 1);
  CHECK(back->slot(2)->thumbnail == "AAAA");
  CHECK(nlohmann::json::parse(back->slot(2)->state)["units"].size() == 3);
  CHECK_FALSE(SaveFile::parse("{}").has_value());

  const fs::path path = fs::temp_directory_path() / "myrts_saves_test" / "saves.json";
  fs::remove_all(path.parent_path());
  CHECK(SaveFile::load(path.string()).has_value());  // no file yet: empty slots
  REQUIRE(file.save(path.string()).has_value());
  const auto loaded = SaveFile::load(path.string());
  REQUIRE(loaded.has_value());
  CHECK(loaded->slot(2).has_value());
  fs::remove_all(path.parent_path());
}

namespace {

struct Data {
  fs::path assets = fs::path(MYRTS_SOURCE_DIR) / "assets";
  Registry registry = *Registry::load_directory((assets / "data").string());
  Campaign campaign = *Campaign::load_file((assets / "data" / "campaign.json").string());
  std::shared_ptr<const TileSets> tilesets =
      std::make_shared<const TileSets>(*TileSets::load_file((assets / "data" / "tilesets.json").string()));
  Match make() const {
    MatchOptions o;
    o.assets_dir = assets.string();
    return std::move(*build_match(registry, tilesets, campaign.level(0), o));
  }
};

/// owner -> definition id -> sorted tiles
std::map<std::string, std::vector<Point>> where(const World& w) {
  std::map<std::string, std::vector<Point>> out;
  w.units().for_each([&](const Unit& u) { out["u" + std::to_string(u.owner) + u.def->id].push_back(u.anchor()); });
  w.buildings().for_each([&](const Building& b) { out["b" + std::to_string(b.owner) + b.def->id].push_back(b.origin); });
  for (auto& [_, tiles] : out)
    std::ranges::sort(tiles, [](Point a, Point b) { return std::tie(a.y, a.x) < std::tie(b.y, b.x); });
  return out;
}

}  // namespace

TEST_CASE("a match saved in the middle of play comes back with the same units, buildings, resources and fog") {
  const Data d;
  Match a = d.make();
  World& w = *a.world;

  // Some play: workers gather, the hall trains, the computer thinks.
  std::vector<EntityId> workers;
  w.units().for_each([&](const Unit& u) {
    if (u.owner == a.local && u.def->worker) workers.push_back(u.id);
  });
  const Point tree = *w.nearest_tree(a.local_hall, 12);
  REQUIRE(w.order_harvest(workers, tree).ok);
  const EntityId hall = w.building_at(a.local_hall);
  const UnitDef* worker = d.registry.unit(a.local_faction->worker);
  (void)w.order_train(a.local, hall, *worker);
  for (int i = 0; i < 40 * 20; ++i) {
    w.update(0.05);
    for (auto& ai : a.ais) ai->update(0.05);
  }
  REQUIRE(w.player(a.local).stock.lumber != 0);

  const nlohmann::json state = w.save_state();
  const nlohmann::json reread = nlohmann::json::parse(state.dump());  // through text, as a save file does

  Match b = d.make();
  const auto loaded = b.world->load_state(reread, d.registry);
  REQUIRE(loaded.has_value());
  const World& r = *b.world;

  CHECK(where(r) == where(w));
  for (PlayerId p = 0; p < 2; ++p) {
    CHECK(r.player(p).stock == w.player(p).stock);
    CHECK(r.food_used(p) == w.food_used(p));
    CHECK(r.food_cap(p) == w.food_cap(p));
  }
  CHECK(r.trees().positions() == w.trees().positions());
  CHECK(r.mines().size() == w.mines().size());
  for (std::size_t i = 0; i < w.mines().size(); ++i) CHECK(r.mines()[i].site.remaining() == w.mines()[i].site.remaining());
  CHECK(r.fog(a.local).explored(a.local_hall));
  CHECK(r.fog(a.local).encode() == w.fog(a.local).encode());
  const Building* hall_after = r.buildings().get(r.building_at(a.local_hall));
  REQUIRE(hall_after != nullptr);
  CHECK(hall_after->queue.size() == w.buildings().get(hall)->queue.size());

  // The restored match goes on: the gatherers keep gathering and nothing crashes.
  const int lumber = r.player(a.local).stock.lumber;
  for (int i = 0; i < 60 * 20; ++i) {
    b.world->update(0.05);
    for (auto& ai : b.ais) ai->update(0.05);
  }
  CHECK(r.player(a.local).stock.lumber > lumber);
}

TEST_CASE("a saved game for another map or with unknown units is refused without touching the world") {
  const Data d;
  Match a = d.make();
  nlohmann::json state = a.world->save_state();
  Match b = d.make();
  const auto before = where(*b.world);

  nlohmann::json wrong_size = state;
  wrong_size["width"] = 5;
  CHECK_FALSE(b.world->load_state(wrong_size, d.registry).has_value());
  nlohmann::json ghost = state;
  ghost["units"][0]["def"] = "no_such_unit";
  CHECK_FALSE(b.world->load_state(ghost, d.registry).has_value());
  CHECK(where(*b.world) == before);
  CHECK_FALSE(b.world->load_state(nlohmann::json::object(), d.registry).has_value());
}
