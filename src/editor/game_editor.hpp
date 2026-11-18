#pragma once

#include <memory>
#include <string>

#include "collection_editor.hpp"
#include "editor_model/project.hpp"

namespace myrts::editor {

/// The Game tab: everything about the game that is not a level, a character, a building, a sprite or a map. Each part
/// is a tab of its own: the manifest, the rules, resources, factions, commands, right-click rules, projectiles, AI
/// profiles, sounds and texts.
class GameEditor {
 public:
  GameEditor();
  ~GameEditor();
  void draw(Project& project);
  void reset();

 private:
  void draw_document(Project& project, const std::string& doc, const std::vector<Field>& schema, const char* missing_help);
  void draw_texts(Project& project);

  std::unique_ptr<CollectionEditor> resources_, factions_, commands_, projectiles_;
  std::string language_, filter_, new_key_, new_language_, error_;
  bool only_untranslated_ = false;
};

/// Edits one data file as JSON text, for anything the other editors do not cover (the translations of a faction, say).
class RawEditor {
 public:
  void draw(Project& project, const std::string& doc);

 private:
  std::string doc_, text_, error_;
  std::uint64_t revision_ = ~0ull;
  bool edited_ = false;
};

}  // namespace myrts::editor
