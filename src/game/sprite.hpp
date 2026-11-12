#pragma once

#include <allegro5/allegro5.h>
#include <allegro5/allegro_image.h>

#include <expected>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "raii.hpp"

/// A rectangle of the spritesheet.
struct s_frame {
  int x;
  int y;
  int width;
  int height;
};

/// Immutable animation description (shared by every unit using the sprite).
struct s_animation {
  std::vector<s_frame> frames;
  double delay = 0.2;
  bool flip_horizontal = false;
  bool loop = true;  // false: stops on the last frame (death animations)
};

/// Spritesheet + animations. Loaded once and shared between units.
struct SpriteDef {
  BitmapPtr spritesheet;
  std::string default_animation;
  std::map<std::string, s_animation> animations;
};

/// Per-unit playback state.
struct AnimationState {
  std::string current;
  int frame = 0;
  double elapsed = 0.0;
};

/// Loads a sprite definition; returns an error message on failure.
std::expected<std::shared_ptr<SpriteDef>, std::string> load_sprite_def(const std::string& filename);

/// The first of `candidates` that this sprite defines, or its default animation. Lets a unit
/// without e.g. "walk_gold_*" frames fall back to plain walking.
std::string pick_animation(const SpriteDef& def, std::initializer_list<std::string> candidates);

/// Switches animation (restarting it only if it actually changed).
void set_animation(AnimationState& state, const std::string& name);
void update_animation(const SpriteDef& def, AnimationState& state, double delta_time);

/// Frame currently shown, or nullptr if the animation is unknown.
const s_frame* current_frame(const SpriteDef& def, const AnimationState& state);

/// Draws the current frame centred at (cx, cy) in the current target's coordinates.
void draw_sprite_centered(const SpriteDef& def, const AnimationState& state, float cx, float cy, float alpha = 1.0f);

/// Draws the current frame scaled down (never up) to fit the box, centred in it.
void draw_sprite_fit(const SpriteDef& def, const AnimationState& state, float bx, float by, float bw, float bh);
