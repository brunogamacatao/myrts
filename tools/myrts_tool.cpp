// Command line tools for game packages. No graphics needed.
//
//   myrts_tool validate <game-dir>                      check a whole game: data, references, files, every level
//   myrts_tool import-pud <game-dir> <in.pud> <out.mrmap>   convert a Warcraft II map to the native map format
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "core/campaign.hpp"
#include "core/defs.hpp"
#include "core/game_package.hpp"
#include "core/game_rules.hpp"
#include "core/mapfile.hpp"
#include "core/match_setup.hpp"
#include "core/sounds.hpp"
#include "core/tileset.hpp"

namespace fs = std::filesystem;
using namespace myrts;

namespace {

struct Report {
  int problems = 0;
  void problem(const std::string& text) {
    ++problems;
    std::printf("  PROBLEM: %s\n", text.c_str());
  }
  void check_file(const GamePackage& pkg, const std::string& relative, const std::string& who) {
    if (relative.empty()) return;
    if (!fs::exists(pkg.path(relative))) problem(who + ": missing file " + relative);
  }
};

bool load_everything(const fs::path& dir, Report& report, GamePackage& pkg_out, Registry& registry_out,
                     std::shared_ptr<const TileSets>& tilesets_out, GameRules& rules_out, Campaign& campaign_out) {
  auto pkg = GamePackage::load(dir);
  if (!pkg) {
    report.problem(pkg.error());
    return false;
  }
  pkg_out = *pkg;
  std::printf("game '%s' (%s) version %s\n", pkg->title.c_str(), pkg->id.c_str(), pkg->version.c_str());

  auto registry = Registry::load_directory(pkg->path(pkg->data_dir));
  if (!registry) {
    report.problem(registry.error());
    return false;
  }
  registry_out = std::move(*registry);
  std::printf("  %zu units, %zu buildings, %zu commands, %zu factions, %d resources\n", registry_out.units().size(),
              registry_out.buildings().size(), registry_out.commands().size(), registry_out.factions().size(),
              registry_out.resources()->size());

  if (fs::exists(pkg->path(pkg->rules_file))) {
    auto rules = GameRules::load_file(pkg->path(pkg->rules_file));
    if (!rules) report.problem(rules.error());
    else rules_out = *rules;
  }
  auto tilesets = TileSets::load_file(pkg->path(pkg->tilesets_file));
  if (!tilesets) {
    report.problem(tilesets.error());
    return false;
  }
  tilesets_out = std::make_shared<const TileSets>(std::move(*tilesets));
  auto campaign = Campaign::load_file(pkg->path(pkg->campaign_file), pkg->path(pkg->ai_profiles_file));
  if (!campaign) {
    report.problem(campaign.error());
    return false;
  }
  campaign_out = std::move(*campaign);
  std::printf("  %zu levels\n", campaign_out.size());
  return true;
}

int validate(const fs::path& dir) {
  Report report;
  GamePackage pkg = GamePackage::defaults(dir);
  Registry registry;
  std::shared_ptr<const TileSets> tilesets;
  GameRules rules;
  Campaign campaign = Campaign::single("");
  if (!load_everything(dir, report, pkg, registry, tilesets, rules, campaign)) return 1;

  for (const UnitDef& u : registry.units()) report.check_file(pkg, u.sprite, "unit " + u.id);
  for (const BuildingDef& b : registry.buildings()) {
    report.check_file(pkg, b.sprite, "building " + b.id);
    report.check_file(pkg, b.sprite_construction, "building " + b.id);
  }
  for (const ResourceDef& r : registry.resources()->all()) report.check_file(pkg, r.icon, "resource " + r.id);
  for (const auto& music : {pkg.music_intro, pkg.music_menu, pkg.music_victory, pkg.music_defeat}) report.check_file(pkg, music, "game.json music");
  for (std::size_t i = 0; i < tilesets->count(); ++i) report.check_file(pkg, tilesets->era(static_cast<Era>(i)).image(), "tileset");
  if (auto sounds = SoundTable::load_file(pkg.path(pkg.sound_table_file)); !sounds) report.problem(sounds.error());

  // Every level must build a playable match (this loads its map, and checks the factions, objectives and stocks).
  for (std::size_t i = 0; i < campaign.size(); ++i) {
    const Level& level = campaign.level(i);
    report.check_file(pkg, level.music, "level '" + level.name + "' music");
    MatchOptions options;
    options.assets_dir = pkg.root().generic_string();
    options.world.set_rules(rules);
    if (const auto match = build_match(registry, tilesets, level, options); !match)
      report.problem("level '" + level.name + "': " + match.error());
  }
  std::printf(report.problems == 0 ? "OK: no problems found\n" : "%d problem(s) found\n", report.problems);
  return report.problems == 0 ? 0 : 1;
}

int import_pud(const fs::path& dir, const std::string& in, const std::string& out) {
  Report report;
  GamePackage pkg = GamePackage::defaults(dir);
  Registry registry;
  std::shared_ptr<const TileSets> tilesets;
  GameRules rules;
  Campaign campaign = Campaign::single("");
  if (!load_everything(dir, report, pkg, registry, tilesets, rules, campaign)) return 1;
  const auto pud = load_pud(in);
  if (!pud) {
    std::fprintf(stderr, "%s\n", pud.error().c_str());
    return 1;
  }
  const auto map = map_from_pud(*pud, *tilesets, *registry.resources(), rules);
  if (!map) {
    std::fprintf(stderr, "%s\n", map.error().c_str());
    return 1;
  }
  std::ofstream(out) << write_mrmap(*map);
  std::printf("wrote %s: %dx%d tiles, %zu start locations, %zu mines\n", out.c_str(), map->width, map->height, map->starts.size(), map->mines.size());
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string command = argc > 1 ? argv[1] : "";
  if (command == "validate" && argc == 3) return validate(argv[2]);
  if (command == "import-pud" && argc == 5) return import_pud(argv[2], argv[3], argv[4]);
  std::fprintf(stderr,
               "usage:\n  myrts_tool validate <game-dir>\n  myrts_tool import-pud <game-dir> <in.pud> <out.mrmap>\n");
  return 2;
}
