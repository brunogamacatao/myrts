#pragma once

#include <cstddef>
#include <memory>
#include <vector>

namespace myrts {

/// Stack of scenes with a well-defined lifecycle. `S` must provide
/// on_enter(), on_exit(), on_pause(), on_resume() and is_overlay().
///
/// Guarantees:
///  - on_enter() runs after a scene is on the stack; on_exit() runs *before* it is destroyed,
///    so a scene can release resources in either place.
///  - The scene underneath gets on_pause() when another is pushed and on_resume() once it is
///    the top again.
///  - clear() exits scenes top-down, mirroring the order they were entered.
template <class S>
class SceneStack {
 public:
  void push(std::unique_ptr<S> scene) {
    if (!stack_.empty()) stack_.back()->on_pause();
    stack_.push_back(std::move(scene));
    stack_.back()->on_enter();
  }

  void pop() {
    if (stack_.empty()) return;
    stack_.back()->on_exit();
    stack_.pop_back();
    if (!stack_.empty()) stack_.back()->on_resume();
  }

  /// Swaps the top scene (the one below is not touched).
  void replace(std::unique_ptr<S> scene) {
    if (!stack_.empty()) {
      stack_.back()->on_exit();
      stack_.pop_back();
    }
    stack_.push_back(std::move(scene));
    stack_.back()->on_enter();
  }

  /// Exits and destroys every scene, top first.
  void clear() {
    while (!stack_.empty()) {
      stack_.back()->on_exit();
      stack_.pop_back();
    }
  }

  [[nodiscard]] bool empty() const { return stack_.empty(); }
  [[nodiscard]] std::size_t size() const { return stack_.size(); }
  [[nodiscard]] S* top() const { return stack_.empty() ? nullptr : stack_.back().get(); }

  /// Calls `f(scene)` bottom-up for every scene that must be drawn: the top one plus
  /// the scenes it lets show through (overlays) down to the first opaque scene.
  template <class F>
  void for_each_visible(F&& f) const {
    if (stack_.empty()) return;
    std::size_t first = stack_.size() - 1;
    while (first > 0 && stack_[first]->is_overlay()) --first;
    for (std::size_t i = first; i < stack_.size(); ++i) f(*stack_[i]);
  }

 private:
  std::vector<std::unique_ptr<S>> stack_;
};

}  // namespace myrts
