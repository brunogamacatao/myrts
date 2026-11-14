#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace myrts {

/// Standard base64 (RFC 4648, with padding).
[[nodiscard]] std::string base64_encode(std::span<const std::uint8_t> bytes);
/// The bytes of `text`, or nothing if it is not valid base64.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> base64_decode(std::string_view text);

/// One saved game: where it was, when, a small picture of the map for the slot, and the match state (World::save_state as JSON
/// text, so this file format does not depend on the simulation).
struct SaveSlot {
  std::string name;       ///< what the slot shows ("Level 3: Skirmish")
  int level = 0;          ///< campaign level index
  std::string faction;    ///< the side being played
  std::string date;       ///< when it was saved ("2026-11-14 15:04")
  std::string thumbnail;  ///< base64 of kThumbSize x kThumbSize RGB bytes (the minimap), or empty
  std::string state;      ///< JSON text
};

/// The saved games of the player: a fixed number of slots, stored as a JSON file
/// `{"version":1,"slots":[{...} or null, ...]}`.
class SaveFile {
 public:
  static constexpr int kSlots = 4;
  static constexpr int kThumbSize = 64;

  [[nodiscard]] const std::optional<SaveSlot>& slot(int i) const { return slots_.at(static_cast<std::size_t>(i)); }
  void set(int i, SaveSlot slot) { slots_.at(static_cast<std::size_t>(i)) = std::move(slot); }
  void clear(int i) { slots_.at(static_cast<std::size_t>(i)).reset(); }

  [[nodiscard]] std::string to_json() const;
  static std::expected<SaveFile, std::string> parse(std::string_view json_text);
  /// Reads `path`; a file that does not exist yet is an empty set of slots, not an error.
  static std::expected<SaveFile, std::string> load(const std::string& path);
  std::expected<void, std::string> save(const std::string& path) const;

 private:
  std::array<std::optional<SaveSlot>, kSlots> slots_;
};

}  // namespace myrts
