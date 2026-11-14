cmake_minimum_required(VERSION 3.25)
project(myrts VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MYRTS_BUILD_GAME "Build the game executable (needs Allegro 5)" ON)
option(MYRTS_BUILD_TESTS "Build unit tests" ON)
option(MYRTS_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(cmake/Dependencies.cmake)

add_library(myrts_warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(myrts_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow)
  if(MYRTS_WARNINGS_AS_ERRORS)
    target_compile_options(myrts_warnings INTERFACE -Werror)
  endif()
endif()

FetchContent_MakeAvailable(nlohmann_json)

# ---- core: pure logic, no graphics dependencies --------------------------
add_library(myrts_core STATIC
  src/core/pathfinding.cpp
  src/core/formation.cpp
  src/core/harvest.cpp
  src/core/building.cpp
  src/core/defs.cpp
  src/core/world.cpp
  src/core/orders.cpp
  src/core/combat.cpp
  src/core/ai.cpp
  src/core/rules.cpp
  src/core/terrain.cpp
  src/core/tileset.cpp
  src/core/pud.cpp
  src/core/tilemap.cpp
  src/core/scenario.cpp
  src/core/sounds.cpp
  src/core/campaign.cpp
  src/core/fog.cpp
  src/core/savegame.cpp
  src/core/saves.cpp
  src/core/match_setup.cpp)
target_include_directories(myrts_core PUBLIC src)
target_compile_features(myrts_core PUBLIC cxx_std_23)
target_link_libraries(myrts_core PRIVATE myrts_warnings nlohmann_json::nlohmann_json)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(myrts_core PRIVATE -Wconversion)
endif()

# ---- game ----------------------------------------------------------------
if(MYRTS_BUILD_GAME)

  # vcpkg ships a CMake config; system installs only provide pkg-config files.
  find_package(Allegro CONFIG QUIET)
  if(TARGET Allegro::allegro)
    set(MYRTS_ALLEGRO_LIBS Allegro::allegro Allegro::allegro_font
        Allegro::allegro_image Allegro::allegro_primitives
        Allegro::allegro_audio Allegro::allegro_acodec)
    # The static OpenAL that vcpkg builds into Allegro's audio needs fmt.
    find_package(fmt CONFIG QUIET)
    if(TARGET fmt::fmt)
      # It is listed after allegro_audio's own libraries (OpenAL), which is where the linker needs it.
      set_property(TARGET Allegro::allegro_audio APPEND PROPERTY INTERFACE_LINK_LIBRARIES fmt::fmt)
    endif()
    if(WIN32)
      # The static OpenAL's WASAPI backend; must come after OpenAL on the link line, like fmt above.
      set_property(TARGET Allegro::allegro_audio APPEND PROPERTY INTERFACE_LINK_LIBRARIES avrt ole32 uuid winmm)
    endif()
  else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ALLEGRO REQUIRED IMPORTED_TARGET
      allegro-5 allegro_font-5 allegro_image-5 allegro_primitives-5 allegro_audio-5 allegro_acodec-5)
    set(MYRTS_ALLEGRO_LIBS PkgConfig::ALLEGRO)
  endif()

  add_executable(myrts
    src/game/main.cpp
    src/game/sprite.cpp
    src/game/tile_renderer.cpp
    src/game/ui.cpp
    src/game/building_art.cpp
    src/game/ui_art.cpp
    src/game/cursors.cpp
    src/game/draw_util.cpp
    src/game/menu_scene.cpp
    src/game/options_scene.cpp
    src/game/slots_scene.cpp
    src/game/scene_manager.cpp
    src/game/scenes.cpp
    src/game/assets.cpp
    src/game/audio.cpp
    src/game/fog_view.cpp
    src/game/sound_director.cpp
    src/game/game.cpp
    src/game/game_input.cpp
    src/game/game_render.cpp)
  target_link_libraries(myrts PRIVATE
    myrts_core myrts_warnings nlohmann_json::nlohmann_json ${MYRTS_ALLEGRO_LIBS})

  # Make assets available next to the executable (run it from there). A separate target, so that a change to the data
  # alone (a level, a sound table) is copied too, even when the executable itself did not need relinking.
  add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:myrts>)
  add_dependencies(myrts copy_assets)
endif()

# ---- tests ---------------------------------------------------------------
if(MYRTS_BUILD_TESTS)
  enable_testing()
  FetchContent_MakeAvailable(Catch2)
  add_executable(myrts_tests
    tests/pathfinding_test.cpp
    tests/formation_test.cpp
    tests/occupancy_test.cpp
    tests/geometry_test.cpp
    tests/harvest_test.cpp
    tests/building_test.cpp
    tests/scene_stack_test.cpp
    tests/infrastructure_test.cpp
    tests/defs_test.cpp
    tests/world_test.cpp
    tests/combat_test.cpp
    tests/terrain_test.cpp
    tests/sounds_test.cpp
    tests/campaign_test.cpp
    tests/match_setup_test.cpp
    tests/naval_test.cpp
    tests/balance_sim_test.cpp
    tests/fog_test.cpp
    tests/saves_test.cpp)
  target_compile_definitions(myrts_tests PRIVATE MYRTS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  target_link_libraries(myrts_tests PRIVATE myrts_core myrts_warnings nlohmann_json::nlohmann_json Catch2::Catch2WithMain)
  if(catch2_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)  # FetchContent fallback
  endif()
  include(Catch)
  catch_discover_tests(myrts_tests)
endif()
