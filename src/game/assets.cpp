#include "assets.hpp"

std::expected<std::shared_ptr<const SpriteDef>, std::string> Assets::sprite(const std::string& path) {
  auto r = sprites_.get(path, [&]() -> myrts::ResourceCache<SpriteDef>::Result { return load_sprite_def(path); });
  if (!r) return std::unexpected(r.error());
  return std::shared_ptr<const SpriteDef>(std::move(*r));
}

std::expected<std::shared_ptr<const Bitmap>, std::string> Assets::bitmap(const std::string& path) {
  auto r = bitmaps_.get(path, [&]() -> myrts::ResourceCache<Bitmap>::Result {
    BitmapPtr bmp(al_load_bitmap(path.c_str()));
    if (!bmp) return std::unexpected("cannot load image " + path);
    return std::make_shared<Bitmap>(Bitmap{std::move(bmp)});
  });
  if (!r) return std::unexpected(r.error());
  return std::shared_ptr<const Bitmap>(std::move(*r));
}
