# myrts

Small RTS prototype in C++23 (Allegro 5, Warcraft II maps, A* pathfinding).

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
  (Allegro 5, nlohmann-json, Catch2) and built automatically on first configure
  (slow once, then cached). `VCPKG_ROOT` defaults to `~/vcpkg` in the scripts.
- System packages for building Allegro: see `scripts/setup.sh` (Debian/Ubuntu) –
  `autoconf automake libtool` and X11/OpenGL/ALSA dev packages.

## Manual build
```sh
export VCPKG_ROOT=~/vcpkg
cmake --preset release && cmake --build --preset release
cd build/release && ./myrts      # assets are copied next to the executable
```
Without vcpkg, use `--preset debug-system` (needs `liballegro5-dev`; json/Catch2 are then fetched by CMake).

## Windows build (cross-compiled from Linux)
```sh
sudo apt install g++-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix zip   # once
scripts/package-windows.sh    # -> dist/myrts-windows-x64.zip
```
Builds a static `myrts.exe` for Windows 10/11 64-bit with MinGW-w64 and vcpkg (triplet in `cmake/windows/`), then zips
it with the assets. The first run builds the dependencies (slow); later runs are cached. The zip needs no installation:
extract it all and run `myrts.exe` (it loads the assets from its own folder). It can be smoke-tested on Linux with
`wine dist/myrts-windows-x64/myrts.exe`.

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
- `src/game/` – Allegro front-end: `Game` (input, selection, camera), `TileRenderer`, `SpriteDef`, HUD (`ui`), scenes
- `assets/` – sprites, tilesets, Warcraft II maps (`maps/*.pud`) and game data (`data/*.json`)
- `tests/` – Catch2 tests

## Campaign
The levels are listed in order, grouped in chapters (land warfare, air war, total war) that introduce the game step by step: early levels
only allow a few units and buildings and show tutorial hints, and in the air chapter the bases are not connected on foot, so the
player must build a Dragon Roost. Naval levels are not there yet (there are no ships). Everything is in `assets/data/campaign.json`: each level has a name, a briefing line, a Warcraft II map and how hard
the Alliance plays (delay before its first attack, size and growth of its waves, workers, starting stock). Winning a level offers
the next one; losing offers to retry it. Add a level by adding an entry (the map must have two start locations connected by
land (forests and gold mines count as walls); `tests/campaign_test.cpp` checks this for every level). `MYRTS_MAP=<pud>` plays a single map.

## Maps
Maps are Warcraft II `.pud` files, drawn with the same tile scheme the game uses (every tile is built from four terrain
quadrants, so forests, coasts and cliffs get proper borders, and a felled tree redraws its surroundings).
The campaign decides which map is played (see above); play one map on its own with `MYRTS_MAP`, for example
`MYRTS_MAP="maps/Gold Rush BNE.pud" scripts/run.sh`. The sample maps (forest, wasteland and swamp eras) are in `assets/maps/`,
and any `.pud` with two or more start locations works. See [NOTICE.md](NOTICE.md) for the sources.

## Interface
The HUD is built from the original Warcraft II art: cracked-earth panels, the framed minimap, command / unit / building icons,
resource icons and the mouse cursors (crosshairs while choosing a target, scroll arrows at the screen edge). The icon of every
unit, building and command is set in `assets/data/*.json` (an index into `assets/ui/icons.png`). To rebuild the assets from their
sources run `scripts/extract_ui_assets.py` (see its docstring).

## Sound
Ranged units (archers, axe throwers, ballistas, catapults, mages, death knights, dragons) shoot visible missiles, drawn with
primitives; a unit opts in with `"projectile"` in `assets/data/units.json`.

Units speak the way they do in Warcraft II: a line when selected (`what`), when given an order (`yessir`), a sequence of
increasingly annoyed ones when clicked repeatedly (`pissed`), when they finish training (`ready`), when hit (`help`, at most
once every six seconds) and when they die; buildings have their own sound when selected, and construction, errors, chopping,
strikes and explosions have effects, panned and attenuated by where they happen on screen. Which file plays for which event
is in `assets/data/sounds.json` (`scripts/import_sounds.py "<WarCraft 2 Sounds folder>"` copies the WAVs into `assets/sounds/`
and regenerates it). Volume (music and effects separately) and a global mute are in Options (main and game menus); `MYRTS_MUTE=1` turns the sound off; with no sound card the game stays silent.

## Music
The credits, the main menu and every level have their own track (`assets/music/*.ogg`, streamed); level tracks loop and are set per
level with `"music"` in `assets/data/campaign.json` (the six tracks repeat in turn over the levels), while the victory and defeat
tunes play once. Music volume is independent of the effects volume; mute silences both. `scripts/import_music.py "<songs folder>"`
converts the original mp3 files to Ogg Vorbis (needs ffmpeg).

## Group movement
A* stays single-agent. For a group order, `assign_destinations` picks the closest free, connected tiles around
the target and pairs them with units; each unit then runs A* to its own tile. While moving, units reserve
their current and next tile in `Occupancy`; a unit whose next tile is taken waits briefly and then re-plans
around the tiles held by others.

## Documentation
- [docs/OVERVIEW.md](docs/OVERVIEW.md) – system overview: module diagram, class diagrams, match flow (Mermaid, rendered on GitHub)
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) – design rules and how to add units, buildings, commands and scenes
- API reference (Javadoc-style, from the source comments): `sudo apt install doxygen graphviz && scripts/docs.sh`,
  then open `build/docs/html/index.html`
