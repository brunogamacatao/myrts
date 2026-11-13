#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include "audio.hpp"
#include "core/defs.hpp"
#include "core/sounds.hpp"

/// Decides which sound plays for what happens in the match: picks a file from the SoundTable, places it in the stereo
/// field from where it happened on screen and keeps the same sound from firing again and again.
class SoundDirector {
 public:
  SoundDirector(Audio* audio, const myrts::SoundTable* table) : audio_(audio), table_(table) {}

  /// Call every frame: advances the clock and tells where the camera is looking.
  void update(double dt, const myrts::Rect& view) {
    now_ += dt;
    view_ = view;
  }

  /// A unit's voice. With `at` (world pixels) it is heard according to the distance from the screen; without, at full volume.
  void unit(const myrts::UnitDef& def, myrts::SoundEvent event, std::optional<myrts::Vec2> at = std::nullopt);
  /// The unit's "annoyed" lines in order: the first click after a selection gets the first, then the next, and so on.
  void annoyed(const myrts::UnitDef& def, int times);
  void building(const std::string& building_id);
  void effect(const std::string& name, std::optional<myrts::Vec2> at = std::nullopt);

 private:
  void play(const std::vector<std::string>& files, std::optional<myrts::Vec2> at, double cooldown, bool in_order = false,
            int index = 0);

  Audio* audio_;
  const myrts::SoundTable* table_;
  myrts::Rect view_{};
  double now_ = 0;
  std::uint32_t counter_ = 0;                      // varies the random pick
  std::unordered_map<std::string, double> last_;  // when each file last played
};
