#include "tileset.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>

namespace myrts {

TileSet::TileSet(std::string name, std::string image, std::unordered_map<std::uint16_t, TileSpot> spots)
    : name_(std::move(name)), image_(std::move(image)), spots_(std::move(spots)) {
  for (const auto& [code, _] : spots_) variants_[static_cast<std::uint16_t>(code & 0xfff0)].push_back(code);
  for (auto& [_, v] : variants_) std::ranges::sort(v);
}

std::optional<TileSpot> TileSet::spot(std::uint16_t code) const {
  const auto it = spots_.find(code);
  if (it == spots_.end()) return std::nullopt;
  return it->second;
}

const std::vector<std::uint16_t>& TileSet::variants(std::uint16_t base) const {
  static const std::vector<std::uint16_t> kNone;
  const auto it = variants_.find(static_cast<std::uint16_t>(base & 0xfff0));
  return it == variants_.end() ? kNone : it->second;
}

std::optional<std::uint16_t> TileSet::pick(std::uint16_t base, std::uint32_t seed) const {
  const auto& all = variants(base);
  if (all.empty()) return std::nullopt;

  // Solid dirt and grass have decorated variations (nibble 4 and up); keep to the plain ones.
  const std::uint16_t group = base & 0x0ff0;
  const bool plain_only = group >= 0x0030 && group <= 0x0060;
  if (plain_only) {
    std::vector<std::uint16_t> plain;
    std::ranges::copy_if(all, std::back_inserter(plain), [](std::uint16_t c) { return (c & 0xf) <= 2; });
    if (!plain.empty()) return plain[seed % plain.size()];
  }
  return all[seed % all.size()];
}

std::expected<TileSets, std::string> TileSets::parse(std::string_view json_text) {
  using json = nlohmann::json;
  TileSets out;
  try {
    const json doc = json::parse(json_text);
    out.tile_size_ = doc.value("tile_size", 32);
    const auto& eras = doc.at("eras");
    if (eras.size() != out.sets_.size()) return std::unexpected("tilesets: expected 4 eras");
    for (std::size_t i = 0; i < out.sets_.size(); ++i) {
      std::unordered_map<std::uint16_t, TileSpot> spots;
      for (const auto& t : eras[i].at("tiles"))
        spots[t.at(0).get<std::uint16_t>()] = {t.at(1).get<int>(), t.at(2).get<int>()};
      out.sets_[i] = TileSet(eras[i].at("name").get<std::string>(), eras[i].at("image").get<std::string>(), std::move(spots));
    }
  } catch (const std::exception& e) {
    return std::unexpected(std::string("invalid tilesets: ") + e.what());
  }
  return out;
}

std::expected<TileSets, std::string> TileSets::load_file(const std::string& path) {
  std::ifstream f(path);
  if (!f) return std::unexpected("cannot open " + path);
  const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  return parse(text);
}

}  // namespace myrts
