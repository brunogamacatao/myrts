#pragma once

#include <cstdint>
#include <expected>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "geometry.hpp"

namespace myrts {

/// The moments a unit or building speaks (the vocabulary of Warcraft II and of the Wargus engine).
enum class SoundEvent {
  Selected,     // clicked to select it
  Acknowledge,  // given an order ("yes, sir")
  Annoyed,      // clicked over and over without an order
  Ready,        // finished training
  Help,         // under attack
  Dead,         // dies
  Attack,       // a strike (weapon effect)
  WorkDone,     // a construction finished (spoken by the builder's side)
};

/// Which sound files play for which events: loaded from assets/data/sounds.json.
///
/// Voices are shared sets ("orc_basic": what every grunt, and peon says); a unit names its voice and may override any
/// event; buildings only have a selection sound; effects are named global sounds (construction, errors, chopping...).
/// Every event can list several files: one is picked at random (or in order, for "annoyed").
class SoundTable {
 public:
  static std::expected<SoundTable, std::string> parse(std::string_view json_text);
  static std::expected<SoundTable, std::string> load_file(const std::string& path);

  /// Files for `event` of unit type `unit_id` (empty if none).
  [[nodiscard]] const std::vector<std::string>& unit(std::string_view unit_id, SoundEvent event) const;
  /// The sound played when building type `building_id` is selected (empty if none).
  [[nodiscard]] const std::vector<std::string>& building(std::string_view building_id) const;
  /// A named effect ("construct", "error", "tree", "building_destroyed"...).
  [[nodiscard]] const std::vector<std::string>& effect(std::string_view name) const;

  [[nodiscard]] bool knows_unit(std::string_view id) const { return units_.contains(std::string(id)); }
  [[nodiscard]] bool knows_building(std::string_view id) const { return buildings_.contains(std::string(id)); }
  /// Every file the table refers to, without duplicates (for importing and verifying the sound files).
  [[nodiscard]] std::vector<std::string> all_files() const;

  /// One of `files` chosen by `seed`; empty if the list is empty.
  [[nodiscard]] static std::string_view pick(const std::vector<std::string>& files, std::uint32_t seed);

 private:
  using EventMap = std::map<SoundEvent, std::vector<std::string>>;
  std::map<std::string, EventMap, std::less<>> units_;
  std::map<std::string, std::vector<std::string>, std::less<>> buildings_;
  std::map<std::string, std::vector<std::string>, std::less<>> effects_;
};

/// How loud and where a sound is heard, given the part of the map on screen.
struct SpatialSound {
  float gain = 1.0f;  // 0 (inaudible) .. 1
  float pan = 0.0f;   // -1 left .. 1 right
  [[nodiscard]] bool audible() const { return gain > 0.04f; }
};

/// Sounds on screen play at full volume panned by where they are; the ones off screen fade out over about three quarters
/// of a screen width and then vanish.
[[nodiscard]] SpatialSound spatial_sound(const Rect& view, float x, float y);

}  // namespace myrts
