#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_primitives.h>

#include <cstdio>
#include <cstdlib>

#include "assets.hpp"
#include "core/defs.hpp"
#include "core/sounds.hpp"
#include "core/tileset.hpp"
#include "scene_manager.hpp"
#include "scenes.hpp"
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

  auto registry = myrts::Registry::load_directory("data");
  if (!registry) {
    std::fprintf(stderr, "couldn't load game data: %s\n", registry.error().c_str());
    return EXIT_FAILURE;
  }
  auto tilesets = myrts::TileSets::load_file("data/tilesets.json");
  if (!tilesets) {
    std::fprintf(stderr, "couldn't load the tilesets: %s\n", tilesets.error().c_str());
    return EXIT_FAILURE;
  }
  const auto shared_tilesets = std::make_shared<const myrts::TileSets>(std::move(*tilesets));
  Assets assets;
  Audio audio;
  if (!std::getenv("MYRTS_MUTE")) audio.init("sounds");  // no sound card is fine: the game stays silent
  auto sound_table = myrts::SoundTable::load_file("data/sounds.json");
  if (!sound_table) {
    std::fprintf(stderr, "warning: %s\n", sound_table.error().c_str());
    sound_table = myrts::SoundTable{};
  }
  const auto shared_sounds = std::make_shared<const myrts::SoundTable>(std::move(*sound_table));
  Cursors cursors;
  cursors.load(display);

  SceneManager scenes({display, font, &assets, &*registry, &cursors, &audio, shared_sounds, shared_tilesets});
  if (!scenes.start(make_intro([] { return Transition::replace(make_main_menu()); }))) return EXIT_FAILURE;

  bool redraw = true;
  double last_time = al_get_time();
  al_start_timer(timer);

  while (scenes.running()) {
    ALLEGRO_EVENT event;
    al_wait_for_event(queue, &event);

    if (event.type == ALLEGRO_EVENT_TIMER) {
      const double now = al_get_time();
      scenes.update(now - last_time);
      last_time = now;
      redraw = true;
    } else {
      scenes.handle_event(event);
    }

    if (redraw && al_is_event_queue_empty(queue)) {
      scenes.draw();
      al_flip_display();
      redraw = false;
    }
  }

  scenes.shutdown();
  cursors.unload();  // free every scene's resources while Allegro is still initialised
  if (assets.live_sprites() + assets.live_bitmaps() != 0)  // everything is owned by scenes, so none should remain
    std::fprintf(stderr, "warning: %zu sprites and %zu bitmaps still alive after shutdown (leak?)\n",
                 assets.live_sprites(), assets.live_bitmaps());
  al_destroy_font(font);
  al_destroy_display(display);
  al_destroy_timer(timer);
  al_destroy_event_queue(queue);
  return EXIT_SUCCESS;
}
