#include <allegro5/allegro_primitives.h>

#include <algorithm>
#include <cmath>

#include "game.hpp"

namespace {
const ALLEGRO_COLOR kSelectGreen = al_map_rgb(60, 230, 60);
constexpr double kCorpseSeconds = 25.0;  // keep in step with game.cpp
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
  fog_view_.update(fog());
  terrain_view_.draw(*world_->terrain(), camera_.visible());

  const auto tile_px = static_cast<float>(tile_size_);
  for (const auto& mine : world_->mines()) {
    if (mine.site.depleted()) continue;
    const auto o = mine.site.origin();
    draw_centered_in_square(mine.kind() == myrts::ResourceKind::Oil ? art_.oil_patch() : art_.gold_mine(mine.busy), static_cast<float>(o.x) * tile_px,
                            static_cast<float>(o.y) * tile_px, static_cast<float>(mine.site.size()) * tile_px);
  }

  world_->buildings().for_each([&](const myrts::Building& b) {
    if (!sees(b)) return;
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

  // Bodies lie under everything that is alive.
  for (const Corpse& c : corpses_) {
    if (!fog().explored({static_cast<int>(c.x) / tile_size_, static_cast<int>(c.y) / tile_size_})) continue;
    const auto sprite = sprites_.find(c.def);
    if (sprite == sprites_.end() || !sprite->second) continue;
    const float fade = std::clamp(static_cast<float>((kCorpseSeconds - c.age) / 5.0), 0.0f, 1.0f);
    draw_sprite_centered(*sprite->second, c.anim, c.x, c.y - lift_of(*c.def), fade);
  }

  // The rally point of the selected building: a small flag.
  if (const myrts::Building* b = world_->buildings().get(selected_building_); b && b->rally) {
    const float fx = static_cast<float>(b->rally->x * tile_size_ + tile_size_ / 2);
    const float fy = static_cast<float>(b->rally->y * tile_size_ + tile_size_ / 2);
    al_draw_line(fx, fy + 8, fx, fy - 12, al_map_rgb(240, 240, 240), 2);
    al_draw_filled_triangle(fx, fy - 12, fx + 12, fy - 7, fx, fy - 2, al_map_rgb(60, 220, 60));
  }

  // Units sorted by y so lower ones overlap higher ones; flyers go on top of everything on the ground.
  std::vector<const myrts::Unit*> ground, air;
  world_->units().for_each([&](const myrts::Unit& u) {
    if (u.hidden || !sees(u)) return;
    (u.def->move_class == myrts::MoveClass::Air ? air : ground).push_back(&u);
  });
  const auto by_y = [](const myrts::Unit* a, const myrts::Unit* b) { return a->y < b->y; };
  std::ranges::sort(ground, by_y);
  std::ranges::sort(air, by_y);

  const auto draw_unit = [&](const myrts::Unit* u) {
    const float lift = lift_of(*u->def);
    if (lift > 0) {  // shadow on the ground under a flyer
      al_draw_filled_ellipse(u->x, u->y + 6, 14, 6, al_map_rgba(0, 0, 0, 90));
    }
    const auto sprite = sprites_.find(u->def);
    const auto anim = anims_.find(u->id);
    if (sprite != sprites_.end() && sprite->second && anim != anims_.end())
      draw_sprite_centered(*sprite->second, anim->second, u->x, u->y - lift);
    const bool selected = std::ranges::find(selected_, u->id) != selected_.end();
    if (selected) {
      const auto b = unit_box(*u);
      al_draw_rectangle(b.x + 2, b.y + 2, b.x + b.w - 2, b.y + b.h - 2, kSelectGreen, 1.5f);
    }
    if (selected || u->hp < u->def->max_hp) draw_health_bar(u->x - 13, u->y - lift - 21, 26, u->hp, u->def->max_hp);
  };
  for (const myrts::Unit* u : ground) draw_unit(u);
  for (const myrts::Unit* u : air) draw_unit(u);

  draw_projectiles();
  fog_view_.draw(static_cast<float>(tile_size_));

  if (placing_ && mouse_in_window_ && ui::kViewport.contains(mouse_x_, mouse_y_)) draw_placement_ghost();
}

/// Missiles are drawn with primitives: they fly in a straight line (boulders and arrows in a small arc, as thrown).
void Game::draw_projectiles() {
  for (const Projectile& p : projectiles_) {
    const float t = static_cast<float>(std::clamp(p.age / p.duration, 0.0, 1.0));
    const float dx = p.x1 - p.x0, dy = p.y1 - p.y0;
    const float len = std::max(1.0f, std::hypot(dx, dy));
    const float ux = dx / len, uy = dy / len;
    float arc = 0;
    switch (p.kind) {
      case Projectile::Kind::Boulder: arc = std::min(46.0f, len * 0.35f); break;
      case Projectile::Kind::Arrow:
      case Projectile::Kind::Bolt:
      case Projectile::Kind::Axe: arc = std::min(12.0f, len * 0.08f); break;
      default: break;
    }
    const float lift = arc * 4 * t * (1 - t);  // parabola, highest halfway
    const float x = p.x0 + dx * t, y = p.y0 + dy * t - lift;
    const float slope = arc * 4 * (1 - 2 * t) / len;  // the arc tilts the shaft up, then down
    float sx = ux, sy = uy - slope;
    const float sl = std::max(0.001f, std::hypot(sx, sy));
    sx /= sl;
    sy /= sl;

    switch (p.kind) {
      case Projectile::Kind::Arrow:
      case Projectile::Kind::Bolt: {
        const float shaft = p.kind == Projectile::Kind::Bolt ? 16.0f : 11.0f;
        const float width = p.kind == Projectile::Kind::Bolt ? 2.2f : 1.4f;
        al_draw_line(x - sx * shaft, y - sy * shaft, x, y, al_map_rgb(140, 96, 52), width);
        al_draw_line(x - sx * shaft, y - sy * shaft, x - sx * (shaft - 4), y - sy * (shaft - 4), al_map_rgb(220, 220, 210), width);
        al_draw_filled_circle(x, y, width, al_map_rgb(214, 214, 220));  // the head
        break;
      }
      case Projectile::Kind::Axe: {
        const float a = static_cast<float>(p.age) * 22.0f;  // spins as it flies
        for (int i = 0; i < 2; ++i) {
          const float ang = a + static_cast<float>(i) * 1.5708f;
          al_draw_line(x - std::cos(ang) * 6, y - std::sin(ang) * 6, x + std::cos(ang) * 6, y + std::sin(ang) * 6,
                       i == 0 ? al_map_rgb(150, 150, 160) : al_map_rgb(120, 84, 48), 2.0f);
        }
        break;
      }
      case Projectile::Kind::Boulder: {
        al_draw_filled_ellipse(p.x0 + dx * t, p.y0 + dy * t + 4, 5, 2.5f, al_map_rgba_f(0, 0, 0, 0.3f));  // its shadow
        al_draw_filled_circle(x, y, 5, al_map_rgb(88, 82, 76));
        al_draw_filled_circle(x - 1.5f, y - 1.5f, 2.5f, al_map_rgb(132, 124, 116));
        break;
      }
      case Projectile::Kind::Fire: {
        for (int i = 3; i >= 1; --i) {  // a fading trail
          const float tt = std::max(0.0f, t - static_cast<float>(i) * 0.05f);
          al_draw_filled_circle(p.x0 + dx * tt, p.y0 + dy * tt, 5.0f - static_cast<float>(i), al_map_rgba_f(0.9f, 0.35f, 0.05f, 0.25f * (4 - i)));
        }
        al_draw_filled_circle(x, y, 6, al_map_rgb(236, 110, 24));
        al_draw_filled_circle(x, y, 3.2f, al_map_rgb(255, 224, 110));
        break;
      }
      case Projectile::Kind::Magic: {
        al_draw_filled_circle(x, y, 7, al_map_rgba_f(0.35f, 0.55f, 1.0f, 0.35f));
        al_draw_filled_circle(x, y, 3.5f, al_map_rgb(150, 190, 255));
        al_draw_filled_circle(x, y, 1.6f, al_map_rgb(255, 255, 255));
        break;
      }
      case Projectile::Kind::Coil: {
        const float a = static_cast<float>(p.age) * 14.0f;
        al_draw_filled_circle(x, y, 7, al_map_rgba_f(0.5f, 0.1f, 0.7f, 0.35f));
        al_draw_filled_circle(x + std::cos(a) * 3, y + std::sin(a) * 3, 3, al_map_rgb(170, 70, 220));
        al_draw_filled_circle(x - std::cos(a) * 3, y - std::sin(a) * 3, 3, al_map_rgb(90, 220, 110));
        break;
      }
    }
  }
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
  std::vector<ui::UnitCard> cards;
  for (const auto id : selected_) {
    const myrts::Unit* u = world_->units().get(id);
    if (!u) continue;
    cards.push_back({u->def->name, u->def->level, u->hp, u->def->max_hp, u->def->stats, u->def->icon});
  }
  std::vector<ui::MinimapDot> dots;
  world_->buildings().for_each([&](const myrts::Building& b) {
    const float half = static_cast<float>(b.def->size * tile_size_) / 2;
    if (!sees(b)) return;
    dots.push_back({static_cast<float>(b.origin.x * tile_size_) + half, static_cast<float>(b.origin.y * tile_size_) + half,
                    b.id == selected_building_, b.owner != local_player_, 4});
  });
  world_->units().for_each([&](const myrts::Unit& u) {
    if (u.hidden || !sees(u)) return;
    dots.push_back({u.x, u.y, std::ranges::find(selected_, u.id) != selected_.end(), u.owner != local_player_, 2});
  });

  // A selected building: its info card and what it can train.
  std::optional<ui::BuildingCard> building_card;
  std::vector<ui::TrainOption> train_options;
  const myrts::Stock& stock = world_->player(local_player_).stock;
  if (const myrts::Building* b = world_->buildings().get(selected_building_)) {
    ui::BuildingCard card;
    card.name = b->def->name;
    card.hp = b->hp;
    card.max_hp = b->def->hp;
    card.complete = b->complete;
    card.build_progress = static_cast<float>(b->progress / b->def->build_seconds);
    card.icon = b->def->icon;
    for (const myrts::TrainItem& q : b->queue)
      card.queue.push_back({q.unit->name, static_cast<float>(q.progress / q.unit->train_seconds), q.unit->icon});
    building_card = std::move(card);
    for (const myrts::UnitDef* def : current_train_options())
      train_options.push_back({def, myrts::can_afford(stock, def->cost)});
  }

  const std::string faction = selected_faction();
  std::vector<ui::CommandButton> commands;
  for (const myrts::CommandDef* c : current_commands()) commands.push_back({c, c->icon_for(faction), c == awaiting_target_});
  std::vector<ui::BuildOption> options;
  bool more_pages = false;
  for (const myrts::BuildingDef* def : build_page(&more_pages))
    options.push_back({def, myrts::can_afford(stock, def->cost), def == placing_});

  const ui::HudData hud{ui_art_,   stock,       cards,   dots,        terrain_view_.minimap(), minimap_,
                        camera_,   message_,    mouse_x_, mouse_y_,   commands,                build_menu_,
                        options,   more_pages,  building_card ? &*building_card : nullptr,     train_options,
                        world_->food_used(local_player_), world_->food_cap(local_player_), fog_view_.black()};
  ui::draw_hud(font_, hud);
}
