#include "sounds.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <set>

namespace myrts {

namespace {

using json = nlohmann::json;

constexpr std::pair<const char*, SoundEvent> kEventNames[] = {
    {"selected", SoundEvent::Selected}, {"acknowledge", SoundEvent::Acknowledge}, {"annoyed", SoundEvent::Annoyed},
    {"ready", SoundEvent::Ready},       {"help", SoundEvent::Help},               {"dead", SoundEvent::Dead},
    {"attack", SoundEvent::Attack},     {"work_done", SoundEvent::WorkDone},
};

std::vector<std::string> strings(const json& j) {
  std::vector<std::string> out;
  for (const auto& v : j) out.push_back(v.get<std::string>());
  return out;
}

}  // namespace

std::expected<SoundTable, std::string> SoundTable::parse(std::string_view json_text) {
  SoundTable table;
  try {
    const json doc = json::parse(json_text);
    const json empty = json::object();
    const json& voices = doc.contains("voices") ? doc.at("voices") : empty;

    // A voice is a set of events; "base" names another voice that supplies the events it does not have.
    const auto resolve_voice = [&](const std::string& name) {
      EventMap out;
      std::string current = name;
      for (int depth = 0; !current.empty() && depth < 8; ++depth) {  // follow the chain of bases
        if (!voices.contains(current)) throw std::runtime_error("unknown voice '" + current + "'");
        const json& v = voices.at(current);
        for (const auto& [key, event] : kEventNames)
          if (v.contains(key) && !out.contains(event)) out[event] = strings(v.at(key));
        current = v.value("base", std::string{});
      }
      return out;
    };

    if (doc.contains("units"))
      for (const auto& [id, u] : doc.at("units").items()) {
        EventMap events = u.contains("voice") ? resolve_voice(u.at("voice").get<std::string>()) : EventMap{};
        for (const auto& [key, event] : kEventNames)
          if (u.contains(key)) events[event] = strings(u.at(key));  // the unit's own events win
        table.units_[id] = std::move(events);
      }
    if (doc.contains("buildings"))
      for (const auto& [id, b] : doc.at("buildings").items()) table.buildings_[id] = strings(b.at("selected"));
    if (doc.contains("effects"))
      for (const auto& [name, files] : doc.at("effects").items()) table.effects_[name] = strings(files);
  } catch (const std::exception& e) {
    return std::unexpected(std::string("invalid sounds: ") + e.what());
  }
  return table;
}

std::expected<SoundTable, std::string> SoundTable::load_file(const std::string& path) {
  std::ifstream f(path);
  if (!f) return std::unexpected("cannot open " + path);
  const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  return parse(text);
}

const std::vector<std::string>& SoundTable::unit(std::string_view unit_id, SoundEvent event) const {
  static const std::vector<std::string> kNone;
  const auto u = units_.find(unit_id);
  if (u == units_.end()) return kNone;
  const auto e = u->second.find(event);
  return e == u->second.end() ? kNone : e->second;
}

const std::vector<std::string>& SoundTable::building(std::string_view building_id) const {
  static const std::vector<std::string> kNone;
  const auto it = buildings_.find(building_id);
  return it == buildings_.end() ? kNone : it->second;
}

const std::vector<std::string>& SoundTable::effect(std::string_view name) const {
  static const std::vector<std::string> kNone;
  const auto it = effects_.find(name);
  return it == effects_.end() ? kNone : it->second;
}

std::vector<std::string> SoundTable::all_files() const {
  std::set<std::string> files;
  for (const auto& [_, events] : units_)
    for (const auto& [__, list] : events) files.insert(list.begin(), list.end());
  for (const auto& [_, list] : buildings_) files.insert(list.begin(), list.end());
  for (const auto& [_, list] : effects_) files.insert(list.begin(), list.end());
  return {files.begin(), files.end()};
}

std::string_view SoundTable::pick(const std::vector<std::string>& files, std::uint32_t seed) {
  if (files.empty()) return {};
  return files[seed % files.size()];
}

SpatialSound spatial_sound(const Rect& view, float x, float y) {
  SpatialSound s;
  const float centre_x = view.x + view.w / 2;
  s.pan = std::clamp((x - centre_x) / (view.w / 2) * 0.7f, -0.9f, 0.9f);

  // Distance beyond the edge of the screen (0 if on screen).
  const float dx = std::max({view.x - x, 0.0f, x - (view.x + view.w)});
  const float dy = std::max({view.y - y, 0.0f, y - (view.y + view.h)});
  const float outside = std::hypot(dx, dy);
  s.gain = std::clamp(1.0f - outside / (view.w * 0.75f), 0.0f, 1.0f);
  return s;
}

}  // namespace myrts
