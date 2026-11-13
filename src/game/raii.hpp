#pragma once

#include <allegro5/allegro5.h>

#include <memory>

/// Owning handle for a bitmap, freed with al_destroy_bitmap.
struct BitmapDeleter {
  void operator()(ALLEGRO_BITMAP* b) const noexcept {
    if (b) al_destroy_bitmap(b);
  }
};
using BitmapPtr = std::unique_ptr<ALLEGRO_BITMAP, BitmapDeleter>;

/// A bitmap that can be shared between owners (the last one frees it).
struct Bitmap {
  BitmapPtr ptr;
  [[nodiscard]] ALLEGRO_BITMAP* get() const { return ptr.get(); }
};
