#pragma once

#include <expected>
#include <string>

#include "fog_settings.hpp"

namespace myrts {

/// The tunable rules of a game (`rules.json` in the game package). Every field has the default the original game
/// shipped with, so a missing file or key changes nothing. Pure data: the engine reads it, never writes it.
struct GameRules {
  // ---- simulation ----
  int lumber_per_tree = 100;     ///< wood a tree holds
  int max_queue = 5;             ///< training queue length of a building
  double scan_interval = 0.3;    ///< seconds between an idle fighter's looks for enemies
  double retry_delay = 0.5;      ///< seconds before re-planning a blocked route

  // ---- combat ----
  int min_damage = 1;            ///< a hit always does at least this much, whatever the armor
  double attack_flash = 0.45;    ///< seconds the strike animation plays

  // ---- repair ----
  double repair_cost_share = 0.25;  ///< mending from nothing to whole costs this share of the price
  double repair_min_seconds = 20.0; ///< a machine takes at least this long to repair from nothing

  // ---- vision (tiles) ----
  int min_unit_sight = 2;        ///< even a unit with no sight sees this far
  int building_sight_base = 3;   ///< a building sees `base + size` tiles

  // ---- match setup ----
  int initial_workers = 8;       ///< workers the human side starts with
  int mine_size = 3;             ///< a mine is `size` x `size` tiles
  int gold_per_value_unit = 2500;///< a map's mine value is a multiple of this
  int default_mine_gold = 10000; ///< a mine whose map gives no value

  // ---- ai ----
  int ai_farm_margin = 2;        ///< the computer builds a farm when this little food is left

  // ---- fog of war (a level may override it, see FogSettings)
  FogSettings fog;

  // ---- presentation (read by the front end) ----
  double corpse_seconds = 25.0;  ///< how long a body stays
  double corpse_fade_seconds = 5.0;///< the last part of it, fading out
  int max_corpses = 60;
  float air_lift = 14.0f;        ///< pixels a flying unit is drawn above the ground

  /// Parses a rules document. Unknown keys are an error (a typo would otherwise silently do nothing).
  static std::expected<GameRules, std::string> parse(const std::string& json);
  static std::expected<GameRules, std::string> load_file(const std::string& path);
};

}  // namespace myrts
