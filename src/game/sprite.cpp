#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>
#include "sprite.hpp"

using json = nlohmann::json;

void draw_sprite(sprite& sprite, double delta_time) {
  const auto it = sprite.animations.find(sprite.current_animation);
  if (it == sprite.animations.end() || it->second.frames.empty()) return;  // unknown animation
  auto& animation = it->second;

  const int flags = animation.flip_horizontal ? ALLEGRO_FLIP_HORIZONTAL : 0;
  const s_frame& frame = animation.frames[static_cast<std::size_t>(animation.current_frame)];

  al_draw_bitmap_region(sprite.spritesheet, static_cast<float>(frame.x), static_cast<float>(frame.y),
                        static_cast<float>(frame.width), static_cast<float>(frame.height),
                        static_cast<float>(static_cast<int>(sprite.x)),
                        static_cast<float>(static_cast<int>(sprite.y)), flags);

  animation.elapsed_time += delta_time;

  if (animation.elapsed_time >= animation.delay) {
    animation.elapsed_time = 0.0;
    animation.current_frame++;
    if (animation.current_frame >= animation.n_frames) {
      animation.current_frame = 0;
    }
  }
}

std::expected<sprite, std::string> load_sprite_from_json(const std::string& filename) {
  std::ifstream f(filename);
  if (!f) return std::unexpected("cannot open " + filename);

  try {
    const json data = json::parse(f);

    const auto spritesheet_file = data.at("spritesheet").get<std::string>();

    sprite new_sprite;
    new_sprite.x = 0;
    new_sprite.y = 0;
    new_sprite.spritesheet = al_load_bitmap(spritesheet_file.c_str());
    if (!new_sprite.spritesheet) return std::unexpected("cannot load spritesheet " + spritesheet_file);
    new_sprite.current_animation = data.at("default_animation").get<std::string>();
    new_sprite.direction = "_up";

    for (const auto& animation_data : data.at("animations")) {
      s_animation animation;
      const int fps = animation_data.at("fps").get<int>();
      if (fps <= 0) return std::unexpected("invalid fps in animation " + animation_data.at("name").dump());

      animation.delay = 1.0 / fps;
      animation.current_frame = 0;
      animation.elapsed_time = 0.0;
      animation.flip_horizontal = animation_data.value("flip_horizontal", false);

      for (const auto& frame_data : animation_data.at("frames")) {
        animation.frames.push_back({frame_data.at("x").get<int>(), frame_data.at("y").get<int>(),
                                    frame_data.at("w").get<int>(), frame_data.at("h").get<int>()});
      }
      animation.n_frames = static_cast<int>(animation.frames.size());
      if (animation.frames.empty()) return std::unexpected("animation without frames");

      new_sprite.animations[animation_data.at("name").get<std::string>()] = std::move(animation);
    }

    if (!new_sprite.animations.contains(new_sprite.current_animation)) {
      return std::unexpected("default_animation not found: " + new_sprite.current_animation);
    }
    return new_sprite;
  } catch (const json::exception& e) {
    return std::unexpected(filename + ": " + e.what());
  }
}
