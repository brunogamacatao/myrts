#include "fog_settings.hpp"

#include <nlohmann/json.hpp>
#include <set>

namespace myrts {

namespace {
using json = nlohmann::json;

void number(const json& j, const char* key, double& out, double lo, double hi) {
  if (!j.contains(key)) return;
  if (!j.at(key).is_number()) throw std::runtime_error(std::string("fog.") + key + " must be a number");
  const double v = j.at(key).get<double>();
  if (v < lo || v > hi)
    throw std::runtime_error(std::string("fog.") + key + " must be between " + std::to_string(lo) + " and " + std::to_string(hi));
  out = v;
}

void whole(const json& j, const char* key, int& out, int lo, int hi) {
  double v = out;
  if (!j.contains(key)) return;
  number(j, key, v, lo, hi);
  out = static_cast<int>(v);
}

void flag(const json& j, const char* key, bool& out) {
  if (!j.contains(key)) return;
  if (!j.at(key).is_boolean()) throw std::runtime_error(std::string("fog.") + key + " must be true or false");
  out = j.at(key).get<bool>();
}

void color(const json& j, const char* key, std::array<int, 3>& out) {
  if (!j.contains(key)) return;
  const json& c = j.at(key);
  if (!c.is_array() || c.size() != 3) throw std::runtime_error(std::string("fog.") + key + " must be [red, green, blue]");
  for (std::size_t i = 0; i < 3; ++i) {
    if (!c[i].is_number() || c[i].get<double>() < 0 || c[i].get<double>() > 255)
      throw std::runtime_error(std::string("fog.") + key + " values must be between 0 and 255");
    out[i] = static_cast<int>(c[i].get<double>());
  }
}
}  // namespace

std::expected<FogSettings, std::string> FogSettings::with(const FogSettings& base, std::string_view overrides) {
  FogSettings s = base;
  if (overrides.empty()) return s;
  try {
    const json j = json::parse(overrides);
    static const std::set<std::string> keys = {"enabled", "reveal_map", "remember", "sight_scale", "sight_bonus", "min_unit_sight",
                                               "building_sight_base", "update_interval", "minimap", "unknown_color",
                                               "unknown_opacity", "explored_color", "explored_opacity", "edge_softness"};
    for (const auto& [key, _] : j.items())
      if (!keys.contains(key)) throw std::runtime_error("unknown key fog." + key);
    flag(j, "enabled", s.enabled);
    flag(j, "reveal_map", s.reveal_map);
    flag(j, "remember", s.remember);
    number(j, "sight_scale", s.sight_scale, 0.0, 20.0);
    whole(j, "sight_bonus", s.sight_bonus, -20, 40);
    whole(j, "min_unit_sight", s.min_unit_sight, -1, 40);
    whole(j, "building_sight_base", s.building_sight_base, -1, 40);
    number(j, "update_interval", s.update_interval, 0.02, 5.0);
    flag(j, "minimap", s.minimap);
    color(j, "unknown_color", s.unknown_color);
    number(j, "unknown_opacity", s.unknown_opacity, 0.0, 1.0);
    color(j, "explored_color", s.explored_color);
    number(j, "explored_opacity", s.explored_opacity, 0.0, 1.0);
    number(j, "edge_softness", s.edge_softness, 0.1, 5.0);
    return s;
  } catch (const std::exception& e) {
    return std::unexpected(std::string("fog: ") + e.what());
  }
}

}  // namespace myrts
