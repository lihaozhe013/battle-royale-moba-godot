# Project Design Document

> Last updated: 2026-08-02
> **For project state and current feature status, see `CONTEXT.md`.**
> **For component / system / API reference, see `Docs/Reference/sim_api_reference.md`.**
> **For end-to-end data flow, see `docs/DATA_FLOW.md`.**
> **This document is the high-level gameplay vision and design rationale.** Sections labeled "implemented" describe the current state; sections labeled "planned" describe the future roadmap.

---

## Table of Contents

1. [Project overview](#1-project-overview)
2. [Overall architecture](#2-overall-architecture)
3. [Health bar system design](#3-health-bar-system-design)
4. [API contract (Health bar)](#4-api-contract-health-bar)
5. [File structure](#5-file-structure)
6. [HealthBarUI scene layout](#6-healthbarui-scene-layout)
7. [GDScript constraints (must follow)](#7-gdscript-constraints-must-follow)
8. [MOBA battle royale upgrade plan](#8-moba-battle-royale-upgrade-plan)
9. [Implementation priority and effort](#9-implementation-priority-and-effort)
10. [Biggest architectural challenge](#10-biggest-architectural-challenge)

---

## 1. Project overview

- **Type**: Battle royale MOBA (current non-targeted shooting is a placeholder; expansion per §8 "MOBA upgrade plan")
- **Engine**: Godot 4.7, C++ GDExtension (Sim layer) + GDScript (View layer)
- **Architecture**: ECS — C++ Sim (`entt::registry`) → `SimSnapshot` → GDScript View Systems
- **Camera**: 55° top-down perspective, FOV=40, follow player
- **Tick rate**: 30Hz Sim, 60Hz View interpolation
- **Input mode**: **MOBA only** (right-click ground + Q/W/E/R + A basic attack). The previous WASD mode was completely removed.

---

## 2. Overall architecture

```
C++ Sim (entt::registry + 22 systems, 30Hz)
  ↓ SimSnapshot (RefCounted, 8 snap types: heroes / players / bots / arrows / pickups / aoes / events)
GDScript View (60Hz)
  ├─ EntityManager.sync_entities(snap) → 3D entity spawn/despawn + position interpolation
  ├─ HealthBarManager.sync_bars(snap) → 2D bar data update
  ├─ BottomHUD / CastBar / SkillVFX / CameraController / HealthBarManager
  └─ sim_bridge.gd coordinates the 30Hz ↔ 60Hz boundary
```

### ECS alignment

| ECS concept | Project correspondence |
| --- | --- |
| World | C++ `entt::registry` |
| Systems | C++ 22 systems (movement, combat, pathfinding, skill cast, …) |
| Components | C++ ~30 ECS components (see `sim_api_reference.md §2`) |
| Component data | `SimSnapshot` (serialized to GDScript) |
| View Systems | GDScript: `EntityManager`, `HealthBarManager`, `BottomHUD` |
| View Entities | `EntityView` (3D), `HealthBarUI` (2D) |

### Data flow

```
SimSnapshot
  ├→ EntityManager → EntityView       (3D: position, rotation, skeletal animation)
  ├→ HealthBarManager → HealthBarUI   (2D: bar fill, color, screen position)
  └→ BottomHUD / CastBar / SkillVFX   (HUD: level, kills, XP, skills, cast bar, aim VFX)
```

For the full sequence diagram, see `docs/DATA_FLOW.md §5`.

---

## 3. Health bar system design

> **Status: implemented.** The HealthBarManager and HealthBarUI are wired into `main.tscn` and driven by `sim_bridge.gd`. Description below explains the design rationale.

### Design decision: 2D screen-space overlay

**Chosen 2D screen-space bars**, not `Sprite3D` world-space bars.

| Dimension | 2D screen-space ✅ | `Sprite3D` world-space |
| --- | --- | --- |
| MOBA standard | LoL / DOTA / HotS use this | Used in ARPGs (Diablo / PoE) |
| Pixel clarity | Pixel-perfect, not affected by 3D resolution | Affected by `pixel_size` and camera distance |
| Segment lines / icons | Easy with Control + `_draw()` | Needs shader or extra texture |
| Visible through walls | Naturally above 3D (MOBA hard requirement) | Needs `no_depth_test` |
| Position update | Needs `unproject_position` per frame | Auto-follows parent node |
| Extensibility | Add Mana / Shield / status icons as child nodes | Each feature needs a new `Sprite3D` + material |

MOBA bars must be always visible; "visible through walls" is a feature, not a bug.

### Component architecture

```
HealthBarManager (Node, child of main.tscn)
└── CanvasLayer (layer=10, created in _ready)
    └── (HealthBarUI instance pool — dynamic add_child / recycle)

HealthBarUI (Control, health_bar_ui.tscn prefab)
├── Background (ColorRect, FULL_RECT anchor, dark base)
├── DamageBar (ColorRect, TOP_LEFT anchor, yellow delayed bar)
└── Fill       (ColorRect, TOP_LEFT anchor, team color main bar)
```

### HealthBarManager (View system)

**Responsibilities**:

1. Manage the HealthBarUI pool (create / recycle / reuse)
2. Read HP / team from snapshot, update HealthBarUI
3. Query EntityManager each frame for entity interpolated position, project to screen, position HealthBarUI

**Two update paths (separation of concerns)**:

| Path | Frequency | Source | Responsibility |
| --- | --- | --- | --- |
| `sync_bars(snap)` | 30Hz (only when new snapshot arrives) | SimSnapshot | HP data, team, visibility |
| `_process(delta)` | 60Hz (every frame) | EntityManager → `EntityView.global_position` | Screen projection |

**Position query path**:

```
HealthBarManager._process
  → EntityManager.get_entity(id)                        // lookup
  → EntityView.global_position + Vector3(0, 2.0, 0)     // head offset
  → Camera3D.unproject_position(world_pos)               // 3D → 2D projection
  → HealthBarUI.set_screen_position(screen_pos)          // 2D positioning
```

**Why query EntityView, not snapshot position directly?**

- EntityView does client-side interpolation (60Hz lerp); the 3D model position is smooth.
- If the bar used the raw snapshot position (30Hz), it would visually desync from the 3D model (jitter).
- Querying `EntityView.global_position` guarantees the bar stays perfectly synced with the 3D model.

### HealthBarUI (View component)

**Responsibility**: pure presentation — receives data and renders, no game logic.

| Method | Param | Description |
| --- | --- | --- |
| `update_hp` | `hp: int, max_hp: int` | Update Fill width + color |
| `set_team` | `team: int` | Set team color (0=self green, 2=enemy red) |
| `set_screen_position` | `pos: Vector2` | Set 2D screen position (center-aligned) |
| `reset` | — | Reset before recycling back to pool |
| `_process` | `delta: float` | DamageBar lerp chases Fill |

**HealthBarUI decoupling boundaries (what it does NOT know)**:

- Does not know about Sim / Snapshot / EntityManager
- Does not know 3D world coords / camera
- Only receives: `hp`, `max_hp`, `team`, `screen_position`

### Object pool strategy

```
Active entity appears in snapshot → _get_or_create(id)
  ├─ pool has free HealthBarUI → pull it out, visible = true
  └─ pool empty → instantiate health_bar_ui.tscn, add_child to CanvasLayer

Entity disappears from snapshot → _release_bar(id)
  └─ visible = false, return to pool

Entity dead (dead = true) → hide but keep in _active
Entity respawns → set visible = true
```

- Pool only grows, never shrinks (peak held forever).
- Avoids frequent Control node create/destroy.

### Team color system

| Team | Team ID | Fill color | Current mapping |
| --- | --- | --- | --- |
| Self | 0 | `Color(0.2, 1.0, 0.2)` bright green | `is_local == true` Hero |
| Ally | 1 | `Color(0.2, 0.6, 1.0)` blue | reserved for future (multi-player) |
| Enemy | 2 | `Color(1.0, 0.3, 0.3)` red | `is_local == false` Hero / Bot |
| Neutral | 3 | `Color(1.0, 0.8, 0.2)` yellow | reserved for future (neutrals) |

**HP color gradient (MOBA convention)**:

- **Self / ally**: >60% team color → 25-60% yellow → <25% red
- **Enemy**: fixed red, no gradient (enemy bars always red for easy identification)

### DamageBar delayed animation

When HP drops, Fill shrinks immediately, DamageBar (yellow) uses `move_toward` to slowly catch up to Fill, creating a "delayed drop" visual.

```
Fill:     jumps to new ratio immediately
DamageBar: move_toward(_damage_ratio, _hp_ratio, SPEED * delta)
```

---

## 4. API contract (Health bar)

### HealthBarManager

```gdscript
class_name HealthBarManager
extends Node

# injected by sim_bridge in _ready
var entity_manager: EntityManager
var health_bar_scene: PackedScene  # preload("res://scenes/ui/health_bar_ui.tscn")

# called by sim_bridge when new snapshot arrives (30Hz)
func sync_bars(snap: SimSnapshot) -> void

# auto-called (60Hz) — update all active bar screen positions
func _process(delta: float) -> void
```

### HealthBarUI

```gdscript
class_name HealthBarUI
extends Control

const BAR_WIDTH := 100.0
const BAR_HEIGHT := 10.0
const DAMAGE_LERP_SPEED := 3.0

# called by HealthBarManager
func update_hp(hp: int, max_hp: int) -> void
func set_team(team: int) -> void
func set_screen_position(screen_pos: Vector2) -> void
```

---

## 5. File structure

```
scripts/
├── sim_bridge.gd           — Sim ↔ View bridge (30Hz ↔ 60Hz boundary)
├── autoload/
│   └── game_settings.gd    — ConfigFile-backed settings (camera, fullscreen, smooth_pan, edge_pan, edge_pan_speed)
├── input/                  — four-layer input framework (see input_system_design.md)
│   ├── input_event_queue.gd
│   ├── input_state_machine.gd
│   ├── command_builder.gd
│   ├── command_buffer.gd
│   ├── command.gd
│   └── cast_settings.gd
├── view/
│   ├── entity_manager.gd   — 3D entity pool + LERP
│   ├── entity_view.gd      — single 3D entity
│   ├── camera_controller.gd — follow / free / edge-pan / smooth-pan
│   ├── skill_vfx.gd        — dash path, AoE
│   ├── skill_vfx_attachment.gd
│   └── move_target_vfx.gd
└── ui/
    ├── bottom_hud.gd       — level/XP/HP/Mana/4 skills/items
    ├── health_bar_manager.gd
    ├── health_bar_ui.gd
    ├── skill_slot_ui.gd
    ├── item_slot_ui.gd
    ├── cast_bar.gd         — cast progress bar
    ├── cast_error.gd       — error toast (No target, etc.)
    └── settings_panel.gd   — ESC settings (camera / fullscreen / etc.)

scenes/
├── main.tscn
└── ui/
    ├── bottom_hud.tscn
    ├── health_bar_ui.tscn
    ├── skill_slot_ui.tscn
    ├── item_slot_ui.tscn
    ├── cast_bar.tscn
    ├── cast_error.tscn
    └── settings_panel.tscn

src_cpp/sim/                — C++ Sim (22 systems, see sim_api_reference.md)
src_cpp/sim_server.h/.cpp    — SimServer GDExtension binding
data/
├── stats.yaml               — runtime balance values
└── maps/default.json        — map definition
```

---

## 6. HealthBarUI scene layout

```
HealthBarUI (Control, 100×10)
├── Background (ColorRect, FULL_RECT, dark base)
├── DamageBar   (ColorRect, TOP_LEFT, yellow delayed)
└── Fill        (ColorRect, TOP_LEFT, team-color main bar)
```

**Fill / DamageBar width set by script**:

```gdscript
_fill.size = Vector2(BAR_WIDTH * _hp_ratio, BAR_HEIGHT)
```

TOP_LEFT anchor pins the left edge; shrinking width shortens from the right.

---

## 7. GDScript constraints (must follow)

In Godot 4, `Vector2` / `Vector3` / `Rect2` properties return **copies**; sub-field assignment does not take effect:

```
# ❌ FORBIDDEN
node.scale.x = val
node.position.x = val
control.size.x = val
sprite.region_rect.size.x = val
global_rotation.y = val

# ✅ REQUIRED (assign whole struct)
node.scale = Vector3(x, y, z)
node.position = Vector3(x, y, z)
control.size = Vector2(x, y)
sprite.region_rect = Rect2(x, y, w, h)
rotation = Vector3(x, y, z)
# or use look_at() to set rotation
```

---

## 8. MOBA battle royale upgrade plan

> **Status legend**: ✅ implemented · 🟡 in progress · ❌ not started
> Items marked ✅ track the state in `CONTEXT.md`.

### 8.1 Capability inventory (current state)

| System | Implemented | Note |
| --- | --- | --- |
| Right-click ground + mouse aim | ✅ | Single MOBA mode; aim is projected to ground plane |
| Basic attack (A / right-click enemy) | ✅ | Homing arrow; wall-piercing chase |
| HP system + bar | ✅ | Bar above each entity; team color |
| Mana system | ✅ | Regen with delay after cast |
| Level / XP / progression | ✅ | Kill XP, per-level stat growth, `SkillPoints` for skill upgrade |
| Skill system (Q/W/E/R + cast error) | ✅ | 4 skills via `ISkill` interface; quick/normal cast; aim VFX; cast bar |
| 4-skill basic framework | ✅ | Q/W/E/R; per-slot level up via Ctrl+Q/W/E/R |
| Cast indicator (cursor / dash path / AoE) | ✅ | `sim_bridge.gd` + `skill_vfx.gd` + `skill_vfx_attachment.gd` |
| Basic attack indicator (lock + chase) | ✅ | Red lock indicator on `attack_target_id` |
| Cast bar (Casting / Channeling) | ✅ | `cast_bar.tscn`; hidden when not casting |
| Cast error toast | ✅ | "No target", "On Cooldown", etc. |
| Player death + game over | ✅ | `Dead` triggers `_game_over` flag; sim_bridge pauses tree |
| Bot AI (v4) | ✅ | Three-layer state machine + scored skill selection |
| Bot respawn system | ✅ | Tier-based respawn with role distribution |
| Bottom HUD | ✅ | 4 skills + 6 items + 6 backpack slots |
| Settings panel (ESC) | ✅ | Camera mode / edge pan / edge speed / smooth pan / fullscreen / cast mode |
| Camera modes (lock / free / pixel drag) | ✅ | Configurable via settings panel |
| Edge-pan / smooth-pan | ✅ | Both configurable; speeds persisted |
| Fullscreen modes (windowed / borderless / exclusive) | ✅ | Applied at startup + on change |
| Status effects (Root / Stun) | ✅ | `status_effect_system` |
| 3D entities + 60Hz interpolation | ✅ | `entity_view.gd` lerp |
| **Zone shrink (battle royale)** | ❌ | `SafeZone` component, shrink phases |
| **Multi-hero / class system** | 🟡 | `HeroDef` / `HeroRegistry` infrastructure done; only Swordsman registered |
| **Equipment / item system** | ❌ | P1-8 (planned) |
| **Minimap** | ❌ | View-only |
| **Fog of war** | ❌ | View + Sim (`Vision` component) |
| **Shield system** | ❌ | Sim `Shield` component + UI segment |
| **Status effect icons (Stun / Slow / Silence / Burn)** | 🟡 | `StatusEffect` data is implemented; HUD icons not yet |
| **Bush / stealth** | ❌ | Sim + 3D bush + transparency |
| **Multi-player networking** | ❌ | Architecture is network-ready (LocalInputSingleton is injectable) but not implemented |

> Note: bot "attack" is a real basic attack now (Homing arrow via `attack_fire_system`), not a placeholder. Bot skill usage is also real (`ISkill` + `HeroInputState` injection).

### 8.2 Implemented MOBA modules (status per CONTEXT.md)

| # | Module | Status | Reference |
| --- | --- | --- | --- |
| P0-1 | Mana system | ✅ | `sim_api_reference.md §2.3` |
| P0-2 | Skill system (Q/W/E/R + level-up) | ✅ | `hero_skill_architecture.md` |
| P0-3 | Cast indicator (cursor / AoE / dash path) | ✅ | `sim_bridge.gd` + `skill_vfx.gd` |
| P0-4 | Skill bar HUD | ✅ | `bottom_hud.gd` |
| P0-5 | Player death + game over | ✅ | `world.cpp` `Dead` check; `sim_bridge` pauses |
| P0-6 | Zone shrink | ❌ | planned (§8.4) |
| P1-8 | Equipment system | ❌ | planned (§8.5) |

### 8.3 Skill system — implementation reference

For the v3 Hero + Skill refactor design rationale and P1–P6 record, see `hero_skill_architecture.md`. Key design principles:

- `HeroTag` / `HeroInputState` unify Player + Bot.
- Skills are independent `ISkill` implementations in `src_cpp/sim/skills/`; not part of Hero definition.
- Heroes reference skills through `SkillComponent.Slots[i].SkillId`.
- `HeroRegistry` + `HeroDef` for adding new heroes without code changes (data-driven from `data/stats.yaml`).
- `ISkill` lifecycle: `validate_cast` → `on_cast_start` → `on_chase_tick` / `can_enter_casting` → `on_cast_complete` / `on_channel_tick` / `on_dash_start` / `on_dash_update`.
- `BotCastRequest` lets Bot AI write its intent without coupling to the input system.

### 8.4 Zone shrink (planned)

#### Sim layer

```cpp
struct SafeZone {
    Vec2 Center{0.0f};
    float CurrentRadius = 50.0f;
    float TargetRadius = 10.0f;
    float ShrinkSpeed = 2.0f;
    float DamagePerTick = 2.0f;
    float WaitTime = 60.0f;
    float ShrinkTime = 30.0f;
    float WaitTimer = 0.0f;
    float ShrinkTimer = 0.0f;
    int Phase = 0;
};
```

New systems (`systems/safe_zone.h`):

- `SafeZoneShrinkSystem`: timed shrink, phases: wait → shrink → wait …
- `SafeZoneDamageSystem`: damage Damageable entities outside the circle

View layer:

- Safe zone visuals: ground half-transparent cylindrical wall (`MeshInstance3D` + `CylinderMesh`)
- Next-phase preview: white dashed circle (pre-shrink warning)
- Edge-of-zone flashing warning (before storm closes)
- Player HUD: distance to safe zone + direction indicator

Snapshot extension:

```cpp
class SimZoneSnap : public godot::RefCounted {
    float center_x, center_y;
    float current_radius, target_radius;
    float next_radius;
    int phase;
    bool is_shrinking;
};
```

### 8.5 Equipment / item system (planned)

The current `Pickup` system is too simple; BR needs in-match equipment growth.

```cpp
enum class ItemType : uint8_t {
    Weapon, Armor, Consumable, PassiveMod,
};

struct ItemDef {
    int Id;
    ItemType Type;
    StringName Name;
    float AtkBonus = 0.0f;
    float AspBonus = 0.0f;
    float ManaBonus = 0.0f;
    float SpeedBonus = 0.0f;
    float CDRBonus = 0.0f;
    int ActiveSkillId = 0;
};
```

Component extensions:

```cpp
struct Inventory { int Items[6]; int Count; };  // 6 slots, 0=empty
struct EquipmentBonuses {
    float AtkBonus, AspBonus, ManaBonus, SpeedBonus, CDRBonus;
};
```

Systems:

- `ItemSpawnSystem`: world item spawn (initial scatter + airdrops)
- `ItemPickupSystem`: pickup → `Inventory` → update `EquipmentBonuses`
- `ItemDropSystem`: drop / replace
- `ItemPassiveSystem`: per-tick passive application (stat mods)

View layer:

- Inventory HUD (6 slots next to skills)
- 3D world item model (chest / light pillar / icon)
- Hover tooltip
- F-key pickup prompt

### 8.6 Vision and fog of war (planned)

Sim layer:

```cpp
struct Vision {
    float Radius = 20.0f;
    std::vector<int> VisibleEntityIds;
};

struct FogReveal {
    float Radius;
    Vec2 Center;
};
```

New system (`systems/vision_system.h`):

- Compute visible entities per observer
- Write to `VisibleEntityIds`
- Invisible entities not in snapshot
- Wall occlusion (ray test or NavMesh)

View layer:

- Fog rendering: `CanvasLayer` + full-screen black `ColorRect`
- Use `_draw()` or shader to cut transparent holes
- Invisible entities don't create `EntityView`

### 8.7 Minimap (planned)

View only:

```
Minimap (CanvasLayer, top-right, 200×200)
├── Background (ColorRect, semi-transparent black)
├── MapDisplay (TextureRect, top-down map render)
│   ├── PlayerDot (player position)
│   ├── EnemyDot × N (visible enemies)
│   ├── ZoneCircle (safe zone)
│   └── PickupDot × N (pickups)
└── Border (ColorRect, frame)
```

Recommended: manual `_draw()` (lightweight). Data sources:

- Player position: `snapshot.heroes[local_idx].x/y`
- Safe zone: `SimZoneSnap`
- Visible enemies: query via `EntityManager`

Click minimap → move camera to that location.

### 8.8 Combat feedback (planned)

| Feature | Layer | Note |
| --- | --- | --- |
| **Damage numbers** | View | Floating numbers from head; color = damage type |
| **Kill notification** | View | Top-right scrolling message |
| **Skill hit VFX** | View | Particles + screen shake + flash |
| **Audio** | View | Triggered via `SimEventSnap` |
| **Multi-kill prompt** | View | "Double Kill!" banner |

`SimEventSnap` extension:

```cpp
enum class EventType : uint8_t {
    Kill, Damage, SkillCast, ItemPickup, LevelUp, ZoneDamage,
};

class SimEventSnap {
    EventType type;
    int killer_id, victim_id;
    int value;          // damage / xp
    int skill_id;
    int x, y;
};
```

### 8.9 Multi-hero / class system (in progress)

Infrastructure done in v3; only Swordsman registered so far. Adding a new hero is a `data/stats.yaml` + `register_builtin_heroes` registration.

```cpp
struct HeroDef {
    int Id;
    StringName Name;
    float BaseHp, BaseMana, BaseAtk, BaseAsp, BaseSpeed;
    int SkillIds[4];
    // ... (see sim_api_reference.md §4.1)
};
```

View layer:

- Hero select screen (lobby scene)
- Different 3D models / color schemes per hero
- Hero-specific skill icons

### 8.10 Other planned additions

| Feature | Layer | Note |
| --- | --- | --- |
| **Shield system** | Sim | `Shield` component (Cur / Max); damage hits shield before HP |
| **Shield bar** | View | White segment to the right of Fill |
| **Status effect icons** | View | Stun / Slow / Silence / Burn row above health bar |
| **Bush / stealth** | Sim | Entity enters `BushArea` → Stealth state |
| **Bush visual** | View | 3D bush model + transparency on entry |

---

## 9. Implementation priority and effort

> Reflects current planning. Update when P0-6 / P1-8 etc. become implemented.

```
P0 — core loop (must play):
  1. Mana system                                       ✅ done
  2. Skill system framework (1 skillshot end-to-end)   ✅ done
  3. Cast indicator (cast cursor + range circle + AoE)  ✅ done
  4. Skill bar UI                                      ✅ done
  5. Player death + elimination                        ✅ done
  6. Zone shrink (2 phases)                            ❌ not started

P1 — in-match experience:
  7. Multi-skill types (Targeted / AoE / Buff / Dash)  ✅ done
  8. Equipment / item system                           ❌ not started
  9. Minimap                                           ❌ not started
  10. Fog of war                                       ❌ not started
  11. Damage numbers + kill notification               🟡 partial (HUD hooks exist; no floating numbers yet)
  12. Multi-hero (HeroDef + HeroRegistry)              🟡 infra done; only Swordsman registered

P2 — full MOBA:
  13. Status effect system (Stun / Slow / Silence)      🟡 Root/Stun data + system; HUD icons pending
  14. Shield system                                    ❌ not started
  15. Bush / stealth                                   ❌ not started
  16. Map expansion (500×500 + POI + terrain)          ❌ not started
  17. Battle royale zone (P0-6)                        ❌ not started
```

---

## 10. Biggest architectural challenge

> **The original P0-2 skill system refactor (2026 mid-year) was the biggest challenge — and it has been completed.** The notes below describe the historical risk that drove the v3 refactor.

**The original problem**: combat was "single arrow = basic attack"; four tightly-coupled systems (`player_fire.h` → `arrow_movement.h` → `wall_collision.h` → `combat.h`) all worked directly on arrows. The refactor layered:

```
Before:
  player_fire → arrow_movement → wall_collision → combat (arrow → HP)
  bot_combat  ↗

Target (achieved):
  skill_cast → projectile_movement (generalized) → wall_collision → combat
  player_fire → skill_effect (treated as basic-attack skill) ↗
  bot_combat  ↗
```

Key refactor points:

1. `player_fire.h` became a special case of `skill_cast.h` (basic attack = skill with `SkillId = 0`). The current implementation actually went further: basic attack is now a separate `ATTACK` command processed by `attack_command_system` + `attack_fire_system`, not a skill at all. See `input_system_design.md §9`.
2. `arrow_movement.h` generalized to handle Homing, plain, and (future) other projectile types.
3. `combat.h` extended to support Homing-only-target-locking and AoE area damage.
4. New skill effect types (Buff / Dash / Channel) don't go through `combat.h`; they directly mutate components via `ISkill` lifecycle hooks.
