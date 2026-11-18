#include "sprite.hpp"

#include <algorithm>

#include "core/sprite_doc.hpp"

namespace fs = std::filesystem;

namespace myrts::editor::sprite {

namespace {
using Json = Project::Json;

const Json* anims(const Project& p, const std::string& doc) {
  const Json* d = p.document(doc);
  return d && d->contains("animations") && (*d)["animations"].is_array() ? &(*d)["animations"] : nullptr;
}

Json frame_json(const Rect& r) { return Json{{"x", r.x}, {"y", r.y}, {"w", r.w}, {"h", r.h}}; }

std::string unique_name(const Project& p, const std::string& doc, const std::string& base) {
  const auto taken = [&](const std::string& n) {
    for (std::size_t i = 0; i < animation_count(p, doc); ++i)
      if (animation_name(p, doc, i) == n) return true;
    return false;
  };
  if (!taken(base)) return base;
  for (int n = 2;; ++n)
    if (const std::string c = base + "_" + std::to_string(n); !taken(c)) return c;
}
}  // namespace

std::vector<std::string> list(const Project& project) {
  std::vector<std::string> out;
  std::error_code ec;
  for (const auto& entry : fs::recursive_directory_iterator(project.package().path("sprites"), ec))
    if (entry.is_regular_file() && entry.path().extension() == ".json") out.push_back(fs::relative(entry.path(), project.root()).generic_string());
  std::ranges::sort(out);
  return out;
}

std::expected<std::string, std::string> create(Project& project, const fs::path& image, const std::string& name, int width, int height) {
  if (name.empty()) return std::unexpected("the sprite needs a name");
  for (const char c : name)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
      return std::unexpected("a name can only have letters, digits, '_' and '-'");
  if (width < 1 || height < 1) return std::unexpected("the image has no size");
  const std::string json_path = "sprites/" + name + ".json";
  const std::string image_path = "sprites/" + name + image.extension().generic_string();
  if (fs::exists(project.package().path(json_path)) || project.document(json_path)) return std::unexpected(json_path + " already exists");
  if (auto copied = project.import_file(image, image_path); !copied) return std::unexpected(copied.error());
  const Json doc = {{"spritesheet", image_path},
                    {"default_animation", "idle_up"},
                    {"animations", Json::array({Json{{"name", "idle_up"}, {"fps", 5}, {"frames", Json::array({frame_json({0, 0, width, height})})}}})}};
  if (!project.create_document(json_path, doc)) return std::unexpected("cannot create " + json_path);
  return json_path;
}

std::size_t animation_count(const Project& p, const std::string& doc) {
  const Json* a = anims(p, doc);
  return a ? a->size() : 0;
}

std::string animation_name(const Project& p, const std::string& doc, std::size_t index) {
  const Json* a = anims(p, doc);
  return a && index < a->size() ? (*a)[index].value("name", std::string{}) : std::string{};
}

std::vector<Rect> frames(const Project& p, const std::string& doc, std::size_t animation) {
  std::vector<Rect> out;
  const Json* a = anims(p, doc);
  if (!a || animation >= a->size() || !(*a)[animation].contains("frames")) return out;
  for (const Json& f : (*a)[animation]["frames"]) out.push_back({f.value("x", 0), f.value("y", 0), f.value("w", 0), f.value("h", 0)});
  return out;
}

std::size_t add_animation(Project& p, const std::string& doc, const std::string& name) {
  const std::size_t at = animation_count(p, doc);
  Rect first{0, 0, 16, 16};
  if (at > 0)
    if (const auto f = frames(p, doc, 0); !f.empty()) first = f[0];
  const std::string unique = unique_name(p, doc, name);
  p.change(doc, [&](Json& d) {
    d["animations"].push_back(Json{{"name", unique}, {"fps", 5}, {"frames", Json::array({frame_json(first)})}});
  });
  return at;
}

std::size_t duplicate_animation(Project& p, const std::string& doc, std::size_t index) {
  const Json* a = anims(p, doc);
  if (!a || index >= a->size()) return index;
  Json copy = (*a)[index];
  copy["name"] = unique_name(p, doc, copy.value("name", std::string("animation")) + "_copy");
  p.change(doc, [&](Json& d) { d["animations"].insert(d["animations"].begin() + static_cast<std::ptrdiff_t>(index + 1), copy); });
  return index + 1;
}

bool remove_animation(Project& p, const std::string& doc, std::size_t index) {
  if (animation_count(p, doc) <= 1 || index >= animation_count(p, doc)) return false;
  const std::string gone = animation_name(p, doc, index);
  return p.change(doc, [&](Json& d) {
    d["animations"].erase(d["animations"].begin() + static_cast<std::ptrdiff_t>(index));
    if (d.value("default_animation", std::string{}) == gone) d["default_animation"] = d["animations"][0].value("name", std::string{});
  });
}

std::expected<void, std::string> rename_animation(Project& p, const std::string& doc, std::size_t index, const std::string& new_name) {
  const std::string old = animation_name(p, doc, index);
  if (old.empty()) return std::unexpected("no such animation");
  if (new_name.empty()) return std::unexpected("the name cannot be empty");
  if (new_name == old) return {};
  for (std::size_t i = 0; i < animation_count(p, doc); ++i)
    if (animation_name(p, doc, i) == new_name) return std::unexpected("'" + new_name + "' is already an animation");
  p.change(doc, [&](Json& d) {
    d["animations"][index]["name"] = new_name;
    if (d.value("default_animation", std::string{}) == old) d["default_animation"] = new_name;
    if (d.contains("states") && d["states"].is_object())
      for (auto& [state, list] : d["states"].items())
        for (Json& entry : list)
          if (entry.is_string() && entry.get<std::string>() == old) entry = new_name;
  });
  return {};
}

std::size_t add_frame(Project& p, const std::string& doc, std::size_t animation, const Rect& frame) {
  const std::size_t at = frames(p, doc, animation).size();
  if (animation >= animation_count(p, doc)) return at;
  p.change(doc, [&](Json& d) { d["animations"][animation]["frames"].push_back(frame_json(frame)); });
  return at;
}

bool remove_frame(Project& p, const std::string& doc, std::size_t animation, std::size_t frame) {
  if (frame >= frames(p, doc, animation).size()) return false;
  return p.change(doc, [&](Json& d) {
    Json& list = d["animations"][animation]["frames"];
    list.erase(list.begin() + static_cast<std::ptrdiff_t>(frame));
  });
}

bool move_frame(Project& p, const std::string& doc, std::size_t animation, std::size_t from, std::size_t to) {
  const std::size_t n = frames(p, doc, animation).size();
  if (from >= n || to >= n || from == to) return false;
  return p.change(doc, [&](Json& d) {
    Json& list = d["animations"][animation]["frames"];
    Json moved = std::move(list[from]);
    list.erase(list.begin() + static_cast<std::ptrdiff_t>(from));
    list.insert(list.begin() + static_cast<std::ptrdiff_t>(to), std::move(moved));
  });
}

bool set_frames(Project& p, const std::string& doc, std::size_t animation, const std::vector<Rect>& rects) {
  if (animation >= animation_count(p, doc)) return false;
  return p.change(doc, [&](Json& d) {
    Json list = Json::array();
    for (const Rect& r : rects) list.push_back(frame_json(r));
    d["animations"][animation]["frames"] = std::move(list);
  });
}

std::vector<Rect> grid_cells(int x, int y, int cell_w, int cell_h, int columns, int rows, int gap) {
  std::vector<Rect> out;
  for (int row = 0; row < rows; ++row)
    for (int col = 0; col < columns; ++col) out.push_back({x + col * (cell_w + gap), y + row * (cell_h + gap), cell_w, cell_h});
  return out;
}

const std::vector<Field>& animation_schema() {
  static const std::vector<Field> schema = [] {
    std::vector<Field> f;
    Field name;
    name.key = "name";
    name.label = "Name";
    name.type = FieldType::Text;
    name.help = "The game picks animations by name: idle_<dir>, walk_<dir>, attack_<dir>, death_<dir> with <dir> up, up_right, right, down_right, down.";
    f.push_back(name);
    Field fps;
    fps.key = "fps";
    fps.label = "Frames per second";
    fps.type = FieldType::Int;
    fps.default_value = 5;
    fps.min = 1;
    fps.max = 120;
    f.push_back(fps);
    Field loop;
    loop.key = "loop";
    loop.label = "Loops";
    loop.type = FieldType::Bool;
    loop.default_value = true;
    loop.help = "Off: stops on the last frame (death animations).";
    f.push_back(loop);
    Field flip;
    flip.key = "flip_horizontal";
    flip.label = "Mirrored";
    flip.type = FieldType::Bool;
    flip.default_value = false;
    flip.help = "Draws the frames flipped left to right (so a left-facing animation can reuse the right-facing frames).";
    f.push_back(flip);
    Field frames;
    frames.key = "frames";
    frames.label = "Frames";
    frames.type = FieldType::ObjectList;
    for (const char* key : {"x", "y", "w", "h"}) {
      Field c;
      c.key = key;
      c.label = key;
      c.type = FieldType::Int;
      c.default_value = 0;
      c.min = 0;
      c.max = 100000;
      frames.fields.push_back(c);
    }
    f.push_back(frames);
    return f;
  }();
  return schema;
}

std::vector<Field> sprite_schema(const std::string& doc) {
  std::vector<Field> f;
  Field sheet;
  sheet.key = "spritesheet";
  sheet.label = "Spritesheet";
  sheet.type = FieldType::File;
  sheet.file_dir = "sprites";
  sheet.file_ext = ".png,.bmp,.jpg,.pcx,.tga";
  f.push_back(sheet);
  Field def;
  def.key = "default_animation";
  def.label = "Default animation";
  def.type = FieldType::Ref;
  def.source = "animations:" + doc;
  f.push_back(def);
  Field states;
  states.key = "states";
  states.label = "States";
  states.type = FieldType::Object;
  states.help = "Which animations a state of the unit uses, in order of preference. {dir} is the facing, {move} walk or idle, {carry} the carried resource.";
  for (const char* key : {"attack", "work", "carry"}) {
    Field s;
    s.key = key;
    s.label = key;
    s.type = FieldType::TextList;
    states.fields.push_back(s);
  }
  f.push_back(states);
  return f;
}

}  // namespace myrts::editor::sprite
