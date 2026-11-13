#pragma once

#include <allegro5/allegro5.h>

#include <expected>
#include <memory>
#include <string>

#include "assets.hpp"
#include "core/geometry.hpp"

/// The Warcraft II interface art: the 46x38 icon atlas (commands, units, buildings), the resource icons and the panel
/// textures. Built by scripts/extract_ui_assets.py; see NOTICE.md.
class UiArt {
 public:
  static constexpr int kIconW = 46;
  static constexpr int kIconH = 38;
  static constexpr int kIconCols = 15;  // icons per row of the atlas

  std::expected<void, std::string> load(Assets& assets);

  /// Draws atlas icon `index` with its top-left at (x, y), optionally scaled and dimmed (alpha 0..1).
  void draw_icon(int index, float x, float y, float scale = 1.0f, float alpha = 1.0f) const;

  /// Fills `area` by repeating a texture.
  static void draw_tiled(ALLEGRO_BITMAP* texture, const myrts::Rect& area);

  [[nodiscard]] ALLEGRO_BITMAP* panel() const { return panel_->get(); }
  [[nodiscard]] ALLEGRO_BITMAP* top_bar() const { return top_bar_->get(); }
  [[nodiscard]] ALLEGRO_BITMAP* minimap_frame() const { return minimap_frame_->get(); }
  [[nodiscard]] ALLEGRO_BITMAP* gold() const { return gold_->get(); }
  [[nodiscard]] ALLEGRO_BITMAP* lumber() const { return lumber_->get(); }
  [[nodiscard]] ALLEGRO_BITMAP* oil() const { return oil_->get(); }
  [[nodiscard]] ALLEGRO_BITMAP* food() const { return food_->get(); }

 private:
  std::shared_ptr<const Bitmap> icons_, gold_, lumber_, oil_, food_, panel_, top_bar_, minimap_frame_;
};
