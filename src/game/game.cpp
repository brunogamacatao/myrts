#include "game.hpp"

#include <allegro5/allegro_primitives.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/formation.hpp"
#include "core/pathfinding.hpp"
#include "core/selection.hpp"
#include "tilemap.hpp"

namespace {
constexpr float kScrollSpeed = 420.0f;  // pixels per second
constexpr float kEdgeScrollMargin = 6.0f;
constexpr float kDragThreshold = 4.0f;
constexpr int kInitialUnits = 8;

const ALLEGRO_COLOR kSelectGreen = al_map_rgb(60, 230, 60);
}  // namespace

std::expected<std::unique_ptr<Game>, std::string> Game::create(ALLEGRO_FONT* font) {
  std::unique_ptr<Game> game(new Game());
  game->font_ = font;

  init_tilemap_engine();
  game->map_ = tmx_load("tiles/mapa.tmx");
  if (!game->map_) return std::unexpected(std::string("cannot load map: ") + tmx_strerr());
  const tmx_map* map = game->map_;

  game->tile_size_ = static_cast<int>(map->tile_width);
  game->grid_ = get_collision_grid(map);
  game->occupancy_ = myrts::Occupancy(game->grid_.width(), game->grid_.height());

  const auto world_w = static_cast<float>(map->width * map->tile_width);
  const auto world_h = static_cast<float>(map->height * map->tile_height);
  game->camera_ = {0, 0, ui::kViewport.w, ui::kViewport.h, world_w, world_h};
  game->minimap_ = {world_w, world_h, ui::kMinimap.w, ui::kMinimap.h};

  // Render the whole map once into a bitmap used by the minimap.
  game->minimap_bitmap_ = al_create_bitmap(static_cast<int>(world_w), static_cast<int>(world_h));
  if (!game->minimap_bitmap_) return std::unexpected("cannot create minimap bitmap");
  ALLEGRO_BITMAP* previous_target = al_get_target_bitmap();
  al_set_target_bitmap(game->minimap_bitmap_);
  al_clear_to_color(al_map_rgb(0, 0, 0));
  render_map(game->map_);
  al_set_target_bitmap(previous_target);

  auto type = load_unit_type("units/peon.json");
  if (!type) return std::unexpected(type.error());

  // Spawn a group of units in distinct walkable tiles near the top-left corner.
  const std::vector<myrts::Point> start(kInitialUnits, myrts::Point{3, 3});
  const auto spots = myrts::assign_destinations(game->grid_, nullptr, start, {3, 3});
  game->units_.reserve(spots.size());
  for (const auto& spot : spots) {
    if (!spot) continue;
    const int id = static_cast<int>(game->units_.size());
    game->units_.emplace_back(id, *type, *spot, game->tile_size_, game->occupancy_);
  }
  game->camera_.center_on(static_cast<float>(3 * game->tile_size_), static_cast<float>(3 * game->tile_size_));
  return game;
}

Game::~Game() {
  if (minimap_bitmap_) al_destroy_bitmap(minimap_bitmap_);
  if (map_) destroy_map(map_);
}

bool Game::shift_down() const { return keys_[ALLEGRO_KEY_LSHIFT] || keys_[ALLEGRO_KEY_RSHIFT]; }

void Game::set_message(std::string text, double seconds) {
  message_ = std::move(text);
  message_time_left_ = seconds;
}

std::optional<myrts::Point> Game::tile_at_world(myrts::Vec2 w) const {
  if (w.x < 0 || w.y < 0) return std::nullopt;
  const myrts::Point t{static_cast<int>(w.x) / tile_size_, static_cast<int>(w.y) / tile_size_};
  if (!grid_.contains(t)) return std::nullopt;
  return t;
}

// ---- selection & orders ---------------------------------------------------

void Game::refresh_selection_flags() {
  for (auto& u : units_) u.selected = false;
  for (const int id : selected_) units_[static_cast<std::size_t>(id)].selected = true;
}

void Game::select_only(std::vector<int> ids) {
  selected_ = std::move(ids);
  refresh_selection_flags();
  awaiting_target_ = false;
}

void Game::toggle_selected(int id) {
  const auto it = std::ranges::find(selected_, id);
  if (it == selected_.end()) {
    selected_.push_back(id);
  } else {
    selected_.erase(it);
  }
  refresh_selection_flags();
}

void Game::issue_move(myrts::Point target) {
  if (selected_.empty()) return;

  std::vector<myrts::Point> anchors;
  anchors.reserve(selected_.size());
  for (const int id : selected_) anchors.push_back(units_[static_cast<std::size_t>(id)].anchor());

  // Tiles held by units outside the group are obstacles; the group's own tiles are not.
  const myrts::Grid occupied = occupancy_.blocked_except(selected_);
  const auto destinations = myrts::assign_destinations(grid_, &occupied, anchors, target);

  for (std::size_t i = 0; i < selected_.size(); ++i) {
    Unit& unit = units_[static_cast<std::size_t>(selected_[i])];
    if (!destinations[i]) {
      unit.stop();
      continue;
    }
    unit.set_path(myrts::a_star(grid_, anchors[i], *destinations[i], &occupied));
  }
}

void Game::activate_command(Command c) {
  awaiting_target_ = false;
  switch (c) {
    case Command::Move:
      awaiting_target_ = true;
      set_message("Select destination", 10.0);
      break;
    case Command::Stop:
      for (const int id : selected_) units_[static_cast<std::size_t>(id)].stop();
      break;
    default:
      set_message(std::string(command_label(c)) + " is not implemented yet");
      break;
  }
}

void Game::box_select(float x1, float y1, float x2, float y2) {
  const auto a = camera_.to_world(x1 - ui::kViewport.x, y1 - ui::kViewport.y);
  const auto b = camera_.to_world(x2 - ui::kViewport.x, y2 - ui::kViewport.y);
  std::vector<myrts::SelectBox> boxes;
  boxes.reserve(units_.size());
  for (const auto& u : units_) boxes.push_back({u.id(), u.box()});

  auto ids = myrts::pick_in(boxes, myrts::Rect::from_corners(a.x, a.y, b.x, b.y));
  if (shift_down()) {
    for (const int id : selected_) {
      if (std::ranges::find(ids, id) == ids.end()) ids.push_back(id);
    }
  }
  select_only(std::move(ids));
}

void Game::click_in_viewport(float sx, float sy) {
  const auto world = camera_.to_world(sx - ui::kViewport.x, sy - ui::kViewport.y);

  if (awaiting_target_) {  // after pressing the Move button
    awaiting_target_ = false;
    message_.clear();
    if (const auto tile = tile_at_world(world)) issue_move(*tile);
    return;
  }

  std::vector<myrts::SelectBox> boxes;
  boxes.reserve(units_.size());
  for (const auto& u : units_) boxes.push_back({u.id(), u.box()});

  if (const auto hit = myrts::pick_at(boxes, world.x, world.y)) {
    if (shift_down()) {
      toggle_selected(*hit);
    } else {
      select_only({*hit});
    }
  } else if (!selected_.empty()) {
    if (const auto tile = tile_at_world(world)) issue_move(*tile);
  }
}

// ---- events ---------------------------------------------------------------

void Game::handle_event(const ALLEGRO_EVENT& e) {
  switch (e.type) {
    case ALLEGRO_EVENT_KEY_DOWN:
      keys_[e.keyboard.keycode] = true;
      on_key_down(e.keyboard.keycode);
      break;
    case ALLEGRO_EVENT_KEY_UP:
      keys_[e.keyboard.keycode] = false;
      break;
    case ALLEGRO_EVENT_MOUSE_AXES:
      on_mouse_move(e);
      break;
    case ALLEGRO_EVENT_MOUSE_ENTER_DISPLAY:
      mouse_in_window_ = true;
      break;
    case ALLEGRO_EVENT_MOUSE_LEAVE_DISPLAY:
      mouse_in_window_ = false;
      break;
    case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
      on_mouse_down(e);
      break;
    case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
      on_mouse_up(e);
      break;
    case ALLEGRO_EVENT_DISPLAY_CLOSE:
      running_ = false;
      break;
    case ALLEGRO_EVENT_DISPLAY_SWITCH_OUT:
      std::ranges::fill(keys_, false);  // avoid stuck keys while unfocused
      left_down_ = false;
      box_selecting_ = false;
      dragging_minimap_ = false;
      break;
    default:
      break;
  }
}

void Game::on_key_down(int keycode) {
  switch (keycode) {
    case ALLEGRO_KEY_ESCAPE:
      if (awaiting_target_) {
        awaiting_target_ = false;
        message_.clear();
      } else if (!selected_.empty()) {
        select_only({});
      } else {
        running_ = false;
      }
      break;
    case ALLEGRO_KEY_F10:
      set_message("Menu is not implemented yet");
      break;
    case ALLEGRO_KEY_M:
      if (!selected_.empty()) activate_command(Command::Move);
      break;
    case ALLEGRO_KEY_S:
      if (!selected_.empty()) activate_command(Command::Stop);
      break;
    default:
      break;
  }
}

void Game::on_mouse_move(const ALLEGRO_EVENT& e) {
  mouse_x_ = static_cast<float>(e.mouse.x);
  mouse_y_ = static_cast<float>(e.mouse.y);
  mouse_in_window_ = true;

  if (dragging_minimap_) {
    const auto w = minimap_.mini_to_world(mouse_x_ - ui::kMinimap.x, mouse_y_ - ui::kMinimap.y);
    camera_.center_on(w.x, w.y);
  } else if (left_down_ && left_started_in_viewport_ && !box_selecting_) {
    if (std::hypot(mouse_x_ - press_x_, mouse_y_ - press_y_) > kDragThreshold) box_selecting_ = true;
  }
}

void Game::on_mouse_down(const ALLEGRO_EVENT& e) {
  mouse_x_ = static_cast<float>(e.mouse.x);
  mouse_y_ = static_cast<float>(e.mouse.y);
  const bool in_viewport = ui::kViewport.contains(mouse_x_, mouse_y_);
  const bool in_minimap = ui::kMinimap.contains(mouse_x_, mouse_y_);

  if (e.mouse.button == 2) {  // right button: move order
    awaiting_target_ = false;
    if (in_viewport) {
      const auto w = camera_.to_world(mouse_x_ - ui::kViewport.x, mouse_y_ - ui::kViewport.y);
      if (const auto tile = tile_at_world(w)) issue_move(*tile);
    } else if (in_minimap) {
      const auto w = minimap_.mini_to_world(mouse_x_ - ui::kMinimap.x, mouse_y_ - ui::kMinimap.y);
      if (const auto tile = tile_at_world(w)) issue_move(*tile);
    }
    return;
  }
  if (e.mouse.button != 1 || left_down_) return;

  left_down_ = true;
  press_x_ = mouse_x_;
  press_y_ = mouse_y_;
  left_started_in_viewport_ = in_viewport;
  box_selecting_ = false;

  if (in_minimap) {
    dragging_minimap_ = true;
    const auto w = minimap_.mini_to_world(mouse_x_ - ui::kMinimap.x, mouse_y_ - ui::kMinimap.y);
    camera_.center_on(w.x, w.y);
  } else if (ui::kMenuButton.contains(mouse_x_, mouse_y_)) {
    set_message("Menu is not implemented yet");
  } else if (const auto slot = ui::button_at(mouse_x_, mouse_y_)) {
    const auto commands = ui::panel_commands(units_, selected_);
    if (static_cast<std::size_t>(*slot) < commands.size()) activate_command(commands[static_cast<std::size_t>(*slot)]);
  }
}

void Game::on_mouse_up(const ALLEGRO_EVENT& e) {
  if (e.mouse.button != 1) return;
  mouse_x_ = static_cast<float>(e.mouse.x);
  mouse_y_ = static_cast<float>(e.mouse.y);

  if (left_down_ && left_started_in_viewport_) {
    if (box_selecting_) {
      box_select(press_x_, press_y_, mouse_x_, mouse_y_);
    } else {
      click_in_viewport(mouse_x_, mouse_y_);
    }
  }
  left_down_ = false;
  box_selecting_ = false;
  dragging_minimap_ = false;
}

// ---- simulation -----------------------------------------------------------

void Game::update(double dt) {
  // Camera: arrow keys and edge scrolling (not while dragging a selection box or the minimap).
  float dx = 0, dy = 0;
  if (keys_[ALLEGRO_KEY_LEFT]) dx -= 1;
  if (keys_[ALLEGRO_KEY_RIGHT]) dx += 1;
  if (keys_[ALLEGRO_KEY_UP]) dy -= 1;
  if (keys_[ALLEGRO_KEY_DOWN]) dy += 1;
  if (mouse_in_window_ && !dragging_minimap_ && mouse_x_ >= ui::kPanelW) {
    if (mouse_x_ < ui::kViewport.x + kEdgeScrollMargin) dx -= 1;
    if (mouse_x_ >= ui::kWidth - kEdgeScrollMargin) dx += 1;
    if (mouse_y_ < ui::kViewport.y + kEdgeScrollMargin) dy -= 1;
    if (mouse_y_ >= ui::kHeight - kEdgeScrollMargin) dy += 1;
  }
  if (dx != 0 || dy != 0) {
    const auto step = kScrollSpeed * static_cast<float>(dt);
    camera_.move(dx * step, dy * step);
  }

  for (auto& unit : units_) unit.update(dt, grid_, occupancy_);

  if (message_time_left_ > 0 && (message_time_left_ -= dt) <= 0 && !awaiting_target_) message_.clear();
}

// ---- rendering ------------------------------------------------------------

void Game::draw() {
  al_clear_to_color(al_map_rgb(0, 0, 0));

  // Scene, clipped to the viewport and translated by the camera.
  const auto& vp = ui::kViewport;
  al_set_clipping_rectangle(static_cast<int>(vp.x), static_cast<int>(vp.y), static_cast<int>(vp.w),
                            static_cast<int>(vp.h));
  ALLEGRO_TRANSFORM world;
  al_identity_transform(&world);
  al_translate_transform(&world, std::floor(vp.x - camera_.x), std::floor(vp.y - camera_.y));
  al_use_transform(&world);

  render_map(map_);

  // Units sorted by y so lower ones overlap higher ones.
  std::vector<const Unit*> order;
  order.reserve(units_.size());
  for (const auto& u : units_) order.push_back(&u);
  std::ranges::sort(order, [](const Unit* a, const Unit* b) { return a->y() < b->y(); });
  for (const Unit* u : order) {
    draw_sprite_centered(*u->type().sprite, u->animation(), u->x(), u->y());
    if (u->selected) {
      const auto b = u->box();
      al_draw_rectangle(b.x + 2, b.y + 2, b.x + b.w - 2, b.y + b.h - 2, kSelectGreen, 1.5f);
    }
  }

  ALLEGRO_TRANSFORM identity;
  al_identity_transform(&identity);
  al_use_transform(&identity);

  if (box_selecting_) {  // drag rectangle, still clipped to the viewport
    al_draw_rectangle(press_x_ + 0.5f, press_y_ + 0.5f, mouse_x_ + 0.5f, mouse_y_ + 0.5f, kSelectGreen, 1);
  }
  al_set_clipping_rectangle(0, 0, ui::kWidth, ui::kHeight);

  const ui::HudData hud{resources_, units_, selected_, minimap_bitmap_, minimap_, camera_,
                        message_, awaiting_target_, mouse_x_, mouse_y_};
  ui::draw_hud(font_, hud);
}
