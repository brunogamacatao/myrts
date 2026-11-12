# myrts

Small RTS prototype in C++23 (Allegro 5, Tiled maps, A* pathfinding).

## Quick start
```sh
scripts/setup.sh   # once: system packages (apt) + vcpkg in ~/vcpkg
scripts/run.sh     # build (release) and run the game
scripts/test.sh    # unit tests only (no Allegro needed)
```
`scripts/build.sh [release|debug|tests-only|debug-system]` builds a given preset.

## Requirements
- C++23 compiler (GCC ≥ 13 or Clang ≥ 17), CMake ≥ 3.25, Ninja
- [vcpkg](https://github.com/microsoft/vcpkg); dependencies are declared in `vcpkg.json`
  (Allegro 5, tmx, nlohmann-json, Catch2) and built automatically on first configure
  (slow once, then cached). `VCPKG_ROOT` defaults to `~/vcpkg` in the scripts.
- System packages for building Allegro: see `scripts/setup.sh` (Debian/Ubuntu) –
  `autoconf automake libtool` and X11/OpenGL/ALSA dev packages.

## Manual build
```sh
export VCPKG_ROOT=~/vcpkg
cmake --preset release && cmake --build --preset release
cd build/release && ./myrts      # assets are copied next to the executable
```
Without vcpkg, use `--preset debug-system` (needs `liballegro5-dev`, `libxml2-dev`,
`zlib1g-dev`; tmx/json/Catch2 are then fetched by CMake).

Tests: `cmake --preset tests-only && cmake --build --preset tests-only && ctest --preset tests-only`

## Layout
- `src/core/` – pure logic (grid, A*), unit tested
- `src/game/` – Allegro/Tiled front-end
- `assets/` – sprites and maps
- `tests/` – Catch2 tests
