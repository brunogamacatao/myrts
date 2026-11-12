# Architecture

```
src/core/   Pure game logic. No Allegro, no tmx. Everything here is unit-tested headless.
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
| `Ai` | `core/ai.*` | A computer player. Uses only the World's public orders (same as a human): mines, trains, attacks in waves. |
| `SceneStack` | `core/scene_stack.hpp` | Scene lifecycle (enter / exit / pause / resume, overlays). |
| `ResourceCache` | `core/resource_cache.hpp` | Weak-reference cache: a resource lives while some scene uses it. |
| `SceneManager`, scenes | `game/scene*.{hpp,cpp}`, `menu_scene.*`, `scenes.*` | Intro, menus, game, pause, game over. Transitions carry factories. |
| `Game` | `game/game*.cpp` | One match as the player sees it: input, selection, camera, HUD, drawing. |
| `Assets` | `game/assets.*` | Shared sprites and bitmaps (RAII wrappers in `raii.hpp`). |

## Rules of thumb

* A rule of the game belongs in `core/` with a test. `Game` only translates.
* Orders are the `Order` variant in `world.hpp`. A unit is `Idle`, `Harvest`, `Build`, `Attack` or `AttackMove`; moving is just following a path.
* Units never refer to each other by pointer or index, only by `EntityId`.
* Terrain is per **movement class** (`Land`, `Water`, `Air`): each has its own blocked grid and occupancy.

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

## Adding content

**A faction**: units and buildings tagged with its `faction`, plus a player for it in `Game::create` (and an `Ai` if it is
computer-controlled). The Alliance (human) is the example: Peasant, Footman, Town Hall, Farm, Barracks.

**A unit** (any faction): add an entry to `assets/data/units.json` and a sprite JSON under `assets/sprites/`
(`scripts/extract_unit_sprites.py` builds them from the Spriters Resource sheets). Set `move_class`
(`land`/`water`/`air`), `commands`, and optionally `worker` / `builds`. Missing carrying or work animations fall back to
walk/idle. No C++ needed unless it needs a new behaviour.

**A building**: entry in `assets/data/buildings.json` (size, cost, build time, `is_depot`, sprites, `trains`) and the two PNGs.
It shows up in the build menu of every unit whose `builds` lists it.

**A command**: entry in `assets/data/commands.json` (label, icon, hotkey, `targeting`), a line in `Game::register_commands`
for what it does, and a glyph for its `icon` in `ui.cpp` (unknown icons draw a plain box).

**A new activity** (attack, repair, patrol): add an alternative to `Order`, a handler in `World::update_order`, and an
`order_*` method on the World that the command calls.

**A scene**: derive from `Scene`, add a factory in `scenes.cpp`, and return `Transition::push/replace/reset/pop/quit`
from `handle_event` / `update`. Resources are owned by the scene (RAII) and freed when it is destroyed.

## Testing

`scripts/test.sh` builds only `myrts_core` and the tests (no Allegro needed). Gameplay is covered through `World`
(`tests/world_test.cpp`, `tests/combat_test.cpp`): harvesting, mining, construction and builder locking, air vs. land
movement, unit death, combat (armor, chasing, ranged, attack-move, buildings), training, the AI and victory.

Handy environment variables when running the game: `MYRTS_BUILD_SPEED` and `MYRTS_TRAIN_SPEED` speed up construction
and training (e.g. `MYRTS_TRAIN_SPEED=20`).
The Allegro layer is checked by running the game.
