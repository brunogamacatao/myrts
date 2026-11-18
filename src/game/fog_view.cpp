#include "fog_view.hpp"

#include <algorithm>
#include <vector>

namespace {
float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
}  // namespace

void FogView::update(const myrts::FogOfWar& fog) {
  if (fog.empty()) return;
  if (black_ && fog.version() == version_ && fog.width() == width_ && fog.height() == height_) return;
  version_ = fog.version();
  width_ = fog.width();
  height_ = fog.height();

  if (!black_ || al_get_bitmap_width(black_.get()) != width_ || al_get_bitmap_height(black_.get()) != height_) {
    const int flags = al_get_new_bitmap_flags();
    al_set_new_bitmap_flags((flags & ~ALLEGRO_MEMORY_BITMAP) | ALLEGRO_MIN_LINEAR | ALLEGRO_MAG_LINEAR);
    black_.reset(al_create_bitmap(width_, height_));
    grey_.reset(al_create_bitmap(width_, height_));
    al_set_new_bitmap_flags(flags);
    if (!black_ || !grey_) return;
  }

  const auto unknown = [&](int x, int y) {
    x = std::clamp(x, 0, width_ - 1);  // beyond the map edge counts as the nearest tile
    y = std::clamp(y, 0, height_ - 1);
    return fog.explored({x, y}) ? 0.0f : 1.0f;
  };

  // A 3x3 blur of the "unknown" map: the black area gets rounded, with a one-tile rim of partial values around it.
  std::vector<float> shade(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_));
  for (int y = 0; y < height_; ++y)
    for (int x = 0; x < width_; ++x)
      shade[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)] =
          (4 * unknown(x, y) + 2 * (unknown(x - 1, y) + unknown(x + 1, y) + unknown(x, y - 1) + unknown(x, y + 1)) +
           unknown(x - 1, y - 1) + unknown(x + 1, y - 1) + unknown(x - 1, y + 1) + unknown(x + 1, y + 1)) / 16.0f;

  const float soft = static_cast<float>(style_.edge_softness);
  const float unknown_opacity = static_cast<float>(style_.unknown_opacity);
  const float explored_opacity = static_cast<float>(style_.explored_opacity);
  const float black_rgb[3] = {static_cast<float>(style_.unknown_color[0]) / 255.0f, static_cast<float>(style_.unknown_color[1]) / 255.0f, static_cast<float>(style_.unknown_color[2]) / 255.0f};
  const float veil_rgb[3] = {static_cast<float>(style_.explored_color[0]) / 255.0f, static_cast<float>(style_.explored_color[1]) / 255.0f, static_cast<float>(style_.explored_color[2]) / 255.0f};
  ALLEGRO_BITMAP* previous = al_get_target_bitmap();
  for (const bool black : {true, false}) {
    ALLEGRO_BITMAP* target = black ? black_.get() : grey_.get();
    al_set_target_bitmap(target);
    al_lock_bitmap(target, ALLEGRO_PIXEL_FORMAT_ANY_32_WITH_ALPHA, ALLEGRO_LOCK_WRITEONLY);
    for (int y = 0; y < height_; ++y)
      for (int x = 0; x < width_; ++x) {
        const float b = shade[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)];
        const float edge = clamp01((b - 0.42f) / (0.14f * soft));
        const float veil = clamp01(b * 2.4f / soft) * explored_opacity;
        al_put_pixel(x, y, black ? al_premul_rgba_f(black_rgb[0], black_rgb[1], black_rgb[2], edge * unknown_opacity)
                                 : al_premul_rgba_f(veil_rgb[0], veil_rgb[1], veil_rgb[2], veil));
      }
    al_unlock_bitmap(target);
  }
  al_set_target_bitmap(previous);
}

void FogView::draw(float tile_size) const {
  if (!black_) return;
  const auto w = static_cast<float>(width_);
  const auto h = static_cast<float>(height_);
  al_draw_scaled_bitmap(grey_.get(), 0, 0, w, h, 0, 0, w * tile_size, h * tile_size, 0);
  al_draw_scaled_bitmap(black_.get(), 0, 0, w, h, 0, 0, w * tile_size, h * tile_size, 0);
}
