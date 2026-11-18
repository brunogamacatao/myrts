#include "schema.hpp"

#include "core/defs.hpp"

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
Field file(std::string key, std::string label, std::string dir, std::string ext, std::string help = {}) {
  Field f = field(std::move(key), std::move(label), FieldType::File, std::move(help));
  f.file_dir = std::move(dir);
  f.file_ext = std::move(ext);
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

std::vector<Field> ai_fields() {
  return {real("start_delay", "Start delay (s)", 150, 0, 100000, "No attack before this many seconds."),
          whole("first_wave", "First wave", 5, 1, 500, "Fighters needed for the first attack."),
          whole("wave_growth", "Wave growth", 3, 0, 500, "Extra fighters for each later attack."),
          whole("desired_workers", "Workers", 3, 0, 50),
          whole("desired_tankers", "Tankers", 2, 0, 50),
          real("think_interval", "Think interval (s)", 1.0, 0.1, 60),
          whole("farm_margin", "Farm margin", 2, 0, 50, "Builds a farm when this little food is left.")};
}

std::vector<Field> fog_fields() {
  return {flag("enabled", "Enabled", true),
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
}

std::vector<Field> build_level_schema() {
  std::vector<Field> f;
  f.push_back(field("name", "Name", FieldType::Text));
  f.push_back(field("chapter", "Chapter", FieldType::Text, "Levels with the same chapter are grouped in the level list of the game."));
  Field kind = field("kind", "Kind", FieldType::Choice, "land: bases connect on foot. air: they do not. naval: war at sea.");
  kind.options = {"land", "air", "naval"};
  kind.default_value = "land";
  f.push_back(kind);
  f.push_back(file("map", "Map", "maps", ".mrmap,.pud", "A .mrmap or .pud file."));
  f.push_back(file("music", "Music", "music", ".ogg"));
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
  ai.fields = ai_fields();
  f.push_back(ai);

  Field fog = field("fog", "Fog of war", FieldType::Object);
  fog.fields = fog_fields();
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

Field text_field(std::string key, std::string label, std::string help = {}) { return field(std::move(key), std::move(label), FieldType::Text, std::move(help)); }
Field choice(std::string key, std::string label, std::vector<std::string> options, std::string help = {}) {
  Field f = field(std::move(key), std::move(label), FieldType::Choice, std::move(help));
  f.default_value = options.front();
  f.options = std::move(options);
  return f;
}
Field list_of(std::string key, std::string label, std::string source, std::string help = {}) {
  Field f = field(std::move(key), std::move(label), FieldType::RefList, std::move(help));
  f.source = std::move(source);
  return f;
}

std::vector<Field> build_unit_schema() {
  std::vector<Field> f;
  f.push_back(text_field("name", "Name"));
  f.push_back(reference("faction", "Faction", "factions"));
  f.push_back(whole("level", "Level", 1, 1, 99, "Shown on the unit's card."));
  f.push_back(whole("hp", "Hit points", 0, 1, 100000));
  f.push_back(real("move_speed", "Move speed", 64, 0, 2000, "Pixels per second."));
  f.push_back(choice("move_class", "Moves on", {"land", "water", "air"}));
  f.push_back(file("sprite", "Sprite", "sprites", ".json", "The sprite definition (edit it in the Sprites tab)."));
  f.push_back(whole("icon", "Icon", -1, -1, 9999, "Index in the icon sheet; -1: none."));
  Field stats = field("stats", "Stats", FieldType::Object);
  stats.fields = {whole("armor", "Armor", 0, 0, 1000, "Taken off each hit (a hit still does at least 1)."),
                  whole("damage_min", "Damage min", 0, 0, 10000),
                  whole("damage_max", "Damage max", 0, 0, 10000, "0: the unit cannot attack."),
                  whole("range", "Range", 0, 0, 30, "Tiles; 1 is melee."),
                  whole("sight", "Sight", 0, 0, 30, "Tiles."),
                  whole("speed", "Speed (shown)", 0, 0, 100, "Only the number shown on the unit's card.")};
  f.push_back(stats);
  f.push_back(real("attack_seconds", "Seconds per attack", 1.5, 0.05, 100));
  f.push_back(reference("projectile", "Projectile", "projectiles", "Empty: a melee attack."));
  f.push_back(real("siege_multiplier", "Damage vs buildings", 1.0, 0, 100));
  f.push_back(flag("aggressive", "Fights on its own", true, "Workers default to false: they keep working unless ordered."));
  f.push_back(flag("mechanical", "Mechanical", false, "Repaired by workers."));
  f.push_back(whole("capacity", "Carries", 0, 0, 50, "Units it can carry (transports)."));
  f.push_back(whole("food", "Food", 1, 0, 100, "Food the unit eats."));
  f.push_back(field("cost", "Cost", FieldType::Amounts));
  f.push_back(real("train_seconds", "Training time (s)", 0, 0, 10000));
  f.push_back(list_of("commands", "Commands", "commands", "The buttons it has, in order."));
  f.push_back(list_of("requires", "Requires", "buildings", "Buildings the player must have to train it."));
  Field worker = field("worker", "Worker", FieldType::Object, "Present: the unit gathers resources.");
  Field gather = field("gather", "Gathers", FieldType::ObjectMap, "What it gathers and how: amount per trip and seconds of work.");
  gather.source = "resources";
  gather.fields = {whole("per_trip", "Per trip", 100, 1, 100000), real("seconds", "Seconds", 2.5, 0, 1000)};
  worker.fields = {gather};
  f.push_back(worker);
  f.push_back(list_of("builds", "Builds", "buildings", "Needs the Worker block."));
  return f;
}

std::vector<Field> build_building_schema() {
  std::vector<Field> f;
  f.push_back(text_field("name", "Name"));
  f.push_back(reference("faction", "Faction", "factions"));
  f.push_back(whole("size", "Size (tiles)", 2, 1, 8, "The building covers size x size tiles."));
  f.push_back(whole("hp", "Hit points", 400, 1, 100000));
  f.push_back(whole("armor", "Armor", 0, 0, 1000));
  f.push_back(whole("food", "Food provided", 0, 0, 1000));
  f.push_back(field("cost", "Cost", FieldType::Amounts));
  f.push_back(real("build_seconds", "Build time (s)", 100, 1, 10000));
  f.push_back(list_of("accepts", "Accepts deliveries of", "resources", "A depot: workers bring these resources here."));
  f.push_back(choice("placement", "Placement", {"land", "coast", "water"}, "coast: next to open water (shipyards). water: on the sea (oil platforms)."));
  f.push_back(file("sprite", "Sprite", "buildings", ".png"));
  f.push_back(file("sprite_construction", "Sprite while building", "buildings", ".png"));
  f.push_back(whole("icon", "Icon", -1, -1, 9999));
  f.push_back(list_of("trains", "Trains", "units"));
  f.push_back(list_of("requires", "Requires", "buildings", "Buildings the player must have to build it."));
  return f;
}

Field object(std::string key, std::string label, std::vector<Field> fields, std::string help = {}) {
  Field f = field(std::move(key), std::move(label), FieldType::Object, std::move(help));
  f.fields = std::move(fields);
  return f;
}
Field texts(std::string key, std::string label, bool compact = false, std::string help = {}) {
  Field f = field(std::move(key), std::move(label), FieldType::TextList, std::move(help));
  f.compact = compact;
  return f;
}

std::vector<Field> build_manifest_schema() {
  std::vector<Field> f;
  f.push_back(text_field("id", "Id", "A short name for the game (letters, digits, '-')."));
  f.push_back(text_field("title", "Title", "The window title and the name of the game."));
  f.push_back(text_field("version", "Version"));
  f.push_back(text_field("default_language", "Default language", "The code of a language file in the strings folder (\"en\": the texts as written)."));
  f.push_back(object("paths", "Folders and files",
                     {text_field("data", "Data folder"), text_field("sounds", "Sounds folder"), text_field("cursors", "Cursors folder"),
                      text_field("campaign", "Campaign"), text_field("tilesets", "Tile sets"), text_field("sound_table", "Sound table"),
                      text_field("rules", "Rules"), text_field("ai_profiles", "AI profiles"), text_field("strings", "Languages folder"),
                      text_field("credits", "Credits"), file("fallback_map", "Fallback map", "maps", ".mrmap,.pud", "Played when the game has no campaign.")},
                     "Where things are, relative to the game's folder. Leave a path out to use the default."));
  f.push_back(object("music", "Music of the menus",
                     {file("intro", "Intro", "music", ".ogg"), file("menu", "Menu", "music", ".ogg"), file("victory", "Victory", "music", ".ogg"),
                      file("defeat", "Defeat", "music", ".ogg")}));
  return f;
}

std::vector<Field> build_rules_schema() {
  std::vector<Field> f;
  f.push_back(object("simulation", "Simulation",
                     {whole("lumber_per_tree", "Lumber per tree", 100, 1, 100000, "What a tree holds."), whole("max_queue", "Training queue", 5, 1, 50),
                      real("scan_interval", "Enemy scan interval (s)", 0.3, 0.01, 10), real("retry_delay", "Route retry delay (s)", 0.5, 0.01, 10)}));
  f.push_back(object("combat", "Combat",
                     {whole("min_damage", "Minimum damage", 1, 0, 1000, "A hit always does at least this much, whatever the armor."),
                      real("attack_flash", "Strike animation (s)", 0.45, 0, 10)}));
  f.push_back(object("repair", "Repair",
                     {real("cost_share", "Cost share", 0.25, 0, 10, "Mending from nothing to whole costs this share of the price."),
                      real("min_seconds", "Minimum time (s)", 20, 0.01, 10000)}));
  f.push_back(object("vision", "Vision",
                     {whole("min_unit_sight", "Unit sight at least", 2, 0, 40, "Tiles."), whole("building_sight_base", "Building sight base", 3, 0, 40, "A building sees base + its size.")}));
  f.push_back(object("match", "Match setup",
                     {whole("initial_workers", "Starting workers", 8, 0, 100), whole("mine_size", "Mine size (tiles)", 3, 1, 8),
                      whole("gold_per_value_unit", "Mine value unit", 2500, 1, 1000000, "A map's mine value is a multiple of this."),
                      whole("default_mine_gold", "Default mine content", 10000, 1, 10000000)}));
  f.push_back(object("ai", "Computer player", {whole("farm_margin", "Farm margin", 2, 0, 50)}));
  f.push_back(object("presentation", "Presentation",
                     {real("corpse_seconds", "Body stays (s)", 25, 0, 1000), real("corpse_fade_seconds", "Body fades (s)", 5, 0.01, 1000),
                      whole("max_corpses", "Most bodies", 60, 0, 10000), real("air_lift", "Flying height (px)", 14, 0, 200)}));
  f.push_back(object("fog", "Fog of war (a level may change it)", fog_fields()));
  return f;
}

std::vector<Field> build_resource_schema() {
  std::vector<Field> f;
  f.push_back(text_field("name", "Name", "Shown to the player."));
  f.push_back(text_field("short_label", "Short label", "One or two letters after a price."));
  f.push_back(file("icon", "Icon", "ui", ".png"));
  f.push_back(choice("source", "Source", {"site", "tree"}, "site: a mine or a patch on the map. tree: cut from trees (only one resource can be)."));
  f.push_back(choice("site_terrain", "Sites lie on", {"land", "water"}, "water: gathered by ships."));
  f.push_back(flag("enter_site", "The worker goes inside", false, "One at a time (gold mines)."));
  f.push_back(text_field("carry_animation", "Carry animation", "walk_<this>_<dir> in a sprite."));
  f.push_back(text_field("pud_source", "Warcraft II object", "gold_mine or oil_patch: the map object that holds it in a .pud."));
  f.push_back(file("site_sprite", "Site picture", "buildings", ".png", "{era} is replaced by the tile set's name."));
  f.push_back(file("site_sprite_active", "Site picture, being worked", "buildings", ".png"));
  f.push_back(text_field("no_depot_message", "No depot message"));
  f.push_back(text_field("unreachable_message", "Unreachable message"));
  f.push_back(text_field("depleted_message", "Depleted message"));
  f.push_back(text_field("exhausted_message", "Exhausted message"));
  return f;
}

std::vector<Field> build_faction_schema() {
  std::vector<Field> f;
  f.push_back(text_field("name", "Name"));
  f.push_back(reference("worker", "Worker", "units", "The unit that gathers and builds; a level starts with some."));
  f.push_back(reference("town_hall", "Town hall", "buildings", "The building a base starts with."));
  f.push_back(list_of("base_buildings", "Base buildings", "buildings", "Buildings the computer player builds first."));
  return f;
}

std::vector<Field> build_command_schema() {
  std::vector<Field> f;
  f.push_back(text_field("label", "Label", "The button's text."));
  Field order = field("order", "Order", FieldType::Choice, "What it does. Empty: the same as the id.");
  for (const std::string_view o : kBuiltinOrders) order.options.emplace_back(o);
  f.push_back(order);
  f.push_back(text_field("prompt", "Prompt", "Shown while the player picks the target."));
  f.push_back(text_field("icon", "Icon name"));
  f.push_back(text_field("hotkey", "Hotkey"));
  f.push_back(choice("targeting", "Targeting", {"none", "ground", "resource", "menu"}, "none: acts at once. ground: next click picks a tile. resource: a tree or mine. menu: opens a menu (Build)."));
  f.push_back(flag("implemented", "Implemented", true, "Off: the button is shown but does nothing yet."));
  Field icons = field("icons", "Icons", FieldType::Amounts, "Icon number per faction (\"default\" for the rest).");
  icons.source = "factions";
  icons.options = {"default"};
  f.push_back(icons);
  return f;
}

std::vector<Field> build_context_rules_schema() {
  Field rules = field("context_orders", "Right-click rules", FieldType::ObjectList,
                      "The first rule that fits is used. A rule with no condition always fits (put it last). Absent: the game's usual rules.");
  Field when = field("when", "When", FieldType::RefList, "All of these must hold.");
  when.source = "conditions";
  rules.fields = {when, reference("command", "Give the command", "commands")};
  return {rules};
}

std::vector<Field> build_projectile_schema() {
  std::vector<Field> f;
  f.push_back(choice("shape", "Shape", {"arrow", "bolt", "axe", "boulder", "fire", "magic", "coil"}, "How it is drawn. Empty: the same as the id."));
  f.push_back(real("speed", "Speed", 400, 1, 5000, "Pixels per second."));
  f.push_back(real("arc_max", "Highest arc", 0, 0, 500, "How high it rises, in pixels (0: straight). Default depends on the shape."));
  f.push_back(real("arc_ratio", "Arc, share of distance", 0.08, 0, 5));
  f.push_back(real("min_seconds", "Shortest flight (s)", 0.12, 0, 60));
  f.push_back(real("max_seconds", "Longest flight (s)", 1.2, 0, 60));
  return f;
}

std::vector<Field> build_ai_profiles_schema() {
  Field profiles = field("profiles", "Profiles", FieldType::ObjectMap, "Named sets of computer-player settings; a level picks one by name.");
  profiles.free_keys = true;
  profiles.fields = ai_fields();
  return {profiles};
}

std::vector<Field> build_sounds_schema() {
  const auto events = [](bool with_attack) {
    std::vector<Field> e = {texts("selected", "Selected", true), texts("acknowledge", "Acknowledge", true), texts("annoyed", "Annoyed", true),
                            texts("ready", "Ready", true), texts("help", "Help", true), texts("dead", "Dead", true), texts("work_done", "Work done", true)};
    if (with_attack) e.push_back(texts("attack", "Attack", true));
    return e;
  };
  std::vector<Field> f;
  Field voices = field("voices", "Voices", FieldType::ObjectMap, "Sets of sounds that units share.");
  voices.free_keys = true;
  voices.fields = events(false);
  voices.fields.insert(voices.fields.begin(), reference("base", "Takes what is missing from", "voices"));
  f.push_back(voices);
  Field units = field("units", "Units", FieldType::ObjectMap, "Sounds of each unit: its voice and anything of its own.");
  units.source = "units";
  units.fields = events(true);
  units.fields.insert(units.fields.begin(), reference("voice", "Voice", "voices"));
  f.push_back(units);
  Field buildings = field("buildings", "Buildings", FieldType::ObjectMap);
  buildings.source = "buildings";
  buildings.fields = {texts("selected", "Selected", true)};
  f.push_back(buildings);
  Field effects = field("effects", "Effects", FieldType::ListMap, "Named sounds the game plays (construct, error, tree, building_destroyed).");
  effects.free_keys = true;
  effects.compact = true;
  f.push_back(effects);
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

#define MYRTS_SCHEMA(name, build) \
  const std::vector<Field>& name() { \
    static const std::vector<Field> schema = build(); \
    return schema; \
  }
MYRTS_SCHEMA(manifest_schema, build_manifest_schema)
MYRTS_SCHEMA(rules_schema, build_rules_schema)
MYRTS_SCHEMA(resource_schema, build_resource_schema)
MYRTS_SCHEMA(faction_schema, build_faction_schema)
MYRTS_SCHEMA(command_schema, build_command_schema)
MYRTS_SCHEMA(context_rules_schema, build_context_rules_schema)
MYRTS_SCHEMA(projectile_schema, build_projectile_schema)
MYRTS_SCHEMA(ai_profiles_schema, build_ai_profiles_schema)
MYRTS_SCHEMA(sounds_schema, build_sounds_schema)
#undef MYRTS_SCHEMA

const std::vector<Field>& unit_schema() {
  static const std::vector<Field> schema = build_unit_schema();
  return schema;
}

const std::vector<Field>& building_schema() {
  static const std::vector<Field> schema = build_building_schema();
  return schema;
}

const std::vector<Field>& level_schema() {
  static const std::vector<Field> schema = build_level_schema();
  return schema;
}

std::vector<std::string> reference_options(const Project& project, const std::string& source) {
  const std::string data = project.package().data_dir + "/";
  if (source == "factions") return ids_of(project, data + "factions.json", "factions");
  if (source == "units") return ids_of(project, data + "units.json", "units");
  if (source == "buildings") return ids_of(project, data + "buildings.json", "buildings");
  if (source.starts_with("animations:")) {
    std::vector<std::string> names;
    if (const Json* doc = project.document(source.substr(11)); doc && doc->contains("animations"))
      for (const Json& a : (*doc)["animations"]) names.push_back(a.value("name", std::string{}));
    return names;
  }
  if (source == "conditions") return {std::begin(kContextConditions), std::end(kContextConditions)};
  if (source == "voices") {
    std::vector<std::string> names;
    if (const Json* doc = project.document(project.package().sound_table_file); doc && doc->contains("voices"))
      for (const auto& [name, body] : (*doc)["voices"].items()) names.push_back(name);
    return names;
  }
  if (source == "projectiles") return ids_of(project, data + "projectiles.json", "projectiles");
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
