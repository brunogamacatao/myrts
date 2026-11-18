# Making a game with the engine

A game is a folder (a *package*) with a `game.json` manifest and its content. The engine knows nothing about
Warcraft: everything below is data. Run a package with `myrts --game <dir>` (or `MYRTS_GAME=<dir>`), and check one with

    myrts_tool validate <dir>                              # data, references, files, every level builds a match
    myrts_tool import-pud <dir> <in.pud> <out.mrmap>       # Warcraft II map -> native map

`games/warcraft-like/` is the complete example. Paths written inside a package are relative to its root.
Every file has a `format_version`; every optional file falls back to the defaults the original game shipped with.

| File | What it defines |
|---|---|
| `game.json` | id, title, version, `default_language`, where the other files are, music of the menu/credits/victory/defeat screens |
| `data/resources.json` | the resources (up to 8): id, name, icon, where they come from (`site` = mines/patches, `tree`), whether the worker goes inside the site (`enter_site`), cargo animation, messages |
| `data/units.json` | units: hp, speed, `move_class` (land/water/air), `stats` (armor, damage, range, sight), `cost` (by resource id), `train_seconds`, `food`, `attack_seconds`, `siege_multiplier`, `projectile`, `capacity` (transports), `commands`, `builds`, `requires` (buildings needed to train it), `worker.gather` (per resource: `per_trip`, `seconds`), sprite, icon |
| `data/buildings.json` | buildings: size, hp, armor, `cost`, `build_seconds`, `food` provided, `accepts` (resources delivered here), `placement` (land/coast/water), `trains`, `requires`, sprites |
| `data/commands.json` | the buttons: `order` (a builtin: move, stop, attack, repair, harvest, unload, board, resume_build, patrol, hold, build), `targeting`, `prompt`, hotkey, icons per faction; and `context_orders`: the right-click rules (first match wins; conditions `enemy`, `resource`, `own_transport`, `own_damaged`, `unfinished_building`) |
| `data/factions.json` | sides: worker, town hall, the computer's base buildings, translations to play a campaign as another side |
| `data/projectiles.json` | missiles: `shape` (arrow, bolt, axe, boulder, fire, magic, coil), `speed`, arc, flight time |
| `data/rules.json` | tunables: tree wood, queue length, minimum damage, repair share, vision, starting workers, mine size, corpses, ... |
| `data/campaign.json` | levels: map, music, briefing, hints, `allow`, stocks, `ai_profile` + `ai`, `objectives` (victory: destroy_all, destroy_building, survive, collect, build; defeat: lose_building, time_limit), `more_opponents` |
| `data/ai_profiles.json` | named computer-player settings levels can start from |
| `data/sounds.json` | voices, unit/building sounds, effects |
| `data/tilesets.json` | any number of tile sets: image + where each tile code is |
| `sprites/*.json` | animations per unit; optional `states` choose animations for attack/work/carry |
| `strings/<lang>.json` | interface texts by their English wording (`MYRTS_LANG=pt`) |
| `maps/*.mrmap`, `*.pud` | maps (native JSON, or Warcraft II files converted on load) |

## What is still code
* **Order types** (the builtin list above) and the **vocabulary of right-click conditions**: a new kind of order is a handler in
  `World::issue_order` plus an entry in `kBuiltinOrders`; commands and context rules are then pure data.
* **Projectile shapes** and **sound events** (selected, acknowledge, annoyed, ready, help, dead, attack, work done) are drawn/triggered by the engine; their speed, art and files are data.
* **Terrain**: tile codes follow the Warcraft II scheme (each code names the terrain of its four corners); a game can replace the artwork and number of tile sets, not the code scheme.
* Not implemented yet: upgrades/research, spells and abilities, teams and diplomacy, a configurable button grid (6 slots).
