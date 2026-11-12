#pragma once

#include <optional>
#include <span>
#include <vector>

#include "geometry.hpp"

namespace myrts {

struct SelectBox {
  int id;
  Rect box;
};

/// Topmost (last in the list) entity under the point, if any.
[[nodiscard]] inline std::optional<int> pick_at(std::span<const SelectBox> items, float x, float y) {
  for (auto it = items.rbegin(); it != items.rend(); ++it) {
    if (it->box.contains(x, y)) return it->id;
  }
  return std::nullopt;
}

/// All entities whose box overlaps the drag rectangle.
[[nodiscard]] inline std::vector<int> pick_in(std::span<const SelectBox> items, const Rect& area) {
  std::vector<int> ids;
  for (const auto& item : items) {
    if (item.box.intersects(area)) ids.push_back(item.id);
  }
  return ids;
}

}  // namespace myrts
