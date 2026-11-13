#include <allegro5/allegro_primitives.h>

#include <algorithm>
#include <string>

#include "draw_util.hpp"
#include "scenes.hpp"
#include "ui.hpp"

namespace {

constexpr float kRowW = 320;
constexpr float kRowH = 30;
constexpr float kRowGap = 10;
constexpr float kRowX = (ui::kWidth - kRowW) / 2;
constexpr float kStep = 0.05f;

/// Volume and mute. Up/down choose a row, left/right change the volume, Enter toggles or goes back, the mouse drags the slider.
class OptionsScene : public Scene {
 public:
  explicit OptionsScene(const SceneContext& ctx) : ctx_(ctx) {}

  [[nodiscard]] bool is_overlay() const override { return true; }

  Transition handle_event(const ALLEGRO_EVENT& e) override {
    Audio* audio = ctx_.audio;
    switch (e.type) {
      case ALLEGRO_EVENT_KEY_DOWN:
        switch (e.keyboard.keycode) {
          case ALLEGRO_KEY_ESCAPE: return Transition::pop();
          case ALLEGRO_KEY_UP: selected_ = (selected_ + kRows - 1) % kRows; break;
          case ALLEGRO_KEY_DOWN: selected_ = (selected_ + 1) % kRows; break;
          case ALLEGRO_KEY_LEFT: if (selected_ == 0) nudge(-kStep); break;
          case ALLEGRO_KEY_RIGHT: if (selected_ == 0) nudge(kStep); break;
          case ALLEGRO_KEY_ENTER:
          case ALLEGRO_KEY_PAD_ENTER:
          case ALLEGRO_KEY_SPACE: return activate(selected_);
          default: break;
        }
        break;
      case ALLEGRO_EVENT_MOUSE_AXES: {
        const float x = static_cast<float>(e.mouse.x), y = static_cast<float>(e.mouse.y);
        if (dragging_ && audio) audio->set_volume(slider_value(x));
        else if (const int hit = row_at(x, y); hit >= 0) selected_ = hit;
        break;
      }
      case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
        if (e.mouse.button == 1) {
          const float x = static_cast<float>(e.mouse.x), y = static_cast<float>(e.mouse.y);
          const int hit = row_at(x, y);
          if (hit == 0 && audio) {
            dragging_ = true;
            audio->set_volume(slider_value(x));
          } else if (hit > 0) {
            return activate(hit);
          }
        }
        break;
      case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
        if (e.mouse.button == 1) dragging_ = false;
        break;
      default: break;
    }
    return Transition::none();
  }

  void draw() override {
    const auto w = static_cast<float>(ui::kWidth);
    const auto h = static_cast<float>(ui::kHeight);
    al_draw_filled_rectangle(0, 0, w, h, al_map_rgba_f(0, 0, 0, 0.7f));
    al_draw_filled_rectangle(kRowX - 30, row_y(0) - 90, kRowX + kRowW + 30, row_y(2) + kRowH + 30, al_map_rgb(24, 20, 17));
    al_draw_rectangle(kRowX - 29.5f, row_y(0) - 89.5f, kRowX + kRowW + 29.5f, row_y(2) + kRowH + 29.5f, al_map_rgb(128, 122, 112), 1);
    draw_text_scaled(ctx_.font, w / 2, row_y(0) - 70, 4.0f, ALLEGRO_ALIGN_CENTER, "Options", al_map_rgb(240, 200, 60));

    const Audio* audio = ctx_.audio;
    const bool has_audio = audio && audio->available();
    const float volume = audio ? audio->volume() : 0.0f;
    const bool muted = !audio || audio->muted();

    for (int i = 0; i < kRows; ++i) {
      const float y = row_y(i);
      const bool sel = i == selected_;
      al_draw_filled_rectangle(kRowX, y, kRowX + kRowW, y + kRowH, sel ? al_map_rgb(110, 102, 92) : al_map_rgb(64, 60, 56));
      al_draw_rectangle(kRowX + 0.5f, y + 0.5f, kRowX + kRowW - 0.5f, y + kRowH - 0.5f,
                        sel ? al_map_rgb(240, 200, 60) : al_map_rgb(128, 122, 112), 1);
      const ALLEGRO_COLOR text = sel ? al_map_rgb(255, 240, 180) : al_map_rgb(236, 224, 190);
      std::string label;
      if (i == 0) {
        label = "Volume";
        draw_text_scaled(ctx_.font, kRowX + 10, y + 7, 2.0f, ALLEGRO_ALIGN_LEFT, label, text);
        const float bx = kRowX + kSliderOffset, bw = kRowW - kSliderOffset - 12, by = y + kRowH / 2 - 3;
        al_draw_filled_rectangle(bx, by, bx + bw, by + 6, al_map_rgb(30, 28, 26));
        al_draw_filled_rectangle(bx, by, bx + bw * volume, by + 6, muted ? al_map_rgb(120, 110, 90) : al_map_rgb(240, 200, 60));
        al_draw_filled_rectangle(bx + bw * volume - 3, by - 5, bx + bw * volume + 3, by + 11, al_map_rgb(236, 224, 190));
      } else if (i == 1) {
        label = !has_audio ? "Sound: unavailable" : (muted ? "Sound: Off" : "Sound: On");
        draw_text_scaled(ctx_.font, w / 2, y + 7, 2.0f, ALLEGRO_ALIGN_CENTER, label, text);
      } else {
        draw_text_scaled(ctx_.font, w / 2, y + 7, 2.0f, ALLEGRO_ALIGN_CENTER, "Back", text);
      }
    }
  }

 private:
  static constexpr int kRows = 3;  // volume, mute, back
  static constexpr float kSliderOffset = 120;

  static float row_y(int i) {
    const float block = kRows * kRowH + (kRows - 1) * kRowGap;
    return (ui::kHeight - block) / 2 + 40 + static_cast<float>(i) * (kRowH + kRowGap);
  }
  static int row_at(float x, float y) {
    for (int i = 0; i < kRows; ++i)
      if (x >= kRowX && x < kRowX + kRowW && y >= row_y(i) && y < row_y(i) + kRowH) return i;
    return -1;
  }
  static float slider_value(float x) {
    const float bx = kRowX + kSliderOffset, bw = kRowW - kSliderOffset - 12;
    return std::clamp((x - bx) / bw, 0.0f, 1.0f);
  }
  void nudge(float delta) {
    if (!ctx_.audio) return;
    ctx_.audio->set_volume(ctx_.audio->volume() + delta);
    if (ctx_.audio->muted() && delta > 0) ctx_.audio->set_muted(false);  // raising the volume unmutes
  }
  Transition activate(int row) {
    if (row == 1 && ctx_.audio) ctx_.audio->set_muted(!ctx_.audio->muted());
    if (row == 2) return Transition::pop();
    return Transition::none();
  }

  SceneContext ctx_;
  int selected_ = 0;
  bool dragging_ = false;
};

}  // namespace

SceneFactory make_options() {
  return [](const SceneContext& ctx) -> std::expected<std::unique_ptr<Scene>, std::string> {
    return std::make_unique<OptionsScene>(ctx);
  };
}
