#include "tilemap.hpp"

#include <algorithm>
#include <cctype>
#include <string>

void init_tilemap_engine() {
  tmx_img_load_func = Allegro5_tex_loader;
  tmx_img_free_func = (void (*)(void*))al_destroy_bitmap;
}

void destroy_map(tmx_map *map) {
  tmx_map_free(map);
}

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
	return al_map_rgba_f(res.r, res.g, res.b, res.a);
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

/// Draws every layer; the caller is responsible for clearing the target first.
void render_map(tmx_map *map) {
	draw_all_layers(map, map->ly_head);
}

// collision grid functions
// helper: returns true if property exists and is truthy
static bool prop_is_true(tmx_properties *props, const char *name) {
  if (!props || !name) return false;
  tmx_property *p = tmx_get_property(props, name);
  if (!p) return false;

  switch (p->type) {
    case PT_BOOL:
      return p->value.boolean != 0;
    case PT_INT:
      return p->value.integer != 0;
    case PT_STRING:
      if (p->value.string) {
        std::string s = p->value.string;
        std::ranges::transform(s, s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s == "true" || s == "1";
      }
      return false;
    default:
      return false;  // other types (file, color, etc.) are treated as false
  }
}

static void mark_blocked_cells(const tmx_map *map, const tmx_layer *layers, myrts::Grid &grid) {
  for (const tmx_layer *layer = layers; layer; layer = layer->next) {
    if (layer->type == L_GROUP) {
      mark_blocked_cells(map, layer->content.group_head, grid);
      continue;
    }
    if (layer->type != L_LAYER) continue;

    // A layer flagged with the custom property "collision" blocks every non-empty cell.
    const bool layer_is_collision = prop_is_true(layer->properties, "collision");

    for (unsigned int y = 0; y < map->height; ++y) {
      for (unsigned int x = 0; x < map->width; ++x) {
        const unsigned int gid = layer->content.gids[y * map->width + x] & TMX_FLIP_BITS_REMOVAL;
        if (gid == 0) continue;  // empty cell

        bool blocked = layer_is_collision;
        if (!blocked && gid < map->tilecount) {
          const tmx_tile *tile = map->tiles[gid];
          blocked = tile && (prop_is_true(tile->properties, "wall") ||
                             prop_is_true(tile->properties, "water") ||
                             prop_is_true(tile->properties, "tree"));
        }
        // Only ever add obstacles: a later layer must not unblock a cell blocked by an earlier one.
        if (blocked) grid.set_blocked({static_cast<int>(x), static_cast<int>(y)});
      }
    }
  }
}

myrts::Grid get_collision_grid(const tmx_map *map) {
  if (!map) return {};
  myrts::Grid grid(static_cast<int>(map->width), static_cast<int>(map->height));
  mark_blocked_cells(map, map->ly_head, grid);
  return grid;
}
