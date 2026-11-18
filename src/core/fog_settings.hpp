#pragma once

#include <array>
#include <expected>
#include <string>
#include <string_view>

namespace myrts {

/// Every variable of the fog of war. The game's defaults live in `rules.json` ("fog"); a level may change any of them with
/// a "fog" object of its own (`campaign.json`). The defaults are the original game's fog.
struct FogSettings {
  // ---- what is hidden
  bool enabled = true;      ///< false: no fog at all, everyone sees the whole map
  bool reveal_map = false;  ///< true: the whole map is known from the start (no black), but units out of sight stay hidden
  bool remember = true;     ///< false: ground that leaves sight turns black again instead of staying explored

  // ---- how far things see
  double sight_scale = 1.0;       ///< every sight radius is multiplied by this...
  int sight_bonus = 0;            ///< ...then this many tiles are added
  int min_unit_sight = -1;        ///< a unit sees at least this far (-1: the rule of rules.json "vision")
  int building_sight_base = -1;   ///< a building sees `base + size` tiles (-1: the rule of rules.json "vision")
  double update_interval = 0.2;   ///< seconds between two refreshes of what is in sight

  // ---- how it looks
  bool minimap = true;                      ///< the minimap is covered by the fog too
  std::array<int, 3> unknown_color{0, 0, 0};          ///< the black of ground never seen (0-255 each)
  double unknown_opacity = 1.0;                       ///< 0 (clear) to 1 (opaque)
  std::array<int, 3> explored_color{102, 102, 110};   ///< the veil over ground seen before but not in sight now
  double explored_opacity = 0.55;
  double edge_softness = 1.0;     ///< how gradual the edge of the black is (0.2 sharp ... 3 very soft)

  /// `base` with the keys of the JSON object `overrides` replaced ("" or "{}": unchanged). Unknown keys and out-of-range
  /// values are an error naming the key.
  static std::expected<FogSettings, std::string> with(const FogSettings& base, std::string_view overrides);
};

}  // namespace myrts
