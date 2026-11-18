#pragma once

#include <string>

#include "editor_model/project.hpp"
#include "textures.hpp"

namespace myrts::editor {

/// The Sprites tab: the sprite definitions of the game, their spritesheet with the frame rectangles, the animations and
/// a live preview. Dragging on the sheet adds a frame to the selected animation.
class SpriteEditor {
 public:
  explicit SpriteEditor(Textures& textures) : textures_(textures) {}
  void draw(Project& project);
  void reset() { file_.clear(); animation_ = 0; }
  /// Opens `file` (a package-relative sprite path) for editing.
  void show(Project& project, const std::string& file);

 private:
  Textures& textures_;
  std::string file_;
  std::size_t animation_ = 0;
  float zoom_ = 3.0f;
  double preview_time_ = 0.0;
  bool preview_playing_ = true;

  // grid tool
  int grid_x_ = 0, grid_y_ = 0, grid_w_ = 32, grid_h_ = 32, grid_cols_ = 4, grid_rows_ = 1, grid_gap_ = 0;

  // dragging a new frame on the sheet
  int drag_x_ = 0, drag_y_ = 0;

  // dialogs
  std::string new_image_, new_name_, error_, rename_text_;
  bool ask_new_ = false, ask_rename_ = false;
};

}  // namespace myrts::editor
