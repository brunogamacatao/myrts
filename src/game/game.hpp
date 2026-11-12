#pragma once

#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <tmx.h>

#include <array>
#include <expected>
#include <optional>
#include <span>
#include <memory>
#include <string>
#include <vector>

#include "building_art.hpp"
#include "core/building.hpp"
#include "core/geometry.hpp"
#include "core/grid.hpp"
#include "core/harvest.hpp"
#include "core/occupancy.hpp"
#include "ui.hpp"
#include "unit.hpp"

/// A peon's gathering cycle: walk to a tree or gold mine, work, carry the load to a depot, repeat.
struct HarvestJob {
  enum class State { ToResource, Working, ToDepot };
  State state = State::ToResource;
  bool gold = false;
  myrts::Point target{};  // a tree tile, or any tile of the mine
  int mine = -1;          // index into mines_ when gold
  double timer = 0.0;     // work progress, or delay before re-planning a blocked route
};

struct Building {
  myrts::BuildingKind kind;
  myrts::Point origin;
  double progress = 0.0;  // seconds of work done
  bool complete = false;
  int builder = -1;       // unit id currently working on it
};

struct MineSite {
  myrts::GoldMine mine;
  bool busy = false;  // a peon is inside
};

/// A peon heading to (or working on) a construction site.
struct BuildJob {
  enum class State { ToSite, Building };
  State state = State::ToSite;
  int building = 0;
  double timer = 0.0;
};

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
  void issue_harvest(myrts::Point tile);
  void issue_build_on(int building_index);
  /// A peon that started building stays on the job until the construction is finished.
  [[nodiscard]] bool is_building(int unit_id) const { return build_jobs_[static_cast<std::size_t>(unit_id)].has_value(); }
  /// The selected units that are free to take new orders.
  [[nodiscard]] std::vector<int> free_selected() const;
  void cancel_job(int unit_id);

  // gathering
  void update_jobs(double dt);
  void update_harvest_job(Unit& unit, HarvestJob& job, double dt);
  void update_build_job(Unit& unit, BuildJob& job, double dt);
  [[nodiscard]] std::optional<myrts::ApproachPath> approach(const Unit& unit,
                                                            std::span<const myrts::Point> targets) const;
  bool start_gathering(Unit& unit, bool gold, myrts::Point target);
  bool start_gathering_nearest(Unit& unit, bool gold);
  bool send_to_depot(Unit& unit);
  void remove_tree(myrts::Point tile);
  [[nodiscard]] int mine_at(myrts::Point tile) const;
  void remove_mine(int index);

  // buildings
  void start_placing(myrts::BuildingKind kind);
  void try_place_building(myrts::Point origin);
  bool start_building(Unit& unit, int building_index);
  [[nodiscard]] int building_at(myrts::Point tile) const;
  [[nodiscard]] myrts::Point placement_origin(myrts::BuildingKind kind, myrts::Point cursor_tile) const;
  void refresh_depots();
  void close_build_menu();
  void redraw_minimap_bitmap();
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
  myrts::TreeField trees_;
  std::vector<MineSite> mines_;
  std::vector<Building> buildings_;
  std::vector<myrts::Point> depot_tiles_;  // tiles of finished depots, where loads are delivered
  std::vector<std::optional<HarvestJob>> jobs_;   // by unit id
  std::vector<std::optional<BuildJob>> build_jobs_;  // by unit id
  BuildingArt art_;
  bool build_menu_ = false;
  std::optional<myrts::BuildingKind> placing_;
  double build_speed_ = 1.0;  // construction time divisor (MYRTS_BUILD_SPEED, for testing)

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
  Command target_command_ = Command::Move;  // what the awaited click will do

  std::string message_;
  double message_time_left_ = 0;
  bool running_ = true;
};
