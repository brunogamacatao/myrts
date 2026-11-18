#pragma once

#include <expected>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace myrts {

/// The texts of a game's interface in one language. The English text written in the code is the key: `tr("Cannot build
/// there")` gives the translation when the language file has one and the English text otherwise, so a missing
/// translation never shows an empty label. Texts with variable parts use `{0}`, `{1}`... : `tr("{0} ready", {name})`.
///
/// A language file (`strings/<language>.json` in the game package) is `{"language": "pt", "strings": {"English": "Texto"}}`.
class Strings {
 public:
  Strings() = default;

  static std::expected<Strings, std::string> parse(std::string_view json_text);
  static std::expected<Strings, std::string> load_file(const std::string& path);
  /// The shared empty table (every text stays English).
  static std::shared_ptr<const Strings> english();

  [[nodiscard]] std::string tr(std::string_view text, std::initializer_list<std::string_view> args = {}) const;
  [[nodiscard]] const std::string& language() const { return language_; }
  [[nodiscard]] std::size_t size() const { return table_.size(); }

 private:
  std::string language_ = "en";
  std::unordered_map<std::string, std::string> table_;
};

/// `text` with `{0}`, `{1}`... replaced by `args` (a placeholder without an argument stays as written).
[[nodiscard]] std::string format_text(std::string_view text, std::initializer_list<std::string_view> args);

}  // namespace myrts
