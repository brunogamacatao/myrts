#pragma once

#include "core/resources.hpp"

namespace myrts {
/// Slots of the three resources of the default resource set (ResourceTable::legacy()) that most test fixtures use.
inline constexpr ResourceId kGold = 0;
inline constexpr ResourceId kLumber = 1;
inline constexpr ResourceId kOil = 2;
}  // namespace myrts
