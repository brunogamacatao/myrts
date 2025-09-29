#include <stdio.h>
#include <stdbool.h>
#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_primitives.h>
#include <iostream>
#include <fstream>
#include <string>
#include <tmx.h>
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
void paint(tmx_map *map, double delta_time);
void destroy();

// tiled functions
#define LINE_THICKNESS 2.5
void* Allegro5_tex_loader(const char *path);
ALLEGRO_COLOR int_to_al_color(int color);
void draw_polyline(double **points, double x, double y, int pointsc, ALLEGRO_COLOR color);
void draw_polygone(double **points, double x, double y, int pointsc, ALLEGRO_COLOR color);
void draw_objects(tmx_object_group *objgr);
void draw_tile(void *image, unsigned int sx, unsigned int sy, unsigned int sw, unsigned int sh, unsigned int dx, unsigned int dy, float opacity, unsigned int flags);
void draw_layer(tmx_map *map, tmx_layer *layer);
void draw_image_layer(tmx_image *image);
void draw_all_layers(tmx_map *map, tmx_layer *layers);
void render_map(tmx_map *map);

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

  // BEGIN - TILED TEST LOADING MAP
  tmx_img_load_func = Allegro5_tex_loader;
	tmx_img_free_func = (void (*)(void*))al_destroy_bitmap;
  tmx_map *map = tmx_load("tiles/mapa.tmx");

  if (map == NULL) {
		tmx_perror("Não foi possível carregar o mapa");
		return 1;
	} else {
    std::cout << "Mapa carregado com sucesso" << std::endl;
  }
  // END - TILED TEST LOADING MAP

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
      paint(map, delta_time);
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

void paint(tmx_map *map, double delta_time) {
  al_clear_to_color(al_map_rgb(0, 0, 0));

  render_map(map);

  // BEGIN - simple 8 directions controller test
  bool moving = false;
  double vx = 0.5;
  double vy = 0.5;

  if(key[ALLEGRO_KEY_LEFT] && key[ALLEGRO_KEY_UP]) {
    character.current_animation = "walk_up_left";
    character.direction = "up_left";
    character.x -= vx;
    character.y -= vy;
    moving = true;
  } else if (key[ALLEGRO_KEY_LEFT] && key[ALLEGRO_KEY_DOWN]) {
    character.current_animation = "walk_down_left";
    character.direction = "down_left";
    character.x -= vx;
    character.y += vy;
    moving = true;
  } else if(key[ALLEGRO_KEY_RIGHT] && key[ALLEGRO_KEY_UP]) {
    character.current_animation = "walk_up_right";
    character.direction = "up_right";
    character.x += vx;
    character.y -= vy;
    moving = true;
  } else if (key[ALLEGRO_KEY_RIGHT] && key[ALLEGRO_KEY_DOWN]) {
    character.current_animation = "walk_down_right";
    character.direction = "down_right";
    character.x += vx;
    character.y += vy;
    moving = true;
  } else if(key[ALLEGRO_KEY_UP]) {
    character.current_animation = "walk_up";
    character.direction = "up";
    character.y -= vy;
    moving = true;
  } else if(key[ALLEGRO_KEY_LEFT]) {
    character.current_animation = "walk_left";
    character.direction = "left";
    character.x -= vx;
    moving = true;
  } else if(key[ALLEGRO_KEY_DOWN]) {
    character.current_animation = "walk_down";
    character.direction = "down";
    character.y += vy;
    moving = true;
  } else if(key[ALLEGRO_KEY_RIGHT]) {
    character.current_animation = "walk_right";
    character.direction = "right";
    character.x += vx;
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

// tiled functions
void* Allegro5_tex_loader(const char *path) {
	ALLEGRO_BITMAP *res    = NULL;
	ALLEGRO_PATH   *alpath = NULL;

	if (!(alpath = al_create_path(path))) return NULL;

	al_set_new_bitmap_format(ALLEGRO_PIXEL_FORMAT_ANY_WITH_ALPHA);
	res = al_load_bitmap(al_path_cstr(alpath, ALLEGRO_NATIVE_PATH_SEP));

	al_destroy_path(alpath);

	return (void*)res;
}

ALLEGRO_COLOR int_to_al_color(int color) {
	tmx_col_floats res = tmx_col_to_floats(color);
	return *((ALLEGRO_COLOR*)&res);
}

void draw_polyline(double **points, double x, double y, int pointsc, ALLEGRO_COLOR color) {
	int i;
	for (i=1; i<pointsc; i++) {
		al_draw_line(x+points[i-1][0], y+points[i-1][1], x+points[i][0], y+points[i][1], color, LINE_THICKNESS);
	}
}

void draw_polygone(double **points, double x, double y, int pointsc, ALLEGRO_COLOR color) {
	draw_polyline(points, x, y, pointsc, color);
	if (pointsc > 2) {
		al_draw_line(x+points[0][0], y+points[0][1], x+points[pointsc-1][0], y+points[pointsc-1][1], color, LINE_THICKNESS);
	}
}

void draw_objects(tmx_object_group *objgr) {
	ALLEGRO_COLOR color = int_to_al_color(objgr->color);
	tmx_object *head = objgr->head;
	while (head) {
		if (head->visible) {
			if (head->obj_type == OT_SQUARE) {
				al_draw_rectangle(head->x, head->y, head->x+head->width, head->y+head->height, color, LINE_THICKNESS);
			}
			else if (head->obj_type  == OT_POLYGON) {
				draw_polygone(head->content.shape->points, head->x, head->y, head->content.shape->points_len, color);
			}
			else if (head->obj_type == OT_POLYLINE) {
				draw_polyline(head->content.shape->points, head->x, head->y, head->content.shape->points_len, color);
			}
			else if (head->obj_type == OT_ELLIPSE) {
				al_draw_ellipse(head->x + head->width/2.0, head->y + head->height/2.0, head->width/2.0, head->height/2.0, color, LINE_THICKNESS);
			}
		}
		head = head->next;
	}
}

void draw_tile(void *image, unsigned int sx, unsigned int sy, unsigned int sw, unsigned int sh,
               unsigned int dx, unsigned int dy, float opacity, unsigned int flags) {
	ALLEGRO_COLOR colour = al_map_rgba_f(opacity, opacity, opacity, opacity);
	al_draw_tinted_bitmap_region((ALLEGRO_BITMAP*)image, colour, sx, sy, sw, sh, dx, dy, flags);
}

void draw_layer(tmx_map *map, tmx_layer *layer) {
	unsigned long i, j;
	unsigned int gid, x, y, w, h, flags;
	float op;
	tmx_tileset *ts;
	tmx_image *im;
	void* image;
	op = layer->opacity;
	for (i=0; i<map->height; i++) {
		for (j=0; j<map->width; j++) {
			gid = (layer->content.gids[(i*map->width)+j]) & TMX_FLIP_BITS_REMOVAL;
			if (map->tiles[gid] != NULL) {
				ts = map->tiles[gid]->tileset;
				im = map->tiles[gid]->image;
				x  = map->tiles[gid]->ul_x;
				y  = map->tiles[gid]->ul_y;
				w  = ts->tile_width;
				h  = ts->tile_height;
				if (im) {
					image = im->resource_image;
				}
				else {
					image = ts->image->resource_image;
				}
				flags = (layer->content.gids[(i*map->width)+j]) & ~TMX_FLIP_BITS_REMOVAL;
				draw_tile(image, x, y, w, h, j*ts->tile_width, i*ts->tile_height, op, flags);
			}
		}
	}
}

void draw_image_layer(tmx_image *image) {
	ALLEGRO_BITMAP *bitmap = (ALLEGRO_BITMAP*)image->resource_image;
	al_draw_bitmap(bitmap, 0, 0, 0);
}

void draw_all_layers(tmx_map *map, tmx_layer *layers) {
	while (layers) {
		if (layers->visible) {

			if (layers->type == L_GROUP) {
				draw_all_layers(map, layers->content.group_head); // recursive call
			}
			else if (layers->type == L_OBJGR) {
				draw_objects(layers->content.objgr);
			}
			else if (layers->type == L_IMAGE) {
				draw_image_layer(layers->content.image);
			}
			else if (layers->type == L_LAYER) {
				draw_layer(map, layers);
			}
		}
		layers = layers->next;
	}
}

void render_map(tmx_map *map) {
	al_clear_to_color(int_to_al_color(map->backgroundcolor));
	draw_all_layers(map, map->ly_head);
}