#include "mapfile.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <nlohmann/json.hpp>

namespace myrts {

namespace {
using json = nlohmann::json;

constexpr int kFormat = 1;
constexpr const char* kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const std::vector<std::uint8_t>& in) {
  std::string out;
  for (std::size_t i = 0; i < in.size(); i += 3) {
    const std::uint32_t n = (static_cast<std::uint32_t>(in[i]) << 16) | (i + 1 < in.size() ? static_cast<std::uint32_t>(in[i + 1]) << 8 : 0u) |
                            (i + 2 < in.size() ? in[i + 2] : 0u);
    out += kAlphabet[(n >> 18) & 63];
    out += kAlphabet[(n >> 12) & 63];
    out += i + 1 < in.size() ? kAlphabet[(n >> 6) & 63] : '=';
    out += i + 2 < in.size() ? kAlphabet[n & 63] : '=';
  }
  return out;
}

std::expected<std::vector<std::uint8_t>, std::string> base64_decode(const std::string& in) {
  std::vector<std::uint8_t> out;
  std::uint32_t acc = 0;
  int bits = 0;
  for (const char c : in) {
    if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
    const char* at = std::find(kAlphabet, kAlphabet + 64, c);
    if (at == kAlphabet + 64) return std::unexpected(std::string("tiles: not base64 (bad character '") + c + "')");
    acc = (acc << 6) | static_cast<std::uint32_t>(at - kAlphabet);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<std::uint8_t>((acc >> bits) & 0xff));
    }
  }
  return out;
}

std::string pack16(const std::vector<std::uint16_t>& v) {
  std::vector<std::uint8_t> bytes;
  bytes.reserve(v.size() * 2);
  for (const std::uint16_t x : v) {
    bytes.push_back(static_cast<std::uint8_t>(x & 0xff));
    bytes.push_back(static_cast<std::uint8_t>(x >> 8));
  }
  return base64_encode(bytes);
}

std::expected<std::vector<std::uint16_t>, std::string> unpack16(const std::string& text, std::size_t expected, const char* what) {
  const auto bytes = base64_decode(text);
  if (!bytes) return std::unexpected(bytes.error());
  if (bytes->size() != expected * 2)
    return std::unexpected(std::string(what) + ": " + std::to_string(bytes->size() / 2) + " values, expected " + std::to_string(expected));
  std::vector<std::uint16_t> out(expected);
  for (std::size_t i = 0; i < expected; ++i)
    out[i] = static_cast<std::uint16_t>((*bytes)[i * 2] | ((*bytes)[i * 2 + 1] << 8));
  return out;
}

json point(Point p) { return json::array({p.x, p.y}); }
Point to_point(const json& j) { return {j.at(0).get<int>(), j.at(1).get<int>()}; }

}  // namespace

std::expected<MapFile, std::string> map_from_pud(const PudMap& pud, const TileSets& sets, const ResourceTable& resources,
                                                 const GameRules& rules) {
  if (!sets.has(pud.era)) return std::unexpected("the map uses a terrain style the game has no tile set for");
  MapFile m;
  m.name = pud.description;
  m.tileset = sets.era(pud.era).name();
  m.width = pud.width;
  m.height = pud.height;
  m.tiles = pud.tiles;
  if (pud.movement.size() == pud.tiles.size()) m.movement = pud.movement;

  const Scenario s = scenario_from(pud, rules);
  for (const StartSlot& slot : s.starts) m.starts.push_back({slot.owner, slot.start, slot.hall});
  for (const MineSpec& mine : s.mines) {
    const ResourceId r = resources.from_pud(mine.source);
    if (r != kNoResource) m.mines.push_back({mine.origin, resources.at(r).id, mine.gold});
  }
  return m;
}

std::string write_mrmap(const MapFile& map) {
  json doc;
  doc["format_version"] = kFormat;
  doc["name"] = map.name;
  doc["tileset"] = map.tileset;
  doc["width"] = map.width;
  doc["height"] = map.height;
  doc["tiles"] = pack16(map.tiles);
  if (!map.movement.empty()) doc["movement"] = pack16(map.movement);
  json starts = json::array();
  for (const auto& s : map.starts) {
    json j = {{"owner", s.owner}, {"at", point(s.at)}};
    if (s.hall) j["hall"] = point(*s.hall);
    starts.push_back(std::move(j));
  }
  doc["starts"] = std::move(starts);
  json mines = json::array();
  for (const auto& m : map.mines) mines.push_back({{"at", point(m.at)}, {"resource", m.resource}, {"amount", m.amount}});
  doc["mines"] = std::move(mines);
  return doc.dump(1) + "\n";
}

std::expected<MapFile, std::string> parse_mrmap(const std::string& text, const TileSets& sets) {
  try {
    const json doc = json::parse(text);
    if (doc.value("format_version", kFormat) > kFormat) return std::unexpected("this map is newer than the engine understands");
    MapFile m;
    m.name = doc.value("name", std::string{});
    m.tileset = doc.at("tileset").get<std::string>();
    if (!sets.find(m.tileset)) return std::unexpected("unknown tile set '" + m.tileset + "'");
    m.width = doc.at("width").get<int>();
    m.height = doc.at("height").get<int>();
    if (m.width < 1 || m.height < 1 || m.width > 1024 || m.height > 1024) return std::unexpected("the map size must be between 1 and 1024 tiles");
    const auto count = static_cast<std::size_t>(m.width) * static_cast<std::size_t>(m.height);
    auto tiles = unpack16(doc.at("tiles").get<std::string>(), count, "tiles");
    if (!tiles) return std::unexpected(tiles.error());
    m.tiles = std::move(*tiles);
    if (doc.contains("movement")) {
      auto movement = unpack16(doc.at("movement").get<std::string>(), count, "movement");
      if (!movement) return std::unexpected(movement.error());
      m.movement = std::move(*movement);
    }
    const auto inside = [&](Point p) { return p.x >= 0 && p.y >= 0 && p.x < m.width && p.y < m.height; };
    if (doc.contains("starts"))
      for (const json& s : doc.at("starts")) {
        MapFile::Start start{s.at("owner").get<int>(), to_point(s.at("at")), std::nullopt};
        if (s.contains("hall")) start.hall = to_point(s.at("hall"));
        if (!inside(start.at)) return std::unexpected("a start location lies outside the map");
        m.starts.push_back(start);
      }
    if (doc.contains("mines"))
      for (const json& j : doc.at("mines")) {
        MapFile::Mine mine{to_point(j.at("at")), j.at("resource").get<std::string>(), j.at("amount").get<int>()};
        if (!inside(mine.at)) return std::unexpected("a mine lies outside the map");
        if (mine.amount < 0) return std::unexpected("a mine cannot hold a negative amount");
        m.mines.push_back(std::move(mine));
      }
    return m;
  } catch (const std::exception& e) {
    return std::unexpected(std::string("invalid map: ") + e.what());
  }
}

std::expected<MapFile, std::string> load_map(const std::string& path, const TileSets& sets, const ResourceTable& resources,
                                             const GameRules& rules) {
  if (path.ends_with(".mrmap")) {
    std::ifstream f(path);
    if (!f) return std::unexpected("cannot open " + path);
    auto m = parse_mrmap(std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()), sets);
    if (!m) return std::unexpected(path + ": " + m.error());
    return m;
  }
  const auto pud = load_pud(path);
  if (!pud) return std::unexpected(pud.error());
  return map_from_pud(*pud, sets, resources, rules);
}

std::expected<TileMap, std::string> tilemap_from(const MapFile& map, std::shared_ptr<const TileSets> sets) {
  const auto era = sets->find(map.tileset);
  if (!era) return std::unexpected("unknown tile set '" + map.tileset + "'");
  PudMap carrier;  // the terrain half of a PudMap is exactly what a TileMap is built from
  carrier.era = *era;
  carrier.width = map.width;
  carrier.height = map.height;
  carrier.tiles = map.tiles;
  carrier.movement = map.movement;
  return TileMap::from_pud(carrier, std::move(sets));
}

Scenario scenario_from(const MapFile& map) {
  Scenario s;
  std::map<int, StartSlot> by_owner;
  for (const auto& st : map.starts) by_owner[st.owner] = {st.owner, st.at, st.hall};
  for (const auto& [_, slot] : by_owner) s.starts.push_back(slot);
  for (const auto& m : map.mines) s.mines.push_back({m.at, m.amount, m.resource});
  return s;
}

}  // namespace myrts
