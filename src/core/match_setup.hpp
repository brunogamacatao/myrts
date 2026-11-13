#pragma once

#include <expected>
#include <memory>
#include <string>
#include <vector>

#include "ai.hpp"
#include "campaign.hpp"
#include "defs.hpp"
#include "tileset.hpp"
#include "world.hpp"

namespace myrts {

/// A match ready to play: the World with both sides placed, and the computer players that drive the opponents.
/// Everything the view needs to know about the setup is here, so `Game` only has to draw it.
struct Match {
  std::unique_ptr<World> world;                ///< heap-allocated: the AIs keep a reference to it
  std::vector<std::unique_ptr<Ai>> ais;        ///< one per computer player
  PlayerId local = kNoPlayer;                  ///< the human's player
  PlayerId enemy = kNoPlayer;
  const FactionDef* local_faction = nullptr;
  const FactionDef* enemy_faction = nullptr;
  Point local_hall{};                          ///< top-left tile of each side's town hall
  Point enemy_hall{};
  const BuildingDef* local_hall_def = nullptr;
};

struct MatchOptions {
  WorldConfig world;             ///< tile size, speeds, seed
  std::string assets_dir;        ///< prefix for the level's map path ("" when it is relative to the working directory)
};

/// Builds the match for `level`: loads the map, places the gold mines, creates both players (the first start location
/// is the human's, the opponent takes the farthest one), puts a town hall, the starting workers and (for the computer) its
/// production buildings, and attaches an Ai to the opponent. Pure logic: no graphics, so it can be tested headless.
/// @return the match, or a message naming what is wrong with the level or the data.
std::expected<Match, std::string> build_match(const Registry& registry, std::shared_ptr<const TileSets> tilesets,
                                              const Level& level, const MatchOptions& options);

}  // namespace myrts
