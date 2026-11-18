#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <memory>
#include <string>
#include <vector>

#include "core/defs.hpp"
#include "core/mapfile.hpp"
#include "core/terrain.hpp"
#include "core/tileset.hpp"
#include "project.hpp"

namespace myrts::editor {

/// What loading a map needs from the game: its tile sets, its resources and its rules.
struct MapContext {
  std::shared_ptr<const TileSets> tilesets;
  Registry registry;
  GameRules rules;
};
[[nodiscard]] std::expected<MapContext, std::string> load_map_context(const Project& project);
/// Reads the map in `relative` (a .mrmap or a .pud, relative to the game's folder).
[[nodiscard]] std::expected<MapFile, std::string> read_map(const Project& project, const MapContext& context, const std::string& relative);
[[nodiscard]] const char* terrain_label(Terrain terrain);

/// A map being edited. The terrain is edited as the terrain at the *vertices* of the tile grid ((width+1) x (height+1)
/// points; tile (x, y) has the vertices (x, y), (x+1, y), (x, y+1), (x+1, y+1) as its four corners). Painting a vertex
/// recomposes the tiles around it with the game's autotiler, so coasts, forest edges and the like follow, and where
/// two terrains that never touch would meet (water against grass) the terrain between them is filled in (a coast).
class MapEdit {
 public:
  /// Starts editing `map`. Fails if its tile set is unknown.
  static std::expected<MapEdit, std::string> from_map(MapFile map, std::shared_ptr<const TileSets> sets);
  /// A new map: `width` x `height` tiles of `fill`, in the tile set called `tileset`, with a start location for player
  /// 0 and one for player 1 (the corners' quarters).
  static std::expected<MapEdit, std::string> create(std::shared_ptr<const TileSets> sets, const std::string& tileset, int width, int height,
                                                    Terrain fill, const std::string& name);

  [[nodiscard]] const MapFile& map() const { return map_; }
  [[nodiscard]] int width() const { return map_.width; }
  [[nodiscard]] int height() const { return map_.height; }
  [[nodiscard]] const TileSet& tileset() const { return sets_->era(era_); }
  [[nodiscard]] const TileSets& tilesets() const { return *sets_; }

  [[nodiscard]] Terrain vertex(int x, int y) const { return vertices_[vertex_index(x, y)]; }
  /// Whether the terrain at this vertex can be changed (the tiles around it are ordinary terrain, not a wall).
  [[nodiscard]] bool paintable(int x, int y) const;

  /// Sets the terrain of the `size` x `size` vertices around (`x`, `y`) (the cursor is at the middle). Tiles that need
  /// it get the transition terrain. Returns false, changing nothing, if that cannot be done. Part of the current
  /// stroke (see `begin_stroke`), or a step of its own outside one.
  bool paint(int x, int y, int size, Terrain terrain);
  /// A stroke is everything painted until `end_stroke`: one undo step.
  void begin_stroke();
  void end_stroke();

  bool undo();
  bool redo();
  [[nodiscard]] bool can_undo() const { return !undo_.empty(); }
  [[nodiscard]] bool can_redo() const { return !redo_.empty(); }
  [[nodiscard]] bool dirty() const { return top_id() != saved_id_; }
  /// Changes with every edit, undo and redo (for caching what is computed from the map).
  [[nodiscard]] std::uint64_t revision() const { return revision_; }
  void mark_saved() { saved_id_ = top_id(); }

  /// The `.mrmap` text of the map as it is now.
  [[nodiscard]] std::string to_mrmap() const { return write_mrmap(map_); }

  // ---- objects: where players start and the mines. Each change is one undo step; changes with the same
  // non-empty `merge_key` in a row (typing an amount) are one step. ----
  using Start = MapFile::Start;
  using Mine = MapFile::Mine;
  std::size_t add_start(int owner, Point at);
  bool move_start(std::size_t index, Point at);
  bool set_start(std::size_t index, int owner, std::optional<Point> hall);
  bool remove_start(std::size_t index);
  std::size_t add_mine(Point at, const std::string& resource, int amount);
  bool move_mine(std::size_t index, Point at);
  bool set_mine(std::size_t index, const std::string& resource, int amount);
  bool remove_mine(std::size_t index);
  /// Ends a run of merged object changes.
  void seal() { object_key_.clear(); }

  /// What would stop a level from being played on this map: starts or mines that lie on terrain they cannot use,
  /// overlap each other or are in the sea, a resource the game does not have, and (for `land` levels) bases that are not
  /// connected on foot. `level_kind` is the level's kind ("land", "air", "naval"). Empty: nothing found.
  [[nodiscard]] std::vector<std::string> problems(const ResourceTable& resources, const std::string& level_kind) const;

  /// A tile that shows `terrain` all over (for palettes), or nullopt.
  [[nodiscard]] std::optional<std::uint16_t> solid_tile(Terrain terrain) const;

 private:
  struct TileChange {
    std::size_t index;
    std::uint16_t old_code, new_code, old_move, new_move;
  };
  struct VertexChange {
    std::size_t index;
    Terrain old_value, new_value;
  };
  struct Objects {
    std::vector<Start> starts;
    std::vector<Mine> mines;
    bool operator==(const Objects& o) const;
  };
  struct Step {
    std::uint64_t id = 0;
    std::vector<VertexChange> vertices;
    std::vector<TileChange> tiles;
    std::optional<std::pair<Objects, Objects>> objects;  ///< before and after
    std::string key;                                      ///< merges with the next object step of the same key
    [[nodiscard]] bool empty() const { return vertices.empty() && tiles.empty() && !objects; }
  };

  MapEdit() = default;
  [[nodiscard]] std::size_t vertex_index(int x, int y) const { return static_cast<std::size_t>(y) * static_cast<std::size_t>(map_.width + 1) + static_cast<std::size_t>(x); }
  [[nodiscard]] std::size_t tile_index(int x, int y) const { return static_cast<std::size_t>(y) * static_cast<std::size_t>(map_.width) + static_cast<std::size_t>(x); }
  [[nodiscard]] Quad quad_at(int x, int y) const;
  [[nodiscard]] bool tile_valid(int x, int y) const;
  [[nodiscard]] bool tile_frozen(int x, int y) const;
  [[nodiscard]] std::uint64_t top_id() const { return undo_.empty() ? 0 : undo_.back().id; }
  void derive_vertices();
  void apply(const Step& step, bool forward);
  void commit(Step step);
  bool change_objects(const std::function<void(Objects&)>& edit, const std::string& key);

  MapFile map_;
  std::shared_ptr<const TileSets> sets_;
  Era era_ = Era::Forest;
  std::vector<Terrain> vertices_;
  std::vector<bool> frozen_;  ///< per tile: walls and codes we do not understand stay as they are
  std::vector<Step> undo_, redo_;
  Step stroke_;
  bool in_stroke_ = false;
  std::uint64_t next_id_ = 1, saved_id_ = 0;
  std::string object_key_;
  std::uint64_t revision_ = 0;
};

}  // namespace myrts::editor
