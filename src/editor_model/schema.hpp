#pragma once

#include <string>
#include <vector>

#include "project.hpp"

namespace myrts::editor {

/// How a field of a JSON object is edited. The inspector draws a widget for each type; the schema is data, so the same
/// inspector serves levels now and units, buildings and the rest later.
enum class FieldType {
  Text,       ///< one line
  MultiText,  ///< several lines
  Int,
  Float,
  Bool,
  Choice,     ///< one of `options`
  Ref,        ///< the id of something in the game (`source`), picked from a list
  File,       ///< a path inside the game's folder
  Color,      ///< [r, g, b] 0-255
  TextList,   ///< array of strings
  RefList,    ///< array of ids (`source`)
  Amounts,    ///< {resource id: whole number}
  Object,     ///< nested object edited with `fields`
  ObjectList, ///< array of objects, each edited with `fields`
  ObjectMap   ///< {id of `source`: object edited with `fields`}: each id is switched on or off, and its object edited
};

struct Field {
  std::string key;
  std::string label;
  FieldType type = FieldType::Text;
  std::string help;
  double min = 0, max = 1000000;
  Project::Json default_value;          ///< what the game uses when the key is absent (null: nothing to show)
  std::vector<std::string> options;     ///< Choice
  std::string file_dir;                 ///< File: the folder (inside the game) the picker lists
  std::string file_ext;                 ///< File: the extensions the picker offers, ".png,.bmp" (empty: any)
  std::string source;                   ///< Ref, RefList: "factions", "units", "buildings", "units_buildings", "resources", "ai_profiles", "commands", "projectiles"
  std::vector<Field> fields;            ///< Object, ObjectList
  std::string only_key;                 ///< shown only when this sibling key ...
  std::vector<std::string> only_values; ///< ... has one of these values (empty: always shown)
};

/// The fields of one level of the campaign.
[[nodiscard]] const std::vector<Field>& level_schema();

/// The fields of a unit and of a building (the `id` is not among them: the collection editor owns it).
[[nodiscard]] const std::vector<Field>& unit_schema();
[[nodiscard]] const std::vector<Field>& building_schema();

/// The ids a `source` offers, in the order the game lists them.
[[nodiscard]] std::vector<std::string> reference_options(const Project& project, const std::string& source);

}  // namespace myrts::editor
