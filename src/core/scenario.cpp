#include "scenario.hpp"

#include <algorithm>
#include <cmath>
#include <map>

namespace myrts {

namespace {
constexpr int kGoldPerValueUnit = 2500;  // a mine's value in the map is a multiple of this
constexpr int kDefaultMineGold = 10000;
}  // namespace

std::optional<std::size_t> Scenario::farthest_from(std::size_t from) const {
  if (from >= starts.size() || starts.size() < 2) return std::nullopt;
  std::optional<std::size_t> best;
  double best_d = -1;
  for (std::size_t i = 0; i < starts.size(); ++i) {
    if (i == from) continue;
    const double d = std::hypot(static_cast<double>(starts[i].start.x - starts[from].start.x),
                                static_cast<double>(starts[i].start.y - starts[from].start.y));
    if (d > best_d) {
      best_d = d;
      best = i;
    }
  }
  return best;
}

Scenario scenario_from(const PudMap& pud) {
  Scenario s;

  std::map<int, StartSlot> by_owner;
  for (const PudUnit& u : pud.units) {
    if (u.type == pud_unit::kHumanStart || u.type == pud_unit::kOrcStart)
      by_owner[u.owner].start = {u.x, u.y}, by_owner[u.owner].owner = u.owner;
  }
  for (const PudUnit& u : pud.units) {
    if (u.type != pud_unit::kTownHall && u.type != pud_unit::kGreatHall) continue;
    if (const auto it = by_owner.find(u.owner); it != by_owner.end() && !it->second.hall) it->second.hall = Point{u.x, u.y};
  }
  for (auto& [_, slot] : by_owner) s.starts.push_back(slot);

  for (const PudUnit& u : pud.units)
    if (u.type == pud_unit::kGoldMine)
      s.mines.push_back({{u.x, u.y}, u.value > 0 ? u.value * kGoldPerValueUnit : kDefaultMineGold});
  return s;
}

}  // namespace myrts
