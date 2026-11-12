#pragma once

#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "building.hpp"
#include "grid.hpp"
#include "ids.hpp"

namespace myrts::ev {

struct TreeFelled { Point tile; };
struct MineDepleted { int mine; };
struct UnitSpawned { EntityId unit; };
struct UnitDied { EntityId unit; PlayerId owner; };
/// `damage` is what was actually taken off after armor.
struct Attacked { EntityId attacker; EntityId target; bool target_is_building; int damage; };
struct BuildingDestroyed { EntityId building; PlayerId owner; };
struct BuildingPlaced { EntityId building; PlayerId owner; };
struct BuildingCompleted { EntityId building; PlayerId owner; };
struct Delivered { PlayerId player; Stock amount; };
/// A message for the player (shown in the HUD, never parsed by game logic).
struct Notice { PlayerId player; std::string text; };

}  // namespace myrts::ev

namespace myrts {

using Event = std::variant<ev::TreeFelled, ev::MineDepleted, ev::UnitSpawned, ev::UnitDied, ev::Attacked,
                           ev::BuildingPlaced, ev::BuildingCompleted, ev::BuildingDestroyed, ev::Delivered,
                           ev::Notice>;

/// Decouples "something happened" (the simulation) from "who cares" (HUD, sound, victory rules).
/// Events are queued by publish() and delivered, in order, by dispatch(), so a handler never
/// runs in the middle of a state change.
class EventBus {
 public:
  using Handler = std::function<void(const Event&)>;

  void subscribe(Handler handler) { handlers_.push_back(std::move(handler)); }
  /// Queues an event of any alternative type. Built in place (no temporary variant to move around).
  template <class E>
  void publish(E&& e) {
    if constexpr (std::is_same_v<std::decay_t<E>, Event>) {
      queue_.push_back(std::forward<E>(e));
    } else {
      queue_.emplace_back(std::in_place_type<std::decay_t<E>>, std::forward<E>(e));
    }
  }

  /// Delivers everything queued, including events published by handlers while dispatching.
  void dispatch() {
    constexpr int kMaxRounds = 16;  // guards against handlers that keep re-publishing
    for (int round = 0; round < kMaxRounds && !queue_.empty(); ++round) {
      std::vector<Event> batch;
      batch.swap(queue_);
      for (const Event& e : batch)
        for (const auto& h : handlers_) h(e);
    }
  }

  [[nodiscard]] std::size_t pending() const { return queue_.size(); }

 private:
  std::vector<Handler> handlers_;
  std::vector<Event> queue_;
};

/// Calls `f(const E&)` if `e` holds an `E`.
template <class E, class F>
bool on_event(const Event& e, F&& f) {
  if (const E* p = std::get_if<E>(&e)) {
    f(*p);
    return true;
  }
  return false;
}

}  // namespace myrts
