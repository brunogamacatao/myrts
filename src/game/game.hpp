#pragma once

#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>

#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "assets.hpp"
#include "building_art.hpp"
#include "core/ai.hpp"
#include "core/geometry.hpp"
#include "core/rules.hpp"
#include "core/selection.hpp"
#include "core/world.hpp"
#include "raii.hpp"
#include "scene.hpp"
#include "sound_director.hpp"
#include "tile_renderer.hpp"
#include "ui_art.hpp"
#include "ui.hpp"

/// A body left where a unit died: plays its falling animation, rests for a while and fades away.
struct Corpse {
  const myrts::UnitDef* def = nullptr;
  float x = 0;
  float y = 0;
  AnimationState anim;
  double age = 0.0;
};

/// The player's view of a match: input, selection, camera, HUD and drawing on top of a `World`.
/// Everything that is a rule of the game lives in the World; this class only turns clicks into
/// orders and the World's state (and events) into pixels.
///
/// Implementation is split by responsibility: game.cpp (setup, update, commands),
/// game_input.cpp (mouse and keyboard) and game_render.cpp (drawing).
class Game {
 public:
  static std::expected<std::unique_ptr<Game>, std::string> create(const SceneContext& ctx);
  Game(const Game&) = delete;
  Game& operator=(const Game&) = delete;

  /// True (once) after the player asked for the game menu (ESC with nothing to cancel, F10, Menu button).
  [[nodiscard]] bool take_menu_request() { return std::exchange(menu_requested_, false); }
  /// Forgets pressed keys and drags, e.g. when the game regains focus after a pause menu.
  void release_input();
  void handle_event(const ALLEGRO_EVENT& event);
  void update(double dt);
  void draw();

  /// Set once the match is decided (see core/rules.hpp).
  [[nodiscard]] std::optional<myrts::Outcome> outcome() const;

 private:
  Game() = default;

  // ---- setup / events (game.cpp)
  void subscribe_to_world();
  void register_commands();
  void set_message(std::string text, double seconds = 3.0);
  void apply(const myrts::Result& result);
  /// Like apply(), and the selected units answer an order that was accepted ("yes, sir").
  void ordered(const myrts::Result& result);
  void play_selection_sound(bool same_unit_again);
  void play_world_sound(const myrts::Event& e);
  [[nodiscard]] const SpriteDef* sprite_of(const myrts::UnitDef& def);

  // ---- commands & selection (game.cpp)
  void activate_command(const myrts::CommandDef& command);
  void select_only(std::vector<myrts::EntityId> ids);
  void toggle_selected(myrts::EntityId id);
  void close_build_menu();
  void select_building(myrts::EntityId id);
  void start_placing(const myrts::BuildingDef& def);
  void try_place_building(myrts::Point origin);
  [[nodiscard]] std::vector<const myrts::CommandDef*> current_commands() const;
  [[nodiscard]] std::vector<const myrts::BuildingDef*> current_build_options() const;
  /// The build-menu buttons that fit in one page (and whether more pages exist).
  [[nodiscard]] std::vector<const myrts::BuildingDef*> build_page(bool* has_more = nullptr) const;
  [[nodiscard]] std::vector<const myrts::UnitDef*> current_train_options() const;
  [[nodiscard]] myrts::Point placement_origin(const myrts::BuildingDef& def, myrts::Point cursor_tile) const;
  [[nodiscard]] std::string unit_animation(const myrts::Unit& u, const SpriteDef& sprite) const;

  // ---- input (game_input.cpp)
  void on_mouse_down(const ALLEGRO_EVENT& e);
  void on_mouse_up(const ALLEGRO_EVENT& e);
  void on_mouse_move(const ALLEGRO_EVENT& e);
  void on_key_down(int keycode);
  void click_in_viewport(float screen_x, float screen_y);
  void right_click_at(myrts::Point tile);
  void box_select(float x1, float y1, float x2, float y2);
  [[nodiscard]] bool shift_down() const;
  [[nodiscard]] std::optional<myrts::Point> tile_at_world(myrts::Vec2 w) const;
  [[nodiscard]] myrts::Rect unit_box(const myrts::Unit& u) const;
  /// How far a flying unit is drawn above the ground (its shadow stays on the ground).
  static float lift_of(const myrts::UnitDef& def) { return def.move_class == myrts::MoveClass::Air ? 14.0f : 0.0f; }
  /// Selectable units (own, not hidden in a mine); box ids are positions in `ids`.
  void selectable_units(std::vector<myrts::EntityId>& ids, std::vector<myrts::SelectBox>& boxes) const;

  // ---- drawing (game_render.cpp)
  void draw_world();
  void update_cursor(float edge_dx, float edge_dy);
  [[nodiscard]] std::string selected_faction() const;
  void draw_placement_ghost();
  void draw_health_bar(float x, float y, float w, int hp, int max_hp);

  // services (owned by main)
  ALLEGRO_FONT* font_ = nullptr;
  Cursors* cursors_ = nullptr;
  UiArt ui_art_;
  Assets* assets_ = nullptr;
  const myrts::Registry* registry_ = nullptr;
  std::shared_ptr<const myrts::SoundTable> sounds_;
  std::unique_ptr<SoundDirector> director_;
  myrts::EntityId last_clicked_;  // for the "annoyed" lines when the same unit is clicked over and over
  int repeat_clicks_ = 0;

  // simulation
  std::unique_ptr<myrts::World> world_;
  myrts::PlayerId local_player_ = myrts::kNoPlayer;
  std::vector<std::unique_ptr<myrts::Ai>> ais_;  // the computer players

  // map graphics
  TileRenderer terrain_view_;
  int tile_size_ = TileRenderer::kTile;

  // view state
  std::vector<myrts::EntityId> selected_;
  myrts::EntityId selected_building_;  // a building is selected instead of units
  std::unordered_map<myrts::EntityId, AnimationState, myrts::EntityIdHash> anims_;
  std::vector<Corpse> corpses_;
  std::unordered_map<const myrts::UnitDef*, std::shared_ptr<const SpriteDef>> sprites_;
  BuildingArt art_;
  myrts::Camera camera_;
  myrts::Minimap minimap_;

  // commands: behaviour is registered by command id (see register_commands)
  std::unordered_map<std::string, std::function<void()>> instant_commands_;
  std::unordered_map<std::string, std::function<void(myrts::Point)>> targeted_commands_;
  const myrts::CommandDef* awaiting_target_ = nullptr;  // command waiting for its target click
  bool build_menu_ = false;                              // panel shows the buildings to construct
  int build_page_ = 0;                                   // which page of the build menu
  const myrts::BuildingDef* placing_ = nullptr;          // building being positioned

  // mouse / keyboard state
  bool keys_[ALLEGRO_KEY_MAX] = {};
  float mouse_x_ = 0, mouse_y_ = 0;
  bool mouse_in_window_ = false;
  bool left_down_ = false;
  bool left_started_in_viewport_ = false;
  bool dragging_minimap_ = false;
  bool box_selecting_ = false;
  float press_x_ = 0, press_y_ = 0;

  std::string message_;
  double message_time_left_ = 0;
  bool menu_requested_ = false;
};
