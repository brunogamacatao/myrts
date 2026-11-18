#include "app.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#ifndef _WIN32
#include <csignal>
#include <unistd.h>
#endif

#include "editor_model/export.hpp"
#include "editor_model/levels.hpp"
#include "inspector.hpp"

namespace myrts::editor {

namespace {
bool text_field(const char* label, std::string& value) {
  char buffer[512];
  std::snprintf(buffer, sizeof buffer, "%s", value.c_str());
  const bool changed = ImGui::InputText(label, buffer, sizeof buffer);
  if (changed) value = buffer;
  return changed;
}
}  // namespace

namespace {
using Json = Project::Json;

/// A sprite path to start a new thing from: the first one the game already has.
std::string any_sprite(const Project& project, const Collection& c) {
  for (std::size_t i = 0; i < collection::count(project, c); ++i)
    if (const std::string sprite = collection::item(project, c, i)->value("sprite", std::string{}); !sprite.empty()) return sprite;
  return {};
}
}  // namespace

App::App() {
  set_status("Open a game (File > Open) or make one from a template (File > New).");
  CollectionEditor::Config characters;
  characters.noun = "character";
  characters.collection = [](const Project& p) { return units_of(p); };
  characters.schema = &unit_schema();
  characters.textures = &textures_;
  characters.make = [](const Project& p, const std::string& id, const std::string& faction) {
    Json unit = {{"id", id}, {"name", "New character"}, {"hp", 50}, {"sprite", any_sprite(p, units_of(p))},
                 {"stats", {{"damage_min", 1}, {"damage_max", 3}, {"range", 1}, {"sight", 4}}},
                 {"commands", Json::array({"move", "stop", "attack"})}};
    if (!faction.empty()) unit["faction"] = faction;
    return unit;
  };
  characters_ = std::make_unique<CollectionEditor>(characters);
  CollectionEditor::Config buildings;
  buildings.noun = "building";
  buildings.collection = [](const Project& p) { return buildings_of(p); };
  buildings.schema = &building_schema();
  buildings.textures = &textures_;
  buildings.make = [](const Project& p, const std::string& id, const std::string& faction) {
    Json building = {{"id", id}, {"name", "New building"}, {"size", 2}, {"hp", 400}, {"build_seconds", 100},
                     {"sprite", any_sprite(p, buildings_of(p))}};
    if (!faction.empty()) building["faction"] = faction;
    return building;
  };
  buildings_ = std::make_unique<CollectionEditor>(buildings);
  sprites_ = std::make_unique<SpriteEditor>(textures_);
  map_ = std::make_unique<MapEditor>(textures_);
  game_ = std::make_unique<GameEditor>();
}

App::~App() = default;

std::string App::window_title() const {
  if (!project_) return "MyRTS Engine";
  return "MyRTS Engine - " + project_->title() + (unsaved() ? " *" : "");
}

void App::open_project(const std::string& directory) {
  auto opened = Project::open(directory);
  if (!opened) {
    dialog_error_ = opened.error();
    return;
  }
  project_ = std::move(*opened);
  selected_document_.clear();
  selected_level_ = active_level_ = 0;
  characters_->reset();
  buildings_->reset();
  sprites_->reset();
  map_->reset();
  game_->reset();
  textures_.clear();
  characters_->reset();
  buildings_->reset();
  sprites_->reset();
  map_->reset();
  game_->reset();
  textures_.clear();
  dialog_error_.clear();
  set_status("Opened " + project_->root().generic_string());
}

void App::save() {
  if (!project_) return;
  if (auto saved = project_->save(); !saved) {
    set_status("Save failed: " + saved.error());
    return;
  }
  if (const std::string failure = map_->save(*project_); !failure.empty()) {
    set_status("Saving the map failed: " + failure);
    return;
  }
  project_->revalidate();
  set_status(project_->report().ok() ? "Saved. No problems found." : "Saved, but the game has problems (see Problems).");
}

void App::set_game_program(std::string path) {
  game_program_ = std::move(path);
  // New games start from the template that is installed next to the programs.
  const std::filesystem::path installed = std::filesystem::path(game_program_).parent_path() / "templates" / "minimal-game";
  if (std::filesystem::exists(installed / "game.json")) template_path_ = installed.string();
  else if (std::filesystem::exists("templates/minimal-game/game.json")) template_path_ = "templates/minimal-game";
}

void App::play_level() {
  if (!project_) return;
  if (levels::count(*project_) == 0) return;
  save();
  if (unsaved()) return;  // saving failed: the status says why
  if (!project_->report().ok()) {
    set_status("Fix the problems before playing (see Problems).");
    return;
  }
  if (game_program_.empty() || !std::filesystem::exists(game_program_)) {
    set_status("The game program (myrts) was not found next to the editor.");
    return;
  }
  const auto& level = (*project_->document(project_->package().campaign_file))["levels"][active_level_];
  std::string faction = level.value("player_faction", std::string{});
  if (faction.empty())
    if (const auto factions = reference_options(*project_, "factions"); !factions.empty()) faction = factions[0];
  const std::string root = project_->root().generic_string(), number = std::to_string(active_level_ + 1);
#ifdef _WIN32
  const std::string command = "\"" + game_program_ + "\" --game \"" + root + "\" --level " + number + " --faction \"" + faction + "\"";
  (void)std::system(("start \"\" " + command).c_str());
#else
  // No shell: the paths and ids are passed as they are.
  signal(SIGCHLD, SIG_IGN);  // the game is left to run on its own, without leaving a zombie behind
  if (fork() == 0) {
    setsid();
    execl(game_program_.c_str(), game_program_.c_str(), "--game", root.c_str(), "--level", number.c_str(), "--faction", faction.c_str(), static_cast<char*>(nullptr));
    _exit(1);
  }
#endif
  set_status("Playing level " + std::to_string(active_level_ + 1) + ": " + levels::name(*project_, active_level_));
}

void App::close_project() {
  project_.reset();
  selected_document_.clear();
  set_status("Project closed.");
}

void App::request_quit() {
  if (project_ && unsaved()) {
    pending_ = Pending::Quit;
    ask_unsaved_ = true;
  } else {
    quit_ = true;
  }
}

void App::draw() {
  map_->begin_frame();
  // One dockspace over the whole window; the panels dock into it.
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const ImGuiID dockspace = ImGui::DockSpaceOverViewport(0, viewport);
  if (first_frame_ && !ImGui::DockBuilderGetNode(dockspace)->IsSplitNode() && !ImGui::DockBuilderGetNode(dockspace)->IsLeafNode())
    build_default_layout(dockspace);
  else if (first_frame_ && ImGui::DockBuilderGetNode(dockspace)->IsLeafNode() && ImGui::DockBuilderGetNode(dockspace)->Windows.Size == 0)
    build_default_layout(dockspace);
  draw_menu();
  clamp_levels();
  draw_levels_panel();
  draw_project_panel();
  draw_problems_panel();
  draw_welcome();
  draw_dialogs();

  ImGuiIO& io = ImGui::GetIO();
  if (project_ && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) save();
  if (project_ && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) map_->delete_selected();
  if (project_ && ImGui::IsKeyPressed(ImGuiKey_F5, false)) play_level();
  if (project_ && io.KeyCtrl && !io.WantTextInput) {  // a text field has its own undo while it is being typed in
    const bool undo = ImGui::IsKeyPressed(ImGuiKey_Z, true) && !io.KeyShift;
    const bool redo = (ImGui::IsKeyPressed(ImGuiKey_Z, true) && io.KeyShift) || ImGui::IsKeyPressed(ImGuiKey_Y, true);
    if (undo && !map_->undo()) project_->undo();  // the map tab's own history first, when it is in front
    if (redo && !map_->redo()) project_->redo();
  }
  first_frame_ = false;
}

void App::build_default_layout(unsigned int dockspace) {
  // Project on the left, the problems below, the editor taking the rest. The user can rearrange it; ImGui remembers.
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::DockBuilderRemoveNode(dockspace);
  ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspace, viewport->Size);
  ImGuiID center = dockspace;
  ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.22f, nullptr, &center);
  const ImGuiID left_bottom = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.4f, nullptr, &left);
  const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.25f, nullptr, &center);
  ImGui::DockBuilderDockWindow("Levels", left);
  ImGui::DockBuilderDockWindow("Project", left_bottom);
  ImGui::DockBuilderDockWindow("Problems", bottom);
  ImGui::DockBuilderDockWindow("Editor", center);
  ImGui::DockBuilderFinish(dockspace);
}

void App::draw_menu() {
  if (!ImGui::BeginMainMenuBar()) return;
  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("New from template...")) ask_new_ = true;
    if (ImGui::MenuItem("Open...")) ask_open_ = true;
    ImGui::Separator();
    if (ImGui::MenuItem("Save", "Ctrl+S", false, project_ && unsaved())) save();
    if (ImGui::MenuItem("Close project", nullptr, false, project_.has_value())) {
      if (unsaved()) {
        pending_ = Pending::Close;
        ask_unsaved_ = true;
      } else {
        close_project();
      }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Quit")) request_quit();
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("Edit", project_.has_value())) {
    if (ImGui::MenuItem("Undo", "Ctrl+Z", false, project_->can_undo())) project_->undo();
    if (ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, project_->can_redo())) project_->redo();
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("Project", project_.has_value())) {
    if (ImGui::MenuItem("Play the active level", "F5")) play_level();
    if (ImGui::MenuItem("Export the game...")) ask_export_ = true;
    if (ImGui::MenuItem("Check the game")) {
      project_->revalidate();
      set_status(project_->report().ok() ? "No problems found." : "The game has problems (see Problems).");
    }
    ImGui::EndMenu();
  }
  ImGui::TextDisabled("  %s", status_.c_str());
  ImGui::EndMainMenuBar();
}

void App::draw_dialogs() {
  if (ask_open_) ImGui::OpenPopup("Open game");
  if (ask_new_) ImGui::OpenPopup("New game from template");
  if (ask_unsaved_) ImGui::OpenPopup("Unsaved changes");
  if (ask_export_) ImGui::OpenPopup("Export the game");
  ask_open_ = ask_new_ = ask_unsaved_ = ask_export_ = false;

  const auto center = [] { ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f}); };
  const auto switch_to = [&](const std::string& directory) {
    if (project_ && unsaved()) {
      open_path_ = directory;
      pending_ = Pending::Open;
      ask_unsaved_ = true;
    } else {
      open_project(directory);
    }
  };

  center();
  if (ImGui::BeginPopupModal("Open game", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted("Folder of the game (the one with game.json):");
    ImGui::SetNextItemWidth(420);
    text_field("##open", open_path_);
    if (!dialog_error_.empty()) ImGui::TextColored({1, 0.4f, 0.4f, 1}, "%s", dialog_error_.c_str());
    if (ImGui::Button("Open")) {
      if (project_ && unsaved()) {
        ImGui::CloseCurrentPopup();
        switch_to(open_path_);
      } else {
        open_project(open_path_);
        if (dialog_error_.empty()) ImGui::CloseCurrentPopup();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      dialog_error_.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  center();
  if (ImGui::BeginPopupModal("New game from template", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted("The new game starts as a copy of the template and is yours to change.");
    ImGui::TextDisabled("(templates/minimal-game: one faction, a worker, a soldier, three buildings and a level; the art is a placeholder.)");
    ImGui::SetNextItemWidth(420);
    text_field("Title", new_title_);
    ImGui::SetNextItemWidth(420);
    text_field("New folder", new_path_);
    ImGui::SetNextItemWidth(420);
    text_field("Template", template_path_);
    if (!dialog_error_.empty()) ImGui::TextColored({1, 0.4f, 0.4f, 1}, "%s", dialog_error_.c_str());
    if (ImGui::Button("Create")) {
      auto created = Project::create_from(template_path_, new_path_, new_title_);
      if (created) {
        project_ = std::move(*created);
        project_->revalidate();
        selected_document_.clear();
        selected_level_ = active_level_ = 0;
        characters_->reset();
        buildings_->reset();
  sprites_->reset();
  map_->reset();
  game_->reset();
  textures_.clear();
        dialog_error_.clear();
        set_status("Created " + project_->root().generic_string());
        ImGui::CloseCurrentPopup();
      } else {
        dialog_error_ = created.error();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      dialog_error_.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  center();
  if (ImGui::BeginPopupModal("Export the game", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted("Makes a folder with the game program and the game's files, ready to give to players.");
    ImGui::TextUnformatted("The game is saved and checked first.");
    ImGui::SetNextItemWidth(420);
    text_field("Folder to make it in", export_path_);
    if (!dialog_error_.empty()) ImGui::TextColored({1, 0.45f, 0.4f, 1}, "%s", dialog_error_.c_str());
    if (ImGui::Button("Export") && project_) {
      save();
      const auto made = unsaved() ? std::unexpected(std::string("saving failed: ") + status_) : export_game(*project_, game_program_, export_path_);
      if (made) {
        set_status("Exported to " + made->generic_string());
        dialog_error_.clear();
        ImGui::CloseCurrentPopup();
      } else {
        dialog_error_ = made.error();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      dialog_error_.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  center();
  if (ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted("The game has changes that were not saved.");
    const auto proceed = [&] {
      switch (pending_) {
        case Pending::Quit: quit_ = true; break;
        case Pending::Close: close_project(); break;
        case Pending::Open: open_project(open_path_); break;
        case Pending::None: break;
      }
      pending_ = Pending::None;
      ImGui::CloseCurrentPopup();
    };
    if (ImGui::Button("Save")) {
      save();
      if (!project_ || !unsaved()) proceed();
      else ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard")) {
      if (project_) project_->discard_changes();
      map_->reset();
  game_->reset();
      proceed();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      pending_ = Pending::None;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void App::draw_project_panel() {
  ImGui::Begin("Project");
  if (!project_) {
    ImGui::TextDisabled("No game open.");
  } else {
    ImGui::TextUnformatted(project_->title().c_str());
    ImGui::TextDisabled("%s", project_->root().generic_string().c_str());
    ImGui::Separator();
    ImGui::TextUnformatted("Data files");
    for (const std::string& name : project_->document_names()) {
      const std::string label = name + (project_->dirty(name) ? " *" : "");
      if (ImGui::Selectable(label.c_str(), selected_document_ == name)) selected_document_ = name;
    }
  }
  ImGui::End();
}

void App::draw_problems_panel() {
  ImGui::Begin("Problems");
  if (!project_) {
    ImGui::TextDisabled("Nothing to check.");
  } else {
    const PackageReport& report = project_->report();
    for (const std::string& line : report.info) ImGui::TextDisabled("%s", line.c_str());
    if (report.ok()) ImGui::TextColored({0.4f, 0.9f, 0.4f, 1}, "No problems found.");
    for (const std::string& problem : report.problems) {
      ImGui::TextColored({1, 0.45f, 0.4f, 1}, "x");
      ImGui::SameLine();
      ImGui::TextWrapped("%s", problem.c_str());
    }
  }
  ImGui::End();
}

void App::clamp_levels() {
  if (!project_) return;
  const std::size_t n = levels::count(*project_);
  if (n == 0) return;
  selected_level_ = std::min(selected_level_, n - 1);
  active_level_ = std::min(active_level_, n - 1);
}

void App::draw_levels_panel() {
  ImGui::Begin("Levels");
  if (!project_) {
    ImGui::TextDisabled("No game open.");
    ImGui::End();
    return;
  }
  Project& project = *project_;
  const std::size_t n = levels::count(project);
  const std::string campaign = project.package().campaign_file;

  const auto select = [&](std::size_t i) { selected_level_ = i; };
  if (ImGui::Button("+")) select(levels::add(project, "New level", n ? selected_level_ : 0));
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add a level after the selected one");
  ImGui::SameLine();
  if (ImGui::Button("Duplicate") && n) select(levels::duplicate(project, selected_level_));
  ImGui::SameLine();
  if (ImGui::Button("Remove") && n > 1) {
    const std::size_t gone = selected_level_;
    if (levels::remove(project, gone)) {
      if (active_level_ > gone) --active_level_;
      select(std::min(gone, levels::count(project) - 1));
    }
  }
  ImGui::SameLine();
  const auto move_selected = [&](std::size_t to) {
    const std::size_t from = selected_level_;
    if (levels::move(project, from, to)) {
      active_level_ = levels::index_after_move(active_level_, from, to);
      selected_level_ = to;
    }
  };
  if (ImGui::ArrowButton("up", ImGuiDir_Up) && selected_level_ > 0) move_selected(selected_level_ - 1);
  ImGui::SameLine();
  if (ImGui::ArrowButton("down", ImGuiDir_Down) && selected_level_ + 1 < n) move_selected(selected_level_ + 1);
  ImGui::Separator();

  ImGui::BeginChild("levellist", {0, -ImGui::GetTextLineHeightWithSpacing() * 2.5f});
  std::string last_chapter;
  for (std::size_t i = 0; i < n; ++i) {
    const auto& level = (*project.document(campaign))["levels"][i];
    const std::string chapter = level.value("chapter", std::string{});
    if (chapter != last_chapter) {
      if (!chapter.empty()) ImGui::TextDisabled("%s", chapter.c_str());
      last_chapter = chapter;
    }
    const bool active = i == active_level_;
    const std::string text = std::string(active ? "> " : "  ") + std::to_string(i + 1) + ". " + level.value("name", std::string{});
    ImGui::PushID(static_cast<int>(i));
    if (active) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.35f, 1.0f));
    if (ImGui::Selectable(text.c_str(), i == selected_level_, ImGuiSelectableFlags_AllowDoubleClick)) {
      select(i);
      if (ImGui::IsMouseDoubleClicked(0)) active_level_ = i;
    }
    if (active) ImGui::PopStyleColor();
    if (ImGui::BeginPopupContextItem("levelmenu")) {
      select(i);
      if (ImGui::MenuItem("Make active")) active_level_ = i;
      if (ImGui::MenuItem("Rename...")) {
        rename_text_ = levels::name(project, i);
        ask_rename_ = true;
      }
      if (ImGui::MenuItem("Duplicate")) select(levels::duplicate(project, i));
      if (ImGui::MenuItem("Remove", nullptr, false, n > 1)) {
        if (levels::remove(project, i)) {
          if (active_level_ > i) --active_level_;
          select(std::min(i, levels::count(project) - 1));
        }
      }
      ImGui::EndPopup();
    }
    if (ImGui::BeginDragDropSource()) {  // drag a level to a new place in the list
      ImGui::SetDragDropPayload("LEVEL", &i, sizeof i);
      ImGui::TextUnformatted(text.c_str());
      ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("LEVEL")) {
        const std::size_t from = *static_cast<const std::size_t*>(payload->Data);
        if (levels::move(project, from, i)) {
          active_level_ = levels::index_after_move(active_level_, from, i);
          selected_level_ = levels::index_after_move(selected_level_, from, i);
        }
      }
      ImGui::EndDragDropTarget();
    }
    ImGui::PopID();
  }
  ImGui::EndChild();
  ImGui::TextDisabled("Double-click: make active. Drag: reorder.");
  const std::string error = project.campaign_error();
  if (!error.empty()) ImGui::TextColored({1, 0.45f, 0.4f, 1}, "x %s", error.c_str());
  ImGui::End();

  if (ask_rename_) ImGui::OpenPopup("Rename level");
  ask_rename_ = false;
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
  if (ImGui::BeginPopupModal("Rename level", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::SetNextItemWidth(300);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    const bool enter = text_field("##rename", rename_text_);
    (void)enter;
    const bool commit = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::Button("OK");
    ImGui::SameLine();
    const bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape);
    if (commit) {
      levels::rename(project, selected_level_, rename_text_);
      project.seal();
    }
    if (commit || cancel) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
}

void App::draw_level_inspector() {
  Project& project = *project_;
  const std::size_t n = levels::count(project);
  if (n == 0) {
    ImGui::TextDisabled("The campaign has no levels.");
    return;
  }
  ImGui::Text("Level %zu of %zu%s", selected_level_ + 1, n, selected_level_ == active_level_ ? "  (active)" : "");
  ImGui::SameLine();
  if (selected_level_ != active_level_ && ImGui::SmallButton("Make active")) active_level_ = selected_level_;
  ImGui::SameLine();
  if (ImGui::SmallButton("Play the active level (F5)")) play_level();
  ImGui::Separator();
  ImGui::BeginChild("levelfields");
  draw_fields(project, project.package().campaign_file, level_schema(),
              Project::Json::json_pointer("/levels/" + std::to_string(selected_level_)));
  ImGui::EndChild();
}

void App::draw_welcome() {
  ImGui::Begin("Editor");
  if (!project_) {
    ImGui::TextUnformatted("MyRTS Engine");
    ImGui::TextDisabled("Open a game, or start a new one from a template.");
  } else if (ImGui::BeginTabBar("editors")) {
    if (ImGui::BeginTabItem("Level")) {
      draw_level_inspector();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Map")) {
      const std::size_t n = levels::count(*project_);
      std::string level_map, level_kind = "land";
      if (n > 0) {
        const auto& level = (*project_->document(project_->package().campaign_file))["levels"][active_level_];
        level_map = level.value("map", std::string{});
        level_kind = level.value("kind", std::string("land"));
      }
      map_->draw(*project_, level_map, active_level_, level_kind);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Characters")) {
      characters_->draw(*project_);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Buildings")) {
      buildings_->draw(*project_);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Sprites")) {
      sprites_->draw(*project_);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Game")) {
      game_->draw(*project_);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Data files")) {
      if (selected_document_.empty()) {
        ImGui::TextDisabled("Pick a data file in the Project panel: anything the other editors do not cover can be edited here as JSON.");
      } else {
        raw_.draw(*project_, selected_document_);
      }
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::End();
}

}  // namespace myrts::editor
