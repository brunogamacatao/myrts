#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <allegro5/allegro_primitives.h>
#include <nlohmann/json.hpp>

#include "core/match_setup.hpp"

namespace {
constexpr float kScrollSpeed = 420.0f;  // pixels per second
constexpr float kEdgeScrollMargin = 6.0f;
constexpr double kCorpseSeconds = 25.0;  // how long a body stays (the last 5 s it fades)
constexpr std::size_t kMaxCorpses = 60;
constexpr const char* kDefaultMap = "maps/Gold Rush BNE.pud";

}  // namespace

// ---- setup ------------------------------------------------------------------------

std::expected<std::unique_ptr<Game>, std::string> Game::create(const SceneContext& ctx, std::size_t level_index,
                                                                const std::string& faction, const std::string* saved_state) {
  const myrts::Level fallback = myrts::Campaign::single(kDefaultMap).level(0);
  const myrts::Level level = myrts::play_as(ctx.campaign ? ctx.campaign->level(level_index) : fallback, *ctx.registry, faction);
  std::unique_ptr<Game> game(new Game());
  game->font_ = ctx.font;
  game->cursors_ = ctx.cursors;
  game->assets_ = ctx.assets;
  game->registry_ = ctx.registry;
  game->sounds_ = ctx.sounds ? ctx.sounds : std::make_shared<const myrts::SoundTable>();
  game->director_ = std::make_unique<SoundDirector>(ctx.audio, game->sounds_.get());
  const myrts::Registry& registry = *ctx.registry;

  // Everything that is a rule of the setup (map, sides, halls, workers, the opponent) is built by the core.
  myrts::Level setup_level = level;
  if (const char* delay = std::getenv("MYRTS_AI_DELAY")) setup_level.ai.start_delay = std::max(0.0, std::atof(delay));
  myrts::MatchOptions options;
  options.world.tile_size = game->tile_size_;
  if (std::getenv("MYRTS_NOFOG")) options.world.fog = false;  // testing aid: see the whole map
  // Testing aids: speed up construction and training.
  if (const char* speed = std::getenv("MYRTS_BUILD_SPEED")) options.world.build_speed = std::max(0.01, std::atof(speed));
  if (const char* speed = std::getenv("MYRTS_TRAIN_SPEED")) options.world.train_speed = std::max(0.01, std::atof(speed));
  auto match = myrts::build_match(registry, ctx.tilesets, setup_level, options);
  if (!match) return std::unexpected(match.error());
  game->world_ = std::move(match->world);
  game->ais_ = std::move(match->ais);
  game->local_player_ = match->local;
  game->player_faction_ = match->local_faction->id;
  myrts::World& world = *game->world_;
  std::optional<myrts::Point> saved_camera;
  if (saved_state) {  // a saved game: the same level is built, then the saved board replaces the fresh one
    try {
      const nlohmann::json saved = nlohmann::json::parse(*saved_state);
      if (auto loaded = world.load_state(saved.at("world"), registry); !loaded) return std::unexpected(loaded.error());
      for (std::size_t i = 0; i < game->ais_.size() && saved.contains("ais") && i < saved.at("ais").size(); ++i)
        game->ais_[i]->load_state(saved.at("ais")[i]);
      if (saved.contains("camera")) saved_camera = myrts::Point{saved.at("camera").at(0).get<int>(), saved.at("camera").at(1).get<int>()};
    } catch (const std::exception& e) {
      return std::unexpected(std::string("invalid saved game: ") + e.what());
    }
    world.events().dispatch();  // nobody is listening yet: the restored buildings do not announce themselves
  }
  const myrts::PlayerId enemy = match->enemy;
  const myrts::Point our_hall = match->local_hall;
  const myrts::BuildingDef* hall = match->local_hall_def;

  if (auto drawn = game->terrain_view_.load(*world.terrain(), *ctx.assets); !drawn) return std::unexpected(drawn.error());
  const auto world_w = static_cast<float>(world.width() * game->tile_size_);
  const auto world_h = static_cast<float>(world.height() * game->tile_size_);
  game->camera_ = {0, 0, ui::kViewport.w, ui::kViewport.h, world_w, world_h};
  game->minimap_ = {world_w, world_h, ui::kMinimap.w, ui::kMinimap.h};

  game->art_.load(registry, *ctx.assets, world.terrain()->tileset().name());
  if (auto loaded = game->ui_art_.load(*ctx.assets); !loaded) return std::unexpected(loaded.error());
  if (const myrts::UnitDef* worker = registry.unit(match->local_faction->worker); !worker || !game->sprite_of(*worker))
    return std::unexpected("cannot load the sprite of " + match->local_faction->worker);

  myrts::Point focus{our_hall.x + hall->size / 2, our_hall.y + hall->size / 2};
  if (std::getenv("MYRTS_ROSTER")) {  // testing aid: one of every unit type, next to our hall
    int n = 0;
    for (const myrts::UnitDef& def : registry.units()) {
      if (def.worker) continue;
      const myrts::Point tile{our_hall.x + (n % 8), our_hall.y - 3 - (n / 8) * 2};
      world.spawn_unit(def, game->local_player_, tile);
      ++n;
    }
  }
  if (std::getenv("MYRTS_NAVY")) {  // testing aid: a transport and a destroyer at the nearest shore (their tiles are printed)
    const myrts::Grid& sea = world.grid(myrts::MoveClass::Water);
    const myrts::Grid& land = world.grid(myrts::MoveClass::Land);
    int spawned = 0;
    for (int r = 2; r < 40 && spawned < 2; ++r)
      for (int dy = -r; dy <= r && spawned < 2; ++dy)
        for (int dx = -r; dx <= r && spawned < 2; ++dx) {
          const myrts::Point p{our_hall.x + dx, our_hall.y + dy};
          if (std::max(std::abs(dx), std::abs(dy)) != r || !sea.is_walkable(p) || !land.is_blocked(p)) continue;
          bool shore = false;
          for (int ey = -1; ey <= 1; ++ey)
            for (int ex = -1; ex <= 1; ++ex) shore = shore || land.is_walkable({p.x + ex, p.y + ey});
          if (!shore) continue;
          const char* id = spawned == 0 ? match->local_faction->id == "orcs" ? "orc_transport" : "alliance_transport"
                                        : match->local_faction->id == "orcs" ? "orc_destroyer" : "alliance_destroyer";
          if (const myrts::UnitDef* def = registry.unit(id); def && world.spawn_unit(*def, game->local_player_, p).valid()) {
            std::fprintf(stderr, "NAVY %s at %d,%d (hall %d,%d)\n", id, p.x, p.y, our_hall.x, our_hall.y);
            ++spawned;
          }
        }
  }
  if (std::getenv("MYRTS_ROSTER")) {  // ...and a few enemy soldiers close by, so the ranged units have targets
    for (const myrts::UnitDef& def : registry.units()) {
      if (def.faction != match->enemy_faction->id || def.worker || !def.can_attack() || def.move_class != myrts::MoveClass::Land) continue;
      for (int i = 0; i < 6; ++i) world.spawn_unit(def, enemy, {our_hall.x + 3 + i, our_hall.y - 12});
      break;
    }
  }
  if (saved_camera) focus = *saved_camera;
  game->camera_.center_on(static_cast<float>(focus.x * game->tile_size_), static_cast<float>(focus.y * game->tile_size_));
  game->subscribe_to_world();
  game->allow_ = level.allow;
  if (saved_state) {
    game->set_message("Game loaded", 4.0);
  } else {
    game->hints_.assign(level.hints.rbegin(), level.hints.rend());  // popped from the back, so shown in order
    game->hint_timer_ = 6.0;
    game->set_message(ctx.campaign && ctx.campaign->size() > 1
                        ? (level.chapter.empty() ? "" : level.chapter + " - ") + "Level " + std::to_string(level_index + 1) + ": " + level.name + ". " + level.briefing
                        : level.briefing,
                    6.0);
  }
  game->register_commands();
  world.refresh_fog();
  world.events().dispatch();  // spawn / placement events of the initial setup
  return game;
}

void Game::subscribe_to_world() {
  world_->events().subscribe([this](const myrts::Event& e) {
    play_world_sound(e);
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
    myrts::on_event<myrts::ev::Attacked>(e, [&](const myrts::ev::Attacked& a) { spawn_projectile(a); });
    myrts::on_event<myrts::ev::UnitBoarded>(e, [&](const myrts::ev::UnitBoarded& b) {
      std::erase(selected_, b.unit);  // aboard: no longer on the map to be commanded
      if (selected_.empty()) close_build_menu();
    });
    myrts::on_event<myrts::ev::BuildingDestroyed>(e, [&](const myrts::ev::BuildingDestroyed& d) {
      if (selected_building_ == d.building) selected_building_ = myrts::kNoEntity;
    });
    myrts::on_event<myrts::ev::TerrainChanged>(e, [&](const myrts::ev::TerrainChanged& c) {
      for (const myrts::Point p : c.tiles) terrain_view_.update_minimap(*world_->terrain(), p);  // the view reads the tiles itself
    });
    myrts::on_event<myrts::ev::Notice>(e, [&](const myrts::ev::Notice& n) {
      if (n.player == local_player_) set_message(n.text);
    });
  });
}

/// A ranged strike sends a visible missile; the damage itself was already dealt by the World.
void Game::spawn_projectile(const myrts::ev::Attacked& a) {
  const myrts::Unit* attacker = world_->units().get(a.attacker);
  if (!attacker || attacker->def->projectile.empty() || attacker->def->stats.range <= 1) return;
  if (!sees_point(a.from_x, a.from_y) && !sees_point(a.to_x, a.to_y)) return;  // fought out of sight
  using K = Projectile::Kind;
  static const std::unordered_map<std::string, std::pair<K, float>> kKinds = {  // kind and speed (pixels per second)
      {"arrow", {K::Arrow, 460}}, {"bolt", {K::Bolt, 520}},  {"axe", {K::Axe, 320}}, {"boulder", {K::Boulder, 280}},
      {"fire", {K::Fire, 340}},   {"magic", {K::Magic, 400}}, {"coil", {K::Coil, 360}}};
  const auto it = kKinds.find(attacker->def->projectile);
  if (it == kKinds.end()) return;
  Projectile p;
  p.kind = it->second.first;
  const float lift = lift_of(*attacker->def);
  p.x0 = a.from_x;
  p.y0 = a.from_y - lift - 6;
  p.x1 = a.to_x;
  p.y1 = a.to_y;
  const float dist = std::hypot(p.x1 - p.x0, p.y1 - p.y0);
  p.duration = std::clamp(static_cast<double>(dist / it->second.second), 0.12, 1.2);
  if (projectiles_.size() < 200) projectiles_.push_back(p);
}

/// The behaviour behind each command id. The data (label, icon, hotkey, how it is targeted)
/// lives in assets/data/commands.json; adding a command means a JSON entry plus a line here.
void Game::register_commands() {
  instant_commands_["stop"] = [this] { ordered(world_->order_stop(selected_)); };
  targeted_commands_["move"] = [this](myrts::Point t) { ordered(world_->order_move(selected_, t)); };
  targeted_commands_["harvest"] = [this](myrts::Point t) { ordered(world_->order_harvest(selected_, t)); };
  targeted_commands_["unload"] = [this](myrts::Point t) { ordered(world_->order_unload(selected_, t)); };
  targeted_commands_["repair"] = [this](myrts::Point t) { ordered(world_->order_repair(selected_, t)); };
  targeted_commands_["attack"] = [this](myrts::Point t) { ordered(world_->order_attack(selected_, t)); };
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
  if (!result.ok && !result.message.empty()) {
    set_message(result.message);
    director_->effect("error");
  }
}

void Game::ordered(const myrts::Result& result) {
  apply(result);
  if (!result.ok) return;
  for (const auto id : selected_)
    if (const myrts::Unit* u = world_->units().get(id)) {
      director_->unit(*u->def, myrts::SoundEvent::Acknowledge);
      return;
    }
}

/// Selecting a unit makes it speak; clicking the same one again and again makes it lose patience.
void Game::play_selection_sound(bool same_unit_again) {
  if (selected_.empty()) return;
  const myrts::Unit* u = world_->units().get(selected_.front());
  if (!u) return;
  if (same_unit_again) {
    director_->annoyed(*u->def, repeat_clicks_++);
  } else {
    repeat_clicks_ = 0;
    director_->unit(*u->def, myrts::SoundEvent::Selected);
  }
}

/// The sounds of what happens in the match, as the World reports it (the orders and clicks play theirs directly).
void Game::play_world_sound(const myrts::Event& e) {
  using myrts::SoundEvent;
  const float tile = static_cast<float>(tile_size_);
  const auto centre = [&](myrts::Point p, int size) {
    return myrts::Vec2{(static_cast<float>(p.x) + static_cast<float>(size) / 2) * tile,
                       (static_cast<float>(p.y) + static_cast<float>(size) / 2) * tile};
  };
  myrts::on_event<myrts::ev::UnitDied>(e, [&](const myrts::ev::UnitDied& d) {
    if (d.owner != local_player_ && !sees_point(d.x, d.y)) return;  // out of sight, out of earshot
    director_->unit(*d.def, SoundEvent::Dead, myrts::Vec2{d.x, d.y});
  });
  myrts::on_event<myrts::ev::Attacked>(e, [&](const myrts::ev::Attacked& a) {
    const myrts::Unit* attacker = world_->units().get(a.attacker);
    if (attacker && sees(*attacker)) director_->unit(*attacker->def, SoundEvent::Attack, myrts::Vec2{attacker->x, attacker->y});
    const myrts::Unit* target = a.target_is_building ? nullptr : world_->units().get(a.target);
    if (target && target->owner == local_player_)  // one of ours is being hit
      director_->unit(*target->def, SoundEvent::Help, myrts::Vec2{target->x, target->y});
  });
  myrts::on_event<myrts::ev::UnitTrained>(e, [&](const myrts::ev::UnitTrained& t) {
    const myrts::Unit* u = world_->units().get(t.unit);
    if (u && t.owner == local_player_) director_->unit(*u->def, SoundEvent::Ready);
  });
  myrts::on_event<myrts::ev::BuildingPlaced>(e, [&](const myrts::ev::BuildingPlaced& p) {
    if (p.owner == local_player_) director_->effect("construct");
  });
  myrts::on_event<myrts::ev::BuildingCompleted>(e, [&](const myrts::ev::BuildingCompleted& c) {
    const myrts::Building* b = world_->buildings().get(c.building);
    if (!b || c.owner != local_player_) return;
    for (const myrts::UnitDef& def : registry_->units())  // the builders of that side announce it
      if (def.faction == b->def->faction && def.worker) return director_->unit(def, SoundEvent::WorkDone);
  });
  myrts::on_event<myrts::ev::BuildingDestroyed>(e, [&](const myrts::ev::BuildingDestroyed& d) {
    director_->effect("building_destroyed", centre(d.origin, d.size));
  });
  myrts::on_event<myrts::ev::TreeFelled>(e, [&](const myrts::ev::TreeFelled& t) {
    director_->effect("tree", centre(t.tile, 1));
  });
}

std::string Game::selected_faction() const {
  for (const auto id : selected_)
    if (const myrts::Unit* u = world_->units().get(id)) return u->def->faction;
  return player_faction_;
}

/// The cursor tells what a click would do: scroll arrows at the edge, crosshairs while choosing a target (red over an
/// enemy, green over something to work on) and the normal pointer over the HUD.
void Game::update_cursor(float edge_dx, float edge_dy) {
  if (!cursors_) return;
  using K = CursorKind;
  if (edge_dx != 0 || edge_dy != 0) {
    static constexpr K kArrows[3][3] = {{K::ScrollNW, K::ScrollN, K::ScrollNE},
                                        {K::ScrollW, K::Pointer, K::ScrollE},
                                        {K::ScrollSW, K::ScrollS, K::ScrollSE}};
    cursors_->set(kArrows[static_cast<int>(edge_dy) + 1][static_cast<int>(edge_dx) + 1]);
    return;
  }
  if (!mouse_in_window_ || !ui::kViewport.contains(mouse_x_, mouse_y_)) return;  // over the HUD: the pointer

  if (placing_) return cursors_->set(K::Target);
  if (awaiting_target_) return cursors_->set(awaiting_target_->id == "attack" ? K::TargetEnemy : K::TargetOk);
  if (selected_.empty()) return;

  const auto world = camera_.to_world(mouse_x_ - ui::kViewport.x, mouse_y_ - ui::kViewport.y);
  const auto tile = tile_at_world(world);
  if (!tile) return;
  const myrts::EntityId unit = world_->unit_at(*tile);
  const myrts::Unit* foe_unit = unit.valid() ? world_->units().get(unit) : nullptr;
  if (foe_unit && !sees(*foe_unit)) foe_unit = nullptr;
  const myrts::EntityId site = world_->building_at(*tile);
  const myrts::Building* foe_building = site.valid() ? world_->buildings().get(site) : nullptr;
  if (foe_building && !sees(*foe_building)) foe_building = nullptr;
  if ((foe_unit && myrts::World::is_enemy(local_player_, foe_unit->owner)) ||
      (foe_building && myrts::World::is_enemy(local_player_, foe_building->owner)))
    return cursors_->set(K::TargetEnemy);
  if (world_->trees().has_tree(*tile) || world_->mine_at(*tile) >= 0) cursors_->set(K::TargetOk);
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

bool Game::allowed(const std::string& id) const {
  return allow_.empty() || std::ranges::find(allow_, id) != allow_.end();
}

std::vector<const myrts::UnitDef*> Game::current_train_options() const {
  std::vector<const myrts::UnitDef*> out;
  const myrts::Building* b = world_->buildings().get(selected_building_);
  if (!b || !b->complete) return out;
  for (const auto& id : b->def->trains)
    if (const auto* def = registry_->unit(id); def && allowed(id)) out.push_back(def);
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
      if (const auto* def = registry_->building(b); def && allowed(b)) out.push_back(def);
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
    const std::string carried = base + (u.cargo == myrts::Cargo::Gold ? "gold_" : u.cargo == myrts::Cargo::Oil ? "oil_" : "wood_") + dir;
    return pick_animation(sprite, {carried, base + dir});
  }
  return pick_animation(sprite, {base + dir});
}

void Game::update(double dt) {
  director_->update(dt, camera_.visible());
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
  float edge_dx = 0, edge_dy = 0;  // scrolling caused by the mouse at the screen edge (it gets an arrow cursor)
  if (mouse_in_window_ && !dragging_minimap_ && mouse_x_ >= ui::kPanelW) {
    if (mouse_x_ < ui::kViewport.x + kEdgeScrollMargin) edge_dx -= 1;
    if (mouse_x_ >= ui::kWidth - kEdgeScrollMargin) edge_dx += 1;
    if (mouse_y_ < ui::kViewport.y + kEdgeScrollMargin) edge_dy -= 1;
    if (mouse_y_ >= ui::kHeight - kEdgeScrollMargin) edge_dy += 1;
  }
  if (dx != 0 || dy != 0) {
    const auto step = kScrollSpeed * static_cast<float>(dt);
    camera_.move(dx * step, dy * step);
  }
  update_cursor(edge_dx, edge_dy);

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
  if (!hints_.empty() && (hint_timer_ -= dt) <= 0 && !awaiting_target_ && !placing_) {  // the tutorial lines, in turn
    set_message(hints_.back(), 9.0);
    hints_.pop_back();
    hint_timer_ = 11.0;
  }
  for (Projectile& p : projectiles_) p.age += dt;
  std::erase_if(projectiles_, [](const Projectile& p) { return p.age >= p.duration; });

  if (message_time_left_ > 0 && (message_time_left_ -= dt) <= 0 && !awaiting_target_) message_.clear();
}

// ---- saved games ---------------------------------------------------------------------

myrts::SaveSlot Game::snapshot() {
  using json = nlohmann::json;
  json ais = json::array();
  for (const auto& ai : ais_) ais.push_back(ai->save_state());
  const myrts::Rect view = camera_.visible();
  json state = {{"world", world_->save_state()},
                {"ais", std::move(ais)},
                {"camera", json::array({static_cast<int>((view.x + view.w / 2) / static_cast<float>(tile_size_)),
                                        static_cast<int>((view.y + view.h / 2) / static_cast<float>(tile_size_))})}};
  myrts::SaveSlot slot;
  slot.state = state.dump();

  // The picture: the minimap with what is still unexplored in black, and a dot for every unit and building in sight.
  constexpr int kSize = myrts::SaveFile::kThumbSize;
  ALLEGRO_BITMAP* previous = al_get_target_bitmap();
  const int flags = al_get_new_bitmap_flags();
  al_set_new_bitmap_flags(flags & ~ALLEGRO_MEMORY_BITMAP);
  BitmapPtr canvas(al_create_bitmap(kSize, kSize));
  al_set_new_bitmap_flags(flags);
  if (canvas && terrain_view_.minimap()) {
    al_set_target_bitmap(canvas.get());
    al_clear_to_color(al_map_rgb(0, 0, 0));
    ALLEGRO_BITMAP* mini = terrain_view_.minimap();
    al_draw_scaled_bitmap(mini, 0, 0, static_cast<float>(al_get_bitmap_width(mini)), static_cast<float>(al_get_bitmap_height(mini)),
                          0, 0, kSize, kSize, 0);
    if (ALLEGRO_BITMAP* fog = fog_view_.black())
      al_draw_scaled_bitmap(fog, 0, 0, static_cast<float>(al_get_bitmap_width(fog)), static_cast<float>(al_get_bitmap_height(fog)),
                            0, 0, kSize, kSize, 0);
    const float sx = static_cast<float>(kSize) / (static_cast<float>(world_->width()) * static_cast<float>(tile_size_));
    const float sy = static_cast<float>(kSize) / (static_cast<float>(world_->height()) * static_cast<float>(tile_size_));
    const auto dot = [&](float x, float y, float size, bool enemy) {
      al_draw_filled_rectangle(x * sx - size / 2, y * sy - size / 2, x * sx + size / 2, y * sy + size / 2,
                               enemy ? al_map_rgb(230, 60, 50) : al_map_rgb(240, 220, 60));
    };
    world_->buildings().for_each([&](const myrts::Building& b) {
      if (!sees(b)) return;
      const float half = static_cast<float>(b.def->size * tile_size_) / 2;
      dot(static_cast<float>(b.origin.x * tile_size_) + half, static_cast<float>(b.origin.y * tile_size_) + half, 3.0f,
          b.owner != local_player_);
    });
    world_->units().for_each([&](const myrts::Unit& u) {
      if (!u.hidden && sees(u)) dot(u.x, u.y, 2.0f, u.owner != local_player_);
    });
    std::vector<std::uint8_t> rgb;
    rgb.reserve(static_cast<std::size_t>(kSize * kSize * 3));
    if (al_lock_bitmap(canvas.get(), ALLEGRO_PIXEL_FORMAT_ANY, ALLEGRO_LOCK_READONLY)) {
      for (int y = 0; y < kSize; ++y)
        for (int x = 0; x < kSize; ++x) {
          unsigned char r, g, b;
          al_unmap_rgb(al_get_pixel(canvas.get(), x, y), &r, &g, &b);
          rgb.insert(rgb.end(), {r, g, b});
        }
      al_unlock_bitmap(canvas.get());
      slot.thumbnail = myrts::base64_encode(rgb);
    }
  }
  al_set_target_bitmap(previous);
  return slot;
}
