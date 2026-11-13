#include "scenes.hpp"

#include <allegro5/allegro_primitives.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "draw_util.hpp"
#include "game.hpp"
#include "menu_scene.hpp"
#include "ui.hpp"

namespace {

template <class T, class... Args>
SceneFactory factory_of(Args... args) {
  return [=](const SceneContext& ctx) -> std::expected<std::unique_ptr<Scene>, std::string> {
    return std::make_unique<T>(ctx, args...);
  };
}

// ---- opening credits ---------------------------------------------------------

/// Slides separated by blank lines in assets/credits.txt; the first line of each is the heading.
class IntroScene : public Scene {
 public:
  IntroScene(const SceneContext& ctx, std::function<Transition()> when_done)
      : ctx_(ctx), when_done_(std::move(when_done)), slides_(load_slides()) {}

  Transition handle_event(const ALLEGRO_EVENT& e) override {
    const bool key = e.type == ALLEGRO_EVENT_KEY_DOWN;
    const bool click = e.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN;
    if (key && e.keyboard.keycode == ALLEGRO_KEY_ESCAPE) return when_done_();  // skip everything
    if (key || click) {
      advance();
      if (finished()) return when_done_();
    }
    return Transition::none();
  }

  Transition update(double dt) override {
    time_ += dt;
    if (time_ >= kSlideSeconds) {
      advance();
      if (finished()) return when_done_();
    }
    return Transition::none();
  }

  void draw() override {
    if (finished()) return;
    const auto w = static_cast<float>(ui::kWidth);
    const auto h = static_cast<float>(ui::kHeight);

    // fade in/out of each slide
    const double edge = 0.6;
    double alpha = 1.0;
    if (time_ < edge) alpha = time_ / edge;
    if (time_ > kSlideSeconds - edge) alpha = (kSlideSeconds - time_) / edge;
    alpha = std::clamp(alpha, 0.0, 1.0);
    const auto a = static_cast<float>(alpha);

    const auto& lines = slides_[index_];
    const float total = 40.0f + static_cast<float>(lines.size() - 1) * 24.0f;
    float y = (h - total) / 2;
    for (std::size_t i = 0; i < lines.size(); ++i) {
      if (i == 0) {
        draw_text_scaled(ctx_.font, w / 2, y, fit_scale(ctx_.font, lines[i], index_ == 0 ? 6.0f : 3.0f, w - 40),
                         ALLEGRO_ALIGN_CENTER, lines[i],
                         al_map_rgba_f(0.95f * a, 0.78f * a, 0.24f * a, a));
        y += index_ == 0 ? 64.0f : 40.0f;
      } else {
        draw_text_scaled(ctx_.font, w / 2, y, fit_scale(ctx_.font, lines[i], 2.0f, w - 40),
                         ALLEGRO_ALIGN_CENTER, lines[i],
                         al_map_rgba_f(0.93f * a, 0.88f * a, 0.75f * a, a));
        y += 24.0f;
      }
    }
    draw_text_scaled(ctx_.font, w - 12, h - 18, 1.0f, ALLEGRO_ALIGN_RIGHT, "press any key to skip",
                     al_map_rgba_f(0.5f * a, 0.5f * a, 0.5f * a, a));
  }

 private:
  static constexpr double kSlideSeconds = 3.5;

  static std::vector<std::vector<std::string>> load_slides() {
    std::vector<std::vector<std::string>> slides{{}};
    std::ifstream in("credits.txt");
    for (std::string line; std::getline(in, line);) {
      while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
      if (line.empty()) {
        if (!slides.back().empty()) slides.emplace_back();
      } else {
        slides.back().push_back(line);
      }
    }
    if (slides.back().empty()) slides.pop_back();
    if (slides.empty()) slides.push_back({"MYRTS"});
    return slides;
  }

  void advance() {
    ++index_;
    time_ = 0.0;
  }
  [[nodiscard]] bool finished() const { return index_ >= slides_.size(); }

  SceneContext ctx_;
  std::function<Transition()> when_done_;
  std::vector<std::vector<std::string>> slides_;
  std::size_t index_ = 0;
  double time_ = 0.0;
};

// ---- the game itself ------------------------------------------------------------

class GameScene : public Scene {
 public:
  GameScene(std::unique_ptr<Game> game, std::size_t level) : game_(std::move(game)), level_(level) {}

  void on_resume() override { game_->release_input(); }  // keys may have changed while paused

  Transition handle_event(const ALLEGRO_EVENT& e) override {
    if (e.type == ALLEGRO_EVENT_KEY_DOWN) {  // debug shortcuts until real win/lose rules exist
      if (e.keyboard.keycode == ALLEGRO_KEY_F11) return Transition::replace(make_game_over(GameResult::Victory, level_));
      if (e.keyboard.keycode == ALLEGRO_KEY_F12) return Transition::replace(make_game_over(GameResult::Defeat, level_));
    }
    game_->handle_event(e);
    if (game_->take_menu_request()) return Transition::push(make_pause_menu(level_));
    return Transition::none();
  }

  Transition update(double dt) override {
    game_->update(dt);
    if (const auto outcome = game_->outcome())
      return Transition::replace(make_game_over(*outcome == myrts::Outcome::Victory ? GameResult::Victory
                                                                                     : GameResult::Defeat,
                                    level_));
    return Transition::none();
  }

  void draw() override { game_->draw(); }

 private:
  std::unique_ptr<Game> game_;
  std::size_t level_;
};

}  // namespace

// ---- factories -----------------------------------------------------------------

SceneFactory make_intro(std::function<Transition()> when_done) {
  return factory_of<IntroScene>(std::move(when_done));
}

SceneFactory make_game(std::size_t level) {
  return [level](const SceneContext& ctx) -> std::expected<std::unique_ptr<Scene>, std::string> {
    auto game = Game::create(ctx, level);
    if (!game) return std::unexpected(game.error());
    return std::make_unique<GameScene>(std::move(*game), level);
  };
}

SceneFactory make_main_menu() {
  return [](const SceneContext& ctx) -> std::expected<std::unique_ptr<Scene>, std::string> {
    MenuConfig cfg;
    cfg.title = "MYRTS";
    cfg.subtitle = "A real-time strategy game";
    cfg.items = {
        {"New Game", [] { return Transition::replace(make_game()); }},
        {"Options", [] { return Transition::push(make_options()); }},
        {"Credits", [] { return Transition::push(make_intro([] { return Transition::pop(); }), true); }},
        {"Exit", [] { return Transition::quit(); }},
    };
    return std::make_unique<MenuScene>(ctx, std::move(cfg));
  };
}

SceneFactory make_pause_menu(std::size_t level) {
  return [level](const SceneContext& ctx) -> std::expected<std::unique_ptr<Scene>, std::string> {
    MenuConfig cfg;
    cfg.title = "Game Menu";
    cfg.overlay = true;
    cfg.on_escape = [] { return Transition::pop(); };
    cfg.items = {
        {"Resume", [] { return Transition::pop(); }},
        {"Options", [] { return Transition::push(make_options()); }},
        {"Restart Level", [level] { return Transition::reset(make_game(level)); }},
        {"Quit to Main Menu", [] { return Transition::reset(make_main_menu()); }},
        {"Exit Game", [] { return Transition::quit(); }},
    };
    return std::make_unique<MenuScene>(ctx, std::move(cfg));
  };
}

SceneFactory make_game_over(GameResult result, std::size_t level) {
  return [result, level](const SceneContext& ctx) -> std::expected<std::unique_ptr<Scene>, std::string> {
    const bool won = result == GameResult::Victory;
    const auto next = ctx.campaign ? ctx.campaign->next(level) : std::nullopt;
    const bool multi = ctx.campaign && ctx.campaign->size() > 1;
    MenuConfig cfg;
    cfg.title = won ? (multi && !next ? "Campaign Complete!" : "Victory!") : "Defeat";
    cfg.subtitle = won ? (next ? "Next: " + ctx.campaign->level(*next).chapter + " - " + ctx.campaign->level(*next).name : "Your enemies have been crushed.")
                       : "Your forces have been destroyed.";
    cfg.title_color = won ? al_map_rgb(240, 200, 60) : al_map_rgb(220, 60, 50);
    if (won && next) {
      const std::size_t n = *next;
      cfg.items.push_back({"Next Level", [n] { return Transition::reset(make_game(n)); }});
    }
    cfg.items.push_back({won ? "Replay Level" : "Try Again", [level] { return Transition::reset(make_game(level)); }});
    if (won && !next && multi) cfg.items.push_back({"Play From the Start", [] { return Transition::reset(make_game(0)); }});
    cfg.items.push_back(MenuItem{"Main Menu", [] { return Transition::reset(make_main_menu()); }});
    cfg.items.push_back(MenuItem{"Exit Game", [] { return Transition::quit(); }});
    return std::make_unique<MenuScene>(ctx, std::move(cfg));
  };
}
