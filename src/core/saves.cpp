#include "saves.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>

namespace myrts {

namespace {
constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}

std::string base64_encode(std::span<const std::uint8_t> bytes) {
  std::string out;
  out.reserve((bytes.size() + 2) / 3 * 4);
  for (std::size_t i = 0; i < bytes.size(); i += 3) {
    const std::uint32_t b0 = bytes[i];
    const std::uint32_t b1 = i + 1 < bytes.size() ? bytes[i + 1] : 0;
    const std::uint32_t b2 = i + 2 < bytes.size() ? bytes[i + 2] : 0;
    const std::uint32_t v = (b0 << 16) | (b1 << 8) | b2;
    out += kAlphabet[(v >> 18) & 63];
    out += kAlphabet[(v >> 12) & 63];
    out += i + 1 < bytes.size() ? kAlphabet[(v >> 6) & 63] : '=';
    out += i + 2 < bytes.size() ? kAlphabet[v & 63] : '=';
  }
  return out;
}

std::optional<std::vector<std::uint8_t>> base64_decode(std::string_view text) {
  if (text.size() % 4 != 0) return std::nullopt;
  const auto value = [](char c) -> int {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
  };
  std::vector<std::uint8_t> out;
  out.reserve(text.size() / 4 * 3);
  for (std::size_t i = 0; i < text.size(); i += 4) {
    int v[4];
    int pad = 0;
    for (int k = 0; k < 4; ++k) {
      const char c = text[i + static_cast<std::size_t>(k)];
      if (c == '=') {
        if (i + 4 != text.size() || k < 2) return std::nullopt;  // padding only at the very end
        v[k] = 0;
        ++pad;
      } else {
        if (pad > 0) return std::nullopt;
        v[k] = value(c);
        if (v[k] < 0) return std::nullopt;
      }
    }
    const std::uint32_t n = (static_cast<std::uint32_t>(v[0]) << 18) | (static_cast<std::uint32_t>(v[1]) << 12) |
                            (static_cast<std::uint32_t>(v[2]) << 6) | static_cast<std::uint32_t>(v[3]);
    out.push_back(static_cast<std::uint8_t>(n >> 16));
    if (pad < 2) out.push_back(static_cast<std::uint8_t>(n >> 8));
    if (pad < 1) out.push_back(static_cast<std::uint8_t>(n));
  }
  return out;
}

std::string SaveFile::to_json() const {
  using json = nlohmann::json;
  json slots = json::array();
  for (const auto& s : slots_) {
    if (!s) {
      slots.push_back(nullptr);
      continue;
    }
    slots.push_back({{"name", s->name}, {"level", s->level}, {"faction", s->faction}, {"date", s->date},
                     {"thumbnail", s->thumbnail}, {"state", json::parse(s->state, nullptr, false)}});
  }
  return json{{"version", 1}, {"slots", std::move(slots)}}.dump(1);
}

std::expected<SaveFile, std::string> SaveFile::parse(std::string_view text) {
  using json = nlohmann::json;
  SaveFile file;
  try {
    const json doc = json::parse(text);
    const json& slots = doc.at("slots");
    for (std::size_t i = 0; i < slots.size() && i < static_cast<std::size_t>(kSlots); ++i) {
      const json& s = slots[i];
      if (s.is_null()) continue;
      SaveSlot slot;
      slot.name = s.at("name").get<std::string>();
      slot.level = s.at("level").get<int>();
      slot.faction = s.at("faction").get<std::string>();
      slot.date = s.value("date", std::string{});
      slot.thumbnail = s.value("thumbnail", std::string{});
      slot.state = s.at("state").dump();
      file.slots_[i] = std::move(slot);
    }
  } catch (const std::exception& e) {
    return std::unexpected(std::string("invalid save file: ") + e.what());
  }
  return file;
}

std::expected<SaveFile, std::string> SaveFile::load(const std::string& path) {
  if (!std::filesystem::exists(path)) return SaveFile{};
  std::ifstream f(path);
  if (!f) return std::unexpected("cannot open " + path);
  const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  return parse(text);
}

std::expected<void, std::string> SaveFile::save(const std::string& path) const {
  const std::filesystem::path p(path);
  std::error_code ec;
  if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
  // Written beside the file first, so a crash cannot leave half a save behind.
  const std::string tmp = path + ".tmp";
  {
    std::ofstream f(tmp, std::ios::trunc);
    if (!f) return std::unexpected("cannot write " + tmp);
    f << to_json();
    if (!f) return std::unexpected("cannot write " + tmp);
  }
  std::filesystem::rename(tmp, path, ec);
  if (ec) return std::unexpected("cannot replace " + path + ": " + ec.message());
  return {};
}

}  // namespace myrts
