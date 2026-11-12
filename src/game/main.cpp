#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_primitives.h>
#include <string>
#include "sprite.hpp"
#include "tilemap.hpp"
#include "core/pathfinding.hpp"

constexpr int WIDTH = 800;
constexpr int HEIGHT = 600;
constexpr double FPS = 60.0;

struct t_mouse {
  int x;
  int y;
  int mouse_pressed_x;
  int mouse_pressed_y;
  int mouse_released_x;
  int mouse_released_y;
  bool is_pressed;
};

using myrts::Point;

// globals
double last_update_time;
bool running = true;
std::string status = "";

ALLEGRO_TIMER* timer;
ALLEGRO_EVENT_QUEUE* queue;
ALLEGRO_DISPLAY* disp;
ALLEGRO_FONT* font;
sprite character;
tmx_map *map;
t_mouse mouse = {0, 0, 0, 0, 0, 0, false};

// input handling globals
bool key[ALLEGRO_KEY_MAX] = {};

// functions
void init();
void paint(double delta_time);
void destroy();

// mouse functions
void process_mouse();
void mouse_pressed();
void mouse_released();

// fixed map just for testing purposes
std::deque<Point> path;
myrts::Grid collisions_grid;

int main() {
  double delta_time;
  bool redraw = true;

  init();

  ALLEGRO_EVENT event;
  last_update_time = al_get_time();

  al_start_timer(timer);

  while(running) {
    al_wait_for_event(queue, &event);

    switch(event.type) {
      case ALLEGRO_EVENT_TIMER:
        redraw = true;
        break;
      case ALLEGRO_EVENT_KEY_DOWN:
        key[event.keyboard.keycode] = true;
        break;
      case ALLEGRO_EVENT_KEY_UP:
        key[event.keyboard.keycode] = false;
        break;
      case ALLEGRO_EVENT_MOUSE_AXES:
        mouse.x = event.mouse.x;
        mouse.y = event.mouse.y;
        break;
      case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
        if (!mouse.is_pressed) {
          mouse.is_pressed = true;
          mouse.mouse_pressed_x = event.mouse.x;
          mouse.mouse_pressed_y = event.mouse.y;
          mouse_pressed();
        }
        break;
      case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
        mouse.is_pressed = false;
        mouse.mouse_released_x = event.mouse.x;
        mouse.mouse_released_y = event.mouse.y;
        mouse_released();
        break;
      case ALLEGRO_EVENT_DISPLAY_CLOSE:
        running = false;
        break;
    }

    if (redraw && al_is_event_queue_empty(queue)) {
      delta_time = al_get_time() - last_update_time;
      last_update_time = al_get_time();
      paint(delta_time);
      redraw = false;
    }
  }

  destroy();

  return EXIT_SUCCESS;
}

void must_init(bool test, const char *description) {
  if(test) return;

  std::fprintf(stderr, "couldn't initialize %s\n", description);
  std::exit(1);
}

void init() {
  must_init(al_init(), "allegro");
  must_init(al_install_keyboard(), "keyboard");
  must_init(al_install_mouse(), "mouse");
  must_init(al_init_image_addon(), "image addon");

  must_init(al_init_primitives_addon(), "primitives addon");

  timer = al_create_timer(1.0 / FPS);
  must_init(timer, "timer");
  queue = al_create_event_queue();
  must_init(queue, "event queue");
  disp  = al_create_display(WIDTH, HEIGHT);
  must_init(disp, "display");
  font  = al_create_builtin_font();
  must_init(font, "font");

  init_tilemap_engine();

  // loading resources
  auto loaded = load_sprite_from_json("sprites/orcs_peon.json");
  if (!loaded) {
    std::fprintf(stderr, "couldn't load sprite: %s\n", loaded.error().c_str());
    std::exit(1);
  }
  character = std::move(*loaded);

  map = tmx_load("tiles/mapa.tmx");
  if (!map) {
    tmx_perror("couldn't load map");
    std::exit(1);
  }
  collisions_grid = get_collision_grid(map);

  // registering engines
  al_register_event_source(queue, al_get_keyboard_event_source());
  al_register_event_source(queue, al_get_mouse_event_source());
  al_register_event_source(queue, al_get_display_event_source(disp));
  al_register_event_source(queue, al_get_timer_event_source(timer));
}

void paint(double delta_time) {
  al_clear_to_color(al_map_rgb(0, 0, 0));

  if (map) {
    render_map(map);
  }

  // BEGIN simple movement
  if (!path.empty()) {
    const Point goal = path.front();

    const int dest_x = goal.x * static_cast<int>(map->tile_width);
    const int dest_y = goal.y * static_cast<int>(map->tile_height);

    if (static_cast<int>(character.x) == dest_x && static_cast<int>(character.y) == dest_y) {
      path.pop_front();
    } else {
      character.direction = "";
      if (character.y < dest_y) {
        character.y++;
        character.direction += "_down";
      } else if (character.y > dest_y) {
        character.y--;
        character.direction += "_up";
      }
      if (character.x < dest_x) {
        character.x++;
        character.direction += "_right";
      } else if (character.x > dest_x) {
        character.x--;
        character.direction += "_left";
      }
      character.current_animation = "walk" + character.direction;
    }
  } else {
    character.current_animation = "idle" + character.direction;
  }
  // END simple movement

  if(key[ALLEGRO_KEY_ESCAPE]) {
    running = false;
  } 

  process_mouse();

  draw_sprite(character, delta_time);
  al_draw_text(font, al_map_rgb(255, 255, 255), 0, 0, 0, status.c_str());

  al_flip_display();
}

void destroy() {
  if (map) {
    destroy_map(map);
  }

  al_destroy_font(font);
  al_destroy_display(disp);
  al_destroy_timer(timer);
  al_destroy_event_queue(queue);
}

Point get_tile_coordinates() {
  return {mouse.x / static_cast<int>(map->tile_width), mouse.y / static_cast<int>(map->tile_height)};
}

void process_mouse() {
  if (!map) return;

  // fetching the tile based on mouse coordinates
  const Point tile_coords = get_tile_coordinates();

  status = "X: " + std::to_string(mouse.x) + " - Y: " + std::to_string(mouse.y);
  status += " [tile x: " + std::to_string(tile_coords.x) + ", y: " + std::to_string(tile_coords.y) + "]";
}

void mouse_pressed() {
  if (!map) return;

  const Point goal = get_tile_coordinates();

  // path starts at the tile the character currently occupies
  const Point start{
      static_cast<int>(std::lround(character.x / map->tile_width)),
      static_cast<int>(std::lround(character.y / map->tile_height))};

  path = myrts::a_star(collisions_grid, start, goal);
}

void mouse_released() {

}
