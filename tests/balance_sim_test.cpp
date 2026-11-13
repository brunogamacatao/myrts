// Balance probes, not regression tests: they are hidden from the normal run and print tables.
//   build/tests-only/myrts_tests "[balance]"
// The "player" here is an Ai with a scripted build order (barracks first), a stand-in for a competent human. Real players
// micro-manage better than an Ai, so read the numbers as a floor for the player, not an exact prediction.
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <filesystem>

#include "core/match_setup.hpp"

using namespace myrts;
namespace fs = std::filesystem;

namespace {

struct Data {
  fs::path assets = fs::path(MYRTS_SOURCE_DIR) / "assets";
  Registry registry = *Registry::load_directory((assets / "data").string());
  Campaign campaign = *Campaign::load_file((assets / "data" / "campaign.json").string());
  std::shared_ptr<const TileSets> tilesets =
      std::make_shared<const TileSets>(*TileSets::load_file((assets / "data" / "tilesets.json").string()));
};

struct Census {
  int workers = 0, fighters = 0, hp = 0;
};

Census census(const World& w, PlayerId p) {
  Census c;
  w.units().for_each([&](const Unit& u) {
    if (u.owner != p) return;
    if (u.def->worker) ++c.workers;
    else if (u.def->can_attack()) {
      ++c.fighters;
      c.hp += u.hp;
    }
  });
  return c;
}

std::optional<Point> spot_near(const World& w, const BuildingDef& def, Point near) {
  std::optional<Point> best;
  double best_d = 1e9;
  for (int y = near.y - 14; y <= near.y + 14; ++y)
    for (int x = near.x - 14; x <= near.x + 14; ++x) {
      if (w.placement_status(def, {x, y}) != PlaceStatus::Ok) continue;
      if (!can_place(w.grid(MoveClass::Land), {x - 1, y - 1}, def.size + 2)) continue;
      const double d = std::hypot(x - near.x, y - near.y);
      if (d < best_d) best_d = d, best = Point{x, y};
    }
  return best;
}

/// A player's build order: the production buildings it must pay for and wait for (the enemy AI starts with them built).
struct BuildOrder {
  std::vector<std::string> buildings;
  std::size_t next = 0;
  void update(World& w, const Registry& r, const Match& m) {
    if (next >= buildings.size()) return;
    bool constructing = false;
    w.buildings().for_each([&](const Building& b) { constructing = constructing || (b.owner == m.local && !b.complete); });
    if (constructing) return;
    const BuildingDef* def = r.building(buildings[next]);
    if (!def || !can_afford(w.player(m.local).stock, def->cost)) return;
    EntityId builder;
    w.units().for_each([&](const Unit& u) {
      if (u.owner == m.local && u.def->worker && !u.locked() && !builder.valid()) builder = u.id;
    });
    if (!builder.valid()) return;
    if (const auto at = spot_near(w, *def, m.local_hall)) {
      if (w.order_build(m.local, std::array{builder}, *def, *at).ok) ++next;
    }
  }
};

struct Outcome {
  enum class Result { Win, Loss, Timeout } result = Result::Timeout;
  double seconds = 0;
  double first_hit = -1;  ///< when the first player building took damage
  int enemy_fighters_at_first_hit = 0;
  Census player_end, enemy_end;
};

struct Setup {
  Level level;
  AiConfig player_ai;        ///< how the stand-in human plays
  bool player_attacks = true;
  std::vector<std::string> build_order{"barracks", "barracks", "pig_farm"};
  double max_seconds = 2400;
};

Outcome simulate(const Data& d, const Setup& s, bool print_timeline = false) {
  MatchOptions options;
  options.assets_dir = d.assets.string();
  auto match = build_match(d.registry, d.tilesets, s.level, options);
  REQUIRE(match.has_value());
  World& w = *match->world;
  Ai player(w, d.registry, match->local, s.player_ai);
  BuildOrder order{s.build_order};

  Outcome out;
  int hp_before = -1;
  const Point hall = match->local_hall;
  for (double t = 0; t < s.max_seconds; t += 0.2) {
    order.update(w, d.registry, *match);
    if (s.player_attacks) player.update(0.2);
    for (auto& ai : match->ais) ai->update(0.2);
    w.update(0.2);
    if (out.first_hit < 0) {
      bool hit = false;
      w.buildings().for_each([&](const Building& b) { hit = hit || (b.owner == match->local && b.hp < b.def->hp); });
      if (hit) {
        out.first_hit = t;
        out.enemy_fighters_at_first_hit = census(w, match->enemy).fighters;
      }
    }
    if (print_timeline && std::fmod(t, 120.0) < 0.2) {
      const Census p = census(w, match->local), e = census(w, match->enemy);
      std::printf("   t=%4.0f  player: %2d workers %2d fighters | enemy: %2d workers %2d fighters  (gold %d/%d)\n", t,
                  p.workers, p.fighters, e.workers, e.fighters, w.player(match->local).stock.gold,
                  w.player(match->enemy).stock.gold);
    }
    const bool me = w.has_assets(match->local), foe = w.has_assets(match->enemy);
    if (!me || !foe) {
      out.result = foe ? Outcome::Result::Loss : Outcome::Result::Win;
      out.seconds = t;
      break;
    }
    out.seconds = t;
  }
  (void)hp_before; (void)hall;
  out.player_end = census(w, match->local);
  out.enemy_end = census(w, match->enemy);
  return out;
}

const char* name(Outcome::Result r) {
  return r == Outcome::Result::Win ? "WIN " : r == Outcome::Result::Loss ? "LOSS" : "----";
}

}  // namespace

// What does the enemy do if nobody defends? When does its first wave arrive and how big is it?
TEST_CASE("balance: when the first wave arrives and how big it is, per level", "[.balance]") {
  const Data d;
  std::printf("\n%-24s %6s %6s | %8s %10s %12s\n", "level", "delay", "wave", "first hit", "enemy army", "(passive player)");
  for (const Level& level : d.campaign.levels()) {
    Setup s;
    s.level = level;
    s.player_attacks = false;
    s.build_order = {};
    s.max_seconds = level.ai.start_delay + 600;
    const Outcome o = simulate(d, s);
    std::printf("%-24s %6.0f %6d | %8.0f %10d\n", level.name.c_str(), level.ai.start_delay, level.ai.first_wave, o.first_hit,
                o.enemy_fighters_at_first_hit);
  }
}

// A stand-in human (8 workers, builds barracks, waits until T and then attacks) against each level.
TEST_CASE("balance: a scripted player against every level", "[.balance]") {
  const Data d;
  std::printf("\n%-24s %6s | %s\n", "level", "delay", "player attacks at 300 s / 600 s / 900 s: result (seconds, fighters left p/e)");
  for (const Level& level : d.campaign.levels()) {
    std::printf("%-24s %6.0f |", level.name.c_str(), level.ai.start_delay);
    for (const double attack : {300.0, 600.0, 900.0}) {
      Setup s;
      s.level = level;
      s.player_ai.desired_workers = 8;
      s.player_ai.start_delay = attack;
      s.player_ai.first_wave = 8;
      s.player_ai.wave_growth = 4;
      const Outcome o = simulate(d, s);
      std::printf("  %s (%4.0f, %2d/%2d)", name(o.result), o.seconds, o.player_end.fighters, o.enemy_end.fighters);
    }
    std::printf("\n");
  }
}

TEST_CASE("balance: timeline of one level", "[.balance]") {
  const Data d;
  Setup s;
  s.level = d.campaign.level(4);
  s.player_ai.desired_workers = 8;
  s.player_ai.start_delay = 600;
  s.player_ai.first_wave = 8;
  const Outcome o = simulate(d, s, true);
  std::printf("-> %s at %.0f s\n", name(o.result), o.seconds);
}
