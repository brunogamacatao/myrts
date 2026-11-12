#include "scene_manager.hpp"

#include <allegro5/allegro_primitives.h>

#include <algorithm>
#include <cstdio>

#include "ui.hpp"

namespace {
constexpr double kFadeSeconds = 0.25;
}

bool SceneManager::start(const SceneFactory& first) {
  auto scene = first(ctx_);
  if (!scene) {
    std::fprintf(stderr, "cannot start first scene: %s\n", scene.error().c_str());
    return false;
  }
  stack_.push(std::move(*scene));
  return true;
}

void SceneManager::submit(Transition t) {
  if (t.kind == Transition::Kind::None || pending_) return;
  if (t.fade && !stack_.empty()) {
    pending_ = std::move(t);
    fade_ = Fade::Out;
    fade_time_ = 0.0;
  } else {
    apply(t);
  }
}

void SceneManager::apply(const Transition& t) {
  using Kind = Transition::Kind;
  switch (t.kind) {
    case Kind::None:
      return;
    case Kind::Quit:
      quit_ = true;
      return;
    case Kind::Pop:
      stack_.pop();
      return;
    case Kind::Push:
    case Kind::Replace:
    case Kind::Reset: {
      // Build first: if loading fails the current scene stays in place.
      auto scene = t.factory ? t.factory(ctx_) : std::unexpected(std::string("transition without a scene"));
      if (!scene) {
        std::fprintf(stderr, "cannot change scene: %s\n", scene.error().c_str());
        return;
      }
      if (t.kind == Kind::Push) {
        stack_.push(std::move(*scene));
      } else if (t.kind == Kind::Replace) {
        stack_.replace(std::move(*scene));
      } else {
        stack_.clear();
        stack_.push(std::move(*scene));
      }
      return;
    }
  }
}

void SceneManager::handle_event(const ALLEGRO_EVENT& event) {
  if (event.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
    quit_ = true;
    return;
  }
  if (fade_ == Fade::Out) return;  // input is ignored while the screen fades away
  if (Scene* top = stack_.top()) submit(top->handle_event(event));
}

void SceneManager::update(double dt) {
  if (fade_ != Fade::None) fade_time_ += dt;
  if (fade_ == Fade::Out && fade_time_ >= kFadeSeconds) {
    if (pending_) {
      const Transition t = std::move(*pending_);
      pending_.reset();
      apply(t);
    }
    fade_ = Fade::In;
    fade_time_ = 0.0;
  } else if (fade_ == Fade::In && fade_time_ >= kFadeSeconds) {
    fade_ = Fade::None;
  }
  if (Scene* top = stack_.top()) submit(top->update(dt));
}

void SceneManager::draw() {
  al_clear_to_color(al_map_rgb(0, 0, 0));
  stack_.for_each_visible([](Scene& s) { s.draw(); });

  if (fade_ != Fade::None) {
    const double t = std::clamp(fade_time_ / kFadeSeconds, 0.0, 1.0);
    const auto alpha = static_cast<float>(fade_ == Fade::Out ? t : 1.0 - t);
    al_draw_filled_rectangle(0, 0, ui::kWidth, ui::kHeight, al_map_rgba_f(0, 0, 0, alpha));
  }
}
