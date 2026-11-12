#pragma once

#include <vector>

#include "grid.hpp"

namespace myrts {

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

enum class BuildingKind { GreatHall, PigFarm, Barracks };
inline constexpr BuildingKind kAllBuildingKinds[] = {BuildingKind::GreatHall, BuildingKind::PigFarm,
                                                     BuildingKind::Barracks};

struct BuildingSpec {
  const char* name;
  int size;  // footprint in tiles (square)
  Cost cost;
  double build_seconds;
  bool is_depot;  // accepts delivered gold and lumber
};

[[nodiscard]] constexpr BuildingSpec building_spec(BuildingKind k) {
  switch (k) {
    case BuildingKind::GreatHall: return {"Great Hall", 4, {1200, 800, 0}, 255.0, true};
    case BuildingKind::PigFarm: return {"Pig Farm", 2, {500, 250, 0}, 100.0, false};
    case BuildingKind::Barracks: return {"Barracks", 3, {700, 450, 0}, 200.0, false};
  }
  return {"?", 1, {}, 1.0, false};
}

/// All tiles of a `size` x `size` square whose top-left tile is `origin`.
[[nodiscard]] std::vector<Point> footprint(Point origin, int size);

/// True if every tile of the footprint is inside the map and walkable.
[[nodiscard]] bool can_place(const Grid& grid, Point origin, int size);

/// A gold mine: a square obstacle holding a finite amount of gold.
class GoldMine {
 public:
  GoldMine(Point origin, int size, int gold) : origin_(origin), size_(size), remaining_(gold) {}

  [[nodiscard]] Point origin() const { return origin_; }
  [[nodiscard]] int size() const { return size_; }
  [[nodiscard]] int remaining() const { return remaining_; }
  [[nodiscard]] bool depleted() const { return remaining_ <= 0; }
  [[nodiscard]] std::vector<Point> tiles() const { return footprint(origin_, size_); }
  /// Removes up to `amount` gold and returns what was taken.
  int take(int amount);

 private:
  Point origin_;
  int size_;
  int remaining_;
};

}  // namespace myrts
