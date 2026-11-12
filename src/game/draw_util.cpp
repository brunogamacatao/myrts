#include "draw_util.hpp"

#include <algorithm>

float fit_scale(ALLEGRO_FONT* font, const std::string& text, float max_scale, float max_width) {
  const auto w = static_cast<float>(al_get_text_width(font, text.c_str()));
  if (w <= 0) return max_scale;
  return std::min(max_scale, max_width / w);
}

void draw_text_scaled(ALLEGRO_FONT* font, float x, float y, float scale, int align, const std::string& text,
                      ALLEGRO_COLOR color) {
  ALLEGRO_TRANSFORM previous;
  al_copy_transform(&previous, al_get_current_transform());
  ALLEGRO_TRANSFORM t;
  al_identity_transform(&t);
  al_scale_transform(&t, scale, scale);
  al_translate_transform(&t, x, y);
  al_compose_transform(&t, &previous);
  al_use_transform(&t);
  al_draw_text(font, al_map_rgb(20, 16, 12), 1, 1, align, text.c_str());
  al_draw_text(font, color, 0, 0, align, text.c_str());
  al_use_transform(&previous);
}
