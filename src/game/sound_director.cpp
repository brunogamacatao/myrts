#include "sound_director.hpp"

using myrts::SoundEvent;

void SoundDirector::play(const std::vector<std::string>& files, std::optional<myrts::Vec2> at, double cooldown, bool in_order,
                         int index) {
  if (files.empty() || !audio_) return;
  myrts::SpatialSound where;
  if (at) where = myrts::spatial_sound(view_, at->x, at->y);
  if (!where.audible()) return;

  counter_ = counter_ * 1664525u + 1013904223u;  // a cheap generator, plenty to choose between lines
  const std::string_view chosen = in_order ? std::string_view(files[static_cast<std::size_t>(index) % files.size()])
                                           : myrts::SoundTable::pick(files, counter_ >> 8);
  const std::string file(chosen);
  if (const auto it = last_.find(file); it != last_.end() && now_ - it->second < cooldown) return;
  last_[file] = now_;
  audio_->play(file, where.gain, where.pan);
}

void SoundDirector::unit(const myrts::UnitDef& def, SoundEvent event, std::optional<myrts::Vec2> at) {
  // Calls for help are rationed: one every few seconds, however many units are hit.
  double cooldown = 0.12;
  if (event == SoundEvent::Help) {
    const auto it = last_.find("#help");
    if (it != last_.end() && now_ - it->second < 6.0) return;
    last_["#help"] = now_;
    cooldown = 0;
  }
  play(table_->unit(def.id, event), at, cooldown);
}

void SoundDirector::annoyed(const myrts::UnitDef& def, int times) {
  play(table_->unit(def.id, SoundEvent::Annoyed), std::nullopt, 0.0, true, times);
}

void SoundDirector::building(const std::string& building_id) { play(table_->building(building_id), std::nullopt, 0.1); }

void SoundDirector::effect(const std::string& name, std::optional<myrts::Vec2> at) { play(table_->effect(name), at, 0.1); }
