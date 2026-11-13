#pragma once

#include <functional>
#include <string>

#include "scene.hpp"

enum class GameResult { Victory, Defeat };

/// Factories for every scene of the game. Transitions carry these instead of scene pointers,
/// so a scene is only built (and its assets only loaded) at the moment it is entered.
SceneFactory make_intro(std::function<Transition()> when_done);
SceneFactory make_main_menu();
/// The match for campaign level `level` (0 is the first), played as `faction` ("" for the level's own side).
SceneFactory make_game(std::size_t level = 0, std::string faction = {});
SceneFactory make_pause_menu(std::size_t level = 0, std::string faction = {});
/// "Choose your side": one button per faction of the game data; picking one starts the campaign from its first level.
SceneFactory make_faction_select();
/// Sound volume and mute (pushed on top of the main or the pause menu).
SceneFactory make_options();
/// After a match: victory offers the next level of the campaign (if any), defeat offers to retry `level`.
SceneFactory make_game_over(GameResult result, std::size_t level = 0, std::string faction = {});
