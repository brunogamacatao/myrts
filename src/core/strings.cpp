#include "strings.hpp"

#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <vector>

namespace myrts {

std::string format_text(std::string_view text, std::initializer_list<std::string_view> args) {
  if (args.size() == 0) return std::string(text);
  const std::vector<std::string_view> values(args);
  std::string out;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '{' && i + 2 < text.size() + 0 && text[i + 1] >= '0' && text[i + 1] <= '9' && text[i + 2] == '}') {
      const auto n = static_cast<std::size_t>(text[i + 1] - '0');
      if (n < values.size()) {
        out += values[n];
        i += 2;
        continue;
      }
    }
    out += text[i];
  }
  return out;
}

std::expected<Strings, std::string> Strings::parse(std::string_view text) {
  try {
    const auto doc = nlohmann::json::parse(text);
    Strings s;
    s.language_ = doc.value("language", std::string("en"));
    for (const auto& [source, target] : doc.at("strings").items()) {
      if (!target.is_string()) return std::unexpected("strings: the translation of \"" + source + "\" must be text");
      s.table_[source] = target.get<std::string>();
    }
    return s;
  } catch (const std::exception& e) {
    return std::unexpected(std::string("strings: ") + e.what());
  }
}

std::expected<Strings, std::string> Strings::load_file(const std::string& path) {
  std::ifstream f(path);
  if (!f) return std::unexpected("cannot open " + path);
  auto s = parse(std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()));
  if (!s) return std::unexpected(path + ": " + s.error());
  return s;
}

std::shared_ptr<const Strings> Strings::english() {
  static const std::shared_ptr<const Strings> none = std::make_shared<const Strings>();
  return none;
}

std::string Strings::tr(std::string_view text, std::initializer_list<std::string_view> args) const {
  const auto it = table_.find(std::string(text));
  return format_text(it == table_.end() ? text : std::string_view(it->second), args);
}

}  // namespace myrts
