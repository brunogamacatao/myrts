#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "grid.hpp"
#include "pud.hpp"
#include "terrain.hpp"
#include "tileset.hpp"

namespace myrts {

/// The terrain of a map: one tile code and one movement code per tile.
///
/// The codes follow the Warcraft II scheme (see terrain.hpp), so a map loaded from a .pud keeps exactly the look the
/// map's author gave it, and terrain that changes while playing (a felled tree) gets its borders redrawn by the same
/// rules: the tiles around the change are recomposed from their four quadrants.
class TileMap {
 public:
  TileMap() = default;

  /// A map as stored in a .pud. Fails if the tileset lacks a code the map uses.
  static std::expected<TileMap, std::string> from_pud(const PudMap& pud, std::shared_ptr<const TileSets> sets);

  /// The autotiler. Builds a map from the terrain at every vertex of the tile grid: tile (x, y) has the quadrants
  /// (x, y), (x+1, y), (x, y+1) and (x+1, y+1). Fails, naming the tile, if two terrains that never touch meet
  /// (water against grass, say: there has to be a coast between them).
  static std::expected<TileMap, std::string> from_terrain(std::shared_ptr<const TileSets> sets, Era era, int width,
                                                          int height,
                                                          const std::function<Terrain(int, int)>& terrain_at,
                                                          std::uint32_t seed = 1);

  [[nodiscard]] int width() const { return width_; }
  [[nodiscard]] int height() const { return height_; }
  [[nodiscard]] Era era() const { return era_; }
  [[nodiscard]] const TileSet& tileset() const { return sets_->era(era_); }
  [[nodiscard]] bool contains(Point p) const { return p.x >= 0 && p.y >= 0 && p.x < width_ && p.y < height_; }

  [[nodiscard]] std::uint16_t tile(Point p) const { return tiles_[index(p)]; }
  [[nodiscard]] std::uint16_t movement(Point p) const { return movement_[index(p)]; }
  /// The four quadrants of a tile (`nullopt` for an unknown code).
  [[nodiscard]] std::optional<Quad> quad(Point p) const { return decompose(tile(p)); }

  /// Tiles that block land units because of trees, with wood to chop.
  [[nodiscard]] bool is_tree(Point p) const;
  [[nodiscard]] std::vector<Point> tree_tiles() const;

  /// Tiles that land units / ships cannot enter.
  [[nodiscard]] Grid land_blocked() const;
  [[nodiscard]] Grid water_blocked() const;

  /// Number of tiles whose code the tileset cannot draw (should be 0).
  [[nodiscard]] std::size_t missing_tiles() const;

  /// Fells the trees of `tile`: its tree quadrants turn to grass and every tile that shares a corner with it is
  /// recomposed, so forest edges follow. Returns the tiles whose code changed.
  std::vector<Point> clear_trees(Point at);

 private:
  [[nodiscard]] std::size_t index(Point p) const {
    return static_cast<std::size_t>(p.y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(p.x);
  }

  std::shared_ptr<const TileSets> sets_;
  Era era_ = Era::Forest;
  int width_ = 0;
  int height_ = 0;
  std::vector<std::uint16_t> tiles_;
  std::vector<std::uint16_t> movement_;
};

}  // namespace myrts
