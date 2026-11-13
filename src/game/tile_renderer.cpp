#include "tile_renderer.hpp"

#include <algorithm>
#include <cmath>

std::expected<void, std::string> TileRenderer::load(const myrts::TileMap& map, Assets& assets) {
  auto sheet = assets.bitmap(map.tileset().image());
  if (!sheet) return std::unexpected(sheet.error());
  sheet_ = *sheet;

  minimap_.reset(al_create_bitmap(map.width() * kMini, map.height() * kMini));
  if (!minimap_) return std::unexpected("cannot create the minimap bitmap");

  ALLEGRO_BITMAP* previous = al_get_target_bitmap();
  al_set_target_bitmap(minimap_.get());
  al_clear_to_color(al_map_rgb(0, 0, 0));
  al_hold_bitmap_drawing(true);
  for (int y = 0; y < map.height(); ++y)
    for (int x = 0; x < map.width(); ++x) draw_minitile(map, {x, y});
  al_hold_bitmap_drawing(false);
  al_set_target_bitmap(previous);
  return {};
}

void TileRenderer::draw_minitile(const myrts::TileMap& map, myrts::Point tile) const {
  const auto spot = map.tileset().spot(map.tile(tile));
  if (!spot) return;
  al_draw_bitmap_region(sheet_->get(), static_cast<float>(spot->x), static_cast<float>(spot->y), kMini, kMini,
                        static_cast<float>(tile.x * kMini), static_cast<float>(tile.y * kMini), 0);
}

void TileRenderer::update_minimap(const myrts::TileMap& map, myrts::Point tile) {
  if (!minimap_ || !map.contains(tile)) return;
  ALLEGRO_BITMAP* previous = al_get_target_bitmap();
  al_set_target_bitmap(minimap_.get());
  draw_minitile(map, tile);
  al_set_target_bitmap(previous);
}

void TileRenderer::draw(const myrts::TileMap& map, const myrts::Rect& visible) const {
  if (!sheet_) return;
  const int x0 = std::max(0, static_cast<int>(std::floor(visible.x / kTile)));
  const int y0 = std::max(0, static_cast<int>(std::floor(visible.y / kTile)));
  const int x1 = std::min(map.width() - 1, static_cast<int>(std::floor((visible.x + visible.w) / kTile)));
  const int y1 = std::min(map.height() - 1, static_cast<int>(std::floor((visible.y + visible.h) / kTile)));

  ALLEGRO_BITMAP* sheet = sheet_->get();
  al_hold_bitmap_drawing(true);  // one batch for the whole screen
  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      const auto spot = map.tileset().spot(map.tile({x, y}));
      if (!spot) continue;
      al_draw_bitmap_region(sheet, static_cast<float>(spot->x), static_cast<float>(spot->y), kTile, kTile,
                            static_cast<float>(x * kTile), static_cast<float>(y * kTile), 0);
    }
  }
  al_hold_bitmap_drawing(false);
}
