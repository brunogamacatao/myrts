#include <algorithm>
#include <nlohmann/json.hpp>
#include <unordered_map>

#include "ai.hpp"
#include "world.hpp"

namespace myrts {

namespace {

using json = nlohmann::json;

json point(Point p) { return json::array({p.x, p.y}); }
Point to_point(const json& j) { return {j.at(0).get<int>(), j.at(1).get<int>()}; }

}  // namespace

json Ai::save_state() const { return {{"timer", timer_}, {"elapsed", elapsed_}, {"waves", waves_}}; }

void Ai::load_state(const json& state) {
  timer_ = state.value("timer", 0.0);
  elapsed_ = state.value("elapsed", 0.0);
  waves_ = state.value("waves", 0);
}

json World::save_state() const {
  json out;
  out["width"] = width();
  out["height"] = height();

  json players = json::array();
  for (std::size_t i = 0; i < players_.size(); ++i) {
    const Player& p = players_[i];
    players.push_back({{"stock", {{"gold", p.stock.gold}, {"lumber", p.stock.lumber}, {"oil", p.stock.oil}}},
                       {"fog", i < fogs_.size() ? fogs_[i].encode() : std::string{}}});
  }
  out["players"] = std::move(players);

  json trees = json::array();
  for (const Point p : trees_.positions()) trees.push_back({p.x, p.y, trees_.remaining(p)});
  out["trees"] = std::move(trees);

  json mines = json::array();
  for (const Mine& m : mines_) mines.push_back({{"remaining", m.site.remaining()}, {"busy", m.busy}});
  out["mines"] = std::move(mines);

  // Entities refer to each other by their position in these arrays.
  std::unordered_map<EntityId, int, EntityIdHash> building_index, unit_index;
  json buildings = json::array();
  buildings_.for_each([&](const Building& b) {
    building_index[b.id] = static_cast<int>(buildings.size());
    json queue = json::array();
    for (const TrainItem& t : b.queue) queue.push_back({{"unit", t.unit->id}, {"progress", t.progress}});
    buildings.push_back({{"def", b.def->id}, {"owner", b.owner}, {"origin", point(b.origin)}, {"complete", b.complete},
                         {"progress", b.progress}, {"hp", b.hp}, {"queue", std::move(queue)},
                         {"rally", b.rally ? point(*b.rally) : json(nullptr)}});
  });
  out["buildings"] = std::move(buildings);

  int n = 0;
  units_.for_each([&](const Unit& u) { unit_index[u.id] = n++; });
  json units = json::array();
  units_.for_each([&](const Unit& u) {
    json o;
    if (const auto* h = std::get_if<order::Harvest>(&u.order)) {
      o = {{"t", "harvest"}, {"kind", static_cast<int>(h->kind)}, {"target", point(h->target)}, {"state", static_cast<int>(h->state)}};
    } else if (const auto* b = std::get_if<order::Build>(&u.order)) {
      const auto it = building_index.find(b->building);
      o = it == building_index.end() ? json{{"t", "idle"}} : json{{"t", "build"}, {"building", it->second}};
    } else if (const auto* a = std::get_if<order::Attack>(&u.order)) {
      const bool is_unit = a->target.kind == order::Target::Kind::Unit;
      const auto& index = is_unit ? unit_index : building_index;
      const auto it = index.find(a->target.id);
      o = it == index.end() ? json{{"t", "idle"}}
                            : json{{"t", "attack"}, {"unit", is_unit}, {"id", it->second},
                                   {"resume", a->resume ? point(*a->resume) : json(nullptr)}};
    } else if (const auto* rp = std::get_if<order::Repair>(&u.order)) {
      const bool is_unit = rp->target.kind == order::Target::Kind::Unit;
      const auto& index = is_unit ? unit_index : building_index;
      const auto it = index.find(rp->target.id);
      o = it == index.end() ? json{{"t", "idle"}} : json{{"t", "repair"}, {"unit", is_unit}, {"id", it->second}};
    } else if (const auto* m = std::get_if<order::AttackMove>(&u.order)) {
      o = {{"t", "amove"}, {"dest", point(m->dest)}};
    } else {
      o = {{"t", "idle"}};
      if (u.is_moving()) o["goal"] = point(u.path.empty() ? u.anchor() : u.goal);
    }
    units.push_back({{"def", u.def->id}, {"owner", u.owner}, {"tile", point(u.anchor())}, {"hp", u.hp},
                     {"facing", static_cast<int>(u.facing)}, {"cargo", static_cast<int>(u.cargo)},
                     {"carrying", u.carrying}, {"cooldown", u.cooldown}, {"order", std::move(o)},
                     {"aboard", u.aboard.valid() && unit_index.contains(u.aboard) ? unit_index.at(u.aboard) : -1}});
  });
  out["units"] = std::move(units);
  return out;
}

std::expected<void, std::string> World::load_state(const json& state, const Registry& registry) {
  try {
    if (state.at("width").get<int>() != width() || state.at("height").get<int>() != height())
      return std::unexpected("the saved game is for a map of another size");
    const json& players = state.at("players");
    if (players.size() != players_.size()) return std::unexpected("the saved game has a different number of players");
    for (const json& b : state.at("buildings"))
      if (!registry.building(b.at("def").get<std::string>())) return std::unexpected("unknown building '" + b.at("def").get<std::string>() + "'");
    for (const json& u : state.at("units"))
      if (!registry.unit(u.at("def").get<std::string>())) return std::unexpected("unknown unit '" + u.at("def").get<std::string>() + "'");

    // ---- start from an empty board
    for (const EntityId id : buildings_.ids()) {
      const Building* b = buildings_.get(id);
      for (const Point p : footprint(b->origin, b->def->size)) layer_of(*b->def).blocked.set_blocked(p, false);
      buildings_.erase(id);
    }
    for (const EntityId id : units_.ids()) {
      const Unit* u = units_.get(id);
      layer_of(*u).occupancy.release(u->tile, occ_id(*u));
      layer_of(*u).occupancy.release(u->next, occ_id(*u));
      units_.erase(id);
    }

    // ---- players
    for (std::size_t i = 0; i < players_.size(); ++i) {
      const json& s = players[i].at("stock");
      players_[i].stock = Stock{s.value("gold", 0), s.value("lumber", 0), s.value("oil", 0)};
      if (players[i].contains("fog") && i < fogs_.size()) fogs_[i].decode(players[i].at("fog").get<std::string>());
    }

    // ---- trees: those that are gone are felled again (which redraws the forest edges), the rest keep their wood
    std::vector<Point> remaining;
    for (const json& t : state.at("trees")) remaining.push_back({t.at(0).get<int>(), t.at(1).get<int>()});
    for (const Point p : trees_.positions())
      if (std::ranges::find(remaining, p) == remaining.end() && trees_.has_tree(p)) {
        trees_.remove(p);
        fell_tree(p);
      }
    for (const json& t : state.at("trees"))
      if (trees_.has_tree({t.at(0).get<int>(), t.at(1).get<int>()}))
        trees_.add({t.at(0).get<int>(), t.at(1).get<int>()}, t.at(2).get<int>());

    // ---- mines
    const json& mines = state.at("mines");
    for (std::size_t i = 0; i < mines_.size() && i < mines.size(); ++i) {
      mines_[i].site.set_remaining(mines[i].at("remaining").get<int>());
      mines_[i].busy = false;
      if (mines_[i].site.depleted()) exhaust_mine(static_cast<int>(i));
    }

    // ---- buildings
    std::vector<EntityId> building_ids;
    for (const json& j : state.at("buildings")) {
      const BuildingDef& def = *registry.building(j.at("def").get<std::string>());
      const EntityId id = add_building(def, j.at("owner").get<int>(), to_point(j.at("origin")), j.at("complete").get<bool>());
      building_ids.push_back(id);
      Building* b = buildings_.get(id);
      b->progress = j.at("progress").get<double>();
      b->hp = j.at("hp").get<int>();
      for (const json& q : j.at("queue"))
        if (const UnitDef* ud = registry.unit(q.at("unit").get<std::string>())) b->queue.push_back({ud, q.at("progress").get<double>()});
      if (!j.at("rally").is_null()) b->rally = to_point(j.at("rally"));
    }

    // ---- units
    // Units on the map first, then the ones aboard a ship (they need their ship to exist).
    const json& saved_units = state.at("units");
    std::vector<EntityId> unit_ids(saved_units.size());
    for (int pass = 0; pass < 2; ++pass)
    for (std::size_t k = 0; k < saved_units.size(); ++k) {
      const json& j = saved_units[k];
      const int ship_index = j.value("aboard", -1);
      if ((ship_index >= 0) != (pass == 1)) continue;
      const UnitDef& def = *registry.unit(j.at("def").get<std::string>());
      const EntityId id = ship_index >= 0 ? (static_cast<std::size_t>(ship_index) < unit_ids.size() ? spawn_aboard(def, j.at("owner").get<int>(), unit_ids[static_cast<std::size_t>(ship_index)]) : kNoEntity)
                                          : spawn_unit(def, j.at("owner").get<int>(), to_point(j.at("tile")));
      unit_ids[k] = id;
      if (Unit* u = units_.get(id)) {
        u->hp = j.at("hp").get<int>();
        u->facing = static_cast<Facing>(j.at("facing").get<int>());
        u->cargo = static_cast<Cargo>(j.at("cargo").get<int>());
        u->carrying = j.at("carrying").get<int>();
        u->cooldown = j.at("cooldown").get<double>();
      }
    }

    // ---- what the units were doing: routes are planned again from where they stand
    for (std::size_t i = 0; i < unit_ids.size(); ++i) {
      Unit* u = units_.get(unit_ids[i]);
      if (!u) continue;
      const json& o = state.at("units")[i].at("order");
      const std::string t = o.at("t").get<std::string>();
      if (t == "harvest") {
        const auto kind = static_cast<ResourceKind>(o.at("kind").get<int>());
        const auto leg = static_cast<order::Harvest::State>(o.at("state").get<int>());
        const Point target = to_point(o.at("target"));
        u->order = order::Harvest{order::Harvest::State::ToResource, kind, target, -1, 0.0};  // send_to_depot() changes it
        if (leg == order::Harvest::State::ToDepot && u->carrying > 0 && send_to_depot(*u)) continue;  // back to the depot
        if (!start_gathering(*u, kind, target)) start_gathering_nearest(*u, kind);
      } else if (t == "build") {
        const int index = o.at("building").get<int>();
        if (index >= 0 && static_cast<std::size_t>(index) < building_ids.size()) start_building(*u, building_ids[static_cast<std::size_t>(index)]);
      } else if (t == "attack") {
        const bool is_unit = o.at("unit").get<bool>();
        const int index = o.at("id").get<int>();
        const auto& ids = is_unit ? unit_ids : building_ids;
        if (index >= 0 && static_cast<std::size_t>(index) < ids.size() && ids[static_cast<std::size_t>(index)].valid()) {
          std::optional<Point> resume;
          if (!o.at("resume").is_null()) resume = to_point(o.at("resume"));
          begin_attack(*u, {is_unit ? order::Target::Kind::Unit : order::Target::Kind::Building, ids[static_cast<std::size_t>(index)]}, resume);
        }
      } else if (t == "repair") {
        const bool is_unit = o.at("unit").get<bool>();
        const int index = o.at("id").get<int>();
        const auto& ids = is_unit ? unit_ids : building_ids;
        if (index >= 0 && static_cast<std::size_t>(index) < ids.size() && ids[static_cast<std::size_t>(index)].valid())
          start_repair(*u, {is_unit ? order::Target::Kind::Unit : order::Target::Kind::Building, ids[static_cast<std::size_t>(index)]});
      } else if (t == "amove") {
        const std::array one{u->id};
        order_attack_move(one, to_point(o.at("dest")));
      } else if (o.contains("goal")) {
        const std::array one{u->id};
        order_move(one, to_point(o.at("goal")));
      }
    }
  } catch (const std::exception& e) {
    return std::unexpected(std::string("invalid saved game: ") + e.what());
  }
  return {};
}

}  // namespace myrts
