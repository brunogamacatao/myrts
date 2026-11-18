#include "textures.hpp"

namespace myrts::editor {

ALLEGRO_BITMAP* Textures::get(const std::string& path) {
  if (const auto it = bitmaps_.find(path); it != bitmaps_.end()) return it->second;
  ALLEGRO_BITMAP* bitmap = al_load_bitmap(path.c_str());
  bitmaps_[path] = bitmap;
  return bitmap;
}

void Textures::drop(const std::string& path) {
  if (const auto it = bitmaps_.find(path); it != bitmaps_.end()) {
    if (it->second) al_destroy_bitmap(it->second);
    bitmaps_.erase(it);
  }
}

void Textures::clear() {
  for (auto& [path, bitmap] : bitmaps_)
    if (bitmap) al_destroy_bitmap(bitmap);
  bitmaps_.clear();
}

}  // namespace myrts::editor
