#pragma once

#include <cstddef>
#include <string>

#include "project.hpp"

namespace myrts::editor {

/// Changes to the level list of the campaign document (`data/campaign.json`, `{"levels": [...]}`). Each is one undoable
/// step. They return false, changing nothing, when the request makes no sense.
namespace levels {

[[nodiscard]] std::size_t count(const Project& project);
[[nodiscard]] std::string name(const Project& project, std::size_t index);

/// Adds a level at the end (or after `after`). It starts from the settings of the level before it (same map and
/// music, so it is playable at once) with the given name. Returns its index.
std::size_t add(Project& project, const std::string& level_name, std::size_t after = static_cast<std::size_t>(-1));
/// A copy of level `index`, named "<name> (copy)", right after it. Returns the copy's index.
std::size_t duplicate(Project& project, std::size_t index);
/// Removes a level. A campaign always keeps at least one.
bool remove(Project& project, std::size_t index);
/// Moves level `from` so it ends up at position `to`.
bool move(Project& project, std::size_t from, std::size_t to);
bool rename(Project& project, std::size_t index, const std::string& new_name);

/// Where the level that was at `index` is after `move(from, to)`; for keeping the active level selected.
[[nodiscard]] std::size_t index_after_move(std::size_t index, std::size_t from, std::size_t to);

}  // namespace levels
}  // namespace myrts::editor
