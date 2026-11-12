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

## Controls
| Input | Action |
|---|---|
| Left click on a unit | Select it (Shift toggles in/out of the selection) |
| Left drag in the viewport | Box-select units (green rectangle) |
| Left click on ground / right click | Move the selected units (they get distinct, non-overlapping destinations) |
| Move button (`M`) then click | Move to the clicked tile; `S` / Stop button halts the units |
| Click or drag on the minimap | Center the camera there |
| Arrow keys / mouse at the screen edge | Scroll the viewport |
| `Esc` | Cancel the pending command, then clear the selection, then quit |

## Layout
- `src/core/` – pure logic, unit tested: `Grid`, A* (`pathfinding`), group destinations (`formation`), tile
  reservations (`occupancy`), camera/minimap math (`geometry`), picking (`selection`)
- `src/game/` – Allegro/Tiled front-end: `Game` (state, input, rendering), `Unit`/`UnitType`, `SpriteDef`, HUD (`ui`)
- `assets/` – sprites, maps and unit definitions (`units/*.json`: stats, speed, commands)
- `tests/` – Catch2 tests

## Group movement
A* stays single-agent. For a group order, `assign_destinations` picks the closest free, connected tiles around
the target and pairs them with units; each unit then runs A* to its own tile. While moving, units reserve
their current and next tile in `Occupancy`; a unit whose next tile is taken waits briefly and then re-plans
around the tiles held by others.
