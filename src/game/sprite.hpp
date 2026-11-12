#pragma once

#include <allegro5/allegro5.h>
#include <allegro5/allegro_image.h>
#include <map>
#include <vector>
#include <expected>
#include <string>

// types
struct s_frame {
  int x;
  int y;
  int width;
  int height;
};

struct s_animation {
  std::vector<s_frame> frames;
  int current_frame;
  int n_frames;
  double delay;
  double elapsed_time;
  bool flip_horizontal;
};

struct sprite {
  ALLEGRO_BITMAP* spritesheet;
  double x;
  double y;
  std::string direction;
  std::string current_animation;
  std::map<std::string, s_animation> animations;
};

void draw_sprite(sprite& sprite, double delta_time);
/// Loads a sprite definition; returns an error message on failure.
std::expected<sprite, std::string> load_sprite_from_json(const std::string& filename);
