#pragma once

namespace myrts {

enum class Facing { Up, UpRight, Right, DownRight, Down, DownLeft, Left, UpLeft };

/// "up", "up_right", ... (the direction names used by the sprite definitions).
[[nodiscard]] const char* facing_name(Facing f);

}  // namespace myrts
