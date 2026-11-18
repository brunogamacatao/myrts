#pragma once

#include <allegro5/allegro5.h>

#include "raii.hpp"

/// Lets the game keep drawing at its fixed size (ui::kWidth x ui::kHeight) whatever the window is: the frame is drawn on a canvas of
/// that size and then stretched, with black bars where the shapes differ, onto the real display. Fullscreen is a borderless window
/// at the desktop's resolution, so switching it is instant and the same canvas is used either way.
class Screen {
 public:
  /// Creates the canvas; false if it cannot be made (the game then draws straight onto the display).
  bool init(ALLEGRO_DISPLAY* display);

  /// Direct the scene's drawing to the canvas / put the finished frame on the display and flip it.
  void begin_frame();
  void end_frame();

  /// A mouse event in window coordinates turned into game coordinates (other events are returned unchanged).
  [[nodiscard]] ALLEGRO_EVENT to_game(const ALLEGRO_EVENT& e) const;

  [[nodiscard]] bool fullscreen() const;
  void set_fullscreen(bool on);
  void toggle_fullscreen() { set_fullscreen(!fullscreen()); }

 private:
  struct Fit {
    float scale = 1, x = 0, y = 0;
  };
  [[nodiscard]] Fit fit() const;

  ALLEGRO_DISPLAY* display_ = nullptr;
  BitmapPtr canvas_;
};

/// The game-wide screen (set by main); scenes use it to read and change the fullscreen setting.
Screen* screen();
void set_screen(Screen* s);
