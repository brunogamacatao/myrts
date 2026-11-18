#include "json_schema.hpp"

namespace myrts::editor {

namespace {
using json = nlohmann::json;

json field_schema(const Field& f);

json properties_of(const std::vector<Field>& fields) {
  json props = json::object();
  for (const Field& f : fields) props[f.key] = field_schema(f);
  return props;
}

json field_schema(const Field& f) {
  json s = json::object();
  if (!f.label.empty()) s["title"] = f.label;
  if (!f.help.empty()) s["description"] = f.help;
  switch (f.type) {
    case FieldType::Text:
    case FieldType::MultiText:
    case FieldType::File:
    case FieldType::Ref: s["type"] = "string"; break;
    case FieldType::Int:
      s["type"] = "integer";
      s["minimum"] = static_cast<long long>(f.min);
      s["maximum"] = static_cast<long long>(f.max);
      break;
    case FieldType::Float:
      s["type"] = "number";
      s["minimum"] = f.min;
      s["maximum"] = f.max;
      break;
    case FieldType::Bool: s["type"] = "boolean"; break;
    case FieldType::Choice:
      s["type"] = "string";
      s["enum"] = f.options;
      break;
    case FieldType::Color:
      s["type"] = "array";
      s["items"] = {{"type", "integer"}, {"minimum", 0}, {"maximum", 255}};
      s["minItems"] = 3;
      s["maxItems"] = 3;
      break;
    case FieldType::TextList:
    case FieldType::RefList:
      s["type"] = "array";
      s["items"] = {{"type", "string"}};
      break;
    case FieldType::Amounts:
      s["type"] = "object";
      s["additionalProperties"] = {{"type", "integer"}, {"minimum", 0}};
      break;
    case FieldType::Object:
      s["type"] = "object";
      s["properties"] = properties_of(f.fields);
      s["additionalProperties"] = false;
      break;
    case FieldType::ObjectList:
      s["type"] = "array";
      s["items"] = {{"type", "object"}, {"properties", properties_of(f.fields)}};
      break;
    case FieldType::ObjectMap:
      s["type"] = "object";
      s["additionalProperties"] = {{"type", "object"}, {"properties", properties_of(f.fields)}};
      break;
    case FieldType::ListMap:
      s["type"] = "object";
      s["additionalProperties"] = {{"type", "array"}, {"items", {{"type", "string"}}}};
      break;
  }
  if (!f.default_value.is_null() && f.type != FieldType::Object) s["default"] = f.default_value;
  return s;
}
}  // namespace

json json_schema(const std::vector<Field>& fields, const std::string& title, bool item) {
  json s = {{"$schema", "https://json-schema.org/draft/2020-12/schema"}, {"title", title}, {"type", "object"}};
  s["properties"] = properties_of(fields);
  if (item) {
    s["properties"]["id"] = {{"type", "string"}, {"title", "Id"}, {"description", "Unique in its table; other files refer to it."}};
    s["required"] = json::array({"id"});
  }
  return s;
}

std::vector<std::pair<std::string, json>> all_json_schemas() {
  return {
      {"level.schema.json", json_schema(level_schema(), "A level of the campaign", false)},
      {"unit.schema.json", json_schema(unit_schema(), "A unit (units.json)", true)},
      {"building.schema.json", json_schema(building_schema(), "A building (buildings.json)", true)},
      {"resource.schema.json", json_schema(resource_schema(), "A resource (resources.json)", true)},
      {"faction.schema.json", json_schema(faction_schema(), "A faction (factions.json)", true)},
      {"command.schema.json", json_schema(command_schema(), "A command (commands.json)", true)},
      {"projectile.schema.json", json_schema(projectile_schema(), "A projectile (projectiles.json)", true)},
      {"game.schema.json", json_schema(manifest_schema(), "The manifest (game.json)")},
      {"rules.schema.json", json_schema(rules_schema(), "The rules (rules.json)")},
      {"ai_profiles.schema.json", json_schema(ai_profiles_schema(), "The AI profiles (ai_profiles.json)")},
      {"sounds.schema.json", json_schema(sounds_schema(), "The sound table (sounds.json)")},
      {"sprite.schema.json", json_schema(sprite::animation_schema(), "An animation of a sprite (sprites/*.json)")},
  };
}

}  // namespace myrts::editor
