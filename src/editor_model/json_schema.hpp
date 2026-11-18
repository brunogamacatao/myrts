#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "schema.hpp"
#include "sprite.hpp"

namespace myrts::editor {

/// The editor's field list as a JSON Schema (draft 2020-12), so other tools (an IDE, a validator) can check the game's
/// data files. `title` names the document; `item` makes it the schema of an entry of a table (a unit, a building...)
/// that carries an `id` too.
[[nodiscard]] nlohmann::json json_schema(const std::vector<Field>& fields, const std::string& title, bool item = false);

/// Every schema the editor has, by file name ("level.schema.json", "unit.schema.json"...).
[[nodiscard]] std::vector<std::pair<std::string, nlohmann::json>> all_json_schemas();

}  // namespace myrts::editor
