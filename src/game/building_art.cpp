#include "building_art.hpp"

#include <allegro5/allegro_primitives.h>

#include <cstdio>

namespace {

/// Stand-in used when buildings/gold_mine*.png are absent.
std::shared_ptr<const Bitmap> make_placeholder_mine(bool active) {
  BitmapPtr bmp(al_create_bitmap(96, 89));
  if (!bmp) return std::make_shared<const Bitmap>();
  ALLEGRO_BITMAP* previous = al_get_target_bitmap();
  al_set_target_bitmap(bmp.get());
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
  return std::make_shared<const Bitmap>(Bitmap{std::move(bmp)});
}

std::shared_ptr<const Bitmap> load_or(Assets& assets, const std::string& path, bool active_placeholder,
                                      bool placeholder_allowed) {
  auto bmp = assets.bitmap(path);
  if (bmp) return *bmp;
  if (placeholder_allowed) return make_placeholder_mine(active_placeholder);
  std::fprintf(stderr, "warning: %s\n", bmp.error().c_str());
  return nullptr;
}

}  // namespace

void BuildingArt::load(const myrts::Registry& registry, Assets& assets, const std::string& era) {
  for (const auto& def : registry.buildings()) {
    if (auto b = load_or(assets, def.sprite, false, false)) finished_[def.id] = std::move(b);
    if (auto b = load_or(assets, def.sprite_construction, false, false)) construction_[def.id] = std::move(b);
  }
  // The sites of each resource ("{era}" in a path is the terrain era). A missing image is replaced by a drawn placeholder.
  const auto era_path = [&](std::string path) {
    const auto at = path.find("{era}");
    if (at != std::string::npos) path.replace(at, 5, era);
    return path;
  };
  sites_.assign(static_cast<std::size_t>(registry.resources()->size()), {});
  for (myrts::ResourceId r = 0; r < registry.resources()->size(); ++r) {
    const myrts::ResourceDef& def = registry.resources()->at(r);
    if (def.source != myrts::ResourceSource::Site || def.site_sprite.empty()) continue;
    SiteArt& art = sites_[static_cast<std::size_t>(r)];
    art.idle = load_or(assets, era_path(def.site_sprite), false, true);
    if (!def.site_sprite_active.empty()) art.active = load_or(assets, era_path(def.site_sprite_active), true, true);
  }
}

void draw_centered_in_square(ALLEGRO_BITMAP* bitmap, float x, float y, float size_px, ALLEGRO_COLOR tint) {
  if (!bitmap) return;
  const auto w = static_cast<float>(al_get_bitmap_width(bitmap));
  const auto h = static_cast<float>(al_get_bitmap_height(bitmap));
  al_draw_tinted_bitmap(bitmap, tint, x + (size_px - w) / 2, y + (size_px - h) / 2, 0);
}
