#pragma once

#include <expected>
#include <filesystem>
#include <string>

#include "project.hpp"

namespace myrts::editor {

/// Makes a folder somebody can be given to play the game: the game program renamed after the game, and the game's
/// files in a `game` folder next to it (the program looks for `game/game.json` beside itself).
///
///   <out>/<id>/<id>            the program (`<id>.exe` on Windows)
///   <out>/<id>/game/...        the game package
///
/// Fails, writing nothing, if the game has problems (see Project::report; it is checked again on disk first, so save
/// before). Returns the folder made. An existing folder of that name is replaced.
std::expected<std::filesystem::path, std::string> export_game(Project& project, const std::filesystem::path& game_program,
                                                               const std::filesystem::path& out_dir);

}  // namespace myrts::editor
