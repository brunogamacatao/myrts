#pragma once

#include <span>
#include <vector>

#include "grid.hpp"

namespace myrts {

/// Tracks which unit holds each tile so units never share a tile.
/// A moving unit reserves its current tile and the tile it is heading to.
class Occupancy {
 public:
  using Id = int;
  static constexpr Id kNone = -1;

  Occupancy() = default;
  Occupancy(int width, int height) : grid_(width, height), owners_(grid_.empty() ? 0 : cells(), kNone) {}

  [[nodiscard]] Id owner(Point p) const noexcept { return grid_.contains(p) ? owners_[grid_.index(p)] : kNone; }

  /// True if `p` is inside the map and free or already held by `id`.
  [[nodiscard]] bool can_reserve(Point p, Id id) const noexcept {
    return grid_.contains(p) && (owner(p) == kNone || owner(p) == id);
  }

  /// Reserves `p` for `id`; returns false if another unit holds it.
  bool try_reserve(Point p, Id id) noexcept {
    if (!can_reserve(p, id)) return false;
    owners_[grid_.index(p)] = id;
    return true;
  }

  /// Releases `p` only if it is held by `id`.
  void release(Point p, Id id) noexcept {
    if (grid_.contains(p) && owner(p) == id) owners_[grid_.index(p)] = kNone;
  }

  /// Grid with every tile held by a unit *not* listed in `ignored` marked as blocked.
  [[nodiscard]] Grid blocked_except(std::span<const Id> ignored) const {
    Grid blocked(grid_.width(), grid_.height());
    for (int y = 0; y < grid_.height(); ++y) {
      for (int x = 0; x < grid_.width(); ++x) {
        const Id o = owner({x, y});
        if (o == kNone) continue;
        bool skip = false;
        for (const Id i : ignored) skip = skip || i == o;
        if (!skip) blocked.set_blocked({x, y});
      }
    }
    return blocked;
  }

  [[nodiscard]] Grid blocked_for(Id self) const { return blocked_except(std::span<const Id>(&self, 1)); }

 private:
  [[nodiscard]] std::size_t cells() const noexcept {
    return static_cast<std::size_t>(grid_.width()) * static_cast<std::size_t>(grid_.height());
  }

  Grid grid_;  // only used for dimensions/indexing
  std::vector<Id> owners_;
};

}  // namespace myrts
