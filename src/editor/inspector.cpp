#include "inspector.hpp"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace myrts::editor {

namespace {
using Json = Project::Json;
using Ptr = Json::json_pointer;

constexpr float kLabelWidth = 170.0f;
const ImVec4 kBad{1.0f, 0.45f, 0.4f, 1.0f};

struct Ctx {
  Project& project;
  const std::string& doc;
  bool edited = false;

  [[nodiscard]] const Json* get(const Ptr& at) const {
    const Json* root = project.document(doc);
    return root && root->contains(at) ? &(*root)[at] : nullptr;
  }
  void set(const Ptr& at, const Json& value) {
    project.set(doc, at, value);
    edited = true;
  }
  void remove(const Ptr& at) {
    project.remove(doc, at);
    project.seal();
    edited = true;
  }
  /// Swaps two entries of the array at `list`.
  void swap(const Ptr& list, std::size_t a, std::size_t b) {
    project.change(doc, [&](Json& d) { std::swap(d[list][a], d[list][b]); });
    project.seal();
    edited = true;
  }
  void done_typing() {
    if (ImGui::IsItemDeactivatedAfterEdit()) project.seal();
  }
};

void draw_list(Ctx& c, const std::vector<Field>& fields, const Ptr& base);

/// Label on the left, the widget takes the rest of the line. Keys that are not in the file yet are dimmed: the widget
/// shows what the game does then. Right-click: back to the default (removes the key).
void label(Ctx& c, const Field& f, const Ptr& at) {
  const bool present = c.get(at) != nullptr;
  ImGui::AlignTextToFramePadding();
  if (!present) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextUnformatted(f.label.c_str());
  if (!present) ImGui::PopStyleColor();
  if (!f.help.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", f.help.c_str());
  if (present && ImGui::BeginPopupContextItem("reset")) {
    if (ImGui::MenuItem("Back to default")) c.remove(at);
    ImGui::EndPopup();
  }
  ImGui::SameLine(kLabelWidth);
  ImGui::SetNextItemWidth(-FLT_MIN);
}

std::string text_of(const Json* v) { return v && v->is_string() ? v->get<std::string>() : std::string{}; }

bool draw_text(Ctx& c, const Field&, const Ptr& at, bool multiline) {
  char buffer[2048];
  std::snprintf(buffer, sizeof buffer, "%s", text_of(c.get(at)).c_str());
  bool changed;
  if (multiline) changed = ImGui::InputTextMultiline("##v", buffer, sizeof buffer, {-FLT_MIN, ImGui::GetTextLineHeight() * 4.5f});
  else changed = ImGui::InputText("##v", buffer, sizeof buffer);
  if (changed) c.set(at, std::string(buffer));
  c.done_typing();
  return changed;
}

/// The files of the game under `dir` with one of the `extensions` (".png,.bmp"), as package-relative paths.
std::vector<std::string> files_for_picker(const Project& project, const std::string& dir, const std::string& extensions) {
  std::vector<std::string> out;
  std::error_code ec;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(project.package().path(dir), ec)) {
    if (!entry.is_regular_file()) continue;
    const std::string ext = entry.path().extension().generic_string();
    bool wanted = extensions.empty();
    for (std::size_t at = 0; !wanted && at < extensions.size();) {
      const std::size_t end = std::min(extensions.find(',', at), extensions.size());
      wanted = extensions.substr(at, end - at) == ext;
      at = end + 1;
    }
    if (wanted) out.push_back(std::filesystem::relative(entry.path(), project.root()).generic_string());
  }
  std::ranges::sort(out);
  return out;
}

void draw_file(Ctx& c, const Field& f, const Ptr& at) {
  char buffer[1024];
  const std::string current = text_of(c.get(at));
  std::snprintf(buffer, sizeof buffer, "%s", current.c_str());
  const bool missing = !current.empty() && !std::filesystem::exists(c.project.package().path(current));
  const bool picker = !f.file_dir.empty();
  ImGui::SetNextItemWidth(picker ? -34.0f : -FLT_MIN);
  if (missing) ImGui::PushStyleColor(ImGuiCol_Text, kBad);
  if (ImGui::InputText("##v", buffer, sizeof buffer)) c.set(at, std::string(buffer));
  if (missing) ImGui::PopStyleColor();
  c.done_typing();
  if (missing && ImGui::IsItemHovered()) ImGui::SetTooltip("This file is not in the game's folder.");
  if (!picker) return;
  ImGui::SameLine();
  static std::vector<std::string> choices;
  if (ImGui::Button("...")) {
    choices = files_for_picker(c.project, f.file_dir, f.file_ext);
    ImGui::OpenPopup("pick");
  }
  if (ImGui::BeginPopup("pick")) {
    ImGui::BeginChild("choices", {420, 260});
    if (choices.empty()) ImGui::TextDisabled("No files in %s/", f.file_dir.c_str());
    for (const std::string& choice : choices)
      if (ImGui::Selectable(choice.c_str(), choice == current)) {
        c.set(at, choice);
        c.project.seal();
        ImGui::CloseCurrentPopup();
      }
    ImGui::EndChild();
    ImGui::EndPopup();
  }
}

void draw_int(Ctx& c, const Field& f, const Ptr& at) {
  const Json* v = c.get(at);
  int value = v && v->is_number() ? static_cast<int>(v->get<double>()) : (f.default_value.is_number() ? f.default_value.get<int>() : 0);
  if (ImGui::DragInt("##v", &value, 0.2f, static_cast<int>(f.min), static_cast<int>(f.max))) {
    value = std::clamp(value, static_cast<int>(f.min), static_cast<int>(f.max));
    c.set(at, value);
  }
  c.done_typing();
}

void draw_float(Ctx& c, const Field& f, const Ptr& at) {
  const Json* v = c.get(at);
  double value = v && v->is_number() ? v->get<double>() : (f.default_value.is_number() ? f.default_value.get<double>() : 0.0);
  if (ImGui::InputDouble("##v", &value, 0.0, 0.0, "%g")) c.set(at, std::clamp(value, f.min, f.max));
  c.done_typing();
}

void draw_bool(Ctx& c, const Field& f, const Ptr& at) {
  const Json* v = c.get(at);
  bool value = v && v->is_boolean() ? v->get<bool>() : (f.default_value.is_boolean() && f.default_value.get<bool>());
  if (ImGui::Checkbox("##v", &value)) {
    c.set(at, value);
    c.project.seal();
  }
}

/// A combo over `options`. `optional`: the first entry is "(default)" and removes the key.
void draw_combo(Ctx& c, const Field& f, const Ptr& at, const std::vector<std::string>& options, bool optional) {
  const Json* v = c.get(at);
  const std::string current = text_of(v);
  const bool unknown = v && !current.empty() && std::ranges::find(options, current) == options.end();
  std::string shown = v ? current : (f.default_value.is_string() ? f.default_value.get<std::string>() : std::string("(default)"));
  if (optional && (!v || current.empty())) shown = "(default)";
  if (unknown) ImGui::PushStyleColor(ImGuiCol_Text, kBad);
  const bool open = ImGui::BeginCombo("##v", shown.c_str());
  if (unknown) ImGui::PopStyleColor();
  if (!open) return;
  if (optional && ImGui::Selectable("(default)", !v)) {
    if (v) c.remove(at);
  }
  for (const std::string& option : options)
    if (ImGui::Selectable(option.c_str(), option == current)) {
      c.set(at, option);
      c.project.seal();
    }
  ImGui::EndCombo();
}

void draw_color(Ctx& c, const Field& f, const Ptr& at) {
  const Json* v = c.get(at);
  const Json& source = v && v->is_array() && v->size() == 3 ? *v : f.default_value;
  float rgb[3] = {0, 0, 0};
  for (int i = 0; i < 3 && source.is_array() && source.size() == 3; ++i) rgb[i] = source[static_cast<std::size_t>(i)].get<float>() / 255.0f;
  if (ImGui::ColorEdit3("##v", rgb, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Uint8)) {
    Json out = Json::array();
    for (float channel : rgb) out.push_back(std::clamp(static_cast<int>(std::lround(channel * 255.0f)), 0, 255));
    c.set(at, out);
  }
  c.done_typing();
}

void draw_text_list(Ctx& c, const Field& f, const Ptr& at) {
  const Json* list = c.get(at);
  const std::size_t n = list && list->is_array() ? list->size() : 0;
  ImGui::PushID("list");
  for (std::size_t i = 0; i < n; ++i) {
    ImGui::PushID(static_cast<int>(i));
    char buffer[2048];
    std::snprintf(buffer, sizeof buffer, "%s", text_of(&(*list)[i]).c_str());
    ImGui::SetNextItemWidth(-30);
    const bool changed = f.compact ? ImGui::InputText("##t", buffer, sizeof buffer)
                                   : ImGui::InputTextMultiline("##t", buffer, sizeof buffer, {-30, ImGui::GetTextLineHeight() * 3.2f});
    if (changed) c.set(at / i, std::string(buffer));
    c.done_typing();
    ImGui::SameLine();
    if (ImGui::Button("x")) {
      c.remove(at / i);
      ImGui::PopID();
      break;
    }
    ImGui::PopID();
  }
  if (ImGui::Button("+ Add")) {
    c.project.change(c.doc, [&](Json& d) {
      Json& arr = d[at];
      if (!arr.is_array()) arr = Json::array();
      arr.push_back("");
    });
    c.project.seal();
    c.edited = true;
  }
  ImGui::PopID();
}

void draw_ref_list(Ctx& c, const Field& f, const Ptr& at) {
  const Json* list = c.get(at);
  const std::size_t n = list && list->is_array() ? list->size() : 0;
  const auto options = reference_options(c.project, f.source);
  ImGui::PushID("refs");
  std::vector<std::string> present;
  for (std::size_t i = 0; i < n; ++i) {
    const std::string id = text_of(&(*list)[i]);
    present.push_back(id);
    ImGui::PushID(static_cast<int>(i));
    const bool known = std::ranges::find(options, id) != options.end();
    if (!known) ImGui::PushStyleColor(ImGuiCol_Text, kBad);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(id.c_str());
    if (!known) ImGui::PopStyleColor();
    ImGui::SameLine(kLabelWidth + 20);
    if (ImGui::SmallButton("x")) {
      c.remove(at / i);
      ImGui::PopID();
      ImGui::PopID();
      return;
    }
    ImGui::PopID();
  }
  if (ImGui::BeginCombo("##add", "+ Add...")) {
    for (const std::string& option : options) {
      if (std::ranges::find(present, option) != present.end()) continue;
      if (ImGui::Selectable(option.c_str())) {
        c.project.change(c.doc, [&](Json& d) {
          Json& arr = d[at];
          if (!arr.is_array()) arr = Json::array();
          arr.push_back(option);
        });
        c.project.seal();
        c.edited = true;
      }
    }
    ImGui::EndCombo();
  }
  ImGui::PopID();
}

void draw_amounts(Ctx& c, const Field& f, const Ptr& at) {
  auto resources = reference_options(c.project, f.source.empty() ? "resources" : f.source);
  resources.insert(resources.end(), f.options.begin(), f.options.end());
  ImGui::PushID("amounts");
  for (const std::string& id : resources) {
    const Json* v = c.get(at / id);
    int value = v && v->is_number() ? static_cast<int>(v->get<double>()) : 0;
    ImGui::PushID(id.c_str());
    ImGui::AlignTextToFramePadding();
    if (!v) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::Text("    %s", id.c_str());
    if (!v) ImGui::PopStyleColor();
    ImGui::SameLine(kLabelWidth);
    ImGui::SetNextItemWidth(-40);
    if (ImGui::DragInt("##n", &value, 5.0f, 0, 1000000)) c.set(at / id, std::max(value, 0));
    c.done_typing();
    if (v) {
      ImGui::SameLine();
      if (ImGui::SmallButton("x")) {
        c.remove(at / id);
        if (const Json* rest = c.get(at); rest && rest->empty()) c.remove(at);
      }
    }
    ImGui::PopID();
  }
  ImGui::PopID();
}

Json new_item(Ctx& c, const std::vector<Field>& fields) {
  Json item = Json::object();
  for (const Field& f : fields) {
    if (f.type == FieldType::Choice && f.only_key.empty() && !f.options.empty()) item[f.key] = f.options[0];
    if (f.type == FieldType::Ref && f.only_key.empty() && f.key == "faction") {
      if (const auto ids = reference_options(c.project, f.source); !ids.empty()) item[f.key] = ids[0];
    }
  }
  return item;
}

void draw_object_list(Ctx& c, const Field& f, const Ptr& at) {
  const Json* list = c.get(at);
  const std::size_t n = list && list->is_array() ? list->size() : 0;
  ImGui::PushID("items");
  for (std::size_t i = 0; i < n; ++i) {
    ImGui::PushID(static_cast<int>(i));
    std::string title = std::to_string(i + 1) + ". " + (*list)[i].value("type", std::string{});
    if ((*list)[i].contains("faction")) title += text_of(&(*list)[i]["faction"]);
    const bool open = ImGui::TreeNodeEx("##item", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "%s", title.c_str());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 52);
    if (ImGui::SmallButton("^") && i > 0) c.swap(at, i, i - 1);
    ImGui::SameLine();
    if (ImGui::SmallButton("v") && i + 1 < n) c.swap(at, i, i + 1);
    ImGui::SameLine();
    const bool removed = ImGui::SmallButton("x");
    if (open) {
      if (!removed) draw_list(c, f.fields, at / i);
      ImGui::TreePop();
    }
    if (removed) {
      c.remove(at / i);
      ImGui::PopID();
      break;
    }
    ImGui::PopID();
  }
  if (ImGui::Button("+ Add")) {
    const Json item = new_item(c, f.fields);
    c.project.change(c.doc, [&](Json& d) {
      Json& arr = d[at];
      if (!arr.is_array()) arr = Json::array();
      arr.push_back(item);
    });
    c.project.seal();
    c.edited = true;
  }
  ImGui::PopID();
}

Json defaults_of(const std::vector<Field>& fields) {
  Json item = Json::object();
  for (const Field& f : fields)
    if (!f.default_value.is_null() && (f.type == FieldType::Int || f.type == FieldType::Float)) item[f.key] = f.default_value;
  return item;
}

/// Asks for a name; true (and the name) when the user pressed Add.
bool ask_name(const char* button, std::string& out) {
  static std::string typed;
  char buffer[256];
  std::snprintf(buffer, sizeof buffer, "%s", typed.c_str());
  ImGui::SetNextItemWidth(180);
  if (ImGui::InputText("##newname", buffer, sizeof buffer)) typed = buffer;
  ImGui::SameLine();
  if (ImGui::Button(button) && !typed.empty()) {
    out = typed;
    typed.clear();
    return true;
  }
  return false;
}

/// An object (or a map to lists) whose keys are names the user makes up: each is a node with its fields, and a name box adds one.
void draw_free_map(Ctx& c, const Field& f, const Ptr& at) {
  const Json* map = c.get(at);
  std::vector<std::string> keys;
  if (map && map->is_object())
    for (const auto& [key, value] : map->items()) keys.push_back(key);
  ImGui::PushID("free");
  for (const std::string& key : keys) {
    ImGui::PushID(key.c_str());
    const bool open = ImGui::TreeNodeEx("##k", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "%s", key.c_str());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 4);
    const bool removed = ImGui::SmallButton("x");
    if (open) {
      if (!removed) {
        if (f.type == FieldType::ListMap) draw_text_list(c, f, at / key);
        else draw_list(c, f.fields, at / key);
      }
      ImGui::TreePop();
    }
    ImGui::PopID();
    if (removed) {
      c.remove(at / key);
      break;
    }
  }
  std::string name;
  if (ask_name("+ Add", name) && !(map && map->is_object() && map->contains(name))) {
    c.set(at / name, f.type == FieldType::ListMap ? Json::array() : Json::object());
    c.project.seal();
  }
  ImGui::PopID();
}

/// One row per id of the source: a checkbox switches it on (a default object) or off (removed); an object that is on
/// shows its fields below.
void draw_object_map(Ctx& c, const Field& f, const Ptr& at) {
  if (f.free_keys) return draw_free_map(c, f, at);
  ImGui::PushID("map");
  for (const std::string& id : reference_options(c.project, f.source)) {
    const Ptr entry = at / id;
    const bool on = c.get(entry) != nullptr;
    ImGui::PushID(id.c_str());
    bool value = on;
    if (ImGui::Checkbox(id.c_str(), &value)) {
      if (value) c.set(entry, defaults_of(f.fields));
      else {
        c.remove(entry);
        if (const Json* rest = c.get(at); rest && rest->empty()) c.remove(at);
      }
      c.project.seal();
    }
    if (on) {
      ImGui::Indent(24);
      draw_list(c, f.fields, entry);
      ImGui::Unindent(24);
    }
    ImGui::PopID();
  }
  ImGui::PopID();
}

bool shown(Ctx& c, const Field& f, const std::vector<Field>& siblings, const Ptr& base) {
  if (f.only_key.empty()) return true;
  std::string current;
  if (const Json* v = c.get(base / f.only_key); v) current = text_of(v);
  else
    for (const Field& s : siblings)
      if (s.key == f.only_key && s.default_value.is_string()) current = s.default_value.get<std::string>();
  return std::ranges::find(f.only_values, current) != f.only_values.end();
}

void draw_list(Ctx& c, const std::vector<Field>& fields, const Ptr& base) {
  for (const Field& f : fields) {
    if (!shown(c, f, fields, base)) continue;
    const Ptr at = base / f.key;
    ImGui::PushID(f.key.c_str());
    switch (f.type) {
      case FieldType::Object:
      case FieldType::ObjectList: {
        const bool present = c.get(at) != nullptr;
        if (!present) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        const bool open = ImGui::TreeNodeEx(f.label.c_str(), ImGuiTreeNodeFlags_Framed);
        if (!present) ImGui::PopStyleColor();
        if (!f.help.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", f.help.c_str());
        if (present && ImGui::BeginPopupContextItem("reset")) {
          if (ImGui::MenuItem("Back to default (remove)")) c.remove(at);
          ImGui::EndPopup();
        }
        if (open) {
          if (f.type == FieldType::Object) draw_list(c, f.fields, at);
          else draw_object_list(c, f, at);
          ImGui::TreePop();
        }
        break;
      }
      case FieldType::TextList:
      case FieldType::RefList:
      case FieldType::ObjectMap:
      case FieldType::ListMap:
      case FieldType::Amounts: {
        const bool present = c.get(at) != nullptr;
        if (!present) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        const bool open = ImGui::TreeNodeEx(f.label.c_str(), ImGuiTreeNodeFlags_Framed | (present ? ImGuiTreeNodeFlags_DefaultOpen : 0));
        if (!present) ImGui::PopStyleColor();
        if (!f.help.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", f.help.c_str());
        if (present && ImGui::BeginPopupContextItem("reset")) {
          if (ImGui::MenuItem("Back to default (remove)")) c.remove(at);
          ImGui::EndPopup();
        }
        if (open) {
          if (f.type == FieldType::TextList) draw_text_list(c, f, at);
          else if (f.type == FieldType::RefList) draw_ref_list(c, f, at);
          else if (f.type == FieldType::ObjectMap) draw_object_map(c, f, at);
          else if (f.type == FieldType::ListMap) draw_free_map(c, f, at);
          else draw_amounts(c, f, at);
          ImGui::TreePop();
        }
        break;
      }
      default:
        label(c, f, at);
        switch (f.type) {
          case FieldType::Text: draw_text(c, f, at, false); break;
          case FieldType::MultiText: draw_text(c, f, at, true); break;
          case FieldType::File: draw_file(c, f, at); break;
          case FieldType::Int: draw_int(c, f, at); break;
          case FieldType::Float: draw_float(c, f, at); break;
          case FieldType::Bool: draw_bool(c, f, at); break;
          case FieldType::Choice: draw_combo(c, f, at, f.options, false); break;
          case FieldType::Ref: draw_combo(c, f, at, reference_options(c.project, f.source), true); break;
          case FieldType::Color: draw_color(c, f, at); break;
          default: break;
        }
    }
    ImGui::PopID();
  }
}
}  // namespace

bool draw_fields(Project& project, const std::string& doc, const std::vector<Field>& fields, const Json::json_pointer& base) {
  Ctx c{project, doc};
  draw_list(c, fields, base);
  return c.edited;
}

}  // namespace myrts::editor
