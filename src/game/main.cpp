#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_primitives.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "assets.hpp"
#include "i18n.hpp"
#include "core/defs.hpp"
#include "core/game_package.hpp"
#include "core/sounds.hpp"
#include "core/tileset.hpp"
#include "scene_manager.hpp"
#include "screen.hpp"
#include "scenes.hpp"
#include "ui.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
constexpr double kFps = 60.0;

/// The folder the program is in (empty if unknown).
std::filesystem::path program_dir() {
  std::error_code ec;
#ifdef _WIN32
  char buffer[MAX_PATH];
  const DWORD n = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
  return n > 0 && n < MAX_PATH ? std::filesystem::path(std::string(buffer, n)).parent_path() : std::filesystem::path{};
#else
  return std::filesystem::read_symlink("/proc/self/exe", ec).parent_path();
#endif
}

void must_init(bool ok, const char* what) {
  if (ok) return;
  std::fprintf(stderr, "couldn't initialize %s\n", what);
  std::exit(EXIT_FAILURE);
}
}  // namespace

int main(int argc, char** argv) {
  // The game to play is a folder with a game.json: --game <dir>, or MYRTS_GAME, or the one beside the program.
  // A game made with the editor and exported has its package in a "game" folder beside the program.
  std::string game_dir = "games/warcraft-like";
  if (const auto beside = program_dir() / "game"; std::filesystem::exists(beside / "game.json")) game_dir = beside.string();
  if (const char* env = std::getenv("MYRTS_GAME")) game_dir = env;
  std::string level_arg, faction_arg;  // --level N --faction ID: start that level straight away (the editor's "Play")
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "--game") game_dir = argv[i + 1];
    if (std::string(argv[i]) == "--level") level_arg = argv[i + 1];
    if (std::string(argv[i]) == "--faction") faction_arg = argv[i + 1];
  }
  auto package_result = myrts::GamePackage::load(game_dir);
  if (!package_result) {
    std::fprintf(stderr, "couldn't load the game: %s\n", package_result.error().c_str());
    return EXIT_FAILURE;
  }
  const auto package = std::make_shared<const myrts::GamePackage>(std::move(*package_result));

  must_init(al_init(), "allegro");
  al_set_app_name("myrts");  // names the per-user data folder where the saved games go
  must_init(al_install_keyboard(), "keyboard");
  must_init(al_install_mouse(), "mouse");
  must_init(al_init_image_addon(), "image addon");
  must_init(al_init_primitives_addon(), "primitives addon");

  ALLEGRO_TIMER* timer = al_create_timer(1.0 / kFps);
  ALLEGRO_EVENT_QUEUE* queue = al_create_event_queue();
  // The game is drawn at a fixed size and scaled to the window; MYRTS_WINDOW=WxH opens a bigger one (testing aid).
  int window_w = ui::kWidth, window_h = ui::kHeight;
  if (const char* size = std::getenv("MYRTS_WINDOW")) std::sscanf(size, "%dx%d", &window_w, &window_h);
  ALLEGRO_DISPLAY* display = al_create_display(std::max(window_w, 200), std::max(window_h, 150));
  ALLEGRO_FONT* font = al_create_builtin_font();
  must_init(timer, "timer");
  must_init(queue, "event queue");
  must_init(display, "display");
  must_init(font, "font");
  al_set_window_title(display, package->title.c_str());
  Screen screen_;
  if (!screen_.init(display)) std::fprintf(stderr, "warning: no offscreen canvas, fullscreen scaling is off\n");
  set_screen(&screen_);
  if (std::getenv("MYRTS_FULLSCREEN")) screen_.set_fullscreen(true);

  al_register_event_source(queue, al_get_keyboard_event_source());
  al_register_event_source(queue, al_get_mouse_event_source());
  al_register_event_source(queue, al_get_display_event_source(display));
  al_register_event_source(queue, al_get_timer_event_source(timer));

  auto registry = myrts::Registry::load_directory(package->path(package->data_dir));
  if (!registry) {
    std::fprintf(stderr, "couldn't load game data: %s\n", registry.error().c_str());
    return EXIT_FAILURE;
  }
  auto tilesets = myrts::TileSets::load_file(package->path(package->tilesets_file));
  if (!tilesets) {
    std::fprintf(stderr, "couldn't load the tilesets: %s\n", tilesets.error().c_str());
    return EXIT_FAILURE;
  }
  const auto shared_tilesets = std::make_shared<const myrts::TileSets>(std::move(*tilesets));
  // The campaign: levels in order. MYRTS_MAP plays a single map instead; a missing file falls back to one level.
  auto campaign = myrts::Campaign::load_file(package->path(package->campaign_file), package->path(package->ai_profiles_file));
  if (!campaign) {
    std::fprintf(stderr, "warning: %s\n", campaign.error().c_str());
    campaign = myrts::Campaign::single(package->fallback_map);
  }
  if (const char* map = std::getenv("MYRTS_MAP")) campaign = myrts::Campaign::single(map);
  const auto shared_campaign = std::make_shared<const myrts::Campaign>(std::move(*campaign));
  // The rules are optional (every field has a default), but a file that exists must be valid.
  myrts::GameRules rules;
  if (std::filesystem::exists(package->path(package->rules_file))) {
    auto loaded = myrts::GameRules::load_file(package->path(package->rules_file));
    if (!loaded) {
      std::fprintf(stderr, "couldn't load the rules: %s\n", loaded.error().c_str());
      return EXIT_FAILURE;
    }
    rules = *loaded;
  }
  const auto shared_rules = std::make_shared<const myrts::GameRules>(rules);
  // The interface language: MYRTS_LANG, else the game's default. A missing language file leaves everything in English.
  std::shared_ptr<const myrts::Strings> shared_strings = myrts::Strings::english();
  {
    const char* env = std::getenv("MYRTS_LANG");
    const std::string language = env ? env : package->default_language;
    if (language != "en") {
      auto strings = myrts::Strings::load_file(package->path(package->strings_dir + "/" + language + ".json"));
      if (strings) shared_strings = std::make_shared<const myrts::Strings>(std::move(*strings));
      else std::fprintf(stderr, "warning: no '%s' language: %s\n", language.c_str(), strings.error().c_str());
    }
    set_strings(shared_strings);
  }
  Assets assets(package->root().generic_string());
  Audio audio;
  if (!std::getenv("MYRTS_MUTE")) audio.init(package->path(package->sounds_dir));  // no sound card is fine: the game stays silent
  auto sound_table = myrts::SoundTable::load_file(package->path(package->sound_table_file));
  if (!sound_table) {
    std::fprintf(stderr, "warning: %s\n", sound_table.error().c_str());
    sound_table = myrts::SoundTable{};
  }
  const auto shared_sounds = std::make_shared<const myrts::SoundTable>(std::move(*sound_table));
  Cursors cursors;
  cursors.load(display, package->path(package->cursors_dir));

  // Saved games live in the user's data folder (MYRTS_SAVES overrides the file, for testing).
  std::string save_path = "saves.json";
  if (const char* env = std::getenv("MYRTS_SAVES")) {
    save_path = env;
  } else if (ALLEGRO_PATH* dir = al_get_standard_path(ALLEGRO_USER_DATA_PATH)) {
    al_set_path_filename(dir, "saves.json");
    save_path = al_path_cstr(dir, ALLEGRO_NATIVE_PATH_SEP);
    al_destroy_path(dir);
  }

  SceneManager scenes({display, font, &assets, &*registry, &cursors, shared_campaign, &audio, shared_sounds, shared_tilesets, save_path, shared_rules, shared_strings, package});
  // Testing aid: MYRTS_FACTION=alliance starts the first level straight away as that side (skipping the menus).
  if (const char* env_faction = std::getenv("MYRTS_FACTION"); env_faction || !faction_arg.empty()) {
    const char* env_level = std::getenv("MYRTS_LEVEL");  // which level (1-based) to start on
    const std::string level = !level_arg.empty() ? level_arg : (env_level ? env_level : "");
    const std::string faction = !faction_arg.empty() ? faction_arg : env_faction;
    if (!scenes.start(make_game(level.empty() ? 0 : static_cast<std::size_t>(std::max(1, std::atoi(level.c_str())) - 1), faction.c_str()))) return EXIT_FAILURE;
  } else if (!scenes.start(make_intro([] { return Transition::replace(make_main_menu()); }))) {
    return EXIT_FAILURE;
  }

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
    } else if (event.type == ALLEGRO_EVENT_DISPLAY_RESIZE) {
      al_acknowledge_resize(display);
    } else if (event.type == ALLEGRO_EVENT_KEY_DOWN &&
               (event.keyboard.keycode == ALLEGRO_KEY_F11 ||
                (event.keyboard.keycode == ALLEGRO_KEY_ENTER && (event.keyboard.modifiers & ALLEGRO_KEYMOD_ALT)))) {
      screen_.toggle_fullscreen();  // F11 or Alt+Enter, from any screen
    } else {
      scenes.handle_event(screen_.to_game(event));
    }

    if (redraw && al_is_event_queue_empty(queue)) {
      screen_.begin_frame();
      scenes.draw();
      screen_.end_frame();
      redraw = false;
    }
  }

  scenes.shutdown();
  set_screen(nullptr);
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
