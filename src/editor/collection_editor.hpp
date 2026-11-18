#pragma once

#include <functional>
#include <string>
#include <vector>

#include "editor_model/collection.hpp"
#include "editor_model/project.hpp"
#include "editor_model/schema.hpp"
#include "textures.hpp"

namespace myrts::editor {

/// A list of the items of a collection (filter, add, duplicate, delete, rename id) with the inspector for the selected one.
/// Used for characters and buildings now, and for the other tables of the game later.
class CollectionEditor {
 public:
  struct Config {
    std::string noun;                                              ///< "character", "building"
    std::function<Collection(const Project&)> collection;
    const std::vector<Field>* schema = nullptr;
    Textures* textures = nullptr;
    /// A new item with this id, for this faction (may be empty).
    std::function<Project::Json(const Project&, const std::string& id, const std::string& faction)> make;
  };

  explicit CollectionEditor(Config config) : config_(std::move(config)) {}
  void draw(Project& project);
  void reset() { selected_.clear(); }

 private:
  Config config_;
  std::string selected_;       ///< id of the selected item
  std::string filter_;
  std::string faction_filter_;  ///< empty: all
  std::string rename_text_, rename_error_;
  std::vector<std::string> delete_references_;
  bool ask_rename_ = false, ask_delete_ = false;
};

}  // namespace myrts::editor
