#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>
#include <iostream>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>
#include "sprite.hpp"

using json = nlohmann::json;

void draw_sprite(sprite& sprite, double delta_time) {
  auto& animation = sprite.animations[sprite.current_animation];

  int flags = 0;

  if (animation.flip_horizontal) {
    flags = ALLEGRO_FLIP_HORIZONTAL;
  }

  al_draw_bitmap_region(sprite.spritesheet, 
                        animation.frames[animation.current_frame].x, 
                        animation.frames[animation.current_frame].y,
                        animation.frames[animation.current_frame].width, 
                        animation.frames[animation.current_frame].height, 
                        (int)sprite.x, 
                        (int)sprite.y, 
                        flags);

  animation.elapsed_time += delta_time;

  if (animation.elapsed_time >= animation.delay) {
    animation.elapsed_time = 0.0;
    animation.current_frame++;
    if (animation.current_frame >= animation.n_frames) {
      animation.current_frame = 0;
    }
  }
}

sprite load_sprite_from_json(const std::string& filename) {
  std::ifstream f(filename);
  json data = json::parse(f);

  std::string spritesheet_file = data["spritesheet"];

  std::cout << "loading spritesheet: " << spritesheet_file << std::endl;

  sprite new_sprite;

  new_sprite.x = 0;
  new_sprite.y = 0;
  new_sprite.spritesheet = al_load_bitmap(spritesheet_file.c_str());
  new_sprite.current_animation = data["default_animation"];
  new_sprite.direction = "up";

  for (const auto& animation_data : data["animations"]) {
    std::cout << "\tloading animation " << animation_data["name"] << std::endl;

    s_animation animation;
    int fps = (int)animation_data["fps"];

    animation.delay         = 1.0 / (double)fps;
    animation.n_frames      = animation_data["frames"].size();
    animation.current_frame = 0;
    animation.elapsed_time  = 0.0;
    animation.flip_horizontal = animation_data.contains("flip_horizontal") && animation_data["flip_horizontal"];

    for (const auto& frame_data : animation_data["frames"]) {
      s_frame frame;

      frame.x      = frame_data["x"];
      frame.y      = frame_data["y"];
      frame.width  = frame_data["w"];
      frame.height = frame_data["h"];

      animation.frames.push_back(frame);
    }

    new_sprite.animations[animation_data["name"]] = std::move(animation);
  }

  return new_sprite;
}