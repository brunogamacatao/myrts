#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace myrts {

/// The terrain style of a map (the ERA section of a .pud).
enum class Era : std::uint8_t { Forest, Winter, Wasteland, Swamp };

/// Pixel position of a 32x32 tile inside the tileset image.
struct TileSpot {
  int x = 0;
  int y = 0;
};

/// The tiles of one era: which tile codes exist and where each one is in the tileset image.
class TileSet {
 public:
  TileSet() = default;
  TileSet(std::string name, std::string image, std::unordered_map<std::uint16_t, TileSpot> spots);

  [[nodiscard]] const std::string& name() const { return name_; }
  /// Path of the tileset image, relative to the assets directory.
  [[nodiscard]] const std::string& image() const { return image_; }

  [[nodiscard]] bool has(std::uint16_t code) const { return spots_.contains(code); }
  [[nodiscard]] std::optional<TileSpot> spot(std::uint16_t code) const;
  [[nodiscard]] std::size_t size() const { return spots_.size(); }

  /// Every existing code with the same look as `base` (a code with variation 0): its variations.
  [[nodiscard]] const std::vector<std::uint16_t>& variants(std::uint16_t base) const;

  /// One variation of `base`, chosen by `seed`. Plain ground and grass avoid the decorated "special" variations
  /// (bones, rocks...), as the game's own generator does. `nullopt` if the era has no such tile.
  [[nodiscard]] std::optional<std::uint16_t> pick(std::uint16_t base, std::uint32_t seed) const;

 private:
  std::string name_;
  std::string image_;
  std::unordered_map<std::uint16_t, TileSpot> spots_;
  std::unordered_map<std::uint16_t, std::vector<std::uint16_t>> variants_;  // by base code
};

/// The tile sets of all four eras, loaded from assets/data/tilesets.json.
class TileSets {
 public:
  static std::expected<TileSets, std::string> parse(std::string_view json_text);
  static std::expected<TileSets, std::string> load_file(const std::string& path);

  [[nodiscard]] const TileSet& era(Era e) const { return sets_[static_cast<std::size_t>(e)]; }
  [[nodiscard]] int tile_size() const { return tile_size_; }

 private:
  std::array<TileSet, 4> sets_;
  int tile_size_ = 32;
};

/// A reproducible per-tile pseudo-random number (used to pick variations without a global RNG).
[[nodiscard]] constexpr std::uint32_t tile_hash(int x, int y, std::uint32_t seed) {
  std::uint32_t h = seed ^ (static_cast<std::uint32_t>(x) * 0x9E3779B1u) ^ (static_cast<std::uint32_t>(y) * 0x85EBCA6Bu);
  h ^= h >> 15;
  h *= 0x2C1B3C6Du;
  h ^= h >> 12;
  h *= 0x297A2D39u;
  h ^= h >> 15;
  return h;
}

}  // namespace myrts
