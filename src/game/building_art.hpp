#pragma once

#include <allegro5/allegro5.h>

#include "core/building.hpp"

/// Bitmaps of the buildings and the gold mine, loaded once from assets/buildings/.
class BuildingArt {
 public:
  BuildingArt() = default;
  BuildingArt(const BuildingArt&) = delete;
  BuildingArt& operator=(const BuildingArt&) = delete;
  ~BuildingArt();

  /// Loads every sprite. Missing gold-mine sprites are replaced by a drawn placeholder
  /// (see scripts/extract_sprites.py); missing building sprites are an error.
  bool load();

  [[nodiscard]] ALLEGRO_BITMAP* finished(myrts::BuildingKind k) const { return finished_[index(k)]; }
  [[nodiscard]] ALLEGRO_BITMAP* construction(myrts::BuildingKind k) const { return construction_[index(k)]; }
  [[nodiscard]] ALLEGRO_BITMAP* gold_mine(bool active) const { return active ? mine_active_ : mine_; }

 private:
  static constexpr std::size_t index(myrts::BuildingKind k) { return static_cast<std::size_t>(k); }
  static constexpr std::size_t kKinds = 3;

  ALLEGRO_BITMAP* finished_[kKinds] = {};
  ALLEGRO_BITMAP* construction_[kKinds] = {};
  ALLEGRO_BITMAP* mine_ = nullptr;
  ALLEGRO_BITMAP* mine_active_ = nullptr;
};

/// Draws `bitmap` centred inside the square of `size_px` pixels whose top-left is (x, y).
void draw_centered_in_square(ALLEGRO_BITMAP* bitmap, float x, float y, float size_px,
                             ALLEGRO_COLOR tint = al_map_rgb(255, 255, 255));
