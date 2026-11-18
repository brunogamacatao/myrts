#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>

#include "core/campaign.hpp"
#include "editor_model/levels.hpp"
#include "editor_model/schema.hpp"

using namespace myrts;
using namespace myrts::editor;
namespace fs = std::filesystem;

namespace {
const fs::path kGame = fs::path(MYRTS_SOURCE_DIR) / "games" / "warcraft-like";
const std::string kCampaign = "data/campaign.json";

struct TempGame {
  fs::path path;
  Project project;
  explicit TempGame(const std::string& name)
      : path(fs::temp_directory_path() / ("myrts_levels_" + name)), project(make(path)) {}
  ~TempGame() { std::error_code ec; fs::remove_all(path, ec); }
  static Project make(const fs::path& dir) {
    fs::remove_all(dir);
    auto p = Project::create_from(kGame, dir, "Levels");
    REQUIRE(p.has_value());
    return std::move(*p);
  }
};
}  // namespace

TEST_CASE("levels can be added, duplicated, renamed, moved and removed, and every step can be undone") {
  TempGame g("ops");
  Project& p = g.project;
  const std::size_t n = levels::count(p);
  REQUIRE(n >= 3);
  const std::string first = levels::name(p, 0), second = levels::name(p, 1);

  const std::size_t added = levels::add(p, "Brand New", 0);
  CHECK(added == 1);
  CHECK(levels::count(p) == n + 1);
  CHECK(levels::name(p, 1) == "Brand New");
  CHECK((*p.document(kCampaign))["levels"][1]["map"] == (*p.document(kCampaign))["levels"][0]["map"]);  // starts playable
  CHECK(p.campaign_error().empty());
  CHECK(p.dirty());

  CHECK(levels::duplicate(p, 1) == 2);
  CHECK(levels::name(p, 2) == "Brand New (copy)");
  CHECK(levels::rename(p, 2, "Renamed"));
  CHECK(levels::name(p, 2) == "Renamed");
  CHECK_FALSE(levels::rename(p, 2, ""));
  CHECK(levels::move(p, 2, 0));
  CHECK(levels::name(p, 0) == "Renamed");
  CHECK(levels::name(p, 1) == first);
  CHECK(levels::remove(p, 0));
  CHECK(levels::name(p, 0) == first);

  while (p.can_undo()) p.undo();
  CHECK(levels::count(p) == n);
  CHECK(levels::name(p, 0) == first);
  CHECK(levels::name(p, 1) == second);
  CHECK_FALSE(p.dirty());  // back to what is on disk
  while (p.can_redo()) p.redo();
  CHECK(levels::name(p, 0) == first);
  CHECK(levels::count(p) == n + 1);
  CHECK(p.dirty());
}

TEST_CASE("the last level cannot be removed, and bad indices change nothing") {
  TempGame g("limits");
  Project& p = g.project;
  while (levels::count(p) > 1) REQUIRE(levels::remove(p, 0));
  CHECK_FALSE(levels::remove(p, 0));
  CHECK_FALSE(levels::move(p, 0, 5));
  CHECK_FALSE(levels::rename(p, 7, "x"));
  CHECK(levels::duplicate(p, 9) == 9);
  CHECK(levels::count(p) == 1);
}

TEST_CASE("index_after_move follows a level through a move") {
  CHECK(levels::index_after_move(2, 2, 0) == 0);  // the moved one
  CHECK(levels::index_after_move(0, 2, 0) == 1);  // the ones it passed
  CHECK(levels::index_after_move(1, 2, 0) == 2);
  CHECK(levels::index_after_move(3, 2, 0) == 3);  // the others stay
  CHECK(levels::index_after_move(1, 0, 2) == 0);
  CHECK(levels::index_after_move(2, 0, 2) == 1);
  CHECK(levels::index_after_move(0, 0, 2) == 2);
}

TEST_CASE("set and remove are undoable, and the same field edited in a row is one step") {
  TempGame g("merge");
  Project& p = g.project;
  using Ptr = Project::Json::json_pointer;
  const Ptr hp("/levels/0/enemy_workers");
  const auto before = (*p.document(kCampaign))["levels"][0]["enemy_workers"];
  for (int v = 10; v < 20; ++v) p.set(kCampaign, hp, v);  // dragging a slider
  CHECK((*p.document(kCampaign))["levels"][0]["enemy_workers"] == 19);
  CHECK(p.undo());
  CHECK((*p.document(kCampaign))["levels"][0]["enemy_workers"] == before);  // one undo for all ten
  CHECK_FALSE(p.can_undo());

  p.set(kCampaign, hp, 1);
  p.seal();
  p.set(kCampaign, hp, 2);
  p.undo();
  CHECK((*p.document(kCampaign))["levels"][0]["enemy_workers"] == 1);  // sealed: two steps

  CHECK(p.remove(kCampaign, hp));
  CHECK_FALSE((*p.document(kCampaign))["levels"][0].contains("enemy_workers"));
  CHECK_FALSE(p.remove(kCampaign, hp));
  CHECK(p.undo());
  CHECK((*p.document(kCampaign))["levels"][0].contains("enemy_workers"));
  p.set(kCampaign, Ptr("/levels/0/fog/enabled"), false);  // parents are created
  CHECK((*p.document(kCampaign))["levels"][0]["fog"]["enabled"] == false);
  CHECK_FALSE(p.set("nope.json", hp, 1));
}

TEST_CASE("a broken campaign is reported before saving, and saving round-trips the levels") {
  TempGame g("broken");
  Project& p = g.project;
  p.set(kCampaign, Project::Json::json_pointer("/levels/0/kind"), "swimming");
  CHECK(p.campaign_error().contains("kind"));
  p.undo();
  CHECK(p.campaign_error().empty());
  levels::add(p, "Saved Level");
  REQUIRE(p.save().has_value());
  auto again = Project::open(g.path);
  REQUIRE(again.has_value());
  CHECK(levels::name(*again, levels::count(*again) - 1) == "Saved Level");
}

TEST_CASE("every field of the level schema is real: a level using all of them still loads") {
  auto project = Project::open(kGame);
  REQUIRE(project.has_value());
  CHECK(reference_options(*project, "factions") == std::vector<std::string>{"orcs", "alliance"});
  CHECK(reference_options(*project, "resources").size() == 3);
  CHECK(reference_options(*project, "ai_profiles").size() == 3);
  const auto both = reference_options(*project, "units_buildings");
  CHECK(std::ranges::find(both, "orc_grunt") != both.end());
  CHECK(std::ranges::find(both, "great_hall") != both.end());
  CHECK(reference_options(*project, "nonsense").empty());

  // The keys of the schema are exactly the keys the campaign loader reads (a typo here would silently do nothing).
  Project::Json level = {{"name", "x"}, {"map", "m"}};
  const auto defaults = [](const Field& f, auto&& self) -> Project::Json {
    switch (f.type) {
      case FieldType::Object: {
        Project::Json o = Project::Json::object();
        for (const Field& c : f.fields) o[c.key] = self(c, self);
        return o;
      }
      case FieldType::ObjectList: return Project::Json::array();
      case FieldType::TextList:
      case FieldType::RefList: return Project::Json::array();
      case FieldType::Amounts: return Project::Json::object();
      case FieldType::Color: return f.default_value;
      case FieldType::Bool:
      case FieldType::Int:
      case FieldType::Float:
      case FieldType::Choice: return f.default_value;
      default: return Project::Json("");
    }
  };
  for (const Field& f : level_schema()) {
    if (f.key == "name" || f.key == "map") continue;
    const auto v = defaults(f, defaults);
    if (v.is_string() && v.get<std::string>().empty()) continue;  // empty means "not set" for text fields
    level[f.key] = v;
  }
  level["ai_profile"] = "default";
  const auto parsed = Campaign::parse(Project::Json{{"levels", Project::Json::array({level})}}.dump(),
                                      Project::Json(*project->document("data/ai_profiles.json")).dump());
  INFO(std::string(parsed ? "" : parsed.error()));
  CHECK(parsed.has_value());
}
