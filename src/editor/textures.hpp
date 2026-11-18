#pragma once

#include <allegro5/allegro5.h>
#include <imgui.h>

#include <cstdint>
#include <map>
#include <string>

namespace myrts::editor {

/// Pictures loaded for display in the editor, by file path. Must be destroyed while Allegro is still running.
class Textures {
 public:
  Textures() = default;
  Textures(const Textures&) = delete;
  Textures& operator=(const Textures&) = delete;
  ~Textures() { clear(); }

  /// The picture in `path` (nullptr if it cannot be loaded; the failure is remembered until `drop`).
  ALLEGRO_BITMAP* get(const std::string& path);
  void drop(const std::string& path);
  void clear();

  static ImTextureID id(ALLEGRO_BITMAP* bitmap) { return static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(bitmap)); }

 private:
  std::map<std::string, ALLEGRO_BITMAP*> bitmaps_;
};

}  // namespace myrts::editor
