#pragma once

#include <expected>
#include <optional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "ai.hpp"
#include "defs.hpp"

namespace myrts {

/// One level of the campaign: a Warcraft II map and how hard the opponent plays on it.
/// One thing a level asks of the player (see core/rules.hpp for how they are judged). `type` is one of:
/// victory: "destroy_all" (the default when a level lists none), "destroy_building" (`building`), "survive" (`seconds`),
/// "collect" (`resource`, `amount`), "build" (`building`, `count`); defeat (besides losing everything):
/// "lose_building" (`building`: the player owns none), "time_limit" (`seconds`).
struct Objective {
  std::string type;
  std::string text;      ///< how it reads to the player (optional)
  std::string building;  ///< a building id
  std::string resource;  ///< a resource id
  int amount = 0;        ///< "collect": how much; "build": how many
  double seconds = 0.0;
};

struct Objectives {
  std::vector<Objective> victory;  ///< all must hold; empty: destroy_all
  std::vector<Objective> defeat;   ///< any one loses (besides losing everything)
};

/// A computer player beyond the main enemy: it takes the free start location farthest from the others.
struct OpponentSpec {
  std::string faction;  ///< empty: the enemy's faction
  AiConfig ai;
  std::map<std::string, int> stock{{"gold", 1000}, {"lumber", 300}};
  int workers = 2;
};

struct Level {
  std::string name;
  std::string map;       ///< .pud path, relative to the game's assets
  std::string music;     ///< the track that loops during the level (path relative to the assets)
  std::string chapter;   ///< the group it belongs to ("Land warfare", "Air war"...), shown on the level screens
  std::string kind = "land";  ///< "land": the bases connect on foot; "air": they do not, so the way across is by air
  std::string player_faction;  ///< the side the player commands (a faction id); empty: the game's first faction
  std::string enemy_faction;   ///< the computer's side; empty: the game's second faction (or the first when there is one)
  std::string briefing;  ///< one line shown when the level starts
  std::vector<std::string> hints;  ///< tutorial lines shown one after another while the level begins
  std::vector<std::string> allow;  ///< unit and building ids the player may use; empty means everything
  /// Starting resources by resource id ("gold": 2000); resolved against the game's resources when the match is built.
  std::map<std::string, int> player_stock{{"gold", 2000}, {"lumber", 1000}};
  AiConfig ai;           ///< the opponent's pace (waves, delay, workers)
  std::map<std::string, int> enemy_stock{{"gold", 1000}, {"lumber", 300}};
  int enemy_workers = 2;
  std::string ai_profile;  ///< name of the profile in ai_profiles.json that `ai` started from (informative)
  Objectives objectives;   ///< what wins and loses the level
  std::vector<OpponentSpec> more_opponents;  ///< further computer players (every player fights every other)
};

/// `level` with an empty `player_faction` / `enemy_faction` filled in with the game's first and second faction.
[[nodiscard]] Level with_default_sides(Level level, const Registry& registry);

/// `level` as played by `faction`. Levels are written for one faction (`level.player_faction`, the orcs); to play another
/// the sides are swapped (the written player's faction becomes the enemy) and the unit and building ids of `allow` and
/// the words of the briefing and hints are translated with the faction's `Translation` (see assets/data/factions.json).
/// An unknown faction, or the level's own, leaves the level as it is.
[[nodiscard]] Level play_as(const Level& level, const Registry& registry, std::string_view faction);

/// The ordered list of levels, loaded from assets/data/campaign.json. Winning a level leads to the next one.
class Campaign {
 public:
  /// Parses `{"levels":[{"name","map","briefing","ai":{...},"enemy_stock":{...},"enemy_workers"}...]}`; at least one
  /// level is required and every level needs a name and a map.
  /// `profiles_json` is an optional ai_profiles.json: `{"profiles":{"name":{...AiConfig fields...}}}`; a level's
  /// `"ai_profile":"name"` starts its `ai` from that profile, and its own `ai` keys override single values.
  static std::expected<Campaign, std::string> parse(std::string_view json_text, std::string_view profiles_json = {});
  static std::expected<Campaign, std::string> load_file(const std::string& path, const std::string& profiles_path = {});

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
