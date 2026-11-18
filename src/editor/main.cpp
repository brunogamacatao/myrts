// MyRTS Engine: the editor. Dear ImGui drawn through Allegro; the editing itself is in editor_model/.
#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>
#include <imgui_impl_allegro5.h>
#include <imgui.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "app.hpp"

namespace {
void must_init(bool ok, const char* what) {
  if (ok) return;
  std::fprintf(stderr, "couldn't initialize %s\n", what);
  std::exit(EXIT_FAILURE);
}
}  // namespace

int main(int argc, char** argv) {
  must_init(al_init(), "allegro");
  al_set_app_name("myrts_engine");
  must_init(al_install_keyboard(), "keyboard");
  must_init(al_install_mouse(), "mouse");
  must_init(al_init_primitives_addon(), "primitives addon");

  al_set_new_display_flags(ALLEGRO_RESIZABLE);
  int width = 1400, height = 860;
  if (const char* size = std::getenv("MYRTS_WINDOW")) std::sscanf(size, "%dx%d", &width, &height);
  ALLEGRO_DISPLAY* display = al_create_display(width, height);
  must_init(display, "display");
  ALLEGRO_EVENT_QUEUE* queue = al_create_event_queue();
  ALLEGRO_TIMER* timer = al_create_timer(1.0 / 60.0);
  al_register_event_source(queue, al_get_display_event_source(display));
  al_register_event_source(queue, al_get_keyboard_event_source());
  al_register_event_source(queue, al_get_mouse_event_source());
  al_register_event_source(queue, al_get_timer_event_source(timer));

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
  io.IniFilename = "myrts_engine.ini";  // the layout of the panels is remembered between runs
  ImGui::StyleColorsDark();
  ImGui_ImplAllegro5_Init(display);

  myrts::editor::App app;
  // The game to open at start: the first argument, or MYRTS_GAME.
  if (argc > 1) app.open_project(argv[1]);
  else if (const char* env = std::getenv("MYRTS_GAME")) app.open_project(env);

  std::string title;
  al_start_timer(timer);
  bool redraw = true;
  while (!app.wants_quit()) {
    ALLEGRO_EVENT event;
    al_wait_for_event(queue, &event);
    ImGui_ImplAllegro5_ProcessEvent(&event);
    if (event.type == ALLEGRO_EVENT_DISPLAY_CLOSE) app.request_quit();
    if (event.type == ALLEGRO_EVENT_DISPLAY_RESIZE) ImGui_ImplAllegro5_InvalidateDeviceObjects();
    if (event.type == ALLEGRO_EVENT_TIMER) redraw = true;

    if (redraw && al_is_event_queue_empty(queue)) {
      ImGui_ImplAllegro5_NewFrame();
      ImGui::NewFrame();
      app.draw();
      ImGui::Render();
      al_clear_to_color(al_map_rgb(30, 30, 34));
      ImGui_ImplAllegro5_RenderDrawData(ImGui::GetDrawData());
      al_flip_display();
      if (const std::string now = app.window_title(); now != title) {
        title = now;
        al_set_window_title(display, title.c_str());
      }
      redraw = false;
    }
  }

  ImGui_ImplAllegro5_Shutdown();
  ImGui::DestroyContext();
  al_destroy_timer(timer);
  al_destroy_event_queue(queue);
  al_destroy_display(display);
  return EXIT_SUCCESS;
}
