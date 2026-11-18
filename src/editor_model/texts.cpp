#include "texts.hpp"

#include <algorithm>
#include <set>

namespace myrts::editor::texts {

namespace {
using Json = Project::Json;
const Json::json_pointer kStrings("/strings");

const Json* table(const Project& p, const std::string& doc) {
  const Json* d = p.document(doc);
  return d && d->contains("strings") && (*d)["strings"].is_object() ? &(*d)["strings"] : nullptr;
}

void collect(const Json& doc, const char* array, std::initializer_list<const char*> keys, std::set<std::string>& out) {
  if (!doc.contains(array)) return;
  for (const Json& item : doc[array])
    for (const char* key : keys)
      if (item.contains(key) && item[key].is_string() && !item[key].get<std::string>().empty()) out.insert(item[key].get<std::string>());
}
}  // namespace

std::vector<std::string> languages(Project& project) {
  std::vector<std::string> out;
  const std::string prefix = project.package().strings_dir + "/";
  for (const std::string& name : project.document_names())
    if (name.starts_with(prefix)) out.push_back(name);
  return out;
}

std::expected<std::string, std::string> create_language(Project& project, const std::string& code) {
  if (code.empty() || code == "en") return std::unexpected("pick a language code other than \"en\" (the texts as written)");
  for (const char c : code)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_')) return std::unexpected("a language code has only letters");
  const std::string name = project.package().strings_dir + "/" + code + ".json";
  if (!project.create_document(name, Json{{"language", code}, {"strings", Json::object()}})) return std::unexpected(name + " already exists");
  return name;
}

std::size_t count(const Project& p, const std::string& doc) {
  const Json* t = table(p, doc);
  return t ? t->size() : 0;
}

std::string get(const Project& p, const std::string& doc, const std::string& key) {
  const Json* t = table(p, doc);
  return t && t->contains(key) && (*t)[key].is_string() ? (*t)[key].get<std::string>() : std::string{};
}

bool set(Project& p, const std::string& doc, const std::string& key, const std::string& value) {
  if (!table(p, doc) || key.empty()) return false;
  return p.set(doc, kStrings / key, value);
}

bool remove(Project& p, const std::string& doc, const std::string& key) {
  if (!table(p, doc) || !table(p, doc)->contains(key)) return false;
  return p.remove(doc, kStrings / key);
}

std::vector<std::string> data_texts(const Project& project) {
  std::set<std::string> out;
  const std::string data = project.package().data_dir + "/";
  const auto doc = [&](const char* file) { return project.document(data + file); };
  if (const Json* d = doc("units.json")) collect(*d, "units", {"name"}, out);
  if (const Json* d = doc("buildings.json")) collect(*d, "buildings", {"name"}, out);
  if (const Json* d = doc("factions.json")) collect(*d, "factions", {"name"}, out);
  if (const Json* d = doc("commands.json")) collect(*d, "commands", {"label", "prompt"}, out);
  if (const Json* d = doc("resources.json"))
    collect(*d, "resources", {"name", "no_depot_message", "unreachable_message", "depleted_message", "exhausted_message"}, out);
  if (const Json* d = project.document(project.package().campaign_file); d && d->contains("levels"))
    for (const Json& level : (*d)["levels"]) {
      for (const char* key : {"name", "chapter", "briefing"})
        if (level.contains(key) && level[key].is_string() && !level[key].get<std::string>().empty()) out.insert(level[key].get<std::string>());
      if (level.contains("hints"))
        for (const Json& hint : level["hints"])
          if (hint.is_string() && !hint.get<std::string>().empty()) out.insert(hint.get<std::string>());
    }
  return {out.begin(), out.end()};
}

std::vector<std::string> missing(const Project& project, const std::string& doc) {
  std::vector<std::string> out;
  const Json* t = table(project, doc);
  for (const std::string& text : data_texts(project))
    if (!t || !t->contains(text)) out.push_back(text);
  return out;
}

std::size_t add_entries(Project& project, const std::string& doc, const std::vector<std::string>& keys) {
  if (!table(project, doc)) return 0;
  std::size_t added = 0;
  project.change(doc, [&](Json& d) {
    for (const std::string& key : keys)
      if (!d["strings"].contains(key)) {
        d["strings"][key] = key;
        ++added;
      }
  });
  project.seal();
  return added;
}

std::vector<std::string> untranslated(const Project& project, const std::string& doc) {
  std::vector<std::string> out;
  if (const Json* t = table(project, doc))
    for (const auto& [key, value] : t->items())
      if (!value.is_string() || value.get<std::string>().empty() || value.get<std::string>() == key) out.push_back(key);
  return out;
}

}  // namespace myrts::editor::texts
