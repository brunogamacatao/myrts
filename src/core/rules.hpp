#pragma once

#include <optional>

#include "campaign.hpp"
#include "world.hpp"

namespace myrts {

enum class Outcome { Victory, Defeat };

/// The local player loses when nothing of theirs is left (or a defeat objective of the level holds), and wins when the
/// level's victory objectives all hold; without any, when every other player has lost everything. With no opponents in
/// the game there is no outcome (sandbox). See `Objective` for the kinds of objective.
[[nodiscard]] std::optional<Outcome> evaluate_outcome(const World& world, PlayerId local, const Objectives& objectives = {});

}  // namespace myrts
