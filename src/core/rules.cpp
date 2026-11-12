#include "rules.hpp"

namespace myrts {

std::optional<Outcome> evaluate_outcome(const World& world, PlayerId local) {
  if (!world.has_assets(local)) return Outcome::Defeat;

  bool has_opponents = false;
  for (PlayerId p = 0; p < static_cast<PlayerId>(world.player_count()); ++p) {
    if (p == local) continue;
    has_opponents = true;
    if (world.has_assets(p)) return std::nullopt;  // someone is still standing
  }
  if (has_opponents) return Outcome::Victory;
  return std::nullopt;
}

}  // namespace myrts
