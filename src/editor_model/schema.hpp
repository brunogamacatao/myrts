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
  ObjectMap,  ///< {id of `source`: object edited with `fields`}: each id is switched on or off, and its object edited
              ///< (with `free_keys`: any names, added and removed by the user)
  ListMap     ///< {any name: [strings]}
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
  bool free_keys = false;               ///< ObjectMap, ListMap: the keys are names the user makes up
  bool compact = false;                 ///< TextList: one line per entry
  std::string only_key;                 ///< shown only when this sibling key ...
  std::vector<std::string> only_values; ///< ... has one of these values (empty: always shown)
};

/// The fields of one level of the campaign.
[[nodiscard]] const std::vector<Field>& level_schema();

/// The fields of a unit and of a building (the `id` is not among them: the collection editor owns it).
[[nodiscard]] const std::vector<Field>& unit_schema();
[[nodiscard]] const std::vector<Field>& building_schema();

/// The fields of the single-object data files (the path to each is "" for the whole document) and of the tables.
[[nodiscard]] const std::vector<Field>& manifest_schema();       ///< game.json
[[nodiscard]] const std::vector<Field>& rules_schema();          ///< rules.json
[[nodiscard]] const std::vector<Field>& resource_schema();       ///< one entry of resources.json
[[nodiscard]] const std::vector<Field>& faction_schema();        ///< one entry of factions.json
[[nodiscard]] const std::vector<Field>& command_schema();        ///< one entry of commands.json
[[nodiscard]] const std::vector<Field>& context_rules_schema();  ///< commands.json: the right-click rules
[[nodiscard]] const std::vector<Field>& projectile_schema();     ///< one entry of projectiles.json
[[nodiscard]] const std::vector<Field>& ai_profiles_schema();    ///< ai_profiles.json
[[nodiscard]] const std::vector<Field>& sounds_schema();         ///< sounds.json

/// The ids a `source` offers, in the order the game lists them.
[[nodiscard]] std::vector<std::string> reference_options(const Project& project, const std::string& source);

}  // namespace myrts::editor
