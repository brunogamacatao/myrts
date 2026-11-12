#pragma once

namespace myrts {

/// The eight directions a unit can face; they select the `<animation>_<direction>` sprite.
enum class Facing { Up, UpRight, Right, DownRight, Down, DownLeft, Left, UpLeft };

/// "up", "up_right", ... (the direction names used by the sprite definitions).
[[nodiscard]] const char* facing_name(Facing f);

}  // namespace myrts
