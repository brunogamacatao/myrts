#include "sprite_doc.hpp"

#include <set>

namespace myrts {

std::string sprite_problem(const nlohmann::json& s) {
  if (!s.is_object()) return "a sprite definition must be an object";
  if (!s.contains("spritesheet") || !s["spritesheet"].is_string() || s["spritesheet"].get<std::string>().empty()) return "no spritesheet";
  if (!s.contains("animations") || !s["animations"].is_array() || s["animations"].empty()) return "no animations";
  std::set<std::string> names;
  for (const auto& a : s["animations"]) {
    if (!a.is_object() || !a.contains("name") || !a["name"].is_string()) return "an animation has no name";
    const std::string name = a["name"].get<std::string>();
    if (!names.insert(name).second) return "two animations are named '" + name + "'";
    if (!a.contains("fps") || !a["fps"].is_number_integer() || a["fps"].get<int>() <= 0) return "animation '" + name + "': fps must be a whole number above 0";
    if (!a.contains("frames") || !a["frames"].is_array() || a["frames"].empty()) return "animation '" + name + "' has no frames";
    for (const auto& f : a["frames"])
      for (const char* key : {"x", "y", "w", "h"}) {
        if (!f.is_object() || !f.contains(key) || !f[key].is_number_integer()) return "animation '" + name + "': a frame needs whole numbers x, y, w and h";
        if ((key[0] == 'w' || key[0] == 'h') && f[key].get<int>() <= 0) return "animation '" + name + "': a frame's w and h must be above 0";
      }
  }
  if (!s.contains("default_animation") || !s["default_animation"].is_string()) return "no default_animation";
  if (!names.contains(s["default_animation"].get<std::string>())) return "default_animation '" + s["default_animation"].get<std::string>() + "' is not an animation";
  if (s.contains("states")) {
    if (!s["states"].is_object()) return "states must be an object";
    for (const auto& [state, list] : s["states"].items())
      if (!list.is_array()) return "state '" + state + "' must be a list of animation names";
  }
  return {};
}

}  // namespace myrts
