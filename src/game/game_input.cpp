#include <algorithm>
#include <cmath>

#include "game.hpp"

namespace {
constexpr float kDragThreshold = 4.0f;
}

bool Game::shift_down() const { return keys_[ALLEGRO_KEY_LSHIFT] || keys_[ALLEGRO_KEY_RSHIFT]; }

std::optional<myrts::Point> Game::tile_at_world(myrts::Vec2 w) const {
  if (w.x < 0 || w.y < 0) return std::nullopt;
  const myrts::Point t{static_cast<int>(w.x) / tile_size_, static_cast<int>(w.y) / tile_size_};
  if (t.x >= world_->width() || t.y >= world_->height()) return std::nullopt;
  return t;
}

myrts::Rect Game::unit_box(const myrts::Unit& u) const {
  const auto s = static_cast<float>(tile_size_);
  return {u.x - s / 2, u.y - s / 2, s, s};
}

void Game::selectable_units(std::vector<myrts::EntityId>& ids, std::vector<myrts::SelectBox>& boxes) const {
  world_->units().for_each([&](const myrts::Unit& u) {
    if (u.owner != local_player_ || u.hidden) return;
    boxes.push_back({static_cast<int>(ids.size()), unit_box(u)});
    ids.push_back(u.id);
  });
}

void Game::release_input() {
  std::ranges::fill(keys_, false);
  left_down_ = false;
  box_selecting_ = false;
  dragging_minimap_ = false;
}

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
    case ALLEGRO_EVENT_DISPLAY_SWITCH_OUT:
      release_input();  // avoid stuck keys while unfocused
      break;
    default:
      break;
  }
}

void Game::on_key_down(int keycode) {
  if (keycode == ALLEGRO_KEY_ESCAPE) {
    if (placing_ || build_menu_) {
      if (placing_) {
        placing_ = nullptr;
      } else {
        build_menu_ = false;
      }
      message_.clear();
    } else if (awaiting_target_) {
      awaiting_target_ = nullptr;
      message_.clear();
    } else if (!selected_.empty()) {
      select_only({});
    } else {
      menu_requested_ = true;
    }
    return;
  }
  if (keycode == ALLEGRO_KEY_F10) {
    menu_requested_ = true;
    return;
  }

  // Command hotkeys come from the command data, for the selected unit's own commands.
  if (keycode >= ALLEGRO_KEY_A && keycode <= ALLEGRO_KEY_Z) {
    const char hotkey = static_cast<char>('A' + (keycode - ALLEGRO_KEY_A));
    for (const myrts::CommandDef* c : current_commands()) {
      if (c->hotkey == hotkey) {
        activate_command(*c);
        return;
      }
    }
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

/// Right click: what the clicked thing suggests. A tree or mine means harvest, an unfinished
/// building means help build it, anything else means walk there.
void Game::right_click_at(myrts::Point tile) {
  const myrts::EntityId site = world_->building_at(tile);
  const myrts::Building* b = site.valid() ? world_->buildings().get(site) : nullptr;
  const myrts::EntityId other = world_->unit_at(tile);
  const myrts::Unit* foe_unit = other.valid() ? world_->units().get(other) : nullptr;
  const bool enemy_unit = foe_unit && myrts::World::is_enemy(local_player_, foe_unit->owner);
  const bool enemy_building = b && myrts::World::is_enemy(local_player_, b->owner);
  if (enemy_unit || enemy_building) {
    apply(world_->order_attack(selected_, tile));
  } else if (world_->trees().has_tree(tile) || world_->mine_at(tile) >= 0) {
    apply(world_->order_harvest(selected_, tile));
  } else if (b && !b->complete) {
    apply(world_->order_resume_build(selected_, site));
  } else {
    apply(world_->order_move(selected_, tile));
  }
}

void Game::on_mouse_down(const ALLEGRO_EVENT& e) {
  mouse_x_ = static_cast<float>(e.mouse.x);
  mouse_y_ = static_cast<float>(e.mouse.y);
  const bool in_viewport = ui::kViewport.contains(mouse_x_, mouse_y_);
  const bool in_minimap = ui::kMinimap.contains(mouse_x_, mouse_y_);

  if (e.mouse.button == 2) {  // right button: context order
    awaiting_target_ = nullptr;
    if (placing_) {  // cancels the placement, keeping the build menu open
      placing_ = nullptr;
      message_.clear();
      return;
    }
    if (in_viewport) {
      const auto w = camera_.to_world(mouse_x_ - ui::kViewport.x, mouse_y_ - ui::kViewport.y);
      if (const auto tile = tile_at_world(w)) right_click_at(*tile);
    } else if (in_minimap) {
      const auto w = minimap_.mini_to_world(mouse_x_ - ui::kMinimap.x, mouse_y_ - ui::kMinimap.y);
      if (const auto tile = tile_at_world(w)) apply(world_->order_move(selected_, *tile));
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
    menu_requested_ = true;
  } else if (const auto queued = ui::queue_slot_at(mouse_x_, mouse_y_); queued && selected_building_.valid()) {
    apply(world_->order_cancel_train(local_player_, selected_building_, static_cast<std::size_t>(*queued)));
  } else if (const auto slot = ui::button_at(mouse_x_, mouse_y_)) {
    const auto index = static_cast<std::size_t>(*slot);
    if (selected_building_.valid()) {
      const auto options = current_train_options();
      if (index < options.size()) apply(world_->order_train(local_player_, selected_building_, *options[index]));
    } else if (build_menu_) {
      const auto options = current_build_options();
      if (*slot == ui::kBuildMenuCancel) {
        close_build_menu();
        message_.clear();
      } else if (index < options.size()) {
        start_placing(*options[index]);
      }
    } else if (const auto commands = current_commands(); index < commands.size()) {
      activate_command(*commands[index]);
    }
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

void Game::box_select(float x1, float y1, float x2, float y2) {
  const auto a = camera_.to_world(x1 - ui::kViewport.x, y1 - ui::kViewport.y);
  const auto b = camera_.to_world(x2 - ui::kViewport.x, y2 - ui::kViewport.y);
  std::vector<myrts::EntityId> candidates;
  std::vector<myrts::SelectBox> boxes;
  selectable_units(candidates, boxes);

  std::vector<myrts::EntityId> ids;
  for (const int i : myrts::pick_in(boxes, myrts::Rect::from_corners(a.x, a.y, b.x, b.y)))
    ids.push_back(candidates[static_cast<std::size_t>(i)]);
  if (shift_down()) {
    for (const auto id : selected_)
      if (std::ranges::find(ids, id) == ids.end()) ids.push_back(id);
  }
  select_only(std::move(ids));
}

void Game::click_in_viewport(float sx, float sy) {
  const auto world = camera_.to_world(sx - ui::kViewport.x, sy - ui::kViewport.y);

  if (placing_) {
    if (const auto tile = tile_at_world(world)) try_place_building(placement_origin(*placing_, *tile));
    return;
  }

  if (awaiting_target_) {  // after pressing a command that needs a target
    const myrts::CommandDef* command = awaiting_target_;
    awaiting_target_ = nullptr;
    message_.clear();
    if (const auto tile = tile_at_world(world)) {
      if (const auto it = targeted_commands_.find(command->id); it != targeted_commands_.end()) it->second(*tile);
    }
    return;
  }

  std::vector<myrts::EntityId> candidates;
  std::vector<myrts::SelectBox> boxes;
  selectable_units(candidates, boxes);

  if (const auto hit = myrts::pick_at(boxes, world.x, world.y)) {
    const myrts::EntityId id = candidates[static_cast<std::size_t>(*hit)];
    if (shift_down()) {
      toggle_selected(id);
    } else {
      select_only({id});
    }
  } else if (const auto tile = tile_at_world(world); tile && !shift_down() &&
                                                       world_->building_at(*tile).valid() &&
                                                       world_->buildings().get(world_->building_at(*tile))->owner ==
                                                           local_player_) {
    select_building(world_->building_at(*tile));  // one of our buildings
  } else if (!shift_down()) {
    select_only({});  // clicking empty ground (or an enemy) deselects
  }
}
