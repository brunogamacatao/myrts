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
#include "core/package_check.hpp"
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
  const PackageReport report = check_package(dir);
  for (const std::string& line : report.info) std::printf("%s\n", line.c_str());
  for (const std::string& problem : report.problems) std::printf("  PROBLEM: %s\n", problem.c_str());
  if (report.ok()) std::printf("OK: no problems found\n");
  else std::printf("%zu problem(s) found\n", report.problems.size());
  return report.ok() ? 0 : 1;
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
