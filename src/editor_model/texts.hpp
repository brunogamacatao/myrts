#pragma once

#include <string>
#include <vector>

#include "project.hpp"

namespace myrts::editor {

/// The language files of a game (`strings/<language>.json`): `{"language": "pt", "strings": {"English text": "Texto"}}`.
/// The English text is the key; a text with no entry is shown as written.
namespace texts {

/// The language files of the game, as document names ("strings/pt.json"), opened in the project.
[[nodiscard]] std::vector<std::string> languages(Project& project);
/// Makes a new, empty language file for `code` ("es"). Returns its document name. Not undoable.
std::expected<std::string, std::string> create_language(Project& project, const std::string& code);

[[nodiscard]] std::size_t count(const Project& project, const std::string& doc);
[[nodiscard]] std::string get(const Project& project, const std::string& doc, const std::string& key);
bool set(Project& project, const std::string& doc, const std::string& key, const std::string& value);
bool remove(Project& project, const std::string& doc, const std::string& key);

/// The texts the game's data puts in front of the player: names of units, buildings, resources and factions, command
/// labels and prompts, and the chapter, name, briefing and hints of each level.
[[nodiscard]] std::vector<std::string> data_texts(const Project& project);
/// The texts of the data that `doc` has no entry for.
[[nodiscard]] std::vector<std::string> missing(const Project& project, const std::string& doc);
/// Adds an entry (the text itself as the translation, to be changed) for every text in `keys` that has none: one undoable step.
std::size_t add_entries(Project& project, const std::string& doc, const std::vector<std::string>& keys);
/// The keys of the entries whose translation is empty or the same as the key.
[[nodiscard]] std::vector<std::string> untranslated(const Project& project, const std::string& doc);

}  // namespace texts
}  // namespace myrts::editor
