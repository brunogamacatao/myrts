#include "tilemap.hpp"

#include <algorithm>
#include <set>

namespace myrts {

namespace {

constexpr std::uint32_t kVariationSalt = 0x7117u;

std::uint16_t movement_for(std::uint16_t code) {
  const auto q = decompose(code);
  return q ? movement_of(*q) : movement::kLand;
}

}  // namespace

std::expected<TileMap, std::string> TileMap::from_pud(const PudMap& pud, std::shared_ptr<const TileSets> sets) {
  TileMap map;
  map.sets_ = std::move(sets);
  map.era_ = pud.era;
  map.width_ = pud.width;
  map.height_ = pud.height;
  map.tiles_ = pud.tiles;
  if (pud.movement.size() == pud.tiles.size()) {
    map.movement_ = pud.movement;  // the map's own movement data is authoritative
  } else {
    map.movement_.reserve(pud.tiles.size());
    for (const auto code : pud.tiles) map.movement_.push_back(movement_for(code));
  }
  if (const auto missing = map.missing_tiles(); missing > 0)
    return std::unexpected("map uses " + std::to_string(missing) + " tiles the " + map.tileset().name() +
                           " tileset does not have");
  return map;
}

std::expected<TileMap, std::string> TileMap::from_terrain(std::shared_ptr<const TileSets> sets, Era era, int width,
                                                          int height,
                                                          const std::function<Terrain(int, int)>& terrain_at,
                                                          std::uint32_t seed) {
  TileMap map;
  map.sets_ = std::move(sets);
  map.era_ = era;
  map.width_ = width;
  map.height_ = height;
  map.tiles_.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
  map.movement_.resize(map.tiles_.size());

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const Quad q{terrain_at(x, y), terrain_at(x + 1, y), terrain_at(x, y + 1), terrain_at(x + 1, y + 1)};
      const auto base = compose(q);
      if (!base)
        return std::unexpected("tile (" + std::to_string(x) + ", " + std::to_string(y) +
                               "): these terrains cannot meet in one tile");
      const auto code = map.tileset().pick(*base, tile_hash(x, y, seed));
      if (!code)
        return std::unexpected("tile (" + std::to_string(x) + ", " + std::to_string(y) + "): the tileset has no tile 0x" +
                               std::to_string(*base));
      map.tiles_[map.index({x, y})] = *code;
      map.movement_[map.index({x, y})] = movement_of(q);
    }
  }
  return map;
}

bool TileMap::is_tree(Point p) const {
  if (const auto q = quad(p)) return has_trees(*q);
  const std::uint16_t code = tile(p);
  return (code & 0x0ff0) == 0x0070 || (code & 0x0f00) == 0x0700;  // unknown code: go by its group
}

std::vector<Point> TileMap::tree_tiles() const {
  std::vector<Point> out;
  for (int y = 0; y < height_; ++y)
    for (int x = 0; x < width_; ++x)
      if (is_tree({x, y})) out.push_back({x, y});
  return out;
}

Grid TileMap::land_blocked() const {
  Grid g(width_, height_);
  for (int y = 0; y < height_; ++y)
    for (int x = 0; x < width_; ++x)
      if (!land_passable(movement({x, y}))) g.set_blocked({x, y});
  return g;
}

Grid TileMap::water_blocked() const {
  Grid g(width_, height_);
  for (int y = 0; y < height_; ++y)
    for (int x = 0; x < width_; ++x)
      if (!sailable(movement({x, y}))) g.set_blocked({x, y});
  return g;
}

std::size_t TileMap::missing_tiles() const {
  return static_cast<std::size_t>(std::ranges::count_if(tiles_, [&](std::uint16_t c) { return !tileset().has(c); }));
}

std::vector<Point> TileMap::clear_trees(Point at) {
  std::vector<Point> changed;
  if (!contains(at)) return changed;
  const auto own = quad(at);
  if (!own) return changed;

  // The vertices of this tile that hold trees...
  std::set<std::pair<int, int>> vertices;
  if (own->tl == Terrain::Trees) vertices.insert({at.x, at.y});
  if (own->tr == Terrain::Trees) vertices.insert({at.x + 1, at.y});
  if (own->bl == Terrain::Trees) vertices.insert({at.x, at.y + 1});
  if (own->br == Terrain::Trees) vertices.insert({at.x + 1, at.y + 1});
  if (vertices.empty()) return changed;

  // ...and every tile that has one of them as a corner.
  std::set<std::pair<int, int>> affected;
  for (const auto& [vx, vy] : vertices)
    for (int dy = -1; dy <= 0; ++dy)
      for (int dx = -1; dx <= 0; ++dx)
        if (contains({vx + dx, vy + dy})) affected.insert({vx + dx, vy + dy});

  for (const auto& [tx, ty] : affected) {
    const Point p{tx, ty};
    auto q = quad(p);
    if (!q) continue;  // not a tile we understand: leave it alone
    const auto set_if_tree = [&](Terrain& quadrant, int vx, int vy) {
      if (quadrant == Terrain::Trees && vertices.contains({vx, vy})) quadrant = Terrain::GrassLight;
    };
    set_if_tree(q->tl, tx, ty);
    set_if_tree(q->tr, tx + 1, ty);
    set_if_tree(q->bl, tx, ty + 1);
    set_if_tree(q->br, tx + 1, ty + 1);

    const auto base = compose(*q);
    if (!base) continue;  // would be an impossible tile: keep what the map had
    const std::uint16_t old_code = tile(p);
    std::uint16_t code = old_code;
    if ((old_code & 0xfff0) != *base) {  // the look changed: pick one of its variations
      const auto picked = tileset().pick(*base, tile_hash(tx, ty, kVariationSalt));
      if (!picked) continue;
      code = *picked;
    }
    if (code == old_code) continue;
    tiles_[index(p)] = code;
    movement_[index(p)] = movement_of(*q);
    changed.push_back(p);
  }
  return changed;
}

}  // namespace myrts
