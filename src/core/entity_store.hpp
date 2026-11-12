#pragma once

#include <optional>
#include <utility>
#include <vector>

#include "ids.hpp"

namespace myrts {

/// Slot map: O(1) insert/erase/lookup with stable, generation-checked ids. Unlike a plain
/// vector indexed by id, entities can be removed (units die) without invalidating anyone.
template <class T>
class EntityStore {
 public:
  /// `make(id)` builds the value, so it can store its own id.
  template <class Make>
  EntityId insert(Make&& make) {
    std::uint32_t index;
    if (!free_.empty()) {
      index = free_.back();
      free_.pop_back();
    } else {
      index = static_cast<std::uint32_t>(slots_.size());
      slots_.emplace_back();
    }
    const EntityId id{index, slots_[index].generation};
    slots_[index].value.emplace(make(id));
    ++count_;
    return id;
  }

  bool erase(EntityId id) {
    Slot* s = slot(id);
    if (!s) return false;
    s->value.reset();
    ++s->generation;
    free_.push_back(id.index);
    --count_;
    return true;
  }

  [[nodiscard]] T* get(EntityId id) {
    Slot* s = slot(id);
    return s ? &*s->value : nullptr;
  }
  [[nodiscard]] const T* get(EntityId id) const {
    const Slot* s = slot(id);
    return s ? &*s->value : nullptr;
  }
  [[nodiscard]] bool alive(EntityId id) const { return slot(id) != nullptr; }
  [[nodiscard]] std::size_t size() const { return count_; }
  [[nodiscard]] bool empty() const { return count_ == 0; }

  /// Snapshot of the live ids; safe to iterate while entities are added or erased.
  [[nodiscard]] std::vector<EntityId> ids() const {
    std::vector<EntityId> out;
    out.reserve(count_);
    for (std::uint32_t i = 0; i < slots_.size(); ++i)
      if (slots_[i].value) out.push_back({i, slots_[i].generation});
    return out;
  }

  /// Calls f(value) for every live entity. `f` must not add or erase entities.
  template <class F>
  void for_each(F&& f) {
    for (auto& s : slots_)
      if (s.value) f(*s.value);
  }
  template <class F>
  void for_each(F&& f) const {
    for (const auto& s : slots_)
      if (s.value) f(*s.value);
  }

 private:
  struct Slot {
    std::optional<T> value;
    std::uint32_t generation = 0;
  };

  Slot* slot(EntityId id) {
    if (!id.valid() || id.index >= slots_.size()) return nullptr;
    Slot& s = slots_[id.index];
    return (s.value && s.generation == id.generation) ? &s : nullptr;
  }
  const Slot* slot(EntityId id) const {
    if (!id.valid() || id.index >= slots_.size()) return nullptr;
    const Slot& s = slots_[id.index];
    return (s.value && s.generation == id.generation) ? &s : nullptr;
  }

  std::vector<Slot> slots_;
  std::vector<std::uint32_t> free_;
  std::size_t count_ = 0;
};

}  // namespace myrts
