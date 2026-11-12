#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>
#include <vector>

#include "core/scene_stack.hpp"

namespace {

using Log = std::vector<std::string>;

struct FakeScene {
  FakeScene(std::string n, Log& l, bool overlay = false) : name(std::move(n)), log(l), overlay_(overlay) {}
  ~FakeScene() { log.push_back(name + ":destroyed"); }
  void on_enter() { log.push_back(name + ":enter"); }
  void on_exit() { log.push_back(name + ":exit"); }
  void on_pause() { log.push_back(name + ":pause"); }
  void on_resume() { log.push_back(name + ":resume"); }
  [[nodiscard]] bool is_overlay() const { return overlay_; }

  std::string name;
  Log& log;
  bool overlay_;
};

std::unique_ptr<FakeScene> make(const char* name, Log& log, bool overlay = false) {
  return std::make_unique<FakeScene>(name, log, overlay);
}

}  // namespace

TEST_CASE("push pauses the scene below, pop resumes it") {
  Log log;
  myrts::SceneStack<FakeScene> s;
  s.push(make("game", log));
  s.push(make("pause", log, true));
  s.pop();
  CHECK(log == Log{"game:enter", "game:pause", "pause:enter", "pause:exit", "pause:destroyed", "game:resume"});
  CHECK(s.size() == 1);
}

TEST_CASE("replace exits the old scene before destroying it and enters the new one") {
  Log log;
  myrts::SceneStack<FakeScene> s;
  s.push(make("menu", log));
  log.clear();
  s.replace(make("game", log));
  CHECK(log == Log{"menu:exit", "menu:destroyed", "game:enter"});
  CHECK(s.size() == 1);
}

TEST_CASE("clear exits scenes top-down and leaves the stack empty") {
  Log log;
  myrts::SceneStack<FakeScene> s;
  s.push(make("a", log));
  s.push(make("b", log));
  log.clear();
  s.clear();
  CHECK(log == Log{"b:exit", "b:destroyed", "a:exit", "a:destroyed"});
  CHECK(s.empty());
  CHECK(s.top() == nullptr);
}

TEST_CASE("destroying the stack with scenes in it does not leak them") {
  Log log;
  {
    myrts::SceneStack<FakeScene> s;
    s.push(make("a", log));
  }
  CHECK(log.back() == "a:destroyed");
}

TEST_CASE("only overlays let the scene below show through") {
  Log log;
  myrts::SceneStack<FakeScene> s;
  s.push(make("intro", log));
  s.push(make("game", log));
  s.push(make("pause", log, true));
  s.push(make("confirm", log, true));
  std::vector<std::string> drawn;
  s.for_each_visible([&](FakeScene& sc) { drawn.push_back(sc.name); });
  CHECK(drawn == std::vector<std::string>{"game", "pause", "confirm"});  // "intro" is hidden by opaque "game"
}

TEST_CASE("popping an empty stack is harmless") {
  myrts::SceneStack<FakeScene> s;
  s.pop();
  s.clear();
  CHECK(s.empty());
}
