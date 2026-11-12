#pragma once

#include "defs.hpp"
#include "world.hpp"

namespace myrts {

struct AiConfig {
  double think_interval = 1.0;  // seconds between decisions
  int desired_workers = 3;
  int first_wave = 4;           // fighters needed for the first attack
  int wave_growth = 2;          // extra fighters needed for each later attack
};

/// A computer-controlled player. It only uses the same public orders a human player has, so it
/// never cheats the rules (it does see the whole map).
///
/// Policy: keep a few workers mining, train whatever fighters its buildings offer, and when enough
/// fighters have gathered send them all, attack-moving, to the nearest enemy building.
class Ai {
 public:
  Ai(World& world, const Registry& registry, PlayerId player, AiConfig config = {})
      : world_(world), registry_(registry), me_(player), cfg_(config) {}

  void update(double dt);
  [[nodiscard]] int waves_sent() const { return waves_; }

 private:
  void think();
  void manage_workers();
  void manage_production();
  void manage_army();

  World& world_;
  const Registry& registry_;
  PlayerId me_;
  AiConfig cfg_;
  double timer_ = 0.0;
  int waves_ = 0;
};

}  // namespace myrts
