#include "project.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>

#include "core/campaign.hpp"

namespace fs = std::filesystem;

namespace myrts::editor {

namespace {
std::expected<Project::Json, std::string> read_json(const fs::path& file) {
  std::ifstream in(file);
  if (!in) return std::unexpected("cannot open " + file.generic_string());
  try {
    return Project::Json::parse(in);
  } catch (const std::exception& e) {
    return std::unexpected(file.generic_string() + ": " + e.what());
  }
}

std::expected<void, std::string> write_json(const fs::path& file, const Project::Json& doc) {
  std::error_code ec;
  fs::create_directories(file.parent_path(), ec);
  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  if (!out) return std::unexpected("cannot write " + file.generic_string());
  out << doc.dump(2) << '\n';
  out.flush();
  if (!out) return std::unexpected("error writing " + file.generic_string());
  return {};
}
}  // namespace

std::expected<Project, std::string> Project::open(const fs::path& directory) {
  auto package = GamePackage::load(directory);
  if (!package) return std::unexpected(package.error());
  Project project;
  project.package_ = std::move(*package);
  project.load_documents();
  project.revalidate();
  return project;
}

void Project::load_documents() {
  // The manifest, and every JSON table of the game (the data folder, the language files, and the files the manifest
  // points at, wherever they are).
  std::set<std::string> names = {"game.json"};
  const auto add_folder = [&](const std::string& relative) {
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(package_.path(relative), ec))
      if (entry.is_regular_file() && entry.path().extension() == ".json")
        names.insert(fs::relative(entry.path(), package_.root()).generic_string());
  };
  add_folder(package_.data_dir);
  add_folder(package_.strings_dir);
  for (const std::string& file : {package_.campaign_file, package_.tilesets_file, package_.sound_table_file,
                                  package_.ai_profiles_file, package_.rules_file})
    if (fs::exists(package_.path(file))) names.insert(fs::path(file).generic_string());
  for (const std::string& name : names)
    if (auto doc = read_json(package_.path(name))) {
      saved_[name] = *doc;
      documents_.emplace(name, std::move(*doc));
    } else {
      report_.problems.push_back(doc.error());
    }
}

std::expected<Project, std::string> Project::create_from(const fs::path& template_dir, const fs::path& directory,
                                                          const std::string& title) {
  if (fs::exists(directory)) return std::unexpected(directory.generic_string() + " already exists");
  if (!fs::exists(template_dir / "game.json")) return std::unexpected(template_dir.generic_string() + " is not a game (no game.json)");
  std::error_code ec;
  fs::create_directories(directory, ec);
  fs::copy(template_dir, directory, fs::copy_options::recursive, ec);
  if (ec) return std::unexpected("cannot copy the template: " + ec.message());
  auto project = open(directory);
  if (!project) return project;
  std::string id;
  for (const char c : title) {
    const char lower = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    if ((lower >= 'a' && lower <= 'z') || (lower >= '0' && lower <= '9')) id += lower;
    else if (!id.empty() && id.back() != '-') id += '-';
  }
  while (!id.empty() && id.back() == '-') id.pop_back();
  Json* manifest = project->edit("game.json");
  (*manifest)["title"] = title;
  (*manifest)["id"] = id.empty() ? "game" : id;
  project->package_.title = title;
  if (auto saved = project->save(); !saved) return std::unexpected(saved.error());
  return project;
}

std::vector<std::string> Project::document_names() const {
  std::vector<std::string> names;
  for (const auto& [name, doc] : documents_) names.push_back(name);
  return names;
}

const Project::Json* Project::document(const std::string& name) const {
  const auto it = documents_.find(name);
  return it == documents_.end() ? nullptr : &it->second;
}

Project::Json* Project::edit(const std::string& name) {
  const auto it = documents_.find(name);
  if (it == documents_.end()) return nullptr;
  dirty_.insert(name);
  return &it->second;
}

bool Project::open_document(const std::string& name) {
  if (documents_.contains(name)) return true;
  auto doc = read_json(package_.path(name));
  if (!doc) return false;
  saved_[name] = *doc;
  documents_.emplace(name, std::move(*doc));
  return true;
}

bool Project::create_document(const std::string& name, const Json& value) {
  if (documents_.contains(name) || fs::exists(package_.path(name))) return false;
  saved_[name] = Json();  // not on disk: always differs, so it is saved
  documents_.emplace(name, value);
  dirty_.insert(name);
  return true;
}

std::expected<void, std::string> Project::import_file(const fs::path& source, const std::string& relative) {
  const fs::path destination = package_.path(relative);
  std::error_code ec;
  if (!fs::is_regular_file(source, ec)) return std::unexpected(source.generic_string() + " is not a file");
  if (fs::exists(destination) && !fs::equivalent(source, destination, ec))
    return std::unexpected(relative + " already exists in the game");
  fs::create_directories(destination.parent_path(), ec);
  if (!fs::exists(destination)) {
    fs::copy_file(source, destination, ec);
    if (ec) return std::unexpected("cannot copy " + source.generic_string() + ": " + ec.message());
  }
  return {};
}

std::expected<void, std::string> Project::write_file(const std::string& relative, const std::string& text) {
  const fs::path file = package_.path(relative);
  std::error_code ec;
  fs::create_directories(file.parent_path(), ec);
  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  if (!out) return std::unexpected("cannot write " + relative);
  out << text;
  out.flush();
  if (!out) return std::unexpected("error writing " + relative);
  return {};
}

void Project::push_undo(const std::vector<std::string>& names, const std::string& merge_key) {
  redo_.clear();
  if (!merge_key.empty() && merge_key == merge_key_ && !undo_.empty() && undo_.back().before.size() == 1 &&
      undo_.back().before[0].first == names[0])
    return;
  Step step;
  for (const std::string& name : names) step.before.emplace_back(name, documents_.at(name));
  undo_.push_back(std::move(step));
  merge_key_ = merge_key;
}

void Project::refresh_dirty(const std::string& name) {
  if (documents_.at(name) == saved_.at(name)) dirty_.erase(name);
  else dirty_.insert(name);
}

bool Project::change(const std::string& name, const std::function<void(Json&)>& fn, const std::string& merge_key) {
  const auto it = documents_.find(name);
  if (it == documents_.end()) return false;
  push_undo({name}, merge_key);
  fn(it->second);
  refresh_dirty(name);
  return true;
}

bool Project::change_many(const std::vector<std::string>& names, const std::function<void(std::map<std::string, Json*>&)>& fn) {
  if (names.empty()) return false;
  for (const std::string& name : names)
    if (!documents_.contains(name)) return false;
  push_undo(names, {});
  std::map<std::string, Json*> docs;
  for (const std::string& name : names) docs[name] = &documents_.at(name);
  fn(docs);
  for (const std::string& name : names) refresh_dirty(name);
  return true;
}

bool Project::set(const std::string& name, const Json::json_pointer& at, const Json& value) {
  return change(name, [&](Json& doc) { doc[at] = value; }, at.to_string());
}

bool Project::remove(const std::string& name, const Json::json_pointer& at) {
  const auto it = documents_.find(name);
  if (it == documents_.end() || !it->second.contains(at)) return false;
  return change(name, [&](Json& doc) {
    Json& parent = doc[at.parent_pointer()];
    if (parent.is_array()) parent.erase(static_cast<std::size_t>(std::stoul(at.back())));
    else parent.erase(at.back());
  });
}

void Project::swap_step(Step& step, std::vector<Step>& to) {
  Step opposite;
  for (auto& [name, before] : step.before) {
    opposite.before.emplace_back(name, std::move(documents_.at(name)));
    documents_.at(name) = std::move(before);
  }
  for (const auto& [name, _] : opposite.before) refresh_dirty(name);
  to.push_back(std::move(opposite));
  merge_key_.clear();
}

bool Project::undo() {
  if (undo_.empty()) return false;
  Step step = std::move(undo_.back());
  undo_.pop_back();
  swap_step(step, redo_);
  return true;
}

bool Project::redo() {
  if (redo_.empty()) return false;
  Step step = std::move(redo_.back());
  redo_.pop_back();
  swap_step(step, undo_);
  return true;
}

std::string Project::campaign_error() const {
  const Json* campaign = document(package_.campaign_file);
  if (!campaign) return {};
  const Json* profiles = document(package_.ai_profiles_file);
  const auto parsed = Campaign::parse(campaign->dump(), profiles ? profiles->dump() : std::string{});
  return parsed ? std::string{} : parsed.error();
}

void Project::discard_changes() {
  documents_.clear();
  saved_.clear();
  dirty_.clear();
  undo_.clear();
  redo_.clear();
  merge_key_.clear();
  report_ = {};
  load_documents();
  revalidate();
}

std::expected<void, std::string> Project::save() {
  std::string failures;
  for (auto it = dirty_.begin(); it != dirty_.end();) {
    if (auto written = write_json(package_.path(*it), documents_.at(*it)); written) {
      saved_[*it] = documents_.at(*it);
      it = dirty_.erase(it);
    } else {
      failures += (failures.empty() ? "" : "; ") + written.error();
      ++it;
    }
  }
  if (!failures.empty()) return std::unexpected(failures);
  // The manifest may have changed what the package looks like (title, paths).
  if (auto package = GamePackage::load(package_.root())) package_ = std::move(*package);
  return {};
}

std::expected<void, std::string> Project::save_copy(const fs::path& directory) const {
  for (const auto& [name, doc] : documents_)
    if (auto written = write_json(directory / name, doc); !written) return written;
  return {};
}

void Project::revalidate() {
  report_ = check_package(package_.root());
}

}  // namespace myrts::editor
