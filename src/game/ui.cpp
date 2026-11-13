#include <vector>
#include <string>
#include <sstream>
#include "ui.hpp"

#include <allegro5/allegro_primitives.h>

#include <algorithm>
#include <cmath>

#include "draw_util.hpp"

namespace ui {
namespace {

using myrts::Rect;

const ALLEGRO_COLOR kStoneLight = al_map_rgb(150, 126, 90);
const ALLEGRO_COLOR kStoneDark = al_map_rgb(24, 18, 12);
const ALLEGRO_COLOR kInset = al_map_rgb(20, 14, 8);
const ALLEGRO_COLOR kText = al_map_rgb(240, 228, 190);
const ALLEGRO_COLOR kGold = al_map_rgb(240, 200, 40);
const ALLEGRO_COLOR kGreen = al_map_rgb(60, 220, 60);
const ALLEGRO_COLOR kBorder = al_map_rgb(176, 132, 40);  // the gold trim of the original panels

constexpr float kButtonW = UiArt::kIconW;
constexpr float kButtonH = UiArt::kIconH;
constexpr float kButtonGapX = 8;
constexpr float kButtonGapY = 6;
constexpr float kButtonsTop = 440;

float fx(int v) { return static_cast<float>(v); }

void draw_text(ALLEGRO_FONT* font, float x, float y, int align, const std::string& text,
               ALLEGRO_COLOR color = kText) {
  al_draw_text(font, kStoneDark, x + 1, y + 1, align, text.c_str());  // drop shadow
  al_draw_text(font, color, x, y, align, text.c_str());
}

/// Like draw_text, but shrinks long text so it fits in `max_width` pixels.
void draw_text_fit(ALLEGRO_FONT* font, float x, float y, float max_width, int align, const std::string& text,
                   ALLEGRO_COLOR color = kText) {
  const float scale = fit_scale(font, text, 1.0f, max_width);
  if (scale >= 1.0f) return draw_text(font, x, y, align, text, color);
  draw_text_scaled(font, x, y + (1.0f - scale) * 4.0f, scale, align, text, color);
}

/// A thin raised (or sunken) frame around a rectangle.
void draw_frame(const Rect& r, bool raised) {
  const ALLEGRO_COLOR hi = raised ? kStoneLight : kStoneDark;
  const ALLEGRO_COLOR lo = raised ? kStoneDark : kStoneLight;
  al_draw_line(r.x, r.y + 0.5f, r.x + r.w, r.y + 0.5f, hi, 1);
  al_draw_line(r.x + 0.5f, r.y, r.x + 0.5f, r.y + r.h, hi, 1);
  al_draw_line(r.x, r.y + r.h - 0.5f, r.x + r.w, r.y + r.h - 0.5f, lo, 1);
  al_draw_line(r.x + r.w - 0.5f, r.y, r.x + r.w - 0.5f, r.y + r.h, lo, 1);
}

/// A button showing an icon from the atlas; `dim` greys it out (unavailable), `active` outlines it in green.
void draw_icon_button(const HudData& hud, const Rect& r, int icon, bool hovered, bool dim, bool active) {
  hud.art.draw_icon(icon, r.x, r.y, 1.0f, dim ? 0.4f : 1.0f);
  draw_frame(r, !hovered);
  if (hovered) al_draw_rectangle(r.x + 0.5f, r.y + 0.5f, r.x + r.w - 0.5f, r.y + r.h - 0.5f, kGold, 1);
  if (active) al_draw_rectangle(r.x + 2, r.y + 2, r.x + r.w - 2, r.y + r.h - 2, kGreen, 2);
}

void draw_empty_slot(const Rect& r) {
  al_draw_filled_rectangle(r.x, r.y, r.x + r.w, r.y + r.h, al_map_rgba_f(0, 0, 0, 0.45f));
  draw_frame(r, false);
}

/// A small button with a glyph drawn by hand, for the two page/cancel controls that have no original icon.
void draw_control_button(const Rect& r, bool hovered, bool next) {
  al_draw_filled_rectangle(r.x, r.y, r.x + r.w, r.y + r.h, hovered ? al_map_rgb(96, 70, 30) : al_map_rgb(62, 42, 18));
  draw_frame(r, !hovered);
  const float cx = r.x + r.w / 2, cy = r.y + r.h / 2;
  if (next) {
    al_draw_filled_triangle(cx - 8, cy - 12, cx - 8, cy + 12, cx + 12, cy, kText);
  } else {
    al_draw_line(cx - 11, cy - 11, cx + 11, cy + 11, al_map_rgb(235, 70, 55), 4);
    al_draw_line(cx - 11, cy + 11, cx + 11, cy - 11, al_map_rgb(235, 70, 55), 4);
  }
}

void draw_resource(const HudData& hud, ALLEGRO_FONT* font, float x, ALLEGRO_BITMAP* icon, const std::string& text,
                   ALLEGRO_COLOR color = kText) {
  al_draw_bitmap(icon, x, 5, 0);
  draw_text(font, x + 20, 8, ALLEGRO_ALIGN_LEFT, text, color);
  (void)hud;
}

void draw_minimap(const HudData& hud) {
  al_draw_bitmap(hud.art.minimap_frame(), fx(0), kMinimapFrame.y, 0);

  const Rect& area = kMinimap;
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

/// Hit points bar with the numbers under it.
void draw_hp(ALLEGRO_FONT* font, float x, float y, float w, int hp, int max_hp, bool numbers) {
  const float frac = std::clamp(static_cast<float>(hp) / static_cast<float>(max_hp), 0.0f, 1.0f);
  al_draw_filled_rectangle(x, y, x + w, y + 6, kInset);
  al_draw_filled_rectangle(x, y, x + w * frac, y + 6, frac > 0.5f ? kGreen : (frac > 0.25f ? kGold : al_map_rgb(220, 50, 40)));
  if (numbers) draw_text(font, x + w / 2, y + 9, ALLEGRO_ALIGN_CENTER, std::to_string(hp) + "/" + std::to_string(max_hp));
}

constexpr int kQueueCells = 5;
constexpr float kQueueCell = 28;

Rect queue_cell_rect(int i) {
  const Rect& p = kInfoPanel;
  return {p.x + 6 + static_cast<float>(i) * (kQueueCell + 3), p.y + 158, kQueueCell, kQueueCell};
}

/// Dark glass over the panel texture so text stays readable, with the gold trim.
void draw_info_background() {
  const Rect& p = kInfoPanel;
  al_draw_filled_rectangle(p.x, p.y, p.x + p.w, p.y + p.h, al_map_rgba_f(0, 0, 0, 0.55f));
  al_draw_rectangle(p.x + 0.5f, p.y + 0.5f, p.x + p.w - 0.5f, p.y + p.h - 0.5f, kBorder, 1);
}

void draw_building_info(ALLEGRO_FONT* font, const HudData& hud) {
  const Rect& p = kInfoPanel;
  const BuildingCard& b = *hud.building;
  hud.art.draw_icon(b.icon, p.x + 6, p.y + 6);
  draw_frame({p.x + 6, p.y + 6, kButtonW, kButtonH}, false);
  draw_text_fit(font, p.x + 58, p.y + 12, p.w - 64, ALLEGRO_ALIGN_LEFT, b.name);
  draw_hp(font, p.x + 6, p.y + 50, kButtonW, b.hp, b.max_hp, false);
  draw_text(font, p.x + 58, p.y + 30, ALLEGRO_ALIGN_LEFT, std::to_string(b.hp) + "/" + std::to_string(b.max_hp));

  if (!b.complete) {
    draw_text(font, p.x + p.w / 2, p.y + 80, ALLEGRO_ALIGN_CENTER, "Under construction");
    al_draw_filled_rectangle(p.x + 10, p.y + 96, p.x + p.w - 10, p.y + 104, kInset);
    al_draw_filled_rectangle(p.x + 10, p.y + 96, p.x + 10 + (p.w - 20) * b.build_progress, p.y + 104, kGold);
    return;
  }
  if (hud.train_options.empty()) return;

  draw_text(font, p.x + 8, p.y + 140, ALLEGRO_ALIGN_LEFT, "Training");
  for (int i = 0; i < kQueueCells; ++i) {
    const Rect cell = queue_cell_rect(i);
    al_draw_filled_rectangle(cell.x, cell.y, cell.x + cell.w, cell.y + cell.h, kInset);
    draw_frame(cell, false);
    if (static_cast<std::size_t>(i) >= b.queue.size()) continue;
    const QueueEntry& q = b.queue[static_cast<std::size_t>(i)];
    hud.art.draw_icon(q.icon, cell.x + 1, cell.y + 3, 0.58f);
    if (i == 0)  // progress of the unit being trained
      al_draw_filled_rectangle(cell.x + 1, cell.y + cell.h - 4, cell.x + 1 + (cell.w - 2) * q.progress, cell.y + cell.h - 1, kGreen);
  }
  if (!b.queue.empty()) draw_text(font, p.x + 8, p.y + 190, ALLEGRO_ALIGN_LEFT, "click to cancel");
}

/// Command panel while a building is selected: one button per trainable unit.
void draw_train_menu(ALLEGRO_FONT* font, const HudData& hud) {
  for (int slot = 0; slot < kButtonSlots; ++slot) {
    const Rect r = button_rect(slot);
    if (static_cast<std::size_t>(slot) >= hud.train_options.size()) {
      draw_empty_slot(r);
      continue;
    }
    const TrainOption& o = hud.train_options[static_cast<std::size_t>(slot)];
    const bool hovered = r.contains(hud.mouse_x, hud.mouse_y);
    draw_icon_button(hud, r, o.def->icon, hovered, !o.affordable, false);
    if (hovered) {
      draw_text_fit(font, kPanelW / 2.0f, 552, kPanelW - 8, ALLEGRO_ALIGN_CENTER, "Train " + o.def->name,
                    o.affordable ? kText : al_map_rgb(230, 80, 60));
      draw_text(font, kPanelW / 2.0f, 566, ALLEGRO_ALIGN_CENTER,
                std::to_string(o.def->cost.gold) + "g " + std::to_string(static_cast<int>(o.def->train_seconds)) + "s");
    }
  }
}

/// Command panel while the build menu is open: one button per building plus page and cancel buttons.
void draw_build_menu(ALLEGRO_FONT* font, const HudData& hud) {
  for (int slot = 0; slot < kButtonSlots; ++slot) {
    const Rect r = button_rect(slot);
    const bool is_cancel = slot == kBuildMenuCancel;
    const bool is_next = hud.build_has_more && slot == kBuildNextPage;
    const BuildOption* opt = static_cast<std::size_t>(slot) < hud.build_options.size() && !is_cancel && !is_next
                                 ? &hud.build_options[static_cast<std::size_t>(slot)]
                                 : nullptr;
    if (!opt && !is_cancel && !is_next) {
      draw_empty_slot(r);
      continue;
    }
    const bool hovered = r.contains(hud.mouse_x, hud.mouse_y);
    if (is_next || is_cancel) {
      draw_control_button(r, hovered, is_next);
      if (hovered) draw_text(font, kPanelW / 2.0f, 552, ALLEGRO_ALIGN_CENTER, is_next ? "More buildings" : "Cancel");
      continue;
    }
    const myrts::BuildingDef& def = *opt->def;
    draw_icon_button(hud, r, def.icon, hovered, !opt->affordable, opt->placing);
    if (hovered) {
      draw_text_fit(font, kPanelW / 2.0f, 552, kPanelW - 8, ALLEGRO_ALIGN_CENTER, def.name,
                    opt->affordable ? kText : al_map_rgb(230, 80, 60));
      draw_text(font, kPanelW / 2.0f, 566, ALLEGRO_ALIGN_CENTER,
                std::to_string(def.cost.gold) + "g " + std::to_string(def.cost.lumber) + "w " +
                    std::to_string(static_cast<int>(def.build_seconds)) + "s");
    }
  }
}

void draw_unit_info(ALLEGRO_FONT* font, const HudData& hud) {
  const Rect& p = kInfoPanel;
  draw_info_background();
  if (hud.building) return draw_building_info(font, hud);
  if (hud.selection.empty()) return;

  if (hud.selection.size() == 1) {
    const UnitCard& u = hud.selection[0];
    hud.art.draw_icon(u.icon, p.x + 6, p.y + 6);
    draw_frame({p.x + 6, p.y + 6, kButtonW, kButtonH}, false);
    draw_text_fit(font, p.x + 58, p.y + 12, p.w - 64, ALLEGRO_ALIGN_LEFT, u.name);
    draw_text(font, p.x + 58, p.y + 30, ALLEGRO_ALIGN_LEFT, "Level " + std::to_string(u.level));
    draw_hp(font, p.x + 6, p.y + 50, kButtonW, u.hp, u.max_hp, true);

    const auto& s = u.stats;
    const std::string lines[] = {"Armor: " + std::to_string(s.armor),
                                 "Damage: " + std::to_string(s.damage_min) + "-" + std::to_string(s.damage_max),
                                 "Range: " + std::to_string(s.range), "Sight: " + std::to_string(s.sight),
                                 "Speed: " + std::to_string(s.speed)};
    float y = p.y + 96;
    for (const auto& line : lines) {
      draw_text(font, p.x + 26, y, ALLEGRO_ALIGN_LEFT, line);
      y += 16;
    }
    return;
  }

  // Several units: a grid of their icons with a health bar each.
  draw_text(font, p.x + p.w / 2, p.y + 6, ALLEGRO_ALIGN_CENTER, std::to_string(hud.selection.size()) + " units");
  constexpr int kCols = 3, kMaxShown = 15;
  constexpr float kCellW = 50, kCellH = 46;
  const float left = p.x + (p.w - kCols * kCellW) / 2 + 2;
  int shown = 0;
  for (const UnitCard& u : hud.selection) {
    if (shown >= kMaxShown) break;
    const float x = left + static_cast<float>(shown % kCols) * kCellW;
    const float y = p.y + 24 + static_cast<float>(shown / kCols) * kCellH;
    hud.art.draw_icon(u.icon, x, y);
    draw_frame({x, y, kButtonW, kButtonH}, false);
    draw_hp(font, x, y + 40, kButtonW, u.hp, u.max_hp, false);
    ++shown;
  }
  if (hud.selection.size() > kMaxShown)
    draw_text(font, p.x + p.w - 6, p.y + p.h - 14, ALLEGRO_ALIGN_RIGHT, "+" + std::to_string(hud.selection.size() - kMaxShown));
}

}  // namespace

Rect button_rect(int slot) {
  const float left = (kPanelW - (kButtonCols * kButtonW + (kButtonCols - 1) * kButtonGapX)) / 2;
  return {left + static_cast<float>(slot % kButtonCols) * (kButtonW + kButtonGapX),
          kButtonsTop + static_cast<float>(slot / kButtonCols) * (kButtonH + kButtonGapY), kButtonW, kButtonH};
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
  // Left panel: the original cracked-earth texture.
  UiArt::draw_tiled(hud.art.panel(), {0, 0, fx(kPanelW), fx(kHeight)});
  al_draw_line(kPanelW - 0.5f, 0, kPanelW - 0.5f, fx(kHeight), kStoneDark, 1);
  al_draw_line(kPanelW - 1.5f, 0, kPanelW - 1.5f, fx(kHeight), kBorder, 1);

  const Rect& menu = kMenuButton;
  const bool menu_hover = menu.contains(hud.mouse_x, hud.mouse_y);
  al_draw_filled_rectangle(menu.x, menu.y, menu.x + menu.w, menu.y + menu.h, menu_hover ? al_map_rgb(86, 100, 170) : al_map_rgb(46, 58, 120));
  draw_frame(menu, !menu_hover);
  draw_text(font, menu.x + menu.w / 2, menu.y + 5, ALLEGRO_ALIGN_CENTER, "Menu (F10)");

  draw_minimap(hud);
  draw_unit_info(font, hud);

  // Command buttons (only when something is selected)
  if (hud.building) {
    if (hud.building->complete) {
      draw_train_menu(font, hud);
    } else {
      for (int slot = 0; slot < kButtonSlots; ++slot) draw_empty_slot(button_rect(slot));
    }
  } else if (hud.build_menu) {
    draw_build_menu(font, hud);
  } else {
    for (int slot = 0; slot < kButtonSlots; ++slot) {
      const Rect r = button_rect(slot);
      if (static_cast<std::size_t>(slot) >= hud.commands.size()) {
        draw_empty_slot(r);
        continue;
      }
      const CommandButton& b = hud.commands[static_cast<std::size_t>(slot)];
      const bool hovered = r.contains(hud.mouse_x, hud.mouse_y);
      draw_icon_button(hud, r, b.icon, hovered, !b.def->implemented, b.active);
      if (hovered) draw_text(font, kPanelW / 2.0f, 552, ALLEGRO_ALIGN_CENTER, b.def->label);
    }
  }

  // Top bar with resources
  UiArt::draw_tiled(hud.art.top_bar(), {fx(kPanelW), 0, fx(kWidth - kPanelW), fx(kTopBar)});
  al_draw_line(kPanelW, kTopBar - 0.5f, fx(kWidth), kTopBar - 0.5f, kStoneDark, 1);
  const float slot_w = fx(kWidth - kPanelW) / 4;
  draw_resource(hud, font, kPanelW + slot_w * 0.5f - 24, hud.art.gold(), std::to_string(hud.resources.gold));
  draw_resource(hud, font, kPanelW + slot_w * 1.5f - 24, hud.art.lumber(), std::to_string(hud.resources.lumber));
  draw_resource(hud, font, kPanelW + slot_w * 2.5f - 24, hud.art.oil(), std::to_string(hud.resources.oil));
  const bool full = hud.food_used >= hud.food_cap;  // red when no more units can be trained
  draw_resource(hud, font, kPanelW + slot_w * 3.5f - 24, hud.art.food(),
                std::to_string(hud.food_used) + "/" + std::to_string(hud.food_cap), full ? al_map_rgb(240, 90, 70) : kText);

  // Status message
  if (!hud.message.empty()) {
    // Long messages (the tutorial hints) wrap onto several lines, growing upwards from the bottom of the viewport.
    const float max_w = kViewport.w - 24;
    std::vector<std::string> lines;
    std::string line;
    std::istringstream words(hud.message);
    for (std::string word; words >> word;) {
      const std::string trial = line.empty() ? word : line + " " + word;
      if (!line.empty() && static_cast<float>(al_get_text_width(font, trial.c_str())) > max_w) {
        lines.push_back(line);
        line = word;
      } else {
        line = trial;
      }
    }
    if (!line.empty()) lines.push_back(line);
    const float line_h = 12;
    const float top = kViewport.y + kViewport.h - 8 - line_h * static_cast<float>(lines.size());
    al_draw_filled_rectangle(kViewport.x, top - 4, kViewport.x + kViewport.w, kViewport.y + kViewport.h, al_map_rgba_f(0, 0, 0, 0.55f));
    for (std::size_t i = 0; i < lines.size(); ++i)
      draw_text(font, kViewport.x + 12, top + line_h * static_cast<float>(i), ALLEGRO_ALIGN_LEFT, lines[i], al_map_rgb(250, 210, 60));
  }
}

}  // namespace ui
