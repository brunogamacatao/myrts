#include "package_check.hpp"

#include <memory>

#include "campaign.hpp"
#include "defs.hpp"
#include "game_package.hpp"
#include "game_rules.hpp"
#include "match_setup.hpp"
#include <fstream>
#include <set>

#include "sounds.hpp"
#include "sprite_doc.hpp"
#include "tileset.hpp"

namespace fs = std::filesystem;

namespace myrts {

namespace {
void check_file(PackageReport& report, const GamePackage& pkg, const std::string& relative, const std::string& who) {
  if (relative.empty()) return;
  if (!fs::exists(pkg.path(relative))) report.problems.push_back(who + ": missing file " + relative);
}
}  // namespace

PackageReport check_package(const fs::path& dir) {
  PackageReport report;
  auto pkg = GamePackage::load(dir);
  if (!pkg) {
    report.problems.push_back(pkg.error());
    return report;
  }
  report.info.push_back("game '" + pkg->title + "' (" + pkg->id + ") version " + pkg->version);

  auto registry = Registry::load_directory(pkg->path(pkg->data_dir));
  if (!registry) {
    report.problems.push_back(registry.error());
    return report;
  }
  report.info.push_back(std::to_string(registry->units().size()) + " units, " + std::to_string(registry->buildings().size()) +
                        " buildings, " + std::to_string(registry->commands().size()) + " commands, " +
                        std::to_string(registry->factions().size()) + " factions, " +
                        std::to_string(registry->resources()->size()) + " resources");

  GameRules rules;
  if (fs::exists(pkg->path(pkg->rules_file))) {
    auto loaded = GameRules::load_file(pkg->path(pkg->rules_file));
    if (!loaded) report.problems.push_back(loaded.error());
    else rules = *loaded;
  }
  auto tilesets = TileSets::load_file(pkg->path(pkg->tilesets_file));
  if (!tilesets) {
    report.problems.push_back(tilesets.error());
    return report;
  }
  const auto shared_tilesets = std::make_shared<const TileSets>(std::move(*tilesets));
  auto campaign = Campaign::load_file(pkg->path(pkg->campaign_file), pkg->path(pkg->ai_profiles_file));
  if (!campaign) {
    report.problems.push_back(campaign.error());
    return report;
  }
  report.info.push_back(std::to_string(campaign->size()) + " levels");

  std::set<std::string> sprites_checked;
  for (const UnitDef& u : registry->units()) {
    check_file(report, *pkg, u.sprite, "unit " + u.id);
    if (u.sprite.empty() || !sprites_checked.insert(u.sprite).second || !fs::exists(pkg->path(u.sprite))) continue;
    std::ifstream in(pkg->path(u.sprite));
    const auto doc = nlohmann::json::parse(in, nullptr, false);
    if (doc.is_discarded()) {
      report.problems.push_back("sprite " + u.sprite + ": not valid JSON");
      continue;
    }
    if (const std::string problem = sprite_problem(doc); !problem.empty()) report.problems.push_back("sprite " + u.sprite + ": " + problem);
    else check_file(report, *pkg, doc["spritesheet"].get<std::string>(), "sprite " + u.sprite);
  }
  for (const BuildingDef& b : registry->buildings()) {
    check_file(report, *pkg, b.sprite, "building " + b.id);
    check_file(report, *pkg, b.sprite_construction, "building " + b.id);
  }
  for (const ResourceDef& r : registry->resources()->all()) check_file(report, *pkg, r.icon, "resource " + r.id);
  for (const auto& music : {pkg->music_intro, pkg->music_menu, pkg->music_victory, pkg->music_defeat})
    check_file(report, *pkg, music, "game.json music");
  for (std::size_t i = 0; i < shared_tilesets->count(); ++i)
    check_file(report, *pkg, shared_tilesets->era(static_cast<Era>(i)).image(), "tileset");
  if (auto sounds = SoundTable::load_file(pkg->path(pkg->sound_table_file)); !sounds) report.problems.push_back(sounds.error());

  // Every level must build a playable match (this loads its map, and checks the factions, objectives and stocks).
  for (std::size_t i = 0; i < campaign->size(); ++i) {
    const Level& level = campaign->level(i);
    check_file(report, *pkg, level.music, "level '" + level.name + "' music");
    MatchOptions options;
    options.assets_dir = pkg->root().generic_string();
    options.world.set_rules(rules);
    if (const auto match = build_match(*registry, shared_tilesets, level, options); !match)
      report.problems.push_back("level '" + level.name + "': " + match.error());
  }
  return report;
}

}  // namespace myrts
