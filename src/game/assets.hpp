#pragma once

#include <expected>
#include <memory>
#include <string>

#include "core/resource_cache.hpp"
#include "raii.hpp"
#include "sprite.hpp"

/// Loads sprites and images once and shares them. The cache holds weak references only, so
/// whatever the current scene stops using is freed with that scene.
class Assets {
 public:
  std::expected<std::shared_ptr<const SpriteDef>, std::string> sprite(const std::string& path);
  std::expected<std::shared_ptr<const Bitmap>, std::string> bitmap(const std::string& path);

  /// Resources currently alive (for tests and leak checks).
  [[nodiscard]] std::size_t live_sprites() const { return sprites_.live(); }
  [[nodiscard]] std::size_t live_bitmaps() const { return bitmaps_.live(); }

 private:
  myrts::ResourceCache<SpriteDef> sprites_;
  myrts::ResourceCache<Bitmap> bitmaps_;
};
