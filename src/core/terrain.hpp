#pragma once

#include <cstdint>
#include <optional>

namespace myrts {

// Warcraft II draws its maps from 32x32 "map tiles". Every map tile has a 16-bit code (the values of the MTXM
// section of a .pud) and is built from FOUR terrain quadrants:
//
//        tl | tr          A tile whose four quadrants have the same terrain is a *solid* tile (code 0x00t0,
//        ---+---          t = terrain). Otherwise it is a *boundary* tile between exactly two terrains: the code
//        bl | br          is 0x0B00 | (mask << 4) | variation, where B names the pair of terrains (water/dirt,
//                         dirt/grass, grass/trees...), `mask` says which quadrants hold the "inner" terrain of the
//                         pair and `variation` picks one of several drawings of the same look.
//
// Neighbouring tiles share quadrants (they all sit on a grid of vertices), which is what makes forests and coasts
// get ragged, correct borders instead of square blocks. The movement rules per tile (what land units may walk on)
// are also a function of the four quadrants.
//
// Fresh C++ implementation of the scheme documented by the Warcraft II modding community and used by the
// open-source editors war2edit / warcraft2-map-editor (see NOTICE.md).

enum class Terrain : std::uint8_t {
  WaterLight,
  WaterDark,
  DirtLight,
  DirtDark,
  GrassLight,
  GrassDark,
  Trees,
  Rocks,
  HumanWall,
  OrcWall,
};

struct Quad {
  Terrain tl{};
  Terrain tr{};
  Terrain bl{};
  Terrain br{};

  constexpr bool operator==(const Quad&) const = default;
};

/// Movement codes, as stored in the SQM section of a .pud.
namespace movement {
inline constexpr std::uint16_t kBridge = 0x0000;
inline constexpr std::uint16_t kLand = 0x0001;
inline constexpr std::uint16_t kCoastCorner = 0x0002;
inline constexpr std::uint16_t kDirt = 0x0011;
inline constexpr std::uint16_t kWater = 0x0040;
inline constexpr std::uint16_t kOrcWall = 0x0089;
inline constexpr std::uint16_t kForest = 0x0081;  // trees and rocks
inline constexpr std::uint16_t kCoast = 0x0082;
inline constexpr std::uint16_t kHumanWall = 0x008d;
}  // namespace movement

/// Whether `terrain` is one of the wall kinds.
[[nodiscard]] constexpr bool is_wall(Terrain t) { return t == Terrain::HumanWall || t == Terrain::OrcWall; }
[[nodiscard]] constexpr bool is_water(Terrain t) { return t == Terrain::WaterLight || t == Terrain::WaterDark; }

/// The four quadrants of the tile with this code (`nullopt` for a code that is not a Warcraft II tile).
/// Wall tiles come back with wall quadrants (whether a wall segment is open or closed is not tracked).
[[nodiscard]] std::optional<Quad> decompose(std::uint16_t code);

/// The code (variation 0) of the tile whose quadrants are `q`; `nullopt` when no such tile exists (more than two
/// terrains, or two that never meet, such as water and grass), and for walls. Pick the final variation from the
/// tileset (see TileSet::pick).
[[nodiscard]] std::optional<std::uint16_t> compose(Quad q);

/// SQM movement code of a tile.
[[nodiscard]] std::uint16_t movement_of(Quad q);

/// Land units can walk here (bridge, open land, dirt and the corner of a coast).
[[nodiscard]] constexpr bool land_passable(std::uint16_t mov) {
  return mov == movement::kBridge || mov == movement::kLand || mov == movement::kDirt || mov == movement::kCoastCorner;
}
/// Ships can sail here.
[[nodiscard]] constexpr bool sailable(std::uint16_t mov) { return mov == movement::kWater || mov == movement::kBridge; }

[[nodiscard]] constexpr bool has_trees(Quad q) {
  return q.tl == Terrain::Trees || q.tr == Terrain::Trees || q.bl == Terrain::Trees || q.br == Terrain::Trees;
}

}  // namespace myrts
