# Game Main Process Execution Order

**Status:** Reference for the current and planned bootstrap architecture.

**Purpose:** Define the order in which the Godot view, input pipeline, C++ simulation, snapshot export, and presentation updates execute. This document is the execution contract for `Main` and `sim_bridge.gd`.

## 1. Runtime Ownership

```text
Main / sim_bridge.gd
├── WorldBootstrap       static 3D light, environment, ground
├── UIRoot               persistent 2D UI and health-bar pool
├── CameraController     camera follow and camera input behavior
├── EntityManager        entity view synchronization
├── SkillVFX             skill presentation
├── MoveTargetVFX        movement target presentation
├── InputEventQueue      raw input collection
├── InputStateMachine    input state mirror
├── CommandBuffer        cross-tick command FIFO
├── CommandBuilder       semantic command creation
└── CastSettings         cast-mode configuration
```

The C++ `SimServer` owns the authoritative simulation. The view communicates with it through command methods and consumes `SimSnapshot` objects. The view never writes directly to the ECS registry.

## 2. Scene Loading and Readiness

Godot first loads `project.godot` and its main scene, `res://scenes/main.tscn`. The scene creates `Main` and its persistent child nodes.

Child readiness completes before `Main._ready()` runs. The target scene ordering is:

1. `WorldBootstrap._ready()` creates the directional light, environment, and ground.
2. `UIRoot._ready()` calls its guarded `initialize()` once and builds the static UI hierarchy.
3. Camera, entity, VFX, and other view-node readiness completes.
4. `Main` enters `sim_bridge.gd._ready()`.

The bridge must not depend on an unready sibling. Dependencies that cross subsystem boundaries are bound explicitly from `sim_bridge.gd._ready()` after all child readiness callbacks have completed.

## 3. `sim_bridge.gd._ready()` Order

The bridge performs the following operations in order:

### 3.1 Load static simulation inputs

1. Read `data/maps/default.json`.
2. Read `data/stats.yaml`.
3. Abort with an error if either file cannot be opened.

### 3.2 Create input helpers

Call `_ensure_node()` for:

1. `InputEventQueue`;
2. `InputStateMachine`;
3. `CommandBuffer`;
4. `CommandBuilder`;
5. `CastSettings`.

Then call `CommandBuilder.setup(input_event_queue, input_state_machine, command_buffer, cast_settings)`.

These nodes belong to the input pipeline, not to `UIRoot`.

### 3.3 Create map visuals

Parse map JSON and create the wall meshes used by the view. Wall visuals are generated from map data and remain owned by `sim_bridge.gd`.

### 3.4 Bind UI runtime dependencies

Call the single UI binding method with:

- `EntityManager`;
- the active `Camera3D`;
- `CastSettings`.

This allows the health-bar manager and settings panel to operate without sibling path lookups.

### 3.5 Initialize the simulation

1. Create `SimServer`.
2. Call `sim.initialize(map_json, stats_yaml)`.
3. Abort if initialization fails.
4. Read `sim.get_hero_capacity()`.
5. Call `UIRoot.prewarm_health_bars(capacity)` exactly once before the first simulation tick.

### 3.6 Initialize skill presentation

Ensure the `SkillVFX` node exists, attach its presentation script if needed, and retain the node for the match lifetime.

## 4. Input and Simulation Tick Order

The simulation runs at 30 Hz using an accumulator. Godot may execute zero, one, or multiple simulation ticks during one `_physics_process()` call.

For each simulation tick:

1. Determine whether this is the first tick within the current physics frame.
2. On the first tick only, call `CommandBuilder.process_frame()` to drain render-frame input events into semantic commands.
3. Pop all commands from `CommandBuffer`.
4. Merge compatible commands.
5. Apply merged commands into bridge adapter fields.
6. Send the current command state to `SimServer`:
   - skill command;
   - skill upgrade command;
   - attack command;
   - cancel command;
   - move command;
   - stop command.
7. Call `sim.tick(TICK_RATE)`.
8. Clear pulse fields after the tick. First-tick-only fields are cleared only after the first tick of the frame; per-tick pulse fields are cleared after every tick.
9. If the simulation reports game over, pause the tree and stop further simulation work.
10. Pop the latest snapshot and cache it as `last_snapshot`.

After at least one simulation tick has run, synchronize `InputStateMachine` from the local hero/player snapshot so the view-side FSM mirrors authoritative cast and movement state.

## 5. Render-Frame Presentation Order

`sim_bridge.gd._process()` runs at the render-frame rate, normally 60 Hz.

If there is no cached snapshot, return without presentation work. Otherwise:

### 5.1 Input hover sampling

1. Read the current mouse world position.
2. Find the nearest valid non-local living hero/bot within the hover radius.
3. Update `EntityManager` hover state.

### 5.2 Snapshot sequence gate

Only when `last_snapshot.seq` differs from `_last_snap_seq`:

1. Cache the new sequence value.
2. Synchronize entity views through `EntityManager.sync_entities(last_snapshot)`.
3. Synchronize world-space health bars through `HealthBarManager.sync_bars(last_snapshot)`.
4. Locate the local hero/player.
5. Update attack-target presentation.
6. Detect completed casts and trigger skill-hit VFX when applicable.
7. Detect a new cast error and call `CastError.show_error(code)`.
8. Synchronize `InputStateMachine` from the local authoritative snapshot.

This gate prevents repeated entity and health-bar synchronization when the render frame runs faster than snapshot production.

### 5.3 Continuous local-player presentation

Every render frame with a valid local snapshot:

1. Update camera follow position.
2. Call `BottomHUD.sync_player(player)`.
3. Call `BottomHUD.sync_skills(player.skills)`.
4. Show or hide cast progress through `CastBar.sync_cast()` / `CastBar.hide_cast()`.
5. Set skill-aiming state on `SkillVFX`.
6. Synchronize `SkillVFX` with the current snapshot and local entity view.

These updates are presentation-only and do not modify simulation state.

## 6. Input Event Priority

Input event priority remains:

1. Raw events enter `InputEventQueue`.
2. The input state machine and command builder interpret event edges.
3. If Escape cancels an active cast, the input layer consumes the event.
4. If Escape is not consumed by cast cancellation, it continues to `SettingsPanel._unhandled_input()` and toggles the settings panel.

Moving UI creation into code must not change this event propagation order.

## 7. Startup and Runtime Invariants

- `WorldBootstrap.initialize()` runs once.
- `UIRoot.initialize()` runs once.
- Static UI and root-world visual nodes are not recreated per frame.
- Health-bar nodes are all prebuilt before the first simulation tick and are reused thereafter.
- `CommandBuffer` remains the only deferred command queue between input and simulation.
- `SimSnapshot` remains the only Sim-to-View channel.
- `EntityManager` may pool or remove entity views according to snapshot membership; this does not change the persistent UI contract.
- Game-over pauses the scene tree after the current simulation tick and prevents later simulation ticks.

## 8. Failure Handling

- Missing map or stats files stop initialization and report an error.
- Failed `SimServer.initialize()` stops the startup sequence before health-bar prewarming.
- Missing camera binding causes health-bar projection to report an error and skip positioning.
- Health-bar pool exhaustion reports `[ui_bootstrap]` and does not allocate a new UI node during gameplay.
- A missing local snapshot skips local-player presentation for that frame without touching simulation state.

## 9. Verification Checklist

During implementation and review, verify:

- `WorldBootstrap` and `UIRoot` are ready before `sim_bridge.gd._ready()` uses them.
- `get_hero_capacity()` is called only after successful simulation initialization.
- First-frame command processing occurs once even when multiple simulation ticks run in one physics frame.
- Pulse command fields are cleared at the documented boundaries.
- Snapshot sequence gating prevents duplicate entity and health-bar synchronization.
- UI callbacks and settings events still reach the same handlers.
- No scene-authored UI, light, environment, or ground node is reintroduced after the code migration.

