#include "collection_editor.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdio>

#include "inspector.hpp"

namespace myrts::editor {

namespace {
using Json = Project::Json;

bool contains_nocase(const std::string& haystack, const std::string& needle) {
  if (needle.empty()) return true;
  const auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
  std::string h, n;
  std::transform(haystack.begin(), haystack.end(), std::back_inserter(h), lower);
  std::transform(needle.begin(), needle.end(), std::back_inserter(n), lower);
  return h.find(n) != std::string::npos;
}

bool text_input(const char* label, std::string& value, float width = -FLT_MIN) {
  char buffer[512];
  std::snprintf(buffer, sizeof buffer, "%s", value.c_str());
  ImGui::SetNextItemWidth(width);
  const bool changed = ImGui::InputText(label, buffer, sizeof buffer);
  if (changed) value = buffer;
  return changed;
}
}  // namespace

void CollectionEditor::draw(Project& project) {
  const Collection coll = config_.collection(project);
  const std::size_t n = collection::count(project, coll);
  std::size_t selected_index = n;
  if (const auto found = collection::find(project, coll, selected_)) selected_index = *found;
  else selected_.clear();
  const auto select = [&](std::size_t index) { selected_ = collection::id(project, coll, index); };

  // ---- the list ----
  ImGui::BeginChild("list", {260, 0}, ImGuiChildFlags_Borders);
  if (ImGui::Button("+ New")) {
    std::string faction = faction_filter_;
    if (faction.empty()) {
      const auto factions = reference_options(project, "factions");
      if (!factions.empty()) faction = factions[0];
    }
    const std::string id = collection::unique_id(project, coll, "new_" + config_.noun);
    select(collection::add(project, coll, config_.make(project, id, faction)));
    project.seal();
  }
  ImGui::SameLine();
  if (ImGui::Button("Duplicate") && selected_index < n) {
    select(collection::duplicate(project, coll, selected_index));
    project.seal();
  }
  ImGui::SameLine();
  if (ImGui::Button("Delete") && selected_index < n) {
    delete_references_ = collection::references(project, selected_);
    ask_delete_ = true;
  }
  text_input("##filter", filter_);
  if (filter_.empty() && !ImGui::IsItemActive()) {
    ImGui::SameLine(10);
    ImGui::TextDisabled("search...");
  }
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (ImGui::BeginCombo("##faction", faction_filter_.empty() ? "All factions" : faction_filter_.c_str())) {
    if (ImGui::Selectable("All factions", faction_filter_.empty())) faction_filter_.clear();
    for (const std::string& faction : reference_options(project, "factions"))
      if (ImGui::Selectable(faction.c_str(), faction == faction_filter_)) faction_filter_ = faction;
    ImGui::EndCombo();
  }
  ImGui::Separator();
  ImGui::BeginChild("items");
  for (std::size_t i = 0; i < n; ++i) {
    const Json& it = *collection::item(project, coll, i);
    const std::string faction = it.value("faction", std::string{});
    const std::string name = it.value("name", std::string{});
    const std::string id = it.value("id", std::string{});
    if (!faction_filter_.empty() && faction != faction_filter_) continue;
    if (!contains_nocase(name + " " + id, filter_)) continue;
    const std::string label = name + "  (" + id + ")";
    if (ImGui::Selectable(label.c_str(), id == selected_)) selected_ = id;
  }
  ImGui::EndChild();
  ImGui::EndChild();
  ImGui::SameLine();

  // ---- the selected item ----
  ImGui::BeginChild("item");
  if (selected_index >= n) {
    ImGui::TextDisabled("Pick a %s in the list, or add one.", config_.noun.c_str());
  } else {
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Id: %s", selected_.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Rename id...")) {
      rename_text_ = selected_;
      rename_error_.clear();
      ask_rename_ = true;
    }
    ImGui::Separator();
    ImGui::BeginChild("fields");
    draw_fields(project, coll.doc, *config_.schema, Json::json_pointer("/" + coll.array + "/" + std::to_string(selected_index)));
    ImGui::EndChild();
  }
  ImGui::EndChild();

  // ---- dialogs ----
  if (ask_rename_) ImGui::OpenPopup("Rename id");
  if (ask_delete_) ImGui::OpenPopup("Delete item");
  ask_rename_ = ask_delete_ = false;
  const ImVec2 center = ImGui::GetMainViewport()->GetCenter();

  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
  if (ImGui::BeginPopupModal("Rename id", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted("Every use of the id in the game's data is updated too.");
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    text_input("##newid", rename_text_, 320);
    if (!rename_error_.empty()) ImGui::TextColored({1, 0.45f, 0.4f, 1}, "%s", rename_error_.c_str());
    const bool commit = ImGui::Button("Rename") || ImGui::IsKeyPressed(ImGuiKey_Enter);
    ImGui::SameLine();
    const bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape);
    if (commit && selected_index < n) {
      if (const auto done = collection::rename_id(project, coll, selected_index, rename_text_)) {
        selected_ = rename_text_;
        project.seal();
        ImGui::CloseCurrentPopup();
      } else {
        rename_error_ = done.error();
      }
    }
    if (cancel) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }

  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
  if (ImGui::BeginPopupModal("Delete item", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Delete '%s'?", selected_.c_str());
    if (!delete_references_.empty()) {
      ImGui::TextColored({1, 0.8f, 0.4f, 1}, "It is still used in %zu place(s); they will be left pointing at nothing:", delete_references_.size());
      for (std::size_t i = 0; i < delete_references_.size() && i < 8; ++i) ImGui::BulletText("%s", delete_references_[i].c_str());
      if (delete_references_.size() > 8) ImGui::TextDisabled("... and %zu more", delete_references_.size() - 8);
    }
    if (ImGui::Button("Delete") && selected_index < n) {
      collection::remove(project, coll, selected_index);
      project.seal();
      selected_.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
}

}  // namespace myrts::editor
