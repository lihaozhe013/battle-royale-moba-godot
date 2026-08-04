# UI and World Bootstrap Refactor Plan

**Status:** Proposed design; implementation not started.

**Scope:** Godot view-layer UI and static root-world visuals. The C++ simulation architecture, input command flow, entity scenes, and gameplay behavior remain unchanged except for one read-only capacity getter required for deterministic UI preallocation.

## 1. Current Assessment

The current `main.tscn` contains several permanent presentation nodes:

- `BottomHUD` is a `CanvasLayer` scene and instantiates four skill-slot scenes and six item-slot scenes.
- `SettingsPanel`, `CastBarLayer`, and `CastErrorLayer` are separate `CanvasLayer` scenes.
- `HealthBarManager` is a scene-authored `Node`, but its implementation already contains a code fallback for constructing `HealthBarUI`.
- `HealthBarManager` still preloads `health_bar_ui.tscn` from `sim_bridge.gd`, so the fallback is not the authoritative path.
- The root scene also owns a directional light, a `WorldEnvironment`, a 100 x 100 ground plane, and an unused `CanvasLayer`.

`sim_bridge.gd` currently depends on direct child paths such as `$BottomHUD`, `$CastBarLayer`, `$CastErrorLayer`, and `$HealthBarManager`. This couples the bridge to the authored scene tree and makes visual hierarchy changes more fragile than they need to be.

The current light, environment, and ground have no runtime script dependencies. They are static root-world configuration and can be built in code without changing simulation behavior. They must not be placed in the UI layer because that would mix 2D presentation ownership with 3D world-rendering ownership.

## 2. Target Ownership Model

`Main` will contain two explicit composition nodes:

```text
Main (Node3D, SimBridge)
├── WorldBootstrap (Node)
├── UIRoot (Node)
├── CameraController
├── EntityManager
├── SkillVFX
└── MoveTargetVFX
```

### `WorldBootstrap`

`WorldBootstrap` is responsible only for static root-world visuals:

1. Create one `DirectionalLight3D`.
2. Create one `WorldEnvironment` with one `Environment`.
3. Create one ground `MeshInstance3D` with a `PlaneMesh` and a `StandardMaterial3D`.

It must preserve the current values from `main.tscn`:

- directional-light shadow enablement;
- directional-light orientation;
- environment background mode and color;
- ambient light source and color;
- ground size and material behavior.

The existing directional-light translation is not meaningful for a directional light. The implementation should retain the orientation as a named static basis/configuration and omit the unused translation.

Map-wall visuals remain owned by `sim_bridge.gd` because they are generated from map JSON and are not static root configuration.

### `UIRoot`

`UIRoot` is a persistent `Node` that creates all current 2D UI once in its guarded `initialize()` method. It owns four explicit canvas layers:

| Layer | Purpose |
|---:|---|
| 10 | World-space health bars |
| 100 | Main HUD |
| 101 | Cast progress and cast errors |
| 200 | Settings/modal UI |

The root retains all created nodes for the lifetime of the match. Visibility and state change; persistent UI nodes are not destroyed.

The root exposes typed references or methods for:

- `BottomHUD`;
- `HealthBarManager`;
- `CastBar`;
- `CastError`;
- `SettingsPanel`.

`sim_bridge.gd` binds runtime dependencies explicitly through one method, conceptually:

```gdscript
ui_root.bind_runtime(entity_manager, camera, cast_settings)
```

This removes UI code's dependency on sibling lookup paths such as `../CastSettings`.

## 3. Code-Only UI Design System

Create one shared `UIStyle`/`UITokens` script under `scripts/ui/`.

It is the only authority for:

- font resource preloads;
- palette colors;
- standard dimensions and spacing;
- HUD scale and bar widths;
- common label settings;
- `StyleBoxFlat` factory methods.

Factories must return a new style resource per owner. A mutable `StyleBox` must not be shared between unrelated controls. This prevents one widget's customization from changing another widget unexpectedly.

Each UI component becomes a code-built control with a small, explicit construction contract:

- `build(style)` creates children and stores typed references;
- update methods mutate those stored references;
- no `@onready` scene paths;
- no UI scene instantiation;
- no per-frame hierarchy rebuilding.

The migration preserves the current visual layout and behavior. It does not redesign the HUD.

### Component migration

- `BottomHUD` becomes a code-built control that creates the avatar, stat text, resource bars, skill row, item rows, and backpack row.
- Skill slots and item slots are created in loops, with their references appended when built. This also makes item synchronization deterministic instead of depending on scene children being present.
- `SettingsPanel` builds its rows and buttons in code, connects signals directly, and keeps the current camera, edge-pan, smooth-pan, fullscreen, cast-mode, quit, close, and Escape behavior.
- `CastBar` and `CastError` build their controls in the feedback layer and begin hidden.
- `HealthBarUI` builds its badge, labels, bars, and status control directly.

The existing font and avatar texture files remain valid code-loaded resources. The seven files under `scenes/ui/` become removable only after all references are gone.

## 4. Deterministic Health-Bar Preallocation

The match currently spawns one local hero plus the configured bot count. Respawn reuses those entities; it does not increase the required visible-hero capacity.

Add a read-only API:

```text
World::hero_capacity() -> int
SimServer.get_hero_capacity() -> int
```

The value is available only after successful simulation initialization and is derived from the parsed simulation configuration, not duplicated in UI code.

Startup sequence:

1. `UIRoot` builds static UI and creates an empty health-bar pool.
2. `sim_bridge.gd` creates and configures the input helper nodes.
3. `SimServer.initialize(map_json, stats_yaml)` loads the authoritative bot count.
4. `sim_bridge.gd` calls `ui_root.prewarm_health_bars(sim.get_hero_capacity())`.
5. `HealthBarManager` creates exactly that many `HealthBarUI` nodes once and adds them to the world overlay.
6. Gameplay only assigns, resets, hides, and returns those nodes to the pool.

`HealthBarManager` must not load or instantiate `health_bar_ui.tscn`. If the pool is exhausted, it reports a clear `[ui_bootstrap]` error so a capacity-contract violation is visible instead of silently reintroducing runtime allocation.

## 5. `main.tscn` and Bridge Changes

Remove these scene-authored UI/world definitions from `main.tscn`:

- `HealthBarManager`;
- `BottomHUD`;
- `SettingsPanel`;
- `CastBarLayer`;
- `CastErrorLayer`;
- the unused root `CanvasLayer`;
- `DirectionalLight3D`;
- `MeshInstance3D` ground;
- `WorldEnvironment`;
- their UI/world subresources and UI scene ext-resources.

Add `WorldBootstrap` and `UIRoot` as scripted children.

Update `sim_bridge.gd` so it:

- stores one typed `ui_root` reference;
- binds runtime dependencies once;
- requests health-bar preallocation after simulation initialization;
- routes snapshot updates through `ui_root` references;
- removes all UI scene preloads and direct UI child-path lookups.

Input helper nodes, `SkillVFX`, map-wall visuals, `EntityManager`, camera behavior, and entity prefab loading remain outside `UIRoot`.

## 6. Public Interface and Invariants

### New public interfaces

- `SimServer.get_hero_capacity() -> int`.
- `UIRoot.initialize() -> void`, guarded against duplicate execution.
- `UIRoot.bind_runtime(entity_manager, camera, cast_settings) -> void`.
- `UIRoot.prewarm_health_bars(capacity) -> void`.

### Invariants

- `SimSnapshot` remains the only Sim-to-View data channel.
- The C++ simulation never references Godot UI types.
- No UI system mutates the simulation registry.
- No UI node is created or destroyed during normal snapshot synchronization.
- World rendering and UI rendering have separate ownership roots.
- UI layer order is explicit and documented.

## 7. Verification and Acceptance Criteria

Run:

```text
make build
godot --headless --path . --editor --quit
```

Then perform a visual match run and verify:

1. `WorldBootstrap` and `UIRoot` each initialize once before the first simulation tick.
2. HUD values, skill slots, item slots, cast progress, cast errors, settings controls, and Escape priority match current behavior.
3. The light direction, shadows, ambient color, background color, and ground appearance match the current scene.
4. Exactly `SimServer.get_hero_capacity()` health bars are prebuilt and reused through deaths and respawns.
5. No `res://scenes/ui/*.tscn` resource is loaded or instantiated.
6. No direct `$BottomHUD`, `$CastBarLayer`, `$CastErrorLayer`, or `$HealthBarManager` bridge dependency remains.
7. No C++ simulation architecture rule is violated.

## 8. Rollout Order

Implement in small reversible phases:

1. Add the Sim capacity getter and documentation.
2. Add `UIStyle` and convert leaf widgets: skill slot, item slot, and health bar.
3. Convert `BottomHUD`, cast UI, and settings UI.
4. Add `UIRoot` and route `sim_bridge.gd` through it.
5. Add `WorldBootstrap` and move the three static root visual nodes.
6. Remove obsolete UI scene files and scene resources.
7. Build, run headless validation, and perform the visual acceptance pass.

If a phase fails visually, restore the previous scene owner for that phase while keeping earlier verified phases intact.

