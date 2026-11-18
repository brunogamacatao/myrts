#pragma once

#include <expected>
#include <string>
#include <vector>

#include "project.hpp"
#include "schema.hpp"

namespace myrts::editor {

struct Rect {
  int x = 0, y = 0, w = 0, h = 0;
  bool operator==(const Rect&) const = default;
};

/// Editing sprite definitions (`sprites/<name>.json`). Each function works on the document of the sprite named by its
/// path in the package, which must be open in the project (`Project::open_document`), and is one undoable step.
namespace sprite {

/// The sprite definitions of the game: every `.json` under the `sprites` folder, as package-relative paths.
[[nodiscard]] std::vector<std::string> list(const Project& project);

/// Imports `image` (a PNG, say) into `sprites/<name>.<ext>` and starts a definition for it: one animation named
/// "idle_up" showing the whole picture (`width` x `height`). Returns the definition's path ("sprites/<name>.json").
/// Fails if the name is empty or taken. Not undoable (it makes files).
std::expected<std::string, std::string> create(Project& project, const std::filesystem::path& image, const std::string& name, int width, int height);

[[nodiscard]] std::size_t animation_count(const Project& project, const std::string& doc);
[[nodiscard]] std::string animation_name(const Project& project, const std::string& doc, std::size_t index);
[[nodiscard]] std::vector<Rect> frames(const Project& project, const std::string& doc, std::size_t animation);

/// Adds an animation with one frame (the whole of the first frame of the first animation, or 16x16). Returns its index.
std::size_t add_animation(Project& project, const std::string& doc, const std::string& name);
std::size_t duplicate_animation(Project& project, const std::string& doc, std::size_t index);
/// Removes an animation. The last one stays. If it was the default, the first one becomes the default.
bool remove_animation(Project& project, const std::string& doc, std::size_t index);
/// Renames and keeps everything that points at the old name (the default animation, the states) pointing at it.
std::expected<void, std::string> rename_animation(Project& project, const std::string& doc, std::size_t index, const std::string& new_name);

std::size_t add_frame(Project& project, const std::string& doc, std::size_t animation, const Rect& frame);
bool remove_frame(Project& project, const std::string& doc, std::size_t animation, std::size_t frame);
bool move_frame(Project& project, const std::string& doc, std::size_t animation, std::size_t from, std::size_t to);
/// Replaces the frames of an animation.
bool set_frames(Project& project, const std::string& doc, std::size_t animation, const std::vector<Rect>& frames);

/// The cells of a grid over a spritesheet, left to right then top to bottom: `columns` x `rows` cells of `cell_w` x
/// `cell_h`, the first at (`x`, `y`), `gap` pixels apart.
[[nodiscard]] std::vector<Rect> grid_cells(int x, int y, int cell_w, int cell_h, int columns, int rows, int gap = 0);

/// The fields of one animation, and of the sprite itself, for the inspector.
[[nodiscard]] const std::vector<Field>& animation_schema();
[[nodiscard]] std::vector<Field> sprite_schema(const std::string& doc);

}  // namespace sprite
}  // namespace myrts::editor
