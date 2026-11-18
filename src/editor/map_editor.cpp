#include "map_editor.hpp"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace myrts::editor {

namespace {
using Json = Project::Json;
constexpr std::array<Terrain, 8> kPalette{Terrain::GrassLight, Terrain::GrassDark, Terrain::DirtLight, Terrain::DirtDark,
                                          Terrain::WaterLight, Terrain::WaterDark, Terrain::Trees,     Terrain::Rocks};

bool text_input(const char* label, std::string& value, float width) {
  char buffer[512];
  std::snprintf(buffer, sizeof buffer, "%s", value.c_str());
  ImGui::SetNextItemWidth(width);
  const bool changed = ImGui::InputText(label, buffer, sizeof buffer);
  if (changed) value = buffer;
  return changed;
}
}  // namespace

void MapEditor::reset() {
  edit_.reset();
  context_.reset();
  path_.clear();
  source_.clear();
  error_.clear();
}

bool MapEditor::delete_selected() {
  if (!active_ || !edit_ || selected_kind_ == Kind::None) return false;
  const bool done = selected_kind_ == Kind::Start ? edit_->remove_start(selected_) : edit_->remove_mine(selected_);
  selected_kind_ = Kind::None;
  return done;
}

bool MapEditor::undo() { return active_ && edit_ && edit_->undo(); }
bool MapEditor::redo() { return active_ && edit_ && edit_->redo(); }

void MapEditor::open(Project& project, const std::string& relative) {
  error_.clear();
  if (!context_) {
    auto context = load_map_context(project);
    if (!context) {
      error_ = context.error();
      return;
    }
    context_ = std::move(*context);
  }
  auto map = read_map(project, *context_, relative);
  if (!map) {
    error_ = map.error();
    return;
  }
  auto edit = MapEdit::from_map(std::move(*map), context_->tilesets);
  if (!edit) {
    error_ = edit.error();
    return;
  }
  edit_ = std::move(*edit);
  source_ = relative;
  path_ = relative.ends_with(".mrmap") ? relative : "maps/" + std::filesystem::path(relative).stem().generic_string() + ".mrmap";
  textures_.drop(project.package().path(edit_->tileset().image()));
}

std::string MapEditor::save(Project& project) {
  if (!edit_ || !edit_->dirty()) return {};
  if (const auto written = project.write_file(path_, edit_->to_mrmap()); !written) return written.error();
  edit_->mark_saved();
  source_ = path_;
  return {};
}

void MapEditor::draw_toolbar(Project& project, const std::string& level_map, std::size_t level_index) {
  if (ImGui::Button("New map...")) {
    if (!context_) {
      if (auto context = load_map_context(project)) context_ = std::move(*context);
    }
    ask_new_ = true;
  }
  ImGui::SameLine();
  const bool other = !level_map.empty() && level_map != source_ && level_map != path_;
  if (ImGui::Button("Open the active level's map") && !level_map.empty()) open(project, level_map);
  ImGui::SameLine();
  if (edit_ && ImGui::Button(edit_->dirty() ? "Save map *" : "Save map")) {
    const std::string failure = save(project);
    error_ = failure;
  }
  if (edit_) {
    ImGui::SameLine();
    if (ImGui::Button("Use in the active level") && !level_map.empty()) {
      project.set(project.package().campaign_file, Json::json_pointer("/levels/" + std::to_string(level_index) + "/map"), path_);
      project.seal();
    }
  }
  if (!error_.empty()) ImGui::TextColored({1, 0.45f, 0.4f, 1}, "%s", error_.c_str());
  if (edit_) {
    ImGui::TextDisabled("%s  %dx%d  tile set: %s%s", path_.c_str(), edit_->width(), edit_->height(), edit_->map().tileset.c_str(),
                        source_ != path_ ? "   (converted from a .pud: Save map makes the .mrmap)" : "");
  } else {
    ImGui::TextDisabled(other || !level_map.empty() ? "Active level's map: %s" : "No map open.", level_map.c_str());
  }
  if (!edit_) return;

  ImGui::Separator();
  ImGui::TextUnformatted("Terrain");
  ImGui::SameLine();
  for (const Terrain t : kPalette) {
    const auto code = edit_->solid_tile(t);
    ImGui::SameLine();
    ImGui::PushID(static_cast<int>(t));
    bool clicked = false;
    std::optional<TileSpot> spot;
    if (code) spot = edit_->tileset().spot(*code);
    ALLEGRO_BITMAP* sheet = textures_.get(project.package().path(edit_->tileset().image()));
    if (spot && sheet) {
      const float w = static_cast<float>(al_get_bitmap_width(sheet)), h = static_cast<float>(al_get_bitmap_height(sheet));
      const float s = static_cast<float>(edit_->tilesets().tile_size());
      clicked = ImGui::ImageButton("##t", Textures::id(sheet), {28, 28}, {static_cast<float>(spot->x) / w, static_cast<float>(spot->y) / h},
                                   {(static_cast<float>(spot->x) + s) / w, (static_cast<float>(spot->y) + s) / h});
    } else {
      clicked = ImGui::Button(terrain_label(t));
    }
    if (clicked) terrain_ = t;
    if (terrain_ == t) {
      const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
      ImGui::GetWindowDrawList()->AddRect(a, b, IM_COL32(255, 215, 80, 255), 0, 0, 2.5f);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", terrain_label(t));
    ImGui::PopID();
  }
  ImGui::SameLine();
  ImGui::Text(" %s", terrain_label(terrain_));
  ImGui::TextUnformatted("Tool");
  ImGui::SameLine();
  const auto tool_button = [&](const char* label, Tool tool) {
    const bool on = tool_ == tool;
    if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::Button(label)) tool_ = tool;
    if (on) ImGui::PopStyleColor();
    ImGui::SameLine();
  };
  tool_button("Paint terrain", Tool::Terrain);
  tool_button("Select / move", Tool::Select);
  tool_button("Add start", Tool::AddStart);
  tool_button("Add mine", Tool::AddMine);
  if (tool_ == Tool::AddMine && context_) {
    ImGui::SetNextItemWidth(110);
    if (ImGui::BeginCombo("##mineres", new_mine_resource_.c_str())) {
      for (const ResourceDef& r : context_->registry.resources()->all())
        if (ImGui::Selectable(r.id.c_str(), r.id == new_mine_resource_)) new_mine_resource_ = r.id;
      ImGui::EndCombo();
    }
    ImGui::SameLine();
  }
  ImGui::NewLine();
  ImGui::SetNextItemWidth(120);
  ImGui::SliderInt("Brush", &brush_, 1, 8);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(120);
  ImGui::SliderFloat("Zoom", &zoom_, 0.25f, 3.0f, "%.2fx");
  ImGui::SameLine();
  ImGui::Checkbox("Grid", &grid_);
  ImGui::SameLine();
  if (ImGui::Button("Undo")) edit_->undo();
  ImGui::SameLine();
  if (ImGui::Button("Redo")) edit_->redo();
}

void MapEditor::handle_object_tools(const ImVec2& origin, float cell, bool hovered) {
  MapEdit& edit = *edit_;
  ImGuiIO& io = ImGui::GetIO();
  const Point tile{static_cast<int>(std::floor((io.MousePos.x - origin.x) / cell)), static_cast<int>(std::floor((io.MousePos.y - origin.y) / cell))};
  const bool inside = tile.x >= 0 && tile.y >= 0 && tile.x < edit.width() && tile.y < edit.height();
  if (hovered) {
    const ImVec2 a{origin.x + static_cast<float>(tile.x) * cell, origin.y + static_cast<float>(tile.y) * cell};
    const float size = tool_ == Tool::AddMine ? 3.0f : 1.0f;
    ImGui::GetWindowDrawList()->AddRect(a, {a.x + size * cell, a.y + size * cell}, IM_COL32(255, 215, 80, 255), 0, 0, 2.0f);
  }
  if (!hovered || !inside) {
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) edit.seal();
    if (!ImGui::IsItemActive()) return;
  }
  if (ImGui::IsItemActivated() && inside) {
    if (tool_ == Tool::AddStart) {
      int owner = 0;
      for (const auto& s : edit.map().starts) owner = std::max(owner, s.owner + 1);
      selected_ = edit.add_start(owner, tile);
      selected_kind_ = Kind::Start;
      tool_ = Tool::Select;
    } else if (tool_ == Tool::AddMine) {
      selected_ = edit.add_mine({std::clamp(tile.x - 1, 0, edit.width() - 3), std::clamp(tile.y - 1, 0, edit.height() - 3)}, new_mine_resource_, 2500);
      selected_kind_ = Kind::Mine;
      tool_ = Tool::Select;
    } else {  // select: the mine or the start under the cursor
      selected_kind_ = Kind::None;
      for (std::size_t i = 0; i < edit.map().mines.size(); ++i) {
        const Point at = edit.map().mines[i].at;
        if (tile.x >= at.x && tile.x < at.x + 3 && tile.y >= at.y && tile.y < at.y + 3) {
          selected_kind_ = Kind::Mine, selected_ = i, grab_ = {tile.x - at.x, tile.y - at.y};
        }
      }
      for (std::size_t i = 0; i < edit.map().starts.size(); ++i) {
        const Point at = edit.map().starts[i].at;
        if (std::abs(tile.x - at.x) <= 1 && std::abs(tile.y - at.y) <= 1) selected_kind_ = Kind::Start, selected_ = i, grab_ = {};
      }
    }
    edit.seal();
  }
  if (ImGui::IsItemActive() && tool_ == Tool::Select && selected_kind_ != Kind::None && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f)) {
    if (selected_kind_ == Kind::Start && selected_ < edit.map().starts.size())
      edit.move_start(selected_, {std::clamp(tile.x, 0, edit.width() - 1), std::clamp(tile.y, 0, edit.height() - 1)});
    if (selected_kind_ == Kind::Mine && selected_ < edit.map().mines.size())
      edit.move_mine(selected_, {std::clamp(tile.x - grab_.x, 0, edit.width() - 3), std::clamp(tile.y - grab_.y, 0, edit.height() - 3)});
  }
  if (ImGui::IsItemDeactivated()) edit.seal();
}

void MapEditor::draw_objects_panel(Project& project) {
  (void)project;
  MapEdit& edit = *edit_;
  if (edit.revision() != checked_revision_ && context_) {
    problems_ = edit.problems(*context_->registry.resources(), level_kind_);
    checked_revision_ = edit.revision();
  }
  if (selected_kind_ == Kind::Start && selected_ < edit.map().starts.size()) {
    const auto start = edit.map().starts[selected_];
    int player = start.owner + 1;
    bool has_hall = start.hall.has_value();
    Point hall = start.hall.value_or(Point{start.at.x, start.at.y});
    ImGui::Text("Start %zu:", selected_ + 1);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90);
    bool changed = ImGui::InputInt("Player", &player, 1);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Own hall position", &has_hall);
    if (has_hall) {
      ImGui::SameLine();
      ImGui::SetNextItemWidth(80);
      changed |= ImGui::InputInt("hall x", &hall.x, 0);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(80);
      changed |= ImGui::InputInt("hall y", &hall.y, 0);
    }
    if (changed) edit.set_start(selected_, std::clamp(player, 1, 8) - 1, has_hall ? std::optional<Point>(hall) : std::nullopt);
    ImGui::SameLine();
    if (ImGui::Button("Delete")) delete_selected();
  } else if (selected_kind_ == Kind::Mine && selected_ < edit.map().mines.size() && context_) {
    const auto mine = edit.map().mines[selected_];
    std::string resource = mine.resource;
    int amount = mine.amount;
    ImGui::Text("Mine %zu:", selected_ + 1);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110);
    bool changed = false;
    if (ImGui::BeginCombo("Resource", resource.c_str())) {
      for (const ResourceDef& r : context_->registry.resources()->all())
        if (ImGui::Selectable(r.id.c_str(), r.id == resource)) resource = r.id, changed = true;
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130);
    changed |= ImGui::InputInt("Amount", &amount, 100, 1000);
    if (changed) edit.set_mine(selected_, resource, std::max(amount, 0));
    ImGui::SameLine();
    if (ImGui::Button("Delete##mine")) delete_selected();
  } else {
    ImGui::TextDisabled("Select tool: click a start or a mine to edit it, drag to move it. Delete removes it.");
  }
  if (problems_.empty()) {
    ImGui::TextColored({0.4f, 0.9f, 0.4f, 1}, "The map is fine for a %s level.", level_kind_.c_str());
  } else {
    for (const std::string& p : problems_) ImGui::TextColored({1, 0.45f, 0.4f, 1}, "x %s", p.c_str());
  }
}

void MapEditor::draw_canvas(Project& project) {
  MapEdit& edit = *edit_;
  ALLEGRO_BITMAP* sheet = textures_.get(project.package().path(edit.tileset().image()));
  const int tile = edit.tilesets().tile_size();
  const float scale = zoom_ * 1.0f;
  const float cell = static_cast<float>(tile) * scale;
  const ImVec2 total{static_cast<float>(edit.width()) * cell, static_cast<float>(edit.height()) * cell};

  ImGui::BeginChild("canvas", {0, 0}, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("##map", total, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
  const bool hovered = ImGui::IsItemHovered();
  ImGuiIO& io = ImGui::GetIO();

  // Pan with the middle button, scroll with the wheel, zoom with Ctrl+wheel.
  if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0)) {
    ImGui::SetScrollX(ImGui::GetScrollX() - io.MouseDelta.x);
    ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseDelta.y);
  }
  if (hovered && io.MouseWheel != 0) {
    if (io.KeyCtrl) zoom_ = std::clamp(zoom_ * (io.MouseWheel > 0 ? 1.15f : 1.0f / 1.15f), 0.25f, 3.0f);
    else ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseWheel * cell * 3);
  }

  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 view_min = ImGui::GetWindowPos(), view_max{view_min.x + ImGui::GetWindowWidth(), view_min.y + ImGui::GetWindowHeight()};
  dl->PushClipRect(view_min, view_max, true);
  const int x0 = std::max(0, static_cast<int>((view_min.x - origin.x) / cell)), y0 = std::max(0, static_cast<int>((view_min.y - origin.y) / cell));
  const int x1 = std::min(edit.width(), static_cast<int>((view_max.x - origin.x) / cell) + 1), y1 = std::min(edit.height(), static_cast<int>((view_max.y - origin.y) / cell) + 1);
  if (sheet) {
    const float sw = static_cast<float>(al_get_bitmap_width(sheet)), sh = static_cast<float>(al_get_bitmap_height(sheet));
    for (int y = y0; y < y1; ++y)
      for (int x = x0; x < x1; ++x) {
        const auto spot = edit.tileset().spot(edit.map().tiles[static_cast<std::size_t>(y * edit.width() + x)]);
        const ImVec2 a{origin.x + static_cast<float>(x) * cell, origin.y + static_cast<float>(y) * cell};
        const ImVec2 b{a.x + cell, a.y + cell};
        if (!spot) {
          dl->AddRectFilled(a, b, IM_COL32(200, 0, 200, 255));
          continue;
        }
        dl->AddImage(Textures::id(sheet), a, b, {static_cast<float>(spot->x) / sw, static_cast<float>(spot->y) / sh},
                     {static_cast<float>(spot->x + tile) / sw, static_cast<float>(spot->y + tile) / sh});
      }
  } else {
    dl->AddRectFilled(view_min, view_max, IM_COL32(60, 0, 60, 255));
  }
  if (grid_)
    for (int y = y0; y <= y1; ++y)
      for (int x = x0; x <= x1; ++x) {
        const ImVec2 p{origin.x + static_cast<float>(x) * cell, origin.y + static_cast<float>(y) * cell};
        dl->AddLine(p, {p.x + cell, p.y}, IM_COL32(255, 255, 255, 40));
        dl->AddLine(p, {p.x, p.y + cell}, IM_COL32(255, 255, 255, 40));
      }

  // Starts and mines (drawn now, edited in the next phase).
  for (const auto& s : edit.map().starts) {
    const ImVec2 c{origin.x + (static_cast<float>(s.at.x) + 0.5f) * cell, origin.y + (static_cast<float>(s.at.y) + 0.5f) * cell};
    const bool chosen = selected_kind_ == Kind::Start && &s == &edit.map().starts[std::min(selected_, edit.map().starts.size() - 1)];
    dl->AddCircleFilled(c, cell * 0.8f, s.owner == 0 ? IM_COL32(80, 160, 255, 150) : IM_COL32(255, 90, 80, 150));
    if (chosen) dl->AddCircle(c, cell * 0.9f, IM_COL32(255, 255, 255, 255), 0, 2.5f);
    char label[16];
    std::snprintf(label, sizeof label, "P%d", s.owner + 1);
    dl->AddText({c.x - 8, c.y - 7}, IM_COL32_WHITE, label);
  }
  for (const auto& m : edit.map().mines) {
    const ImVec2 a{origin.x + static_cast<float>(m.at.x) * cell, origin.y + static_cast<float>(m.at.y) * cell};
    const bool chosen = selected_kind_ == Kind::Mine && &m == &edit.map().mines[std::min(selected_, edit.map().mines.size() - 1)];
    dl->AddRectFilled(a, {a.x + 3 * cell, a.y + 3 * cell}, IM_COL32(255, 215, 80, 50));
    dl->AddRect(a, {a.x + 3 * cell, a.y + 3 * cell}, chosen ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 215, 80, 255), 0, 0, chosen ? 3.0f : 2.0f);
    dl->AddText({a.x + 3, a.y + 3}, IM_COL32_WHITE, m.resource.c_str());
  }

  // The brush: it paints the vertices around the cursor; the outline shows which.
  const ImVec2 mouse = io.MousePos;
  if (tool_ != Tool::Terrain) {
    handle_object_tools(origin, cell, hovered);
  } else if (hovered) {
    const int vx = static_cast<int>(std::lround((mouse.x - origin.x) / cell)), vy = static_cast<int>(std::lround((mouse.y - origin.y) / cell));
    const int first = -(brush_ / 2);
    const ImVec2 a{origin.x + static_cast<float>(vx + first) * cell, origin.y + static_cast<float>(vy + first) * cell};
    dl->AddRect(a, {a.x + static_cast<float>(brush_ - 1) * cell, a.y + static_cast<float>(brush_ - 1) * cell}, IM_COL32(255, 215, 80, 255), 0, 0, 2.0f);
    dl->AddText({mouse.x + 14, mouse.y + 14}, IM_COL32_WHITE, ("(" + std::to_string(std::clamp(vx, 0, edit.width())) + ", " + std::to_string(std::clamp(vy, 0, edit.height())) + ")").c_str());
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsItemActive()) {
      if (!painting_) {
        edit.begin_stroke();
        painting_ = true;
      }
      edit.paint(std::clamp(vx, 0, edit.width()), std::clamp(vy, 0, edit.height()), brush_, terrain_);
    }
  }
  if (painting_ && (tool_ != Tool::Terrain || !ImGui::IsMouseDown(ImGuiMouseButton_Left))) {
    edit.end_stroke();
    painting_ = false;
  }
  dl->PopClipRect();
  ImGui::EndChild();
}

void MapEditor::draw(Project& project, const std::string& level_map, std::size_t level_index, const std::string& level_kind) {
  active_ = true;
  if (level_kind != level_kind_) {
    level_kind_ = level_kind;
    checked_revision_ = ~0ull;
  }
  draw_toolbar(project, level_map, level_index);
  if (edit_) {
    draw_objects_panel(project);
    draw_canvas(project);
  }

  if (ask_new_) ImGui::OpenPopup("New map");
  ask_new_ = false;
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
  if (ImGui::BeginPopupModal("New map", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    text_input("File name", new_name_, 240);
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("Width", &new_width_, 8);
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("Height", &new_height_, 8);
    new_width_ = std::clamp(new_width_, 8, 256);
    new_height_ = std::clamp(new_height_, 8, 256);
    const TileSets* sets = context_ ? context_->tilesets.get() : nullptr;
    if (sets) {
      std::vector<std::string> names;
      for (std::size_t i = 0; i < sets->count(); ++i) names.push_back(sets->era(static_cast<Era>(i)).name());
      new_tileset_ = std::clamp(new_tileset_, 0, static_cast<int>(names.size()) - 1);
      ImGui::SetNextItemWidth(160);
      if (ImGui::BeginCombo("Tile set", names[static_cast<std::size_t>(new_tileset_)].c_str())) {
        for (int i = 0; i < static_cast<int>(names.size()); ++i)
          if (ImGui::Selectable(names[static_cast<std::size_t>(i)].c_str(), i == new_tileset_)) new_tileset_ = i;
        ImGui::EndCombo();
      }
    }
    ImGui::SetNextItemWidth(160);
    if (ImGui::BeginCombo("Fill with", terrain_label(kPalette[static_cast<std::size_t>(new_fill_) % kPalette.size()]))) {
      for (int i = 0; i < static_cast<int>(kPalette.size()); ++i)
        if (ImGui::Selectable(terrain_label(kPalette[static_cast<std::size_t>(i)]), i == new_fill_)) new_fill_ = i;
      ImGui::EndCombo();
    }
    if (!error_.empty()) ImGui::TextColored({1, 0.45f, 0.4f, 1}, "%s", error_.c_str());
    if (ImGui::Button("Create") && sets) {
      const std::string tileset = sets->era(static_cast<Era>(new_tileset_)).name();
      auto created = MapEdit::create(context_->tilesets, tileset, new_width_, new_height_, kPalette[static_cast<std::size_t>(new_fill_) % kPalette.size()], new_name_);
      if (created) {
        edit_ = std::move(*created);
        path_ = "maps/" + new_name_ + ".mrmap";
        source_ = path_;
        error_.clear();
        ImGui::CloseCurrentPopup();
      } else {
        error_ = created.error();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      error_.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

}  // namespace myrts::editor
