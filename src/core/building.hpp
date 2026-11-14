#pragma once

#include <vector>

#include "grid.hpp"

namespace myrts {

/// What workers gather. Gold and oil come from sites on the map (mines, oil patches); lumber from trees.
enum class ResourceKind { Gold, Lumber, Oil };

/// Amounts of each resource (a player's stock, or the price of something).
struct Stock {
  int gold = 0;
  int lumber = 0;
  int oil = 0;

  constexpr bool operator==(const Stock&) const = default;
};
using Cost = Stock;

[[nodiscard]] constexpr bool can_afford(const Stock& have, const Cost& cost) {
  return have.gold >= cost.gold && have.lumber >= cost.lumber && have.oil >= cost.oil;
}
/// Deducts `cost` from `stock`. Returns false (leaving `stock` untouched) if it cannot be paid.
constexpr bool pay(Stock& stock, const Cost& cost) {
  if (!can_afford(stock, cost)) return false;
  stock.gold -= cost.gold;
  stock.lumber -= cost.lumber;
  stock.oil -= cost.oil;
  return true;
}

/// All tiles of a `size` x `size` square whose top-left tile is `origin`.
[[nodiscard]] std::vector<Point> footprint(Point origin, int size);

/// True if every tile of the footprint is inside the map and walkable.
[[nodiscard]] bool can_place(const Grid& grid, Point origin, int size);

/// A gold mine or an oil patch: a square obstacle holding a finite amount of one resource.
class ResourceSite {
 public:
  ResourceSite(Point origin, int size, int amount, ResourceKind kind = ResourceKind::Gold)
      : origin_(origin), size_(size), remaining_(amount), kind_(kind) {}

  [[nodiscard]] ResourceKind kind() const { return kind_; }
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
  ResourceKind kind_;
};

}  // namespace myrts
