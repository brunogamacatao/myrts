#pragma once

#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>

#include "core/strings.hpp"

/// The language the interface is drawn in. Set once at startup (main) and read by the drawing helpers, so that every
/// label goes through the translation table without each call site having to ask (see myrts::Strings).
void set_strings(std::shared_ptr<const myrts::Strings> strings);

/// `text` in the current language, with `{0}`, `{1}`... filled in from `args`.
[[nodiscard]] std::string tr(std::string_view text, std::initializer_list<std::string_view> args = {});
