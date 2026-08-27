# Battle Royale MOBA

Battle Royale MOBA is a top-down action prototype built around a clear separation between authoritative gameplay simulation and real-time presentation. The project explores how a MOBA-style command model, hero combat, bot behavior, pathfinding, and a responsive Godot view can work together without coupling gameplay rules to rendering code.

## Design Direction

The project follows four central ideas:

- **Simulation is authoritative.** Movement, targeting, casting, combat, progression, pickups, and bot decisions are resolved by a fixed-rate simulation rather than by rendered frames.
- **Simulation and presentation are independent.** The simulation does not depend on Godot scene nodes or view behavior. The Godot layer sends commands and renders the latest published state.
- **Player and bots share the same gameplay pipeline.** Bots express movement, attack, and skill intent through the same hero input state used by the local player. They then pass through the same pathfinding, movement, casting, and combat systems.
- **Gameplay is data-oriented and extensible.** Entities are composed from components, systems operate on those components, hero definitions reference skills by ID, and individual skills implement their own behavior behind a common interface.

The intended interaction model is a focused MOBA command scheme: right-click movement, Q/W/E/R skills, and A-based attack commands. The project deliberately keeps one command model instead of maintaining separate WASD and MOBA modes.

## Technology Stack

| Area | Technology | Purpose |
| --- | --- | --- |
| Engine | Godot 4.7 | Windowing, rendering, scenes, input, and presentation |
| Simulation bridge | C++ GDExtension | Exposes the simulation to Godot without moving gameplay rules into GDScript |
| Simulation language | C++20 | Fixed-rate gameplay logic and runtime data processing |
| Entity model | EnTT ECS | Composes entities from components and keeps systems independent |
| Math | GLM-style vector math | Godot-independent simulation geometry and movement calculations |
| View layer | GDScript | Input interpretation, snapshot consumption, UI, camera, entities, and VFX |
| Configuration | JSON and YAML | Map geometry, walls, balance values, hero data, and skill tuning |
| Native build system | Meson | Produces the GDExtension shared library |

## High-Level Architecture

```text
┌──────────────────────────── Godot View ────────────────────────────┐
│  InputEventQueue → InputStateMachine → CommandBuilder              │
│                                      → CommandBuffer                │
│                                                                     │
│  EntityManager · Camera · UIRoot · SkillVFX · WorldBootstrap        │
└───────────────────────┬─────────────────────────▲─────────────────┘
                        │ commands                │ SimSnapshot
                        ▼                         │
┌──────────────────────── C++ Simulation ─────────┴──────────────────┐
│  SimServer → World → EnTT Registry → fixed-order systems            │
│                                                                     │
│  heroes · bots · skills · navigation · movement · combat · pickups  │
└─────────────────────────────────────────────────────────────────────┘
```

### Simulation layer

`SimServer` is the GDExtension-facing entry point. It owns a simulation `World`, which contains the EnTT registry, navigation data, command buffer, simulation clock, and random number generator.

The world advances at 30 Hz. Each tick applies the current input state and runs a defined system pipeline covering:

1. Local input injection and bot decision-making.
2. Unified attack and skill command processing.
3. A* pathfinding, movement, and wall collision.
4. Projectiles, direct combat, area effects, status effects, and resource regeneration.
5. Pickups, skill cooldowns, skill leveling, and progression.
6. Snapshot export for the view layer.

Entity creation and destruction are deferred through `CommandBuffer`, so systems do not invalidate the registry while iterating over it. Systems communicate through components rather than direct cross-system calls or global state.

### Hero and skill model

Players and bots are heroes in the same simulation model. A hero owns runtime components such as health, resources, movement state, input state, progression, and skill slots. A local-player marker distinguishes the controlled hero from AI-controlled heroes; it does not create a separate combat implementation.

Hero definitions provide base data and references to four skill IDs. Skills are independent `ISkill` implementations registered in a skill registry. The cast system dispatches validation, targeting, cast lifecycle, and effects to the selected skill, allowing new skills to be added without expanding a monolithic hero or combat switch.

Bots use a layered decision model. Goal selection, combat phase selection, and skill scoring produce intents that are injected into `HeroInputState`. The regular hero systems then handle chasing, movement, attacks, and casts, keeping AI behavior aligned with player behavior.

### View layer

`sim_bridge.gd` coordinates the runtime boundary. It collects input, translates semantic commands into `SimServer` calls, advances the simulation, and publishes snapshots to presentation systems.

The view runs at the render rate, normally 60 Hz. It interpolates or synchronizes presentation state independently of the 30 Hz simulation, while sequence-gating snapshot updates so entities, health bars, and one-shot events are not processed repeatedly.

The main presentation responsibilities are split into focused components:

- `EntityManager` synchronizes hero, projectile, pickup, and other world views.
- `CameraController` handles follow, free camera, drag, edge-pan, and camera smoothing behavior.
- `UIRoot` owns the persistent HUD, health bars, skill slots, cast feedback, and settings presentation.
- `SkillVFX` dispatches per-skill visual effects from authoritative cast transitions.
- `WorldBootstrap` creates static lighting, environment, ground, and world presentation.
- `MoveTargetVFX` presents movement command feedback without changing simulation state.

## Data Flow

```text
Godot input events
        ↓
InputEventQueue
        ↓
InputStateMachine + CommandBuilder
        ↓
CommandBuffer
        ↓
SimServer command API
        ↓
30 Hz World tick
        ↓
EnTT components and systems
        ↓
SnapshotBuilder → SimSnapshot
        ↓
EntityManager / UI / Camera / SkillVFX
```

The `CommandBuffer` preserves input edges between render frames and simulation ticks. The `SimSnapshot` is the only simulation-to-view data channel: the view never reaches into the ECS registry, and the simulation never calls view code.

## Core Design Decisions

### Fixed-rate authority with render-rate presentation

Gameplay uses a stable 30 Hz simulation clock. Presentation can update more frequently and smooth visible motion without changing the authoritative result of a tick.

### ECS systems over object hierarchies

Gameplay state is stored in components such as position, health, hero input, skill slots, cast state, bot state, and projectile state. Small systems transform that state in a predictable order, making player, bot, and world interactions share the same rules.

### Deferred world mutation

Systems request entity creation or destruction through the command buffer. The world flushes those requests at the end of the tick, preserving safe iteration and explicit lifecycle boundaries.

### Snapshot-based isolation

Snapshots provide a narrow, inspectable contract between simulation and view. They carry state and discrete events needed for rendering, animation, UI, and VFX without exposing simulation internals.

### Presentation as a consumer

Godot scenes and scripts are responsible for displaying state, responding to input, and managing visual lifecycles. They do not decide damage, movement validity, skill results, bot outcomes, or other authoritative gameplay rules.

## Repository Areas

```text
src_cpp/       C++ GDExtension, simulation world, ECS systems, heroes, skills, snapshots
scripts/       GDScript bridge, input pipeline, view systems, UI, camera, and VFX
scenes/        Main scene and reusable presentation scenes
resources/     Character, UI, map, and skill presentation assets
data/          Map and gameplay configuration data
Docs/          Short agent-oriented operational notes
```
