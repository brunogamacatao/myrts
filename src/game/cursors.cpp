#include "cursors.hpp"

#include <cstdio>
#include <fstream>
#include <nlohmann/json.hpp>

#include "raii.hpp"

namespace {
constexpr const char* kFiles[] = {"pointer",   "pointer_no", "target",    "target_enemy", "target_ok",
                                  "magnifier", "wait",       "scroll_n",  "scroll_ne",    "scroll_e",
                                  "scroll_se", "scroll_s",   "scroll_sw", "scroll_w",     "scroll_nw"};
static_assert(std::size(kFiles) == static_cast<std::size_t>(CursorKind::Count));
}  // namespace

bool Cursors::load(ALLEGRO_DISPLAY* display) {
  std::ifstream in("cursors/cursors.json");
  if (!in) return false;
  nlohmann::json hot;
  try {
    hot = nlohmann::json::parse(in);
  } catch (const std::exception&) {
    return false;
  }

  display_ = display;
  for (std::size_t i = 0; i < cursors_.size(); ++i) {
    const std::string name = kFiles[i];
    BitmapPtr bmp(al_load_bitmap(("cursors/" + name + ".png").c_str()));
    if (!bmp || !hot.contains(name)) {
      std::fprintf(stderr, "warning: cursor %s is missing\n", name.c_str());
      continue;
    }
    cursors_[i] = al_create_mouse_cursor(bmp.get(), hot[name].value("hot_x", 0), hot[name].value("hot_y", 0));
  }
  set(CursorKind::Pointer);
  return cursors_[0] != nullptr;
}

void Cursors::unload() {
  if (display_) al_set_system_mouse_cursor(display_, ALLEGRO_SYSTEM_MOUSE_CURSOR_DEFAULT);
  for (auto& c : cursors_) {
    if (c) al_destroy_mouse_cursor(c);
    c = nullptr;
  }
  display_ = nullptr;
  current_ = CursorKind::Count;
}

void Cursors::set(CursorKind kind) {
  if (!display_ || kind == current_) return;
  ALLEGRO_MOUSE_CURSOR* c = cursors_[static_cast<std::size_t>(kind)];
  if (!c) return;
  al_set_mouse_cursor(display_, c);
  current_ = kind;
}
