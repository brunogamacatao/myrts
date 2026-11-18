# MyRTS Engine: the editor

`myrts_engine` is the graphical editor for games made with the engine. It opens a game (a *package*: a folder with a
`game.json`, see [GAME_FORMAT.md](GAME_FORMAT.md)) and edits all of it: levels, characters, buildings, sprites, maps,
rules, texts. It writes the very files the game reads, so there is nothing to export before trying a change.

## Building and running

```
scripts/build.sh release                 # builds myrts (the game), myrts_engine (the editor) and myrts_tool
build/release/myrts_engine [game-folder]
scripts/run-editor.sh                    # builds and opens the demo game
```

The editor needs Dear ImGui (the vcpkg feature `editor`, on by default). Without it everything else still builds. The
layout of the panels is remembered in `myrts_engine.ini` (next to where you run it).

## The window

| Panel | What it does |
|---|---|
| **Levels** | The campaign's levels: **+** adds, Duplicate, Remove, the arrows or dragging reorder. Double-click makes a level *active* (yellow `>`): the Map tab and **Play** work on it. Right-click: rename and more. |
| **Project** | The game's data files; pick one to edit it as JSON in the *Data files* tab. |
| **Problems** | What is wrong with the game as it is on disk (after Save). The checks are the ones `myrts_tool validate` makes: every reference resolves, every file exists, every level builds a playable match. |
| **Editor** | The tabs below. |

`Ctrl+S` saves, `Ctrl+Z` / `Ctrl+Shift+Z` undo and redo (the map has its own history while its tab is in front), `F5`
plays the active level. A `*` marks what is not saved.

## Tabs

- **Level**: every setting of the level (name, kind, map, music, factions, briefing, hints, allowed units, stocks,
  AI, fog of war, objectives, extra opponents). A field that is absent from the file is shown dimmed with the value the
  game uses then; right-click its name to go back to that default.
- **Map**: the map of the active level, drawn from the game's tile set. *Paint terrain* with the brush: the coasts,
  forest edges and so on follow by themselves, and where water would meet grass a shore is filled in. *Select / move*,
  *Add start*, *Add mine* place the players' starts and the mines. The line under the toolbar says what would stop a
  level from being played on the map (a start in the sea, bases not connected on foot in a `land` level...). A `.pud`
  is converted to the native `.mrmap` on *Save map*; *Use in the active level* points the level at it.
  Middle button drags the view, the wheel scrolls, Ctrl+wheel zooms.
- **Characters** and **Buildings**: a list with search and faction filter, and the inspector for the selected one.
  *Rename id* updates every use of the id in the game's data. *Delete* lists what still refers to the item.
- **Sprites**: the picture of a sprite with its frame rectangles. Drag on the picture to add a frame to the selected
  animation, or fill the animation from a grid. The preview plays the animation. *New sprite* imports a picture.
- **Game**: the manifest (`game.json`), the rules, resources, factions, commands, the right-click rules, projectiles,
  AI profiles, sounds and texts (translations, with a list of the texts the game's data still lacks).
- **Data files**: any data file as JSON text, for what the other editors do not cover (a faction's `translations`).

## Making a game

1. *File > New from template*: copies `templates/minimal-game` (one faction, a worker, a soldier, three buildings,
   one level; the art is a placeholder borrowed from the demo game) and gives it your title.
2. Replace the art: import your sprites, point the buildings at your pictures, change the tile set images.
3. Edit the units, buildings, resources and rules to taste; make levels and maps; **Play** to try them.
4. *Project > Export the game...*: makes `<folder>/<id>/` with the game program (renamed after the game) and the
   package in `game/` next to it. The program plays the package beside it. Zip the folder to give it away.

## Command line

```
myrts_tool validate <game-dir>                         check a whole game
myrts_tool import-pud <game-dir> <in.pud> <out.mrmap>  convert a Warcraft II map
myrts_tool new-map <game-dir> <tileset> <w> <h> <out>  a two-player map to start from
myrts_tool schema docs/schema                          write the JSON Schemas
myrts --game <dir> [--level N --faction ID]            play a game (N from 1)
```

`docs/schema/*.schema.json` describe the data files for other tools (an IDE can validate and complete the JSON). They
are generated from the same field lists the editor uses, so they cannot drift from it.

## Limits

- Tile codes follow the Warcraft II scheme (the editor's terrain brush is built on it): a game can have any number of
  tile sets, but each is laid out like the original's.
- Maps hold terrain, starts and mines. Units and buildings placed at the start come from the level, not the map.
- Not in the editor (nor the engine yet): upgrades and research, spells, teams, a configurable command grid.
