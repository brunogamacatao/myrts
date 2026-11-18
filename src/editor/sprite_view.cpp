#include "sprite_view.hpp"

#include <imgui.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>

namespace myrts::editor {

namespace {
using Json = Project::Json;

/// A definition that is not open in the project is read from disk once.
const Json* definition(const Project& project, const std::string& relative) {
  if (const Json* open = project.document(relative)) return open;
  static std::map<std::string, Json> disk;
  const std::string key = project.root().generic_string() + "/" + relative;
  auto it = disk.find(key);
  if (it == disk.end()) {
    std::ifstream in(project.package().path(relative));
    Json doc = in ? Json::parse(in, nullptr, false) : Json(Json::value_t::discarded);
    it = disk.emplace(key, doc.is_discarded() ? Json() : std::move(doc)).first;
  }
  return it->second.is_null() ? nullptr : &it->second;
}
}  // namespace

bool draw_preview(const Project& project, Textures& textures, const std::string& relative, float box) {
  ImVec2 origin = ImGui::GetCursorScreenPos();
  ImGui::Dummy({box, box});
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(origin, {origin.x + box, origin.y + box}, IM_COL32(24, 24, 28, 255));
  dl->AddRect(origin, {origin.x + box, origin.y + box}, IM_COL32(70, 70, 80, 255));
  if (relative.empty()) return false;

  std::string image = relative;
  float sx = 0, sy = 0, sw = 0, sh = 0;
  bool whole = true;
  if (relative.ends_with(".json")) {
    const Json* def = definition(project, relative);
    if (!def || !def->is_object() || !def->contains("spritesheet") || !def->contains("animations")) return false;
    image = (*def)["spritesheet"].get<std::string>();
    const std::string wanted = def->value("default_animation", std::string{});
    for (const Json& a : (*def)["animations"])
      if (a.value("name", std::string{}) == wanted && a.contains("frames") && !a["frames"].empty()) {
        const Json& f = a["frames"][0];
        sx = f.value("x", 0), sy = f.value("y", 0), sw = f.value("w", 0), sh = f.value("h", 0);
        whole = false;
      }
    if (whole) return false;
  }
  ALLEGRO_BITMAP* bitmap = textures.get(project.package().path(image));
  if (!bitmap) return false;
  if (whole) {
    sw = static_cast<float>(al_get_bitmap_width(bitmap));
    sh = static_cast<float>(al_get_bitmap_height(bitmap));
  }
  if (sw <= 0 || sh <= 0) return false;
  const float bw = static_cast<float>(al_get_bitmap_width(bitmap)), bh = static_cast<float>(al_get_bitmap_height(bitmap));
  const float scale = std::min(box / sw, box / sh);
  const float w = sw * scale, h = sh * scale;
  const ImVec2 a{origin.x + (box - w) / 2, origin.y + (box - h) / 2};
  dl->AddImage(Textures::id(bitmap), a, {a.x + w, a.y + h}, {sx / bw, sy / bh}, {(sx + sw) / bw, (sy + sh) / bh});
  return true;
}

}  // namespace myrts::editor
