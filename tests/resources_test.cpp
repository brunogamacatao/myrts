#include <catch2/catch_test_macros.hpp>
#include <array>
#include <nlohmann/json.hpp>

#include "core/world.hpp"

using namespace myrts;

namespace {

// A game that knows none of gold, lumber or oil: crystal (a land mine, not entered), timber (trees) and ichor (water).
constexpr const char* kResources = R"({"resources":[
  {"id":"crystal","name":"Crystal","short_label":"c","icon":"ui/c.png","source":"site","site_terrain":"land",
   "carry_animation":"crystal","pud_source":"gold_mine","no_depot_message":"Build a Refinery","depleted_message":"The crystals are gone"},
  {"id":"timber","name":"Timber","source":"tree"},
  {"id":"ichor","name":"Ichor","source":"site","site_terrain":"water","pud_source":"oil_patch"}]})";
constexpr const char* kUnits = R"({"units":[
  {"id":"miner","name":"Miner","hp":30,"move_speed":96,"sprite":"s","cost":{"crystal":50,"timber":5},
   "worker":{"gather":{"crystal":{"per_trip":40,"seconds":1.0},"timber":{"per_trip":10,"seconds":1.0}}},"builds":["refinery"]},
  {"id":"skiff","name":"Skiff","hp":30,"move_speed":96,"move_class":"water","sprite":"s",
   "worker":{"gather":{"ichor":{"per_trip":50,"seconds":1.0}}}}]})";
constexpr const char* kBuildings = R"({"buildings":[
  {"id":"refinery","name":"Refinery","size":3,"cost":{"crystal":300},"build_seconds":5,"accepts":["crystal","timber"],"sprite":"r"}]})";

const Registry& registry() {
  static const Registry r = [] {
    auto parsed = Registry::parse(kUnits, kBuildings, R"({"commands":[]})", {}, kResources);
    if (!parsed) FAIL("test data is invalid: " << parsed.error());
    return std::move(*parsed);
  }();
  return r;
}

struct Fixture {
  static WorldConfig config() {
    WorldConfig c;
    c.resources = registry().resources();
    return c;
  }
  World world{config(), Grid(24, 14)};
  const ResourceTable& table = *registry().resources();
  const ResourceId crystal = table.index("crystal"), timber = table.index("timber"), ichor = table.index("ichor");
  PlayerId me = world.add_player("me", "x", *table.stock_from({{"crystal", 500}}));
  const UnitDef& miner = *registry().unit("miner");
  const BuildingDef& refinery = *registry().building("refinery");
  void run(double seconds) {
    for (double t = 0; t < seconds; t += 0.05) world.update(0.05);
  }
};

}  // namespace

TEST_CASE("a game defines its own resources, in order, with generic messages as fallback", "[resources]") {
  const ResourceTable& t = *registry().resources();
  REQUIRE(t.size() == 3);
  CHECK(t.index("crystal") == 0);
  CHECK(t.index("gold") == kNoResource);
  CHECK(t.tree_resource() == t.index("timber"));
  CHECK(t.first_site_resource(SiteTerrain::Water) == t.index("ichor"));
  CHECK(t.from_pud("gold_mine") == t.index("crystal"));  // a Warcraft map's gold mine holds this game's crystal
  CHECK(t.no_depot_message(0) == "Build a Refinery");
  CHECK(t.unreachable_message(1) == "Cannot reach the Timber");  // generated from the name
}

TEST_CASE("resource tables reject mistakes", "[resources]") {
  CHECK_FALSE(ResourceTable::parse(R"({"resources":[]})"));
  CHECK_FALSE(ResourceTable::parse(R"({"resources":[{"id":"a"},{"id":"a"}]})"));
  CHECK_FALSE(ResourceTable::parse(R"({"resources":[{"id":"a","source":"tree"},{"id":"b","source":"tree"}]})"));
  CHECK_FALSE(ResourceTable::parse(R"({"resources":[{"id":"a","source":"sky"}]})"));
  CHECK_FALSE(ResourceTable::parse(R"({"resources":[{"id":"a","site_terrain":"lava"}]})"));
  std::string many = R"({"resources":[)";
  for (int i = 0; i <= kMaxResources; ++i) many += std::string(i ? "," : "") + R"({"id":"r)" + std::to_string(i) + "\"}";
  CHECK_FALSE(ResourceTable::parse(many + "]}"));
}

TEST_CASE("prices, depots and gatherers can only name resources the game has", "[resources]") {
  const auto with = [](const char* units, const char* buildings) {
    return Registry::parse(units, buildings, R"({"commands":[]})", {}, kResources);
  };
  CHECK(with(kUnits, kBuildings));
  CHECK_FALSE(with(R"({"units":[{"id":"u","name":"U","hp":1,"sprite":"s","cost":{"gold":5}}]})", kBuildings));
  CHECK_FALSE(with(kUnits, R"({"buildings":[{"id":"b","name":"B","size":1,"build_seconds":1,"sprite":"s","accepts":["gold"]}]})"));
  CHECK_FALSE(with(R"({"units":[{"id":"u","name":"U","hp":1,"sprite":"s","worker":{"gather":{"gold":{}}}}]})", kBuildings));
  const auto cost = registry().unit("miner")->cost;
  CHECK(cost[0] == 50);
  CHECK(cost[1] == 5);
}

TEST_CASE("workers mine, chop and deliver the game's own resources", "[resources]") {
  Fixture f;
  f.world.add_building(f.refinery, f.me, {1, 1}, true);
  f.world.add_mine({15, 6}, 3, 80, f.crystal);  // two trips of 40
  f.world.add_tree({12, 10}, 10);
  const EntityId u = f.world.spawn_unit(f.miner, f.me, {8, 6});
  const std::array ids{u};

  REQUIRE(f.world.order_harvest(ids, {16, 7}).ok);
  f.run(60);
  CHECK(f.world.player(f.me).stock[f.crystal] == 500 + 80);
  CHECK(f.world.mines()[0].site.depleted());
  CHECK(f.world.player(f.me).stock[f.timber] == 0);

  REQUIRE(f.world.order_harvest(ids, {12, 10}).ok);
  f.run(40);
  CHECK(f.world.player(f.me).stock[f.timber] == 10);
}

TEST_CASE("a worker only gathers what it is able to, and the depot must accept it", "[resources]") {
  Fixture f;
  f.world.add_mine({15, 6}, 3, 80, f.crystal);
  const EntityId u = f.world.spawn_unit(f.miner, f.me, {8, 6});
  const std::array ids{u};
  const Result no_depot = f.world.order_harvest(ids, {16, 7});
  REQUIRE_FALSE(no_depot.ok);
  CHECK(no_depot.message == "Build a Refinery");

  f.world.add_building(f.refinery, f.me, {1, 1}, true);
  f.world.add_mine({15, 1}, 3, 80, f.crystal);
  // a skiff (water worker) is not able to gather crystal
  const EntityId skiff_id = f.world.spawn_unit(*registry().unit("skiff"), f.me, {1, 12});
  if (skiff_id.valid()) {
    const std::array s{skiff_id};
    CHECK_FALSE(f.world.order_harvest(s, {16, 7}).ok);
  }
}

TEST_CASE("a saved game keeps the stock and the cargo by resource id", "[resources]") {
  Fixture f;
  f.world.player(f.me).stock[f.timber] = 7;
  const auto saved = f.world.save_state();
  CHECK(saved.at("players")[0].at("stock").at("crystal") == 500);
  CHECK(saved.at("players")[0].at("stock").at("timber") == 7);
  CHECK_FALSE(saved.at("players")[0].at("stock").contains("gold"));
}

TEST_CASE("the shipped resources.json is what a game without one would get", "[resources]") {
  const auto shipped = Registry::load_directory(std::string(MYRTS_SOURCE_DIR) + "/games/warcraft-like/data");
  REQUIRE(shipped);
  const ResourceTable& a = *shipped->resources();
  const ResourceTable& b = *ResourceTable::legacy();
  REQUIRE(a.size() == b.size());
  for (ResourceId r = 0; r < a.size(); ++r) {
    CHECK(a.at(r).id == b.at(r).id);
    CHECK(a.at(r).icon == b.at(r).icon);
    CHECK(a.at(r).no_depot_message == b.at(r).no_depot_message);
    CHECK(a.at(r).site_sprite == b.at(r).site_sprite);
    CHECK(a.at(r).carry_animation == b.at(r).carry_animation);
    CHECK(a.at(r).enter_site == b.at(r).enter_site);
  }
}

TEST_CASE("units and buildings can require other buildings (a tech tree)", "[techtree]") {
  const char* units = R"({"units":[
    {"id":"w","name":"Worker","hp":1,"sprite":"s","worker":{"gather":{"crystal":{}}},"builds":["hq","lab"]},
    {"id":"mage","name":"Mage","hp":1,"sprite":"s","cost":{"crystal":10},"food":0,"requires":["lab"]}]})";
  const char* buildings = R"({"buildings":[
    {"id":"hq","name":"HQ","size":2,"build_seconds":1,"sprite":"s","trains":["mage"],"accepts":["crystal"]},
    {"id":"lab","name":"Lab","size":2,"build_seconds":1,"sprite":"s","requires":["hq"]}]})";
  const auto r = Registry::parse(units, buildings, R"({"commands":[]})", {}, kResources);
  REQUIRE(r);
  CHECK_FALSE(Registry::parse(R"({"units":[{"id":"m","name":"M","hp":1,"sprite":"s","requires":["nope"]}]})", buildings,
                              R"({"commands":[]})", {}, kResources));

  WorldConfig cfg;
  cfg.resources = r->resources();
  World world{cfg, Grid(20, 10)};
  const PlayerId me = world.add_player("me", "x", *r->resources()->stock_from({{"crystal", 1000}}));
  const BuildingDef& hq = *r->building("hq");
  const BuildingDef& lab = *r->building("lab");
  const UnitDef& mage = *r->unit("mage");

  const EntityId hq_id = world.add_building(hq, me, {1, 1}, false);  // still under construction
  CHECK(world.missing_requirement(me, lab.required_defs) == "HQ");
  CHECK_FALSE(world.order_train(me, hq_id, mage).ok);
  for (int i = 0; i < 100; ++i) world.update(0.05);  // nobody builds it: it stays unfinished
  CHECK(world.missing_requirement(me, lab.required_defs) == "HQ");

  World w2{cfg, Grid(20, 10)};
  const PlayerId p = w2.add_player("me", "x", *r->resources()->stock_from({{"crystal", 1000}}));
  const EntityId hq2 = w2.add_building(hq, p, {1, 1}, true);
  CHECK(w2.missing_requirement(p, lab.required_defs).empty());
  const Result no_lab = w2.order_train(p, hq2, mage);
  REQUIRE_FALSE(no_lab.ok);
  CHECK(no_lab.message == "Requires Lab");
  w2.add_building(lab, p, {6, 1}, true);
  CHECK(w2.order_train(p, hq2, mage).ok);
}

TEST_CASE("projectiles are data: shipped ones resolve, unknown shapes and ids are rejected", "[projectiles]") {
  const auto shipped = Registry::load_directory(std::string(MYRTS_SOURCE_DIR) + "/games/warcraft-like/data");
  REQUIRE(shipped);
  for (const UnitDef& u : shipped->units())
    if (!u.projectile.empty()) {
      INFO(u.id);
      CHECK(shipped->projectile(u.projectile) != nullptr);
    }
  const ProjectileDef* boulder = shipped->projectile("boulder");
  REQUIRE(boulder);
  CHECK(boulder->speed == 280);
  CHECK(boulder->arc_max == 46.0f);
  CHECK(shipped->projectile("arrow")->arc_max == 12.0f);

  const char* units = R"({"units":[{"id":"u","name":"U","hp":1,"sprite":"s","projectile":"laser"}]})";
  CHECK_FALSE(Registry::parse(units, R"({"buildings":[]})", R"({"commands":[]})"));  // not in the legacy set
  const char* lasers = R"({"projectiles":[{"id":"laser","shape":"magic","speed":900}]})";
  CHECK(Registry::parse(units, R"({"buildings":[]})", R"({"commands":[]})", {}, {}, lasers));
  CHECK_FALSE(Registry::parse(units, R"({"buildings":[]})", R"({"commands":[]})", {}, {},
                              R"({"projectiles":[{"id":"laser","shape":"plasma"}]})"));
}

TEST_CASE("right-click rules are data, with the original behaviour as the default", "[orders]") {
  const auto without = Registry::parse(R"({"units":[]})", R"({"buildings":[]})", R"({"commands":[{"id":"move"}]})");
  REQUIRE(without);
  REQUIRE(without->context_rules().size() == 6);
  CHECK(without->context_rules().front().command == "attack");
  CHECK(without->context_rules().back().command == "move");
  CHECK(without->context_rules().back().when.empty());

  const auto own = Registry::parse(R"({"units":[]})", R"({"buildings":[]})",
                                   R"({"commands":[{"id":"move"},{"id":"attack"}],
                                       "context_orders":[{"when":["enemy"],"command":"attack"},{"command":"move"}]})");
  REQUIRE(own);
  CHECK(own->context_rules().size() == 2);
  const char* bad_command = R"({"commands":[{"id":"move"}],"context_orders":[{"command":"fly"}]})";
  CHECK_FALSE(Registry::parse(R"({"units":[]})", R"({"buildings":[]})", bad_command));
  const char* bad_when = R"({"commands":[{"id":"move"}],"context_orders":[{"when":["sunny"],"command":"move"}]})";
  CHECK_FALSE(Registry::parse(R"({"units":[]})", R"({"buildings":[]})", bad_when));
  CHECK_FALSE(Registry::parse(R"({"units":[]})", R"({"buildings":[]})", R"({"commands":[{"id":"x","order":"teleport"}]})"));
}

#include "core/strings.hpp"

TEST_CASE("texts are translated by their English wording, with placeholders", "[i18n]") {
  const auto s = Strings::parse(R"({"language":"pt","strings":{"Cannot build there":"Não dá para construir ali","{0} ready":"{0} pronto"}})");
  REQUIRE(s);
  CHECK(s->language() == "pt");
  CHECK(s->tr("Cannot build there") == "Não dá para construir ali");
  CHECK(s->tr("{0} ready", {"Grunt"}) == "Grunt pronto");
  CHECK(s->tr("Not translated") == "Not translated");                              // falls back to the English text
  CHECK(s->tr("{0} needs {1}", {"A", "B"}) == "A needs B");                       // placeholders work on the fallback too
  CHECK(s->tr("{0} {1}", {"only"}) == "only {1}");                                 // a missing argument stays as written
  CHECK(Strings::english()->tr("Cannot build there") == "Cannot build there");
  CHECK_FALSE(Strings::parse(R"({"strings":{"a":3}})"));
  CHECK_FALSE(Strings::parse("{"));
}

TEST_CASE("a world gives its messages in the game's language", "[i18n]") {
  const char* units = R"({"units":[{"id":"u","name":"U","hp":5,"sprite":"s"}]})";
  const auto r = Registry::parse(units, R"({"buildings":[]})", R"({"commands":[]})");
  REQUIRE(r);
  WorldConfig cfg;
  cfg.strings = std::make_shared<const Strings>(*Strings::parse(R"({"strings":{"These units cannot attack":"Essas unidades não atacam"}})"));
  World world{cfg, Grid(10, 10)};
  const PlayerId me = world.add_player("me", "x", Stock{});
  const EntityId unit = world.spawn_unit(*r->unit("u"), me, {2, 2});
  const std::array ids{unit};
  const Result result = world.order_attack(ids, {5, 5});
  REQUIRE_FALSE(result.ok);
  CHECK(result.message == "Essas unidades não atacam");
}

TEST_CASE("the shipped Portuguese file parses", "[i18n]") {
  const auto pt = Strings::load_file(std::string(MYRTS_SOURCE_DIR) + "/games/warcraft-like/strings/pt.json");
  REQUIRE(pt);
  CHECK(pt->language() == "pt");
  CHECK(pt->size() > 100);
  CHECK(pt->tr("Victory!") == "Vitória!");
}
