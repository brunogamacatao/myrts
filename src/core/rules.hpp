#pragma once

#include <optional>

#include "world.hpp"

namespace myrts {

enum class Outcome { Victory, Defeat };

/// The local player loses when nothing of theirs is left, and wins when every other player has
/// lost everything. With no opponents in the game there is no outcome (sandbox).
[[nodiscard]] std::optional<Outcome> evaluate_outcome(const World& world, PlayerId local);

}  // namespace myrts
