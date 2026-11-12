#include "game.hpp"

#include <allegro5/allegro_primitives.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "core/formation.hpp"
#include "core/pathfinding.hpp"
#include "core/selection.hpp"
#include "tilemap.hpp"

namespace {
constexpr float kScrollSpeed = 420.0f;  // pixels per second
constexpr float kEdgeScrollMargin = 6.0f;
constexpr float kDragThreshold = 4.0f;
constexpr int kInitialUnits = 8;
constexpr int kLumberPerTree = 100;
constexpr int kLumberPerTrip = 10;
constexpr int kGoldPerMine = 10000;
constexpr int kGoldPerTrip = 100;
constexpr double kChopTime = 3.0;    // seconds to fill a load of lumber
constexpr double kMineTime = 2.5;    // seconds spent inside the mine per load of gold
constexpr double kRetryDelay = 0.5;  // seconds before re-planning a blocked route
constexpr unsigned kGrassGid = 191;  // replaces a felled tree
constexpr myrts::Point kStartHall{1, 6};  // the Great Hall the game starts with
constexpr myrts::Point kMineOrigin{22, 6};
constexpr int kMineSize = 3;
constexpr myrts::Stock kStartingStock{2000, 1000, 0};

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

  for (const auto& tree : find_tree_tiles(map)) game->trees_.add(tree, kLumberPerTree);
  if (!game->art_.load()) return std::unexpected("cannot load building sprites (assets/buildings)");
  game->buildings_.push_back({myrts::BuildingKind::GreatHall, kStartHall, 0.0, true, -1});
  for (const auto p : myrts::footprint(kStartHall, myrts::building_spec(myrts::BuildingKind::GreatHall).size))
    game->grid_.set_blocked(p);
  game->refresh_depots();
  game->mines_.push_back({myrts::GoldMine(kMineOrigin, kMineSize, kGoldPerMine), false});
  for (const auto p : game->mines_.back().mine.tiles()) game->grid_.set_blocked(p);
  if (const char* speed = std::getenv("MYRTS_BUILD_SPEED")) game->build_speed_ = std::max(0.01, std::atof(speed));
  game->resources_ = kStartingStock;

  // Render the whole map once into a bitmap used by the minimap.
  game->minimap_bitmap_ = al_create_bitmap(static_cast<int>(world_w), static_cast<int>(world_h));
  if (!game->minimap_bitmap_) return std::unexpected("cannot create minimap bitmap");
  game->redraw_minimap_bitmap();

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
  game->jobs_.assign(game->units_.size(), std::nullopt);
  game->build_jobs_.assign(game->units_.size(), std::nullopt);
  game->camera_.center_on(static_cast<float>(3 * game->tile_size_), static_cast<float>(3 * game->tile_size_));
  return game;
}

void Game::redraw_minimap_bitmap() {
  ALLEGRO_BITMAP* previous_target = al_get_target_bitmap();
  al_set_target_bitmap(minimap_bitmap_);
  al_clear_to_color(al_map_rgb(0, 0, 0));
  render_map(map_);
  al_set_target_bitmap(previous_target);
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
  close_build_menu();
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

std::vector<int> Game::free_selected() const {
  std::vector<int> ids;
  for (const int id : selected_)
    if (!is_building(id)) ids.push_back(id);
  return ids;
}

void Game::issue_move(myrts::Point target) {
  if (selected_.empty()) return;
  const std::vector<int> movers = free_selected();
  if (movers.empty()) {
    set_message("Builders cannot be interrupted");
    return;
  }
  for (const int id : movers) cancel_job(id);

  std::vector<myrts::Point> anchors;
  anchors.reserve(movers.size());
  for (const int id : movers) anchors.push_back(units_[static_cast<std::size_t>(id)].anchor());

  // Tiles held by units outside the group are obstacles; the group's own tiles are not.
  const myrts::Grid occupied = occupancy_.blocked_except(movers);
  const auto destinations = myrts::assign_destinations(grid_, &occupied, anchors, target);

  for (std::size_t i = 0; i < movers.size(); ++i) {
    Unit& unit = units_[static_cast<std::size_t>(movers[i])];
    if (!destinations[i]) {
      unit.stop();
      continue;
    }
    unit.set_path(myrts::a_star(grid_, anchors[i], *destinations[i], &occupied));
  }
}

// ---- gathering & building ---------------------------------------------------

namespace {
bool adjacent_to_any(myrts::Point p, std::span<const myrts::Point> tiles) {
  return std::ranges::any_of(tiles, [&](myrts::Point t) { return myrts::is_adjacent(p, t); });
}
}  // namespace

void Game::cancel_job(int id) {
  Unit& unit = units_[static_cast<std::size_t>(id)];
  auto& job = jobs_[static_cast<std::size_t>(id)];
  if (job && job->gold && job->mine >= 0 && job->state == HarvestJob::State::Working)
    mines_[static_cast<std::size_t>(job->mine)].busy = false;
  job.reset();
  auto& bjob = build_jobs_[static_cast<std::size_t>(id)];
  if (bjob) buildings_[static_cast<std::size_t>(bjob->building)].builder = -1;
  bjob.reset();
  unit.set_chopping(false);
  unit.hidden = false;
}

/// Route to a tile next to one of `targets`, avoiding other units when possible. If they
/// hem the unit in, plan through them anyway: Unit re-plans around whoever is still in the way.
std::optional<myrts::ApproachPath> Game::approach(const Unit& unit, std::span<const myrts::Point> targets) const {
  const myrts::Grid others = occupancy_.blocked_for(unit.id());
  auto route = myrts::path_to_adjacent(grid_, unit.anchor(), targets, &others);
  if (!route) route = myrts::path_to_adjacent(grid_, unit.anchor(), targets);
  return route;
}

int Game::mine_at(myrts::Point tile) const {
  for (std::size_t i = 0; i < mines_.size(); ++i) {
    if (mines_[i].mine.depleted()) continue;
    const auto tiles = mines_[i].mine.tiles();
    if (std::ranges::find(tiles, tile) != tiles.end()) return static_cast<int>(i);
  }
  return -1;
}

int Game::building_at(myrts::Point tile) const {
  for (std::size_t i = 0; i < buildings_.size(); ++i) {
    const auto tiles = myrts::footprint(buildings_[i].origin, myrts::building_spec(buildings_[i].kind).size);
    if (std::ranges::find(tiles, tile) != tiles.end()) return static_cast<int>(i);
  }
  return -1;
}

bool Game::start_gathering(Unit& unit, bool gold, myrts::Point target) {
  std::vector<myrts::Point> targets;
  int mine = -1;
  if (gold) {
    mine = mine_at(target);
    if (mine < 0) return false;
    targets = mines_[static_cast<std::size_t>(mine)].mine.tiles();
  } else {
    if (!trees_.has_tree(target)) return false;
    targets = {target};
  }
  auto route = approach(unit, targets);
  if (!route) return false;
  unit.set_chopping(false);
  unit.set_path(std::move(route->path));
  jobs_[static_cast<std::size_t>(unit.id())] = HarvestJob{HarvestJob::State::ToResource, gold, target, mine, 0.0};
  return true;
}

bool Game::start_gathering_nearest(Unit& unit, bool gold) {
  std::vector<myrts::Point> targets;
  if (gold) {
    for (const auto& m : mines_)
      if (!m.mine.depleted()) {
        const auto tiles = m.mine.tiles();
        targets.insert(targets.end(), tiles.begin(), tiles.end());
      }
  } else {
    targets = trees_.positions();
  }
  auto route = approach(unit, targets);
  if (!route) return false;
  unit.set_chopping(false);
  unit.set_path(std::move(route->path));
  jobs_[static_cast<std::size_t>(unit.id())] =
      HarvestJob{HarvestJob::State::ToResource, gold, route->target, gold ? mine_at(route->target) : -1, 0.0};
  return true;
}

bool Game::send_to_depot(Unit& unit) {
  auto route = approach(unit, depot_tiles_);
  if (!route) return false;
  unit.set_path(std::move(route->path));
  auto& job = jobs_[static_cast<std::size_t>(unit.id())];
  if (job) {
    job->state = HarvestJob::State::ToDepot;
    job->timer = 0.0;
  }
  return true;
}

void Game::remove_tree(myrts::Point tile) {
  grid_.set_blocked(tile, false);
  replace_tree_tile(map_, tile, kGrassGid);
  redraw_minimap_bitmap();
}

void Game::remove_mine(int index) {
  for (const auto p : mines_[static_cast<std::size_t>(index)].mine.tiles()) grid_.set_blocked(p, false);
}

void Game::refresh_depots() {
  depot_tiles_.clear();
  for (const auto& b : buildings_) {
    const auto spec = myrts::building_spec(b.kind);
    if (!b.complete || !spec.is_depot) continue;
    const auto tiles = myrts::footprint(b.origin, spec.size);
    depot_tiles_.insert(depot_tiles_.end(), tiles.begin(), tiles.end());
  }
}

void Game::issue_harvest(myrts::Point tile) {
  if (selected_.empty()) return;
  const int mine = mine_at(tile);
  const bool gold = mine >= 0;
  if (!gold && !trees_.has_tree(tile)) {
    set_message("Select a tree or a gold mine");
    return;
  }
  if (depot_tiles_.empty()) {
    set_message("You need a Great Hall to deliver resources");
    return;
  }
  const std::vector<int> workers = free_selected();
  if (workers.empty()) {
    set_message("Builders cannot be interrupted");
    return;
  }
  bool any_failed = false;
  for (const int id : workers) {
    Unit& unit = units_[static_cast<std::size_t>(id)];
    cancel_job(id);
    unit.stop();
    const Cargo wanted = gold ? Cargo::Gold : Cargo::Lumber;
    if (unit.carrying > 0 && unit.cargo != wanted) unit.carrying = 0;  // drops what it was carrying
    if (!start_gathering(unit, gold, tile)) any_failed = true;
  }
  if (any_failed) set_message(gold ? "Cannot reach that gold mine" : "Cannot reach that tree");
}

void Game::update_harvest_job(Unit& unit, HarvestJob& job, double dt) {
  const auto id = unit.id();
  const auto fail = [&](const char* msg) {
    set_message(msg);
    cancel_job(id);
  };
  const auto retry = [&] {
    const bool alive = job.gold ? mine_at(job.target) >= 0 : trees_.has_tree(job.target);
    const bool ok = alive ? start_gathering(unit, job.gold, job.target) : start_gathering_nearest(unit, job.gold);
    if (!ok) fail(job.gold ? "There is no gold left to mine" : "There are no more trees");
  };

  switch (job.state) {
    case HarvestJob::State::ToResource: {
      if (!unit.is_idle()) break;
      if (job.gold) {
        const int mine = mine_at(job.target);
        if (mine < 0) {
          retry();
          break;
        }
        auto& site = mines_[static_cast<std::size_t>(mine)];
        const auto tiles = site.mine.tiles();
        if (adjacent_to_any(unit.tile(), tiles)) {
          if (site.busy) break;  // wait for the peon inside to come out
          site.busy = true;
          job.mine = mine;
          job.state = HarvestJob::State::Working;
          job.timer = 0.0;
          unit.hidden = true;
          break;
        }
      } else if (trees_.has_tree(job.target) && myrts::is_adjacent(unit.tile(), job.target)) {
        job.state = HarvestJob::State::Working;
        job.timer = 0.0;
        unit.face_tile(job.target);
        unit.set_chopping(true);
        break;
      }
      // Arrived without reaching the resource (blocked route, or someone else used it up).
      if ((job.timer += dt) >= kRetryDelay) {
        job.timer = 0.0;
        retry();
      }
      break;
    }
    case HarvestJob::State::Working: {
      job.timer += dt;
      if (job.gold) {
        if (job.timer < kMineTime) break;
        auto& site = mines_[static_cast<std::size_t>(job.mine)];
        const int got = site.mine.take(kGoldPerTrip);
        site.busy = false;
        unit.hidden = false;
        unit.cargo = Cargo::Gold;
        unit.carrying = got;
        if (site.mine.depleted()) {
          remove_mine(job.mine);
          set_message("The gold mine is exhausted");
        }
      } else {
        if (job.timer < kChopTime) break;
        unit.set_chopping(false);
        const int got = trees_.take(job.target, kLumberPerTrip);
        if (got > 0 && !trees_.has_tree(job.target)) remove_tree(job.target);
        unit.cargo = Cargo::Lumber;
        unit.carrying = got;
      }
      if (unit.carrying > 0) {
        if (!send_to_depot(unit)) fail("Cannot reach a Great Hall");
      } else {
        retry();
      }
      break;
    }
    case HarvestJob::State::ToDepot: {
      if (!unit.is_idle()) break;
      if (adjacent_to_any(unit.tile(), depot_tiles_)) {
        (unit.cargo == Cargo::Gold ? resources_.gold : resources_.lumber) += unit.carrying;
        unit.carrying = 0;
        unit.cargo = Cargo::None;
        retry();
        break;
      }
      if ((job.timer += dt) >= kRetryDelay && !send_to_depot(unit)) fail("Cannot reach a Great Hall");
      break;
    }
  }
}

// ---- construction -----------------------------------------------------------

void Game::close_build_menu() {
  build_menu_ = false;
  placing_.reset();
}

void Game::start_placing(myrts::BuildingKind kind) {
  const auto spec = myrts::building_spec(kind);
  if (!myrts::can_afford(resources_, spec.cost)) {
    set_message(std::string("Not enough resources for a ") + spec.name);
    return;
  }
  placing_ = kind;
  set_message(std::string("Place the ") + spec.name + " (right click cancels)", 10.0);
}

myrts::Point Game::placement_origin(myrts::BuildingKind kind, myrts::Point cursor) const {
  const int size = myrts::building_spec(kind).size;
  return {cursor.x - size / 2, cursor.y - size / 2};
}

void Game::try_place_building(myrts::Point origin) {
  if (!placing_ || selected_.empty()) return;
  const auto kind = *placing_;
  const auto spec = myrts::building_spec(kind);
  const auto tiles = myrts::footprint(origin, spec.size);

  const std::vector<int> candidates = free_selected();
  if (candidates.empty()) {
    set_message("The selected peons are busy building");
    return;
  }
  if (!myrts::can_place(grid_, origin, spec.size)) {
    set_message("Cannot build there");
    return;
  }
  const bool unit_in_way = std::ranges::any_of(
      tiles, [&](myrts::Point p) { return occupancy_.owner(p) != myrts::Occupancy::kNone; });
  if (unit_in_way) {
    set_message("A unit is in the way");
    return;
  }
  if (!myrts::pay(resources_, spec.cost)) {
    set_message(std::string("Not enough resources for a ") + spec.name);
    return;
  }

  buildings_.push_back({kind, origin, 0.0, false, -1});
  for (const auto p : tiles) grid_.set_blocked(p);
  const int index = static_cast<int>(buildings_.size()) - 1;

  // The selected peon with the shortest unobstructed walk builds it (others may be hemmed in).
  int best_id = candidates.front();
  std::size_t best_len = static_cast<std::size_t>(-1);
  for (const int id : candidates) {
    const Unit& candidate = units_[static_cast<std::size_t>(id)];
    const myrts::Grid others = occupancy_.blocked_for(id);
    const auto route = myrts::path_to_adjacent(grid_, candidate.anchor(), tiles, &others);
    if (route && route->path.size() < best_len) {
      best_len = route->path.size();
      best_id = id;
    }
  }
  Unit& builder = units_[static_cast<std::size_t>(best_id)];
  cancel_job(builder.id());
  builder.stop();
  if (!start_building(builder, index)) set_message("The builder cannot reach the construction site");
  placing_.reset();
  build_menu_ = false;
  message_.clear();
}

bool Game::start_building(Unit& unit, int index) {
  auto& b = buildings_[static_cast<std::size_t>(index)];
  const auto tiles = myrts::footprint(b.origin, myrts::building_spec(b.kind).size);
  auto route = approach(unit, tiles);
  if (!route) return false;
  unit.set_chopping(false);
  unit.set_path(std::move(route->path));
  b.builder = unit.id();
  build_jobs_[static_cast<std::size_t>(unit.id())] = BuildJob{BuildJob::State::ToSite, index, 0.0};
  return true;
}

void Game::issue_build_on(int index) {
  if (selected_.empty()) return;
  auto& b = buildings_[static_cast<std::size_t>(index)];
  if (b.complete) return;
  if (b.builder >= 0) {
    set_message("Someone is already building it");
    return;
  }
  const std::vector<int> free = free_selected();
  if (free.empty()) {
    set_message("Builders cannot be interrupted");
    return;
  }
  Unit& unit = units_[static_cast<std::size_t>(free.front())];
  cancel_job(unit.id());
  unit.stop();
  if (!start_building(unit, index)) set_message("Cannot reach the construction site");
}

void Game::update_build_job(Unit& unit, BuildJob& job, double dt) {
  auto& b = buildings_[static_cast<std::size_t>(job.building)];
  const auto spec = myrts::building_spec(b.kind);
  const auto tiles = myrts::footprint(b.origin, spec.size);

  if (job.state == BuildJob::State::ToSite) {
    if (!unit.is_idle()) return;
    if (adjacent_to_any(unit.tile(), tiles)) {
      job.state = BuildJob::State::Building;
      const auto it = std::ranges::find_if(tiles, [&](myrts::Point t) { return myrts::is_adjacent(unit.tile(), t); });
      unit.face_tile(*it);
      unit.set_chopping(true);  // hammering reuses the chopping animation
      return;
    }
    if ((job.timer += dt) >= kRetryDelay) {
      job.timer = 0.0;
      const int index = job.building;
      cancel_job(unit.id());
      if (!start_building(unit, index)) set_message("Cannot reach the construction site");
    }
    return;
  }

  b.progress += dt * build_speed_;
  if (b.progress < spec.build_seconds) return;
  b.progress = spec.build_seconds;
  b.complete = true;
  const int id = unit.id();
  cancel_job(id);  // clears the job and the builder slot
  refresh_depots();
  set_message(std::string(spec.name) + " completed");
}

void Game::update_jobs(double dt) {
  for (auto& unit : units_) {
    const auto id = static_cast<std::size_t>(unit.id());
    if (jobs_[id]) update_harvest_job(unit, *jobs_[id], dt);
    if (build_jobs_[id]) update_build_job(unit, *build_jobs_[id], dt);
  }
}

void Game::activate_command(Command c) {
  awaiting_target_ = false;
  switch (c) {
    case Command::Harvest:
      awaiting_target_ = true;
      target_command_ = Command::Harvest;
      set_message("Select a tree or a gold mine", 10.0);
      break;
    case Command::Build:
      build_menu_ = true;
      set_message("Choose a building", 10.0);
      break;
    case Command::Move:
      awaiting_target_ = true;
      target_command_ = Command::Move;
      set_message("Select destination", 10.0);
      break;
    case Command::Stop:
      for (const int id : free_selected()) {
        cancel_job(id);
        units_[static_cast<std::size_t>(id)].stop();
      }
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

  if (placing_) {
    if (const auto tile = tile_at_world(world)) try_place_building(placement_origin(*placing_, *tile));
    return;
  }

  if (awaiting_target_) {  // after pressing the Move button
    awaiting_target_ = false;
    message_.clear();
    if (const auto tile = tile_at_world(world)) {
      if (target_command_ == Command::Harvest) {
        issue_harvest(*tile);
      } else {
        issue_move(*tile);
      }
    }
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
  } else if (!shift_down()) {
    select_only({});  // clicking empty ground deselects
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
    case ALLEGRO_EVENT_DISPLAY_SWITCH_OUT:
      release_input();  // avoid stuck keys while unfocused
      break;
    default:
      break;
  }
}

void Game::release_input() {
  std::ranges::fill(keys_, false);
  left_down_ = false;
  box_selecting_ = false;
  dragging_minimap_ = false;
}

void Game::on_key_down(int keycode) {
  switch (keycode) {
    case ALLEGRO_KEY_ESCAPE:
      if (placing_ || build_menu_) {
        if (placing_) {
          placing_.reset();
        } else {
          build_menu_ = false;
        }
        message_.clear();
      } else if (awaiting_target_) {
        awaiting_target_ = false;
        message_.clear();
      } else if (!selected_.empty()) {
        select_only({});
      } else {
        menu_requested_ = true;
      }
      break;
    case ALLEGRO_KEY_F10:
      menu_requested_ = true;
      break;
    case ALLEGRO_KEY_M:
      if (!selected_.empty()) activate_command(Command::Move);
      break;
    case ALLEGRO_KEY_B:
      if (!selected_.empty()) activate_command(Command::Build);
      break;
    case ALLEGRO_KEY_H:
      if (!selected_.empty()) activate_command(Command::Harvest);
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

  if (e.mouse.button == 2) {  // right button: context order
    awaiting_target_ = false;
    if (placing_) {  // cancels the placement, keeping the build menu open
      placing_.reset();
      message_.clear();
      return;
    }
    if (in_viewport) {
      const auto w = camera_.to_world(mouse_x_ - ui::kViewport.x, mouse_y_ - ui::kViewport.y);
      if (const auto tile = tile_at_world(w)) {
        const int site = building_at(*tile);
        if (trees_.has_tree(*tile) || mine_at(*tile) >= 0) {  // harvest what was clicked
          issue_harvest(*tile);
        } else if (site >= 0 && !buildings_[static_cast<std::size_t>(site)].complete) {
          issue_build_on(site);  // resume an abandoned construction
        } else {
          issue_move(*tile);
        }
      }
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
    menu_requested_ = true;
  } else if (const auto slot = ui::button_at(mouse_x_, mouse_y_); slot && build_menu_) {
    if (const auto kind = ui::build_menu_slot(*slot)) {
      start_placing(*kind);
    } else if (*slot == ui::kBuildMenuCancel) {
      close_build_menu();
      message_.clear();
    }
  } else if (slot) {
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

  update_jobs(dt);
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

  const auto tile_px = static_cast<float>(tile_size_);
  for (const auto& site : mines_) {
    if (site.mine.depleted()) continue;
    const auto o = site.mine.origin();
    draw_centered_in_square(art_.gold_mine(site.busy), static_cast<float>(o.x) * tile_px,
                            static_cast<float>(o.y) * tile_px, static_cast<float>(site.mine.size()) * tile_px);
  }
  for (const auto& b : buildings_) {
    const auto spec = myrts::building_spec(b.kind);
    const float x = static_cast<float>(b.origin.x) * tile_px;
    const float y = static_cast<float>(b.origin.y) * tile_px;
    const float side = static_cast<float>(spec.size) * tile_px;
    draw_centered_in_square(b.complete ? art_.finished(b.kind) : art_.construction(b.kind), x, y, side);
    if (!b.complete) {  // progress bar above the site
      const float frac = static_cast<float>(b.progress / spec.build_seconds);
      al_draw_filled_rectangle(x, y - 8, x + side, y - 3, al_map_rgb(20, 18, 16));
      al_draw_filled_rectangle(x + 1, y - 7, x + 1 + (side - 2) * frac, y - 4, al_map_rgb(60, 220, 60));
    }
  }

  // Units sorted by y so lower ones overlap higher ones.
  std::vector<const Unit*> order;
  order.reserve(units_.size());
  for (const auto& u : units_) order.push_back(&u);
  std::ranges::sort(order, [](const Unit* a, const Unit* b) { return a->y() < b->y(); });
  for (const Unit* u : order) {
    if (u->hidden) continue;
    draw_sprite_centered(*u->type().sprite, u->animation(), u->x(), u->y());
    if (u->selected) {
      const auto b = u->box();
      al_draw_rectangle(b.x + 2, b.y + 2, b.x + b.w - 2, b.y + b.h - 2, kSelectGreen, 1.5f);
    }
  }

  if (placing_ && mouse_in_window_ && vp.contains(mouse_x_, mouse_y_)) {  // placement ghost
    const auto w = camera_.to_world(mouse_x_ - vp.x, mouse_y_ - vp.y);
    if (const auto tile = tile_at_world(w)) {
      const auto spec = myrts::building_spec(*placing_);
      const auto origin = placement_origin(*placing_, *tile);
      const bool free_of_units = std::ranges::none_of(myrts::footprint(origin, spec.size), [&](myrts::Point p) {
        return occupancy_.owner(p) != myrts::Occupancy::kNone;
      });
      const bool ok = myrts::can_place(grid_, origin, spec.size) && free_of_units;
      const float x = static_cast<float>(origin.x) * tile_px;
      const float y = static_cast<float>(origin.y) * tile_px;
      const float side = static_cast<float>(spec.size) * tile_px;
      al_draw_filled_rectangle(x, y, x + side, y + side,
                               ok ? al_map_rgba(40, 200, 40, 70) : al_map_rgba(220, 40, 40, 90));
      draw_centered_in_square(art_.finished(*placing_), x, y, side, al_map_rgba_f(0.6f, 0.6f, 0.6f, 0.6f));
      al_draw_rectangle(x + 0.5f, y + 0.5f, x + side - 0.5f, y + side - 0.5f,
                        ok ? al_map_rgb(60, 230, 60) : al_map_rgb(230, 60, 60), 2);
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
                        message_, awaiting_target_, mouse_x_, mouse_y_,
                        build_menu_, &art_, placing_};
  ui::draw_hud(font_, hud);
}
