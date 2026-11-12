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
inline constexpr int kBuildMenuCancel = kButtonSlots - 1;  // last slot closes the build menu

// The HUD only sees small view-models filled in by the game every frame; it knows nothing about
// the simulation.

/// A selected unit, as shown in the info panel.
struct UnitCard {
  std::string name;
  int level = 1;
  int hp = 1;
  int max_hp = 1;
  myrts::UnitStats stats;
  const SpriteDef* sprite = nullptr;
  const AnimationState* anim = nullptr;
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
  bool active = false;  // waiting for its target click
};

struct BuildOption {
  const myrts::BuildingDef* def = nullptr;
  ALLEGRO_BITMAP* icon = nullptr;
  bool affordable = true;
  bool placing = false;
};

/// A unit waiting in a building's training queue.
struct QueueEntry {
  std::string name;
  float progress = 0;  // 0..1, only meaningful for the front entry
  const SpriteDef* sprite = nullptr;
  const AnimationState* anim = nullptr;
};

/// A selected building, as shown in the info panel.
struct BuildingCard {
  std::string name;
  int hp = 1;
  int max_hp = 1;
  bool complete = true;
  float build_progress = 1;  // 0..1 while under construction
  std::vector<QueueEntry> queue;
};

/// A unit a selected building can train (one button each).
struct TrainOption {
  const myrts::UnitDef* def = nullptr;
  const SpriteDef* sprite = nullptr;
  const AnimationState* anim = nullptr;
  bool affordable = true;
};

struct HudData {
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
  std::span<const BuildOption> build_options;   // build-menu buttons, in slot order
  const BuildingCard* building = nullptr;       // a building is selected (instead of units)
  std::span<const TrainOption> train_options;   // its training buttons, in slot order
  int food_used = 0;
  int food_cap = 0;
};

/// Rectangle of a command button slot (0..kButtonSlots-1), in screen coordinates.
myrts::Rect button_rect(int slot);
/// Slot under the point, if any.
std::optional<int> button_at(float x, float y);
/// Training-queue cell (0..4) under the point in the building info panel, if any.
std::optional<int> queue_slot_at(float x, float y);

void draw_hud(ALLEGRO_FONT* font, const HudData& hud);

}  // namespace ui
