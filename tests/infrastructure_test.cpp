#include <catch2/catch_test_macros.hpp>
#include <memory>

#include "core/entity_store.hpp"
#include "core/events.hpp"
#include "core/resource_cache.hpp"

using namespace myrts;

TEST_CASE("EntityStore ids stay valid while others come and go") {
  EntityStore<int> s;
  const auto a = s.insert([](EntityId) { return 10; });
  const auto b = s.insert([](EntityId) { return 20; });
  REQUIRE(s.erase(a));
  CHECK_FALSE(s.alive(a));
  REQUIRE(s.get(b) != nullptr);
  CHECK(*s.get(b) == 20);
  CHECK(s.size() == 1);
}

TEST_CASE("EntityStore: a reused slot does not resurrect a stale handle") {
  EntityStore<int> s;
  const auto old_id = s.insert([](EntityId) { return 1; });
  s.erase(old_id);
  const auto new_id = s.insert([](EntityId) { return 2; });
  CHECK(new_id.index == old_id.index);  // slot reused...
  CHECK(new_id != old_id);              // ...but it is a different entity
  CHECK(s.get(old_id) == nullptr);
  CHECK(*s.get(new_id) == 2);
  CHECK_FALSE(s.erase(old_id));
  CHECK_FALSE(s.get(kNoEntity));
}

TEST_CASE("EntityStore ids() is a safe snapshot") {
  EntityStore<int> s;
  for (int i = 0; i < 4; ++i) s.insert([i](EntityId) { return i; });
  int visited = 0;
  for (const auto id : s.ids()) {
    ++visited;
    s.erase(id);  // erasing while walking the snapshot is fine
  }
  CHECK(visited == 4);
  CHECK(s.empty());
}

TEST_CASE("EventBus delivers in order, only on dispatch") {
  EventBus bus;
  std::vector<std::string> seen;
  bus.subscribe([&](const Event& e) {
    on_event<ev::Notice>(e, [&](const ev::Notice& n) { seen.push_back(n.text); });
    on_event<ev::TreeFelled>(e, [&](const ev::TreeFelled&) { seen.push_back("tree"); });
  });
  bus.publish(ev::Notice{0, "a"});
  bus.publish(ev::TreeFelled{{1, 1}});
  CHECK(seen.empty());
  CHECK(bus.pending() == 2);
  bus.dispatch();
  CHECK(seen == std::vector<std::string>{"a", "tree"});
  CHECK(bus.pending() == 0);
}

TEST_CASE("EventBus handlers may publish; a runaway loop is cut off") {
  EventBus bus;
  int calls = 0;
  bus.subscribe([&](const Event&) {
    ++calls;
    bus.publish(ev::Notice{0, "again"});  // would loop forever
  });
  bus.publish(ev::Notice{0, "start"});
  bus.dispatch();
  CHECK(calls == 16);
}

TEST_CASE("ResourceCache shares a live resource and reloads a freed one") {
  ResourceCache<int> cache;
  int loads = 0;
  const auto load = [&]() -> ResourceCache<int>::Result {
    ++loads;
    return std::make_shared<int>(42);
  };

  auto a = cache.get("x", load);
  auto b = cache.get("x", load);
  REQUIRE(a);
  CHECK(*a == b.value());  // same pointer
  CHECK(loads == 1);
  CHECK(cache.live() == 1);

  a = {};
  b = {};
  CHECK(cache.live() == 0);  // freed once nobody holds it
  auto c = cache.get("x", load);
  CHECK(loads == 2);
  CHECK(cache.live() == 1);
}

TEST_CASE("ResourceCache does not cache failures") {
  ResourceCache<int> cache;
  int loads = 0;
  const auto fail = [&]() -> ResourceCache<int>::Result {
    ++loads;
    return std::unexpected("boom");
  };
  CHECK_FALSE(cache.get("x", fail));
  CHECK_FALSE(cache.get("x", fail));
  CHECK(loads == 2);
}
