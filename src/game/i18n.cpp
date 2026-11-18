#include "i18n.hpp"

namespace {
std::shared_ptr<const myrts::Strings> g_strings = myrts::Strings::english();
}

void set_strings(std::shared_ptr<const myrts::Strings> strings) { g_strings = strings ? std::move(strings) : myrts::Strings::english(); }

std::string tr(std::string_view text, std::initializer_list<std::string_view> args) { return g_strings->tr(text, args); }
