#include "collection.hpp"

#include <algorithm>

namespace myrts::editor {

namespace {
using Json = Project::Json;

std::string data_file(const Project& p, const char* name) { return p.package().data_dir + "/" + name; }

bool is_data_doc(const Project& p, const std::string& name) { return name.starts_with(p.package().data_dir + "/"); }

void collect(const Json& node, const std::string& path, const std::string& id, std::vector<std::string>& out) {
  if (node.is_string()) {
    if (node.get<std::string>() == id) out.push_back(path);
  } else if (node.is_object()) {
    for (const auto& [key, value] : node.items()) {
      if (key == id) out.push_back(path + "/" + key);
      if (key == "id" && value.is_string()) continue;  // a definition's own id is not a use of it
      collect(value, path + "/" + key, id, out);
    }
  } else if (node.is_array()) {
    for (std::size_t i = 0; i < node.size(); ++i) collect(node[i], path + "/" + std::to_string(i), id, out);
  }
}

void replace_all(Json& node, const std::string& from, const std::string& to) {
  if (node.is_string()) {
    if (node.get<std::string>() == from) node = to;
  } else if (node.is_object()) {
    Json rebuilt = Json::object();
    for (auto& [key, value] : node.items()) {
      replace_all(value, from, to);
      rebuilt[key == from ? to : key] = std::move(value);
    }
    node = std::move(rebuilt);
  } else if (node.is_array()) {
    for (Json& child : node) replace_all(child, from, to);
  }
}
}  // namespace

Collection units_of(const Project& project) { return {data_file(project, "units.json"), "units"}; }
Collection buildings_of(const Project& project) { return {data_file(project, "buildings.json"), "buildings"}; }

namespace collection {

std::size_t count(const Project& project, const Collection& c) {
  const Json* doc = project.document(c.doc);
  return doc && doc->contains(c.array) && (*doc)[c.array].is_array() ? (*doc)[c.array].size() : 0;
}

const Json* item(const Project& project, const Collection& c, std::size_t index) {
  return index < count(project, c) ? &(*project.document(c.doc))[c.array][index] : nullptr;
}

std::string id(const Project& project, const Collection& c, std::size_t index) {
  const Json* it = item(project, c, index);
  return it ? it->value("id", std::string{}) : std::string{};
}

std::optional<std::size_t> find(const Project& project, const Collection& c, const std::string& wanted) {
  for (std::size_t i = 0; i < count(project, c); ++i)
    if (id(project, c, i) == wanted) return i;
  return std::nullopt;
}

std::string unique_id(const Project& project, const Collection& c, const std::string& base) {
  if (!find(project, c, base)) return base;
  for (int n = 2;; ++n)
    if (const std::string candidate = base + "_" + std::to_string(n); !find(project, c, candidate)) return candidate;
}

std::size_t add(Project& project, const Collection& c, const Json& value) {
  const std::size_t at = count(project, c);
  project.change(c.doc, [&](Json& doc) {
    if (!doc.contains(c.array) || !doc[c.array].is_array()) doc[c.array] = Json::array();
    doc[c.array].push_back(value);
  });
  return at;
}

std::size_t duplicate(Project& project, const Collection& c, std::size_t index) {
  const Json* original = item(project, c, index);
  if (!original) return index;
  Json copy = *original;
  const std::string new_id = unique_id(project, c, copy.value("id", std::string{}) + "_copy");
  copy["id"] = new_id;
  if (copy.contains("name")) copy["name"] = copy.value("name", std::string{}) + " (copy)";
  project.change(c.doc, [&](Json& doc) {
    doc[c.array].insert(doc[c.array].begin() + static_cast<std::ptrdiff_t>(index + 1), copy);
  });
  return index + 1;
}

bool remove(Project& project, const Collection& c, std::size_t index) {
  if (index >= count(project, c)) return false;
  return project.remove(c.doc, Json::json_pointer("/" + c.array + "/" + std::to_string(index)));
}

std::vector<std::string> references(const Project& project, const std::string& wanted) {
  std::vector<std::string> out;
  for (const std::string& name : project.document_names()) {
    if (!is_data_doc(project, name)) continue;
    std::vector<std::string> found;
    collect(*project.document(name), "", wanted, found);
    for (const std::string& path : found) out.push_back(name + ": " + path);
  }
  return out;
}

std::expected<void, std::string> rename_id(Project& project, const Collection& c, std::size_t index, const std::string& new_id) {
  const std::string old_id = id(project, c, index);
  if (old_id.empty()) return std::unexpected("no such item");
  if (new_id.empty()) return std::unexpected("the id cannot be empty");
  for (const char ch : new_id)
    if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-'))
      return std::unexpected("an id can only have letters, digits, '_' and '-'");
  if (new_id == old_id) return {};
  if (find(project, c, new_id)) return std::unexpected("'" + new_id + "' is already used");
  std::vector<std::string> names;
  for (const std::string& name : project.document_names())
    if (is_data_doc(project, name)) names.push_back(name);
  project.change_many(names, [&](std::map<std::string, Json*>& docs) {
    for (auto& [name, doc] : docs) replace_all(*doc, old_id, new_id);
  });
  return {};
}

}  // namespace collection
}  // namespace myrts::editor
