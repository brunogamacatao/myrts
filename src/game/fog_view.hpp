#pragma once

#include <allegro5/allegro5.h>

#include <cstdint>

#include "core/fog.hpp"
#include "raii.hpp"

/// Draws the fog of war as in the original game: black where nothing has been seen, with a soft translucent grey rim
/// between it and the explored ground.
///
/// Two small bitmaps (one pixel per map tile) hold the shades; they are stretched over the whole map with bilinear
/// filtering, which is what rounds off the edge of the black area.
class FogView {
 public:
  /// Rebuilds the shades if something new was discovered since the last call.
  void update(const myrts::FogOfWar& fog);
  /// Draws the fog over the world (call with the world transform active).
  void draw(float tile_size) const;
  /// The black shade, one pixel per tile (for the minimap); null before the first update().
  [[nodiscard]] ALLEGRO_BITMAP* black() const { return black_.get(); }

 private:
  BitmapPtr black_;
  BitmapPtr grey_;
  std::uint32_t version_ = 0;
  int width_ = 0;
  int height_ = 0;
};
