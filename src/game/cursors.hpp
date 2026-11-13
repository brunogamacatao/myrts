#pragma once

#include <allegro5/allegro5.h>

#include <array>

/// The original Warcraft II mouse cursors (assets/cursors, built by scripts/extract_ui_assets.py).
enum class CursorKind {
  Pointer,
  PointerNo,
  Target,       // yellow crosshairs: choosing where to build
  TargetEnemy,  // red crosshairs: an attack target
  TargetOk,     // green crosshairs: a valid destination or resource
  Magnifier,
  Wait,
  ScrollN,
  ScrollNE,
  ScrollE,
  ScrollSE,
  ScrollS,
  ScrollSW,
  ScrollW,
  ScrollNW,
  Count
};

class Cursors {
 public:
  Cursors() = default;
  Cursors(const Cursors&) = delete;
  Cursors& operator=(const Cursors&) = delete;
  ~Cursors() { unload(); }

  /// Loads the cursors for `display`. Returns false (leaving the system cursor) if the files are missing.
  bool load(ALLEGRO_DISPLAY* display);
  /// Frees them; call before the display is destroyed.
  void unload();
  void set(CursorKind kind);

 private:
  ALLEGRO_DISPLAY* display_ = nullptr;
  std::array<ALLEGRO_MOUSE_CURSOR*, static_cast<std::size_t>(CursorKind::Count)> cursors_{};
  CursorKind current_ = CursorKind::Count;
};
