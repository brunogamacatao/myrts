#pragma once

#include <string>
#include <vector>

#include "editor_model/project.hpp"
#include "editor_model/schema.hpp"

namespace myrts::editor {

/// Draws the widgets for `fields` of the JSON object found at `base` in document `doc`, and applies every edit to the
/// project as an undoable step. Returns true if something was edited this frame.
bool draw_fields(Project& project, const std::string& doc, const std::vector<Field>& fields, const Project::Json::json_pointer& base);

}  // namespace myrts::editor
