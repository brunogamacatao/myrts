#include "resources.hpp"

#include <nlohmann/json.hpp>

namespace myrts {

namespace {
using json = nlohmann::json;

constexpr const char* kLegacy = R"({"resources":[
 {"id":"gold","name":"Gold","short_label":"g","icon":"ui/gold.png","source":"site","site_terrain":"land","enter_site":true,
  "carry_animation":"gold","pud_source":"gold_mine","site_sprite":"buildings/gold_mine_{era}.png",
  "site_sprite_active":"buildings/gold_mine_active_{era}.png",
  "no_depot_message":"You need a Great Hall or Town Hall to deliver resources","unreachable_message":"Cannot reach that gold mine",
  "depleted_message":"There is no gold left to mine","exhausted_message":"The gold mine is exhausted"},
 {"id":"lumber","name":"Lumber","short_label":"w","icon":"ui/lumber.png","source":"tree","carry_animation":"wood",
  "no_depot_message":"You need a Great Hall or Town Hall to deliver resources","unreachable_message":"Cannot reach that tree",
  "depleted_message":"There are no more trees"},
 {"id":"oil","name":"Oil","short_label":"o","icon":"ui/oil.png","source":"site","site_terrain":"water","carry_animation":"oil",
  "pud_source":"oil_patch","site_sprite":"buildings/oil_patch.png",
  "no_depot_message":"You need a Shipyard to deliver oil","unreachable_message":"Cannot reach that oil patch",
  "depleted_message":"There is no oil left to pump","exhausted_message":"The oil patch is exhausted"}]})";

std::string text(const json& j, const char* key, const std::string& fallback = {}) {
  if (!j.contains(key)) return fallback;
  if (!j.at(key).is_string()) throw std::runtime_error(std::string("\"") + key + "\" must be a string");
  return j.at(key).get<std::string>();
}
}  // namespace

std::expected<ResourceTable, std::string> ResourceTable::parse(std::string_view source) {
  try {
    const json doc = json::parse(source);
    ResourceTable t;
    for (const json& j : doc.at("resources")) {
      ResourceDef d;
      d.id = j.at("id").get<std::string>();
      if (d.id.empty()) throw std::runtime_error("a resource has an empty id");
      for (const ResourceDef& other : t.defs_)
        if (other.id == d.id) throw std::runtime_error("duplicate resource id '" + d.id + "'");
      d.name = text(j, "name", d.id);
      d.short_label = text(j, "short_label", d.id.substr(0, 1));
      d.icon = text(j, "icon");
      const std::string src = text(j, "source", "site");
      if (src == "site") d.source = ResourceSource::Site;
      else if (src == "tree") d.source = ResourceSource::Tree;
      else throw std::runtime_error("resource '" + d.id + "': unknown source '" + src + "' (expected site or tree)");
      const std::string terrain = text(j, "site_terrain", "land");
      if (terrain == "land") d.site_terrain = SiteTerrain::Land;
      else if (terrain == "water") d.site_terrain = SiteTerrain::Water;
      else throw std::runtime_error("resource '" + d.id + "': unknown site_terrain '" + terrain + "' (expected land or water)");
      d.enter_site = j.value("enter_site", false);
      d.carry_animation = text(j, "carry_animation", d.id);
      d.pud_source = text(j, "pud_source");
      d.site_sprite = text(j, "site_sprite");
      d.site_sprite_active = text(j, "site_sprite_active");
      d.no_depot_message = text(j, "no_depot_message");
      d.unreachable_message = text(j, "unreachable_message");
      d.depleted_message = text(j, "depleted_message");
      d.exhausted_message = text(j, "exhausted_message");
      if (d.source == ResourceSource::Tree) {
        if (t.tree_ != kNoResource) throw std::runtime_error("only one resource may be gathered from trees");
        t.tree_ = static_cast<ResourceId>(t.defs_.size());
      }
      t.defs_.push_back(std::move(d));
      if (t.defs_.size() > static_cast<std::size_t>(kMaxResources))
        throw std::runtime_error("a game may define at most " + std::to_string(kMaxResources) + " resources");
    }
    if (t.defs_.empty()) throw std::runtime_error("a game needs at least one resource");
    return t;
  } catch (const std::exception& e) {
    return std::unexpected(std::string("resources: ") + e.what());
  }
}

std::shared_ptr<const ResourceTable> ResourceTable::legacy() {
  static const std::shared_ptr<const ResourceTable> table = std::make_shared<const ResourceTable>(*parse(kLegacy));
  return table;
}

ResourceId ResourceTable::index(std::string_view id) const {
  for (std::size_t i = 0; i < defs_.size(); ++i)
    if (defs_[i].id == id) return static_cast<ResourceId>(i);
  return kNoResource;
}

ResourceId ResourceTable::first_site_resource(SiteTerrain terrain) const {
  for (std::size_t i = 0; i < defs_.size(); ++i)
    if (defs_[i].source == ResourceSource::Site && defs_[i].site_terrain == terrain) return static_cast<ResourceId>(i);
  return kNoResource;
}

ResourceId ResourceTable::from_pud(std::string_view source) const {
  if (source.empty()) return kNoResource;
  for (std::size_t i = 0; i < defs_.size(); ++i)
    if (defs_[i].pud_source == source) return static_cast<ResourceId>(i);
  return kNoResource;
}

std::expected<Stock, std::string> ResourceTable::stock_from(const std::map<std::string, int>& amounts) const {
  Stock s;
  for (const auto& [id, n] : amounts) {
    const ResourceId r = index(id);
    if (r == kNoResource) return std::unexpected("unknown resource '" + id + "'");
    if (n < 0) return std::unexpected("resource '" + id + "' cannot be negative");
    s[r] = n;
  }
  return s;
}

std::map<std::string, int> ResourceTable::to_map(const Stock& stock) const {
  std::map<std::string, int> out;
  for (std::size_t i = 0; i < defs_.size(); ++i)
    if (stock.amount[i] != 0) out[defs_[i].id] = stock.amount[i];
  return out;
}

std::string ResourceTable::no_depot_message(ResourceId r) const {
  const ResourceDef& d = at(r);
  return d.no_depot_message.empty() ? "You need a building that accepts " + d.name : d.no_depot_message;
}
std::string ResourceTable::unreachable_message(ResourceId r) const {
  const ResourceDef& d = at(r);
  return d.unreachable_message.empty() ? "Cannot reach the " + d.name : d.unreachable_message;
}
std::string ResourceTable::depleted_message(ResourceId r) const {
  const ResourceDef& d = at(r);
  return d.depleted_message.empty() ? "There is no " + d.name + " left" : d.depleted_message;
}
std::string ResourceTable::exhausted_message(ResourceId r) const {
  const ResourceDef& d = at(r);
  return d.exhausted_message.empty() ? "The " + d.name + " is exhausted" : d.exhausted_message;
}

}  // namespace myrts
