#include "levels.hpp"

#include <algorithm>

namespace myrts::editor::levels {

namespace {
using Json = Project::Json;

const Json* list(const Project& project) {
  const Json* doc = project.document(project.package().campaign_file);
  return doc && doc->contains("levels") && (*doc)["levels"].is_array() ? &(*doc)["levels"] : nullptr;
}
std::string doc_name(const Project& project) { return project.package().campaign_file; }
}  // namespace

std::size_t count(const Project& project) {
  const Json* l = list(project);
  return l ? l->size() : 0;
}

std::string name(const Project& project, std::size_t index) {
  const Json* l = list(project);
  if (!l || index >= l->size()) return {};
  return (*l)[index].value("name", std::string{});
}

std::size_t add(Project& project, const std::string& level_name, std::size_t after) {
  const std::size_t n = count(project);
  const std::size_t at = after >= n ? n : after + 1;
  Json level = Json::object();
  if (n > 0) {
    const Json& base = (*list(project))[at == 0 ? 0 : at - 1];
    for (const char* key : {"chapter", "kind", "map", "music"})
      if (base.contains(key)) level[key] = base[key];
  }
  level["name"] = level_name;
  if (!level.contains("map")) level["map"] = "";
  project.change(doc_name(project), [&](Json& doc) {
    if (!doc.contains("levels")) doc["levels"] = Json::array();
    doc["levels"].insert(doc["levels"].begin() + static_cast<std::ptrdiff_t>(at), level);
  });
  return at;
}

std::size_t duplicate(Project& project, std::size_t index) {
  if (index >= count(project)) return index;
  Json copy = (*list(project))[index];
  copy["name"] = copy.value("name", std::string{}) + " (copy)";
  project.change(doc_name(project), [&](Json& doc) {
    doc["levels"].insert(doc["levels"].begin() + static_cast<std::ptrdiff_t>(index + 1), copy);
  });
  return index + 1;
}

bool remove(Project& project, std::size_t index) {
  if (count(project) <= 1 || index >= count(project)) return false;
  return project.remove(doc_name(project), Json::json_pointer("/levels/" + std::to_string(index)));
}

bool move(Project& project, std::size_t from, std::size_t to) {
  const std::size_t n = count(project);
  if (from >= n || to >= n || from == to) return false;
  return project.change(doc_name(project), [&](Json& doc) {
    Json& l = doc["levels"];
    Json moved = std::move(l[from]);
    l.erase(l.begin() + static_cast<std::ptrdiff_t>(from));
    l.insert(l.begin() + static_cast<std::ptrdiff_t>(to), std::move(moved));
  });
}

bool rename(Project& project, std::size_t index, const std::string& new_name) {
  if (index >= count(project) || new_name.empty()) return false;
  return project.set(doc_name(project), Json::json_pointer("/levels/" + std::to_string(index) + "/name"), new_name);
}

std::size_t index_after_move(std::size_t index, std::size_t from, std::size_t to) {
  if (index == from) return to;
  if (from < to && index > from && index <= to) return index - 1;
  if (from > to && index >= to && index < from) return index + 1;
  return index;
}

}  // namespace myrts::editor::levels
