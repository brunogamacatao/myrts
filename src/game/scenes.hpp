#pragma once

#include <functional>

#include "scene.hpp"

enum class GameResult { Victory, Defeat };

/// Factories for every scene of the game. Transitions carry these instead of scene pointers,
/// so a scene is only built (and its assets only loaded) at the moment it is entered.
SceneFactory make_intro(std::function<Transition()> when_done);
SceneFactory make_main_menu();
SceneFactory make_game();
SceneFactory make_pause_menu();
/// Sound volume and mute (pushed on top of the main or the pause menu).
SceneFactory make_options();
SceneFactory make_game_over(GameResult result);
