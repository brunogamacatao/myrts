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
  stop_music();
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
  if (ALLEGRO_SAMPLE* s = sample(file)) al_play_sample(s, gain * volume_, pan, 1.0f, ALLEGRO_PLAYMODE_ONCE, nullptr);
}

void Audio::apply_music_gain() {
  if (music_) al_set_audio_stream_gain(music_, muted_ ? 0.0f : music_volume_);
}

void Audio::stop_music() {
  if (music_) {
    al_detach_audio_stream(music_);
    al_destroy_audio_stream(music_);
    music_ = nullptr;
  }
  music_file_.clear();
}

void Audio::play_music(const std::string& file, bool loop) {
  if (!ready_) return;
  if (music_ && file == music_file_ && loop == music_loop_ && al_get_audio_stream_playing(music_)) return;
  stop_music();
  if (file.empty()) return;
  music_ = al_load_audio_stream(file.c_str(), 4, 2048);
  if (!music_) {
    std::fprintf(stderr, "warning: couldn't load music %s\n", file.c_str());
    return;
  }
  al_set_audio_stream_playmode(music_, loop ? ALLEGRO_PLAYMODE_LOOP : ALLEGRO_PLAYMODE_ONCE);
  if (!al_attach_audio_stream_to_mixer(music_, al_get_default_mixer())) {
    std::fprintf(stderr, "warning: couldn't play music %s\n", file.c_str());
    al_destroy_audio_stream(music_);
    music_ = nullptr;
    return;
  }
  music_file_ = file;
  music_loop_ = loop;
  apply_music_gain();
}
