#pragma once

#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>

#include <optional>
#include <span>
#include <string>
#include <vector>

#include "core/building.hpp"
#include "core/defs.hpp"
#include "core/geometry.hpp"
#include "sprite.hpp"
#include "ui_art.hpp"

namespace ui {

inline constexpr int kWidth = 800;
inline constexpr int kHeight = 600;
inline constexpr int kPanelW = 176;  // the width of the original game's side panel
inline constexpr int kTopBar = 24;

inline constexpr myrts::Rect kViewport{kPanelW, kTopBar, kWidth - kPanelW, kHeight - kTopBar};
inline constexpr myrts::Rect kMenuButton{8, 4, 160, 18};
inline constexpr myrts::Rect kMinimapFrame{0, 26, 176, 136};  // the frame texture...
inline constexpr myrts::Rect kMinimap{24, 28, 128, 128};      // ...and the 128x128 hole in it
inline constexpr myrts::Rect kInfoPanel{8, 168, 160, 262};
inline constexpr int kButtonCols = 3;
inline constexpr int kButtonRows = 2;
inline constexpr int kButtonSlots = kButtonCols * kButtonRows;
inline constexpr int kBuildMenuCancel = kButtonSlots - 1;  // last slot closes the build menu
inline constexpr int kBuildNextPage = kButtonSlots - 2;    // shown instead of a building when there are more than fit

// The HUD only sees small view-models filled in by the game every frame; it knows nothing about
// the simulation. Icons are indices into the icon atlas (see UiArt).

/// A selected unit, as shown in the info panel.
struct UnitCard {
  std::string name;
  int level = 1;
  int hp = 1;
  int max_hp = 1;
  myrts::UnitStats stats;
  int icon = -1;
  int capacity = 0;                // a transport: how many it carries
  std::vector<int> cargo_icons;    // and the icons of who is aboard
};

struct MinimapDot {
  float x = 0;  // world pixels
  float y = 0;
  bool selected = false;
  bool enemy = false;
  float size = 2;  // pixels on the minimap (buildings are bigger)
};

struct CommandButton {
  const myrts::CommandDef* def = nullptr;
  int icon = -1;
  bool active = false;  // waiting for its target click
};

struct BuildOption {
  const myrts::BuildingDef* def = nullptr;
  bool affordable = true;
  bool placing = false;
};

/// A unit waiting in a building's training queue.
struct QueueEntry {
  std::string name;
  float progress = 0;  // 0..1, only meaningful for the front entry
  int icon = -1;
};

/// A selected building, as shown in the info panel.
struct BuildingCard {
  std::string name;
  int hp = 1;
  int max_hp = 1;
  bool complete = true;
  float build_progress = 1;  // 0..1 while under construction
  int icon = -1;
  std::vector<QueueEntry> queue;
};

/// A unit a selected building can train (one button each).
struct TrainOption {
  const myrts::UnitDef* def = nullptr;
  bool affordable = true;
};

struct HudData {
  const UiArt& art;
  const myrts::Stock& resources;
  std::span<const UnitCard> selection;
  std::span<const MinimapDot> dots;
  ALLEGRO_BITMAP* minimap_bitmap;  // whole map, drawn scaled
  const myrts::Minimap& minimap;
  const myrts::Camera& camera;
  const std::string& message;  // shown at the bottom of the viewport
  float mouse_x;
  float mouse_y;
  std::span<const CommandButton> commands;     // panel buttons, in slot order
  bool build_menu = false;                      // panel shows `build_options` instead of `commands`
  std::span<const BuildOption> build_options;   // build-menu buttons of the current page, in slot order
  bool build_has_more = false;                  // more pages exist: slot kBuildNextPage turns the page
  const BuildingCard* building = nullptr;       // a building is selected (instead of units)
  std::span<const TrainOption> train_options;   // its training buttons, in slot order
  int food_used = 0;
  int food_cap = 0;
  ALLEGRO_BITMAP* minimap_fog = nullptr;       // the black of the unexplored map, one pixel per tile (drawn over the minimap)
};

/// Rectangle of a command button slot (0..kButtonSlots-1), in screen coordinates.
myrts::Rect button_rect(int slot);
/// Slot under the point, if any.
std::optional<int> button_at(float x, float y);
/// Training-queue cell (0..4) under the point in the building info panel, if any.
std::optional<int> queue_slot_at(float x, float y);

/// Draws the side panel, top bar, minimap and message line.
void draw_hud(ALLEGRO_FONT* font, const HudData& hud);

}  // namespace ui
