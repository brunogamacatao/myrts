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

void draw_command_glyph(const std::string& icon, const Rect& r, float alpha) {
  const float cx = r.x + r.w / 2;
  const float cy = r.y + r.h / 2;
  const auto a = [alpha](int rr, int g, int b) { return al_map_rgba_f(static_cast<float>(rr) / 255 * alpha,
                                                                      static_cast<float>(g) / 255 * alpha,
                                                                      static_cast<float>(b) / 255 * alpha, alpha); };
  if (icon == "move") {  // arrow pointing right
    al_draw_filled_rectangle(cx - 14, cy - 3, cx + 4, cy + 3, a(220, 60, 50));
    al_draw_filled_triangle(cx + 2, cy - 11, cx + 2, cy + 11, cx + 16, cy, a(220, 60, 50));
  } else if (icon == "stop") {
    al_draw_filled_rectangle(cx - 11, cy - 11, cx + 11, cy + 11, a(200, 40, 40));
    al_draw_rectangle(cx - 11, cy - 11, cx + 11, cy + 11, a(250, 200, 190), 2);
  } else if (icon == "attack" || icon == "cancel") {  // crossed swords double as "cancel"
    al_draw_line(cx - 12, cy + 12, cx + 12, cy - 12, a(210, 210, 220), 3);
    al_draw_line(cx - 12, cy - 12, cx + 12, cy + 12, a(210, 210, 220), 3);
  } else if (icon == "repair") {
    al_draw_circle(cx - 6, cy - 6, 7, a(200, 200, 210), 3);
    al_draw_line(cx - 2, cy - 2, cx + 12, cy + 12, a(200, 200, 210), 4);
  } else if (icon == "harvest") {
    al_draw_filled_triangle(cx - 12, cy + 6, cx + 12, cy + 6, cx, cy - 14, a(40, 140, 50));
    al_draw_filled_rectangle(cx - 3, cy + 6, cx + 3, cy + 14, a(110, 70, 30));
  } else if (icon == "build") {
    al_draw_filled_rectangle(cx - 12, cy - 2, cx + 12, cy + 12, a(170, 130, 80));
    al_draw_filled_triangle(cx - 15, cy - 2, cx + 15, cy - 2, cx, cy - 14, a(140, 40, 40));
  } else {  // unknown icon: a plain box so a data typo is visible but harmless
    al_draw_rectangle(cx - 10, cy - 10, cx + 10, cy + 10, a(200, 200, 200), 2);
  }
}

/// Command panel while the build menu is open: one button per building plus a cancel button.
void draw_build_menu(ALLEGRO_FONT* font, const HudData& hud) {
  for (int slot = 0; slot < kButtonSlots; ++slot) {
    const Rect r = button_rect(slot);
    const bool is_cancel = slot == kBuildMenuCancel;
    const BuildOption* opt =
        static_cast<std::size_t>(slot) < hud.build_options.size() && !is_cancel ? &hud.build_options[static_cast<std::size_t>(slot)]
                                                                                 : nullptr;
    const bool has = opt || is_cancel;
    const bool hovered = has && r.contains(hud.mouse_x, hud.mouse_y);
    draw_bevel(r, !has, has ? (hovered ? al_map_rgb(100, 94, 86) : al_map_rgb(64, 60, 56)) : kInset);
    if (!has) continue;
    if (is_cancel) {
      draw_command_glyph("cancel", r, 1.0f);
      if (hovered) draw_text(font, kPanelW / 2.0f, 552, ALLEGRO_ALIGN_CENTER, "Cancel");
      continue;
    }
    const myrts::BuildingDef& def = *opt->def;
    if (opt->icon) {
      const auto bw = static_cast<float>(al_get_bitmap_width(opt->icon));
      const auto bh = static_cast<float>(al_get_bitmap_height(opt->icon));
      const float scale = std::min((r.w - 6) / bw, (r.h - 6) / bh);
      const float alpha = opt->affordable ? 1.0f : 0.35f;
      al_draw_tinted_scaled_bitmap(opt->icon, al_map_rgba_f(alpha, alpha, alpha, alpha), 0, 0, bw, bh,
                                   r.x + (r.w - bw * scale) / 2, r.y + (r.h - bh * scale) / 2, bw * scale,
                                   bh * scale, 0);
    }
    if (opt->placing) al_draw_rectangle(r.x + 2, r.y + 2, r.x + r.w - 2, r.y + r.h - 2, kGreen, 2);
    if (hovered) {
      draw_text(font, kPanelW / 2.0f, 552, ALLEGRO_ALIGN_CENTER, def.name,
                opt->affordable ? kText : al_map_rgb(230, 80, 60));
      draw_text(font, kPanelW / 2.0f, 566, ALLEGRO_ALIGN_CENTER,
                std::to_string(def.cost.gold) + "g " + std::to_string(def.cost.lumber) + "w " +
                    std::to_string(static_cast<int>(def.build_seconds)) + "s");
    }
  }
}

void draw_resource(ALLEGRO_FONT* font, float x, int kind, const std::string& text, ALLEGRO_COLOR color = kText) {
  const float y = 12;
  switch (kind) {
    case 0: al_draw_filled_circle(x, y, 6, kGold); al_draw_circle(x, y, 6, kStoneDark, 1); break;
    case 1: al_draw_filled_triangle(x - 6, y + 6, x + 6, y + 6, x, y - 7, al_map_rgb(50, 160, 60)); break;
    case 2: al_draw_filled_circle(x, y + 1, 6, al_map_rgb(40, 40, 60)); al_draw_filled_triangle(x - 5, y, x + 5, y, x, y - 8, al_map_rgb(40, 40, 60)); break;
    default:  // food: a little house
      al_draw_filled_rectangle(x - 5, y - 1, x + 5, y + 6, al_map_rgb(200, 150, 90));
      al_draw_filled_triangle(x - 7, y - 1, x + 7, y - 1, x, y - 8, al_map_rgb(170, 60, 50));
      break;
  }
  draw_text(font, x + 12, 8, ALLEGRO_ALIGN_LEFT, text, color);
}

void draw_minimap(const HudData& hud) {
  const Rect& area = kMinimap;
  draw_bevel({area.x - 3, area.y - 3, area.w + 6, area.h + 6}, true, kInset);

  const auto& mm = hud.minimap;
  al_draw_scaled_bitmap(hud.minimap_bitmap, 0, 0, static_cast<float>(al_get_bitmap_width(hud.minimap_bitmap)),
                        static_cast<float>(al_get_bitmap_height(hud.minimap_bitmap)), area.x + mm.offset_x(),
                        area.y + mm.offset_y(), mm.width(), mm.height(), 0);

  for (const MinimapDot& d : hud.dots) {
    const auto p = mm.world_to_mini(d.x, d.y);
    const ALLEGRO_COLOR color = d.enemy ? al_map_rgb(230, 60, 50) : (d.selected ? kGreen : al_map_rgb(240, 220, 60));
    const float h = d.size / 2;
    al_draw_filled_rectangle(area.x + p.x - h, area.y + p.y - h, area.x + p.x + h, area.y + p.y + h, color);
  }

  const Rect view = hud.camera.visible();
  const auto tl = mm.world_to_mini(view.x, view.y);
  const auto br = mm.world_to_mini(view.x + view.w, view.y + view.h);
  al_set_clipping_rectangle(static_cast<int>(area.x), static_cast<int>(area.y), static_cast<int>(area.w),
                            static_cast<int>(area.h));
  al_draw_rectangle(area.x + tl.x, area.y + tl.y, area.x + br.x, area.y + br.y, al_map_rgb(255, 255, 255), 1);
  al_set_clipping_rectangle(0, 0, kWidth, kHeight);
}

constexpr int kQueueCells = 5;
constexpr float kQueueCell = 26;

Rect queue_cell_rect(int i) {
  const Rect& p = kInfoPanel;
  return {p.x + 6 + static_cast<float>(i) * (kQueueCell + 2), p.y + 150, kQueueCell, kQueueCell};
}

void draw_building_info(ALLEGRO_FONT* font, const HudData& hud) {
  const Rect& p = kInfoPanel;
  const BuildingCard& b = *hud.building;
  draw_text(font, p.x + p.w / 2, p.y + 10, ALLEGRO_ALIGN_CENTER, b.name);

  const float frac = static_cast<float>(b.hp) / static_cast<float>(b.max_hp);
  al_draw_filled_rectangle(p.x + 10, p.y + 30, p.x + p.w - 10, p.y + 38, kInset);
  al_draw_filled_rectangle(p.x + 10, p.y + 30, p.x + 10 + (p.w - 20) * frac, p.y + 38,
                           frac > 0.5f ? kGreen : (frac > 0.25f ? kGold : al_map_rgb(220, 50, 40)));
  draw_text(font, p.x + p.w / 2, p.y + 42, ALLEGRO_ALIGN_CENTER, std::to_string(b.hp) + "/" + std::to_string(b.max_hp));

  if (!b.complete) {
    draw_text(font, p.x + p.w / 2, p.y + 70, ALLEGRO_ALIGN_CENTER, "Under construction");
    al_draw_filled_rectangle(p.x + 10, p.y + 86, p.x + p.w - 10, p.y + 94, kInset);
    al_draw_filled_rectangle(p.x + 10, p.y + 86, p.x + 10 + (p.w - 20) * b.build_progress, p.y + 94, kGold);
    return;
  }
  if (hud.train_options.empty()) return;

  draw_text(font, p.x + 8, p.y + 130, ALLEGRO_ALIGN_LEFT, "Training");
  for (int i = 0; i < kQueueCells; ++i) {
    const Rect cell = queue_cell_rect(i);
    draw_bevel(cell, true, kInset);
    if (static_cast<std::size_t>(i) >= b.queue.size()) continue;
    const QueueEntry& q = b.queue[static_cast<std::size_t>(i)];
    if (q.sprite && q.anim) draw_sprite_fit(*q.sprite, *q.anim, cell.x + 2, cell.y + 2, cell.w - 4, cell.h - 7);
    if (i == 0)  // progress of the unit being trained
      al_draw_filled_rectangle(cell.x + 2, cell.y + cell.h - 5, cell.x + 2 + (cell.w - 4) * q.progress,
                               cell.y + cell.h - 2, kGreen);
  }
  if (!b.queue.empty()) draw_text(font, p.x + 8, p.y + 182, ALLEGRO_ALIGN_LEFT, "click to cancel");
}

/// Command panel while a building is selected: one button per trainable unit.
void draw_train_menu(ALLEGRO_FONT* font, const HudData& hud) {
  for (int slot = 0; slot < kButtonSlots; ++slot) {
    const Rect r = button_rect(slot);
    const bool has = static_cast<std::size_t>(slot) < hud.train_options.size();
    const bool hovered = has && r.contains(hud.mouse_x, hud.mouse_y);
    draw_bevel(r, !has, has ? (hovered ? al_map_rgb(100, 94, 86) : al_map_rgb(64, 60, 56)) : kInset);
    if (!has) continue;
    const TrainOption& o = hud.train_options[static_cast<std::size_t>(slot)];
    if (o.sprite && o.anim) draw_sprite_fit(*o.sprite, *o.anim, r.x + 3, r.y + 3, r.w - 6, r.h - 6);
    if (!o.affordable) al_draw_filled_rectangle(r.x + 1, r.y + 1, r.x + r.w - 1, r.y + r.h - 1, al_map_rgba_f(0, 0, 0, 0.55f));
    if (hovered) {
      draw_text(font, kPanelW / 2.0f, 552, ALLEGRO_ALIGN_CENTER, "Train " + o.def->name,
                o.affordable ? kText : al_map_rgb(230, 80, 60));
      draw_text(font, kPanelW / 2.0f, 566, ALLEGRO_ALIGN_CENTER,
                std::to_string(o.def->cost.gold) + "g " + std::to_string(static_cast<int>(o.def->train_seconds)) + "s");
    }
  }
}

void draw_unit_info(ALLEGRO_FONT* font, const HudData& hud) {
  const Rect& p = kInfoPanel;
  draw_bevel(p, true, kInset);
  if (hud.building) return draw_building_info(font, hud);
  if (hud.selection.empty()) return;

  const auto draw_portrait = [&](const UnitCard& u, const Rect& box) {
    if (u.sprite && u.anim) draw_sprite_fit(*u.sprite, *u.anim, box.x + 2, box.y + 2, box.w - 4, box.h - 4);
  };

  if (hud.selection.size() == 1) {
    const UnitCard& u = hud.selection[0];
    const Rect portrait{p.x + 6, p.y + 6, 52, 52};
    draw_bevel(portrait, true, al_map_rgb(40, 50, 40));
    draw_portrait(u, portrait);

    draw_text(font, p.x + 100, p.y + 12, ALLEGRO_ALIGN_CENTER, u.name);
    draw_text(font, p.x + 100, p.y + 28, ALLEGRO_ALIGN_CENTER, "Level " + std::to_string(u.level));

    // HP bar + numbers under the portrait
    const float frac = static_cast<float>(u.hp) / static_cast<float>(u.max_hp);
    al_draw_filled_rectangle(portrait.x, portrait.y + 58, portrait.x + portrait.w, portrait.y + 64, kInset);
    al_draw_filled_rectangle(portrait.x, portrait.y + 58, portrait.x + portrait.w * frac, portrait.y + 64,
                             frac > 0.5f ? kGreen : (frac > 0.25f ? kGold : al_map_rgb(220, 50, 40)));
    draw_text(font, portrait.x + portrait.w / 2, portrait.y + 67, ALLEGRO_ALIGN_CENTER,
              std::to_string(u.hp) + "/" + std::to_string(u.max_hp));

    const auto& s = u.stats;
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
  draw_text(font, p.x + p.w / 2, p.y + 8, ALLEGRO_ALIGN_CENTER, std::to_string(hud.selection.size()) + " units");
  constexpr int kCols = 3, kCell = 46, kMaxShown = 15;
  const float left = p.x + (p.w - kCols * kCell) / 2;
  int shown = 0;
  for (const UnitCard& u : hud.selection) {
    if (shown >= kMaxShown) break;
    const Rect cell{left + static_cast<float>(shown % kCols * kCell), p.y + 24 + static_cast<float>(shown / kCols * kCell),
                    kCell - 2, kCell - 2};
    draw_bevel(cell, true, al_map_rgb(40, 50, 40));
    draw_portrait(u, {cell.x, cell.y, cell.w, cell.h - 8});
    const float frac = static_cast<float>(u.hp) / static_cast<float>(u.max_hp);
    al_draw_filled_rectangle(cell.x + 2, cell.y + cell.h - 6, cell.x + cell.w - 2, cell.y + cell.h - 3, kInset);
    al_draw_filled_rectangle(cell.x + 2, cell.y + cell.h - 6, cell.x + 2 + (cell.w - 4) * frac, cell.y + cell.h - 3, kGreen);
    ++shown;
  }
  if (hud.selection.size() > kMaxShown) {
    draw_text(font, p.x + p.w - 6, p.y + p.h - 12, ALLEGRO_ALIGN_RIGHT,
              "+" + std::to_string(hud.selection.size() - kMaxShown));
  }
}

}  // namespace

Rect button_rect(int slot) {
  constexpr float kSize = 44, kGap = 4;
  const float left = (kPanelW - (kButtonCols * kSize + (kButtonCols - 1) * kGap)) / 2;
  return {left + static_cast<float>(slot % kButtonCols) * (kSize + kGap),
          440 + static_cast<float>(slot / kButtonCols) * (kSize + kGap), kSize, kSize};
}

std::optional<int> queue_slot_at(float x, float y) {
  for (int i = 0; i < kQueueCells; ++i)
    if (queue_cell_rect(i).contains(x, y)) return i;
  return std::nullopt;
}

std::optional<int> button_at(float x, float y) {
  for (int i = 0; i < kButtonSlots; ++i) {
    if (button_rect(i).contains(x, y)) return i;
  }
  return std::nullopt;
}

void draw_hud(ALLEGRO_FONT* font, const HudData& hud) {
  // Left panel
  draw_bevel({0, 0, fx(kPanelW), fx(kHeight)}, false, kStone);
  draw_bevel(kMenuButton, false, al_map_rgb(40, 50, 110));
  draw_text(font, kMenuButton.x + kMenuButton.w / 2, kMenuButton.y + 5, ALLEGRO_ALIGN_CENTER, "Menu (F10)");

  draw_minimap(hud);
  draw_unit_info(font, hud);

  // Command buttons (only when something is selected)
  if (hud.building) {
    if (hud.building->complete) draw_train_menu(font, hud);
    for (int slot = 0; !hud.building->complete && slot < kButtonSlots; ++slot) draw_bevel(button_rect(slot), true, kInset);
  } else if (hud.build_menu) {
    draw_build_menu(font, hud);
  } else {
    for (int slot = 0; slot < kButtonSlots; ++slot) {
      const Rect r = button_rect(slot);
      const bool has = static_cast<std::size_t>(slot) < hud.commands.size();
      const bool hovered = has && r.contains(hud.mouse_x, hud.mouse_y);
      draw_bevel(r, !has, has ? (hovered ? al_map_rgb(100, 94, 86) : al_map_rgb(64, 60, 56)) : kInset);
      if (!has) continue;
      const CommandButton& b = hud.commands[static_cast<std::size_t>(slot)];
      draw_command_glyph(b.def->icon, r, b.def->implemented ? 1.0f : 0.35f);
      if (b.active) al_draw_rectangle(r.x + 2, r.y + 2, r.x + r.w - 2, r.y + r.h - 2, kGreen, 2);
      if (hovered) draw_text(font, kPanelW / 2.0f, 552, ALLEGRO_ALIGN_CENTER, b.def->label);
    }
  }

  // Top bar with resources
  draw_bevel({fx(kPanelW), 0, fx(kWidth - kPanelW), fx(kTopBar)}, false, kStone);
  const float slot_w = fx(kWidth - kPanelW) / 4;
  draw_resource(font, kPanelW + slot_w * 0.5f - 24, 0, std::to_string(hud.resources.gold));
  draw_resource(font, kPanelW + slot_w * 1.5f - 24, 1, std::to_string(hud.resources.lumber));
  draw_resource(font, kPanelW + slot_w * 2.5f - 24, 2, std::to_string(hud.resources.oil));
  const bool full = hud.food_used >= hud.food_cap;  // red when no more units can be trained
  draw_resource(font, kPanelW + slot_w * 3.5f - 24, 3, std::to_string(hud.food_used) + "/" + std::to_string(hud.food_cap),
                full ? al_map_rgb(240, 90, 70) : kText);

  // Status message
  if (!hud.message.empty()) {
    draw_text(font, kViewport.x + 12, kViewport.y + kViewport.h - 20, ALLEGRO_ALIGN_LEFT, hud.message, al_map_rgb(250, 210, 60));
  }
}

}  // namespace ui
