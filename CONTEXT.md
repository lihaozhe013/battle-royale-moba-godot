# Project Context

## Current Architecture

- Godot 4.7 top-down Battle Royale MOBA.
- Application startup loads the full-screen code-built `StartMenu` scene; `scenes/main.tscn` remains the gameplay scene and is loaded only after `Start Match`.
- C++ GDExtension simulation runs at 30 Hz and has no Godot dependency inside `sim/` systems.
- GDScript view and input code run at the render rate, normally 60 Hz.
- `SimSnapshot` is the only Sim-to-View data channel; skill slot snapshots include the authoritative current-level `cast_range`, and local hero snapshots include `attack_range`.
- Player and bot views use the shared Arcane Duel character presentation. Idle/run clips, W cast presentation, basic-attack cast presentation, and basic-attack under-attack reaction are selected from snapshot state in the Godot view layer.
- Basic attack arrow snapshots expose `owner_id` and `source_skill_id`; the view uses these fields to trigger the owner attack animation and to keep channel-burst projectiles separate from basic-attack fireballs.
- Basic-attack impact snapshots expose an event type and source skill id; only source skill id `0` triggers the Arcane Duel under-attack animation.
- Input uses the MOBA command flow: ground right-click movement, Q/W/E/R skills, and A attack commands.
- Native builds use Meson with `clang++` discovery from `PATH`; game sources use C++20 and no `build_env.yaml` file is required.

## Runtime Logging Contract

- Project runtime logs are routed through the `DebugLogger` autoload instead of Godot `print()` calls.
- `DebugLogger` buffers messages and flushes them to the configured log path every 250 ms and when the tree exits.
- The current development log path is `res://debug.log`; the logger falls back to `user://debug.log` when the resource path is not writable.
- `application/run/disable_stdout=true` keeps project logs out of the Godot editor Output panel. Godot engine warnings and errors written to stderr remain visible.
- Godot file logging is disabled because `DebugLogger` owns the configured file and avoids duplicate writes.
- See `Docs/Reference/logging.md` for the logging configuration, performance implications, and verification command.

## Main Runtime Ownership

Application flow:

```text
StartMenu (full-screen Control, no simulation)
└── Start Match → Main / sim_bridge.gd
```

```text
Main / sim_bridge.gd
├── WorldBootstrap   static light, environment, ground
├── UIRoot           persistent code-built 2D UI and health-bar pool
├── CameraController camera follow and camera input behavior
├── EntityManager    entity view synchronization
├── SkillVFX         skill presentation
└── MoveTargetVFX    movement target presentation
```

`sim_bridge.gd` creates the input helper nodes (`InputEventQueue`, `InputStateMachine`, `CommandBuffer`, `CommandBuilder`, and `CastSettings`) at runtime. They remain outside `UIRoot`.

`SkillVFX` owns the shared cast/attack range indicator, dash-path and AoE presentation, and dispatches registered per-skill VFX scenes from snapshot cast transitions.
Each skill VFX is isolated in `resources/vfx/skills/<skill>/` plus `scripts/view/skill_vfx/<skill>_vfx.gd`; `SkillVfxAttachment` is only a generic target anchor.

## UI Bootstrap Contract

- `StartMenu` is a standalone full-viewport scene. It owns the start/settings/quit navigation and does not create `SimServer`, map visuals, or gameplay input nodes.
- `StartMenu` uses a black background with an optional future texture hook and a responsive two-column layout that collapses to the navigation column on narrow viewports.
- `UIRoot.initialize()` runs once during child readiness before `Main._ready()`.
- UI is created by code under explicit canvas layers: world overlay `10`, HUD `100`, feedback `101`, and modal settings `200`.
- UI scenes under `scenes/ui/` are not runtime dependencies.
- `UIStyle` is the code-only authority for UI fonts, colors, dimensions, and style factories.
- `SettingsPanelUI` is reusable in the start menu and gameplay. It persists the shared camera/display/cast settings and emits a confirmed main-menu request only in gameplay context.
- Opening settings never pauses an active match. Returning to the start menu clears any existing scene-tree pause left by the current game-over flow.
- `BottomHUD` keeps its existing 750 x 108 composition, applies a uniform scale to 75% of the current viewport width, and anchors its lower edge to the viewport bottom.
- `HealthBarManager` prewarms all health bars before the first simulation tick using `SimServer.get_hero_capacity()`.
- Normal snapshot synchronization does not instantiate or destroy UI nodes.

## World Bootstrap Contract

- `WorldBootstrap.initialize()` runs once.
- It creates the directional light, `WorldEnvironment`, and static ground in code while preserving the prior orientation and geometry. The current lighting tune uses 1.8 directional-light energy, 0.45 ambient-light energy, and a neutral ambient color to keep shadows visible without the prior purple cast.
- Map-wall meshes remain generated by `sim_bridge.gd` from map JSON.

## Simulation API Addition

`SimServer.get_hero_capacity()` is a read-only binding available after successful initialization. It returns the configured local hero plus bot capacity and is used only for deterministic health-bar preallocation.

## Authoritative References

- `Docs/Reference/ui_world_bootstrap_plan.md` — complete refactor design.
- `Docs/Reference/game_main_process_execution_order.md` — startup, tick, and render-frame order.
- `Docs/Reference/start_menu_design.md` — start-menu ownership, navigation, and settings behavior.
- `Docs/Reference/sim_api_reference.md` — C++ simulation and GDExtension API.
- `Docs/DATA_FLOW.md` — end-to-end input, simulation, snapshot, and view flow.
- `Docs/Reference/skill_vfx_architecture.md` — per-skill VFX ownership, layout, and snapshot event contract.
