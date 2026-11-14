#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "grid.hpp"

namespace myrts {

/// What a player knows about a tile: never seen (black), seen before (the terrain stays on the map, but units out of sight
/// are not shown), or in sight of one of their units or buildings right now.
enum class Seen : std::uint8_t { Unknown = 0, Explored = 1, Visible = 2 };

/// One player's view of the map. Call begin() and then reveal() for everything that can see, once per refresh.
class FogOfWar {
 public:
  FogOfWar() = default;
  FogOfWar(int width, int height) : width_(width), height_(height), cells_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {}

  [[nodiscard]] int width() const { return width_; }
  [[nodiscard]] int height() const { return height_; }
  [[nodiscard]] bool empty() const { return cells_.empty(); }

  /// Starts a refresh: what was in sight becomes "explored" until it is revealed again.
  void begin();
  /// Ends a refresh (after the reveal() calls): bumps version() if something new was discovered.
  void end() {
    if (changed_) ++version_;
    changed_ = false;
  }
  /// Reveals the tiles within `radius` tiles of `centre` (a disc: sight does not stop at walls, as in Warcraft II).
  void reveal(Point centre, int radius);
  /// Everything visible at once (for a game played without fog).
  void reveal_all();

  [[nodiscard]] Seen at(Point p) const { return in_bounds(p) ? static_cast<Seen>(cells_[index(p)]) : Seen::Unknown; }
  [[nodiscard]] bool visible(Point p) const { return at(p) == Seen::Visible; }
  [[nodiscard]] bool explored(Point p) const { return at(p) != Seen::Unknown; }
  /// True if any tile of the `size` x `size` square at `origin` is visible / explored.
  [[nodiscard]] bool any_visible(Point origin, int size) const;
  [[nodiscard]] bool any_explored(Point origin, int size) const;

  /// Increases whenever a tile became known that was not (or the map was reset), so a view knows when to redraw its black
  /// areas. Moving in and out of sight does not change it.
  [[nodiscard]] std::uint32_t version() const { return version_; }

  /// A compact text form of what has been explored (run-length encoded), for saved games.
  [[nodiscard]] std::string encode() const;
  /// Restores what encode() produced; false (and no change) if it does not fit this map.
  bool decode(std::string_view text);

 private:
  [[nodiscard]] bool in_bounds(Point p) const { return p.x >= 0 && p.y >= 0 && p.x < width_ && p.y < height_; }
  [[nodiscard]] std::size_t index(Point p) const { return static_cast<std::size_t>(p.y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(p.x); }

  int width_ = 0;
  int height_ = 0;
  std::vector<std::uint8_t> cells_;
  std::uint32_t version_ = 0;
  bool changed_ = false;
};

}  // namespace myrts
