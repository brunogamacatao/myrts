#include "pud.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>

namespace myrts {

namespace {

std::uint16_t u16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }
std::uint32_t u32(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

}  // namespace

std::vector<PudUnit> PudMap::of_type(std::uint8_t type) const {
  std::vector<PudUnit> out;
  std::ranges::copy_if(units, std::back_inserter(out), [&](const PudUnit& u) { return u.type == type; });
  return out;
}

std::expected<PudMap, std::string> parse_pud(std::span<const std::uint8_t> bytes) {
  PudMap map;
  bool have_type = false, have_dim = false, have_mtxm = false, have_erax = false;
  std::span<const std::uint8_t> mtxm, sqm;

  std::size_t pos = 0;
  while (pos + 8 <= bytes.size()) {
    const std::string name(reinterpret_cast<const char*>(&bytes[pos]), 4);
    const std::size_t length = u32(&bytes[pos + 4]);
    pos += 8;
    if (length > bytes.size() - pos) return std::unexpected("pud: section '" + name + "' is cut short");
    const std::span<const std::uint8_t> data = bytes.subspan(pos, length);
    pos += length;

    if (name == "TYPE") {
      have_type = data.size() >= 8 && std::memcmp(data.data(), "WAR2 MAP", 8) == 0;
    } else if (name == "VER " && data.size() >= 2) {
      map.version = u16(data.data());
    } else if (name == "DESC") {
      map.description.assign(reinterpret_cast<const char*>(data.data()), strnlen(reinterpret_cast<const char*>(data.data()), data.size()));
    } else if (name == "OWNR") {
      for (std::size_t i = 0; i < std::min<std::size_t>(16, data.size()); ++i) map.controller[i] = data[i];
    } else if ((name == "ERA " || name == "ERAX") && data.size() >= 2) {
      // ERAX (when present) wins over ERA; values above 3 mean forest.
      if (name == "ERAX" || !have_erax) {
        const std::uint16_t v = u16(data.data());
        map.era = v <= 3 ? static_cast<Era>(v) : Era::Forest;
      }
      have_erax = have_erax || name == "ERAX";
    } else if (name == "DIM " && data.size() >= 4) {
      map.width = u16(data.data());
      map.height = u16(data.data() + 2);
      have_dim = true;
    } else if (name == "MTXM") {
      mtxm = data;
      have_mtxm = true;
    } else if (name == "SQM ") {
      sqm = data;
    } else if (name == "SGLD" || name == "SLBR") {
      auto& target = name == "SGLD" ? map.gold : map.lumber;
      for (std::size_t i = 0; i < std::min<std::size_t>(16, data.size() / 2); ++i) target[i] = u16(&data[i * 2]);
    } else if (name == "UNIT") {
      for (std::size_t i = 0; i + 8 <= data.size(); i += 8)
        map.units.push_back({u16(&data[i]), u16(&data[i + 2]), data[i + 4], data[i + 5], u16(&data[i + 6])});
    }
  }

  if (!have_type) return std::unexpected("pud: not a Warcraft II map (missing TYPE section)");
  if (!have_dim || map.width <= 0 || map.height <= 0 || map.width > 256 || map.height > 256)
    return std::unexpected("pud: missing or invalid map size");
  const std::size_t cells = static_cast<std::size_t>(map.width) * static_cast<std::size_t>(map.height);
  if (!have_mtxm || mtxm.size() < cells * 2) return std::unexpected("pud: the tile map (MTXM) is missing or too small");

  map.tiles.resize(cells);
  for (std::size_t i = 0; i < cells; ++i) map.tiles[i] = u16(&mtxm[i * 2]);
  if (sqm.size() >= cells * 2) {
    map.movement.resize(cells);
    for (std::size_t i = 0; i < cells; ++i) map.movement[i] = u16(&sqm[i * 2]);
  }
  return map;
}

std::expected<PudMap, std::string> load_pud(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return std::unexpected("cannot open " + path);
  const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  auto map = parse_pud(bytes);
  if (!map) return std::unexpected(path + ": " + map.error());
  return map;
}

}  // namespace myrts
