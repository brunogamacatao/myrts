# myrts – visão geral do sistema

Protótipo de RTS (estilo Warcraft II) em C++23. Duas facções (Horde e Aliança), coleta de recursos,
construção, treinamento, combate, IA adversária e um front-end gráfico em Allegro 5.

> Este documento explica **como as peças se encaixam**. Para detalhes por classe/função veja a
> documentação de API gerada pelo Doxygen (`scripts/docs.sh`, saída em `build/docs/html/index.html`).
> Para receitas ("como adicionar uma unidade") veja [ARCHITECTURE.md](ARCHITECTURE.md).

## 1. A ideia central

O projeto é dividido em **duas bibliotecas/camadas** com uma regra de dependência de mão única:

| Camada | Pasta | Depende de | Testada? |
|---|---|---|---|
| **core** (`myrts_core`) | `src/core/` | só a STL e nlohmann-json | Sim, *headless* (Catch2) |
| **game** (`myrts`) | `src/game/` | `core`, Allegro 5, libtmx | Não (verifica-se rodando) |

* `core` contém **todas as regras do jogo**: movimento, coleta, construção, combate, treinamento, comida,
  IA, vitória/derrota. Não sabe o que é pixel, janela, mouse ou som.
* `game` apenas **traduz**: cliques viram *ordens* (`World::order_*`), e o estado do `World` (mais os
  eventos que ele publica) vira imagem na tela.
* Os **dados** (unidades, prédios, comandos) vivem em JSON (`assets/data/*.json`), não em código.

## 2. Diagrama de módulos

```mermaid
flowchart TB
  subgraph DATA["assets/ (dados)"]
    J["data/units.json<br/>buildings.json<br/>commands.json"]
    SP["sprites/*.json + png"]
    TM["tiles/mapa.tmx + tileset"]
  end

  subgraph GAME["src/game — front-end Allegro (executável myrts)"]
    MAIN["main.cpp<br/>loop de eventos / timer 60 FPS"]
    SM["SceneManager<br/>(pilha de cenas + fade)"]
    SC["scenes.cpp<br/>Intro · GameScene · fábricas"]
    MS["MenuScene<br/>(menu principal, pausa, game over)"]
    G["Game<br/>(game.cpp · game_input.cpp · game_render.cpp)"]
    UI["ui<br/>HUD, minimapa, botões"]
    SPR["sprite<br/>SpriteDef · AnimationState"]
    AS["Assets + BuildingArt<br/>cache de bitmaps/sprites"]
    TMAP["tilemap<br/>libtmx → Grid, árvores"]
  end

  subgraph CORE["src/core — lógica pura (biblioteca myrts_core)"]
    W["World<br/>(world.cpp · orders.cpp · combat.cpp)"]
    REG["Registry<br/>(defs)"]
    AI["Ai"]
    RULES["rules<br/>(vitória/derrota)"]
    EV["EventBus"]
    ES["EntityStore&lt;T&gt;<br/>EntityId"]
    NAV["pathfinding (A*)<br/>formation · occupancy · grid"]
    ECO["harvest · building<br/>(árvores, minas, custo)"]
    GEO["geometry · selection<br/>(Camera, Minimap, Rect)"]
    SS["SceneStack&lt;S&gt;<br/>ResourceCache&lt;V&gt;"]
  end

  J --> REG
  MAIN --> SM --> SC --> G
  SC --> MS
  SM --> SS
  AS --> SS
  G --> W
  G --> AI
  G --> RULES
  G --> UI
  G --> SPR
  G --> AS
  G --> TMAP
  G --> GEO
  TMAP --> TM
  SPR --> SP
  AS --> SP
  W --> REG
  W --> EV
  W --> ES
  W --> NAV
  W --> ECO
  AI --> W
  RULES --> W
  G -. "assina eventos" .-> EV
```

Leitura rápida: setas sempre apontam de quem **usa** para quem **é usado**. Nada em `core` aponta para
`game` — é isso que permite rodar o jogo inteiro em testes.

### Responsabilidade de cada módulo

| Módulo | Arquivos | O que faz |
|---|---|---|
| `World` | `core/world.*`, `orders.cpp`, `combat.cpp` | A simulação. Terreno (3 classes de movimento), unidades, prédios, minas, árvores, jogadores, ordens e *tick*. |
| `Registry` | `core/defs.*` | Carrega e valida `units/buildings/commands.json`. Imutável depois de carregado. |
| `EntityStore<T>` / `EntityId` | `core/entity_store.hpp`, `ids.hpp` | *Slot map* com *generation*: ids de entidades mortas ficam inválidos, mesmo se o slot for reaproveitado. |
| `EventBus` | `core/events.hpp` | Fila de eventos (`UnitDied`, `Attacked`, `Notice`…). O `World` publica, o `Game` escuta. |
| `Ai` | `core/ai.*` | Jogador computador. Usa as mesmas `order_*` públicas de um humano. |
| `rules` | `core/rules.*` | `evaluate_outcome`: vitória/derrota. |
| Navegação | `pathfinding`, `formation`, `occupancy`, `grid` | A\* em grade 8-conexa, destinos distintos para grupos, reserva de tiles. |
| Economia | `harvest`, `building` | `TreeField`, `GoldMine`, `Stock`/`Cost`, `footprint`/`can_place`. |
| Geometria | `geometry`, `selection` | `Camera`, `Minimap`, `Rect`, seleção por clique/caixa. |
| Infra | `scene_stack`, `resource_cache` | Pilha de cenas genérica e cache de recursos com *weak_ptr*. |
| `SceneManager` + cenas | `game/scene*`, `scenes`, `menu_scene` | Ciclo de vida de telas (intro, menu, jogo, pausa, game over) com *fade*. |
| `Game` | `game/game*.cpp` | Uma partida vista pelo jogador: câmera, seleção, comandos, HUD, desenho. |
| `ui` | `game/ui.*` | HUD desenhado a partir de *view-models* (`HudData`, `UnitCard`…). |
| `sprite`, `Assets`, `BuildingArt`, `tilemap` | `game/*` | Animações, cache de imagens, arte de prédios e mapa Tiled. |

## 3. Diagrama de classes

### 3.1 Núcleo (`src/core`)

```mermaid
classDiagram
  direction LR

  class World {
    +WorldConfig config
    +add_player() PlayerId
    +spawn_unit() EntityId
    +add_building() EntityId
    +order_move() Result
    +order_harvest() Result
    +order_build() Result
    +order_attack() Result
    +order_attack_move() Result
    +order_train() Result
    +order_rally() Result
    +update(dt)
    +food_cap() int
    +food_used() int
    +events() EventBus&
  }
  class Layer {
    Grid blocked
    Occupancy occupancy
  }
  class Unit {
    EntityId id
    UnitDef* def
    PlayerId owner
    int hp
    float x, y
    Point tile, next
    deque~Point~ path
    Order order
    Cargo cargo
  }
  class Building {
    EntityId id
    BuildingDef* def
    PlayerId owner
    Point origin
    double progress
    bool complete
    int hp
    vector~TrainItem~ queue
    optional~Point~ rally
  }
  class Mine {
    GoldMine site
    bool busy
  }
  class Player {
    string name
    string faction
    Stock stock
  }
  class TrainItem {
    UnitDef* unit
    double progress
  }
  class Order {
    <<variant>>
    Idle
    Harvest
    Build
    Attack
    AttackMove
  }

  class EntityStore~T~ {
    +insert(make) EntityId
    +erase(id) bool
    +get(id) T*
    +ids() vector~EntityId~
    +for_each(f)
  }
  class EntityId {
    uint32 index
    uint32 generation
    +valid() bool
  }
  class EventBus {
    +subscribe(handler)
    +publish(event)
    +dispatch()
  }
  class Event {
    <<variant>>
    TreeFelled
    UnitDied
    Attacked
    BuildingCompleted
    Notice
    ...
  }

  class Registry {
    +parse() expected
    +load_directory() expected
    +unit(id) UnitDef*
    +building(id) BuildingDef*
    +command(id) CommandDef*
  }
  class UnitDef {
    string id, faction
    int max_hp
    MoveClass move_class
    UnitStats stats
    Cost cost
    optional~WorkerDef~ worker
    vector~string~ builds
  }
  class BuildingDef {
    string id, faction
    int size, hp, armor, food
    bool is_depot
    vector~string~ trains
  }
  class CommandDef {
    string id, label, icon
    Targeting targeting
    char hotkey
  }

  class Grid {
    +is_blocked(Point) bool
    +set_blocked(Point)
  }
  class Occupancy {
    +try_reserve(Point,Id) bool
    +release(Point,Id)
    +blocked_except(ids) Grid
  }
  class TreeField {
    +add(Point,lumber)
    +take(Point,amount) int
  }
  class GoldMine {
    +take(amount) int
    +depleted() bool
  }

  class Ai {
    +update(dt)
    +waves_sent() int
  }
  class AiConfig

  World "1" *-- "3" Layer : Land / Water / Air
  World *-- "units" EntityStore~T~
  World *-- "buildings" EntityStore~T~
  World *-- "0..*" Mine
  World *-- "0..*" Player
  World *-- TreeField
  World *-- EventBus
  Layer *-- Grid
  Layer *-- Occupancy
  Mine *-- GoldMine
  Building *-- "0..*" TrainItem
  Unit --> Order
  Unit --> UnitDef : def
  Building --> BuildingDef : def
  TrainItem --> UnitDef
  Unit ..> EntityId
  Building ..> EntityId
  EntityStore~T~ ..> EntityId : emite
  EventBus o-- Event
  Registry *-- UnitDef
  Registry *-- BuildingDef
  Registry *-- CommandDef
  UnitDef ..> BuildingDef : builds (por id)
  BuildingDef ..> UnitDef : trains (por id)
  Ai --> World : só usa order_*
  Ai --> Registry
  Ai *-- AiConfig
```

### 3.2 Front-end (`src/game`)

```mermaid
classDiagram
  direction TB

  class SceneManager {
    +start(factory) bool
    +handle_event(e)
    +update(dt)
    +draw()
    +running() bool
  }
  class SceneStack~Scene~ {
    +push() +pop() +replace() +clear()
    +for_each_visible(f)
  }
  class Scene {
    <<abstract>>
    +on_enter() +on_exit()
    +on_pause() +on_resume()
    +is_overlay() bool
    +handle_event(e) Transition
    +update(dt) Transition
    +draw()*
  }
  class Transition {
    Kind kind
    SceneFactory factory
    bool fade
    +push() +pop() +replace() +reset() +quit()
  }
  class SceneContext {
    ALLEGRO_DISPLAY* display
    ALLEGRO_FONT* font
    Assets* assets
    Registry* registry
  }
  class IntroScene
  class GameScene
  class MenuScene {
    MenuConfig cfg
  }
  class Game {
    +create(ctx)$ expected
    +handle_event(e)
    +update(dt)
    +draw()
    +outcome() optional
    -World world_
    -Ai[] ais_
    -Camera camera_
    -EntityId[] selected_
    -Corpse[] corpses_
  }
  class Corpse
  class Assets {
    +sprite(path)
    +bitmap(path)
  }
  class ResourceCache~V~ {
    +get(key, loader)
    +live() size_t
  }
  class BuildingArt {
    +load(registry, assets)
    +finished(def) ALLEGRO_BITMAP*
  }
  class SpriteDef {
    BitmapPtr spritesheet
    map~string,s_animation~ animations
  }
  class AnimationState
  class HudData {
    <<view-model>>
  }

  SceneManager *-- SceneStack~Scene~
  SceneManager --> SceneContext
  SceneStack~Scene~ o-- Scene
  Scene ..> Transition : retorna
  Transition ..> Scene : SceneFactory cria
  Scene <|-- IntroScene
  Scene <|-- GameScene
  Scene <|-- MenuScene
  GameScene *-- Game
  Game *-- "world_" World
  Game *-- "ais_" Ai
  Game *-- BuildingArt
  Game o-- "0..*" Corpse
  Game o-- AnimationState
  Game ..> HudData : preenche a cada frame
  Game ..> SpriteDef
  Assets *-- "2" ResourceCache~V~
  BuildingArt --> Assets
  SceneContext --> Assets
  SceneContext --> Registry
  Corpse *-- AnimationState
```

(`World`, `Ai` e `Registry` são as classes do núcleo do diagrama anterior.)

## 4. Fluxo de uma partida

### 4.1 Ciclo de cenas

```mermaid
stateDiagram-v2
  [*] --> Intro
  Intro --> MainMenu : replace
  MainMenu --> Game : "New Game" (replace)
  MainMenu --> Credits : push (Intro)
  Credits --> MainMenu : pop
  Game --> Pause : ESC / F10 (push, overlay)
  Pause --> Game : "Resume" (pop)
  Pause --> Game : "Restart" (reset)
  Pause --> MainMenu : "Quit to Main Menu" (reset)
  Game --> GameOver : Victory / Defeat (replace)
  GameOver --> Game : "Play Again" (reset)
  GameOver --> MainMenu : (reset)
  MainMenu --> [*] : Exit
```

Uma cena nunca instancia outra diretamente: devolve uma `Transition` com uma `SceneFactory`.
O `SceneManager` aplica a transição **entre** chamadas (nunca no meio de um método da cena) e, se
pedido, faz fade para preto. Enquanto uma fábrica roda, a cena antiga ainda existe — se o carregamento
falha, o jogo continua onde estava.

### 4.2 Um quadro (tick) da partida

```mermaid
sequenceDiagram
  autonumber
  participant Main as main loop
  participant SM as SceneManager
  participant GS as GameScene
  participant G as Game
  participant AI as Ai
  participant W as World
  participant EB as EventBus

  Main->>SM: update(dt)  (timer 60 Hz)
  SM->>GS: update(dt)
  GS->>G: update(dt)
  G->>G: câmera (setas / borda da tela)
  G->>AI: update(dt)
  AI->>W: order_train / order_build / order_attack_move ...
  G->>W: update(dt)
  W->>W: update_order() de cada unidade (colheita, construção, ataque)
  W->>W: step_unit() de cada unidade (andar pelo path, reservar tiles)
  W->>W: update_training() (filas dos prédios)
  W->>EB: dispatch()
  EB-->>G: UnitDied, Notice, BuildingCompleted ...
  G->>G: avança animações, corpos, mensagem do HUD
  GS->>G: outcome()
  alt partida decidida
    GS-->>SM: Transition::replace(make_game_over(...))
  end
  Main->>SM: draw()
  SM->>G: draw()  (mapa, entidades, HUD)
```

Entrada do jogador (mouse/teclado) chega por `Game::handle_event`, que a converte numa chamada
`World::order_*` — o único caminho para alterar a simulação.

### 4.3 Ordens de uma unidade

```mermaid
stateDiagram-v2
  [*] --> Idle
  Idle --> Harvest : order_harvest
  Idle --> Build : order_build / order_resume_build
  Idle --> Attack : order_attack (alvo) / auto_acquire
  Idle --> AttackMove : order_attack_move / order_attack (chão)
  Harvest --> Idle : order_stop / order_move
  Attack --> AttackMove : alvo morreu e havia "resume"
  Attack --> Idle : alvo morreu
  AttackMove --> Attack : inimigo à vista
  AttackMove --> Idle : chegou
  Build --> Idle : prédio concluído
  note right of Build
    Um construtor fica "locked":
    ignora outras ordens até terminar.
  end note
```

`Harvest` tem seu próprio subciclo: `ToResource → Working → ToDepot → ToResource …`.
Mover-se **não** é uma ordem: é só seguir o `path` da unidade.

## 5. Conceitos que aparecem em todo lugar

* **`EntityId` em vez de ponteiros.** Unidades e prédios se referenciam só por id
  (`index` + `generation`). Uma unidade pode morrer sem invalidar quem a mirava: `get(id)` devolve `nullptr`.
* **Classes de movimento** (`Land`, `Water`, `Air`). Cada uma tem sua grade de bloqueio e ocupação
  (`World::Layer`). Unidades aéreas só são limitadas pela borda do mapa.
* **Ocupação de tiles.** Uma unidade em movimento reserva o tile atual e o próximo (`Occupancy`).
  Se o próximo está ocupado, ela espera um instante e replaneja evitando os tiles alheios.
* **Pathfinding.** A\* de agente único, 8-conexo, sem cortar quinas. Para grupos, `assign_destinations`
  escolhe tiles livres próximos ao alvo e os distribui às unidades; cada unidade roda o A\* até o seu.
* **Comida.** Prédios concluídos fornecem (`food`), unidades consomem vivas *ou na fila*. `order_train` recusa acima do teto.
* **Combate.** Dano = rolagem entre `damage_min` e `damage_max` menos `armor` (mínimo 1), usando o RNG
  com *seed* do `World` — mesma seed, mesma luta. `siege_multiplier` multiplica o dano contra prédios.
* **Eventos.** O `World` enfileira eventos durante o `update` e só os entrega no fim (`dispatch`), então
  um *handler* nunca roda no meio de uma mudança de estado.
* **Dados primeiro.** Novas unidades/prédios/comandos são entradas JSON; veja "Adding content" em
  [ARCHITECTURE.md](ARCHITECTURE.md).
* **RAII para tudo do Allegro.** `raii.hpp` embrulha `ALLEGRO_BITMAP`, `tmx_map` etc. Cenas são donas dos
  seus recursos e os liberam no destrutor; `Assets` guarda só `weak_ptr`.

## 6. Mapa do código

```
src/core/
  world.hpp/.cpp      World: estado, spawn, movimento (step_unit), update
  orders.cpp          order_* e handlers de Harvest/Build (parte da classe World)
  combat.cpp          ataque, auto-aquisição de alvos, treinamento (parte da classe World)
  defs.hpp/.cpp       Registry e *Def (dados carregados de JSON)
  ai.hpp/.cpp         jogador computador
  rules.hpp/.cpp      vitória / derrota
  entity_store.hpp    slot map; ids.hpp: EntityId, PlayerId
  events.hpp          EventBus e os tipos de evento
  grid.hpp            Point, Grid
  pathfinding.*       a_star, path_to_adjacent, is_adjacent
  formation.*         assign_destinations
  occupancy.hpp       reserva de tiles
  harvest.*           TreeField, ApproachPath
  building.*          Stock/Cost, GoldMine, footprint, can_place
  geometry.hpp        Vec2, Rect, Camera, Minimap
  selection.hpp       pick_at, pick_in
  scene_stack.hpp     SceneStack<S>;  resource_cache.hpp: ResourceCache<V>
src/game/
  main.cpp            inicialização do Allegro e loop principal
  scene*.{hpp,cpp}    Scene, Transition, SceneManager
  scenes.*            IntroScene, GameScene e fábricas make_*
  menu_scene.*        MenuScene (menus configuráveis)
  game.hpp/.cpp       Game: criação da partida, comandos, update
  game_input.cpp      mouse e teclado → ordens
  game_render.cpp     desenho do mundo
  ui.*                HUD (view-models + desenho)
  sprite.*            SpriteDef, animações
  assets.*, building_art.*, tilemap.*, raii.hpp, draw_util.*
tests/                Catch2: um arquivo por módulo do core
assets/               data/*.json, sprites, tiles, prédios
```
