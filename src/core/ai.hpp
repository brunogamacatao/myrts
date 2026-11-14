#pragma once

#include <nlohmann/json_fwd.hpp>

#include "defs.hpp"
#include "world.hpp"

namespace myrts {

struct AiConfig {
  double think_interval = 1.0;  // seconds between decisions
  int desired_workers = 3;
  int desired_tankers = 2;      // water workers pumping oil (only when the map has oil and a shipyard can be built)
  int first_wave = 5;           // fighters needed for the first attack
  int wave_growth = 3;          // extra fighters needed for each later attack
  double start_delay = 150.0;   // no attack before this many seconds have passed
  int farm_margin = 2;          // build a farm when this little food is left
};

/// A computer-controlled player. It only uses the same public orders a human player has, so it
/// never cheats the rules (it does see the whole map).
///
/// Policy: keep a few workers mining (one cuts wood when a farm is needed), train whatever fighters
/// its buildings offer, build a farm whenever it is about to run out of food, and when enough
/// fighters have gathered send them all, attack-moving, to the nearest enemy building.
class Ai {
 public:
  Ai(World& world, const Registry& registry, PlayerId player, AiConfig config = {})
      : world_(world), registry_(registry), me_(player), cfg_(config) {}

  /// Advances the AI's clock; it makes decisions every AiConfig::think_interval seconds.
  void update(double dt);
  /// How many attack waves have been launched so far.
  [[nodiscard]] int waves_sent() const { return waves_; }
  /// The AI's clock and wave count, for saved games (the rest is decided afresh from the world each time it thinks).
  [[nodiscard]] nlohmann::json save_state() const;
  void load_state(const nlohmann::json& state);

 private:
  void think();                ///< runs the four managers below, in order
  void manage_workers();       ///< keep AiConfig::desired_workers mining (one on wood if a farm is needed)
  void manage_tankers();       ///< water workers pump oil patches and deliver to a shipyard
  void manage_construction();  ///< build a farm when food is nearly used up, a shipyard when there is oil to pump
  void manage_production();    ///< queue whatever fighters the buildings can train
  void manage_army();          ///< send a wave, attack-moving, once enough fighters have gathered

  World& world_;
  const Registry& registry_;
  PlayerId me_;
  AiConfig cfg_;
  double timer_ = 0.0;
  double elapsed_ = 0.0;
  int waves_ = 0;
};

}  // namespace myrts
