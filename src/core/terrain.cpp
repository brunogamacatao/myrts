#include "terrain.hpp"

#include <algorithm>
#include <array>

namespace myrts {

namespace {

// Pairs of terrains that can share a tile: the "outer" one, the "inner" one (the one the mask marks) and the
// code of the boundary group.
struct Pair {
  Terrain outer;
  Terrain inner;
  std::uint16_t group;
};
constexpr std::array<Pair, 7> kPairs{{
    {Terrain::GrassLight, Terrain::Trees, 0x0700},
    {Terrain::GrassLight, Terrain::GrassDark, 0x0600},
    {Terrain::GrassLight, Terrain::DirtLight, 0x0500},
    {Terrain::DirtLight, Terrain::Rocks, 0x0400},
    {Terrain::DirtLight, Terrain::DirtDark, 0x0300},
    {Terrain::DirtLight, Terrain::WaterLight, 0x0200},
    {Terrain::WaterLight, Terrain::WaterDark, 0x0100},
}};

// Terrain of a solid tile, by the middle digit of its code.
std::optional<Terrain> solid_terrain(std::uint16_t group) {
  switch (group) {
    case 0x0010: return Terrain::WaterLight;
    case 0x0020: return Terrain::WaterDark;
    case 0x0030: return Terrain::DirtLight;
    case 0x0040: return Terrain::DirtDark;
    case 0x0050: return Terrain::GrassLight;
    case 0x0060: return Terrain::GrassDark;
    case 0x0070: return Terrain::Trees;
    case 0x0080: return Terrain::Rocks;
    case 0x0090:
    case 0x00b0: return Terrain::HumanWall;
    case 0x00a0:
    case 0x00c0: return Terrain::OrcWall;
    default: return std::nullopt;
  }
}

constexpr std::uint16_t solid_code(Terrain t) {
  switch (t) {
    case Terrain::WaterLight: return 0x0010;
    case Terrain::WaterDark: return 0x0020;
    case Terrain::DirtLight: return 0x0030;
    case Terrain::DirtDark: return 0x0040;
    case Terrain::GrassLight: return 0x0050;
    case Terrain::GrassDark: return 0x0060;
    case Terrain::Trees: return 0x0070;
    case Terrain::Rocks: return 0x0080;
    default: return 0;
  }
}

// Bit of each quadrant in the mask of a boundary tile.
constexpr unsigned kTl = 1, kTr = 2, kBl = 4, kBr = 8;

}  // namespace

std::optional<Quad> decompose(std::uint16_t code) {
  const std::uint16_t group = code & 0x0f00;
  const std::uint16_t middle = code & 0x00f0;

  if (group == 0) {  // solid
    const auto t = solid_terrain(middle);
    if (!t) return std::nullopt;
    return Quad{*t, *t, *t, *t};
  }

  if (group == 0x0800 || group == 0x0900) {  // wall boundary: only "wall" matters to us
    const Terrain wall = group == 0x0800 ? Terrain::HumanWall : Terrain::OrcWall;
    return Quad{wall, wall, wall, wall};
  }

  // Boundary tile: the mask (stored minus one) tells which quadrants hold the inner terrain.
  const auto it = std::ranges::find(kPairs, group, &Pair::group);
  if (it == kPairs.end()) return std::nullopt;
  const unsigned mask = static_cast<unsigned>(middle >> 4) + 1;
  if (mask > 14) return std::nullopt;  // 15 would be a solid tile
  const auto pick = [&](unsigned bit) { return (mask & bit) ? it->inner : it->outer; };
  return Quad{pick(kTl), pick(kTr), pick(kBl), pick(kBr)};
}

std::optional<std::uint16_t> compose(Quad q) {
  if (is_wall(q.tl) || is_wall(q.tr) || is_wall(q.bl) || is_wall(q.br)) return std::nullopt;

  if (q.tl == q.tr && q.tl == q.bl && q.tl == q.br) return solid_code(q.tl);  // solid

  // Exactly two terrains, and a pair that exists.
  const std::array<Terrain, 4> quads{q.tl, q.tr, q.bl, q.br};
  for (const Pair& p : kPairs) {
    const bool only_pair = std::ranges::all_of(quads, [&](Terrain t) { return t == p.outer || t == p.inner; });
    if (!only_pair) continue;
    unsigned mask = 0;
    if (q.tl == p.inner) mask |= kTl;
    if (q.tr == p.inner) mask |= kTr;
    if (q.bl == p.inner) mask |= kBl;
    if (q.br == p.inner) mask |= kBr;
    return static_cast<std::uint16_t>(p.group | ((mask - 1) << 4));  // 1 <= mask <= 14 here
  }
  return std::nullopt;
}

std::uint16_t movement_of(Quad q) {
  const std::array<Terrain, 4> quads{q.tl, q.tr, q.bl, q.br};
  const auto count = [&](auto pred) { return std::ranges::count_if(quads, pred); };
  const auto any = [&](auto pred) { return count(pred) > 0; };
  const auto is = [](Terrain t) { return [t](Terrain x) { return x == t; }; };

  if (any(is_wall)) return any(is(Terrain::OrcWall)) ? movement::kOrcWall : movement::kHumanWall;

  const auto dirt_light = count(is(Terrain::DirtLight));
  const auto water_light = count(is(Terrain::WaterLight));
  // A coast corner: land with one corner in the water, or a checkerboard of two opposite water and land corners.
  const bool checkerboard = q.tl == q.br && q.tr == q.bl && q.tl != q.tr;
  if (dirt_light == 3 && water_light == 1) return movement::kCoastCorner;
  if (checkerboard && dirt_light == 2 && water_light == 2) return movement::kCoastCorner;
  if (dirt_light > 0 && water_light > 0) return movement::kCoast;
  if (any(is_water)) return movement::kWater;
  if (any(is(Terrain::Trees)) || any(is(Terrain::Rocks))) return movement::kForest;
  if (any(is(Terrain::DirtLight)) || any(is(Terrain::DirtDark))) return movement::kDirt;
  return movement::kLand;
}

}  // namespace myrts
