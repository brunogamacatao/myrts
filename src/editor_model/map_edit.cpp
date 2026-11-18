#include "map_edit.hpp"

#include <algorithm>
#include <filesystem>
#include <array>
#include <map>
#include <queue>
#include <set>

#include "core/pathfinding.hpp"

namespace myrts::editor {

namespace {
// The terrains that can share a tile form a tree (the pairs of terrain.cpp), rooted at light grass:
//   water dark - water light - dirt light - grass light - trees       (+ dirt dark and rocks on dirt light,
//                                                                       grass dark on grass light)
constexpr std::array<std::pair<Terrain, Terrain>, 7> kEdges{{
    {Terrain::GrassLight, Terrain::Trees},
    {Terrain::GrassLight, Terrain::GrassDark},
    {Terrain::GrassLight, Terrain::DirtLight},
    {Terrain::DirtLight, Terrain::Rocks},
    {Terrain::DirtLight, Terrain::DirtDark},
    {Terrain::DirtLight, Terrain::WaterLight},
    {Terrain::WaterLight, Terrain::WaterDark},
}};

/// The terrain one step from `from` on the way to `to`.
Terrain step_toward(Terrain from, Terrain to) {
  if (from == to) return from;
  std::map<Terrain, Terrain> came_from;  // BFS from `to`: the parent of a node is the next hop towards `to`
  std::queue<Terrain> queue;
  queue.push(to);
  came_from[to] = to;
  while (!queue.empty()) {
    const Terrain at = queue.front();
    queue.pop();
    if (at == from) return came_from[at];
    for (const auto& [a, b] : kEdges) {
      const Terrain other = at == a ? b : (at == b ? a : at);
      if (other != at && !came_from.contains(other)) {
        came_from[other] = at;
        queue.push(other);
      }
    }
  }
  return from;
}
}  // namespace

std::expected<MapContext, std::string> load_map_context(const Project& project) {
  const GamePackage& pkg = project.package();
  auto tilesets = TileSets::load_file(pkg.path(pkg.tilesets_file));
  if (!tilesets) return std::unexpected(tilesets.error());
  auto registry = Registry::load_directory(pkg.path(pkg.data_dir));
  if (!registry) return std::unexpected(registry.error());
  GameRules rules;
  if (std::filesystem::exists(pkg.path(pkg.rules_file))) {
    auto loaded = GameRules::load_file(pkg.path(pkg.rules_file));
    if (!loaded) return std::unexpected(loaded.error());
    rules = *loaded;
  }
  return MapContext{std::make_shared<const TileSets>(std::move(*tilesets)), std::move(*registry), rules};
}

std::expected<MapFile, std::string> read_map(const Project& project, const MapContext& context, const std::string& relative) {
  return load_map(project.package().path(relative), *context.tilesets, *context.registry.resources(), context.rules);
}

const char* terrain_label(Terrain terrain) {
  switch (terrain) {
    case Terrain::WaterLight: return "Shallow water";
    case Terrain::WaterDark: return "Deep water";
    case Terrain::DirtLight: return "Dirt";
    case Terrain::DirtDark: return "Dark dirt";
    case Terrain::GrassLight: return "Grass";
    case Terrain::GrassDark: return "Dark grass";
    case Terrain::Trees: return "Trees";
    case Terrain::Rocks: return "Rocks";
    default: return "Wall";
  }
}

Quad MapEdit::quad_at(int x, int y) const {
  return {vertex(x, y), vertex(x + 1, y), vertex(x, y + 1), vertex(x + 1, y + 1)};
}

bool MapEdit::tile_frozen(int x, int y) const { return frozen_[tile_index(x, y)]; }

bool MapEdit::tile_valid(int x, int y) const {
  if (tile_frozen(x, y)) return true;
  const auto base = compose(quad_at(x, y));
  return base && tileset().pick(*base, 0).has_value();
}

bool MapEdit::paintable(int x, int y) const {
  for (int dy = -1; dy <= 0; ++dy)
    for (int dx = -1; dx <= 0; ++dx) {
      const int tx = x + dx, ty = y + dy;
      if (tx >= 0 && ty >= 0 && tx < map_.width && ty < map_.height && tile_frozen(tx, ty)) return false;
    }
  return true;
}

void MapEdit::derive_vertices() {
  const int w = map_.width, h = map_.height;
  vertices_.assign(static_cast<std::size_t>(w + 1) * static_cast<std::size_t>(h + 1), Terrain::GrassLight);
  std::vector<bool> known(vertices_.size(), false);
  frozen_.assign(map_.tiles.size(), false);
  const auto set = [&](int x, int y, Terrain t) {
    if (!known[vertex_index(x, y)]) {
      vertices_[vertex_index(x, y)] = t;
      known[vertex_index(x, y)] = true;
    }
  };
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const auto q = decompose(map_.tiles[tile_index(x, y)]);
      if (!q || is_wall(q->tl)) {
        frozen_[tile_index(x, y)] = true;
        continue;
      }
      set(x, y, q->tl);
      set(x + 1, y, q->tr);
      set(x, y + 1, q->bl);
      set(x + 1, y + 1, q->br);
    }
}

std::expected<MapEdit, std::string> MapEdit::from_map(MapFile map, std::shared_ptr<const TileSets> sets) {
  const auto era = sets->find(map.tileset);
  if (!era) return std::unexpected("unknown tile set '" + map.tileset + "'");
  MapEdit edit;
  edit.map_ = std::move(map);
  edit.sets_ = std::move(sets);
  edit.era_ = *era;
  edit.derive_vertices();
  return edit;
}

std::expected<MapEdit, std::string> MapEdit::create(std::shared_ptr<const TileSets> sets, const std::string& tileset, int width, int height,
                                                    Terrain fill, const std::string& name) {
  if (width < 8 || height < 8 || width > 256 || height > 256) return std::unexpected("a map is between 8 and 256 tiles wide and high");
  const auto era = sets->find(tileset);
  if (!era) return std::unexpected("unknown tile set '" + tileset + "'");
  if (is_wall(fill)) return std::unexpected("a map cannot be filled with walls");
  const auto base = compose({fill, fill, fill, fill});
  const auto code = base ? sets->era(*era).pick(*base, 0) : std::nullopt;
  if (!code) return std::unexpected("the tile set has no plain tile of that terrain");
  MapFile map;
  map.name = name;
  map.tileset = tileset;
  map.width = width;
  map.height = height;
  map.tiles.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), *code);
  map.movement.assign(map.tiles.size(), movement_of({fill, fill, fill, fill}));
  map.starts.push_back({0, {width / 4, height / 4}, std::nullopt});
  map.starts.push_back({1, {width * 3 / 4 - 1, height * 3 / 4 - 1}, std::nullopt});
  return from_map(std::move(map), std::move(sets));
}

std::optional<std::uint16_t> MapEdit::solid_tile(Terrain terrain) const {
  const auto base = compose({terrain, terrain, terrain, terrain});
  return base ? tileset().pick(*base, 0) : std::nullopt;
}

bool MapEdit::paint(int x, int y, int size, Terrain terrain) {
  if (is_wall(terrain) || size < 1) return false;
  ++revision_;
  const int w = map_.width, h = map_.height;
  const int first = -(size / 2);  // the cursor is at the middle of the brush
  std::map<std::size_t, Terrain> old_values;  // every vertex changed, with what it was
  std::set<std::size_t> locked;               // the vertices the brush set: the fixing never moves them
  std::map<std::size_t, int> order;           // when each fixed vertex last changed
  int tick = 0;

  const auto change = [&](int vx, int vy, Terrain t) {
    const std::size_t i = vertex_index(vx, vy);
    old_values.emplace(i, vertices_[i]);
    vertices_[i] = t;
  };
  const auto revert = [&] {
    for (const auto& [i, old] : old_values) vertices_[i] = old;
  };

  std::set<std::pair<int, int>> region;  // tiles to check (and recompose)
  const auto touch = [&](int vx, int vy) {
    for (int dy = -1; dy <= 0; ++dy)
      for (int dx = -1; dx <= 0; ++dx)
        if (vx + dx >= 0 && vy + dy >= 0 && vx + dx < w && vy + dy < h) region.insert({vx + dx, vy + dy});
  };

  for (int dy = 0; dy < size; ++dy)
    for (int dx = 0; dx < size; ++dx) {
      const int vx = x + first + dx, vy = y + first + dy;
      if (vx < 0 || vy < 0 || vx > w || vy > h || !paintable(vx, vy)) continue;
      if (vertex(vx, vy) == terrain) continue;
      change(vx, vy, terrain);
      locked.insert(vertex_index(vx, vy));
      touch(vx, vy);
    }
  if (old_values.empty()) return true;  // nothing to change

  // Fix the tiles that cannot exist: the vertices that are not locked move one step towards the terrain that was painted.
  bool stable = false;
  for (int pass = 0; pass < 200 && !stable; ++pass) {
    stable = true;
    const std::set<std::pair<int, int>> checking = region;
    for (const auto& [tx, ty] : checking) {
      if (tile_valid(tx, ty)) continue;
      stable = false;
      const std::array<std::pair<int, int>, 4> corners{{{tx, ty}, {tx + 1, ty}, {tx, ty + 1}, {tx + 1, ty + 1}}};
      // The pivot: a vertex the brush set, else the one that changed last.
      std::optional<std::size_t> pivot;
      int best = -1;
      for (const auto& [vx, vy] : corners) {
        const std::size_t i = vertex_index(vx, vy);
        if (locked.contains(i)) {
          pivot = i;
          break;
        }
        if (const auto it = order.find(i); it != order.end() && it->second > best) best = it->second, pivot = i;
      }
      if (!pivot) {  // a tile that was already impossible before this stroke: leave it
        region.erase({tx, ty});
        continue;
      }
      const Terrain target = vertices_[*pivot];
      for (const auto& [vx, vy] : corners) {
        const std::size_t i = vertex_index(vx, vy);
        if (locked.contains(i) || vertices_[i] == target || !paintable(vx, vy)) continue;
        change(vx, vy, step_toward(vertices_[i], target));
        order[i] = ++tick;
        touch(vx, vy);
      }
    }
  }
  if (!stable) {
    revert();
    return false;
  }

  // Recompose the tiles that changed look.
  Step step;
  step.id = 0;
  for (const auto& [i, old] : old_values)
    if (vertices_[i] != old) step.vertices.push_back({i, old, vertices_[i]});
  for (const auto& [tx, ty] : region) {
    if (tile_frozen(tx, ty)) continue;
    const Quad q = quad_at(tx, ty);
    const auto base = compose(q);
    if (!base) continue;
    const std::size_t ti = tile_index(tx, ty);
    const std::uint16_t old_code = map_.tiles[ti];
    std::uint16_t code = old_code;
    if ((old_code & 0xfff0) != *base) {
      const auto picked = tileset().pick(*base, tile_hash(tx, ty, 7));
      if (!picked) continue;
      code = *picked;
    }
    const std::uint16_t old_move = map_.movement.empty() ? 0 : map_.movement[ti];
    const std::uint16_t new_move = movement_of(q);
    if (code == old_code && (map_.movement.empty() || old_move == new_move)) continue;
    step.tiles.push_back({ti, old_code, code, old_move, new_move});
  }
  if (step.empty()) return true;
  apply(step, true);
  commit(std::move(step));
  return true;
}

bool MapEdit::Objects::operator==(const Objects& o) const {
  if (starts.size() != o.starts.size() || mines.size() != o.mines.size()) return false;
  for (std::size_t i = 0; i < starts.size(); ++i)
    if (starts[i].owner != o.starts[i].owner || !(starts[i].at == o.starts[i].at) || starts[i].hall != o.starts[i].hall) return false;
  for (std::size_t i = 0; i < mines.size(); ++i)
    if (!(mines[i].at == o.mines[i].at) || mines[i].resource != o.mines[i].resource || mines[i].amount != o.mines[i].amount) return false;
  return true;
}

bool MapEdit::change_objects(const std::function<void(Objects&)>& edit, const std::string& key) {
  ++revision_;
  Objects before{map_.starts, map_.mines};
  Objects after = before;
  edit(after);
  if (after == before) return false;
  map_.starts = after.starts;
  map_.mines = after.mines;
  redo_.clear();
  if (!key.empty() && key == object_key_ && !undo_.empty() && undo_.back().objects && undo_.back().key == key) {
    undo_.back().objects->second = std::move(after);  // the same field again: one step
    return true;
  }
  Step step;
  step.id = next_id_++;
  step.objects = std::make_pair(std::move(before), std::move(after));
  step.key = key;
  object_key_ = key;
  undo_.push_back(std::move(step));
  return true;
}

std::size_t MapEdit::add_start(int owner, Point at) {
  const std::size_t index = map_.starts.size();
  change_objects([&](Objects& o) { o.starts.push_back({owner, at, std::nullopt}); }, {});
  return index;
}
bool MapEdit::move_start(std::size_t i, Point at) {
  return i < map_.starts.size() && change_objects([&](Objects& o) { o.starts[i].at = at; }, "start" + std::to_string(i));
}
bool MapEdit::set_start(std::size_t i, int owner, std::optional<Point> hall) {
  return i < map_.starts.size() && change_objects([&](Objects& o) { o.starts[i].owner = owner; o.starts[i].hall = hall; }, "startp" + std::to_string(i));
}
bool MapEdit::remove_start(std::size_t i) {
  return i < map_.starts.size() && change_objects([&](Objects& o) { o.starts.erase(o.starts.begin() + static_cast<std::ptrdiff_t>(i)); }, {});
}
std::size_t MapEdit::add_mine(Point at, const std::string& resource, int amount) {
  const std::size_t index = map_.mines.size();
  change_objects([&](Objects& o) { o.mines.push_back({at, resource, amount}); }, {});
  return index;
}
bool MapEdit::move_mine(std::size_t i, Point at) {
  return i < map_.mines.size() && change_objects([&](Objects& o) { o.mines[i].at = at; }, "mine" + std::to_string(i));
}
bool MapEdit::set_mine(std::size_t i, const std::string& resource, int amount) {
  return i < map_.mines.size() && change_objects([&](Objects& o) { o.mines[i].resource = resource; o.mines[i].amount = amount; }, "minep" + std::to_string(i));
}
bool MapEdit::remove_mine(std::size_t i) {
  return i < map_.mines.size() && change_objects([&](Objects& o) { o.mines.erase(o.mines.begin() + static_cast<std::ptrdiff_t>(i)); }, {});
}

std::vector<std::string> MapEdit::problems(const ResourceTable& resources, const std::string& level_kind) const {
  std::vector<std::string> out;
  const auto tilemap = tilemap_from(map_, sets_);
  if (!tilemap) return {tilemap.error()};
  const Grid land = tilemap->land_blocked();
  const Grid water = tilemap->water_blocked();
  const auto inside = [&](Point p, int size) { return p.x >= 0 && p.y >= 0 && p.x + size <= map_.width && p.y + size <= map_.height; };

  std::set<int> owners;
  for (std::size_t i = 0; i < map_.starts.size(); ++i) {
    const Start& s = map_.starts[i];
    const std::string who = "start of player " + std::to_string(s.owner + 1);
    if (!owners.insert(s.owner).second) out.push_back("two starts for player " + std::to_string(s.owner + 1));
    if (!inside(s.at, 1)) {
      out.push_back(who + " is outside the map");
      continue;
    }
    if (level_kind != "naval" && land.is_blocked(s.at)) out.push_back(who + " is on ground that units cannot walk on");
  }
  if (!owners.contains(0)) out.push_back("there is no start for player 1 (you)");
  if (owners.size() < 2) out.push_back("there must be a start for an opponent too");

  std::vector<Point> taken;  // tiles covered by mines
  for (std::size_t i = 0; i < map_.mines.size(); ++i) {
    const Mine& m = map_.mines[i];
    const std::string who = "mine " + std::to_string(i + 1) + " (" + m.resource + ")";
    const ResourceId r = resources.index(m.resource);
    if (r == kNoResource) {
      out.push_back(who + ": the game has no resource '" + m.resource + "'");
      continue;
    }
    if (m.amount <= 0) out.push_back(who + " holds nothing");
    if (!inside(m.at, 3)) {
      out.push_back(who + " is outside the map");
      continue;
    }
    const bool at_sea = resources.at(r).site_terrain == SiteTerrain::Water;
    for (int dy = 0; dy < 3; ++dy)
      for (int dx = 0; dx < 3; ++dx) {
        const Point p{m.at.x + dx, m.at.y + dy};
        if (at_sea ? water.is_blocked(p) : land.is_blocked(p)) {
          out.push_back(who + (at_sea ? " is not on open water" : " is on ground that cannot hold it"));
          dy = 3;
          break;
        }
        if (std::ranges::find(taken, p) != taken.end()) {
          out.push_back(who + " overlaps another mine");
          dy = 3;
          break;
        }
        taken.push_back(p);
      }
  }

  if (level_kind == "land" && owners.size() >= 2 && out.empty()) {
    Grid walk = land;
    for (const Mine& m : map_.mines)
      for (int dy = 0; dy < 3; ++dy)
        for (int dx = 0; dx < 3; ++dx) walk.set_blocked({m.at.x + dx, m.at.y + dy});
    const Scenario scenario = scenario_from(map_);
    const auto far = scenario.farthest_from(0);
    if (far && scenario.starts.size() > *far) {
      const Point a = scenario.starts[0].start, b = scenario.starts[*far].start;
      const auto path = a_star_closest(walk, a, b);
      if (path.empty() || std::abs(path.back().x - b.x) + std::abs(path.back().y - b.y) > 4)
        out.push_back("the bases are not connected on foot (a \"land\" level needs a way between them; trees and water block)");
    }
  }
  return out;
}

void MapEdit::apply(const Step& step, bool forward) {
  if (step.objects) {
    const Objects& to = forward ? step.objects->second : step.objects->first;
    map_.starts = to.starts;
    map_.mines = to.mines;
  }
  for (const VertexChange& c : step.vertices) vertices_[c.index] = forward ? c.new_value : c.old_value;
  for (const TileChange& c : step.tiles) {
    map_.tiles[c.index] = forward ? c.new_code : c.old_code;
    if (!map_.movement.empty()) map_.movement[c.index] = forward ? c.new_move : c.old_move;
  }
}

void MapEdit::commit(Step step) {
  // paint() leaves the vertices already changed (it needed them to check the tiles): put them back, then apply the whole step.
  if (in_stroke_) {
    for (const VertexChange& c : step.vertices) {
      const auto found = std::ranges::find(stroke_.vertices, c.index, &VertexChange::index);
      if (found != stroke_.vertices.end()) found->new_value = c.new_value;
      else stroke_.vertices.push_back(c);
    }
    for (const TileChange& c : step.tiles) {
      const auto found = std::ranges::find(stroke_.tiles, c.index, &TileChange::index);
      if (found != stroke_.tiles.end()) found->new_code = c.new_code, found->new_move = c.new_move;
      else stroke_.tiles.push_back(c);
    }
    return;
  }
  step.id = next_id_++;
  object_key_.clear();
  redo_.clear();
  undo_.push_back(std::move(step));
}

void MapEdit::begin_stroke() {
  in_stroke_ = true;
  stroke_ = {};
}

void MapEdit::end_stroke() {
  if (!in_stroke_) return;
  in_stroke_ = false;
  if (stroke_.empty()) return;
  stroke_.id = next_id_++;
  redo_.clear();
  undo_.push_back(std::move(stroke_));
  stroke_ = {};
}

bool MapEdit::undo() {
  ++revision_;
  object_key_.clear();
  if (undo_.empty() || in_stroke_) return false;
  Step step = std::move(undo_.back());
  undo_.pop_back();
  apply(step, false);
  redo_.push_back(std::move(step));
  return true;
}

bool MapEdit::redo() {
  ++revision_;
  object_key_.clear();
  if (redo_.empty() || in_stroke_) return false;
  Step step = std::move(redo_.back());
  redo_.pop_back();
  apply(step, true);
  undo_.push_back(std::move(step));
  return true;
}

}  // namespace myrts::editor
