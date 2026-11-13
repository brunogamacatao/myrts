#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "tileset.hpp"

namespace myrts {

/// A unit or building placed on a map (the UNIT section). Coordinates are the top-left tile.
struct PudUnit {
  int x = 0;
  int y = 0;
  std::uint8_t type = 0;
  std::uint8_t owner = 0;
  std::uint16_t value = 0;  // gold mines and oil patches: the amount in units of 2500
};

/// Unit types the game cares about when reading a map.
namespace pud_unit {
inline constexpr std::uint8_t kCritter = 57;
inline constexpr std::uint8_t kTownHall = 74;
inline constexpr std::uint8_t kGreatHall = 75;
inline constexpr std::uint8_t kGoldMine = 92;
inline constexpr std::uint8_t kOilPatch = 93;
inline constexpr std::uint8_t kHumanStart = 94;
inline constexpr std::uint8_t kOrcStart = 95;
}  // namespace pud_unit

/// The parts of a Warcraft II map file (.pud) this game uses. A .pud is a list of sections, each a four-letter
/// name, a 32-bit length and the data; the tile codes (MTXM) and the movement map (SQM) hold one 16-bit value per tile.
struct PudMap {
  std::string description;
  int version = 0;
  Era era = Era::Forest;
  int width = 0;
  int height = 0;
  std::vector<std::uint16_t> tiles;     // MTXM, row-major
  std::vector<std::uint16_t> movement;  // SQM, row-major
  std::vector<PudUnit> units;
  std::array<std::uint16_t, 16> gold{};    // starting gold per player (SGLD)
  std::array<std::uint16_t, 16> lumber{};  // SLBR
  std::array<std::uint8_t, 16> controller{};  // OWNR: who plays each slot (5 human, 4 computer, 3 nobody...)

  [[nodiscard]] std::vector<PudUnit> of_type(std::uint8_t type) const;
};

std::expected<PudMap, std::string> parse_pud(std::span<const std::uint8_t> bytes);
std::expected<PudMap, std::string> load_pud(const std::string& path);

}  // namespace myrts
