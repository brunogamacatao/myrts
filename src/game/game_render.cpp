#include <allegro5/allegro_primitives.h>

#include <algorithm>
#include <cmath>

#include "game.hpp"
#include "tilemap.hpp"

namespace {
const ALLEGRO_COLOR kSelectGreen = al_map_rgb(60, 230, 60);
}

void Game::draw_health_bar(float x, float y, float w, int hp, int max_hp) {
  const float frac = std::clamp(static_cast<float>(hp) / static_cast<float>(max_hp), 0.0f, 1.0f);
  al_draw_filled_rectangle(x, y, x + w, y + 3, al_map_rgb(20, 18, 16));
  al_draw_filled_rectangle(x + 0.5f, y + 0.5f, x + 0.5f + (w - 1) * frac, y + 2.5f,
                           frac > 0.5f ? al_map_rgb(60, 220, 60) : (frac > 0.25f ? al_map_rgb(240, 200, 40) : al_map_rgb(220, 50, 40)));
}

void Game::draw_placement_ghost() {
  const myrts::BuildingDef& def = *placing_;
  const auto w = camera_.to_world(mouse_x_ - ui::kViewport.x, mouse_y_ - ui::kViewport.y);
  const auto tile = tile_at_world(w);
  if (!tile) return;

  const auto origin = placement_origin(def, *tile);
  const bool ok = world_->placement_status(def, origin) == myrts::PlaceStatus::Ok;
  const auto tile_px = static_cast<float>(tile_size_);
  const float x = static_cast<float>(origin.x) * tile_px;
  const float y = static_cast<float>(origin.y) * tile_px;
  const float side = static_cast<float>(def.size) * tile_px;
  al_draw_filled_rectangle(x, y, x + side, y + side, ok ? al_map_rgba(40, 200, 40, 70) : al_map_rgba(220, 40, 40, 90));
  draw_centered_in_square(art_.finished(def), x, y, side, al_map_rgba_f(0.6f, 0.6f, 0.6f, 0.6f));
  al_draw_rectangle(x + 0.5f, y + 0.5f, x + side - 0.5f, y + side - 0.5f,
                    ok ? al_map_rgb(60, 230, 60) : al_map_rgb(230, 60, 60), 2);
}

void Game::draw_world() {
  render_map(map_.get());

  const auto tile_px = static_cast<float>(tile_size_);
  for (const auto& mine : world_->mines()) {
    if (mine.site.depleted()) continue;
    const auto o = mine.site.origin();
    draw_centered_in_square(art_.gold_mine(mine.busy), static_cast<float>(o.x) * tile_px,
                            static_cast<float>(o.y) * tile_px, static_cast<float>(mine.site.size()) * tile_px);
  }

  world_->buildings().for_each([&](const myrts::Building& b) {
    const float x = static_cast<float>(b.origin.x) * tile_px;
    const float y = static_cast<float>(b.origin.y) * tile_px;
    const float side = static_cast<float>(b.def->size) * tile_px;
    draw_centered_in_square(b.complete ? art_.finished(*b.def) : art_.construction(*b.def), x, y, side);
    if (b.hp < b.def->hp || b.id == selected_building_) draw_health_bar(x, y - 14, side, b.hp, b.def->hp);
    if (!b.complete) {  // progress bar above the site
      const float frac = static_cast<float>(b.progress / b.def->build_seconds);
      al_draw_filled_rectangle(x, y - 8, x + side, y - 3, al_map_rgb(20, 18, 16));
      al_draw_filled_rectangle(x + 1, y - 7, x + 1 + (side - 2) * frac, y - 4, al_map_rgb(60, 220, 60));
    }
  });

  // Units sorted by y so lower ones overlap higher ones.
  std::vector<const myrts::Unit*> order;
  order.reserve(world_->units().size());
  world_->units().for_each([&](const myrts::Unit& u) {
    if (!u.hidden) order.push_back(&u);
  });
  std::ranges::sort(order, [](const myrts::Unit* a, const myrts::Unit* b) { return a->y < b->y; });
  for (const myrts::Unit* u : order) {
    const auto sprite = sprites_.find(u->def);
    const auto anim = anims_.find(u->id);
    if (sprite != sprites_.end() && sprite->second && anim != anims_.end())
      draw_sprite_centered(*sprite->second, anim->second, u->x, u->y);
    const bool selected = std::ranges::find(selected_, u->id) != selected_.end();
    if (selected) {
      const auto b = unit_box(*u);
      al_draw_rectangle(b.x + 2, b.y + 2, b.x + b.w - 2, b.y + b.h - 2, kSelectGreen, 1.5f);
    }
    if (selected || u->hp < u->def->max_hp) draw_health_bar(u->x - 13, u->y - 21, 26, u->hp, u->def->max_hp);
  }

  if (placing_ && mouse_in_window_ && ui::kViewport.contains(mouse_x_, mouse_y_)) draw_placement_ghost();
}

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

  draw_world();

  ALLEGRO_TRANSFORM identity;
  al_identity_transform(&identity);
  al_use_transform(&identity);

  if (box_selecting_) {  // drag rectangle, still clipped to the viewport
    al_draw_rectangle(press_x_ + 0.5f, press_y_ + 0.5f, mouse_x_ + 0.5f, mouse_y_ + 0.5f, kSelectGreen, 1);
  }
  al_set_clipping_rectangle(0, 0, ui::kWidth, ui::kHeight);

  // View-models for the HUD.
  static const AnimationState kNoAnimation;
  std::vector<ui::UnitCard> cards;
  for (const auto id : selected_) {
    const myrts::Unit* u = world_->units().get(id);
    if (!u) continue;
    const auto sprite = sprites_.find(u->def);
    const auto anim = anims_.find(id);
    cards.push_back({u->def->name, u->def->level, u->hp, u->def->max_hp, u->def->stats,
                     sprite != sprites_.end() ? sprite->second.get() : nullptr,
                     anim != anims_.end() ? &anim->second : &kNoAnimation});
  }
  std::vector<ui::MinimapDot> dots;
  world_->buildings().for_each([&](const myrts::Building& b) {
    const float half = static_cast<float>(b.def->size * tile_size_) / 2;
    dots.push_back({static_cast<float>(b.origin.x * tile_size_) + half, static_cast<float>(b.origin.y * tile_size_) + half,
                    b.id == selected_building_, b.owner != local_player_, 4});
  });
  world_->units().for_each([&](const myrts::Unit& u) {
    if (u.hidden) return;
    dots.push_back({u.x, u.y, std::ranges::find(selected_, u.id) != selected_.end(), u.owner != local_player_, 2});
  });

  // A selected building: its info card and what it can train.
  std::optional<ui::BuildingCard> building_card;
  std::vector<ui::TrainOption> train_options;
  if (const myrts::Building* b = world_->buildings().get(selected_building_)) {
    static AnimationState icon_anim;  // every icon shows the unit standing, facing the viewer
    icon_anim.current = "idle_down";
    ui::BuildingCard card;
    card.name = b->def->name;
    card.hp = b->hp;
    card.max_hp = b->def->hp;
    card.complete = b->complete;
    card.build_progress = static_cast<float>(b->progress / b->def->build_seconds);
    for (const myrts::TrainItem& q : b->queue) {
      const auto sprite = sprites_.find(q.unit);
      card.queue.push_back({q.unit->name, static_cast<float>(q.progress / q.unit->train_seconds),
                            sprite != sprites_.end() ? sprite->second.get() : nullptr, &icon_anim});
    }
    building_card = std::move(card);
    const myrts::Stock& have = world_->player(local_player_).stock;
    for (const myrts::UnitDef* def : current_train_options())
      train_options.push_back({def, sprite_of(*def), &icon_anim, myrts::can_afford(have, def->cost)});
  }
  std::vector<ui::CommandButton> commands;
  for (const myrts::CommandDef* c : current_commands()) commands.push_back({c, c == awaiting_target_});
  std::vector<ui::BuildOption> options;
  const myrts::Stock& stock = world_->player(local_player_).stock;
  for (const myrts::BuildingDef* def : current_build_options())
    options.push_back({def, art_.finished(*def), myrts::can_afford(stock, def->cost), def == placing_});

  const ui::HudData hud{stock,     cards,     dots,       minimap_bitmap_.get(), minimap_, camera_,
                        message_,  mouse_x_,  mouse_y_,   commands,              build_menu_, options,
                        building_card ? &*building_card : nullptr, train_options};
  ui::draw_hud(font_, hud);
}
