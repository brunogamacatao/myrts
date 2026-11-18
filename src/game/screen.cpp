#include "screen.hpp"

#include <algorithm>
#include <cmath>

#include "ui.hpp"

namespace {
Screen* g_screen = nullptr;
}

Screen* screen() { return g_screen; }
void set_screen(Screen* s) { g_screen = s; }

bool Screen::init(ALLEGRO_DISPLAY* display) {
  display_ = display;
  const int flags = al_get_new_bitmap_flags();
  al_set_new_bitmap_flags((flags & ~ALLEGRO_MEMORY_BITMAP) | ALLEGRO_MIN_LINEAR | ALLEGRO_MAG_LINEAR);
  canvas_.reset(al_create_bitmap(ui::kWidth, ui::kHeight));
  al_set_new_bitmap_flags(flags);
  return canvas_ != nullptr;
}

Screen::Fit Screen::fit() const {
  const auto dw = static_cast<float>(al_get_display_width(display_));
  const auto dh = static_cast<float>(al_get_display_height(display_));
  Fit f;
  f.scale = std::min(dw / static_cast<float>(ui::kWidth), dh / static_cast<float>(ui::kHeight));
  if (f.scale <= 0) f.scale = 1;
  f.x = std::floor((dw - static_cast<float>(ui::kWidth) * f.scale) / 2);
  f.y = std::floor((dh - static_cast<float>(ui::kHeight) * f.scale) / 2);
  return f;
}

void Screen::begin_frame() {
  if (canvas_) al_set_target_bitmap(canvas_.get());
}

void Screen::end_frame() {
  if (canvas_) {
    al_set_target_backbuffer(display_);
    al_set_clipping_rectangle(0, 0, al_get_display_width(display_), al_get_display_height(display_));
    al_clear_to_color(al_map_rgb(0, 0, 0));
    const Fit f = fit();
    al_draw_scaled_bitmap(canvas_.get(), 0, 0, static_cast<float>(ui::kWidth), static_cast<float>(ui::kHeight), f.x, f.y,
                          static_cast<float>(ui::kWidth) * f.scale, static_cast<float>(ui::kHeight) * f.scale, 0);
  }
  al_flip_display();
}

ALLEGRO_EVENT Screen::to_game(const ALLEGRO_EVENT& e) const {
  ALLEGRO_EVENT out = e;
  if (!canvas_) return out;
  switch (e.type) {
    case ALLEGRO_EVENT_MOUSE_AXES:
    case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
    case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
    case ALLEGRO_EVENT_MOUSE_ENTER_DISPLAY:
    case ALLEGRO_EVENT_MOUSE_LEAVE_DISPLAY: {
      const Fit f = fit();
      out.mouse.x = static_cast<int>(std::lround((static_cast<float>(e.mouse.x) - f.x) / f.scale));
      out.mouse.y = static_cast<int>(std::lround((static_cast<float>(e.mouse.y) - f.y) / f.scale));
      out.mouse.dx = static_cast<int>(std::lround(static_cast<float>(e.mouse.dx) / f.scale));
      out.mouse.dy = static_cast<int>(std::lround(static_cast<float>(e.mouse.dy) / f.scale));
      break;
    }
    default: break;
  }
  return out;
}

bool Screen::fullscreen() const { return (al_get_display_flags(display_) & ALLEGRO_FULLSCREEN_WINDOW) != 0; }

void Screen::set_fullscreen(bool on) {
  if (on != fullscreen()) al_toggle_display_flag(display_, ALLEGRO_FULLSCREEN_WINDOW, on);
}
