# myrts {#mainpage}

RTS prototype in C++23 (Allegro 5, Tiled maps, A* pathfinding).

This is the generated API reference. The narrative documentation (module and class diagrams, match
flow) lives next to the sources:

* `docs/OVERVIEW.md` – system overview with Mermaid diagrams (rendered by GitHub).
* `docs/ARCHITECTURE.md` – rules of thumb and recipes for adding content.

## Two layers

| Namespace / folder | Role |
|---|---|
| `myrts` (`src/core/`) | Pure game logic, no graphics. Unit-tested headless. Start at myrts::World. |
| global (`src/game/`) | Allegro front-end: Game, SceneManager, Scene, the HUD in namespace ui. |

## Where to start reading

1. myrts::World – the simulation and every `order_*` a player (or the myrts::Ai) can issue.
2. myrts::Registry – game data loaded from `assets/data/*.json`.
3. myrts::EventBus – how the simulation tells the view what happened.
4. Game – how input and drawing sit on top of the World.
5. Scene, Transition, SceneManager – screen lifecycle.
