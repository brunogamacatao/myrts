#include <stdio.h>
#include <stdbool.h>
#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_primitives.h>
#include <iostream>
#include <fstream>
#include <string>
#include "sprite.hpp"
#include "tilemap.hpp"
#include "pathfinding.hpp"

#define WIDTH 800
#define HEIGHT 600
#define FPS 60.0

typedef struct {
  int x;
  int y;
  int mouse_pressed_x;
  int mouse_pressed_y;
  int mouse_released_x;
  int mouse_released_y;
  bool is_pressed;
} t_mouse;

typedef struct {
  int x;
  int y;
} t_point;

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
bool key[ALLEGRO_KEY_MAX];

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
std::vector<std::vector<int>> collisions_grid;

int main() {
  double delta_time;
  bool redraw = true;

  init();

  ALLEGRO_EVENT event;
  last_update_time = al_get_time();
  memset(key, false, sizeof(key)); // initializing keys map

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

    if (event.type == ALLEGRO_EVENT_TIMER) {
      redraw = true;
    } else if(event.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
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

  return 0;
}

void must_init(bool test, const char *description) {
  if(test) return;

  fprintf(stderr, "couldn't initialize %s\n", description);

  running = false;
  exit(1);
}

void init() {
  must_init(al_init(), "allegro");
  must_init(al_install_keyboard(), "keyboard");
  must_init(al_install_mouse(), "mouse");
  must_init(al_init_image_addon(), "image addon");

  timer = al_create_timer(1.0 / FPS);
  queue = al_create_event_queue();
  disp  = al_create_display(WIDTH, HEIGHT);
  font  = al_create_builtin_font();

  init_tilemap_engine();

  // loading resources
  character = load_sprite_from_json("sprites/orcs_peon.json");
  map = tmx_load("tiles/mapa.tmx");
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
    Point goal = path.front();

    int dest_x = goal.x * map->tile_width;
    int dest_y = goal.y * map->tile_height;

    if (character.x == dest_x && character.y == dest_y) {
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

t_point get_tile_coordinates() {
  t_point coords;

  unsigned int tile_width = map->tile_width;
  unsigned int tile_height = map->tile_height;

  coords.x = mouse.x / tile_width;
  coords.y = mouse.y / tile_height;

  return coords;
}

void process_mouse() {
  if (!map) return;

  // fetching the tile based on mouse coordinates
  t_point tile_coords = get_tile_coordinates();

  status = "X: " + std::to_string(mouse.x) + " - Y: " + std::to_string(mouse.y);
  status += " [tile x: " + std::to_string(tile_coords.x) + ", y: " + std::to_string(tile_coords.y) + "]";
}

Point start{0,0};
Point goal{0,0};

void mouse_pressed() {
  t_point tile_coords = get_tile_coordinates();

  goal.x = tile_coords.x;
  goal.y = tile_coords.y;

  path = a_star(collisions_grid, start, goal);

  start.x = tile_coords.x;
  start.y = tile_coords.y;
}

void mouse_released() {

}
