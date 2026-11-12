#pragma once

#include <algorithm>
#include <cmath>

namespace myrts {

struct Vec2 {
  float x{};
  float y{};
};

struct Rect {
  float x{};
  float y{};
  float w{};
  float h{};

  /// Rectangle spanning two arbitrary corner points (works for any drag direction).
  [[nodiscard]] static constexpr Rect from_corners(float x1, float y1, float x2, float y2) {
    const float left = std::min(x1, x2);
    const float top = std::min(y1, y2);
    return {left, top, std::max(x1, x2) - left, std::max(y1, y2) - top};
  }

  [[nodiscard]] constexpr bool contains(float px, float py) const {
    return px >= x && py >= y && px < x + w && py < y + h;
  }

  [[nodiscard]] constexpr bool intersects(const Rect& o) const {
    return x < o.x + o.w && o.x < x + w && y < o.y + o.h && o.y < y + h;
  }
};

/// Scrollable window over the world. Coordinates "in view" are relative to the viewport's top-left.
struct Camera {
  float x{};  // world position of the viewport's top-left corner
  float y{};
  float view_w{};
  float view_h{};
  float world_w{};
  float world_h{};

  /// Keeps the view inside the world (a world smaller than the view stays pinned at 0).
  void clamp() {
    x = std::clamp(x, 0.0f, std::max(0.0f, world_w - view_w));
    y = std::clamp(y, 0.0f, std::max(0.0f, world_h - view_h));
  }
  void move(float dx, float dy) {
    x += dx;
    y += dy;
    clamp();
  }
  void center_on(float wx, float wy) {
    x = wx - view_w / 2;
    y = wy - view_h / 2;
    clamp();
  }
  [[nodiscard]] constexpr Vec2 to_world(float vx, float vy) const { return {vx + x, vy + y}; }
  [[nodiscard]] constexpr Vec2 to_view(float wx, float wy) const { return {wx - x, wy - y}; }
  [[nodiscard]] constexpr Rect visible() const { return {x, y, view_w, view_h}; }
};

/// Maps world coordinates to a minimap area of `area_w` x `area_h` pixels, preserving aspect ratio
/// and centring the map inside the area. Coordinates are relative to the area's top-left.
struct Minimap {
  float world_w{};
  float world_h{};
  float area_w{};
  float area_h{};

  [[nodiscard]] float scale() const { return std::min(area_w / world_w, area_h / world_h); }
  [[nodiscard]] float width() const { return world_w * scale(); }
  [[nodiscard]] float height() const { return world_h * scale(); }
  [[nodiscard]] float offset_x() const { return (area_w - width()) / 2; }
  [[nodiscard]] float offset_y() const { return (area_h - height()) / 2; }

  [[nodiscard]] Vec2 world_to_mini(float wx, float wy) const {
    return {offset_x() + wx * scale(), offset_y() + wy * scale()};
  }
  /// Clamped to the world, so clicks on the letterbox margins map to the nearest edge.
  [[nodiscard]] Vec2 mini_to_world(float mx, float my) const {
    const float s = scale();
    return {std::clamp((mx - offset_x()) / s, 0.0f, world_w), std::clamp((my - offset_y()) / s, 0.0f, world_h)};
  }
};

}  // namespace myrts
