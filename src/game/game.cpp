#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "core/formation.hpp"
#include "tilemap.hpp"

namespace {
constexpr float kScrollSpeed = 420.0f;  // pixels per second
constexpr float kEdgeScrollMargin = 6.0f;
constexpr int kInitialUnits = 8;
constexpr unsigned kGrassGid = 191;       // replaces a felled tree
constexpr int kGoldPerMine = 10000;
constexpr double kCorpseSeconds = 25.0;  // how long a body stays (the last 5 s it fades)
constexpr std::size_t kMaxCorpses = 60;
constexpr int kMineSize = 3;
constexpr myrts::Stock kStartingStock{2000, 1000, 0};

// Start positions (tiles) on assets/tiles/mapa.tmx. The player is in the west, the Alliance in the north-east.
constexpr myrts::Point kPlayerHall{1, 6};
constexpr myrts::Point kPlayerMine{6, 0};
constexpr myrts::Point kPlayerUnits{3, 3};
constexpr myrts::Point kEnemyHall{19, 0};
constexpr myrts::Point kEnemyBarracks{13, 0};
constexpr myrts::Point kEnemyMine{16, 0};
constexpr myrts::Point kEnemyStables{9, 0};
constexpr myrts::Point kEnemyAviary{22, 6};
constexpr myrts::Point kEnemyUnits{18, 4};
constexpr myrts::Stock kEnemyStock{1000, 300, 0};
}  // namespace

// ---- setup ------------------------------------------------------------------------

std::expected<std::unique_ptr<Game>, std::string> Game::create(const SceneContext& ctx) {
  std::unique_ptr<Game> game(new Game());
  game->font_ = ctx.font;
  game->assets_ = ctx.assets;
  game->registry_ = ctx.registry;
  const myrts::Registry& registry = *ctx.registry;

  const myrts::UnitDef* peon = registry.unit("orc_peon");
  const myrts::UnitDef* peasant = registry.unit("alliance_peasant");
  const myrts::BuildingDef* hall = registry.building("great_hall");
  const myrts::BuildingDef* town_hall = registry.building("alliance_town_hall");
  const myrts::BuildingDef* enemy_barracks = registry.building("alliance_barracks");
  const myrts::BuildingDef* enemy_stables = registry.building("alliance_stables");
  const myrts::BuildingDef* enemy_aviary = registry.building("alliance_aviary");
  if (!peon || !hall || !peasant || !town_hall || !enemy_barracks || !enemy_stables || !enemy_aviary)
    return std::unexpected("game data is missing the starting units or buildings (assets/data)");

  init_tilemap_engine();
  game->map_.reset(tmx_load("tiles/mapa.tmx"));
  if (!game->map_) return std::unexpected(std::string("cannot load map: ") + tmx_strerr());
  const tmx_map* map = game->map_.get();
  game->tile_size_ = static_cast<int>(map->tile_width);

  myrts::WorldConfig config;
  config.tile_size = game->tile_size_;
  // Testing aids: speed up construction and training.
  if (const char* speed = std::getenv("MYRTS_BUILD_SPEED")) config.build_speed = std::max(0.01, std::atof(speed));
  if (const char* speed = std::getenv("MYRTS_TRAIN_SPEED")) config.train_speed = std::max(0.01, std::atof(speed));
  game->world_ = std::make_unique<myrts::World>(config, get_collision_grid(map), get_water_grid(map));
  myrts::World& world = *game->world_;

  game->local_player_ = world.add_player("Player", "orcs", kStartingStock);
  for (const auto& tree : find_tree_tiles(map)) world.add_tree(tree, config.lumber_per_tree);
  world.add_building(*hall, game->local_player_, kPlayerHall, true);
  world.add_mine(kPlayerMine, kMineSize, kGoldPerMine);

  // The Alliance: a computer-controlled opponent with its own base, mine and workers.
  const myrts::PlayerId enemy = world.add_player("Alliance", "alliance", kEnemyStock);
  world.add_building(*town_hall, enemy, kEnemyHall, true);
  world.add_building(*enemy_barracks, enemy, kEnemyBarracks, true);
  world.add_building(*enemy_stables, enemy, kEnemyStables, true);
  world.add_building(*enemy_aviary, enemy, kEnemyAviary, true);
  world.add_mine(kEnemyMine, kMineSize, kGoldPerMine);
  myrts::AiConfig ai_config;
  if (const char* delay = std::getenv("MYRTS_AI_DELAY")) ai_config.start_delay = std::max(0.0, std::atof(delay));
  game->ais_.push_back(std::make_unique<myrts::Ai>(world, registry, enemy, ai_config));

  const auto world_w = static_cast<float>(map->width * map->tile_width);
  const auto world_h = static_cast<float>(map->height * map->tile_height);
  game->camera_ = {0, 0, ui::kViewport.w, ui::kViewport.h, world_w, world_h};
  game->minimap_ = {world_w, world_h, ui::kMinimap.w, ui::kMinimap.h};

  game->art_.load(registry, *ctx.assets);
  if (!game->sprite_of(*peon)) return std::unexpected("cannot load the sprite of " + peon->id);

  // Render the whole map once into a bitmap used by the minimap.
  game->minimap_bitmap_.reset(al_create_bitmap(static_cast<int>(world_w), static_cast<int>(world_h)));
  if (!game->minimap_bitmap_) return std::unexpected("cannot create minimap bitmap");
  game->redraw_minimap_bitmap();

  // Spawn a group of units in distinct walkable tiles near the top-left corner.
  const std::vector<myrts::Point> start(kInitialUnits, kPlayerUnits);
  const auto spots = myrts::assign_destinations(world.grid(myrts::MoveClass::Land), nullptr, start, kPlayerUnits);
  for (const auto& spot : spots)
    if (spot) world.spawn_unit(*peon, game->local_player_, *spot);

  const std::vector<myrts::Point> enemy_start(2, kEnemyUnits);
  const auto enemy_spots = myrts::assign_destinations(world.grid(myrts::MoveClass::Land), nullptr, enemy_start, kEnemyUnits);
  for (const auto& spot : enemy_spots)
    if (spot) world.spawn_unit(*peasant, enemy, *spot);

  myrts::Point focus = kPlayerUnits;
  if (std::getenv("MYRTS_ROSTER")) {  // testing aid: one of every unit type, in the pocket south of the walls
    int n = 0;
    for (const myrts::UnitDef& def : registry.units()) {
      if (def.worker) continue;
      const myrts::Point tile{8 + (n % 8), 10 + (n / 8) * 2};
      world.spawn_unit(def, game->local_player_, tile);
      ++n;
    }
    focus = {12, 11};
  }
  game->camera_.center_on(static_cast<float>(focus.x * game->tile_size_), static_cast<float>(focus.y * game->tile_size_));
  game->subscribe_to_world();
  game->register_commands();
  world.events().dispatch();  // spawn / placement events of the initial setup
  return game;
}

void Game::subscribe_to_world() {
  world_->events().subscribe([this](const myrts::Event& e) {
    myrts::on_event<myrts::ev::UnitDied>(e, [&](const myrts::ev::UnitDied& d) {
      std::erase(selected_, d.unit);
      anims_.erase(d.unit);
      if (const SpriteDef* sprite = sprite_of(*d.def)) {
        const std::string name = std::string("death_") + myrts::facing_name(d.facing);
        if (sprite->animations.contains(name)) {
          if (corpses_.size() >= kMaxCorpses) corpses_.erase(corpses_.begin());
          corpses_.push_back({d.def, d.x, d.y, AnimationState{name, 0, 0.0}, 0.0});
        }
      }
      if (selected_.empty()) close_build_menu();
    });
    myrts::on_event<myrts::ev::BuildingDestroyed>(e, [&](const myrts::ev::BuildingDestroyed& d) {
      if (selected_building_ == d.building) selected_building_ = myrts::kNoEntity;
    });
    myrts::on_event<myrts::ev::TreeFelled>(e, [&](const myrts::ev::TreeFelled& t) {
      replace_tree_tile(map_.get(), t.tile, kGrassGid);
      redraw_minimap_bitmap();
    });
    myrts::on_event<myrts::ev::Notice>(e, [&](const myrts::ev::Notice& n) {
      if (n.player == local_player_) set_message(n.text);
    });
  });
}

/// The behaviour behind each command id. The data (label, icon, hotkey, how it is targeted)
/// lives in assets/data/commands.json; adding a command means a JSON entry plus a line here.
void Game::register_commands() {
  instant_commands_["stop"] = [this] { apply(world_->order_stop(selected_)); };
  targeted_commands_["move"] = [this](myrts::Point t) { apply(world_->order_move(selected_, t)); };
  targeted_commands_["harvest"] = [this](myrts::Point t) { apply(world_->order_harvest(selected_, t)); };
  targeted_commands_["attack"] = [this](myrts::Point t) { apply(world_->order_attack(selected_, t)); };
}

void Game::redraw_minimap_bitmap() {
  ALLEGRO_BITMAP* previous_target = al_get_target_bitmap();
  al_set_target_bitmap(minimap_bitmap_.get());
  al_clear_to_color(al_map_rgb(0, 0, 0));
  render_map(map_.get());
  al_set_target_bitmap(previous_target);
}

const SpriteDef* Game::sprite_of(const myrts::UnitDef& def) {
  const auto it = sprites_.find(&def);
  if (it != sprites_.end()) return it->second.get();
  auto loaded = assets_->sprite(def.sprite);
  if (!loaded) std::fprintf(stderr, "warning: %s\n", loaded.error().c_str());
  const SpriteDef* result = loaded ? loaded->get() : nullptr;
  sprites_[&def] = loaded ? *loaded : nullptr;  // failures are remembered so they warn only once
  return result;
}

void Game::set_message(std::string text, double seconds) {
  message_ = std::move(text);
  message_time_left_ = seconds;
}

void Game::apply(const myrts::Result& result) {
  if (!result.ok && !result.message.empty()) set_message(result.message);
}

std::optional<myrts::Outcome> Game::outcome() const { return myrts::evaluate_outcome(*world_, local_player_); }

// ---- selection & commands ---------------------------------------------------------------

void Game::select_only(std::vector<myrts::EntityId> ids) {
  close_build_menu();
  selected_ = std::move(ids);
  selected_building_ = myrts::kNoEntity;
  awaiting_target_ = nullptr;
}

void Game::select_building(myrts::EntityId id) {
  select_only({});
  selected_building_ = id;
}

void Game::toggle_selected(myrts::EntityId id) {
  const auto it = std::ranges::find(selected_, id);
  if (it == selected_.end()) {
    selected_.push_back(id);
  } else {
    selected_.erase(it);
  }
}

std::vector<const myrts::BuildingDef*> Game::build_page(bool* has_more) const {
  const auto all = current_build_options();
  constexpr std::size_t kFitsWithoutPaging = ui::kButtonSlots - 1;  // everything but the cancel button
  const bool paged = all.size() > kFitsWithoutPaging;
  if (has_more) *has_more = paged;
  if (!paged) return all;
  constexpr std::size_t kPerPage = ui::kButtonSlots - 2;  // minus "next page" and "cancel"
  const std::size_t pages = (all.size() + kPerPage - 1) / kPerPage;
  const std::size_t page = static_cast<std::size_t>(build_page_) % pages;
  const std::size_t begin = page * kPerPage;
  return {all.begin() + static_cast<std::ptrdiff_t>(begin),
          all.begin() + static_cast<std::ptrdiff_t>(std::min(all.size(), begin + kPerPage))};
}

void Game::close_build_menu() {
  build_page_ = 0;
  build_menu_ = false;
  placing_ = nullptr;
}

std::vector<const myrts::UnitDef*> Game::current_train_options() const {
  std::vector<const myrts::UnitDef*> out;
  const myrts::Building* b = world_->buildings().get(selected_building_);
  if (!b || !b->complete) return out;
  for (const auto& id : b->def->trains)
    if (const auto* def = registry_->unit(id)) out.push_back(def);
  return out;
}

std::vector<const myrts::CommandDef*> Game::current_commands() const {
  std::vector<const myrts::CommandDef*> out;
  if (selected_.empty()) return out;
  const myrts::Unit* first = world_->units().get(selected_.front());
  if (!first) return out;
  for (const auto& id : first->def->commands)
    if (const auto* c = registry_->command(id)) out.push_back(c);
  return out;
}

std::vector<const myrts::BuildingDef*> Game::current_build_options() const {
  std::vector<const myrts::BuildingDef*> out;
  for (const auto id : selected_) {
    const myrts::Unit* u = world_->units().get(id);
    if (!u || u->locked() || u->def->builds.empty()) continue;
    for (const auto& b : u->def->builds)
      if (const auto* def = registry_->building(b)) out.push_back(def);
    break;  // the first free builder decides the menu
  }
  return out;
}

void Game::activate_command(const myrts::CommandDef& command) {
  awaiting_target_ = nullptr;
  if (!command.implemented) {
    set_message(command.label + " is not implemented yet");
    return;
  }
  switch (command.targeting) {
    case myrts::Targeting::None: {
      const auto it = instant_commands_.find(command.id);
      if (it != instant_commands_.end()) {
        it->second();
        return;
      }
      break;
    }
    case myrts::Targeting::Ground:
    case myrts::Targeting::Resource:
      if (targeted_commands_.contains(command.id)) {
        awaiting_target_ = &command;
        set_message(command.targeting == myrts::Targeting::Ground ? "Select destination"
                                                                  : "Select a tree or a gold mine",
                    10.0);
        return;
      }
      break;
    case myrts::Targeting::Menu:
      if (!current_build_options().empty()) {
        build_menu_ = true;
        set_message("Choose a building", 10.0);
        return;
      }
      break;
  }
  set_message(command.label + " is not implemented yet");
}

myrts::Point Game::placement_origin(const myrts::BuildingDef& def, myrts::Point cursor) const {
  return {cursor.x - def.size / 2, cursor.y - def.size / 2};
}

void Game::start_placing(const myrts::BuildingDef& def) {
  if (!myrts::can_afford(world_->player(local_player_).stock, def.cost)) {
    set_message("Not enough resources for a " + def.name);
    return;
  }
  placing_ = &def;
  set_message("Place the " + def.name + " (right click cancels)", 10.0);
}

void Game::try_place_building(myrts::Point origin) {
  if (!placing_ || selected_.empty()) return;
  const myrts::BuildingDef& def = *placing_;
  const auto result = world_->order_build(local_player_, selected_, def, origin);
  if (world_->building_at(origin).valid()) {  // it was placed (and paid for), even if the walk failed
    placing_ = nullptr;
    build_menu_ = false;
    message_.clear();
  }
  apply(result);
}

// ---- simulation --------------------------------------------------------------------------

/// Chooses the animation for what a unit is doing. Sprites that lack a variant (say, no
/// carrying frames) fall back to the plain walk/idle one.
std::string Game::unit_animation(const myrts::Unit& u, const SpriteDef& sprite) const {
  const std::string dir = myrts::facing_name(u.facing);
  const std::string base = u.moving ? "walk_" : "idle_";
  if (u.attacking() && !u.moving) return pick_animation(sprite, {"attack_" + dir, "chop_" + dir, base + dir});
  if (u.working() && !u.moving) return pick_animation(sprite, {"chop_" + dir, base + dir});
  if (u.carrying > 0) {
    const std::string carried = base + (u.cargo == myrts::Cargo::Gold ? "gold_" : "wood_") + dir;
    return pick_animation(sprite, {carried, base + dir});
  }
  return pick_animation(sprite, {base + dir});
}

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

  for (auto& ai : ais_) ai->update(dt);
  world_->update(dt);

  world_->units().for_each([&](const myrts::Unit& u) {
    const SpriteDef* sprite = sprite_of(*u.def);
    if (!sprite) return;
    AnimationState& anim = anims_[u.id];
    set_animation(anim, unit_animation(u, *sprite));
    update_animation(*sprite, anim, dt);
  });

  for (Corpse& c : corpses_) {
    c.age += dt;
    if (const SpriteDef* sprite = sprite_of(*c.def)) update_animation(*sprite, c.anim, dt);
  }
  std::erase_if(corpses_, [](const Corpse& c) { return c.age > kCorpseSeconds; });

  if (message_time_left_ > 0 && (message_time_left_ -= dt) <= 0 && !awaiting_target_) message_.clear();
}
