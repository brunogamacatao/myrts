#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace myrts {

/// Stable handle to a unit or building. The generation makes handles to destroyed
/// entities invalid even after their slot is reused.
struct EntityId {
  static constexpr std::uint32_t kInvalid = 0xFFFFFFFFu;

  std::uint32_t index = kInvalid;
  std::uint32_t generation = 0;

  [[nodiscard]] constexpr bool valid() const { return index != kInvalid; }
  constexpr auto operator<=>(const EntityId&) const = default;
};
/// The invalid handle: refers to nothing.
inline constexpr EntityId kNoEntity{};

struct EntityIdHash {
  std::size_t operator()(const EntityId& id) const noexcept {
    return std::hash<std::uint64_t>{}((static_cast<std::uint64_t>(id.generation) << 32) | id.index);
  }
};

/// Index of a player in World (0, 1, ...).
using PlayerId = int;
inline constexpr PlayerId kNoPlayer = -1;

}  // namespace myrts
