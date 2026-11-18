#include "game_package.hpp"

#include <fstream>
#include <nlohmann/json.hpp>

namespace myrts {

namespace {
constexpr int kSupportedFormat = 1;

void read(const nlohmann::json& j, const char* key, std::string& out) {
  if (!j.contains(key)) return;
  if (!j.at(key).is_string()) throw std::runtime_error(std::string("\"") + key + "\" must be a string");
  out = j.at(key).get<std::string>();
}
}  // namespace

GamePackage GamePackage::defaults(const std::filesystem::path& directory) {
  GamePackage p;
  p.root_ = directory;
  return p;
}

std::string GamePackage::path(const std::string& relative) const {
  if (relative.empty()) return {};
  return (root_ / relative).generic_string();
}

std::expected<GamePackage, std::string> GamePackage::load(const std::filesystem::path& directory) {
  const auto manifest = directory / "game.json";
  std::ifstream f(manifest);
  if (!f) return std::unexpected("cannot open " + manifest.string() + " (is this a game folder?)");
  try {
    const auto j = nlohmann::json::parse(f);
    GamePackage p = defaults(directory);
    p.format_version = j.value("format_version", 1);
    if (p.format_version > kSupportedFormat)
      return std::unexpected(manifest.string() + ": format_version " + std::to_string(p.format_version) +
                             " is newer than this engine supports (" + std::to_string(kSupportedFormat) + ")");
    read(j, "id", p.id);
    read(j, "title", p.title);
    read(j, "version", p.version);
    read(j, "default_language", p.default_language);
    if (j.contains("paths")) {
      const auto& d = j.at("paths");
      read(d, "data", p.data_dir);
      read(d, "sounds", p.sounds_dir);
      read(d, "cursors", p.cursors_dir);
      read(d, "campaign", p.campaign_file);
      read(d, "tilesets", p.tilesets_file);
      read(d, "sound_table", p.sound_table_file);
      read(d, "rules", p.rules_file);
      read(d, "ai_profiles", p.ai_profiles_file);
      read(d, "strings", p.strings_dir);
      read(d, "credits", p.credits_file);
      read(d, "fallback_map", p.fallback_map);
    }
    if (j.contains("music")) {
      const auto& m = j.at("music");
      read(m, "intro", p.music_intro);
      read(m, "menu", p.music_menu);
      read(m, "victory", p.music_victory);
      read(m, "defeat", p.music_defeat);
    }
    return p;
  } catch (const std::exception& e) {
    return std::unexpected(manifest.string() + ": " + e.what());
  }
}

}  // namespace myrts
