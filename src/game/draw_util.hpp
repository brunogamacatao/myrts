#pragma once

#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>

#include <string>

/// Draws text with the built-in font magnified by `scale` (with a drop shadow). The position is
/// the anchor given by `align` (ALLEGRO_ALIGN_LEFT / CENTER / RIGHT).
void draw_text_scaled(ALLEGRO_FONT* font, float x, float y, float scale, int align, const std::string& text,
                      ALLEGRO_COLOR color);

/// Largest scale <= `max_scale` at which `text` still fits in `max_width` pixels.
float fit_scale(ALLEGRO_FONT* font, const std::string& text, float max_scale, float max_width);
