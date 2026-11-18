#include "schema.hpp"

namespace myrts::editor {

namespace {
using Json = Project::Json;

Field field(std::string key, std::string label, FieldType type, std::string help = {}) {
  Field f;
  f.key = std::move(key);
  f.label = std::move(label);
  f.type = type;
  f.help = std::move(help);
  return f;
}
Field whole(std::string key, std::string label, int def, int lo, int hi, std::string help = {}) {
  Field f = field(std::move(key), std::move(label), FieldType::Int, std::move(help));
  f.default_value = def;
  f.min = lo;
  f.max = hi;
  return f;
}
Field real(std::string key, std::string label, double def, double lo, double hi, std::string help = {}) {
  Field f = field(std::move(key), std::move(label), FieldType::Float, std::move(help));
  f.default_value = def;
  f.min = lo;
  f.max = hi;
  return f;
}
Field flag(std::string key, std::string label, bool def, std::string help = {}) {
  Field f = field(std::move(key), std::move(label), FieldType::Bool, std::move(help));
  f.default_value = def;
  return f;
}
Field color(std::string key, std::string label, Json def, std::string help = {}) {
  Field f = field(std::move(key), std::move(label), FieldType::Color, std::move(help));
  f.default_value = std::move(def);
  return f;
}
Field reference(std::string key, std::string label, std::string source, std::string help = {}) {
  Field f = field(std::move(key), std::move(label), FieldType::Ref, std::move(help));
  f.source = std::move(source);
  return f;
}

std::vector<Field> objective_fields(bool victory) {
  Field type = field("type", "Type", FieldType::Choice);
  type.options = victory ? std::vector<std::string>{"destroy_all", "destroy_building", "survive", "collect", "build"}
                         : std::vector<std::string>{"lose_building", "time_limit"};
  type.default_value = type.options[0];
  Field building = reference("building", "Building", "buildings");
  building.only_key = "type";
  building.only_values = {"destroy_building", "build", "lose_building"};
  Field resource = reference("resource", "Resource", "resources");
  resource.only_key = "type";
  resource.only_values = {"collect"};
  Field amount = whole("amount", "Amount", 1, 1, 1000000, "How many to destroy, build or collect.");
  amount.only_key = "type";
  amount.only_values = {"destroy_building", "collect", "build"};
  Field seconds = real("seconds", "Seconds", 0, 0, 100000);
  seconds.only_key = "type";
  seconds.only_values = {"survive", "time_limit"};
  return {type, building, resource, amount, seconds, field("text", "Text shown", FieldType::Text, "The line in the objectives list (empty: a default one).")};
}

std::vector<Field> build_level_schema() {
  std::vector<Field> f;
  f.push_back(field("name", "Name", FieldType::Text));
  f.push_back(field("chapter", "Chapter", FieldType::Text, "Levels with the same chapter are grouped in the level list of the game."));
  Field kind = field("kind", "Kind", FieldType::Choice, "land: bases connect on foot. air: they do not. naval: war at sea.");
  kind.options = {"land", "air", "naval"};
  kind.default_value = "land";
  f.push_back(kind);
  f.push_back(field("map", "Map", FieldType::File, "A .mrmap or .pud file."));
  f.push_back(field("music", "Music", FieldType::File));
  f.push_back(reference("player_faction", "Your faction", "factions", "Empty: the first faction of the game."));
  f.push_back(reference("enemy_faction", "Enemy faction", "factions", "Empty: the second faction of the game."));
  f.push_back(field("briefing", "Briefing", FieldType::MultiText));
  f.push_back(field("hints", "Hints", FieldType::TextList, "Shown in the first minutes of the level."));
  Field allow = field("allow", "Allowed", FieldType::RefList, "The units and buildings you can make. None listed: everything.");
  allow.source = "units_buildings";
  f.push_back(allow);
  f.push_back(field("player_stock", "Your stock", FieldType::Amounts, "Resources you start with."));
  f.push_back(field("enemy_stock", "Enemy stock", FieldType::Amounts, "Resources the enemy starts with."));
  f.push_back(whole("enemy_workers", "Enemy workers", 4, 0, 50));

  Field profile = reference("ai_profile", "AI profile", "ai_profiles", "A named set of AI settings (ai_profiles.json); the values below change single settings of it.");
  f.push_back(profile);
  Field ai = field("ai", "AI settings", FieldType::Object);
  ai.fields = {real("start_delay", "Start delay (s)", 150, 0, 100000, "No attack before this many seconds."),
               whole("first_wave", "First wave", 5, 1, 500, "Fighters needed for the first attack."),
               whole("wave_growth", "Wave growth", 3, 0, 500, "Extra fighters for each later attack."),
               whole("desired_workers", "Workers", 3, 0, 50),
               whole("desired_tankers", "Tankers", 2, 0, 50),
               real("think_interval", "Think interval (s)", 1.0, 0.1, 60),
               whole("farm_margin", "Farm margin", 2, 0, 50, "Builds a farm when this little food is left.")};
  f.push_back(ai);

  Field fog = field("fog", "Fog of war", FieldType::Object);
  fog.fields = {flag("enabled", "Enabled", true),
                flag("reveal_map", "Reveal the map", false, "No black: the whole map is seen (the fog of explored areas stays unless off)."),
                flag("remember", "Remember explored", true),
                real("sight_scale", "Sight scale", 1.0, 0, 20, "Multiplies every unit's sight."),
                whole("sight_bonus", "Sight bonus", 0, -20, 40, "Tiles added to every unit's sight."),
                whole("min_unit_sight", "Min unit sight", -1, -1, 40, "-1: the game's rule."),
                whole("building_sight_base", "Building sight base", -1, -1, 40, "-1: the game's rule."),
                real("update_interval", "Update interval (s)", 0.2, 0.02, 5),
                flag("minimap", "Show on minimap", true),
                color("unknown_color", "Unknown color", Json::array({0, 0, 0})),
                real("unknown_opacity", "Unknown opacity", 1.0, 0, 1),
                color("explored_color", "Explored color", Json::array({102, 102, 110})),
                real("explored_opacity", "Explored opacity", 0.55, 0, 1),
                real("edge_softness", "Edge softness", 1.0, 0.1, 5)};
  f.push_back(fog);

  Field objectives = field("objectives", "Objectives", FieldType::Object, "Empty: destroy every enemy unit and building to win.");
  Field victory = field("victory", "Victory", FieldType::ObjectList, "Win when all of these are met.");
  victory.fields = objective_fields(true);
  Field defeat = field("defeat", "Defeat", FieldType::ObjectList, "Lose when any of these happens.");
  defeat.fields = objective_fields(false);
  objectives.fields = {victory, defeat};
  f.push_back(objectives);

  Field more = field("more_opponents", "More opponents", FieldType::ObjectList, "Extra computer players.");
  more.fields = {reference("faction", "Faction", "factions"), reference("ai_profile", "AI profile", "ai_profiles"),
                 field("stock", "Stock", FieldType::Amounts), whole("workers", "Workers", 4, 0, 50)};
  f.push_back(more);
  return f;
}

std::vector<std::string> ids_of(const Project& project, const std::string& file, const char* array) {
  std::vector<std::string> ids;
  const Json* doc = project.document(file);
  if (doc && doc->contains(array))
    for (const Json& item : (*doc)[array])
      if (item.contains("id")) ids.push_back(item["id"].get<std::string>());
  return ids;
}
}  // namespace

const std::vector<Field>& level_schema() {
  static const std::vector<Field> schema = build_level_schema();
  return schema;
}

std::vector<std::string> reference_options(const Project& project, const std::string& source) {
  const std::string data = project.package().data_dir + "/";
  if (source == "factions") return ids_of(project, data + "factions.json", "factions");
  if (source == "units") return ids_of(project, data + "units.json", "units");
  if (source == "buildings") return ids_of(project, data + "buildings.json", "buildings");
  if (source == "commands") return ids_of(project, data + "commands.json", "commands");
  if (source == "resources") {
    if (!project.document(data + "resources.json")) return {"gold", "lumber", "oil"};  // the game's built-in set
    return ids_of(project, data + "resources.json", "resources");
  }
  if (source == "units_buildings") {
    auto ids = ids_of(project, data + "units.json", "units");
    for (auto& id : ids_of(project, data + "buildings.json", "buildings")) ids.push_back(std::move(id));
    return ids;
  }
  if (source == "ai_profiles") {
    std::vector<std::string> ids;
    if (const Json* doc = project.document(project.package().ai_profiles_file); doc && doc->contains("profiles"))
      for (const auto& [name, body] : (*doc)["profiles"].items()) ids.push_back(name);
    return ids;
  }
  return {};
}

}  // namespace myrts::editor
