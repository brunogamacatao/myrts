#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

#include "game_rules.hpp"
#include "grid.hpp"
#include "pud.hpp"
#include "resources.hpp"
#include "scenario.hpp"
#include "tilemap.hpp"

namespace myrts {

/// A map as the engine uses it, whatever file it came from: terrain, where each player starts and what each mine holds.
/// The native file format is `.mrmap` (JSON); Warcraft II `.pud` maps are converted on load (see `load_map`).
struct MapFile {
  std::string name;
  std::string tileset;  ///< name of a tile set of the game ("forest")
  int width = 0;
  int height = 0;
  std::vector<std::uint16_t> tiles;     ///< tile codes (the codes of the game's tile sets), row-major
  std::vector<std::uint16_t> movement;  ///< optional: movement flags per tile (empty: derived from the tile codes)

  struct Start {
    int owner = 0;                ///< slot number: 0 is the human's
    Point at{};
    std::optional<Point> hall;    ///< where the map places that player's town hall (top-left), if it does
  };
  struct Mine {
    Point at{};                   ///< top-left tile
    std::string resource;         ///< resource id ("gold")
    int amount = 0;
  };
  std::vector<Start> starts;
  std::vector<Mine> mines;
};

/// Converts a Warcraft II map. Mines are given the game's resource for their kind (`ResourceDef::pud_source`); a map
/// object the game has no resource for is dropped. The map's `Era` is named by `sets` (the era's index).
[[nodiscard]] std::expected<MapFile, std::string> map_from_pud(const PudMap& pud, const TileSets& sets,
                                                               const ResourceTable& resources, const GameRules& rules);

/// Reads a `.mrmap` document. Fails naming what is wrong (size mismatch, unknown tile set, bad tile data...).
[[nodiscard]] std::expected<MapFile, std::string> parse_mrmap(const std::string& json_text, const TileSets& sets);
/// Writes the `.mrmap` form of `map` (tiles packed as base64 of little-endian 16-bit codes).
[[nodiscard]] std::string write_mrmap(const MapFile& map);

/// Reads `path`: a `.mrmap` as is, anything else as a Warcraft II `.pud`.
[[nodiscard]] std::expected<MapFile, std::string> load_map(const std::string& path, const TileSets& sets,
                                                           const ResourceTable& resources, const GameRules& rules);

/// The tile map (terrain) of `map`.
[[nodiscard]] std::expected<TileMap, std::string> tilemap_from(const MapFile& map, std::shared_ptr<const TileSets> sets);
/// Who starts where, and the mines.
[[nodiscard]] Scenario scenario_from(const MapFile& map);

}  // namespace myrts
