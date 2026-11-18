#pragma once

#include <memory>
#include <optional>
#include <string>

#include "collection_editor.hpp"
#include "game_editor.hpp"
#include "map_editor.hpp"
#include "sprite_editor.hpp"
#include "textures.hpp"
#include "editor_model/project.hpp"

namespace myrts::editor {

/// The editor's windows and state, independent of how the frame gets to the screen (see main.cpp). `draw` is called
/// once per frame between ImGui's NewFrame and Render.
class App {
 public:
  App();
  ~App();

  void draw();
  /// True once the user asked to quit and there is nothing left to save (or they chose to discard it).
  [[nodiscard]] bool wants_quit() const { return quit_; }
  /// A request to close the window (the title bar's close button): the same as File > Quit.
  void request_quit();
  /// The window title for the project that is open.
  [[nodiscard]] std::string window_title() const;

  void open_project(const std::string& directory);
  /// Where the game executable is (it is started by "Play").
  void set_game_program(std::string path) { game_program_ = std::move(path); }

 private:
  [[nodiscard]] bool unsaved() const { return project_ && (project_->dirty() || map_->dirty()); }
  void build_default_layout(unsigned int dockspace);
  void draw_menu();
  void draw_dialogs();
  void draw_project_panel();
  void draw_problems_panel();
  void draw_levels_panel();
  void draw_level_inspector();
  void clamp_levels();
  void draw_welcome();
  void save();
  void play_level();
  void close_project();
  void set_status(std::string text) { status_ = std::move(text); }

  std::optional<Project> project_;
  Textures textures_;
  std::unique_ptr<CollectionEditor> characters_, buildings_;
  std::unique_ptr<SpriteEditor> sprites_;
  std::unique_ptr<MapEditor> map_;
  std::unique_ptr<GameEditor> game_;
  RawEditor raw_;
  bool quit_ = false;

  enum class Pending { None, Quit, Close, Open };
  Pending pending_ = Pending::None;  ///< what the "unsaved changes" dialog is guarding
  bool ask_open_ = false, ask_new_ = false, ask_unsaved_ = false, first_frame_ = true;
  std::string open_path_ = "games/warcraft-like";
  std::string new_path_ = "my-game";
  std::string new_title_ = "My Game";
  std::string template_path_ = "games/warcraft-like";
  std::string dialog_error_;
  std::string status_;
  std::string game_program_;
  std::string selected_document_;
  std::size_t selected_level_ = 0;  ///< the one the inspector shows
  std::size_t active_level_ = 0;    ///< the level the editors work on (double-click in the list)
  std::string rename_text_;
  bool ask_rename_ = false;
};

}  // namespace myrts::editor
