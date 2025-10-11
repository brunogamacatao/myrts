#include "tilemap.hpp"
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

// collision grid functions
// helper: returns true if property exists and is truthy
static bool prop_is_true(tmx_properties *props, const char *name) {
  if (!props || !name) return false;
  tmx_property *p = tmx_get_property(props, name); // <-- correct API
  if (!p) return false;

  if (p->type == PT_BOOL) {
    return p->value.boolean != 0;
  } else if (p->type == PT_STRING && p->value.string) {
    // consider "true" (case-insensitive) as true
    std::string s = p->value.string;
    for (auto &c : s) c = static_cast<char>(std::tolower(c));
    return s == "true" || s == "1";
  } else if (p->type == PT_INT) {
    return p->value.integer != 0;
  }
  // other types (file, color, etc.) are treated as false here
  return false;
}

std::vector<std::vector<int>> get_collision_grid(tmx_map *map) {
  std::vector<std::vector<int>> grid;
  if (!map) return grid;

  // initialize grid (0 = walkable)
  grid.assign(map->height, std::vector<int>(map->width, 0));

  // For convenience, treat any layer that has layer property "collision"=true
  // as fully blocking. You can remove this if you only want tile-based properties.
  for (tmx_layer *layer = map->ly_head; layer; layer = layer->next) {
    if (layer->type != L_LAYER) continue;

    const bool layer_is_collision = prop_is_true(layer->properties, "collision");

    for (unsigned int y = 0; y < map->height; ++y) {
      for (unsigned int x = 0; x < map->width; ++x) {
        // get cell (may include flip bits)
        int cell = layer->content.gids[y * map->width + x];
        unsigned int gid = (unsigned int)(cell & TMX_FLIP_BITS_REMOVAL);

        if (gid == 0) {
          // empty tile
          continue;
        }

        // If the entire layer is flagged as collision, mark it blocked
        if (layer_is_collision) {
          grid[y][x] = 1;
          continue;
        }

        // get tmx_tile pointer for this gid (map->tiles is GID indexed)
        tmx_tile *tile = nullptr;
        if (gid < map->tilecount) {
          tile = map->tiles[gid];
        } else {
          // safe fallback: try to index anyway (some lib versions store up to highest gid)
          tile = map->tiles[gid];
        }

        if (!tile) continue;

        // check multiple tile-level properties that should block movement
        if (prop_is_true(tile->properties, "wall") ||
            prop_is_true(tile->properties, "water") ||
            prop_is_true(tile->properties, "tree")) {
          grid[y][x] = 1;
        } else {
          grid[y][x] = 0;
        }
      }
    }
  }

  return grid;
}