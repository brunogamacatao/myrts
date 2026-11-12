#include "building_art.hpp"

#include <allegro5/allegro_primitives.h>

namespace {

const char* kFinished[] = {"buildings/great_hall.png", "buildings/pig_farm.png", "buildings/barracks.png"};
const char* kConstruction[] = {"buildings/great_hall_construction.png", "buildings/pig_farm_construction.png",
                               "buildings/barracks_construction.png"};

/// Stand-in used when buildings/gold_mine*.png are absent.
ALLEGRO_BITMAP* make_placeholder_mine(bool active) {
  ALLEGRO_BITMAP* bmp = al_create_bitmap(96, 89);
  if (!bmp) return nullptr;
  ALLEGRO_BITMAP* previous = al_get_target_bitmap();
  al_set_target_bitmap(bmp);
  al_clear_to_color(al_map_rgba(0, 0, 0, 0));
  al_draw_filled_ellipse(48, 56, 46, 30, al_map_rgb(96, 92, 86));
  al_draw_filled_ellipse(48, 46, 38, 30, al_map_rgb(128, 122, 112));
  al_draw_filled_ellipse(40, 36, 20, 14, al_map_rgb(150, 144, 134));
  al_draw_filled_rectangle(34, 54, 62, 80, al_map_rgb(40, 28, 16));
  const ALLEGRO_COLOR gold = active ? al_map_rgb(255, 230, 90) : al_map_rgb(200, 160, 30);
  al_draw_filled_circle(48, 66, 8, gold);
  al_draw_filled_circle(30, 40, 4, gold);
  al_draw_filled_circle(66, 44, 4, gold);
  al_set_target_bitmap(previous);
  return bmp;
}

}  // namespace

BuildingArt::~BuildingArt() {
  for (auto* b : finished_) al_destroy_bitmap(b);
  for (auto* b : construction_) al_destroy_bitmap(b);
  al_destroy_bitmap(mine_);
  al_destroy_bitmap(mine_active_);
}

bool BuildingArt::load() {
  for (std::size_t i = 0; i < kKinds; ++i) {
    finished_[i] = al_load_bitmap(kFinished[i]);
    construction_[i] = al_load_bitmap(kConstruction[i]);
    if (!finished_[i] || !construction_[i]) return false;
  }
  mine_ = al_load_bitmap("buildings/gold_mine.png");
  if (!mine_) mine_ = make_placeholder_mine(false);
  mine_active_ = al_load_bitmap("buildings/gold_mine_active.png");
  if (!mine_active_) mine_active_ = make_placeholder_mine(true);
  return mine_ && mine_active_;
}

void draw_centered_in_square(ALLEGRO_BITMAP* bitmap, float x, float y, float size_px, ALLEGRO_COLOR tint) {
  if (!bitmap) return;
  const auto w = static_cast<float>(al_get_bitmap_width(bitmap));
  const auto h = static_cast<float>(al_get_bitmap_height(bitmap));
  al_draw_tinted_bitmap(bitmap, tint, x + (size_px - w) / 2, y + (size_px - h) / 2, 0);
}
