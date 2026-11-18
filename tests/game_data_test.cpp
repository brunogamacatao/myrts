#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>

#include "core/campaign.hpp"
#include "core/defs.hpp"
#include "core/game_package.hpp"
#include "core/game_rules.hpp"
#include "core/sounds.hpp"
#include "core/strings.hpp"
#include "editor_model/schema.hpp"
#include "editor_model/texts.hpp"

using namespace myrts;
using namespace myrts::editor;
namespace fs = std::filesystem;
using Json = Project::Json;

namespace {
const fs::path kGame = fs::path(MYRTS_SOURCE_DIR) / "games" / "warcraft-like";

struct TempGame {
  fs::path path;
  Project project;
  explicit TempGame(const std::string& name) : path(fs::temp_directory_path() / ("myrts_gamedata_" + name)), project(make(path)) {}
  ~TempGame() { std::error_code ec; fs::remove_all(path, ec); }
  static Project make(const fs::path& dir) {
    fs::remove_all(dir);
    auto p = Project::create_from(kGame, dir, "Game data");
    REQUIRE(p.has_value());
    return std::move(*p);
  }
};

/// A document with every field of the schema present (the default, or a plausible value), as the inspector would
/// leave it after someone touched every widget.
Json filled(const std::vector<Field>& fields, const Project& project) {
  Json out = Json::object();
  for (const Field& f : fields) {
    switch (f.type) {
      case FieldType::Object: out[f.key] = filled(f.fields, project); break;
      case FieldType::Bool:
      case FieldType::Int:
      case FieldType::Float:
      case FieldType::Color: out[f.key] = f.default_value; break;
      case FieldType::Choice: out[f.key] = f.options.front(); break;
      case FieldType::Ref: {
        const auto ids = reference_options(project, f.source);
        if (!ids.empty()) out[f.key] = ids[0];
        break;
      }
      default: break;  // text, lists and maps stay absent: empty values are not "filled"
    }
  }
  return out;
}
}  // namespace

TEST_CASE("every field of the manifest schema is a key the loader reads") {
  TempGame g("manifest");
  Json manifest = *g.project.document("game.json");
  for (const Field& f : manifest_schema()) {
    if (f.type == FieldType::Object) {
      CHECK(manifest.contains(f.key));  // paths and music are the sections the loader knows
      for (const Field& c : f.fields) CHECK((manifest[f.key].contains(c.key) || !c.key.empty()));
    }
  }
  CHECK(g.project.set("game.json", Json::json_pointer("/music/menu"), "music/orc2.ogg"));
  REQUIRE(g.project.save().has_value());
  CHECK(GamePackage::load(g.path)->music_menu == "music/orc2.ogg");
}

TEST_CASE("rules written with every field of the schema load, and the schema defaults are the engine's defaults") {
  TempGame g("rules");
  const Json doc = filled(rules_schema(), g.project);
  const auto rules = GameRules::parse(doc.dump());
  REQUIRE(rules.has_value());
  const GameRules defaults;
  CHECK(rules->lumber_per_tree == defaults.lumber_per_tree);
  CHECK(rules->max_queue == defaults.max_queue);
  CHECK(rules->scan_interval == defaults.scan_interval);
  CHECK(rules->min_damage == defaults.min_damage);
  CHECK(rules->repair_cost_share == defaults.repair_cost_share);
  CHECK(rules->repair_min_seconds == defaults.repair_min_seconds);
  CHECK(rules->min_unit_sight == defaults.min_unit_sight);
  CHECK(rules->building_sight_base == defaults.building_sight_base);
  CHECK(rules->initial_workers == defaults.initial_workers);
  CHECK(rules->mine_size == defaults.mine_size);
  CHECK(rules->gold_per_value_unit == defaults.gold_per_value_unit);
  CHECK(rules->default_mine_gold == defaults.default_mine_gold);
  CHECK(rules->ai_farm_margin == defaults.ai_farm_margin);
  CHECK(rules->corpse_seconds == defaults.corpse_seconds);
  CHECK(rules->corpse_fade_seconds == defaults.corpse_fade_seconds);
  CHECK(rules->max_corpses == defaults.max_corpses);
  CHECK(rules->air_lift == defaults.air_lift);
}

TEST_CASE("an entry written with every field of the resource, projectile, command and faction schemas loads") {
  TempGame g("tables");
  Project& p = g.project;
  const std::string data = "data/";
  p.change(data + "resources.json", [&](Json& d) {
    Json r = filled(resource_schema(), p);
    r["id"] = "crystal";
    r["name"] = "Crystal";
    r["source"] = "site";
    d["resources"].push_back(r);
  });
  p.change(data + "projectiles.json", [&](Json& d) {
    Json r = filled(projectile_schema(), p);
    r["id"] = "spear";
    d["projectiles"].push_back(r);
  });
  p.change(data + "commands.json", [&](Json& d) {
    Json r = filled(command_schema(), p);
    r["id"] = "scout";
    r["order"] = "move";
    d["commands"].push_back(r);
  });
  p.change(data + "factions.json", [&](Json& d) {
    Json r = filled(faction_schema(), p);
    r["id"] = "elves";
    d["factions"].push_back(r);
  });
  REQUIRE(p.save().has_value());
  const auto registry = Registry::load_directory((g.path / "data").string());
  INFO(std::string(registry ? "" : registry.error()));
  REQUIRE(registry.has_value());
  CHECK(registry->resources()->index("crystal") != kNoResource);
  CHECK(registry->projectile("spear") != nullptr);
  CHECK(registry->command("scout") != nullptr);
  CHECK(registry->faction("elves") != nullptr);
}

TEST_CASE("right-click rules, AI profiles and sounds edited through their schemas still load") {
  TempGame g("misc");
  Project& p = g.project;
  p.set("data/commands.json", Json::json_pointer("/context_orders"),
        Json::array({Json{{"when", Json::array({"enemy"})}, {"command", "attack"}}, Json{{"command", "move"}}}));
  p.set(p.package().ai_profiles_file, Json::json_pointer("/profiles/sneaky"), Json{{"first_wave", 9}, {"start_delay", 30}});
  p.set(p.package().sound_table_file, Json::json_pointer("/effects/boom"), Json::array({"constrct.wav"}));
  p.set(p.package().sound_table_file, Json::json_pointer("/voices/troll"), Json{{"base", "orc"}, {"dead", Json::array({"odead.wav"})}});
  REQUIRE(p.save().has_value());
  p.revalidate();
  CHECK(p.report().ok());
  CHECK(reference_options(p, "voices") == std::vector<std::string>{"orc", "human", "troll"});
  CHECK(reference_options(p, "conditions").size() == 5);
  CHECK(reference_options(p, "ai_profiles").size() == 4);
  CHECK(SoundTable::load_file((g.path / p.package().sound_table_file).string()).has_value());
}

TEST_CASE("texts: a language file lists what is translated, finds what is missing, and can be started from the game's data") {
  TempGame g("texts");
  Project& p = g.project;
  const auto languages = texts::languages(p);
  REQUIRE(languages.size() == 1);
  const std::string pt = languages[0];
  CHECK(texts::count(p, pt) > 100);
  CHECK(texts::get(p, pt, "Options") == "Opções");

  const auto data = texts::data_texts(p);
  CHECK(std::ranges::find(data, "Grunt") != data.end());
  CHECK(std::ranges::find(data, "First Steps") != data.end());
  CHECK(std::ranges::find(data, "Attack") != data.end());

  // A fresh language has everything missing; adding the entries leaves them untranslated until edited.
  const auto made = texts::create_language(p, "es");
  REQUIRE(made.has_value());
  CHECK_FALSE(texts::create_language(p, "es").has_value());
  CHECK_FALSE(texts::create_language(p, "en").has_value());
  CHECK(texts::missing(p, *made).size() == data.size());
  const std::size_t added = texts::add_entries(p, *made, data);
  CHECK(added == data.size());
  CHECK(texts::missing(p, *made).empty());
  CHECK(texts::untranslated(p, *made).size() == data.size());
  CHECK(texts::set(p, *made, "Grunt", "Gruñón"));
  CHECK(texts::untranslated(p, *made).size() == data.size() - 1);
  CHECK(texts::remove(p, *made, "Grunt"));
  CHECK_FALSE(texts::remove(p, *made, "Grunt"));
  CHECK(texts::set(p, *made, "Grunt", "Gruñón"));
  REQUIRE(p.save().has_value());

  const auto strings = Strings::load_file((g.path / *made).string());
  REQUIRE(strings.has_value());
  CHECK(strings->tr("Grunt") == "Gruñón");
}
