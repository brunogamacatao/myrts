#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

#include "core/defs.hpp"
#include "core/sounds.hpp"

using namespace myrts;
namespace fs = std::filesystem;

namespace {
constexpr const char* kJson = R"({
 "voices":{"base":{"selected":["a.wav","b.wav"],"acknowledge":["y.wav"]}},
 "units":{"grunt":{"voice":"base","acknowledge":["own.wav"],"ready":["r.wav"]},"mute":{}},
 "buildings":{"hall":{"selected":["h.wav"]}},
 "effects":{"error":["e.wav"]}})";
}  // namespace

TEST_CASE("a unit takes its voice and overrides single events") {
  const auto t = SoundTable::parse(kJson);
  REQUIRE(t.has_value());
  CHECK(t->unit("grunt", SoundEvent::Selected).size() == 2);
  CHECK(t->unit("grunt", SoundEvent::Acknowledge) == std::vector<std::string>{"own.wav"});
  CHECK(t->unit("grunt", SoundEvent::Ready) == std::vector<std::string>{"r.wav"});
  CHECK(t->unit("grunt", SoundEvent::Dead).empty());
  CHECK(t->unit("nobody", SoundEvent::Selected).empty());
  CHECK(t->building("hall") == std::vector<std::string>{"h.wav"});
  CHECK(t->effect("error").size() == 1);
  CHECK(t->all_files().size() == 6);
}

TEST_CASE("a table naming an unknown voice is rejected") {
  CHECK_FALSE(SoundTable::parse(R"({"units":{"x":{"voice":"ghost"}}})").has_value());
  CHECK_FALSE(SoundTable::parse("not json").has_value());
}

TEST_CASE("pick is deterministic and handles an empty list") {
  const std::vector<std::string> files{"a", "b", "c"};
  CHECK(SoundTable::pick(files, 4) == "b");
  CHECK(SoundTable::pick({}, 4).empty());
}

TEST_CASE("sounds fade with the distance from the screen") {
  const Rect view{100, 100, 400, 300};
  const auto centre = spatial_sound(view, 300, 250);
  CHECK(centre.gain == Catch::Approx(1.0f));
  CHECK(centre.pan == Catch::Approx(0.0f));
  CHECK(spatial_sound(view, 480, 250).pan > 0.5f);
  CHECK(spatial_sound(view, 120, 250).pan < -0.5f);
  const auto near_edge = spatial_sound(view, 600, 250);  // 100 px beyond the right edge
  CHECK(near_edge.gain < 1.0f);
  CHECK(near_edge.audible());
  CHECK_FALSE(spatial_sound(view, 2000, 250).audible());
}

TEST_CASE("the shipped sound table covers every unit and building and its files exist") {
  const fs::path assets = fs::path(MYRTS_SOURCE_DIR) / "assets";
  const auto reg = Registry::load_directory((assets / "data").string());
  REQUIRE(reg.has_value());
  const auto table = SoundTable::load_file((assets / "data" / "sounds.json").string());
  REQUIRE(table.has_value());
  for (const auto& u : reg->units()) {
    INFO(u.id);
    CHECK(table->knows_unit(u.id));
    CHECK_FALSE(table->unit(u.id, SoundEvent::Selected).empty());
    CHECK_FALSE(table->unit(u.id, SoundEvent::Acknowledge).empty());
  }
  for (const auto& b : reg->buildings()) {
    INFO(b.id);
    CHECK_FALSE(table->building(b.id).empty());
  }
  for (const char* e : {"construct", "error", "tree", "building_destroyed"}) CHECK_FALSE(table->effect(e).empty());
  for (const auto& f : table->all_files()) {
    INFO(f);
    CHECK(fs::exists(assets / "sounds" / f));
  }
}
