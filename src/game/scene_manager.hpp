#pragma once

#include <optional>

#include "core/scene_stack.hpp"
#include "scene.hpp"

/// Owns the stack of scenes, applies their transitions and draws the fade between them.
class SceneManager {
 public:
  explicit SceneManager(SceneContext ctx) : ctx_(ctx) {}
  SceneManager(const SceneManager&) = delete;
  SceneManager& operator=(const SceneManager&) = delete;
  ~SceneManager() { shutdown(); }

  /// Builds and enters the first scene.
  bool start(const SceneFactory& first);
  void handle_event(const ALLEGRO_EVENT& event);
  void update(double dt);
  void draw();

  [[nodiscard]] bool running() const { return !quit_ && !stack_.empty(); }
  /// Exits and frees every scene. Call before destroying the display.
  void shutdown() { stack_.clear(); }

 private:
  enum class Fade { None, Out, In };

  void submit(Transition t);
  void apply(const Transition& t);

  SceneContext ctx_;
  myrts::SceneStack<Scene> stack_;
  std::optional<Transition> pending_;  // waiting for the fade-out to finish
  Fade fade_ = Fade::None;
  double fade_time_ = 0.0;
  bool quit_ = false;
};
