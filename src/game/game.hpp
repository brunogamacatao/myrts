#pragma once

#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <tmx.h>

#include <expected>
#include <memory>
#include <string>
#include <vector>

#include "core/geometry.hpp"
#include "core/grid.hpp"
#include "core/occupancy.hpp"
#include "ui.hpp"
#include "unit.hpp"

/// Game state, input handling and rendering.
class Game {
 public:
  static std::expected<std::unique_ptr<Game>, std::string> create(ALLEGRO_FONT* font);
  ~Game();
  Game(const Game&) = delete;
  Game& operator=(const Game&) = delete;

  [[nodiscard]] bool running() const { return running_; }
  void handle_event(const ALLEGRO_EVENT& event);
  void update(double dt);
  void draw();

 private:
  Game() = default;

  // input
  void on_mouse_down(const ALLEGRO_EVENT& e);
  void on_mouse_up(const ALLEGRO_EVENT& e);
  void on_mouse_move(const ALLEGRO_EVENT& e);
  void on_key_down(int keycode);
  void click_in_viewport(float screen_x, float screen_y);
  void box_select(float x1, float y1, float x2, float y2);
  void activate_command(Command c);

  // orders / selection
  void issue_move(myrts::Point target_tile);
  void select_only(std::vector<int> ids);
  void toggle_selected(int id);
  void refresh_selection_flags();
  void set_message(std::string text, double seconds = 3.0);
  [[nodiscard]] bool shift_down() const;
  [[nodiscard]] std::optional<myrts::Point> tile_at_world(myrts::Vec2 w) const;

  ALLEGRO_FONT* font_ = nullptr;
  tmx_map* map_ = nullptr;
  ALLEGRO_BITMAP* minimap_bitmap_ = nullptr;
  int tile_size_ = 32;

  myrts::Grid grid_;
  myrts::Occupancy occupancy_;
  std::vector<Unit> units_;  // unit id == index
  std::vector<int> selected_;
  myrts::Camera camera_;
  myrts::Minimap minimap_;
  ui::Resources resources_;

  // mouse / keyboard state
  bool keys_[ALLEGRO_KEY_MAX] = {};
  float mouse_x_ = 0, mouse_y_ = 0;
  bool mouse_in_window_ = false;
  bool left_down_ = false;
  bool left_started_in_viewport_ = false;
  bool dragging_minimap_ = false;
  bool box_selecting_ = false;
  float press_x_ = 0, press_y_ = 0;
  bool awaiting_target_ = false;

  std::string message_;
  double message_time_left_ = 0;
  bool running_ = true;
};
