# Architecture

```
src/core/   Pure game logic. No Allegro. Everything here is unit-tested headless.
src/game/   Allegro: rendering, input, UI, scenes. Turns the World's state into pixels and clicks into orders.
assets/     Sprites, tiles and the data files below (copied next to the executable at build time).
tests/      Catch2 tests for src/core (scripts/test.sh).
```

## Layers

| Piece | Where | Role |
|---|---|---|
| `World` | `core/world.*`, `core/orders.cpp` | The simulation: terrain layers, units, buildings, mines, trees, players, orders. |
| `Registry` | `core/defs.*` | Unit / building / command definitions loaded from `assets/data/*.json`, cross-checked at load. |
| `EntityStore`, `EntityId` | `core/entity_store.hpp` | Slot map with generation-checked handles: units can die without invalidating anyone. |
| `EventBus` | `core/events.hpp` | The World publishes (`TreeFelled`, `UnitDied`, `BuildingCompleted`, `Notice`...); the HUD and rules listen. |
| `rules` | `core/rules.*` | Victory / defeat from the World state. |
| `TileMap`, `terrain`, `TileSet`, `PudMap` | `core/tilemap.*`, `terrain.*`, `tileset.*`, `pud.*` | The terrain: Warcraft II map tiles, their borders and movement rules (see "Maps and terrain"). |
| `Scenario` | `core/scenario.*` | Who starts where and which gold mines a map holds, read from its units. |
| `Ai` | `core/ai.*` | A computer player. Uses only the World's public orders (same as a human): mines, trains, attacks in waves. |
| `SceneStack` | `core/scene_stack.hpp` | Scene lifecycle (enter / exit / pause / resume, overlays). |
| `ResourceCache` | `core/resource_cache.hpp` | Weak-reference cache: a resource lives while some scene uses it. |
| `SceneManager`, scenes | `game/scene*.{hpp,cpp}`, `menu_scene.*`, `scenes.*` | Intro, menus, game, pause, game over. Transitions carry factories. |
| `Game` | `game/game*.cpp` | One match as the player sees it: input, selection, camera, HUD, drawing. |
| `Assets` | `game/assets.*` | Shared sprites and bitmaps (RAII wrappers in `raii.hpp`). |
| `UiArt`, `Cursors` | `game/ui_art.*`, `game/cursors.*` | The Warcraft II interface art (icon atlas, panel textures, resource icons) and the mouse cursors. |

## Rules of thumb

* A rule of the game belongs in `core/` with a test. `Game` only translates.
* Orders are the `Order` variant in `world.hpp`. A unit is `Idle`, `Harvest`, `Build`, `Attack` or `AttackMove`; moving is just following a path.
* Units never refer to each other by pointer or index, only by `EntityId`.
* Terrain is per **movement class** (`Land`, `Water`, `Air`): each has its own blocked grid and occupancy.

## Maps and terrain

Maps are Warcraft II `.pud` files and are drawn with the original tile scheme (documented by the open-source editors
war2edit and warcraft2-map-editor; see `NOTICE.md`).

* A **map tile** is 32x32 pixels with a 16-bit code. Each tile is built from **four terrain quadrants** (top-left,
  top-right, bottom-left, bottom-right). If all four are the same it is a *solid* tile (`0x0070` = trees); otherwise it is a
  *boundary* tile between exactly two terrains: `0x0B00 | (mask << 4) | variation`, where `B` names the pair (`7` =
  grass/trees, `5` = grass/dirt, `2` = dirt/water...) and `mask` marks which quadrants hold the inner terrain.
* Neighbouring tiles share quadrants: terrain lives on a grid of *vertices* and tile `(x, y)` sits on the four vertices
  `(x, y)`, `(x+1, y)`, `(x, y+1)`, `(x+1, y+1)`. That is what turns a forest into ragged, correct edges instead of square blocks.
* `terrain.hpp` has the rules: `decompose` (code to quadrants), `compose` (quadrants to code, refusing terrains that never
  meet), `movement_of` (what land units can walk on). They reproduce the movement data stored in every sample map.
* `TileSet` knows which codes exist in an era and where each is in the tileset image (`assets/data/tilesets.json`,
  built by `scripts/extract_tilesets.py`); `pick` chooses a variation so repeated tiles do not look identical.
* `TileMap::from_terrain` is the **autotiler** (give it a terrain per vertex, get a map). `TileMap::clear_trees` fells a
  tree and recomposes the tiles around it, so forest edges follow. The `World` calls it when a tree runs out of wood and
  publishes `TerrainChanged` so the view redraws.
* The renderer draws only the visible tiles; the minimap uses the top-left 8x8 "minitile" of every tile.

## Combat and training

* An `Attack` order chases a target (unit or building) until it is in range, then strikes every `attack_seconds`.
  Damage is a roll between `damage_min` and `damage_max` minus the target's `armor` (never below 1); the roll uses the
  World's seeded RNG, so a seed reproduces a fight.
* `AttackMove` walks to a point and engages whatever enemy comes into `sight` on the way, then carries on.
* Idle units with `aggressive: true` (every non-worker by default) attack what they see: units first, then buildings. Any
  armed unit fights back when hit while idle. Workers keep working otherwise.
* Buildings have `hp` and `armor` and can be destroyed (their ground becomes walkable, their builder is freed).
* Buildings train the units listed in `trains` (`order_train`): the price is paid up front, the unit appears on the
  nearest free tile around the building, cancelling refunds.
* `evaluate_outcome` ends the match when one side has nothing left.
* **Food**: finished buildings provide food (`food` in `buildings.json`: halls 10, farms 4), every unit eats `food`
  (default 1) while alive *or waiting in a queue*. `order_train` refuses when it would exceed the cap.
* **Rally point**: `order_rally` sets where a building's new units go; workers rallied onto a tree or a mine start gathering.
* `UnitDied` carries the unit's definition, position and facing so the view can leave a corpse
  (`death_<dir>` animations are one-shot: a falling frame, then the body; they fade after 25 s).
* The `Ai` builds a farm when food is nearly used up (cutting wood first if it has none), and holds its first attack
  until `start_delay` has passed.

## The roster

| | Horde (orcs) | Alliance |
|---|---|---|
| Workers | Peon | Peasant |
| Barracks | Grunt, Troll Axethrower | Footman, Archer |
| Siege / vehicles | Catapult (Ogre Mound), Goblin Zeppelin (Alchemist, air scout) | Ballista (Blacksmith) |
| Heavy / elite | Ogre (Ogre Mound), Death Knight (Temple of the Damned) | Knight (Stables), Mage (Mage Tower) |
| Air | Dragon (Dragon Roost) | Gryphon Rider (Gryphon Aviary) |

Everything is data (`assets/data/units.json`, `buildings.json`). Behaviour differences come from a few fields: `move_class`
(air units cross walls and are drawn lifted with a shadow), `stats.range` (ranged attackers), `siege_multiplier`
(catapults and ballistas hit buildings much harder), `food`, and `aggressive`. Not implemented yet: naval units (needs a
shipyard and a coast), Goblin Sappers, summoned Skeletons and spells.

`MYRTS_ROSTER=1` spawns one of every unit type, which is a quick way to eyeball new sprites.

The build menu pages itself when a worker can build more than five things.

## Adding content

**A faction**: units and buildings tagged with its `faction`, plus a player for it in `Game::create` (and an `Ai` if it is
computer-controlled). The Alliance (human) is the example: Peasant, Footman, Town Hall, Farm, Barracks.

**A unit** (any faction): add an entry to `assets/data/units.json` and a sprite JSON under `assets/sprites/`
(`scripts/extract_unit_sprites.py` builds them from the Spriters Resource sheets: add the rows of each animation to
`UNITS`). A test checks that every sprite exists and has idle/walk in all eight directions. Put the unit in some building's
`trains` list so it can be obtained. Set `move_class`
(`land`/`water`/`air`), `commands`, and optionally `worker` / `builds`. Missing carrying or work animations fall back to
walk/idle. No C++ needed unless it needs a new behaviour.

**A building**: entry in `assets/data/buildings.json` (size, cost, build time, `is_depot`, sprites, `trains`) and the two PNGs.
It shows up in the build menu of every unit whose `builds` lists it.

**A command**: entry in `assets/data/commands.json` (label, hotkey, `targeting`, and `icons`: atlas indices by faction with a
`default`) and a line in `Game::register_commands` for what it does.

**A new activity** (attack, repair, patrol): add an alternative to `Order`, a handler in `World::update_order`, and an
`order_*` method on the World that the command calls.

**A scene**: derive from `Scene`, add a factory in `scenes.cpp`, and return `Transition::push/replace/reset/pop/quit`
from `handle_event` / `update`. Resources are owned by the scene (RAII) and freed when it is destroyed.

## Testing

`scripts/test.sh` builds only `myrts_core` and the tests (no Allegro needed). Gameplay is covered through `World`
(`tests/world_test.cpp`, `tests/combat_test.cpp`): harvesting, mining, construction and builder locking, air vs. land
movement, unit death, combat (armor, chasing, ranged, attack-move, buildings), training, the AI and victory.

Handy environment variables when running the game: `MYRTS_BUILD_SPEED` and `MYRTS_TRAIN_SPEED` speed up construction
and training (e.g. `MYRTS_TRAIN_SPEED=20`); `MYRTS_AI_DELAY` sets how many seconds the computer player waits before its
first attack (default 150).
The Allegro layer is checked by running the game.
