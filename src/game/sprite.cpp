#include "sprite.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {
int flip_flags(const s_animation& a) { return a.flip_horizontal ? ALLEGRO_FLIP_HORIZONTAL : 0; }
}  // namespace

const s_frame* current_frame(const SpriteDef& def, const AnimationState& state) {
  const auto it = def.animations.find(state.current);
  if (it == def.animations.end() || it->second.frames.empty()) return nullptr;
  const auto& frames = it->second.frames;
  return &frames[static_cast<std::size_t>(state.frame) % frames.size()];
}

std::string pick_animation(const SpriteDef& def, std::initializer_list<std::string> candidates) {
  for (const auto& c : candidates)
    if (def.animations.contains(c)) return c;
  return def.default_animation;
}

void set_animation(AnimationState& state, const std::string& name) {
  if (state.current == name) return;
  state.current = name;
  state.frame = 0;
  state.elapsed = 0.0;
}

void update_animation(const SpriteDef& def, AnimationState& state, double delta_time) {
  const auto it = def.animations.find(state.current);
  if (it == def.animations.end() || it->second.frames.empty()) return;
  const auto& anim = it->second;

  state.elapsed += delta_time;
  while (state.elapsed >= anim.delay) {
    state.elapsed -= anim.delay;
    state.frame = (state.frame + 1) % static_cast<int>(anim.frames.size());
  }
}

void draw_sprite_centered(const SpriteDef& def, const AnimationState& state, float cx, float cy) {
  const s_frame* f = current_frame(def, state);
  if (!f) return;
  const auto& anim = def.animations.at(state.current);
  al_draw_bitmap_region(def.spritesheet.get(), static_cast<float>(f->x), static_cast<float>(f->y),
                        static_cast<float>(f->width), static_cast<float>(f->height),
                        std::floor(cx - static_cast<float>(f->width) / 2),
                        std::floor(cy - static_cast<float>(f->height) / 2), flip_flags(anim));
}

void draw_sprite_fit(const SpriteDef& def, const AnimationState& state, float bx, float by, float bw, float bh) {
  const s_frame* f = current_frame(def, state);
  if (!f) return;
  const auto& anim = def.animations.at(state.current);
  const float fw = static_cast<float>(f->width);
  const float fh = static_cast<float>(f->height);
  const float scale = std::min({1.0f, bw / fw, bh / fh});
  al_draw_scaled_bitmap(def.spritesheet.get(), static_cast<float>(f->x), static_cast<float>(f->y), fw, fh,
                        bx + (bw - fw * scale) / 2, by + (bh - fh * scale) / 2, fw * scale, fh * scale,
                        flip_flags(anim));
}

std::expected<std::shared_ptr<SpriteDef>, std::string> load_sprite_def(const std::string& filename) {
  std::ifstream f(filename);
  if (!f) return std::unexpected("cannot open " + filename);

  try {
    const json data = json::parse(f);
    auto def = std::make_shared<SpriteDef>();

    const auto spritesheet_file = data.at("spritesheet").get<std::string>();
    def->spritesheet.reset(al_load_bitmap(spritesheet_file.c_str()));
    if (!def->spritesheet) return std::unexpected("cannot load spritesheet " + spritesheet_file);
    def->default_animation = data.at("default_animation").get<std::string>();

    for (const auto& animation_data : data.at("animations")) {
      s_animation animation;
      const int fps = animation_data.at("fps").get<int>();
      if (fps <= 0) return std::unexpected("invalid fps in animation " + animation_data.at("name").dump());

      animation.delay = 1.0 / fps;
      animation.flip_horizontal = animation_data.value("flip_horizontal", false);

      for (const auto& frame_data : animation_data.at("frames")) {
        animation.frames.push_back({frame_data.at("x").get<int>(), frame_data.at("y").get<int>(),
                                    frame_data.at("w").get<int>(), frame_data.at("h").get<int>()});
      }
      if (animation.frames.empty()) return std::unexpected("animation without frames");

      def->animations[animation_data.at("name").get<std::string>()] = std::move(animation);
    }

    if (!def->animations.contains(def->default_animation)) {
      return std::unexpected("default_animation not found: " + def->default_animation);
    }
    return def;
  } catch (const json::exception& e) {
    return std::unexpected(filename + ": " + e.what());
  }
}
