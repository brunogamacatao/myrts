#pragma once

#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>

#include <optional>
#include <span>
#include <string>
#include <vector>

#include "building_art.hpp"
#include "core/building.hpp"
#include "core/geometry.hpp"
#include "unit.hpp"

namespace ui {

inline constexpr int kWidth = 800;
inline constexpr int kHeight = 600;
inline constexpr int kPanelW = 160;
inline constexpr int kTopBar = 24;

inline constexpr myrts::Rect kViewport{kPanelW, kTopBar, kWidth - kPanelW, kHeight - kTopBar};
inline constexpr myrts::Rect kMenuButton{8, 4, 144, 18};
inline constexpr myrts::Rect kMinimap{16, 30, 128, 128};
inline constexpr myrts::Rect kInfoPanel{8, 166, 144, 262};
inline constexpr int kButtonCols = 3;
inline constexpr int kButtonRows = 2;
inline constexpr int kButtonSlots = kButtonCols * kButtonRows;

using Resources = myrts::Stock;

struct HudData {
  const Resources& resources;
  const std::vector<Unit>& units;
  std::span<const int> selected;  // unit ids
  ALLEGRO_BITMAP* minimap_bitmap;  // whole map, drawn scaled
  const myrts::Minimap& minimap;
  const myrts::Camera& camera;
  const std::string& message;  // shown at the bottom of the viewport
  bool awaiting_target;
  float mouse_x;
  float mouse_y;
  bool build_menu = false;  // command panel shows the buildings a peon can construct
  const BuildingArt* art = nullptr;
  std::optional<myrts::BuildingKind> placing;  // building being positioned
};

/// Rectangle of a command button slot (0..kButtonSlots-1), in screen coordinates.
myrts::Rect button_rect(int slot);
/// Slot under the point, if any.
std::optional<int> button_at(float x, float y);
/// Building offered in a build-menu slot (slot kBuildMenuCancel closes the menu).
std::optional<myrts::BuildingKind> build_menu_slot(int slot);
inline constexpr int kBuildMenuCancel = 5;

/// Commands to offer for the current selection (those of the first selected unit's type).
std::vector<Command> panel_commands(const std::vector<Unit>& units, std::span<const int> selected);

void draw_hud(ALLEGRO_FONT* font, const HudData& hud);

}  // namespace ui
