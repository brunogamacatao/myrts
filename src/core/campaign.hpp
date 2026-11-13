#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ai.hpp"
#include "defs.hpp"

namespace myrts {

/// One level of the campaign: a Warcraft II map and how hard the opponent plays on it.
struct Level {
  std::string name;
  std::string map;       ///< .pud path, relative to the game's assets
  std::string music;     ///< the track that loops during the level (path relative to the assets)
  std::string chapter;   ///< the group it belongs to ("Land warfare", "Air war"...), shown on the level screens
  std::string kind = "land";  ///< "land": the bases connect on foot; "air": they do not, so the way across is by air
  std::string briefing;  ///< one line shown when the level starts
  std::vector<std::string> hints;  ///< tutorial lines shown one after another while the level begins
  std::vector<std::string> allow;  ///< unit and building ids the player may use; empty means everything
  Stock player_stock{2000, 1000, 0};
  AiConfig ai;           ///< the opponent's pace (waves, delay, workers)
  Stock enemy_stock{1000, 300, 0};
  int enemy_workers = 2;
};

/// The ordered list of levels, loaded from assets/data/campaign.json. Winning a level leads to the next one.
class Campaign {
 public:
  /// Parses `{"levels":[{"name","map","briefing","ai":{...},"enemy_stock":{...},"enemy_workers"}...]}`; at least one
  /// level is required and every level needs a name and a map.
  static std::expected<Campaign, std::string> parse(std::string_view json_text);
  static std::expected<Campaign, std::string> load_file(const std::string& path);

  /// A one-level campaign on `map` (for MYRTS_MAP, and when no campaign file exists).
  static Campaign single(std::string map);

  [[nodiscard]] std::size_t size() const { return levels_.size(); }
  [[nodiscard]] const Level& level(std::size_t i) const { return levels_[i < levels_.size() ? i : levels_.size() - 1]; }
  [[nodiscard]] const std::vector<Level>& levels() const { return levels_; }
  /// The index after `i`, or nothing after the last level.
  [[nodiscard]] std::optional<std::size_t> next(std::size_t i) const {
    return i + 1 < levels_.size() ? std::optional<std::size_t>(i + 1) : std::nullopt;
  }

 private:
  std::vector<Level> levels_;
};

}  // namespace myrts
