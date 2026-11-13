#pragma once

#include <functional>
#include <string>
#include <vector>

#include "scene.hpp"

/// One button: its label and the transition to make when chosen.
struct MenuItem {
  std::string label;
  std::function<Transition()> action;
};

/// Describes a menu screen completely (title, buttons, ESC behaviour).
struct MenuConfig {
  std::string title;
  std::string subtitle;
  ALLEGRO_COLOR title_color = al_map_rgb(240, 200, 60);
  std::vector<MenuItem> items;
  std::string music;                      // track to play while this menu is up (empty: leave the music as it is)
  bool music_loop = true;                 // false: play it once (the victory and defeat jingles)
  bool overlay = false;                   // dim and draw over the scene below
  std::function<Transition()> on_escape;  // optional: what ESC does
};

/// A vertical list of buttons, usable with keyboard (up/down/enter/esc) and mouse.
/// Main menu, pause menu and the victory/defeat screens are all just configurations of this.
class MenuScene : public Scene {
 public:
  MenuScene(const SceneContext& ctx, MenuConfig config) : ctx_(ctx), cfg_(std::move(config)) {}

  [[nodiscard]] bool is_overlay() const override { return cfg_.overlay; }
  void on_enter() override { start_music(); }
  void on_resume() override { start_music(); }  // back from the credits, say
  Transition handle_event(const ALLEGRO_EVENT& e) override;
  void draw() override;

 private:
  void start_music() {
    if (ctx_.audio && !cfg_.music.empty()) ctx_.audio->play_music(cfg_.music, cfg_.music_loop);
  }
  [[nodiscard]] float button_y(std::size_t i) const;
  [[nodiscard]] int button_at(float x, float y) const;

  SceneContext ctx_;
  MenuConfig cfg_;
  int selected_ = 0;
};
