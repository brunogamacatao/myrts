#pragma once

#include <vector>

#include "grid.hpp"
#include "resources.hpp"

namespace myrts {

/// All tiles of a `size` x `size` square whose top-left tile is `origin`.
[[nodiscard]] std::vector<Point> footprint(Point origin, int size);

/// True if every tile of the footprint is inside the map and walkable.
[[nodiscard]] bool can_place(const Grid& grid, Point origin, int size);

/// A mine or patch: a square obstacle holding a finite amount of one resource.
class ResourceSite {
 public:
  /// `kind` defaults to the first resource of the game.
  ResourceSite(Point origin, int size, int amount, ResourceId kind = 0)
      : origin_(origin), size_(size), remaining_(amount), kind_(kind) {}

  [[nodiscard]] ResourceId kind() const { return kind_; }
  [[nodiscard]] Point origin() const { return origin_; }
  [[nodiscard]] int size() const { return size_; }
  [[nodiscard]] int remaining() const { return remaining_; }
  [[nodiscard]] bool depleted() const { return remaining_ <= 0; }
  [[nodiscard]] std::vector<Point> tiles() const { return footprint(origin_, size_); }
  /// Removes up to `amount` and returns what was taken.
  int take(int amount);
  /// Sets what is left (restoring a saved game).
  void set_remaining(int amount) { remaining_ = amount; }

 private:
  Point origin_;
  int size_;
  int remaining_;
  ResourceId kind_;
};

}  // namespace myrts
