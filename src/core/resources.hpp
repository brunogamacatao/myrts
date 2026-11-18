#pragma once

#include <array>
#include <expected>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace myrts {

/// Index of a resource in the game's `ResourceTable` (the order of resources.json).
using ResourceId = int;
inline constexpr ResourceId kNoResource = -1;
/// The most kinds of resource a game may define (a Stock holds this many amounts).
inline constexpr int kMaxResources = 8;

/// Where a resource is gathered.
enum class ResourceSource {
  Site,  ///< from a mine or patch placed on the map (a finite amount in one obstacle)
  Tree   ///< from the map's trees (at most one resource per game may be of this kind)
};
enum class SiteTerrain { Land, Water };

/// One kind of resource a game defines (an entry of resources.json). The engine never mentions a resource by name:
/// everything it needs to know is here.
struct ResourceDef {
  std::string id;            ///< key used in costs, stocks and saved games ("gold")
  std::string name;          ///< shown to the player
  std::string short_label;   ///< one or two letters after a price ("g")
  std::string icon;          ///< HUD image, relative to the game package
  ResourceSource source = ResourceSource::Site;
  SiteTerrain site_terrain = SiteTerrain::Land;  ///< where its sites lie (a water site is pumped by ships)
  bool enter_site = false;   ///< the worker goes inside the site, one at a time (gold mines)
  std::string carry_animation;  ///< infix of the walking-with-cargo animation ("gold" -> walk_gold_<dir>)
  std::string pud_source;    ///< which Warcraft II map object holds it ("gold_mine", "oil_patch"), empty for none
  std::string site_sprite;         ///< image of a site; "{era}" is replaced by the terrain era's name
  std::string site_sprite_active;  ///< image while a worker is inside (optional)
  // Messages (an empty one gets a generic text built from `name`).
  std::string no_depot_message;
  std::string unreachable_message;
  std::string depleted_message;   ///< nothing of this resource is left to gather
  std::string exhausted_message;  ///< a site has just run dry
};

/// Amounts of each resource (a player's stock, or the price of something), indexed by ResourceId.
struct Stock {
  std::array<int, kMaxResources> amount{};

  [[nodiscard]] constexpr int& operator[](ResourceId r) { return amount[static_cast<std::size_t>(r)]; }
  [[nodiscard]] constexpr int operator[](ResourceId r) const { return amount[static_cast<std::size_t>(r)]; }
  constexpr bool operator==(const Stock&) const = default;
  [[nodiscard]] constexpr bool empty() const {
    for (const int a : amount)
      if (a != 0) return false;
    return true;
  }
};
using Cost = Stock;

[[nodiscard]] constexpr bool can_afford(const Stock& have, const Cost& cost) {
  for (std::size_t i = 0; i < kMaxResources; ++i)
    if (have.amount[i] < cost.amount[i]) return false;
  return true;
}
/// Deducts `cost` from `stock`. Returns false (leaving `stock` untouched) if it cannot be paid.
constexpr bool pay(Stock& stock, const Cost& cost) {
  if (!can_afford(stock, cost)) return false;
  for (std::size_t i = 0; i < kMaxResources; ++i) stock.amount[i] -= cost.amount[i];
  return true;
}
/// Adds `gain` to `stock`.
constexpr void receive(Stock& stock, const Stock& gain) {
  for (std::size_t i = 0; i < kMaxResources; ++i) stock.amount[i] += gain.amount[i];
}

/// The resources of a game, in the order of resources.json. Immutable once built.
class ResourceTable {
 public:
  /// Parses `{"resources":[...]}`. Fails naming the problem (duplicate ids, more than kMaxResources, two tree resources...).
  static std::expected<ResourceTable, std::string> parse(std::string_view json);
  /// Gold, lumber and oil as the original game defines them: what is used when a game ships no resources.json.
  static std::shared_ptr<const ResourceTable> legacy();

  [[nodiscard]] int size() const { return static_cast<int>(defs_.size()); }
  [[nodiscard]] const std::vector<ResourceDef>& all() const { return defs_; }
  [[nodiscard]] const ResourceDef& at(ResourceId r) const { return defs_[static_cast<std::size_t>(r)]; }
  /// kNoResource when there is no such id.
  [[nodiscard]] ResourceId index(std::string_view id) const;
  [[nodiscard]] bool valid(ResourceId r) const { return r >= 0 && r < size(); }
  /// The resource gathered from trees, or kNoResource.
  [[nodiscard]] ResourceId tree_resource() const { return tree_; }
  /// The first resource gathered from a site of the given terrain, or kNoResource.
  [[nodiscard]] ResourceId first_site_resource(SiteTerrain terrain) const;
  /// The resource a Warcraft II map object stands for ("gold_mine"), or kNoResource.
  [[nodiscard]] ResourceId from_pud(std::string_view source) const;

  /// `{"gold": 100, ...}` as a Stock; fails on an unknown id or a negative amount.
  [[nodiscard]] std::expected<Stock, std::string> stock_from(const std::map<std::string, int>& amounts) const;
  /// The reverse: every non-zero amount by id.
  [[nodiscard]] std::map<std::string, int> to_map(const Stock& stock) const;

  // Messages with the generic fallbacks applied.
  [[nodiscard]] std::string no_depot_message(ResourceId r) const;
  [[nodiscard]] std::string unreachable_message(ResourceId r) const;
  [[nodiscard]] std::string depleted_message(ResourceId r) const;
  [[nodiscard]] std::string exhausted_message(ResourceId r) const;

 private:
  std::vector<ResourceDef> defs_;
  ResourceId tree_ = kNoResource;
};

}  // namespace myrts
