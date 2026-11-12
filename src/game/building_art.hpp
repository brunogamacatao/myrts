#pragma once

#include <allegro5/allegro5.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "assets.hpp"
#include "core/defs.hpp"

/// Bitmaps of every building in the registry plus the gold mine. Sprites are shared through
/// `Assets`; this object only keeps them alive while the game uses them.
class BuildingArt {
 public:
  /// Loads what the registry names. A missing sprite is reported on stderr and that building is
  /// simply not drawn; a missing gold-mine sprite is replaced by a drawn placeholder
  /// (see scripts/extract_sprites.py).
  void load(const myrts::Registry& registry, Assets& assets);

  [[nodiscard]] ALLEGRO_BITMAP* finished(const myrts::BuildingDef& def) const { return find(finished_, def.id); }
  [[nodiscard]] ALLEGRO_BITMAP* construction(const myrts::BuildingDef& def) const {
    return find(construction_, def.id);
  }
  [[nodiscard]] ALLEGRO_BITMAP* gold_mine(bool active) const { return (active ? mine_active_ : mine_)->get(); }

 private:
  using Map = std::unordered_map<std::string, std::shared_ptr<const Bitmap>>;
  static ALLEGRO_BITMAP* find(const Map& m, const std::string& id) {
    const auto it = m.find(id);
    return it == m.end() ? nullptr : it->second->get();
  }

  Map finished_;
  Map construction_;
  std::shared_ptr<const Bitmap> mine_;
  std::shared_ptr<const Bitmap> mine_active_;
};

/// Draws `bitmap` centred inside the square of `size_px` pixels whose top-left is (x, y).
void draw_centered_in_square(ALLEGRO_BITMAP* bitmap, float x, float y, float size_px,
                             ALLEGRO_COLOR tint = al_map_rgb(255, 255, 255));
