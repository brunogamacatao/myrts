#include "audio.hpp"

#include <allegro5/allegro_acodec.h>

#include <cstdio>

bool Audio::init(std::string directory) {
  directory_ = std::move(directory);
  if (!al_install_audio() || !al_init_acodec_addon()) {
    std::fprintf(stderr, "warning: no audio, the game will be silent\n");
    return false;
  }
  if (!al_reserve_samples(16)) {
    std::fprintf(stderr, "warning: couldn't open the sound device, the game will be silent\n");
    return false;
  }
  ready_ = true;
  return true;
}

void Audio::shutdown() {
  if (!ready_) return;
  al_stop_samples();
  for (auto& [_, s] : samples_)
    if (s) al_destroy_sample(s);
  samples_.clear();
  ready_ = false;
}

ALLEGRO_SAMPLE* Audio::sample(const std::string& file) {
  const auto it = samples_.find(file);
  if (it != samples_.end()) return it->second;
  ALLEGRO_SAMPLE* s = al_load_sample((directory_ + "/" + file).c_str());
  if (!s) std::fprintf(stderr, "warning: couldn't load sound %s\n", file.c_str());
  samples_[file] = s;  // failures are remembered so they warn only once
  return s;
}

void Audio::play(const std::string& file, float gain, float pan) {
  if (!ready_ || muted_ || gain <= 0.0f) return;
  if (ALLEGRO_SAMPLE* s = sample(file)) al_play_sample(s, gain, pan, 1.0f, ALLEGRO_PLAYMODE_ONCE, nullptr);
}
