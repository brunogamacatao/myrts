#include "rules.hpp"

#include <algorithm>

namespace myrts {

namespace {

int count_buildings(const World& world, PlayerId owner, const std::string& def_id, bool finished_only) {
  int n = 0;
  world.buildings().for_each([&](const Building& b) {
    n += (b.owner == owner && b.def->id == def_id && (!finished_only || b.complete)) ? 1 : 0;
  });
  return n;
}

bool opponents_standing(const World& world, PlayerId local) {
  for (PlayerId p = 0; p < static_cast<PlayerId>(world.player_count()); ++p)
    if (p != local && world.has_assets(p)) return true;
  return false;
}

bool achieved(const World& world, PlayerId local, const Objective& o) {
  if (o.type == "destroy_all") return !opponents_standing(world, local);
  if (o.type == "destroy_building") {  // no opponent owns one any more
    for (PlayerId p = 0; p < static_cast<PlayerId>(world.player_count()); ++p)
      if (p != local && count_buildings(world, p, o.building, false) > 0) return false;
    return true;
  }
  if (o.type == "survive") return world.elapsed() >= o.seconds;
  if (o.type == "collect") {
    const ResourceId r = world.resources().index(o.resource);
    return r != kNoResource && world.player(local).stock[r] >= o.amount;
  }
  if (o.type == "build") return count_buildings(world, local, o.building, true) >= o.amount;
  return false;
}

bool failed(const World& world, PlayerId local, const Objective& o) {
  if (o.type == "lose_building") return count_buildings(world, local, o.building, false) == 0;
  if (o.type == "time_limit") return world.elapsed() >= o.seconds;
  return false;
}

}  // namespace

std::optional<Outcome> evaluate_outcome(const World& world, PlayerId local, const Objectives& objectives) {
  if (!world.has_assets(local)) return Outcome::Defeat;

  bool has_opponents = false;
  for (PlayerId p = 0; p < static_cast<PlayerId>(world.player_count()); ++p) has_opponents = has_opponents || p != local;
  bool won = false;
  if (objectives.victory.empty()) {
    won = has_opponents && !opponents_standing(world, local);
  } else {
    won = std::ranges::all_of(objectives.victory, [&](const Objective& o) { return achieved(world, local, o); });
  }
  if (won) return Outcome::Victory;  // winning on the very tick a time limit runs out still wins

  for (const Objective& o : objectives.defeat)
    if (failed(world, local, o)) return Outcome::Defeat;
  return std::nullopt;
}

}  // namespace myrts
