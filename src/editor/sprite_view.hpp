#pragma once

#include <string>

#include "editor_model/project.hpp"
#include "textures.hpp"

namespace myrts::editor {

/// Draws a picture of what the game file `relative` shows: a sprite definition (.json: the first frame of its default
/// animation) or a picture (.png), scaled down to fit a `box` x `box` square. Returns false (drawing a blank) if it
/// cannot be shown.
bool draw_preview(const Project& project, Textures& textures, const std::string& relative, float box);

}  // namespace myrts::editor
