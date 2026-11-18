#pragma once

#include <allegro5/allegro5.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "assets.hpp"
#include "core/defs.hpp"

/// Bitmaps of every building in the registry plus the gold mine of the map's era. Sprites are shared through
/// `Assets`; this object only keeps them alive while the game uses them.
class BuildingArt {
 public:
  /// Loads what the registry names. A missing sprite is reported on stderr and that building is
  /// simply not drawn; a missing gold-mine sprite is replaced by a drawn placeholder
  /// (see scripts/extract_sprites.py).
  void load(const myrts::Registry& registry, Assets& assets, const std::string& era);

  [[nodiscard]] ALLEGRO_BITMAP* finished(const myrts::BuildingDef& def) const { return find(finished_, def.id); }
  [[nodiscard]] ALLEGRO_BITMAP* construction(const myrts::BuildingDef& def) const {
    return find(construction_, def.id);
  }
  /// The image of a mine or patch of resource `r` (the "active" one while a worker is inside, when it has one).
  [[nodiscard]] ALLEGRO_BITMAP* site(myrts::ResourceId r, bool active) const {
    if (r < 0 || static_cast<std::size_t>(r) >= sites_.size()) return nullptr;
    const SiteArt& s = sites_[static_cast<std::size_t>(r)];
    const auto& bmp = (active && s.active) ? s.active : s.idle;
    return bmp ? bmp->get() : nullptr;
  }

 private:
  using Map = std::unordered_map<std::string, std::shared_ptr<const Bitmap>>;
  static ALLEGRO_BITMAP* find(const Map& m, const std::string& id) {
    const auto it = m.find(id);
    return it == m.end() ? nullptr : it->second->get();
  }

  Map finished_;
  Map construction_;
  struct SiteArt {
    std::shared_ptr<const Bitmap> idle, active;
  };
  std::vector<SiteArt> sites_;  // by ResourceId
};

/// Draws `bitmap` centred inside the square of `size_px` pixels whose top-left is (x, y).
void draw_centered_in_square(ALLEGRO_BITMAP* bitmap, float x, float y, float size_px,
                             ALLEGRO_COLOR tint = al_map_rgb(255, 255, 255));
