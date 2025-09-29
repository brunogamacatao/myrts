#include <allegro5/allegro5.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_primitives.h>
#include <tmx.h>

#define LINE_THICKNESS 2.5

void init_tilemap_engine();
void destroy_map(tmx_map *map);
void* Allegro5_tex_loader(const char *path);
ALLEGRO_COLOR int_to_al_color(int color);
void draw_polyline(double **points, double x, double y, int pointsc, ALLEGRO_COLOR color);
void draw_polygone(double **points, double x, double y, int pointsc, ALLEGRO_COLOR color);
void draw_objects(tmx_object_group *objgr);
void draw_tile(void *image, unsigned int sx, unsigned int sy, unsigned int sw, 
               unsigned int sh, unsigned int dx, unsigned int dy, float opacity, 
               unsigned int flags);
void draw_layer(tmx_map *map, tmx_layer *layer);
void draw_image_layer(tmx_image *image);
void draw_all_layers(tmx_map *map, tmx_layer *layers);
void render_map(tmx_map *map);
