#pragma once

#include <optional>
#include <vector>

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
  int gold = 0;
};

/// What a map says about who starts where and what it holds, read from its units.
struct Scenario {
  std::vector<StartSlot> starts;  // by owner
  std::vector<MineSpec> mines;

  /// The start slot farthest from `slots[from]`: the natural opponent in a game of two.
  [[nodiscard]] std::optional<std::size_t> farthest_from(std::size_t from) const;
};

Scenario scenario_from(const PudMap& pud);

}  // namespace myrts
