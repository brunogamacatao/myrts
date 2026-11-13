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
  /// Mute silences everything, music and effects alike.
  void set_muted(bool muted) {
    muted_ = muted;
    apply_music_gain();
  }
  /// Master volume, 0..1.
  [[nodiscard]] float volume() const { return volume_; }
  void set_volume(float v) { volume_ = clamp01(v); }

  /// The music has a volume of its own, independent of the effects.
  [[nodiscard]] float music_volume() const { return music_volume_; }
  void set_music_volume(float v) {
    music_volume_ = clamp01(v);
    apply_music_gain();
  }
  /// Streams `file` (Ogg Vorbis, a path from the working directory), looping or once. Replaces the current track, but
  /// asking again for the one already playing leaves it alone (so a menu opened over a level does not restart it).
  void play_music(const std::string& file, bool loop);
  void stop_music();

  /// Plays `file` (a name in the sound directory) at `gain` 0..1 panned -1 (left) .. 1 (right). Loads it on first use.
  void play(const std::string& file, float gain = 1.0f, float pan = 0.0f);

 private:
  ALLEGRO_SAMPLE* sample(const std::string& file);
  void apply_music_gain();
  static float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

  std::string directory_;
  std::unordered_map<std::string, ALLEGRO_SAMPLE*> samples_;  // nullptr remembers a file that would not load
  bool ready_ = false;
  bool muted_ = false;
  float volume_ = 0.8f;
  float music_volume_ = 0.6f;
  ALLEGRO_AUDIO_STREAM* music_ = nullptr;
  std::string music_file_;
  bool music_loop_ = false;
};
