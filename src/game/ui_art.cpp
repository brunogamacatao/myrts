#include "ui_art.hpp"

#include <algorithm>

std::expected<void, std::string> UiArt::load(Assets& assets) {
  const std::pair<const char*, std::shared_ptr<const Bitmap>*> files[] = {
      {"ui/icons.png", &icons_},   {"ui/gold.png", &gold_},         {"ui/lumber.png", &lumber_},
      {"ui/oil.png", &oil_},       {"ui/food.png", &food_},         {"ui/panel.png", &panel_},
      {"ui/topbar.png", &top_bar_}, {"ui/minimap_frame.png", &minimap_frame_}};
  for (const auto& [path, target] : files) {
    auto bmp = assets.bitmap(path);
    if (!bmp) return std::unexpected(bmp.error());
    *target = *bmp;
  }
  return {};
}

void UiArt::draw_icon(int index, float x, float y, float scale, float alpha) const {
  if (index < 0 || !icons_) return;
  const auto sx = static_cast<float>((index % kIconCols) * kIconW);
  const auto sy = static_cast<float>((index / kIconCols) * kIconH);
  al_draw_tinted_scaled_bitmap(icons_->get(), al_map_rgba_f(alpha, alpha, alpha, alpha), sx, sy, kIconW, kIconH, x, y,
                               kIconW * scale, kIconH * scale, 0);
}

void UiArt::draw_tiled(ALLEGRO_BITMAP* texture, const myrts::Rect& area) {
  if (!texture) return;
  const auto tw = static_cast<float>(al_get_bitmap_width(texture));
  const auto th = static_cast<float>(al_get_bitmap_height(texture));
  al_hold_bitmap_drawing(true);
  for (float y = area.y; y < area.y + area.h; y += th) {
    for (float x = area.x; x < area.x + area.w; x += tw) {
      const float w = std::min(tw, area.x + area.w - x);
      const float h = std::min(th, area.y + area.h - y);
      al_draw_bitmap_region(texture, 0, 0, w, h, x, y, 0);
    }
  }
  al_hold_bitmap_drawing(false);
}
