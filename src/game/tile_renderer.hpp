#pragma once

#include <allegro5/allegro5.h>

#include <memory>
#include <string>

#include "assets.hpp"
#include "core/geometry.hpp"
#include "core/tilemap.hpp"
#include "raii.hpp"

/// Draws a TileMap from the tileset image of its era, and keeps a one-pixel-per-minitile picture of it for the
/// minimap.
///
/// A map tile is a 32x32 "megatile" of the tileset image; the table in assets/data/tilesets.json says where each
/// tile code lives. The minimap uses the top-left 8x8 "minitile" of every megatile, as the Warcraft II editors do.
class TileRenderer {
 public:
  static constexpr int kTile = 32;
  static constexpr int kMini = 8;

  /// Loads the tileset image and renders the minimap. Returns an error message on failure.
  std::expected<void, std::string> load(const myrts::TileMap& map, Assets& assets);

  /// Draws the tiles inside `visible` (world pixels) with the current transform.
  void draw(const myrts::TileMap& map, const myrts::Rect& visible) const;

  /// Redraws one tile of the minimap after the terrain changed.
  void update_minimap(const myrts::TileMap& map, myrts::Point tile);

  [[nodiscard]] ALLEGRO_BITMAP* minimap() const { return minimap_.get(); }

 private:
  void draw_minitile(const myrts::TileMap& map, myrts::Point tile) const;

  std::shared_ptr<const Bitmap> sheet_;
  BitmapPtr minimap_;
};
