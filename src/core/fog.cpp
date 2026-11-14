#include "fog.hpp"

#include <algorithm>
#include <charconv>

namespace myrts {

void FogOfWar::begin() {
  for (auto& c : cells_)
    if (c == static_cast<std::uint8_t>(Seen::Visible)) c = static_cast<std::uint8_t>(Seen::Explored);
}

void FogOfWar::reveal(Point centre, int radius) {
  const int r2 = radius * radius + radius;  // a slightly rounder disc than r*r
  for (int dy = -radius; dy <= radius; ++dy)
    for (int dx = -radius; dx <= radius; ++dx) {
      if (dx * dx + dy * dy > r2) continue;
      const Point p{centre.x + dx, centre.y + dy};
      if (!in_bounds(p)) continue;
      auto& c = cells_[index(p)];
      if (c != static_cast<std::uint8_t>(Seen::Visible)) {
        if (c == static_cast<std::uint8_t>(Seen::Unknown)) changed_ = true;
        c = static_cast<std::uint8_t>(Seen::Visible);
      }
    }
}

void FogOfWar::reveal_all() {
  std::fill(cells_.begin(), cells_.end(), static_cast<std::uint8_t>(Seen::Visible));
  ++version_;
}

bool FogOfWar::any_visible(Point origin, int size) const {
  for (int dy = 0; dy < size; ++dy)
    for (int dx = 0; dx < size; ++dx)
      if (visible({origin.x + dx, origin.y + dy})) return true;
  return false;
}

bool FogOfWar::any_explored(Point origin, int size) const {
  for (int dy = 0; dy < size; ++dy)
    for (int dx = 0; dx < size; ++dx)
      if (explored({origin.x + dx, origin.y + dy})) return true;
  return false;
}

std::string FogOfWar::encode() const {
  // "<count><U|E>" runs, where E is any tile that is not unknown (what is in sight now is saved as explored).
  std::string out;
  std::size_t i = 0;
  while (i < cells_.size()) {
    const bool known = cells_[i] != 0;
    std::size_t j = i;
    while (j < cells_.size() && (cells_[j] != 0) == known) ++j;
    out += std::to_string(j - i);
    out += known ? 'E' : 'U';
    i = j;
  }
  return out;
}

bool FogOfWar::decode(std::string_view text) {
  std::vector<std::uint8_t> cells;
  cells.reserve(cells_.size());
  std::size_t pos = 0;
  while (pos < text.size()) {
    std::size_t count = 0;
    const auto [ptr, ec] = std::from_chars(text.data() + pos, text.data() + text.size(), count);
    if (ec != std::errc{} || ptr == text.data() + text.size()) return false;
    const char kind = *ptr;
    if (kind != 'E' && kind != 'U') return false;
    if (cells.size() + count > cells_.size()) return false;
    cells.insert(cells.end(), count, kind == 'E' ? static_cast<std::uint8_t>(Seen::Explored) : static_cast<std::uint8_t>(Seen::Unknown));
    pos = static_cast<std::size_t>(ptr - text.data()) + 1;
  }
  if (cells.size() != cells_.size()) return false;
  cells_ = std::move(cells);
  ++version_;
  return true;
}

}  // namespace myrts
