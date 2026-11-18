#pragma once

#include <allegro5/allegro5.h>

#include <cstdint>

#include "core/fog.hpp"
#include "core/fog_settings.hpp"
#include "raii.hpp"

/// Draws the fog of war as in the original game: black where nothing has been seen, with a soft translucent grey rim
/// between it and the explored ground.
///
/// Two small bitmaps (one pixel per map tile) hold the shades; they are stretched over the whole map with bilinear
/// filtering, which is what rounds off the edge of the black area.
class FogView {
 public:
  /// The colours, opacities and softness to draw with (call before the first update()).
  void set_style(const myrts::FogSettings& style) {
    style_ = style;
    version_ = 0;
    black_.reset();
  }
  /// Rebuilds the shades if something new was discovered since the last call.
  void update(const myrts::FogOfWar& fog);
  /// Draws the fog over the world (call with the world transform active).
  void draw(float tile_size) const;
  /// The black shade, one pixel per tile (for the minimap); null before the first update().
  [[nodiscard]] ALLEGRO_BITMAP* black() const { return black_.get(); }

 private:
  myrts::FogSettings style_;
  BitmapPtr black_;
  BitmapPtr grey_;
  std::uint32_t version_ = 0;
  int width_ = 0;
  int height_ = 0;
};
