#include <allegro5/allegro5.h>
#include <allegro5/allegro_image.h>
#include <map>
#include <vector>
#include <string>

// types
typedef struct {
  int x;
  int y;
  int width;
  int height;
} s_frame;

typedef struct {
  std::vector<s_frame> frames;
  int current_frame;
  int n_frames;
  double delay;
  double elapsed_time;
  bool flip_horizontal;
} s_animation;

typedef struct {
  ALLEGRO_BITMAP* spritesheet;
  double x;
  double y;
  std::string direction;
  std::string current_animation;
  std::map<std::string, s_animation> animations;
} sprite;

void draw_sprite(sprite& sprite, double delta_time);
sprite load_sprite_from_json(const std::string& filename);
