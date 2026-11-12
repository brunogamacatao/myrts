#include "menu_scene.hpp"

#include <allegro5/allegro_primitives.h>

#include "draw_util.hpp"
#include "ui.hpp"

namespace {
constexpr float kButtonW = 260;
constexpr float kButtonH = 30;
constexpr float kButtonGap = 10;
constexpr float kButtonX = (ui::kWidth - kButtonW) / 2;
}  // namespace

float MenuScene::button_y(std::size_t i) const {
  const auto n = static_cast<float>(cfg_.items.size());
  const float block = n * kButtonH + (n - 1) * kButtonGap;
  const float top = (ui::kHeight - block) / 2 + 40;
  return top + static_cast<float>(i) * (kButtonH + kButtonGap);
}

int MenuScene::button_at(float x, float y) const {
  for (std::size_t i = 0; i < cfg_.items.size(); ++i) {
    const float by = button_y(i);
    if (x >= kButtonX && x < kButtonX + kButtonW && y >= by && y < by + kButtonH) return static_cast<int>(i);
  }
  return -1;
}

Transition MenuScene::handle_event(const ALLEGRO_EVENT& e) {
  const auto count = static_cast<int>(cfg_.items.size());
  if (count == 0) return Transition::none();

  const auto activate = [&](int i) { return cfg_.items[static_cast<std::size_t>(i)].action(); };

  switch (e.type) {
    case ALLEGRO_EVENT_KEY_DOWN:
      switch (e.keyboard.keycode) {
        case ALLEGRO_KEY_UP: selected_ = (selected_ + count - 1) % count; break;
        case ALLEGRO_KEY_DOWN: selected_ = (selected_ + 1) % count; break;
        case ALLEGRO_KEY_ENTER:
        case ALLEGRO_KEY_PAD_ENTER:
        case ALLEGRO_KEY_SPACE: return activate(selected_);
        case ALLEGRO_KEY_ESCAPE:
          if (cfg_.on_escape) return cfg_.on_escape();
          break;
        default: break;
      }
      break;
    case ALLEGRO_EVENT_MOUSE_AXES: {
      const int hit = button_at(static_cast<float>(e.mouse.x), static_cast<float>(e.mouse.y));
      if (hit >= 0) selected_ = hit;
      break;
    }
    case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
      if (e.mouse.button == 1) {
        const int hit = button_at(static_cast<float>(e.mouse.x), static_cast<float>(e.mouse.y));
        if (hit >= 0) return activate(hit);
      }
      break;
    default:
      break;
  }
  return Transition::none();
}

void MenuScene::draw() {
  const auto w = static_cast<float>(ui::kWidth);
  const auto h = static_cast<float>(ui::kHeight);

  if (cfg_.overlay) {
    al_draw_filled_rectangle(0, 0, w, h, al_map_rgba_f(0, 0, 0, 0.6f));
  } else {
    for (int y = 0; y < ui::kHeight; y += 4) {  // dark vertical gradient
      const float f = static_cast<float>(y) / h;
      al_draw_filled_rectangle(0, static_cast<float>(y), w, static_cast<float>(y + 4),
                               al_map_rgb(static_cast<unsigned char>(24 + 22 * (1 - f)),
                                          static_cast<unsigned char>(20 + 16 * (1 - f)),
                                          static_cast<unsigned char>(16 + 10 * (1 - f))));
    }
  }

  const float first = cfg_.items.empty() ? h / 2 : button_y(0);
  draw_text_scaled(ctx_.font, w / 2, first - 100, fit_scale(ctx_.font, cfg_.title, 4.0f, w - 40),
                   ALLEGRO_ALIGN_CENTER, cfg_.title, cfg_.title_color);
  if (!cfg_.subtitle.empty()) {
    draw_text_scaled(ctx_.font, w / 2, first - 52, fit_scale(ctx_.font, cfg_.subtitle, 1.5f, w - 40),
                     ALLEGRO_ALIGN_CENTER, cfg_.subtitle,
                     al_map_rgb(236, 224, 190));
  }

  for (std::size_t i = 0; i < cfg_.items.size(); ++i) {
    const float y = button_y(i);
    const bool sel = static_cast<int>(i) == selected_;
    al_draw_filled_rectangle(kButtonX, y, kButtonX + kButtonW, y + kButtonH,
                             sel ? al_map_rgb(110, 102, 92) : al_map_rgb(64, 60, 56));
    al_draw_rectangle(kButtonX + 0.5f, y + 0.5f, kButtonX + kButtonW - 0.5f, y + kButtonH - 0.5f,
                      sel ? al_map_rgb(240, 200, 60) : al_map_rgb(128, 122, 112), 1);
    const float s = fit_scale(ctx_.font, cfg_.items[i].label, 2.0f, kButtonW - 16);
    draw_text_scaled(ctx_.font, w / 2, y + (kButtonH - 8 * s) / 2, s, ALLEGRO_ALIGN_CENTER, cfg_.items[i].label,
                     sel ? al_map_rgb(255, 240, 180) : al_map_rgb(236, 224, 190));
  }
}
