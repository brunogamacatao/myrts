#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace myrts {

/// What is wrong with a sprite definition (`sprites/<name>.json`: a spritesheet, its animations made of frame rectangles,
/// the default animation and the optional `states`), in the words of a message, or empty if it is sound. The game's
/// loader and the editor both rely on exactly these rules.
[[nodiscard]] std::string sprite_problem(const nlohmann::json& sprite);

}  // namespace myrts
