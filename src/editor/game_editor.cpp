#include "game_editor.hpp"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cstdio>

#include "editor_model/texts.hpp"
#include "inspector.hpp"

namespace myrts::editor {

namespace {
using Json = Project::Json;

/// A multi-line text box that edits a std::string of any size.
bool input_multiline(const char* label, std::string& text, const ImVec2& size) {
  return ImGui::InputTextMultiline(
      label, text.data(), text.capacity() + 1, size, ImGuiInputTextFlags_CallbackResize,
      [](ImGuiInputTextCallbackData* data) {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
          auto* str = static_cast<std::string*>(data->UserData);
          str->resize(static_cast<std::size_t>(data->BufTextLen));
          data->Buf = str->data();
        }
        return 0;
      },
      &text);
}

bool line(const char* label, std::string& value, float width = -FLT_MIN) {
  char buffer[512];
  std::snprintf(buffer, sizeof buffer, "%s", value.c_str());
  ImGui::SetNextItemWidth(width);
  const bool changed = ImGui::InputText(label, buffer, sizeof buffer);
  if (changed) value = buffer;
  return changed;
}

std::string first_id(const Project& project, const char* source) {
  const auto ids = reference_options(project, source);
  return ids.empty() ? std::string{} : ids[0];
}
}  // namespace

GameEditor::GameEditor() {
  CollectionEditor::Config resources;
  resources.noun = "resource";
  resources.collection = [](const Project& p) { return resources_of(p); };
  resources.schema = &resource_schema();
  resources.make = [](const Project&, const std::string& id, const std::string&) { return Json{{"id", id}, {"name", "New resource"}, {"source", "site"}}; };
  resources_ = std::make_unique<CollectionEditor>(resources);

  CollectionEditor::Config factions;
  factions.noun = "faction";
  factions.collection = [](const Project& p) { return factions_of(p); };
  factions.schema = &faction_schema();
  factions.hide_faction_filter = true;
  factions.make = [](const Project& p, const std::string& id, const std::string&) {
    return Json{{"id", id}, {"name", "New faction"}, {"worker", first_id(p, "units")}, {"town_hall", first_id(p, "buildings")}};
  };
  factions_ = std::make_unique<CollectionEditor>(factions);

  CollectionEditor::Config commands;
  commands.noun = "command";
  commands.collection = [](const Project& p) { return commands_of(p); };
  commands.schema = &command_schema();
  commands.hide_faction_filter = true;
  commands.make = [](const Project&, const std::string& id, const std::string&) {
    return Json{{"id", id}, {"label", "New command"}, {"order", "move"}, {"targeting", "none"}};
  };
  commands_ = std::make_unique<CollectionEditor>(commands);

  CollectionEditor::Config projectiles;
  projectiles.noun = "projectile";
  projectiles.collection = [](const Project& p) { return projectiles_of(p); };
  projectiles.schema = &projectile_schema();
  projectiles.hide_faction_filter = true;
  projectiles.make = [](const Project&, const std::string& id, const std::string&) { return Json{{"id", id}, {"shape", "arrow"}, {"speed", 400}}; };
  projectiles_ = std::make_unique<CollectionEditor>(projectiles);
}

GameEditor::~GameEditor() = default;

void GameEditor::reset() {
  resources_->reset();
  factions_->reset();
  commands_->reset();
  projectiles_->reset();
  language_.clear();
  error_.clear();
}

void GameEditor::draw_document(Project& project, const std::string& doc, const std::vector<Field>& schema, const char* missing_help) {
  if (!project.document(doc)) {
    ImGui::TextDisabled("%s is not in the game yet. %s", doc.c_str(), missing_help);
    if (ImGui::Button("Create it") && !project.create_document(doc, Json{{"format_version", 1}})) error_ = "cannot create " + doc;
    return;
  }
  ImGui::TextDisabled("%s", doc.c_str());
  ImGui::BeginChild("doc");
  draw_fields(project, doc, schema, Json::json_pointer(""));
  ImGui::EndChild();
}

void GameEditor::draw_texts(Project& project) {
  const auto languages = texts::languages(project);
  if (language_.empty() || !project.document(language_)) language_ = languages.empty() ? std::string{} : languages[0];
  ImGui::SetNextItemWidth(220);
  if (ImGui::BeginCombo("Language", language_.empty() ? "(none)" : language_.c_str())) {
    for (const std::string& l : languages)
      if (ImGui::Selectable(l.c_str(), l == language_)) language_ = l;
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  line("##newlang", new_language_, 70);
  ImGui::SameLine();
  if (ImGui::Button("+ New language")) {
    if (const auto made = texts::create_language(project, new_language_)) {
      language_ = *made;
      new_language_.clear();
      error_.clear();
    } else {
      error_ = made.error();
    }
  }
  if (!error_.empty()) ImGui::TextColored({1, 0.45f, 0.4f, 1}, "%s", error_.c_str());
  if (language_.empty()) {
    ImGui::TextDisabled("No language file yet: type a code (es, fr...) and press + New language.");
    return;
  }
  const auto missing = texts::missing(project, language_);
  const auto untranslated = texts::untranslated(project, language_);
  ImGui::Text("%zu texts, %zu not translated, %zu of the game's data texts have no entry.", texts::count(project, language_), untranslated.size(), missing.size());
  if (!missing.empty()) {
    ImGui::SameLine();
    if (ImGui::SmallButton("Add the missing ones")) texts::add_entries(project, language_, missing);
  }
  line("##filter", filter_, 260);
  ImGui::SameLine();
  ImGui::Checkbox("Only not translated", &only_untranslated_);
  ImGui::SameLine();
  line("##newkey", new_key_, 260);
  ImGui::SameLine();
  if (ImGui::Button("+ Add text") && !new_key_.empty()) {
    texts::set(project, language_, new_key_, new_key_);
    project.seal();
    new_key_.clear();
  }
  const Json& table = (*project.document(language_))["strings"];
  std::vector<std::string> keys;
  for (const auto& [key, value] : table.items()) {
    if (only_untranslated_ && std::ranges::find(untranslated, key) == untranslated.end()) continue;
    if (!filter_.empty() && key.find(filter_) == std::string::npos && value.dump().find(filter_) == std::string::npos) continue;
    keys.push_back(key);
  }
  if (ImGui::BeginTable("texts", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV, {0, 0})) {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Text (the key)", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("Translation", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 24.0f);
    ImGui::TableHeadersRow();
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(keys.size()));
    std::string to_remove;
    while (clipper.Step())
      for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
        const std::string& key = keys[static_cast<std::size_t>(row)];
        ImGui::PushID(row);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextWrapped("%s", key.c_str());
        ImGui::TableNextColumn();
        std::string value = texts::get(project, language_, key);
        char buffer[1024];
        std::snprintf(buffer, sizeof buffer, "%s", value.c_str());
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputText("##v", buffer, sizeof buffer)) texts::set(project, language_, key, buffer);
        if (ImGui::IsItemDeactivatedAfterEdit()) project.seal();
        ImGui::TableNextColumn();
        if (ImGui::SmallButton("x")) to_remove = key;
        ImGui::PopID();
      }
    if (!to_remove.empty()) {
      texts::remove(project, language_, to_remove);
      project.seal();
    }
    ImGui::EndTable();
  }
}

void GameEditor::draw(Project& project) {
  if (!ImGui::BeginTabBar("gametabs")) return;
  const GamePackage& pkg = project.package();
  if (ImGui::BeginTabItem("Manifest")) {
    draw_document(project, "game.json", manifest_schema(), "");
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem("Rules")) {
    draw_document(project, pkg.rules_file, rules_schema(), "Without it the game uses the default rules.");
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem("Resources")) {
    resources_->draw(project);
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem("Factions")) {
    factions_->draw(project);
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem("Commands")) {
    commands_->draw(project);
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem("Right-click")) {
    draw_document(project, pkg.data_dir + "/commands.json", context_rules_schema(), "");
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem("Projectiles")) {
    projectiles_->draw(project);
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem("AI profiles")) {
    draw_document(project, pkg.ai_profiles_file, ai_profiles_schema(), "Levels can then pick a named profile.");
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem("Sounds")) {
    draw_document(project, pkg.sound_table_file, sounds_schema(), "");
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem("Texts")) {
    draw_texts(project);
    ImGui::EndTabItem();
  }
  ImGui::EndTabBar();
}

void RawEditor::draw(Project& project, const std::string& doc) {
  const Json* document = project.document(doc);
  if (!document) return;
  if (doc != doc_ || (!edited_ && project.revision() != revision_)) {
    doc_ = doc;
    text_ = document->dump(2);
    revision_ = project.revision();
    edited_ = false;
    error_.clear();
  }
  ImGui::TextUnformatted(doc.c_str());
  ImGui::SameLine();
  if (ImGui::SmallButton("Apply")) {
    const Json parsed = Json::parse(text_, nullptr, false);
    if (parsed.is_discarded()) {
      error_ = "This is not valid JSON.";
    } else {
      project.change(doc, [&](Json& d) { d = parsed; });
      project.seal();
      edited_ = false;
      revision_ = project.revision();
      error_.clear();
    }
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("Revert")) {
    text_ = document->dump(2);
    edited_ = false;
    error_.clear();
  }
  if (!error_.empty()) {
    ImGui::SameLine();
    ImGui::TextColored({1, 0.45f, 0.4f, 1}, "%s", error_.c_str());
  }
  if (input_multiline("##raw", text_, {-FLT_MIN, -FLT_MIN})) edited_ = true;
}

}  // namespace myrts::editor
