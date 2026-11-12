#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_primitives.h>

#include <cstdio>
#include <cstdlib>

#include "game.hpp"
#include "ui.hpp"

namespace {
constexpr double kFps = 60.0;

void must_init(bool ok, const char* what) {
  if (ok) return;
  std::fprintf(stderr, "couldn't initialize %s\n", what);
  std::exit(EXIT_FAILURE);
}
}  // namespace

int main() {
  must_init(al_init(), "allegro");
  must_init(al_install_keyboard(), "keyboard");
  must_init(al_install_mouse(), "mouse");
  must_init(al_init_image_addon(), "image addon");
  must_init(al_init_primitives_addon(), "primitives addon");

  ALLEGRO_TIMER* timer = al_create_timer(1.0 / kFps);
  ALLEGRO_EVENT_QUEUE* queue = al_create_event_queue();
  ALLEGRO_DISPLAY* display = al_create_display(ui::kWidth, ui::kHeight);
  ALLEGRO_FONT* font = al_create_builtin_font();
  must_init(timer, "timer");
  must_init(queue, "event queue");
  must_init(display, "display");
  must_init(font, "font");
  al_set_window_title(display, "myrts");

  al_register_event_source(queue, al_get_keyboard_event_source());
  al_register_event_source(queue, al_get_mouse_event_source());
  al_register_event_source(queue, al_get_display_event_source(display));
  al_register_event_source(queue, al_get_timer_event_source(timer));

  auto game = Game::create(font);
  if (!game) {
    std::fprintf(stderr, "couldn't start game: %s\n", game.error().c_str());
    return EXIT_FAILURE;
  }

  bool redraw = true;
  double last_time = al_get_time();
  al_start_timer(timer);

  while ((*game)->running()) {
    ALLEGRO_EVENT event;
    al_wait_for_event(queue, &event);

    if (event.type == ALLEGRO_EVENT_TIMER) {
      const double now = al_get_time();
      (*game)->update(now - last_time);
      last_time = now;
      redraw = true;
    } else {
      (*game)->handle_event(event);
    }

    if (redraw && al_is_event_queue_empty(queue)) {
      (*game)->draw();
      al_flip_display();
      redraw = false;
    }
  }

  game->reset();  // free game resources while Allegro is still initialised
  al_destroy_font(font);
  al_destroy_display(display);
  al_destroy_timer(timer);
  al_destroy_event_queue(queue);
  return EXIT_SUCCESS;
}
