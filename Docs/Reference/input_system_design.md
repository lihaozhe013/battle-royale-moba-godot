# Input System Design (v2 — Single Source of Truth)

> Last updated: 2026-08-02
> Status: ✅ **Fully implemented** — this is the design and operational reference for the input system.
> Replaces: `skill_system_design.md`, `targeted_attack_design.md`, `right_click_movement_design.md`, `skill_cast_error_fix.md` (all deleted).
> Related: `sim_api_reference.md` (Sim component/API reference), `docs/DATA_FLOW.md` (end-to-end data flow), `hero_skill_architecture.md` (Hero + Skill refactor).

---

## 0. Historical clarification and scope

- The project **previously had WASD + MOBA dual input modes**. The WASD mode has been **completely removed**. Any older docs mentioning WASD mode, `MoveMode` enum, `move_mode` field, `mode_changed` signal, or dual-mode switch panels are **obsolete** — **this document is the only standard**.
- Current and future: **only MOBA mode** (right-click ground to move + Q/W/E/R skills + A for basic attack).
- This doc covers: full refactor of `input_controller` + the Sim-side cast / basic-attack / move input pipeline, with a layered, stateful, lossless command system.
- **Not in scope**: Bot behavior refactor, equipment, zone shrinking, death/elimination. Bots are placeholders (their "attack" is a non-targeted skill placeholder, not a real basic attack); they will be refactored to be full heroes. **Bot current behavior does NOT influence input_controller design**.

---

## Table of contents

1. [Design goals and principles](#1-design-goals-and-principles)
2. [Overall architecture (four layers)](#2-overall-architecture-four-layers)
3. [Layer 1 — Input event queue (no command loss)](#3-layer-1--input-event-queue-no-command-loss)
4. [Layer 2 — Input state machine (View-side FSM)](#4-layer-2--input-state-machine-view-side-fsm)
5. [Layer 3 — Command translation (Command Builder)](#5-layer-3--command-translation-command-builder)
6. [Layer 4 — Command buffer and Sim consumption](#6-layer-4--command-buffer-and-sim-consumption)
7. [Sim-side CastState refactor (incl. Chasing)](#7-sim-side-caststate-refactor-incl-chasing)
8. [Quick Cast and Normal Cast flow](#8-quick-cast-and-normal-cast-flow)
9. [Basic attack command mode (independent branch)](#9-basic-attack-command-mode-independent-branch)
10. [Movement and pathfinding (A* chase/follow)](#10-movement-and-pathfinding-a-chasefollow)
11. [SimServer API (unified command interface)](#11-simserver-api-unified-command-interface)
12. [Snapshot extensions (state echo sync)](#12-snapshot-extensions-state-echo-sync)
13. [Tick order](#13-tick-order)
14. [Component change list (historical — fully implemented)](#14-component-change-list-historical--fully-implemented)
15. [File change list (historical)](#15-file-change-list-historical)
16. [Implementation phases (historical — all complete)](#16-implementation-phases-historical--all-complete)
17. [Edge cases and pitfalls](#17-edge-cases-and-pitfalls)
18. [Clarifications on Bot units](#18-clarifications-on-bot-units)
19. [Summary](#19-summary)

---

## 1. Design goals and principles

| # | Goal | Principle |
| --- | --- | --- |
| G1 | `input_controller` decoupled, clear responsibilities | Four layers: raw events / state machine / command translation / command buffer |
| G2 | Explicit state machine, no scattered conditional branches | View and Sim **each maintain an FSM**, bidirectionally synced via **Snapshot** |
| G3 | Cast interruption must not introduce bugs | input layer **mirrors Sim's CastState**; interruption = state transition, not scattered flags |
| G4 | No command loss (30Hz Sim < 60Hz render) | A single **CommandBuffer** layer handles cross-tick commands; **no per-action handling** |
| G5 | Quick cast / Normal cast coexist | Player preference (per-slot or global) decides; input layer branches |
| G6 | Normal cast / basic attack command / move modes are independent | **Three independent FSM branches**; most decoupled, fewest bugs |
| G7 | Chase-and-cast (A* follow when out of range) | Sim-side new **Chasing** phase, advanced by Sim |
| G8 | Basic attack passes through walls | Sim-side `wall_collision` skips chasing players + homing arrows |

**Core principles**:

1. **Sim authoritative**: all gameplay state (`CastState` / `AttackTarget` / `MovePath`) is owned by Sim; View reads snapshot.
2. **View mirror**: `input_controller` maintains a local FSM copy used **only to decide how to translate the next input event**; never a source of gameplay truth.
3. **Commands, not raw events**: View→Sim transmits **commands** ("cast slot 2 at (x,y) on target_id confirmed"), not raw keypresses.
4. **No event loss**: all edges (key press/release, mouse click) queue; CommandBuffer consumes until empty.

---

## 2. Overall architecture (four layers)

```
┌──────────────────────────────────────────────────────────────┐
│  Godot raw input (InputEvent / Input.is_*_pressed)          │
│  60Hz _process / _physics_process                            │
└──────────────────────────┬───────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────┐
│  Layer 1 — Input Event Queue (GDScript)                      │
│  - _input/_unhandled_input collects edge events to queue     │
│  - Persistent state (held keys, mouse pos) sampled per frame │
│  - Queue element: {type, key/button, pos, timestamp, seq}    │
│  - Never lost; Layer 3 drains                                 │
└──────────────────────────┬───────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────┐
│  Layer 2 — Input State Machine (GDScript)                    │
│  - Two orthogonal axes:                                       │
│      MoveAxis   : Moving | NotMoving                          │
│      CommandAxis: Idle | SkillAiming | AttackAiming | CastLocked │
│  - Reads Sim snapshot cast_state to sync CastLocked           │
│  - Outputs "which events to process this frame"               │
└──────────────────────────┬───────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────┐
│  Layer 3 — Command Builder (GDScript)                         │
│  - FSM state + event queue → semantic commands               │
│  - Command types: MoveCmd / SkillCmd / AttackCmd / CancelCmd / StopCmd │
│  - Quick vs Normal cast branches here                         │
└──────────────────────────┬───────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────┐
│  Layer 4 — Command Buffer (GDScript + C++)                   │
│  - GDScript: FIFO queue; multiple per frame allowed           │
│  - sim_bridge: per Sim tick, pop all N → SimServer.set_command │
│  - Cross-tick persistent; never lost (no cap on small bursts) │
└──────────────────────────┬───────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────┐
│  C++ Sim (30Hz)                                               │
│  - Consume commands → write LocalInputSingleton              │
│  - local_input_injection → attack_command →                  │
│    skill_cast → pathfinding → movement → attack_fire →       │
│    physics ...                                                │
│  - Outputs SimSnapshot (cast_state / attack_target_id / ...) │
└──────────────────────────┬───────────────────────────────────┘
                           │
                           ▼
            SimSnapshot → View → Layer 2 reverse sync
```

---

## 3. Layer 1 — Input event queue (no command loss)

### 3.1 Motivation

Sim runs at 30Hz, render at 60Hz. A single render frame can run 0/1/2 sim ticks. If every action separately decides "should I send to Sim?":

- Edge events (key press) between ticks → next tick already released → Sim never sees it → **command lost**.
- Multiple events in one frame → only the last is sent → **command lost**.

**Solution**: all edge events queue; CommandBuffer consumes until empty.

### 3.2 Data structure

```gdscript
# scripts/input/input_event_queue.gd
class_name InputEventQueue
extends Node

enum EType { KEY_PRESS, KEY_RELEASE, MB_PRESS, MB_RELEASE, MOUSE_MOVE }

class Ev:
    var type: int
    var key: int        # KEY_* or MOUSE_BUTTON_*
    var pos: Vector2    # mouse world coords (for MOUSE_MOVE / MB_*)
    var t: float        # Time.get_ticks_msec() / 1000.0
    var seq: int        # monotonic

var _queue: Array[Ev] = []
var _seq := 0
var mouse_world := Vector2.ZERO      # persistent: current mouse world coords
var held_keys: Dictionary = {}       # persistent: held key → true
var held_mouse: Dictionary = {}      # persistent: held mouse button → true

func push_key_press(k: int) -> void
func push_key_release(k: int) -> void
func push_mb_press(b: int, pos: Vector2) -> void
func push_mb_release(b: int, pos: Vector2) -> void
func push_mouse_move(pos: Vector2) -> void

func pop_all() -> Array[Ev]    # pop and clear
func peek_all() -> Array[Ev]   # read-only
```

### 3.3 Wiring

- `_input(event)` or `_unhandled_input(event)`: catches `InputEventKey` / `InputEventMouseButton`, queues press/release. `InputEventMouseMotion` updates `mouse_world` (camera ray → y=0 plane) + queues `MOUSE_MOVE`.
- Persistent state (`held_keys` / `held_mouse`) is reconciled each frame with `Input.is_*_pressed` to prevent drift from focus loss.
- Mouse world projection (camera ray → y=0 plane) lives in this layer; downstream always uses `mouse_world`.

### 3.4 No-loss guarantee

- Once queued, events are only removed by `Layer 3 pop_all()`.
- Layer 3 may emit 0..N commands into Layer 4.
- Layer 4 is a cross-tick FIFO; sim_bridge drains each tick until empty (or a cap to prevent runaway).

---

## 4. Layer 2 — Input state machine (View-side FSM)

### 4.1 Two orthogonal axes

To support "enter cast mode without breaking move", use **two orthogonal axes** rather than a single FSM:

```
MoveAxis (movement axis):
  NotMoving  — no active MovePath
  Moving     — Sim-side player is self-moving (right-click pathing / basic attack chase / skill Chasing)
                  Determined by snapshot `is_moving` field (NOT MovePath.Following — see §12.1 note)

CommandAxis (command axis):
  Idle          — no pending command
  SkillAiming   — Normal cast awaiting left-click confirm (cast cursor shown)
  AttackAiming  — A / right-click enemy awaiting left-click confirm (cast cursor + attack range circle)
  CastLocked    — Sim-side CastState != None (Aiming/Chasing/Casting/Channeling/Dashing)
                  input-layer mirror; only responds to cancel/interrupt
```

**Orthogonality**:

- `Moving + SkillAiming` is valid → player presses Q while moving → enters aiming, **movement continues**.
- `Moving + CastLocked` is valid → Sim-side Chasing; player is **walking toward the target while casting**.
- `NotMoving + CastLocked` is valid → Sim-side Casting/Channeling/Dashing; player **stands still to cast**.

### 4.2 Transition tables

#### 4.2.1 MoveAxis

| Current | Event | Target | Note |
| --- | --- | --- | --- |
| NotMoving | right-click ground | Moving | emit MoveCmd |
| NotMoving | snapshot is_moving == true | Moving | Sim reverse sync |
| Moving | snapshot is_moving == false | NotMoving | Sim reverse sync (arrived / stop / cancel / target died) |
| Moving | right-click ground | Moving | re-issue MoveCmd (overwrite target) |
| Moving | S press | NotMoving (request) | emit StopCmd; Sim clears Following → next snap flips to NotMoving |

**Important**: MoveAxis **does not change** on entering SkillAiming / AttackAiming. Only snapshot `is_moving` (and the right-click-active edge) decides it.

#### 4.2.2 CommandAxis

| Current | Event | Target | Note |
| --- | --- | --- | --- |
| Idle | skill key press (normal cast) | SkillAiming | record `active_skill_slot = Q/W/E/R` |
| Idle | skill key press (quick cast) | CastLocked (pending Sim) | immediately emit `SKILL{confirm=true}` |
| Idle | A press | AttackAiming | enter basic attack aim |
| Idle | right-click + hover enemy | AttackAiming | direct lock-on (emits AttackCmd) |
| Idle | snapshot cast_state != None | CastLocked | Sim reverse sync |
| SkillAiming | left-click press | CastLocked (pending Sim) | emit `SKILL{confirm=true}` |
| SkillAiming | right-click press | Idle | emit `CANCEL{scope=skill}` (**MoveAxis unchanged**) |
| SkillAiming | S / ESC / H press | Idle | emit `CANCEL{scope=skill}` |
| SkillAiming | other skill key press | SkillAiming | switch `active_skill_slot` |
| SkillAiming | snapshot cast_state != None | CastLocked | confirmed; Sim Aiming → Chasing / Casting |
| SkillAiming | snapshot cast_error > 0 | SkillAiming (preserved!) | targeted no-target error; **keep aiming** |
| AttackAiming | left-click + hover enemy | Idle | emit `ATTACK{target_id=hover}` (MoveAxis decided by Sim) |
| AttackAiming | left-click + ground | Idle | emit `ATTACK{ground_pos}` (find nearest enemy) |
| AttackAiming | right-click / S / ESC / H | Idle | emit `CANCEL{scope=attack}` |
| AttackAiming | A release (optional) | AttackAiming | press-triggered; A release doesn't cancel by default |
| CastLocked | snapshot cast_state == None | Idle | Sim reverse sync (cast ended / interrupted / cancelled) |
| CastLocked | right-click press | CastLocked | emit `CANCEL{scope=skill}` (Sim decides if interruptible) |
| CastLocked | S / H press | CastLocked | emit `CANCEL{scope=skill}` (same) |
| CastLocked | skill key press | CastLocked | ignored (no skill switch mid-cast) |
| CastLocked | A press | CastLocked | ignored (no basic attack mid-cast) |

### 4.3 Reverse sync from Sim

Each `_process` frame reads from `last_snapshot.heroes[local_idx]` (or `players[0]` legacy fallback):

```gdscript
var snap_cast_state: int = p.cast_state   # 0=None 1=Aiming 2=Chasing 3=Casting 4=Channeling 5=Dashing
var snap_is_moving: bool = ...
var snap_attack_target: int = p.attack_target_id

# MoveAxis sync
move_axis = MoveAxis.Moving if snap_is_moving else MoveAxis.NotMoving

# CommandAxis sync
if snap_cast_state != 0:
    command_axis = CommandAxis.CastLocked
else:
    if command_axis == CommandAxis.CastLocked:
        command_axis = CommandAxis.Idle
    # SkillAiming is preserved (normal cast Aiming keeps Sim state None)
```

**Key**: `SkillAiming` is a View-side independent state. Sim-side state may still be `None` (normal cast Aiming is maintained by View until left-click confirm). Two Aiming states:

- **View Aiming (SkillAiming state)**: normal cast awaiting left-click; **Sim doesn't know**; sim_bridge keeps sending `cast_slot` + `cast_aim` per frame.
- **Sim Aiming (snapshot cast_state == Aiming)**: quick cast or post-confirm same-tick transit; **barely visible** (next tick goes to Chasing / Casting).

**Simplification**: Sim-side `Aiming` is **only a same-tick transit** for quick cast / confirm. The "await left-click" of normal cast is fully owned by View. See §7.

---

## 5. Layer 3 — Command translation (Command Builder)

### 5.1 Command types

```gdscript
# scripts/input/command.gd
class_name Command
extends RefCounted

enum CmdType { MOVE, SKILL, SKILL_UPGRADE, ATTACK, CANCEL, STOP }

var type: int
# MOVE
var move_target: Vector2
# SKILL / SKILL_UPGRADE
var skill_slot: int           # 0-3 (QWER); 10-15 reserved for equipment (P1-8)
var skill_confirm: bool       # true = confirm, false = Aiming only
var skill_aim: Vector2
var skill_target_id: int      # targeted skill NetworkId; -1 = none
# ATTACK
var attack_target_id: int     # -1 = ground
var attack_ground: Vector2
# CANCEL
var cancel_scope: int         # 0 = skill, 1 = attack, 2 = all
```

### 5.2 Translation rules (by FSM state + event)

Every frame Layer 3 takes all events from Layer 1 `pop_all()`, combines with Layer 2 state, and emits 0..N Commands into Layer 4.

| FSM state | Event | Command emitted |
| --- | --- | --- |
| Idle | right-click ground press | `MOVE{target=mouse_world}` |
| Moving | right-click ground press | `MOVE{target=mouse_world}` (overwrite) |
| Moving | right-click ground held (throttled) | `MOVE{target=mouse_world}` (6Hz) |
| Idle / Moving | S press | `STOP` |
| Idle / Moving | A press | (no command; only switch to `AttackAiming`) |
| AttackAiming | left-click + hover enemy | `ATTACK{target_id=hover}` → Idle |
| AttackAiming | left-click + ground | `ATTACK{ground=mouse_world}` → Idle |
| Idle / Moving | skill key (quick) | `SKILL{slot, confirm=true, aim, target_id}` → CastLocked |
| Idle / Moving | skill key (normal) | `SKILL{slot, confirm=false, aim, target_id}` → SkillAiming |
| SkillAiming | skill key (other slot) | `SKILL{slot, confirm=false, aim, target_id}` → SkillAiming (switch) |
| SkillAiming | left-click | `SKILL{slot, confirm=true, aim, target_id}` → Idle |
| SkillAiming | right-click / S / ESC / H | `CANCEL{scope=skill}` → Idle (**MoveAxis unchanged**) |
| SkillAiming | per frame (persistent) | `SKILL{slot, confirm=false, aim=mouse_world, target_id=hover}` (keep Sim Aiming aim updated) |
| CastLocked | right-click / S / H | `CANCEL{scope=skill}` (Sim decides interruptibility) |
| AttackAiming | right-click / S / ESC / H | `CANCEL{scope=attack}` → Idle |
| Moving | right-click enemy press | `ATTACK{target_id=hover}` (direct lock-on, no AttackAiming) |
| Idle / Moving | Ctrl + skill key | `SKILL_UPGRADE{slot}` (see §5.5) |
| CastLocked | Ctrl + skill key | ignored (no upgrade mid-cast) |

### 5.3 Quick vs Normal cast configuration

```gdscript
# scripts/input/cast_settings.gd (autoload or merged into GameSettings)
enum CastMode { NORMAL, QUICK }
var skill_cast_mode: Array[int] = [NORMAL, NORMAL, NORMAL, NORMAL]  # per slot
# or global: var global_cast_mode: CastMode = NORMAL
```

- **Configurable per slot** (e.g. Q = quick, W = normal); settable in the settings panel.
- Default all NORMAL (preserves indicator; beginner-friendly).
- Layer 3 branches by per-slot lookup.

### 5.4 Skill slot namespace and extension reservation

`skill_slot` uses a partitioned namespace to **reserve space for planned but un-implemented features**, avoiding retrofit:

| Segment | Slot value | Purpose | Status |
| --- | --- | --- | --- |
| QWER active skills | 0-3 | 4 skill slots (Q/W/E/R) | Implemented |
| Equipment active skills | 10-15 | Equipment actives (P1-8) | **Reserved** |

**Basic-attack virtual slot removed (decided)**:

- v1 used `SkillComponent.Slots[5]` as a "basic attack virtual slot" (`SkillId = 5, SkillKind::Attack`), processed by `skill_cast`.
- v2 **removes this slot**: basic attack uses the independent `ATTACK` command (§9), no `skill_slot` namespace consumption.
- `SkillComponent.Slots[5]` → `Slots[4]` (QWER only).
- `skill_defs.h` id=5 Attack row removed; `SkillKind::Attack` enum removed (basic attack no longer in `skill_cast`).
- `skill_cast.h` `cast_slot < 5` check changed to `< 4`; Attack branch deleted (v1 lines 157-182).
- `world.cpp _spawn_player` no longer initializes slot 4.
- **Future P1-8 equipment slots**: `SkillComponent` stays at `Slots[4]`; add a separate `EquipmentSkillComponent` (6 slots, slot 10-15 mapped). `get_skill_def(id)` SkillId namespace decoupled from slot: QWER = 1-4, equipment active = 100+. Layer 3 emits `SKILL{slot=10+i, ...}`; Sim-side `skill_cast` resolves slot → `EquipmentSkillComponent.Slots[i]` → SkillId.
- **This plan does not implement equipment slots**, only reserves the namespace; P1-8 can be added without touching Layers 1-4.

### 5.5 Skill upgrade command (SKILL_UPGRADE)

> **Historical note (v1)**: `SkillPoints` component and `SkillSlot::Level` field **did not exist in v1** (the `sim_system_reference.md` description was design intent, never implemented). The v2 refactor added them.

`SkillPoints` + `SkillSlot::Level` are the basis of MOBA skill-point allocation; v1 had no input path. v2 closes the gap.

**New component / field**:

```cpp
// components.h — new
struct SkillPoints {
    int Available = 0;
};

// SkillSlot gets a new field
struct SkillSlot {
    int SkillId = 0;
    int Level = 1;                 // ← new (default 1)
    float CooldownTimer = 0.0f;
    float MaxCooldown = 0.0f;
    float ManaCost = 0.0f;
};
```

**Command**:

```gdscript
# command.gd
# SKILL_UPGRADE
var skill_slot: int   # 0-3, reuses SKILL's slot field
```

**Trigger**: `Ctrl + Q/W/E/R` (edge press). Ctrl modifier chosen to avoid clashing with cast (bare Q = cast, Ctrl+Q = upgrade).

**Layer 3 translation**:

- Detect `Ctrl held + skill key press` → emit `SKILL_UPGRADE{slot=i}`; **does not enter SkillAiming**; CommandAxis unchanged.
- Ignored during CastLocked.

**Sim consumer** (new `systems/skill_level.h`):

```cpp
// systems/skill_level.h
inline void skill_level_system(entt::registry &reg) {
    auto view = reg.view<HeroTag, HeroInputState, SkillComponent, SkillPoints>();
    for (auto e : view) {
        auto &tag = view.get<HeroTag>(e);
        if (tag.IsLocal) continue;
        auto &input = view.get<HeroInputState>(e);
        auto &skills = view.get<SkillComponent>(e);
        auto &sp = view.get<SkillPoints>(e);
        if (input.SkillUpgradeSlot < 0 || input.SkillUpgradeSlot >= 4) continue;
        if (sp.Available <= 0) continue;
        auto &slot = skills.Slots[input.SkillUpgradeSlot];
        if (slot.SkillId <= 0 || slot.Level >= GameConfig::MaxSkillLevel) continue;
        slot.Level++;
        sp.Available--;
    }
}
```

**`LocalInputSingleton` new field**: `int SkillUpgradeSlot = -1;` (pulse).

**SimServer API**: `void set_skill_upgrade_command(int slot);`

**Snapshot fix**:

- `SimSkillSlotSnap.level` **semantic fix**: v1 was incorrectly filled with `char_level` in `snapshot_builder.cpp:33` (player level) — a bug; v2 fills `slot.Level` (skill level).
- `SimPlayerSnap.skill_points` **new** (added §12.1) = `SkillPoints.Available`; BottomHUD uses it for the allocatable-points prompt.

**`_spawn_player` patches**:

```cpp
_reg.emplace<SkillPoints>(e, 0);   // start at 0; progression adds on level up
for (int i = 0; i < 4; ++i) {
    sc.Slots[i].SkillId = ...;
    sc.Slots[i].Level = 1;          // ← new
    ...
}
```

**`progression_system` companion**: on level-up, `SkillPoints.Available++` (current `progression` doesn't do this — needs a follow-up; recommended to ship together).

**Not in scope**: numeric upgrade curve, per-upgrade CD/Mana refresh policy. This plan only wires "press → Sim upgrade → snapshot echo" + fixes the v1 `SimSkillSlotSnap.level` semantic bug.

---

## 6. Layer 4 — Command buffer and Sim consumption

### 6.1 GDScript queue

```gdscript
# scripts/input/command_buffer.gd
class_name CommandBuffer
extends Node

var _q: Array[Command] = []

func push(cmd: Command) -> void
func pop_all() -> Array[Command]
func empty() -> bool
func clear() -> void
```

Layer 3's commands push here; sim_bridge pops all per tick.

### 6.2 sim_bridge consumption

```gdscript
func _physics_process(delta: float) -> void:
    elapsed += delta
    while elapsed >= TICK:
        var cmds := command_buffer.pop_all()
        # Merge rules:
        #   - same-frame multiple MOVE → keep only the last
        #   - same-frame multiple SKILL on the same slot → keep the last
        #   - SKILL confirm + later SKILL no-confirm → keep confirm
        #   - CANCEL → preserve
        #   - ATTACK → keep last
        #   - STOP → preserve
        var merged := merge_commands(cmds)
        for cmd in merged:
            sim.set_command(cmd)   # unified entry; see §11
        sim.tick(TICK)
        ...
        elapsed -= TICK
```

### 6.3 No loss + no duplication

- **No loss**: event queue + command buffer persist across ticks.
- **No duplication**: edge events queue once; persistent state (held keys) doesn't generate repeated commands; only `mouse_world` updates via SkillAiming's per-frame "send aim" mechanism.
- **Throttling**: long-press right-click 6Hz, throttled at Layer 3 translation (not queued).
- **Dedup**: merge rules in §6.2 prevent repeated A* in the same tick.

---

## 7. Sim-side CastState refactor (incl. Chasing)

### 7.1 New `Phase` enum

```cpp
struct CastState {
    enum class Phase : uint8_t {
        None       = 0,
        Aiming     = 1,  // same-tick transit only (quick cast / post-confirm instant)
        Chasing    = 2,  // confirmed but out of range; A* follow
        Casting    = 3,  // cast time
        Channeling = 4,  // channel (F)
        Dashing    = 5,  // displacement (R)
    };
    Phase State = Phase::None;
    int ActiveSlot = -1;
    int SkillId = 0;
    float Timer = 0.0f;
    float SubTimer = 0.0f;
    Vec2 AimPos{0.0f};
    Vec2 DashStart{0.0f};
    Vec2 DashTarget{0.0f};
    int HitTargetId = -1;
    int CastError = 0;
    entt::entity TargetEntity = entt::null;   // targeted skill locked target
      int TargetNetworkId = -1;
      bool QuickCast = false;                   // marks cast source
      float RejectTimer = 0.0f;
      float PendingCooldown = 0.0f;
      float PendingManaCost = 0.0f;
};
```

### 7.2 State transitions (Sim authoritative)

```
Phase::None + received SKILL{confirm=true}
  ├─ SkillKind=MeleeSingle and target invalid → CastError=4, stay None (View shows "No target"; SkillAiming preserved)
  ├─ CD > 0 → CastError=1, stay None
  ├─ Mana insufficient → CastError=2, stay None
  ├─ Stun → CastError=3, stay None
  ├─ in range / no target needed → store pending mana/CD → Phase::Casting (Timer = CastTime)
  └─ out of range (MeleeSingle has target but dist > Range / non-targeted dist > Range with chase target)
       → store pending mana/CD → Phase::Chasing (Timer = 0, TargetEntity locked)

Phase::None + received SKILL{confirm=false}
  → no Aiming (Sim no longer maintains normal cast "await left-click")
  → only update ActiveSlot / AimPos / TargetEntity (for next confirm)
  → state stays None

Phase::Chasing + each tick (handled inside skill_cast; see §13 tick order)
   ├─ target dead → discard pending mana/CD + Phase::None + CastError=5
   ├─ target in range → Phase::Casting (Timer = CastTime)
   ├─ CANCEL received → discard pending mana/CD + Phase::None
   ├─ movement: this tick #3 skill_cast sets Chasing; #4 pathfinding uses A* to TargetEntity / AimPos, writes MovePath; #5 movement follows (no 1-tick delay)
   └─ non-targeted: AimPos updated each tick by input.SkillAim (follows mouse)

Phase::Casting + Timer <= 0 → commit mana/CD → trigger effect → per SkillKind transition to Channeling / Dashing / None
Phase::Casting + CANCEL + not ChannelBurst → discard pending mana/CD + None
Phase::Channeling → uninterruptible (CANCEL ignored); Timer ends → None
Phase::Dashing → position advance; on arrival / wall hit → None
```

### 7.3 Chasing rules

| Skill type | Chasing entry condition | Chasing movement | End condition |
| --- | --- | --- | --- |
| MeleeSingle (targeted) | confirm with target alive but `dist > Range` | A* to target | in Range → Casting; target dead → discard pending mana/CD + None |
| AoEField (non-targeted, ground) | confirm with `dist(AimPos, pos) > Range` | A* to AimPos | in Range → Casting |
| Dash (displacement) | no Chasing (dash is the displacement) | — | direct Casting → Dashing |
| ChannelBurst (self) | no Chasing (no range concept) | — | direct Casting → Channeling |

### 7.4 "No target" error + preserve aiming

User requirement: **targeted skill confirm with no valid target → "No target" error → keep SkillAiming, don't exit**.

Implementation:

- Sim: `Phase::None + SKILL{confirm=true}` validation failure → `CastError = 4`; state stays `None`.
- View: sees `snapshot.cast_error == 4` and `cast_state == None` → **does not exit SkillAiming** (only shows red "No target" text); player can keep aiming + left-click to retry.
- View: `CastError` is detected by `sim_bridge` via `prev_error != cur_error` to avoid duplicate toasts.
- SkillAiming is only exited by explicit user cancel (right-click / S / ESC / H) or successful confirm (→ CastLocked).

### 7.5 Unified cast interruption

No more "scattered flags":

| Sim phase | CANCEL handling | Behavior |
| --- | --- | --- |
| None | ignored | — |
| Aiming (Sim instant) | discard pending resources + None | barely happens |
| Chasing | discard pending resources + None | no mana/CD was committed |
| Casting | discard pending resources + None | no mana/CD was committed before cast completion |
| Channeling | **ignored** | F channel uninterruptible |
| Dashing | **ignored** | R displacement uninterruptible |

**Resource policy**: mana and cooldown are committed only when the cast-time timer completes and the skill effect is triggered. Cancelling during Chasing or Casting simply discards the pending resource values; there is nothing to refund.

### 7.6 Why the input-layer mirror matters

View's `CastLocked` is directly determined by `snapshot.cast_state != None`:

- Player presses H / S / right-click → Layer 3 emits `CANCEL` → Sim consumes → next snapshot `cast_state = None` → View auto-exits `CastLocked`.
- **No "View cancelled but Sim still in Casting" desync**, because View state fully follows Sim.
- Sole exception: `SkillAiming` (View-maintained normal-cast wait) has Sim state `None`; cancelling at that stage just flips View to Idle — **no CANCEL is emitted** (Sim doesn't need to know).

---

## 8. Quick Cast and Normal Cast flow

### 8.1 Quick cast

**Trigger**: skill key **press** (edge, not held).

```
Player presses Q (quick cast)
  ↓ Layer 1 queues KEY_PRESS{Q}
  ↓ Layer 3 translates
SKILL{slot=0, confirm=true, aim=mouse_world, target_id=hover}
  ↓ Layer 4 queues
  ↓ sim_bridge next tick
sim.set_skill_command(0, true, ax, ay, target_id)
  ↓ Sim skill_cast_system
  ├─ validate CD / Mana / Target / Stun
  ├─ fail → CastError, State = None (View shows error, **View returns to Idle**)
  ├─ in range → State = Casting
  └─ out of range → State = Chasing (A* follow)
  ↓ snapshot
View sees cast_state != None → CommandAxis = CastLocked
```

**Key**: quick cast has no separate aiming phase; the cast cursor appears after Sim enters CastLocked, with the cast bar during the Casting phase.

**No target behavior**: same as normal cast → error + **View returns to Idle** (quick cast has no Aiming to preserve). See §8.3.

### 8.2 Normal cast

**Trigger**: skill key **press** (edge) → enter SkillAiming (cast cursor) → left-click confirm.

```
Player presses Q (normal cast)
  ↓ Layer 1 queues KEY_PRESS{Q}
  ↓ Layer 3 translates
SKILL{slot=0, confirm=false, aim=mouse_world, target_id=hover}
  ↓ Layer 4
  ↓ Sim
Sim only updates ActiveSlot / AimPos / TargetEntity; State stays None
  ↓ snapshot (cast_state still None)
View switches CommandAxis = SkillAiming (by Layer 3 when generating the command; not waiting for snapshot)

Each frame Layer 3 keeps emitting SKILL{slot=0, confirm=false, aim=current_mouse, target_id=current_hover}
  ↓ keep Sim-side ActiveSlot / AimPos / TargetEntity in sync with mouse

Player left-click
  ↓ Layer 1 queues MB_PRESS{LEFT}
  ↓ Layer 3 translates
SKILL{slot=0, confirm=true, aim=mouse_world, target_id=hover}
  ↓ Sim
  ├─ validate CD / Mana / Target / Stun
  ├─ fail → CastError (4 = No target → View preserves SkillAiming)
  ├─ in range → State = Casting
  └─ out of range → State = Chasing
  ↓ snapshot
View sees cast_state != None → CommandAxis = CastLocked
(if CastError == 4 and State == None → View preserves SkillAiming)
```

### 8.3 Behavior comparison

| Behavior | Normal cast | Quick cast |
| --- | --- | --- |
| Trigger | press → aiming → left-click confirm | press (direct confirm) |
| Indicator (cast cursor + range circle when `cast_range > 0`) | Yes (during SkillAiming) | Yes (during CastLocked) |
| Real-time aim follow | aim updates during aiming | Only at press instant |
| Out of range | confirm → Sim Chasing → A* follow | Same |
| Targeted no target | **preserve SkillAiming**, show "No target" | **return to Idle**, show "No target" (no Aiming to preserve) |
| Cancel (aiming phase) | right-click / S / ESC / H → return to Idle (MoveAxis unchanged) | No aiming phase |
| Cancel (Chasing / Casting) | CANCEL → discard pending resources + None | Same |
| Channeling / Dashing | uninterruptible | Same |

**Design basis (per user)**: quick-cast semantics match normal-cast's left-click behavior (out-of-range uses A* chase; targeted no-target errors; preserves move). Normal cast's "preserve SkillAiming" is a quick-cast-exclusive extra behavior, since quick cast has no explicit Aiming state.

---

## 9. Basic attack command mode (independent branch)

### 9.1 Why a separate FSM

Basic attack and skills are **completely different command flows**:

- Skill: CD + Mana + CastTime + Effect trigger
- Basic attack: AttackSpeed throttle + homing arrow + auto-chase + **wall-piercing**

Forcing them to share an FSM would introduce a lot of if/else and bugs. **Independent mode is most decoupled.**

### 9.2 Entry (two ways)

| Trigger | Behavior | Left-click confirm? |
| --- | --- | --- |
| **A press** | Enter AttackAiming; await left-click | Yes (normal-attack mode) |
| **right-click enemy** | **Direct lock; no confirm needed** | No (MOBA standard: right-click enemy = attack) |
| right-click ground | Move (MoveCmd) + clear lock (AttackClear) | — |

**Key**: A-key mode is "aim + confirm" like normal cast; right-click enemy is instant attack (no AttackAiming). Both emit `ATTACK`; only the View path differs.

### 9.3 AttackAiming sub-flow

```
Player presses A
  ↓ Layer 1: KEY_PRESS{A}
  ↓ Layer 3: switch CommandAxis = AttackAiming (no command)
  ↓ View shows the cast cursor and shared attack range circle

Player left-click
  ├─ hover enemy → Layer 3 emits ATTACK{target_id=hover}
  └─ ground     → Layer 3 emits ATTACK{ground=mouse_world, target_id=-1}
  ↓ CommandAxis = Idle

Sim attack_command_system consumes ATTACK
  ├─ target_id >= 0 → resolve → set AttackTarget{Target, TargetNetworkId}
  └─ ground        → find_nearest_enemy(AcquisitionRange) → set AttackTarget or invalid
  ↓
pathfinding_system: AttackTarget valid and out of Range → A* to target; write MovePath
movement_system: follow MovePath
attack_fire_system: in Range → fire homing arrow
```

### 9.4 Projectile wall-piercing / character wall-blocking

**Rule**: projectile can pierce walls; character never can. All character movement goes through A*.

| Layer | Wall rule |
| --- | --- |
| `pathfinding` | basic attack chase uses A* (around walls), same as skill Chasing |
| `movement` | follows MovePath (A* path); does not set `Chasing` flag for right-click basic attack |
| `wall_collision` | Mover branch: skip no one (Dashing excepted) |
| `arrow_movement` | Homing arrows update velocity toward target each tick |
| `wall_collision` | Arrow branch: skip arrows with `Homing` (pierce) |
| `combat` | Homing arrows only check collision with their locked target (no incidental hits) |

**Note**: projectile pierces, character doesn't. Skill Chasing and basic attack chase both use A* (unified rule).

### 9.5 Canceling basic attack aim

| State | Cancel key | Behavior |
| --- | --- | --- |
| AttackAiming | right-click / S / ESC / H | switch to Idle, **no command** (no Sim-side AttackAiming to clear) |
| Locked (AttackTarget valid) | right-click ground / S / move command | emit `ATTACK{clear=true}` or Sim detects MoveIssue / Stop and clears in `attack_command` |

### 9.6 Basic attack vs cast mutual exclusion

- `attack_fire` and `skill_cast` both check `CastState != None` for mutual exclusion (already implemented).
- During AttackAiming, pressing a skill key → Layer 3 switches to SkillAiming (**AttackAiming auto-canceled**, no ATTACK emitted).
- During SkillAiming, pressing A → Layer 3 emits `CANCEL{scope=skill}` + switches to AttackAiming.

---

## 10. Movement and pathfinding (A* chase/follow)

### 10.1 Three movement sources

| Source | Trigger | Pathfinding | Pierces walls? |
| --- | --- | --- | --- |
| Player right-click | `MOVE` command | A* (around walls) | No |
| Skill chase-cast | Sim `Chasing` phase | A* (around walls) | No |
| Basic attack chase | `AttackTarget` valid + out of Range | A* (around walls) | No |

> Note: basic attack chase also sets `AttackTarget.Chasing = true` each tick in `movement`; this is consumed by `wall_collision` to skip the player. The actual movement is via the MovePath written by `pathfinding`.

### 10.2 `pathfinding_system` responsibilities (each tick)

1. **Sim Chasing phase**: if `CastState.State == Chasing` → A* to `CastState.TargetEntity` (MeleeSingle) or `CastState.AimPos` (AoEField); write `MovePath`. **Highest priority** (player confirmed cast, auto-follow).
2. **Basic attack chase**: if `AttackTarget.Target` valid and out of Range → A* to target; write `MovePath`. `movement` follows MovePath.
3. **Player right-click move**: `input.MoveIssue == true` and `CastState == None` and `AttackTarget.Target == null` → A* to `input.MoveTarget`; write `MovePath`.

### 10.3 `movement_system` priority

```
Each tick:
1. Status gate (Root / Stun) → continue
2. CastState gate (Casting / Channeling / Dashing) → continue
   (Chasing does NOT gate; movement is allowed)
3. Stop command → clear MovePath, continue
4. MovePath.Following → follow (A* path): skill Chasing / basic attack chase / right-click move all use this branch
```

### 10.4 Turn rate

All path-following branches use `PathTurnRate` smoothed orientation (already implemented). The straight-chase branch also smooths. **Only quick cast's instant turn does not smooth** (dash sets orientation directly via Sim).

### 10.5 Pathfinding deadzone and throttle

- `RepathTargetDeadzone` (already, 1.5 units): repeated right-click in similar area doesn't trigger A* recompute.
- During Chasing, target moves > deadzone per tick → repath.
- View-side long-press right-click 6Hz throttle `MOVE_REPEAT_INTERVAL = 0.167s`.

---

## 11. SimServer API (unified command interface)

### 11.1 Fine-grained API (current)

```cpp
// sim_server.h
void set_move_command(float target_x, float target_y, bool issue);
void set_stop_command(bool stop);
void set_skill_command(int slot, bool confirm, float aim_x, float aim_y, int target_id);
void set_skill_upgrade_command(int slot);
void set_attack_command_full(int target_id, bool ground, float gx, float gy, bool clear);
void set_cancel_command(bool skill, bool attack);
```

**Recommended fine-grained approach**: API is self-describing; CommandBuffer can dispatch directly to the matching method after merge.

### 11.2 Deprecated legacy API (kept for migration only)

| Legacy API | Replacement |
| --- | --- |
| `set_local_input(move, aim, fire, seq)` | `set_move_command` + (aim via skill) |
| `set_cast_input(slot, confirm, cancel, interrupt, aim_x, aim_y, target_id)` | `set_skill_command` + `set_cancel_command` |
| `set_attack_command(target_id)` (single-arg) | `set_attack_command_full(target_id, ground, gx, gy, clear)` (extended) |
| `set_stop(stop)` | `set_stop_command()` |

`fire` field removed (basic attack no longer fires at mouse; lock-on homing). `PlayerInputState.Fire` removed.

### 11.3 `LocalInputSingleton` (current)

```cpp
struct LocalInputSingleton {
    // ── Move ──
    Vec2 MoveTarget{0.0f};
    bool MoveIssue = false;
    bool Stop = false;

    // ── Skill ──
    int  SkillSlot = -1;        // current Aiming slot, -1=none (0-3 QWER, 10-15 equipment reserved)
    bool SkillConfirm = false;  // whether this tick confirms
    Vec2 SkillAim{0.0f};
    int  SkillTargetId = -1;
    int  SkillUpgradeSlot = -1; // upgrade pulse (Ctrl+QWER), -1=none

    // ── Cancel ──
    bool CancelSkill = false;
    bool CancelAttack = false;

    // ── Basic attack ──
    int  AttackTargetId = -1;
    bool AttackGround = false;
    Vec2 AttackGroundPos{0.0f};
    bool AttackClear = false;

    // ── Sequence ──
    int Seq = 0;
};
```

`Move` / `Aim` / `Fire` / `CastInterrupt` fields removed (replaced by command fields). `PlayerInputState` mirrors this layout.

### 11.4 World command consumption

```cpp
void World::set_skill_command(int slot, bool confirm, float ax, float ay, int target_id) {
    auto &li = _reg.get<LocalInputSingleton>(_local_input_entity);
    li.SkillSlot = slot;
    li.SkillConfirm = confirm;
    li.SkillAim = {ax, ay};
    li.SkillTargetId = target_id;
}
// ... other set_*_command follow the same pattern
```

Each tick start: `local_input_injection_system` copies to `PlayerInputState`; tick end clears pulse fields (`SkillConfirm` / `SkillUpgradeSlot` / `MoveIssue` / `Stop` / `CancelSkill` / `CancelAttack` / `AttackGround` / `AttackClear`).

---

## 12. Snapshot extensions (state echo sync)

### 12.1 `SimHeroSnap` (and `SimPlayerSnap` legacy) new fields

| Field | Type | Source | Purpose |
| --- | --- | --- | --- |
| `cast_state` | int | `CastState.Phase` | 0=None 1=Aiming 2=Chasing 3=Casting 4=Channeling 5=Dashing |
| `cast_slot` | int | `CastState.ActiveSlot` | Which slot View shows |
| `cast_progress` | float | `Timer / max` | Progress bar |
| `cast_aim_x/y` | float | `CastState.AimPos` | VFX position |
| `cast_target_id` | int | `CastState.TargetNetworkId` | View highlight follow target |
| `cast_error` | int | `CastState.CastError` | Error code |
| `dash_sx/sy/tx/ty` | float | `DashStart/Target` | dash path VFX |
| `hit_target_id` | int | `HitTargetId` | C hit VFX |
| `attack_target_id` | int | `AttackTarget.TargetNetworkId` | Red lock indicator |
| `skill_points` | int | `SkillPoints.Available` | **New** — allocatable points / upgrade prompt |
| `is_moving` | bool | derived (see note) | **New** — View MoveAxis reverse sync |
| `tier` | int | `BotTier` | Bot-specific |
| `is_local` | bool | `HeroTag.IsLocal` | Sole local player |
| `hero_def_id` | int | `HeroDefId.Value` | View prefab selection |

**`is_moving` source note**: must not simply be `MovePath.Following` — basic attack chase (§9.4) is straight-line and does not go through `MovePath`; during skill Chasing, `MovePath` is rewritten by `pathfinding` each tick. Correct source is in `player_movement_system` (or `snapshot_export`) at end of tick, based on "this tick did we actually advance position":

```cpp
// snapshot_builder.cpp _build_heroes (illustrative)
s->is_moving = (path.Following)                      // A* path-following
    || (at.Chasing)                                   // basic attack straight chase
    || (cs.State == CastState::Phase::Chasing);       // skill chase-cast
// note: Casting/Channeling/Dashing do NOT count as is_moving (stand still / dash self-managed)
```

### 12.2 View sync

```gdscript
# input_state_machine.gd
func sync_from_snapshot(p: SimHeroSnap) -> void:
    # MoveAxis
    move_axis = MoveAxis.Moving if p.is_moving else MoveAxis.NotMoving

    # CommandAxis
    if p.cast_state != 0:   # != None
        command_axis = CommandAxis.CastLocked
    else:
        if command_axis == CommandAxis.CastLocked:
            command_axis = CommandAxis.Idle
        # SkillAiming is self-maintained; do not exit here
```

### 12.3 Error display

```gdscript
# sim_bridge.gd
if prev_cast_error != p.cast_error and p.cast_error > 0:
    cast_error_layer.show_error(p.cast_error)
prev_cast_error = p.cast_error
```

Error codes: 1=On Cooldown, 2=Not enough Mana, 3=Stunned, 4=No target, 5=Target unavailable (target died after confirm).

---

## 13. Tick order

> **Important**: this section's order matches the **v4** implementation (`world.cpp::tick` with 22 systems). Earlier drafts of this doc showed 20 systems; the v4 Bot refactor added `bot_combat_state_system` (between `bot_ai_system` and `bot_skill_decider_system`). For the canonical sequence, see `docs/DATA_FLOW.md §3`.

```
# Input phase
1.  local_input_injection_system    # copy LocalInputSingleton → HeroInputState (local heroes only)

# Bot AI phase (5 systems)
2.  bot_targeting_system            # pick TargetEntity (prefer local player)
3.  bot_ai_system                   # Goal FSM + respawn roll
4.  bot_combat_state_system         # v4 combat phase FSM
5.  bot_skill_decider_system        # v4 score-based skill selection → BotCastRequest
6.  bot_input_injection_system      # BotAIState + BotCastRequest → HeroInputState

# Unified combat phase (treats all HeroTag; player or AI)
7.  attack_command_system           # ATTACK command → AttackTarget (generalized)
8.  skill_cast_system               # CastState state machine (None → Aiming / Chasing / Casting / Channeling / Dashing)
9.  pathfinding_system              # A* pathfinding → MovePath — same tick sees #8 setting Chasing
10. movement_system                 # MovePath follow + AttackTarget chase + Dashing — sets is_moving
11. attack_fire_system              # in range → fire homing arrow

# Physics
12. arrow_movement_system           # position advance + Homing tracking
13. wall_collision_system           # AABB resolve + arrow destroy; skip Chasing + Dashing + Homing
14. combat_system                   # arrow collision + damage + kill events

# Game systems
15. pickup_system                   # spawner + pickup overlap
16. aoe_system                      # AoE entity lifecycle
17. status_effect_system            # Root / Stun timer decrement
18. mana_regen_system               # Mana.Cur regen
19. skill_cooldown_system           # CooldownTimer decrement
20. skill_level_system              # consume SKILL_UPGRADE → Level++ / Available--
21. progression_system              # KillEventBuffer → XP / Level / ATK
22. snapshot_export_system          # build SimSnapshot
```

**Order constraints (no 1-tick delay design)**:

- `skill_cast` (#8) must run **before** `pathfinding` (#9) and `movement` (#10): confirm this tick sets `State = Chasing` + `TargetEntity` / `AimPos` in #8; #9 same tick sees Chasing → `nav.find_path` → MovePath; #10 same tick follows MovePath. **Confirm → A* → movement all in the same tick; no 33ms delay.**
- `movement` (#10) runs after `skill_cast` (#8): reads the latest CastState this tick (Casting / Channeling / Dashing gate immediately; effect trigger flips State to None → gate released immediately).
- `attack_command` (#7) before `skill_cast` (#8): processes ATTACK first, sets / clears AttackTarget; skill_cast then decides casting (during cast AttackCmd is queued, consumed next tick when CastState = None).
- `attack_fire` (#11) after `skill_cast` (#8): reads `CastState != None` → skip basic attack immediately.
- `wall_collision` (#13) after `movement` (#10): reads `AttackTarget.Chasing` flag to skip wall-piercing chase; reads `CastState::Dashing` to skip dash displacement.
- `combat` (#14) after `arrow_movement` (#12): Homing arrows have already tracked to the target vicinity.
- `skill_level` (#20) independent of `skill_cast`; placed near `skill_cooldown`; doesn't participate in state machine, only consumes the upgrade pulse.

---

## 14. Component change list (historical — fully implemented)

> All changes in this section were applied during the v1 → v2 refactor and are now part of the codebase. Listed here for traceability.

### 14.1 New

```cpp
struct Homing {
    entt::entity Target = entt::null;
    int TargetNetId = -1;
};

struct SkillPoints {        // ← new (v1 was missing)
    int Available = 0;
};
```

### 14.2 Modified

```cpp
struct CastState {
    enum class Phase : uint8_t {
        None = 0, Aiming = 1, Chasing = 2,  // ← added Chasing
        Casting = 3, Channeling = 4, Dashing = 5,
    };
    // ... added TargetNetworkId, QuickCast
};

struct AttackTarget {
    entt::entity Target = entt::null;
    int TargetNetworkId = -1;
    bool Chasing = false;   // set by movement; consumed by wall_collision
};

struct SkillSlot {
    int SkillId = 0;
    int Level = 1;                  // ← new (v1 was missing)
    float CooldownTimer = 0.0f;
    float MaxCooldown = 0.0f;
    float ManaCost = 0.0f;
};

struct SkillComponent {
    SkillSlot Slots[4];             // ← Slots[5] → Slots[4] (basic-attack virtual slot removed)
};

struct LocalInputSingleton {
    // fully refactored to command-style, see §11.3 (includes SkillUpgradeSlot)
};
// PlayerInputState mirrors the same
```

### 14.3 Removed

- `PlayerInputState.Move / Aim / Fire / CastInterrupt / CastSlot / CastConfirm / CastCancel / CastAim / CastTargetId` → all removed, replaced by command fields.
- `LocalInputSingleton` same.
- `SkillComponent.Slots[4]` (basic-attack virtual slot, `SkillId = 5`) → removed; `Slots[5]` shrinks to `Slots[4]`.
- `skill_defs.h` id=5 Attack row → removed.
- `SkillKind::Attack` enum → removed (basic attack no longer in `skill_cast`).
- `game_config.h` `SkillCooldowns[4]` / `SkillManaCosts[4]` / `SkillCount` → removed (unified in `skill_defs.h`, which itself is now removed in favor of `SkillRegistry`).
- `ArrowTag.LifestealRatio` **preserved** (F ultimate lifesteal still needs it).

---

## 15. File change list (historical)

> All changes below were applied during the v1 → v2 refactor. Listed for traceability.

### 15.1 GDScript — new

| File | Responsibility |
| --- | --- |
| `scripts/input/input_event_queue.gd` | Layer 1: raw event queue + persistent state |
| `scripts/input/input_state_machine.gd` | Layer 2: dual-axis FSM + snapshot reverse sync |
| `scripts/input/command.gd` | Command data class |
| `scripts/input/command_builder.gd` | Layer 3: FSM + events → Command |
| `scripts/input/command_buffer.gd` | Layer 4: cross-tick FIFO |
| `scripts/input/cast_settings.gd` | Quick / Normal cast preferences (per-slot) |

### 15.2 GDScript — modified

| File | Change |
| --- | --- |
| `scripts/input/input_collector.gd` | **Fully rewritten** as the composite of Layers 1-4 (or split into the files above) |
| `scripts/sim_bridge.gd` | Pop from `command_buffer` → call `set_*_command`; remove old `set_local_input` / `set_cast_input` / `set_attack_command` calls |
| `scripts/autoload/game_settings.gd` | Remove `move_mode` / `MoveMode` / `mode_changed` (deprecated); keep camera / fullscreen config |
| `scripts/ui/settings_panel.gd/.tscn` | Remove mode switch OptionButton; add per-slot cast mode preference |
| `scripts/ui/bottom_hud.gd` | Remove per-mode `KEY_HINTS` switch; fixed QWER + A |
| `scripts/view/skill_vfx.gd` | Shared cast/attack range indicator, dash path, and AoE visuals; targeting modes use the cast cursor |
| `resources/ui/cursors/*.png` | Normal and cast-mode mouse cursor textures |
| `scripts/view/entity_view.gd` | `attack_targeted` red indicator (already present; preserved) |

### 15.3 C++ — modified

| File | Change |
| --- | --- |
| `components.h` | `CastState` + Chasing / TargetNetworkId / QuickCast; `AttackTarget` + Chasing; **new `SkillPoints`**; `SkillSlot` + `Level`; `SkillComponent.Slots[5]` → `Slots[4]`; `LocalInputSingleton` / `PlayerInputState` full refactor; remove Move / Aim / Fire / CastInterrupt; **new `Homing`** |
| `game_config.h` | + `SkillChaseRepathDeadzone` / `MaxSkillLevel`(=4) |
| `skill_defs.h` | **Deleted** (replaced by `SkillRegistry` + `ISkill`) |
| `systems/local_input_injection.h` | Copy new command fields (including `SkillUpgradeSlot`) |
| `systems/pathfinding.h` | + Chasing branch (A* to TargetEntity / AimPos); basic-attack chase is straight-line (set in movement) |
| `systems/movement.h` | + Chasing phase movement (uses MovePath); + AttackTarget straight chase + sets `Chasing=true`; removed WASD; **sets `is_moving` at end** for snapshot |
| `systems/attack_command.h` | Consumes `ATTACK` command; clear / ground / target_id |
| `systems/attack_fire.h` | Preserved (homing arrow logic) |
| `systems/skill_cast.h` | **Chasing branch added internally** (no 1-tick delay): None+confirm out of range → Chasing; Chasing+in range → Casting; Chasing+target dead → discard pending resources + None; Chasing+CANCEL → discard pending resources + None; commit mana/CD only after Casting completes; `cast_slot<4`; Attack branch removed |
| `systems/skill_level.h` | **New**: consumes `SkillUpgradeSlot` → `SkillPoints.Available--` + `slot.Level++` |
| `systems/wall_collision.h` | + Skip `AttackTarget.Chasing` players (wall-piercing chase) + skip Homing arrows; preserves existing Dashing skip |
| `systems/combat.h` | + Homing only hits locked target |
| `systems/arrow_movement.h` | + Homing tracking |
| `arrow_spawner.h` | `ArrowSpawnContext` +homing field |
| `world.h/.cpp` | tick order (§13); `set_*_command` impls (incl. `set_skill_upgrade_command`); `_spawn_player` emplaces `SkillPoints{0}` + `slot.Level=1` + removes slot 4 init; remove `set_local_input` / `set_cast_input` / `set_skill_input` |
| `sim_server.h/.cpp` | + `set_move_command` / `set_skill_command` / `set_skill_upgrade_command` / `set_attack_command_full` / `set_cancel_command` / `set_stop_command`; remove legacy API |
| `register_types.cpp` | Bind new API |
| `snapshot_types/` | `SimHeroSnap` + `is_moving` / `cast_target_id` / `skill_points` / `is_local` / `hero_def_id` / `tier` |
| `snapshot_bindings.cpp` | Register new fields |
| `snapshot_builder.cpp` | Populate new fields (`is_moving` source: see §12.1 note); **fix `SimSkillSlotSnap.level` semantic bug** (`char_level` → `slot.Level`) |

### 15.4 C++ — deleted

| File | Reason |
| --- | --- |
| `src_cpp/sim/systems/skill_input.h` | Replaced by `skill_cast` (if still present) |
| `src_cpp/sim/systems/player_fire.h` | Replaced by `attack_fire` (if still present) |

### 15.5 Scene

| File | Change |
| --- | --- |
| `scenes/main.tscn` | Split InputCollector into InputEventQueue / InputStateMachine / CommandBuilder / CommandBuffer subnodes (or keep one node with composite script) |
| `scenes/ui/settings_panel.tscn` | Remove mode OptionButton; add cast mode preference |

---

## 16. Implementation phases (historical — all complete)

> All phases below were completed during the v1 → v2 refactor. Documented for traceability.

### Phase A — Layers 1+2+3+4 (View side, no Sim changes)

| Step | File | Note |
| --- | --- | --- |
| A1 | `input_event_queue.gd` | Event queue + persistent state |
| A2 | `command.gd` | Command data class |
| A3 | `input_state_machine.gd` | Dual-axis FSM (Idle / Moving / SkillAiming / AttackAiming / CastLocked) |
| A4 | `command_builder.gd` | FSM + events → Command (quick / normal cast branch) |
| A5 | `command_buffer.gd` | FIFO |
| A6 | `cast_settings.gd` | per-slot preference |
| A7 | `sim_bridge.gd` temporary | `command_buffer.pop` → still calls legacy `set_*_input` (compat) |

**Acceptance**: builds; event queue loses no commands (100 Q presses → 100 skill commands to Sim).

### Phase B — Sim command API refactor

| Step | File | Note |
| --- | --- | --- |
| B1 | `components.h` | `LocalInputSingleton` / `PlayerInputState` refactor (incl. `SkillUpgradeSlot`) |
| B2 | `sim_server.h/.cpp` + `world.h/.cpp` | `set_*_command` series (incl. `set_skill_upgrade_command`) |
| B3 | `local_input_injection.h` | Copy new fields |
| B4 | `sim_bridge.gd` | Switch to `set_*_command` |
| B5 | Remove legacy `set_local_input` / `set_cast_input` / `set_skill_input` | — |

**Acceptance**: builds; old `input_collector` deleted; new pipeline works.

### Phase C — Sim CastState + Chasing

| Step | File | Note |
| --- | --- | --- |
| C1 | `components.h` CastState | + Chasing / TargetNetworkId / QuickCast |
| C2 | `skill_cast.h` | **Add Chasing branch internally** (no system split): None+confirm out of range → Chasing; Chasing+in range → Casting; Chasing+target dead → discard pending resources + None; Chasing+CANCEL → discard pending resources + None; commit mana/CD only after Casting completes |
| C3 | `world.cpp` | **Tick order adjustment**: `skill_cast` moved before `pathfinding` (§13), same-tick A* + movement |
| C4 | `pathfinding.h` | + Chasing branch A* (same-tick sees `skill_cast` Chasing) |
| C5 | `movement.h` | + Chasing movement (uses MovePath); remove WASD; set `is_moving` at end |
| C6 | `snapshot_types.h` / `bindings` / `builder` | + `cast_state` (incl. Chasing) / `cast_target_id` / `is_moving` / `skill_points` |

**Acceptance (key: no 1-tick delay)**:

- Q (MeleeSingle) confirm out of Range → **same tick** Sim enters Chasing + `pathfinding` A* + `movement` follow → subsequent ticks in Range → Casting → effect.
- Pressing right-click during Chasing → discard pending resources + None.
- Target dies during Chasing → discard pending resources + None + CastError = 5.

### Phase D — Quick Cast / Normal Cast dual mode

| Step | File | Note |
| --- | --- | --- |
| D1 | `command_builder.gd` | Branch by `cast_settings` |
| D2 | `skill_vfx.gd` | Green line only in normal-cast SkillAiming |
| D3 | `cast_settings.gd` + settings panel | Player can switch per-slot |
| D4 | No-target error + preserve SkillAiming (normal cast) | `sim_bridge` detects `cast_error=4` → does not exit SkillAiming |

**Acceptance**:

- Normal cast Q → cast cursor → left-click → in range → Casting → hit.
- Normal cast Q → cast cursor → left-click → out of Range → Chasing → catch up → Casting.
- Normal cast Q → cast cursor → left-click on ground (MeleeSingle) → "No target" red text; **cast cursor preserved**.
- Quick cast Q → CastLocked cursor → direct Casting.
- Quick cast Q out of Range → Chasing → catch up → Casting.

### Phase E — Basic attack command mode

| Step | File | Note |
| --- | --- | --- |
| E1 | `components.h` `AttackTarget` + Chasing; + `Homing` component | — |
| E2 | `attack_command.h` | Consumes ATTACK (target / ground / clear) |
| E3 | `pathfinding.h` | Basic-attack chase **uses A***, sets `MovePath`; `movement` follows |
| E4 | `movement.h` | + basic-attack chase branch + sets `Chasing = true` |
| E5 | `attack_fire.h` | homing arrow (preserved) |
| E6 | `wall_collision.h` | skip Chasing players + Homing arrows |
| E7 | `combat.h` + `arrow_movement.h` | Homing hit + tracking |
| E8 | `command_builder.gd` | A → AttackAiming; right-click enemy → direct AttackCmd |
| E9 | `entity_view.gd` | Red lock indicator (preserved) |

**Acceptance**:

- A → left-click bot → lock → straight wall-piercing chase → in Range → homing arrow hits.
- Right-click bot → direct lock (no A) → same.
- Right-click ground → move + clear lock.
- A → right-click → cancel AttackAiming.

### Phase E2 — Skill upgrade link (SKILL_UPGRADE)

> Independent of D / E; closes the v1 SkillPoints input gap + SimSkillSlotSnap.level semantic bug.

| Step | File | Note |
| --- | --- | --- |
| E2-1 | `components.h` | **New `SkillPoints` component**; `SkillSlot` + `Level`; `LocalInputSingleton` / `PlayerInputState` + `SkillUpgradeSlot` |
| E2-2 | `game_config.h` | + `MaxSkillLevel` (= 4) |
| E2-3 | `systems/skill_level.h` **new** | consume `SkillUpgradeSlot`; validate `SkillPoints.Available>0 && slot.Level<Max` → `Level++ / Available--` |
| E2-4 | `world.cpp` | insert `skill_level_system` in tick order (§13 #20, after `skill_cooldown`); `_spawn_player` emplaces `SkillPoints{0}` + `slot.Level=1` |
| E2-5 | `progression.h` | level-up: `SkillPoints.Available++` (current progression doesn't; add) |
| E2-6 | `sim_server.h/.cpp` + `register_types.cpp` | + `set_skill_upgrade_command` binding |
| E2-7 | `command_builder.gd` | Ctrl + Q/W/E/R press → `SKILL_UPGRADE{slot}` |
| E2-8 | `snapshot_types.h` / `bindings` / `builder` | `SimPlayerSnap` + `skill_points`; **fix `SimSkillSlotSnap.level` semantic bug** |
| E2-9 | `bottom_hud.gd` | show allocatable-points prompt (highlight skill slot when `skill_points>0`) |

**Acceptance**:

- After level-up → kill bot → XP → level up → `SkillPoints.Available++` → Ctrl+Q → Q skill Level++ → snapshot echo → HUD updates.
- No points: Ctrl+Q ignored.
- Max level: Ctrl+Q ignored.
- Mid-cast: Ctrl+Q ignored (CommandAxis = CastLocked).

### Phase F — Polish and regression

| # | Item | Method |
| --- | --- | --- |
| 1 | Resource timing | test that Casting cancellation leaves mana and cooldown unchanged |
| 2 | Chasing repath deadzone | prevent target jitter triggering per-tick repath |
| 3 | A-key hold vs press | press-triggered vs hold behavior |
| 4 | ESC behavior | mid-cast ESC = cancel; not-casting ESC = settings panel |
| 5 | S-key semantics | Stop (stop moving) + Cancel (cancel cast / basic attack aim) dual role |
| 6 | Cross-tick command merge | correct merge of same-frame multiple MOVE / SKILL |
| 7 | Focus recovery | `held_keys` reconciled after window focus loss |

---

## 17. Edge cases and pitfalls

### 17.1 Cross-tick command loss

- **Scenario**: 60Hz render frame presses Q + left-click (same frame); Sim tick 30Hz, runs next tick.
- **Risk**: if Q press and left-click press queue in the same frame → Layer 3 generates two SKILL (confirm=false + confirm=true) → Layer 4 merges → Sim receives confirm=true one → normal.
- **Risk**: if left-click happens on the next render frame (still within the same Sim tick) → same merge → normal.
- **Risk**: if left-click happens on the next Sim tick → two SKILL on different ticks → Sim first tick receives confirm=false (enters SkillAiming but Sim doesn't maintain it) → second tick receives confirm=true → normal.
- **Guarantee**: CommandBuffer persists across ticks, no loss.

### 17.2 Sim-side `Aiming` is instantaneous

- v2 Sim-side `Aiming` is **only same-tick transit** for quick cast / post-confirm.
- **Normal cast "await left-click" is owned by View**; Sim state stays None.
- This avoids v1's "Sim Aiming vs View Aiming dual source of truth" desync bug.

### 17.3 State preservation after no-target error

- During View `SkillAiming`, see `cast_error=4` and `cast_state=None` → **do not exit SkillAiming**.
- Implementation: `sim_bridge` detects this condition and **does not trigger** the "CastLocked → Idle" transition; only shows the error.
- Player keeps left-clicking to retry → new `SKILL{confirm=true}` → Sim validates again.
- Player presses right-click / S / ESC / H → Layer 3 emits `CANCEL{scope=skill}` → switches to Idle.

### 17.4 Movement continues when entering cast

- Player right-click moving, presses Q (normal cast) → MoveAxis stays Moving; CommandAxis switches to SkillAiming.
- Sim-side: `MovePath.Following` preserved (`is_moving=true`); `pathfinding` sees `CastState==None` and does not clear Following → player keeps walking.
- Player left-click confirms → Sim enters Chasing / Casting:
  - Chasing: `pathfinding` switches to A* toward TargetEntity / AimPos (overwrites MovePath) → player turns and walks to cast target.
  - Casting: `movement` gates → player stops for cast time.
- **Key**: MoveAxis is determined only by snapshot `is_moving`; not affected by CommandAxis.

### 17.5 S-key dual semantics

- Not casting / not aiming: S = Stop (clear MovePath, stop).
- SkillAiming: S = Cancel skill (return to Idle).
- AttackAiming: S = Cancel attack (return to Idle).
- CastLocked (Chasing / Casting): S = Cancel skill (discard pending resources + None).
- CastLocked (Channeling / Dashing): S ignored.
- **Layer 3 branches by CommandAxis to decide S's semantics**; not decided downstream in Sim.

### 17.6 ESC-key triple semantics

- SkillAiming: ESC = Cancel skill.
- AttackAiming: ESC = Cancel attack.
- CastLocked: ESC = Cancel skill (if interruptible).
- Idle / Moving: ESC = open / close settings panel (`settings_panel._unhandled_input` takes over).
- **Priority**: input_event_queue queues first, Layer 3 consumes ESC; if CommandAxis != Idle → emits CANCEL, **accepts event** to prevent settings_panel from receiving it; if CommandAxis == Idle → does not consume, event continues to settings_panel.

### 17.7 Resource timing policy

| Phase | Default | Note |
| --- | --- | --- |
| Chasing cancel | no mana/CD change | No effect was triggered |
| Casting cancel | **no mana/CD change** | Resource commit is deferred until cast completion |
| Channeling cancel | uninterruptible | — |
| Dashing cancel | uninterruptible | — |
| No target (MeleeSingle) | no mana / no CD | Validation failed, never entered Chasing / Casting |
| Target dead during Chasing | no mana/CD change | Not the player's fault; no effect was triggered |

### 17.8 Homing arrows and walls

- Homing arrows pierce walls (wall_collision skip).
- Homing arrows only hit their locked target (combat filter).
- Target dies → Homing arrow keeps current velocity straight → Lifetime expires → destroyed.

### 17.9 Basic attack chase (wall-piercing) vs skill Chasing (A*)

- **Basic attack chase**: straight movement + `Chasing=true` + wall_collision skip = **wall-piercing**.
- **Skill Chasing**: A* path + MovePath follow = **around walls**.
- The user explicitly required these to be different; **do not unify**.

### 17.10 Focus loss and held_keys drift

- When Godot window loses focus, release events may not fire → `held_keys` residue.
- Layer 1 reconciles each frame with `Input.is_key_pressed` to clear drift.
- No new press events during focus loss (only reconciliation).

---

## 18. Clarifications on Bot units

### 18.1 Current state (post v4)

- Bot and Player are **unified Hero units**:
  - Player: `HeroTag{IsLocal=true}` + `HeroInputState` + `CastState` + `AttackTarget` + `SkillComponent` (4 slots)
  - Bot: `HeroTag{IsLocal=false}` + `HeroInputState` (filled by `bot_input_injection`) + `CastState` + `AttackTarget` + `SkillComponent`
- Bot's "attack" is `bot_input_injection_system` writing `AttackTargetId` → `attack_fire_system` fires a Homing arrow at the target — **a real basic attack**, not a placeholder.
- Bot uses skills: `bot_skill_decider_system` writes `BotCastRequest` → `bot_input_injection_system` translates to `HeroInputState.SkillSlot + SkillConfirm + SkillAim + SkillTargetId` → `skill_cast_system` dispatches the same `ISkill` instances as the player.
- Bot goes through `pathfinding` + `movement` + `attack_fire` (generalized for all HeroTag).

### 18.2 Constraints

- **Don't** add Bot-specific special cases in `input_controller` to accommodate current Bot behavior.
- **Don't** assume Bots won't use skills or basic-attack chase.
- Sim-side `skill_cast` / `attack_command` are still filtered by `HeroTag.IsLocal` in some places; for Bot use the generalized `HeroTag` view (without `IsLocal` filter) — this is already done in the v3 refactor.
- `input_controller` states and commands are all "local player" concepts, orthogonal to the Sim's entity-type layer.

### 18.3 For more on Bot AI

See `bot_ai.md` for the three-layer state machine (Goal / Combat / Skill) and scoring-based skill selection.

---

## 19. Summary

Key improvements of this design:

1. **Four-layer separation**: event queue / state machine / command translation / command buffer; each layer has a single responsibility.
2. **Two orthogonal state axes**: MoveAxis × CommandAxis; supports "cast mode does not break movement".
3. **Sim-side Chasing phase**: authoritative chase-cast; View does not participate in pathfinding logic; **Chasing branch added inside `skill_cast` with tick order `skill_cast` before `pathfinding`; same-tick A* + movement, no 1-tick delay**.
4. **Quick / Normal cast dual mode**: per-slot configurable; consistent behavior (left-click confirm semantics).
5. **Independent basic-attack mode**: AttackAiming + homing arrow + straight wall-piercing chase (distinct from skill Chasing's A*); decoupled from skills; **basic-attack virtual slot removed**.
6. **No command loss**: CommandBuffer persists across ticks; handles 30Hz vs 60Hz difference uniformly.
7. **State mirror**: View's `CastLocked` is fully determined by snapshot; cast interruption is a state transition, not a flag.
8. **WASD mode completely removed**: single MOBA mode; docs and code consistent.
9. **Skill upgrade link filled**: new `SkillPoints` + `SkillSlot::Level` + `skill_level.h` + `SKILL_UPGRADE` command; fixes the v1 missing input path + `SimSkillSlotSnap.level` semantic bug.
10. **Deferred cast resources**: mana and cooldown are committed only when the cast-time timer completes; Chasing/Casting cancellation leaves both unchanged.

The original implementation followed phases A → F + E2 in order. The biggest risk was in phase C (Sim-side Chasing tick order — `skill_cast` must precede `pathfinding` to avoid the 1-tick delay) and phase D (preserve SkillAiming after no-target error). Each phase was validated independently before merging.
