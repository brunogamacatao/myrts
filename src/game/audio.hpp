#pragma once

#include <allegro5/allegro5.h>
#include <allegro5/allegro_audio.h>

#include <string>
#include <unordered_map>

/// The sound device and the WAV files it plays. If there is no sound card (or the audio addons are missing) it stays
/// silent: every call is then a no-op, so the game never depends on audio being available.
class Audio {
 public:
  Audio() = default;
  Audio(const Audio&) = delete;
  Audio& operator=(const Audio&) = delete;
  ~Audio() { shutdown(); }

  /// Starts the audio system; `directory` holds the WAV files. Returns false (and stays silent) on failure.
  bool init(std::string directory);
  void shutdown();

  [[nodiscard]] bool available() const { return ready_; }
  [[nodiscard]] bool muted() const { return muted_; }
  void set_muted(bool muted) { muted_ = muted; }

  /// Plays `file` (a name in the sound directory) at `gain` 0..1 panned -1 (left) .. 1 (right). Loads it on first use.
  void play(const std::string& file, float gain = 1.0f, float pan = 0.0f);

 private:
  ALLEGRO_SAMPLE* sample(const std::string& file);

  std::string directory_;
  std::unordered_map<std::string, ALLEGRO_SAMPLE*> samples_;  // nullptr remembers a file that would not load
  bool ready_ = false;
  bool muted_ = false;
};
