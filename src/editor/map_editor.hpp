#pragma once

#include <imgui.h>

#include <memory>
#include <optional>
#include <string>

#include "editor_model/map_edit.hpp"
#include "editor_model/project.hpp"
#include "textures.hpp"

namespace myrts::editor {

/// The Map tab: the map of the active level (or any other), drawn from the game's tile set, with the terrain brush.
class MapEditor {
 public:
  explicit MapEditor(Textures& textures) : textures_(textures) {}

  /// `level_map` is the map file of the active level (package-relative); `level_index` its position in the campaign.
  void draw(Project& project, const std::string& level_map, std::size_t level_index, const std::string& level_kind);
  void reset();
  /// Deletes the selected start or mine (the Delete key). True if something was deleted.
  bool delete_selected();

  /// Undo/redo of the map being edited (Ctrl+Z while this tab is in front). Returns true if it did something.
  bool undo();
  bool redo();
  void begin_frame() { active_ = false; }
  [[nodiscard]] bool active() const { return active_; }
  [[nodiscard]] bool dirty() const { return edit_ && edit_->dirty(); }
  /// Writes the map if it has unsaved changes. Returns an error message, or empty.
  std::string save(Project& project);

 private:
  void open(Project& project, const std::string& relative);
  void draw_canvas(Project& project);
  void draw_objects_panel(Project& project);
  void handle_object_tools(const ImVec2& origin, float cell, bool hovered);
  void draw_toolbar(Project& project, const std::string& level_map, std::size_t level_index);

  Textures& textures_;
  std::optional<MapContext> context_;
  std::optional<MapEdit> edit_;
  std::string path_;     ///< package-relative file the map is saved to (.mrmap)
  std::string source_;   ///< the file it was read from (maybe a .pud)
  std::string error_;
  bool active_ = false;  ///< this tab was drawn this frame

  enum class Tool { Terrain, Select, AddStart, AddMine };
  enum class Kind { None, Start, Mine };
  Tool tool_ = Tool::Terrain;
  Kind selected_kind_ = Kind::None;
  std::size_t selected_ = 0;
  Point grab_{};  ///< where in the selected object a drag took hold
  std::string new_mine_resource_ = "gold";
  std::string level_kind_ = "land";
  std::uint64_t checked_revision_ = ~0ull;
  std::vector<std::string> problems_;

  Terrain terrain_ = Terrain::GrassLight;
  int brush_ = 2;
  float zoom_ = 1.0f;
  bool grid_ = false;
  bool painting_ = false;

  // new map dialog
  bool ask_new_ = false;
  std::string new_name_ = "new_map";
  int new_width_ = 64, new_height_ = 64, new_tileset_ = 0, new_fill_ = 4;
};

}  // namespace myrts::editor
