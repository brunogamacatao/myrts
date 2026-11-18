#pragma once

#include <optional>
#include <vector>

#include "building.hpp"
#include "game_rules.hpp"
#include "grid.hpp"
#include "pud.hpp"

namespace myrts {

/// Where a player starts: the start-location marker and, if the map placed one, the player's town or great hall
/// (top-left tile).
struct StartSlot {
  int owner = 0;
  Point start{};
  std::optional<Point> hall;
};

struct MineSpec {
  Point origin{};  // top-left tile of the 3x3 mine
  int gold = 0;  // what it holds (gold or oil: see `kind`)
  std::string source;  // what it holds: a resource id ("gold") in a Scenario of a MapFile; a Warcraft II map object name
                       // ("gold_mine", "oil_patch") in one read straight from a .pud (see ResourceDef::pud_source)
};

/// What a map says about who starts where and what it holds, read from its units.
struct Scenario {
  std::vector<StartSlot> starts;  // by owner
  std::vector<MineSpec> mines;

  /// The start slot farthest from `slots[from]`: the natural opponent in a game of two.
  [[nodiscard]] std::optional<std::size_t> farthest_from(std::size_t from) const;
};

/// `rules` gives the gold value of a mine (the map holds a multiple of `gold_per_value_unit`).
Scenario scenario_from(const PudMap& pud, const GameRules& rules = {});

}  // namespace myrts
