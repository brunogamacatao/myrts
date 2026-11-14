#include <allegro5/allegro_primitives.h>

#include <algorithm>
#include <ctime>
#include <memory>
#include <string>

#include "core/saves.hpp"
#include "draw_util.hpp"
#include "raii.hpp"
#include "scenes.hpp"
#include "ui.hpp"

namespace {

constexpr float kRowW = 480;
constexpr float kRowH = 76;
constexpr float kRowGap = 8;
constexpr float kRowX = (ui::kWidth - kRowW) / 2;
constexpr float kRowTop = 118;
constexpr float kBackY = kRowTop + myrts::SaveFile::kSlots * (kRowH + kRowGap) + 6;
constexpr float kBackW = 220;
constexpr float kBackH = 30;
constexpr int kBackRow = myrts::SaveFile::kSlots;

std::string now_text() {
  const std::time_t t = std::time(nullptr);
  char buf[32] = {};
  std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", std::localtime(&t));
  return buf;
}

/// Four slots, each with the picture of its map, the level and the date. Used to save the match under way and to load one.
class SlotsScene : public Scene {
 public:
  SlotsScene(const SceneContext& ctx, SlotsMode mode, SnapshotFn snapshot)
      : ctx_(ctx), mode_(mode), snapshot_(std::move(snapshot)) {
    reload();
  }

  [[nodiscard]] bool is_overlay() const override { return true; }

  Transition handle_event(const ALLEGRO_EVENT& e) override {
    switch (e.type) {
      case ALLEGRO_EVENT_KEY_DOWN:
        switch (e.keyboard.keycode) {
          case ALLEGRO_KEY_ESCAPE: return Transition::pop();
          case ALLEGRO_KEY_UP: selected_ = (selected_ + kBackRow) % (kBackRow + 1); break;
          case ALLEGRO_KEY_DOWN: selected_ = (selected_ + 1) % (kBackRow + 1); break;
          case ALLEGRO_KEY_ENTER:
          case ALLEGRO_KEY_PAD_ENTER:
          case ALLEGRO_KEY_SPACE: return activate(selected_);
          case ALLEGRO_KEY_DELETE: erase(selected_); break;
          default: break;
        }
        break;
      case ALLEGRO_EVENT_MOUSE_AXES: {
        const int hit = row_at(static_cast<float>(e.mouse.x), static_cast<float>(e.mouse.y));
        if (hit >= 0) selected_ = hit;
        break;
      }
      case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
        if (e.mouse.button == 1) {
          const int hit = row_at(static_cast<float>(e.mouse.x), static_cast<float>(e.mouse.y));
          if (hit >= 0) {
            selected_ = hit;
            return activate(hit);
          }
        }
        break;
      default: break;
    }
    return Transition::none();
  }

  void draw() override {
    const auto w = static_cast<float>(ui::kWidth);
    const auto h = static_cast<float>(ui::kHeight);
    al_draw_filled_rectangle(0, 0, w, h, al_map_rgba_f(0, 0, 0, 0.75f));
    al_draw_filled_rectangle(kRowX - 24, 24, kRowX + kRowW + 24, kBackY + kBackH + 54, al_map_rgb(24, 20, 17));
    al_draw_rectangle(kRowX - 23.5f, 24.5f, kRowX + kRowW + 23.5f, kBackY + kBackH + 53.5f, al_map_rgb(128, 122, 112), 1);
    draw_text_scaled(ctx_.font, w / 2, 40, 3.0f, ALLEGRO_ALIGN_CENTER, mode_ == SlotsMode::Save ? "Save Game" : "Load Game",
                     al_map_rgb(240, 200, 60));

    for (int i = 0; i < myrts::SaveFile::kSlots; ++i) draw_slot(i);

    const bool sel = selected_ == kBackRow;
    const float bx = (w - kBackW) / 2;
    al_draw_filled_rectangle(bx, kBackY, bx + kBackW, kBackY + kBackH, sel ? al_map_rgb(110, 102, 92) : al_map_rgb(64, 60, 56));
    al_draw_rectangle(bx + 0.5f, kBackY + 0.5f, bx + kBackW - 0.5f, kBackY + kBackH - 0.5f,
                      sel ? al_map_rgb(240, 200, 60) : al_map_rgb(128, 122, 112), 1);
    draw_text_scaled(ctx_.font, w / 2, kBackY + 7, 2.0f, ALLEGRO_ALIGN_CENTER, "Back",
                     sel ? al_map_rgb(255, 240, 180) : al_map_rgb(236, 224, 190));

    const std::string hint = message_.empty() ? "Enter: choose   Delete: erase the slot   Esc: back" : message_;
    draw_text_scaled(ctx_.font, w / 2, kBackY + kBackH + 14, 1.0f, ALLEGRO_ALIGN_CENTER, hint,
                     message_.empty() ? al_map_rgb(170, 160, 140) : al_map_rgb(250, 210, 60));
  }

 private:
  static float row_y(int i) { return kRowTop + static_cast<float>(i) * (kRowH + kRowGap); }

  static int row_at(float x, float y) {
    for (int i = 0; i < myrts::SaveFile::kSlots; ++i)
      if (x >= kRowX && x < kRowX + kRowW && y >= row_y(i) && y < row_y(i) + kRowH) return i;
    const float bx = (ui::kWidth - kBackW) / 2;
    if (x >= bx && x < bx + kBackW && y >= kBackY && y < kBackY + kBackH) return kBackRow;
    return -1;
  }

  void draw_slot(int i) {
    const float y = row_y(i);
    const bool sel = i == selected_;
    const auto& slot = file_.slot(i);
    al_draw_filled_rectangle(kRowX, y, kRowX + kRowW, y + kRowH, sel ? al_map_rgb(110, 102, 92) : al_map_rgb(64, 60, 56));
    al_draw_rectangle(kRowX + 0.5f, y + 0.5f, kRowX + kRowW - 0.5f, y + kRowH - 0.5f,
                      sel ? al_map_rgb(240, 200, 60) : al_map_rgb(128, 122, 112), 1);
    const float tx = kRowX + 6, ty = y + 6;
    al_draw_filled_rectangle(tx - 1, ty - 1, tx + 65, ty + 65, al_map_rgb(20, 18, 16));
    if (thumbs_[static_cast<std::size_t>(i)]) al_draw_bitmap(thumbs_[static_cast<std::size_t>(i)].get(), tx, ty, 0);
    const ALLEGRO_COLOR text = sel ? al_map_rgb(255, 240, 180) : al_map_rgb(236, 224, 190);
    const ALLEGRO_COLOR dim = al_map_rgb(170, 160, 140);
    draw_text_scaled(ctx_.font, kRowX + 84, y + 10, 1.0f, ALLEGRO_ALIGN_LEFT, "Slot " + std::to_string(i + 1), dim);
    if (!slot) {
      draw_text_scaled(ctx_.font, kRowX + 84, y + 34, 2.0f, ALLEGRO_ALIGN_LEFT, "Empty", dim);
      return;
    }
    draw_text_scaled(ctx_.font, kRowX + 84, y + 26, fit_scale(ctx_.font, slot->name, 2.0f, kRowW - 96), ALLEGRO_ALIGN_LEFT, slot->name, text);
    draw_text_scaled(ctx_.font, kRowX + 84, y + 50, 1.0f, ALLEGRO_ALIGN_LEFT, faction_name(slot->faction) + "   " + slot->date, dim);
  }

  [[nodiscard]] std::string faction_name(const std::string& id) const {
    if (ctx_.registry)
      if (const myrts::FactionDef* f = ctx_.registry->faction(id)) return f->name;
    return id;
  }

  [[nodiscard]] std::string level_name(int level) const {
    if (!ctx_.campaign) return "Level " + std::to_string(level + 1);
    return "Level " + std::to_string(level + 1) + ": " + ctx_.campaign->level(static_cast<std::size_t>(level)).name;
  }

  /// Reads the file again and rebuilds the pictures.
  void reload() {
    auto loaded = myrts::SaveFile::load(ctx_.save_path);
    if (loaded) {
      file_ = std::move(*loaded);
    } else {
      file_ = {};
      message_ = loaded.error();
    }
    for (int i = 0; i < myrts::SaveFile::kSlots; ++i) thumbs_[static_cast<std::size_t>(i)] = make_thumbnail(file_.slot(i));
  }

  static BitmapPtr make_thumbnail(const std::optional<myrts::SaveSlot>& slot) {
    constexpr int kSize = myrts::SaveFile::kThumbSize;
    if (!slot || slot->thumbnail.empty()) return {};
    const auto bytes = myrts::base64_decode(slot->thumbnail);
    if (!bytes || bytes->size() != static_cast<std::size_t>(kSize * kSize * 3)) return {};
    const int flags = al_get_new_bitmap_flags();
    al_set_new_bitmap_flags(flags & ~ALLEGRO_MEMORY_BITMAP);
    BitmapPtr bitmap(al_create_bitmap(kSize, kSize));
    al_set_new_bitmap_flags(flags);
    if (!bitmap) return {};
    ALLEGRO_BITMAP* previous = al_get_target_bitmap();
    al_set_target_bitmap(bitmap.get());
    al_lock_bitmap(bitmap.get(), ALLEGRO_PIXEL_FORMAT_ANY, ALLEGRO_LOCK_WRITEONLY);
    for (int y = 0; y < kSize; ++y)
      for (int x = 0; x < kSize; ++x) {
        const std::size_t i = static_cast<std::size_t>(y * kSize + x) * 3;
        al_put_pixel(x, y, al_map_rgb((*bytes)[i], (*bytes)[i + 1], (*bytes)[i + 2]));
      }
    al_unlock_bitmap(bitmap.get());
    al_set_target_bitmap(previous);
    return bitmap;
  }

  Transition activate(int row) {
    if (row == kBackRow) return Transition::pop();
    const auto& slot = file_.slot(row);
    if (mode_ == SlotsMode::Load) {
      if (!slot) {
        message_ = "That slot is empty";
        return Transition::none();
      }
      return Transition::reset(make_game(static_cast<std::size_t>(slot->level), slot->faction,
                                         std::make_shared<const std::string>(slot->state)));
    }
    if (!snapshot_) return Transition::none();
    myrts::SaveSlot s = snapshot_();
    s.name = level_name(s.level);
    s.date = now_text();
    file_.set(row, std::move(s));
    if (const auto written = file_.save(ctx_.save_path); written) {
      message_ = "Game saved in slot " + std::to_string(row + 1);
      thumbs_[static_cast<std::size_t>(row)] = make_thumbnail(file_.slot(row));
    } else {
      message_ = written.error();
    }
    return Transition::none();
  }

  void erase(int row) {
    if (row < 0 || row >= myrts::SaveFile::kSlots || !file_.slot(row)) return;
    file_.clear(row);
    thumbs_[static_cast<std::size_t>(row)].reset();
    const auto written = file_.save(ctx_.save_path);
    message_ = written ? "Slot " + std::to_string(row + 1) + " erased" : written.error();
  }

  SceneContext ctx_;
  SlotsMode mode_;
  SnapshotFn snapshot_;
  myrts::SaveFile file_;
  std::array<BitmapPtr, myrts::SaveFile::kSlots> thumbs_;
  int selected_ = 0;
  std::string message_;
};

}  // namespace

SceneFactory make_slots(SlotsMode mode, SnapshotFn snapshot) {
  return [mode, snapshot = std::move(snapshot)](const SceneContext& ctx) -> std::expected<std::unique_ptr<Scene>, std::string> {
    return std::make_unique<SlotsScene>(ctx, mode, snapshot);
  };
}
