#include <stdio.h>
#include <stdbool.h>
#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>
#include <iostream>
#include <fstream>
#include <string>
#include "sprite.hpp"

#define WIDTH 800
#define HEIGHT 600
#define FPS 60.0

// globals
double last_update_time;
bool running = true;

ALLEGRO_TIMER* timer;
ALLEGRO_EVENT_QUEUE* queue;
ALLEGRO_DISPLAY* disp;
ALLEGRO_FONT* font;
sprite character;

// input handling globals
bool key[ALLEGRO_KEY_MAX];

// functions
bool init();
void paint(double delta_time);
void destroy();

int main() {
  double delta_time;

  if (!init()) {
    exit(-1);
    return -1;
  }

  bool redraw = true;

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

bool init() {
  al_init();
  al_install_keyboard();

  if(!al_init_image_addon()) {
    return false;
    fprintf(stderr, "couldn't initialize image addon\n");
  }

  timer = al_create_timer(1.0 / FPS);
  queue = al_create_event_queue();
  disp  = al_create_display(WIDTH, HEIGHT);
  font  = al_create_builtin_font();
  character = load_sprite_from_json("sprites/orcs_peon.json");

  al_register_event_source(queue, al_get_keyboard_event_source());
  al_register_event_source(queue, al_get_display_event_source(disp));
  al_register_event_source(queue, al_get_timer_event_source(timer));

  return true;
}

void paint(double delta_time) {
  al_clear_to_color(al_map_rgb(0, 0, 0));

  // BEGIN - simple 8 directions controller test
  bool moving = false;

  if(key[ALLEGRO_KEY_UP]) {
    character.current_animation = "walk_up";
    character.direction = "up";
    moving = true;
  }

  if(key[ALLEGRO_KEY_DOWN]) {
    character.current_animation = "walk_down";
    character.direction = "down";
    moving = true;
  }

  if(key[ALLEGRO_KEY_RIGHT]) {
    character.current_animation = "walk_right";
    character.direction = "right";
    moving = true;
  }

  if(key[ALLEGRO_KEY_LEFT]) {
    character.current_animation = "walk_left";
    character.direction = "left";
    moving = true;
  }

  if (!moving) {
    character.current_animation = "idle_" + character.direction;
  }
  // END - simple 8 directions controller test

  if(key[ALLEGRO_KEY_ESCAPE]) {
    running = false;
  } 

  draw_sprite(character, delta_time);

  al_flip_display();
}

void destroy() {
  al_destroy_font(font);
  al_destroy_display(disp);
  al_destroy_timer(timer);
  al_destroy_event_queue(queue);
}