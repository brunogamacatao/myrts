#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

#include "core/game_package.hpp"
#include "core/package_check.hpp"

namespace myrts::editor {

/// What the editor works on: one game package (a folder with `game.json`). Every JSON file of the game that the editor
/// edits is held as a document in memory (key order and unknown keys are kept, so saving changes only what was edited);
/// `save` writes the documents that were changed back, in the very files the game reads.
class Project {
 public:
  using Json = nlohmann::ordered_json;

  /// Opens the package in `directory` and loads its JSON documents.
  static std::expected<Project, std::string> open(const std::filesystem::path& directory);
  /// Copies the package in `template_dir` to the new folder `directory` (which must not exist yet), gives the copy the
  /// title and id of `title`, and opens it.
  static std::expected<Project, std::string> create_from(const std::filesystem::path& template_dir,
                                                          const std::filesystem::path& directory, const std::string& title);

  [[nodiscard]] const GamePackage& package() const { return package_; }
  [[nodiscard]] const std::filesystem::path& root() const { return package_.root(); }
  [[nodiscard]] const std::string& title() const { return package_.title; }

  // ---- documents: paths relative to the package root ("game.json", "data/units.json", ...) ----
  [[nodiscard]] std::vector<std::string> document_names() const;
  [[nodiscard]] const Json* document(const std::string& name) const;
  /// The document for changing directly: it counts as changed from now on (and cannot be undone). nullptr for an
  /// unknown name. Editors go through `change`, `set` and `remove` instead.
  [[nodiscard]] Json* edit(const std::string& name);

  // ---- undoable changes ----
  /// Runs `fn` on the document as one undoable step. Consecutive changes with the same non-empty `merge_key` (typing in
  /// one text field, dragging one slider) are a single step. False (and nothing done) for an unknown document.
  bool change(const std::string& name, const std::function<void(Json&)>& fn, const std::string& merge_key = {});
  /// `doc[at] = value` as an undoable step (parents are created as needed).
  bool set(const std::string& name, const Json::json_pointer& at, const Json& value);
  /// Removes the key or array element at `at`, as an undoable step. False if it is not there.
  bool remove(const std::string& name, const Json::json_pointer& at);
  /// Ends the run of merged changes: the next change is a new undo step even with the same key.
  void seal() { merge_key_.clear(); }

  bool undo();
  bool redo();
  [[nodiscard]] bool can_undo() const { return !undo_.empty(); }
  [[nodiscard]] bool can_redo() const { return !redo_.empty(); }

  [[nodiscard]] bool dirty() const { return !dirty_.empty(); }
  [[nodiscard]] bool dirty(const std::string& name) const { return dirty_.contains(name); }

  /// What is wrong with the campaign as it is now in memory (before saving), or empty.
  [[nodiscard]] std::string campaign_error() const;

  /// Throws away the changes: every document is read again from disk.
  void discard_changes();

  /// Writes the changed documents. Does not stop at the first failure; the message lists every file that failed.
  std::expected<void, std::string> save();
  /// Writes every document (changed or not) under `directory`, keeping the folder layout. For tests and "save a copy".
  std::expected<void, std::string> save_copy(const std::filesystem::path& directory) const;

  // ---- problems ----
  /// Checks the package as it is on disk (so after `save`) the way the game would load it.
  void revalidate();
  [[nodiscard]] const PackageReport& report() const { return report_; }

 private:
  Project() = default;
  void load_documents();

  struct Step {
    std::string name;
    Json before;  ///< the document as it was before the step
  };
  void push_undo(const std::string& name, const std::string& merge_key);
  void refresh_dirty(const std::string& name);

  GamePackage package_ = GamePackage::defaults({});
  std::map<std::string, Json> documents_;
  std::map<std::string, Json> saved_;  ///< each document as it is on disk
  std::set<std::string> dirty_;
  std::vector<Step> undo_, redo_;
  std::string merge_key_;
  PackageReport report_;
};

}  // namespace myrts::editor
