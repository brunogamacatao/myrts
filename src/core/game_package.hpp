#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace myrts {

/// A game is a folder (a "package") with a `game.json` manifest and all of its content: data tables, sprites, sounds,
/// music, maps. The engine knows nothing about a particular game; it loads one package and resolves every content
/// path against that folder. Paths written inside the package's own files are relative to the package root.
class GamePackage {
 public:
  /// Reads `<directory>/game.json`. Fails with a message naming the file and the field that is wrong.
  static std::expected<GamePackage, std::string> load(const std::filesystem::path& directory);

  /// Builds a package without a manifest (all defaults), for tests and for tools that only have a folder.
  static GamePackage defaults(const std::filesystem::path& directory);

  [[nodiscard]] const std::filesystem::path& root() const { return root_; }
  /// `relative` (a path written in the package's files) resolved against the package root.
  [[nodiscard]] std::string path(const std::string& relative) const;

  // ---- manifest ----
  std::string id = "game";
  std::string title = "myrts";
  std::string version = "0.0.0";
  int format_version = 1;
  std::string default_language = "en";

  // Where things are, relative to the root. These are the defaults; the manifest may move any of them.
  std::string data_dir = "data";            ///< units.json, buildings.json, commands.json, factions.json
  std::string sounds_dir = "sounds";        ///< the WAV effects named by the sound table
  std::string cursors_dir = "cursors";      ///< cursors.json and the cursor images
  std::string campaign_file = "data/campaign.json";
  std::string tilesets_file = "data/tilesets.json";
  std::string sound_table_file = "data/sounds.json";
  std::string ai_profiles_file = "data/ai_profiles.json";  ///< optional: named AI settings levels can start from
  std::string rules_file = "data/rules.json";  ///< optional: a missing file means the default rules
  std::string strings_dir = "strings";      ///< <language>.json files: the interface texts in each language
  std::string credits_file = "credits.txt";
  std::string fallback_map;                 ///< played when there is no campaign (empty: none)

  // Music of the screens that are not levels (a level names its own track).
  std::string music_intro;
  std::string music_menu;
  std::string music_victory;
  std::string music_defeat;

 private:
  std::filesystem::path root_;
};

}  // namespace myrts
