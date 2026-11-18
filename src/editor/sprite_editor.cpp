#include "sprite_editor.hpp"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <filesystem>

#include "core/sprite_doc.hpp"
#include "editor_model/sprite.hpp"
#include "inspector.hpp"

namespace myrts::editor {

namespace {
using Json = Project::Json;

bool text_input(const char* label, std::string& value, float width) {
  char buffer[1024];
  std::snprintf(buffer, sizeof buffer, "%s", value.c_str());
  ImGui::SetNextItemWidth(width);
  const bool changed = ImGui::InputText(label, buffer, sizeof buffer);
  if (changed) value = buffer;
  return changed;
}

std::string sprite_problem_text(const Project& project, const std::string& file) {
  const Json* doc = project.document(file);
  return doc ? sprite_problem(*doc) : std::string{};
}

const std::vector<Field>& animation_fields_without_name() {
  static const std::vector<Field> fields(sprite::animation_schema().begin() + 1, sprite::animation_schema().end());
  return fields;
}
}  // namespace

void SpriteEditor::show(Project& project, const std::string& file) {
  if (!project.open_document(file)) return;
  file_ = file;
  animation_ = 0;
  preview_time_ = 0;
}

void SpriteEditor::draw(Project& project) {
  const auto files = sprite::list(project);
  if (!file_.empty() && !project.document(file_)) file_.clear();
  if (!file_.empty() && animation_ >= sprite::animation_count(project, file_)) animation_ = 0;

  // ---- the sprites ----
  ImGui::BeginChild("sprites", {220, 0}, ImGuiChildFlags_Borders);
  if (ImGui::Button("+ New sprite...")) {
    error_.clear();
    ask_new_ = true;
  }
  ImGui::Separator();
  ImGui::BeginChild("spritelist");
  for (const std::string& file : files) {
    const std::string label = std::filesystem::path(file).stem().string() + (project.dirty(file) ? " *" : "");
    if (ImGui::Selectable(label.c_str(), file == file_)) show(project, file);
  }
  ImGui::EndChild();
  ImGui::EndChild();
  ImGui::SameLine();

  if (file_.empty()) {
    ImGui::BeginChild("nosprite");
    ImGui::TextDisabled("Pick a sprite, or make one from a picture (+ New sprite).");
    ImGui::EndChild();
  } else {
    const Json& doc = *project.document(file_);
    const std::string sheet = doc.value("spritesheet", std::string{});
    ALLEGRO_BITMAP* bitmap = sheet.empty() ? nullptr : textures_.get(project.package().path(sheet));
    const auto frames = sprite::frames(project, file_, animation_);
    const std::size_t animations = sprite::animation_count(project, file_);
    const float right_width = 330;

    // ---- the sheet ----
    ImGui::BeginChild("sheetpane", {ImGui::GetContentRegionAvail().x - right_width - 8, 0});
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("Zoom", &zoom_, 1.0f, 10.0f, "%.1fx");
    ImGui::SameLine();
    if (bitmap) ImGui::TextDisabled("%s  %dx%d   drag on the picture to add a frame", sheet.c_str(), al_get_bitmap_width(bitmap), al_get_bitmap_height(bitmap));
    else ImGui::TextColored({1, 0.45f, 0.4f, 1}, "cannot load the spritesheet '%s'", sheet.c_str());
    ImGui::BeginChild("sheet", {0, 0}, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    if (bitmap) {
      const float bw = static_cast<float>(al_get_bitmap_width(bitmap)), bh = static_cast<float>(al_get_bitmap_height(bitmap));
      const ImVec2 size{bw * zoom_, bh * zoom_};
      const ImVec2 origin = ImGui::GetCursorScreenPos();
      ImDrawList* dl = ImGui::GetWindowDrawList();
      dl->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, IM_COL32(40, 40, 46, 255));
      dl->AddImage(Textures::id(bitmap), origin, {origin.x + size.x, origin.y + size.y});
      ImGui::InvisibleButton("##sheet", size, ImGuiButtonFlags_MouseButtonLeft);
      if (ImGui::IsItemHovered() && ImGui::GetIO().KeyCtrl && ImGui::GetIO().MouseWheel != 0)
        zoom_ = std::clamp(zoom_ + ImGui::GetIO().MouseWheel * 0.5f, 1.0f, 10.0f);
      const auto pixel = [&](float v, float o, float limit) {
        return std::clamp(static_cast<int>(std::floor((v - o) / zoom_)), 0, static_cast<int>(limit));
      };
      const auto rect_on_screen = [&](const Rect& r) {
        return std::pair<ImVec2, ImVec2>{{origin.x + static_cast<float>(r.x) * zoom_, origin.y + static_cast<float>(r.y) * zoom_},
                                         {origin.x + static_cast<float>(r.x + r.w) * zoom_, origin.y + static_cast<float>(r.y + r.h) * zoom_}};
      };
      for (std::size_t i = 0; i < frames.size(); ++i) {
        const auto [a, b] = rect_on_screen(frames[i]);
        dl->AddRect(a, b, IM_COL32(255, 215, 80, 255));
        char number[32];
        std::snprintf(number, sizeof number, "%zu", i + 1);
        dl->AddText({a.x + 2, a.y + 1}, IM_COL32(255, 255, 255, 230), number);
      }
      const ImVec2 mouse = ImGui::GetIO().MousePos;
      if (ImGui::IsItemActivated()) {
        drag_x_ = pixel(mouse.x, origin.x, bw - 1);
        drag_y_ = pixel(mouse.y, origin.y, bh - 1);
      }
      if (ImGui::IsItemActive()) {
        const int x = pixel(mouse.x, origin.x, bw - 1), y = pixel(mouse.y, origin.y, bh - 1);
        const Rect r{std::min(x, drag_x_), std::min(y, drag_y_), std::abs(x - drag_x_) + 1, std::abs(y - drag_y_) + 1};
        const auto [a, b] = rect_on_screen(r);
        dl->AddRect(a, b, IM_COL32(90, 255, 120, 255), 0, 0, 2.0f);
      }
      if (ImGui::IsItemDeactivated() && ImGui::IsMouseReleased(0)) {
        const int x = pixel(mouse.x, origin.x, bw - 1), y = pixel(mouse.y, origin.y, bh - 1);
        const Rect r{std::min(x, drag_x_), std::min(y, drag_y_), std::abs(x - drag_x_) + 1, std::abs(y - drag_y_) + 1};
        if (r.w >= 4 && r.h >= 4) {  // a click or a tiny drag is not a frame
          sprite::add_frame(project, file_, animation_, r);
          project.seal();
        }
      }
    }
    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::SameLine();

    // ---- animations, preview, properties ----
    ImGui::BeginChild("props", {right_width, 0});
    ImGui::TextUnformatted("Animations");
    ImGui::BeginChild("animlist", {0, 140}, ImGuiChildFlags_Borders);
    for (std::size_t i = 0; i < animations; ++i) {
      const bool is_default = sprite::animation_name(project, file_, i) == doc.value("default_animation", std::string{});
      const std::string label = sprite::animation_name(project, file_, i) + (is_default ? "  (default)" : "");
      ImGui::PushID(static_cast<int>(i));
      if (ImGui::Selectable(label.c_str(), i == animation_)) {
        animation_ = i;
        preview_time_ = 0;
      }
      ImGui::PopID();
    }
    ImGui::EndChild();
    if (ImGui::SmallButton("+ Add")) {
      animation_ = sprite::add_animation(project, file_, "new_animation");
      project.seal();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Duplicate")) {
      animation_ = sprite::duplicate_animation(project, file_, animation_);
      project.seal();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Rename...")) {
      rename_text_ = sprite::animation_name(project, file_, animation_);
      error_.clear();
      ask_rename_ = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove")) {
      sprite::remove_animation(project, file_, animation_);
      project.seal();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Make default")) {
      project.set(file_, Json::json_pointer("/default_animation"), sprite::animation_name(project, file_, animation_));
      project.seal();
    }

    // preview
    ImGui::Separator();
    const auto current = sprite::frames(project, file_, animation_);
    if (bitmap && !current.empty()) {
      const Json& animation = doc["animations"][animation_];
      const double fps = std::max(1, animation.value("fps", 5));
      const bool loops = animation.value("loop", true);
      if (preview_playing_) preview_time_ += ImGui::GetIO().DeltaTime;
      std::size_t index = static_cast<std::size_t>(preview_time_ * fps);
      index = loops ? index % current.size() : std::min(index, current.size() - 1);
      const Rect f = current[index];
      const float bw = static_cast<float>(al_get_bitmap_width(bitmap)), bh = static_cast<float>(al_get_bitmap_height(bitmap));
      const float box = 140;
      const float scale = std::max(0.1f, std::min({box / static_cast<float>(std::max(f.w, 1)), box / static_cast<float>(std::max(f.h, 1)), 4.0f}));
      const ImVec2 origin = ImGui::GetCursorScreenPos();
      ImGui::Dummy({box, box});
      ImDrawList* dl = ImGui::GetWindowDrawList();
      dl->AddRectFilled(origin, {origin.x + box, origin.y + box}, IM_COL32(24, 24, 28, 255));
      const float w = static_cast<float>(f.w) * scale, h = static_cast<float>(f.h) * scale;
      const ImVec2 a{origin.x + (box - w) / 2, origin.y + (box - h) / 2};
      float u0 = static_cast<float>(f.x) / bw, u1 = static_cast<float>(f.x + f.w) / bw;
      if (animation.value("flip_horizontal", false)) std::swap(u0, u1);
      dl->AddImage(Textures::id(bitmap), a, {a.x + w, a.y + h}, {u0, static_cast<float>(f.y) / bh}, {u1, static_cast<float>(f.y + f.h) / bh});
      ImGui::SameLine();
      ImGui::BeginGroup();
      ImGui::Text("frame %zu of %zu", index + 1, current.size());
      ImGui::Text("%dx%d px", f.w, f.h);
      ImGui::Checkbox("Play", &preview_playing_);
      ImGui::EndGroup();
    }

    // properties of the animation
    ImGui::Separator();
    ImGui::BeginChild("animprops", {0, 0});
    draw_fields(project, file_, animation_fields_without_name(), Json::json_pointer("/animations/" + std::to_string(animation_)));
    if (ImGui::CollapsingHeader("Fill from a grid")) {
      ImGui::SetNextItemWidth(80); ImGui::InputInt("x", &grid_x_, 0);
      ImGui::SetNextItemWidth(80); ImGui::InputInt("y", &grid_y_, 0);
      ImGui::SetNextItemWidth(80); ImGui::InputInt("cell width", &grid_w_, 0);
      ImGui::SetNextItemWidth(80); ImGui::InputInt("cell height", &grid_h_, 0);
      ImGui::SetNextItemWidth(80); ImGui::InputInt("columns", &grid_cols_, 0);
      ImGui::SetNextItemWidth(80); ImGui::InputInt("rows", &grid_rows_, 0);
      ImGui::SetNextItemWidth(80); ImGui::InputInt("gap", &grid_gap_, 0);
      grid_w_ = std::max(grid_w_, 1);
      grid_h_ = std::max(grid_h_, 1);
      grid_cols_ = std::clamp(grid_cols_, 1, 64);
      grid_rows_ = std::clamp(grid_rows_, 1, 64);
      const auto cells = sprite::grid_cells(grid_x_, grid_y_, grid_w_, grid_h_, grid_cols_, grid_rows_, grid_gap_);
      if (ImGui::Button("Replace the frames with these")) {
        sprite::set_frames(project, file_, animation_, cells);
        project.seal();
      }
      ImGui::SameLine();
      if (ImGui::Button("Add them")) {
        auto all = sprite::frames(project, file_, animation_);
        all.insert(all.end(), cells.begin(), cells.end());
        sprite::set_frames(project, file_, animation_, all);
        project.seal();
      }
    }
    if (ImGui::CollapsingHeader("Sprite", ImGuiTreeNodeFlags_DefaultOpen)) draw_fields(project, file_, sprite::sprite_schema(file_), Json::json_pointer(""));
    const std::string problem = sprite_problem_text(project, file_);
    if (!problem.empty()) ImGui::TextColored({1, 0.45f, 0.4f, 1}, "x %s", problem.c_str());
    ImGui::EndChild();
    ImGui::EndChild();
  }

  // ---- dialogs ----
  if (ask_new_) ImGui::OpenPopup("New sprite");
  if (ask_rename_) ImGui::OpenPopup("Rename animation");
  ask_new_ = ask_rename_ = false;
  const ImVec2 center = ImGui::GetMainViewport()->GetCenter();

  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
  if (ImGui::BeginPopupModal("New sprite", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted("The picture is copied into the game's sprites folder.");
    text_input("Picture file", new_image_, 420);
    text_input("Name", new_name_, 420);
    if (!error_.empty()) ImGui::TextColored({1, 0.45f, 0.4f, 1}, "%s", error_.c_str());
    if (ImGui::Button("Create")) {
      ALLEGRO_BITMAP* picture = al_load_bitmap(new_image_.c_str());
      if (!picture) {
        error_ = "cannot load the picture " + new_image_;
      } else {
        const int w = al_get_bitmap_width(picture), h = al_get_bitmap_height(picture);
        al_destroy_bitmap(picture);
        if (const auto made = sprite::create(project, new_image_, new_name_, w, h)) {
          show(project, *made);
          error_.clear();
          ImGui::CloseCurrentPopup();
        } else {
          error_ = made.error();
        }
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }

  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
  if (ImGui::BeginPopupModal("Rename animation", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    text_input("##name", rename_text_, 300);
    if (!error_.empty()) ImGui::TextColored({1, 0.45f, 0.4f, 1}, "%s", error_.c_str());
    const bool commit = ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Enter);
    ImGui::SameLine();
    const bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape);
    if (commit) {
      if (const auto done = sprite::rename_animation(project, file_, animation_, rename_text_)) {
        project.seal();
        ImGui::CloseCurrentPopup();
      } else {
        error_ = done.error();
      }
    }
    if (cancel) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
}

}  // namespace myrts::editor
