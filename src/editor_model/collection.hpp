#pragma once

#include <expected>
#include <optional>
#include <string>
#include <vector>

#include "project.hpp"

namespace myrts::editor {

/// An array of objects with ids inside a data file: the units, the buildings, the resources... `{"units": [...]}`.
struct Collection {
  std::string doc;    ///< "data/units.json"
  std::string array;  ///< "units"
};

/// The collection of a data table of this game (the file names come from the manifest's data folder).
[[nodiscard]] Collection units_of(const Project& project);
[[nodiscard]] Collection buildings_of(const Project& project);

[[nodiscard]] Collection resources_of(const Project& project);
[[nodiscard]] Collection factions_of(const Project& project);
[[nodiscard]] Collection commands_of(const Project& project);
[[nodiscard]] Collection projectiles_of(const Project& project);

namespace collection {

[[nodiscard]] std::size_t count(const Project& project, const Collection& c);
[[nodiscard]] const Project::Json* item(const Project& project, const Collection& c, std::size_t index);
[[nodiscard]] std::string id(const Project& project, const Collection& c, std::size_t index);
[[nodiscard]] std::optional<std::size_t> find(const Project& project, const Collection& c, const std::string& id);
/// `base` if no item has that id, else `base_2`, `base_3`...
[[nodiscard]] std::string unique_id(const Project& project, const Collection& c, const std::string& base);

/// Adds `value` (which has an "id") at the end. Returns its index. One undoable step.
std::size_t add(Project& project, const Collection& c, const Project::Json& value);
/// A copy of item `index` right after it, with a new unique id ("<id>_copy") and name. Returns the copy's index.
std::size_t duplicate(Project& project, const Collection& c, std::size_t index);
bool remove(Project& project, const Collection& c, std::size_t index);

/// Every place in the game's data that uses `id` (a string equal to it, or a key named so), as "file: path" lines.
/// The item's own "id" does not count. Language files are not searched (their keys are texts, not ids).
[[nodiscard]] std::vector<std::string> references(const Project& project, const std::string& id);

/// Gives item `index` a new id and updates every reference to the old one in the data files, as one undoable step.
/// Fails (changing nothing) if the id is empty, has spaces or characters other than letters, digits, `_` and `-`, or
/// is already taken in that collection.
std::expected<void, std::string> rename_id(Project& project, const Collection& c, std::size_t index, const std::string& new_id);

}  // namespace collection
}  // namespace myrts::editor
