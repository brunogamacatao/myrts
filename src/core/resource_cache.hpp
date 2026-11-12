#pragma once

#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace myrts {

/// Shares loaded resources by key without owning them: the cache only keeps weak references, so
/// a resource is freed as soon as the last scene using it lets go, and loaded again if someone
/// asks for it later.
template <class V>
class ResourceCache {
 public:
  using Result = std::expected<std::shared_ptr<V>, std::string>;

  /// Returns the live instance for `key`, or builds it with `load()` (which returns Result).
  /// Failed loads are not cached.
  template <class Loader>
  Result get(const std::string& key, Loader&& load) {
    if (auto it = entries_.find(key); it != entries_.end()) {
      if (auto alive = it->second.lock()) return alive;
      entries_.erase(it);
    }
    Result loaded = load();
    if (loaded && *loaded) entries_[key] = *loaded;
    return loaded;
  }

  /// Number of resources currently alive.
  [[nodiscard]] std::size_t live() const {
    std::size_t n = 0;
    for (const auto& [_, w] : entries_) n += w.expired() ? 0 : 1;
    return n;
  }

  /// Forgets entries whose resource has already been freed.
  void prune() {
    std::erase_if(entries_, [](const auto& kv) { return kv.second.expired(); });
  }

 private:
  std::map<std::string, std::weak_ptr<V>> entries_;
};

}  // namespace myrts
