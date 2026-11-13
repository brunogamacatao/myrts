#pragma once

#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>

#include <expected>
#include <functional>
#include <memory>
#include <string>

#include "assets.hpp"
#include "audio.hpp"
#include "core/defs.hpp"
#include "core/sounds.hpp"
#include "core/tileset.hpp"
#include "cursors.hpp"

class Scene;

/// Services every scene may use. Passed explicitly (no globals); owned by main().
struct SceneContext {
  ALLEGRO_DISPLAY* display = nullptr;
  ALLEGRO_FONT* font = nullptr;
  Assets* assets = nullptr;                    // shared, reference-counted resources
  const myrts::Registry* registry = nullptr;   // game data (units, buildings, commands)
  Cursors* cursors = nullptr;                   // the mouse cursors (a scene may change them each frame)
  Audio* audio = nullptr;                       // the sound device (silent when there is none)
  std::shared_ptr<const myrts::SoundTable> sounds;  // which file plays for which event
  std::shared_ptr<const myrts::TileSets> tilesets;  // which tile code is drawn where, per terrain era
};

/// Builds a scene when the manager needs it (the old scene is still alive while this runs, so a
/// failed load leaves the game where it was).
using SceneFactory = std::function<std::expected<std::unique_ptr<Scene>, std::string>(const SceneContext&)>;

/// What a scene asks the manager to do after handling an event or an update.
/// Applied by the manager between calls, never in the middle of a scene's own method.
struct Transition {
  enum class Kind {
    None,
    Push,     // put a scene on top; the one below is paused
    Pop,      // remove the top scene; the one below resumes
    Replace,  // swap the top scene
    Reset,    // drop every scene, then start the new one
    Quit
  };

  Kind kind = Kind::None;
  SceneFactory factory;
  bool fade = false;  // fade to black and back around the switch

  static Transition none() { return {}; }
  static Transition push(SceneFactory f, bool fade = false) { return {Kind::Push, std::move(f), fade}; }
  static Transition pop(bool fade = false) { return {Kind::Pop, {}, fade}; }
  static Transition replace(SceneFactory f, bool fade = true) { return {Kind::Replace, std::move(f), fade}; }
  static Transition reset(SceneFactory f, bool fade = true) { return {Kind::Reset, std::move(f), fade}; }
  static Transition quit() { return {Kind::Quit, {}, false}; }
};

/// One screen of the game (intro, menu, a match...). The SceneManager keeps scenes on a stack; only the
/// top one gets input and updates. Scenes never create each other: they return a Transition.
class Scene {
 public:
  virtual ~Scene() = default;

  /// Acquire resources here or in the constructor; release them in the destructor (RAII).
  /// on_exit() runs before destruction and is only for behaviour (stop music, save state...).
  virtual void on_enter() {}
  virtual void on_exit() {}
  /// Another scene was pushed on top / it was removed again.
  virtual void on_pause() {}
  virtual void on_resume() {}

  /// An overlay is drawn on top of the scene below it (pause menus, dialogs).
  [[nodiscard]] virtual bool is_overlay() const { return false; }

  /// Only the top scene receives events and updates.
  virtual Transition handle_event(const ALLEGRO_EVENT&) { return Transition::none(); }
  virtual Transition update(double /*dt*/) { return Transition::none(); }
  virtual void draw() = 0;
};
