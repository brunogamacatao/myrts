#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace myrts {

/// A tile coordinate (map units, not pixels). Ordered so it can key a std::map.
struct Point {
  int x{};
  int y{};

  constexpr auto operator<=>(const Point&) const = default;
};

/// Rectangular grid of walkable/blocked cells (row-major).
class Grid {
 public:
  Grid() = default;
  Grid(int width, int height) : width_(width), height_(height),
      blocked_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0) {}

  [[nodiscard]] int width() const noexcept { return width_; }
  [[nodiscard]] int height() const noexcept { return height_; }
  [[nodiscard]] bool empty() const noexcept { return blocked_.empty(); }

  [[nodiscard]] constexpr bool contains(Point p) const noexcept {
    return p.x >= 0 && p.y >= 0 && p.x < width_ && p.y < height_;
  }

  [[nodiscard]] std::size_t index(Point p) const noexcept {
    return static_cast<std::size_t>(p.y) * static_cast<std::size_t>(width_) +
           static_cast<std::size_t>(p.x);
  }

  /// Out-of-bounds cells are considered blocked.
  [[nodiscard]] bool is_blocked(Point p) const noexcept {
    return !contains(p) || blocked_[index(p)] != 0;
  }
  [[nodiscard]] bool is_walkable(Point p) const noexcept { return !is_blocked(p); }

  void set_blocked(Point p, bool blocked = true) noexcept {
    if (contains(p)) blocked_[index(p)] = blocked ? 1 : 0;
  }

 private:
  int width_{0};
  int height_{0};
  std::vector<std::uint8_t> blocked_;
};

}  // namespace myrts
