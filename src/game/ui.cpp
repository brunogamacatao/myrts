#include "ui.hpp"

#include <allegro5/allegro_primitives.h>

#include <algorithm>
#include <cmath>

namespace ui {
namespace {

using myrts::Rect;

const ALLEGRO_COLOR kStone = al_map_rgb(78, 74, 70);
const ALLEGRO_COLOR kStoneLight = al_map_rgb(128, 122, 112);
const ALLEGRO_COLOR kStoneDark = al_map_rgb(30, 28, 26);
const ALLEGRO_COLOR kInset = al_map_rgb(20, 18, 16);
const ALLEGRO_COLOR kText = al_map_rgb(236, 224, 190);
const ALLEGRO_COLOR kGold = al_map_rgb(240, 200, 40);
const ALLEGRO_COLOR kGreen = al_map_rgb(60, 220, 60);

float fx(int v) { return static_cast<float>(v); }

// Beveled stone-like rectangle; `sunken` inverts the lighting.
void draw_bevel(const Rect& r, bool sunken, ALLEGRO_COLOR fill) {
  const ALLEGRO_COLOR hi = sunken ? kStoneDark : kStoneLight;
  const ALLEGRO_COLOR lo = sunken ? kStoneLight : kStoneDark;
  al_draw_filled_rectangle(r.x, r.y, r.x + r.w, r.y + r.h, fill);
  al_draw_line(r.x, r.y + 0.5f, r.x + r.w, r.y + 0.5f, hi, 1);
  al_draw_line(r.x + 0.5f, r.y, r.x + 0.5f, r.y + r.h, hi, 1);
  al_draw_line(r.x, r.y + r.h - 0.5f, r.x + r.w, r.y + r.h - 0.5f, lo, 1);
  al_draw_line(r.x + r.w - 0.5f, r.y, r.x + r.w - 0.5f, r.y + r.h, lo, 1);
}

void draw_text(ALLEGRO_FONT* font, float x, float y, int align, const std::string& text,
               ALLEGRO_COLOR color = kText) {
  al_draw_text(font, kStoneDark, x + 1, y + 1, align, text.c_str());  // drop shadow
  al_draw_text(font, color, x, y, align, text.c_str());
}

void draw_command_glyph(Command c, const Rect& r, float alpha) {
  const float cx = r.x + r.w / 2;
  const float cy = r.y + r.h / 2;
  const auto a = [alpha](int rr, int g, int b) { return al_map_rgba_f(static_cast<float>(rr) / 255 * alpha,
                                                                      static_cast<float>(g) / 255 * alpha,
                                                                      static_cast<float>(b) / 255 * alpha, alpha); };
  switch (c) {
    case Command::Move:  // arrow pointing right
      al_draw_filled_rectangle(cx - 14, cy - 3, cx + 4, cy + 3, a(220, 60, 50));
      al_draw_filled_triangle(cx + 2, cy - 11, cx + 2, cy + 11, cx + 16, cy, a(220, 60, 50));
      break;
    case Command::Stop:
      al_draw_filled_rectangle(cx - 11, cy - 11, cx + 11, cy + 11, a(200, 40, 40));
      al_draw_rectangle(cx - 11, cy - 11, cx + 11, cy + 11, a(250, 200, 190), 2);
      break;
    case Command::Attack:
      al_draw_line(cx - 12, cy + 12, cx + 12, cy - 12, a(210, 210, 220), 3);
      al_draw_line(cx - 12, cy - 12, cx + 12, cy + 12, a(210, 210, 220), 3);
      break;
    case Command::Repair:
      al_draw_circle(cx - 6, cy - 6, 7, a(200, 200, 210), 3);
      al_draw_line(cx - 2, cy - 2, cx + 12, cy + 12, a(200, 200, 210), 4);
      break;
    case Command::Harvest:
      al_draw_filled_triangle(cx - 12, cy + 6, cx + 12, cy + 6, cx, cy - 14, a(40, 140, 50));
      al_draw_filled_rectangle(cx - 3, cy + 6, cx + 3, cy + 14, a(110, 70, 30));
      break;
    case Command::Build:
      al_draw_filled_rectangle(cx - 12, cy - 2, cx + 12, cy + 12, a(170, 130, 80));
      al_draw_filled_triangle(cx - 15, cy - 2, cx + 15, cy - 2, cx, cy - 14, a(140, 40, 40));
      break;
  }
}

void draw_resource(ALLEGRO_FONT* font, float x, int kind, int value) {
  const float y = 12;
  switch (kind) {
    case 0: al_draw_filled_circle(x, y, 6, kGold); al_draw_circle(x, y, 6, kStoneDark, 1); break;
    case 1: al_draw_filled_triangle(x - 6, y + 6, x + 6, y + 6, x, y - 7, al_map_rgb(50, 160, 60)); break;
    default: al_draw_filled_circle(x, y + 1, 6, al_map_rgb(40, 40, 60)); al_draw_filled_triangle(x - 5, y, x + 5, y, x, y - 8, al_map_rgb(40, 40, 60)); break;
  }
  draw_text(font, x + 12, 8, ALLEGRO_ALIGN_LEFT, std::to_string(value));
}

void draw_minimap(const HudData& hud) {
  const Rect& area = kMinimap;
  draw_bevel({area.x - 3, area.y - 3, area.w + 6, area.h + 6}, true, kInset);

  const auto& mm = hud.minimap;
  al_draw_scaled_bitmap(hud.minimap_bitmap, 0, 0, static_cast<float>(al_get_bitmap_width(hud.minimap_bitmap)),
                        static_cast<float>(al_get_bitmap_height(hud.minimap_bitmap)), area.x + mm.offset_x(),
                        area.y + mm.offset_y(), mm.width(), mm.height(), 0);

  for (const Unit& u : hud.units) {
    const auto p = mm.world_to_mini(u.x(), u.y());
    al_draw_filled_rectangle(area.x + p.x - 1, area.y + p.y - 1, area.x + p.x + 1.5f, area.y + p.y + 1.5f,
                             u.selected ? kGreen : al_map_rgb(240, 220, 60));
  }

  const Rect view = hud.camera.visible();
  const auto tl = mm.world_to_mini(view.x, view.y);
  const auto br = mm.world_to_mini(view.x + view.w, view.y + view.h);
  al_set_clipping_rectangle(static_cast<int>(area.x), static_cast<int>(area.y), static_cast<int>(area.w),
                            static_cast<int>(area.h));
  al_draw_rectangle(area.x + tl.x, area.y + tl.y, area.x + br.x, area.y + br.y, al_map_rgb(255, 255, 255), 1);
  al_set_clipping_rectangle(0, 0, kWidth, kHeight);
}

void draw_unit_info(ALLEGRO_FONT* font, const HudData& hud) {
  const Rect& p = kInfoPanel;
  draw_bevel(p, true, kInset);
  if (hud.selected.empty()) return;

  if (hud.selected.size() == 1) {
    const Unit& u = hud.units[static_cast<std::size_t>(hud.selected[0])];
    const Rect portrait{p.x + 6, p.y + 6, 52, 52};
    draw_bevel(portrait, true, al_map_rgb(40, 50, 40));
    draw_sprite_fit(*u.type().sprite, u.animation(), portrait.x + 2, portrait.y + 2, portrait.w - 4, portrait.h - 4);

    draw_text(font, p.x + 100, p.y + 12, ALLEGRO_ALIGN_CENTER, u.type().name);
    draw_text(font, p.x + 100, p.y + 28, ALLEGRO_ALIGN_CENTER, "Level " + std::to_string(u.type().level));

    // HP bar + numbers under the portrait
    const float frac = static_cast<float>(u.hp()) / static_cast<float>(u.type().max_hp);
    al_draw_filled_rectangle(portrait.x, portrait.y + 58, portrait.x + portrait.w, portrait.y + 64, kInset);
    al_draw_filled_rectangle(portrait.x, portrait.y + 58, portrait.x + portrait.w * frac, portrait.y + 64,
                             frac > 0.5f ? kGreen : (frac > 0.25f ? kGold : al_map_rgb(220, 50, 40)));
    draw_text(font, portrait.x + portrait.w / 2, portrait.y + 67, ALLEGRO_ALIGN_CENTER,
              std::to_string(u.hp()) + "/" + std::to_string(u.type().max_hp));

    const auto& s = u.type().stats;
    const std::string lines[] = {"Armor: " + std::to_string(s.armor),
                                 "Damage: " + std::to_string(s.damage_min) + "-" + std::to_string(s.damage_max),
                                 "Range: " + std::to_string(s.range), "Sight: " + std::to_string(s.sight),
                                 "Speed: " + std::to_string(s.speed)};
    float y = p.y + 104;
    for (const auto& line : lines) {
      draw_text(font, p.x + 24, y, ALLEGRO_ALIGN_LEFT, line);
      y += 16;
    }
    return;
  }

  // Several units: grid of small portraits.
  draw_text(font, p.x + p.w / 2, p.y + 8, ALLEGRO_ALIGN_CENTER, std::to_string(hud.selected.size()) + " units");
  constexpr int kCols = 3, kCell = 46, kMaxShown = 15;
  const float left = p.x + (p.w - kCols * kCell) / 2;
  int shown = 0;
  for (const int id : hud.selected) {
    if (shown >= kMaxShown) break;
    const Unit& u = hud.units[static_cast<std::size_t>(id)];
    const Rect cell{left + static_cast<float>(shown % kCols * kCell), p.y + 24 + static_cast<float>(shown / kCols * kCell),
                    kCell - 2, kCell - 2};
    draw_bevel(cell, true, al_map_rgb(40, 50, 40));
    draw_sprite_fit(*u.type().sprite, u.animation(), cell.x + 2, cell.y + 2, cell.w - 4, cell.h - 10);
    const float frac = static_cast<float>(u.hp()) / static_cast<float>(u.type().max_hp);
    al_draw_filled_rectangle(cell.x + 2, cell.y + cell.h - 6, cell.x + cell.w - 2, cell.y + cell.h - 3, kInset);
    al_draw_filled_rectangle(cell.x + 2, cell.y + cell.h - 6, cell.x + 2 + (cell.w - 4) * frac, cell.y + cell.h - 3, kGreen);
    ++shown;
  }
  if (hud.selected.size() > kMaxShown) {
    draw_text(font, p.x + p.w - 6, p.y + p.h - 12, ALLEGRO_ALIGN_RIGHT,
              "+" + std::to_string(hud.selected.size() - kMaxShown));
  }
}

}  // namespace

Rect button_rect(int slot) {
  constexpr float kSize = 44, kGap = 4;
  const float left = (kPanelW - (kButtonCols * kSize + (kButtonCols - 1) * kGap)) / 2;
  return {left + static_cast<float>(slot % kButtonCols) * (kSize + kGap),
          440 + static_cast<float>(slot / kButtonCols) * (kSize + kGap), kSize, kSize};
}

std::optional<int> button_at(float x, float y) {
  for (int i = 0; i < kButtonSlots; ++i) {
    if (button_rect(i).contains(x, y)) return i;
  }
  return std::nullopt;
}

std::vector<Command> panel_commands(const std::vector<Unit>& units, std::span<const int> selected) {
  if (selected.empty()) return {};
  return units[static_cast<std::size_t>(selected.front())].type().commands;
}

void draw_hud(ALLEGRO_FONT* font, const HudData& hud) {
  // Left panel
  draw_bevel({0, 0, fx(kPanelW), fx(kHeight)}, false, kStone);
  draw_bevel(kMenuButton, false, al_map_rgb(40, 50, 110));
  draw_text(font, kMenuButton.x + kMenuButton.w / 2, kMenuButton.y + 5, ALLEGRO_ALIGN_CENTER, "Menu (F10)");

  draw_minimap(hud);
  draw_unit_info(font, hud);

  // Command buttons (only when something is selected)
  const auto commands = panel_commands(hud.units, hud.selected);
  for (int slot = 0; slot < kButtonSlots; ++slot) {
    const Rect r = button_rect(slot);
    const bool has = static_cast<std::size_t>(slot) < commands.size();
    const bool hovered = has && r.contains(hud.mouse_x, hud.mouse_y);
    draw_bevel(r, !has, has ? (hovered ? al_map_rgb(100, 94, 86) : al_map_rgb(64, 60, 56)) : kInset);
    if (!has) continue;
    const Command c = commands[static_cast<std::size_t>(slot)];
    draw_command_glyph(c, r, is_implemented(c) ? 1.0f : 0.35f);
    if (c == Command::Move && hud.awaiting_target) al_draw_rectangle(r.x + 2, r.y + 2, r.x + r.w - 2, r.y + r.h - 2, kGreen, 2);
    if (hovered) draw_text(font, kPanelW / 2.0f, 552, ALLEGRO_ALIGN_CENTER, command_label(c));
  }

  // Top bar with resources
  draw_bevel({fx(kPanelW), 0, fx(kWidth - kPanelW), fx(kTopBar)}, false, kStone);
  const float slot_w = fx(kWidth - kPanelW) / 3;
  draw_resource(font, kPanelW + slot_w * 0.5f - 30, 0, hud.resources.gold);
  draw_resource(font, kPanelW + slot_w * 1.5f - 30, 1, hud.resources.lumber);
  draw_resource(font, kPanelW + slot_w * 2.5f - 30, 2, hud.resources.oil);

  // Status message
  if (!hud.message.empty()) {
    draw_text(font, kViewport.x + 12, kViewport.y + kViewport.h - 20, ALLEGRO_ALIGN_LEFT, hud.message, al_map_rgb(250, 210, 60));
  }
}

}  // namespace ui
