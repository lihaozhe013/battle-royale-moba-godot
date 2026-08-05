# Sim API Reference — C++ ECS Layer Detailed Reference

> Last updated: 2026-08-02
> **Scope**: detailed component / system / registry / binding reference for the C++ Sim layer.
> **NOT in scope**: end-to-end data flow (see `docs/DATA_FLOW.md`), input system (see `input_system_design.md`), Hero/Skill rationale (see `hero_skill_architecture.md`), Bot AI (see `bot_ai.md`).
> **Code is the source of truth** — verify any field/signatures here against the actual headers (`src_cpp/sim/components.h`, `src_cpp/sim/snapshot_types/`, `src_cpp/sim_server.h`).

---

## 1. File Layout

```
src_cpp/
├── register_types.cpp / .h           # GDExtension entry; registers 9 GDCLASS types
├── sim_server.cpp / .h               # SimServer (GDExtension binding layer)
│
└── sim/
    ├── components.h                  # 30+ ECS components (see §2)
    ├── game_config.h                 # StatsConfig registry-context accessor
    ├── stats_config.h / .cpp         # stats.yaml loader
    ├── vec2.h                        # Vec2 = glm::vec2 + collision helpers
    ├── command_buffer.h              # deferred entity ops
    ├── arrow_spawner.h               # try_fire() helper
    ├── nav_grid.h / .cpp             # A* pathfinding
    ├── json_util.h                   # minimal JSON parser (map only)
    │
    ├── world.h / .cpp                # World class: init + tick loop
    │   ├── world_spawn.cpp           # _spawn_player / _spawn_bot
    │   └── world_input.cpp           # set_*_command API impls
    │
    ├── snapshot_types.h              # umbrella include
    ├── snapshot_types/               # 9 GDCLASS sub-headers (see §5)
    ├── snapshot_builder.h / .cpp     # registry → SimSnapshot
    ├── snapshot_bindings.cpp         # _bind_methods() for all snap types
    │
    ├── heroes/                       # Hero definitions
    │   ├── hero_def.h                # HeroDef struct
    │   ├── hero_registry.h / .cpp    # singleton registry
    │   └── (per-hero files, e.g. swordsman.h)
    │
    ├── skills/                       # ISkill implementations
    │   ├── skill_interface.h         # ISkill abstract base
    │   ├── skill_registry.h / .cpp   # singleton registry
    │   ├── melee_strike.h            # C — MeleeSingle
    │   ├── aoe_field.h               # E — AoEField
    │   ├── dash.h                    # R — Dash
    │   └── channel_burst.h           # F — ChannelBurst
    │
    └── systems/                      # 22 header-only inline Systems
        ├── local_input_injection.h
        ├── bot_targeting.h
        ├── bot_ai.h
        ├── bot_combat_state.h
        ├── bot_skill_decider.h
        ├── bot_input_injection.h
        ├── bot_role_rules.h
        ├── attack_command.h
        ├── skill_cast.h
        ├── pathfinding.h
        ├── movement.h
        ├── attack_fire.h
        ├── arrow_movement.h
        ├── wall_collision.h
        ├── combat.h
        ├── pickup.h
        ├── aoe.h
        ├── status_effect.h
        ├── mana_regen.h
        ├── skill_cooldown.h
        ├── skill_level.h
        ├── progression.h
        ├── snapshot_export.h
        └── xp_helper.h               # shared XP/level helpers
```

**Conventions**:

- Every system is `inline void` in `namespace sim`, parameter order: `entt::registry &reg` first, then `float dt`, then dependencies.
- Entity creation/destruction always via `CommandBuffer::push()`; `CommandBuffer::flush()` is called at the end of `World::tick`.
- C++ Sim is zero-Godot: no `godot::` types in systems/components. Godot bindings live only in `sim_server.cpp`, `snapshot_types/`, `register_types.cpp`.

---

## 2. Component Catalog

All components live in `src_cpp/sim/components.h`. Components with `= default` initializers are usable with `entt::registry::emplace<T>(e)`.

### 2.1 Spatial / Physics

| Component | Fields | Notes |
| --- | --- | --- |
| `Position2D` | `Vec2 Value` | 2D world coords; map clamps to `MapBounds.Half`. |
| `Velocity2D` | `Vec2 Value` | Used by arrows. |
| `FacingAngle` | `float Radians` | Set by `movement_system`. |
| `Lifetime` | `float Remaining` | Decremented each tick in `arrow_movement`; arrow despawns at 0. |
| `MoveSpeed` | `float Value` | Scales position delta per tick. |
| `MovePath` | `vector<Vec2> Waypoints; int CurrentIndex; bool Following; Vec2 FinalTarget` | Output of `pathfinding_system`, consumed by `movement_system`. |
| `AttackTarget` | `entity Target; int TargetNetworkId; bool Chasing` | Set by `attack_command_system`; `Chasing` flag = currently homing. |
| `Homing` | `entity Target; int TargetNetId` | Attached to a Homing arrow; `arrow_movement` steers it. |
| `StatusEffect` | `StatusType Type; float Timer` | `Root` = can't move, `Stun` = can't act. |
| `CastState` | (complex — see §2.7) | Skill cast state machine. |

### 2.2 Identity & Lifecycle

| Component | Fields | Notes |
| --- | --- | --- |
| `NetworkId` | `int Value` | Stable network-unique id (player/bot/arrow/pickup/aoe). |
| `Health` | `int Cur, Max` | Death when `Cur <= 0` (`combat_system`). |
| `Dead` | `bool enabled` | Tagged dead; most systems skip `Dead` entities. |
| `Damageable` | _(empty tag)_ | Marker for arrow/aoe collision detection. |
| `Level` | `int Value` | Character level. |
| `Experience` | `int Cur, Needed` | `Needed` updates on level-up. |
| `Kills` | `int Value` | Incremented in `combat_system`. |
| `CombatStats` | `float Atk, Asp; double LastFireTime` | LastFireTime gates `attack_fire_system` cooldown. |

### 2.3 Resources

| Component | Fields | Notes |
| --- | --- | --- |
| `Mana` | `float Cur, Max, RegenRate, RegenDelay, RegenTimer` | Skill consumption sets `RegenTimer = RegenDelay`. |

### 2.4 Hero (v3 unified Player + Bot)

| Component | Fields | Notes |
| --- | --- | --- |
| `HeroTag` | `bool IsLocal` | All heroes. `IsLocal=true` only for the local player. |
| `HeroInputState` | (complex — see §2.8) | All heroes. Filled by `local_input_injection` (player) or `bot_input_injection` (bot). |
| `HeroDefId` | `int Value` | Lookup in `HeroRegistry::instance().get(id)`. |
| `SkillComponent` | `SkillSlot Slots[4]` | Q=0, W=1, E=2, R=3. SkillIds assigned at spawn from HeroDef. |
| `SkillSlot` | `int SkillId, Level; float CooldownTimer, MaxCooldown, ManaCost` | Per-slot state. |
| `SimSkillSlotSnap` | `skill_id, level, cooldown, max_cooldown, mana_cost, cast_range` | View snapshot of a skill slot; `cast_range` is calculated by `ISkill::range(level)`. |
| `SkillPoints` | `int Available` | Incremented on level-up by `progression_system`. |
| `CastState` | (see §2.7) | Per-entity cast state. |

**Aliases (transitional)**: `using PlayerTag = HeroTag;` and `using PlayerInputState = HeroInputState;` in `components.h` for legacy code paths. Treat them as `HeroTag` / `HeroInputState`.

### 2.5 Bot (in addition to Hero components)

| Component | Fields | Notes |
| --- | --- | --- |
| `BotTag` | _(empty tag)_ | Marker (legacy; in practice `HeroTag{IsLocal=false}` is used). |
| `BotAIState` | `Vec2 MoveTarget; float RespawnTimer; entity TargetEntity; float WanderTimer; float TargetLockTimer` | Per-tick AI runtime state. |
| `BotBehaviorState` | `enum Goal{ Flee, SeekHeal, SeekXp, Engage, Wander }; entity PickupTarget; float DecisionCooldown; enum KiteSub{Chase, Strafe, Retreat}; int StrafeDir; KiteSub Kite; float GoalCommitTimer` | Goal-layer state. |
| `BotCombatState` | `enum Phase{Approach, Kite, Burst, Sustain, Disengage}; Phase Current; float PhaseTimer; int BurstStep; float BurstTimer` | v4 combat layer. |
| `BotCastRequest` | `int TargetSlot; Vec2 AimPos; int TargetNetworkId; bool Valid; float Score` | v4 skill-decider output, consumed by `bot_input_injection`. |
| `BotTier` | `enum{Normal=0, Elite=1, Boss=2}` | Multiplier on stats. |
| `BotRole` | `enum{Fodder=0, Stalker=1, Brute=2}` | Determines spawn level range (see `bot_role_rules.h`). |
| `BotVisionRange` | `float Value` | `bot_targeting` scan radius. |

### 2.6 Projectile / AoE / Pickup / Wall

| Component | Fields | Notes |
| --- | --- | --- |
| `ArrowTag` | `int OwnerId; entity OwnerEntity; float Dmg; float LifestealRatio` | Arrow entity marker. |
| `AoETag` | `int OwnerId, SkillId; float Radius, Duration, Timer` | AoE entity marker. |
| `PickupTag` | `PickupType Type; int Value` | Pickup entity marker. |
| `PickupSpawner` | `PickupType Type; int Value; Vec2 Position; float RespawnTime, CurrentTimer; bool Active; int CurrentEntityId` | Spawner state (not a visible entity until spawned). |
| `WallTag` | _(empty tag)_ | Wall marker. |
| `WallBounds` | `Vec2 Min, Max` | AABB. |

### 2.7 `CastState` Detail

```cpp
struct CastState {
    enum class Phase : uint8_t {
        None = 0,
        Aiming = 1,     // quick cast same-tick transit only
        Chasing = 2,    // confirmed but out of range; pathfinding moves to target
        Casting = 3,    // cast time
        Channeling = 4, // F channel
        Dashing = 5,    // R dash motion
    };
    Phase State = Phase::None;
    int ActiveSlot = -1;
    int SkillId = 0;
    float Timer = 0.0f;       // current phase timer
    float SubTimer = 0.0f;    // sub-phase (e.g. channel ticks)
    float RejectTimer = 0.0f; // post-None cooldown to prevent re-entry
    float PendingCooldown = 0.0f;
    float PendingManaCost = 0.0f;
    Vec2 AimPos{0.0f};
    Vec2 DashStart{0.0f};
    Vec2 DashTarget{0.0f};
    int HitTargetId = -1;
    int CastError = 0;        // 1=CD 2=Mana 3=Stun 4=NoTarget 5=TargetDead
    entt::entity TargetEntity = entt::null;
    int TargetNetworkId = -1; // targeted skill lock
    bool QuickCast = false;
};
```

Phase transitions are owned by `skill_cast_system`. Detailed table: `input_system_design.md §7.2`.

### 2.8 `HeroInputState` & `LocalInputSingleton` Detail

Both share the same fields (intentionally — `local_input_injection_system` copies `LocalInputSingleton` → `HeroInputState` at tick start).

```cpp
// Move
Vec2  MoveTarget{0.0f};
bool  MoveIssue = false;     // pulse: re-issue right-click pathing
bool  Stop = false;          // pulse: stop following current path

// Skill
int   SkillSlot = -1;        // 0-3 (QWER), 10-15 reserved for equipment (P1-8)
bool  SkillConfirm = false;  // pulse: this tick the cast is confirmed
Vec2  SkillAim{0.0f};
int   SkillTargetId = -1;    // Targeted skill NetworkId lock
int   SkillUpgradeSlot = -1; // pulse: Ctrl+Q/W/E/R upgrade

// Cancel
bool  CancelSkill = false;   // pulse
bool  CancelAttack = false;  // pulse

// Attack (separate from skill)
int   AttackTargetId = -1;   // lock target
bool  AttackGround = false;  // ground-mode attack
Vec2  AttackGroundPos{0.0f};
bool  AttackClear = false;   // pulse: clear current lock

// Sequence
int   Seq = 0;               // for debugging / ordering
```

Pulse fields are cleared by `local_input_injection_system` after copy.

### 2.9 Singletons (on dedicated entities)

| Component | Fields | Entity | Purpose |
| --- | --- | --- | --- |
| `LocalInputSingleton` | (see §2.8) | `_local_input_entity` | Holds current frame's view-issued commands. |
| `MapBounds` | `float Half` | `_map_bounds_entity` | World extent. |
| `IdState` | `int NextPlayerId, NextBotId, NextArrowId, NextPickupId, NextAoEId` | `_id_state_entity` | NetworkId allocator. |
| `KillEventBuffer` | `vector<KillEvent> events` | `_kill_event_entity` | Consumed by `progression_system` then cleared. |

---

## 3. System Reference (22 systems)

All systems are `inline void` free functions in `namespace sim`. Signature convention: `(entt::registry &reg, float dt, …)`. `World::tick` (in `world.cpp`) calls them in the order below.

| # | System | File | Reads | Writes | Key behavior |
| --- | --- | --- | --- | --- | --- |
| 1 | `local_input_injection_system` | `local_input_injection.h` | `LocalInputSingleton` | `HeroInputState` (for `IsLocal=true`) | Copy frame inputs; clear pulse fields. |
| 2 | `bot_targeting_system` | `bot_targeting.h` | `Position2D`, `Health`, `BotVisionRange` | `BotAIState.TargetEntity` | Pick target in vision: `is_local` Hero first, then min HP, then min dist. |
| 3 | `bot_ai_system` | `bot_ai.h` | `BotBehaviorState`, `Health`, pickups | `BotBehaviorState`, `BotAIState`, position via `BotInputState` (not `Position2D` directly) | Goal FSM (Flee/SeekHeal/SeekXp/Engage/Wander); respawn roll (level + tier); writes `MoveTarget` to be picked up by `bot_input_injection`. |
| 4 | `bot_combat_state_system` | `bot_combat_state.h` | `BotBehaviorState`, `BotAIState`, `Health` | `BotCombatState` | v4: Approach / Kite / Burst / Sustain / Disengage. |
| 5 | `bot_skill_decider_system` | `bot_skill_decider.h` | `BotCombatState`, `SkillComponent`, `Mana`, `BotAIState` | `BotCastRequest` | v4: scoring function picks best slot; sets `Score` and `Valid`. |
| 6 | `bot_input_injection_system` | `bot_input_injection.h` | `BotAIState`, `BotCastRequest` | `HeroInputState` (for AI heroes) | Translates AI state into `MoveIssue`, `SkillSlot`/`SkillConfirm`, `AttackTargetId`. |
| 7 | `attack_command_system` | `attack_command.h` | `HeroInputState.AttackTargetId/Ground/Clear` | `AttackTarget` | Resolve target NetworkId → entity; clear lock on `AttackClear`. |
| 8 | `skill_cast_system` | `skill_cast.h` | `HeroInputState.SkillSlot/Confirm`, `CastState`, `SkillComponent`, `Mana`, `StatusEffect` | `CastState`, `SkillSlot.CooldownTimer`, `Mana` | Dispatcher into `ISkill` lifecycle (`validate_cast` → `on_cast_start` → `Chasing`/`Casting` → `on_cast_complete`); commits mana/cooldown only after cast completion and discards pending resources on cancellation. |
| 9 | `pathfinding_system` | `pathfinding.h` | `MovePath`, `CastState` (Chasing phase), `AttackTarget`, `HeroInputState.MoveTarget` | `MovePath` | A* via `NavGrid`. Priority: Chasing > AttackTarget chase > right-click path. |
| 10 | `movement_system` | `movement.h` | `MovePath`, `CastState`, `StatusEffect`, `MoveSpeed` | `Position2D`, `FacingAngle` | Apply MovePath / AttackTarget.Chasing / CastState.Dashing motion; set `AttackTarget.Chasing` flag; gate on `StatusEffect` + `CastState`. |
| 11 | `attack_fire_system` | `attack_fire.h` | `AttackTarget`, `CombatStats`, `CastState` | `CommandBuffer` (new arrow) | Cooldown check via `LastFireTime`; spawns Homing arrow if target in `AttackRange`. |
| 12 | `arrow_movement_system` | `arrow_movement.h` | `Velocity2D`, `Homing` | `Position2D`, `Lifetime` | Advance position; Homing steers toward `Target`. |
| 13 | `wall_collision_system` | `wall_collision.h` | `Position2D`, `WallBounds`, `ArrowTag`, `Homing`, `AttackTarget.Chasing`, `CastState::Dashing` | `Position2D`, `CommandBuffer` (arrow destroy) | AABB resolve for movers; skip Chasing heroes + Dashing heroes + Homing arrows; destroy arrows intersecting wall. |
| 14 | `combat_system` | `combat.h` | `ArrowTag`, `Position2D`, `Health` | `Health`, `Dead`, `Kills`, `KillEventBuffer`, `CommandBuffer` (arrow destroy) | Circle overlap; apply damage; Homing arrows only hit their locked target. |
| 15 | `pickup_system` | `pickup.h` | `PickupSpawner`, `PickupTag`, `Position2D`, `Health` | `PickupSpawner.Active/CurrentTimer`, `Health`, `Experience`, `CommandBuffer` | Spawner timer; overlap XP/Heal pickup. |
| 16 | `aoe_system` | `aoe.h` | `AoETag`, `Position2D` | `AoETag.Timer`, `CommandBuffer` | Tick AoE lifetime; destroy on expiry. |
| 17 | `status_effect_system` | `status_effect.h` | `StatusEffect` | `StatusEffect.Timer` | Decrement; clear when ≤ 0. |
| 18 | `mana_regen_system` | `mana_regen.h` | `Mana` | `Mana.Cur` | If `RegenTimer ≤ 0`: `Cur = min(Cur + RegenRate*dt, Max)`. |
| 19 | `skill_cooldown_system` | `skill_cooldown.h` | `SkillComponent` | `SkillSlot.CooldownTimer` | Per tick: `CooldownTimer = max(0, CooldownTimer - dt)`. |
| 20 | `skill_level_system` | `skill_level.h` | `HeroInputState.SkillUpgradeSlot`, `SkillComponent`, `SkillPoints` | `SkillSlot.Level`, `SkillPoints.Available` | If upgrade pulse and `Available>0` and `Level<Max`: `Level++`, `Available--`. |
| 21 | `progression_system` | `progression.h` | `KillEventBuffer` | `CombatStats`, `Experience`, `Level`, `MoveSpeed`, `Health.Max`, `SkillPoints` | Apply kills → XP, ATK/ASP, level-up cascade. |
| 22 | `snapshot_export_system` | `snapshot_export.h` | entire registry | `godot::Ref<SimSnapshot>` | Build snapshot, increment `tick_counter`, store on `World`. |

**Tick-end**: `World::_cb.flush(_reg)` applies all deferred entity ops from the systems that pushed to `CommandBuffer`.

### 3.1 System call signatures (verbatim)

```cpp
void local_input_injection_system(entt::registry &reg, entt::entity local_input_entity);

void bot_targeting_system(entt::registry &reg, std::mt19937 &rng, float dt);
void bot_ai_system(entt::registry &reg, float dt, float map_half, std::mt19937 &rng);
void bot_combat_state_system(entt::registry &reg, float dt);
void bot_skill_decider_system(entt::registry &reg, std::mt19937 &rng);
void bot_input_injection_system(entt::registry &reg);

void attack_command_system(entt::registry &reg, float dt);
void skill_cast_system(entt::registry &reg, float dt, CommandBuffer &cb, IdState &ids, double now);
void pathfinding_system(entt::registry &reg, NavGrid &nav);
void movement_system(entt::registry &reg, float dt, float map_half);
void attack_fire_system(entt::registry &reg, double now, CommandBuffer &cb, IdState &ids);

void arrow_movement_system(entt::registry &reg, float dt);
void wall_collision_system(entt::registry &reg, CommandBuffer &cb);
void combat_system(entt::registry &reg, CommandBuffer &cb);

void pickup_system(entt::registry &reg, float dt, CommandBuffer &cb, IdState &ids);
void aoe_system(entt::registry &reg, float dt, CommandBuffer &cb);
void status_effect_system(entt::registry &reg, float dt);
void mana_regen_system(entt::registry &reg, float dt);
void skill_cooldown_system(entt::registry &reg, float dt);
void skill_level_system(entt::registry &reg);
void progression_system(entt::registry &reg);

bool snapshot_export_system(entt::registry &reg, int &tick_counter, godot::Ref<SimSnapshot> &out);
```

---

## 4. HeroDef & SkillRegistry

### 4.1 `HeroDef` (`src_cpp/sim/heroes/hero_def.h`)

```cpp
struct HeroDef {
    int Id = 0;
    std::string Name;
    int SkillIds[4] = {0, 0, 0, 0};  // Q=0, W=1, E=2, R=3

    int BaseHp = 100;
    float BaseMana = 300.0f;
    float BaseAtk = 10.0f;
    float BaseAsp = 1.0f;
    float BaseMoveSpeed = 5.0f;
    float AttackRange = 8.0f;

    float HpPerLevel = 10.0f;
    float AtkPerLevel = 1.0f;
    float AspPerLevel = 0.03f;
    float SpeedPerLevel = 0.5f;

    int PrefabId = 0;
};
```

### 4.2 `HeroRegistry` (`heroes/hero_registry.h`)

```cpp
class HeroRegistry {
  public:
    static HeroRegistry &instance();
    const HeroDef &get(int id) const;          // throw or assert on miss
    void register_hero(const HeroDef &def);
};

void register_builtin_heroes(const StatsConfig &config);  // called by World::initialize
```

### 4.3 `ISkill` (`skills/skill_interface.h`)

Skill implementations override only the lifecycle hooks they need; defaults are no-ops.

```cpp
class ISkill {
  public:
    virtual ~ISkill() = default;
    virtual int id() const = 0;
    virtual SkillKind kind() const = 0;

    // Stat lookup; default returns base_*. Override for level-scaling.
    virtual float base_cooldown() const;
    virtual float base_mana_cost() const;
    virtual float base_cast_time() const;
    virtual float base_range(int level) const;
    virtual float cooldown(int level) const;
    virtual float mana_cost(int level) const;
    virtual float cast_time(int level) const;
    virtual float range(int level) const;
    virtual float damage(int level, float atk) const;
    virtual float effect_value(int level) const;

    // Lifecycle hooks.
    virtual int validate_cast(registry&, caster, ctx) = 0;     // returns CastError code or 0=OK
    virtual void on_cast_start(...);                            // may spawn projectile, set CastError=0
    virtual void on_chase_tick(...);                            // per-tick during Chasing phase
    virtual bool can_enter_casting(...);                        // target/lifecycle check
    virtual void on_cast_complete(...);                         // apply effect, transition state
    virtual void on_channel_tick(...);                          // F skill
    virtual void on_dash_start(...);                            // R skill
    virtual void on_dash_update(...);                           // R skill motion
    virtual bool can_interrupt(CastState::Phase phase) const;   // default: Chasing+Casting interruptible
};
```

### 4.4 `SkillRegistry` (`skills/skill_registry.h`)

```cpp
class SkillRegistry {
  public:
    static SkillRegistry &instance();
    void register_skill(int id, std::unique_ptr<ISkill> skill);
    ISkill *get(int id) const;
    bool has(int id) const;
};

void register_builtin_skills(const StatsConfig &config);
```

### 4.5 Built-in Skills

| ID | Kind | File | Notes |
| --- | --- | --- | --- |
| 1 | `MeleeSingle` | `melee_strike.h` | C — single-target melee; uses `CastState.HitTargetId`. |
| 2 | `AoEField` | `aoe_field.h` | E — ground AoE; spawns `AoETag` entity. |
| 3 | `Dash` | `dash.h` | R — position interp; transitions to `Dashing` phase. |
| 4 | `ChannelBurst` | `channel_burst.h` | F — multi-tick channel; spawns arrow array on each tick. |

---

## 5. Snapshot Types

Eight `GDCLASS`-derived RefCounted types are exposed to GDScript. Registered in `register_types.cpp`:

```cpp
godot::ClassDB::register_class<SimServer>();
godot::ClassDB::register_class<sim::SimSnapshot>();
godot::ClassDB::register_class<sim::SimSkillSlotSnap>();
godot::ClassDB::register_class<sim::SimPlayerSnap>();
godot::ClassDB::register_class<sim::SimBotSnap>();
godot::ClassDB::register_class<sim::SimHeroSnap>();
godot::ClassDB::register_class<sim::SimArrowSnap>();
godot::ClassDB::register_class<sim::SimPickupSnap>();
godot::ClassDB::register_class<sim::SimEventSnap>();
godot::ClassDB::register_class<sim::SimAoESnap>();
```

Each type has:
1. Field declarations in `snapshot_types/sim_*_snap.h`.
2. Getters/setters and `_bind_methods()` (via `BIND` + `PROP` macros) in `snapshot_bindings.cpp`.
3. Population in `snapshot_builder.cpp::_build_*`.

For the field-by-field table, see `docs/DATA_FLOW.md §4` (consolidated; not duplicated here).

### 5.1 `SimSnapshot` Container

```cpp
class SimSnapshot : public godot::RefCounted {
    int64_t seq = 0;
    int64_t t = 0;                              // ms timestamp
    godot::TypedArray<SimHeroSnap> heroes;     // unified player+bot (preferred)
    godot::TypedArray<SimPlayerSnap> players;  // legacy fallback (still populated)
    godot::TypedArray<SimBotSnap> bots;        // legacy fallback (still populated)
    godot::TypedArray<SimArrowSnap> arrows;
    godot::TypedArray<SimPickupSnap> pickups;
    godot::TypedArray<SimAoESnap> aoes;
    godot::TypedArray<SimEventSnap> events;
    int get_local_hero_index() const;           // returns index of IsLocal hero
};
```

View layer should prefer `heroes[local_idx]` over `players[0]`; legacy fields are kept for backwards compatibility.

---

## 6. SimServer (GDExtension Binding)

`src_cpp/sim_server.h` exposes the world to GDScript. Each `set_*_command` writes into `LocalInputSingleton`; pulse fields are cleared by `local_input_injection_system` on the next tick.

```cpp
class SimServer : public godot::RefCounted {
  public:
    bool initialize(const godot::String &map_json,
                    const godot::String &stats_yaml);

    // ── v2 command API (current) ──
    void set_move_command(float target_x, float target_y, bool issue);
    void set_stop_command(bool stop);
    void set_skill_command(int slot, bool confirm,
                           float aim_x, float aim_y, int target_id);
    void set_skill_upgrade_command(int slot);
    void set_attack_command_full(int target_id, bool ground,
                                 float gx, float gy, bool clear);
    void set_cancel_command(bool skill, bool attack);

    // ── v1 API (deprecated; do not call from new code) ──
    // set_local_input, set_cast_input, set_attack_command (single arg)
    // are still present for legacy callers; sim_bridge no longer uses them.

    void tick(double delta);
    bool is_game_over();
    int get_hero_capacity() const;
    godot::Ref<godot::RefCounted> pop_snapshot();
};
```

`initialize` parses `map_json` and `stats_yaml`; on success it creates all initial entities (player + bots + walls + pickups). On failure, `last_error` is populated.

`get_hero_capacity()` is available after successful initialization and returns the configured local-hero plus bot capacity. The view uses this read-only value to prewarm the persistent health-bar pool before the first simulation tick.

`tick(delta)` runs the 22-system pipeline (see §3) and stores the latest `SimSnapshot` for `pop_snapshot()`.

`pop_snapshot()` is **consume-and-clear**: each call returns the latest snapshot and resets the stored ref. View should cache locally (e.g. `sim_bridge.last_snapshot`).

---

## 7. NetworkId Range Table

Allocated from `IdState` (`_id_state_entity`); ranges are configurable in `stats.yaml` under `player_id_start` etc., defaults shown.

| Type | Default Start | Allocated by |
| --- | --- | --- |
| Player | 1 | `world_spawn.cpp::_spawn_player` |
| Bot | 1001 | `world_spawn.cpp::_spawn_bot` |
| Arrow | 2001 | `arrow_spawner::try_fire` |
| Pickup | 3001 | `pickup_system` (on spawner activation) |
| AoE | 4001 | `aoe_system` (on `on_cast_start`) |

`NetworkId.Value` is the **only** stable cross-tick handle for any entity. All target lookups (`AttackTarget.TargetNetworkId`, `HeroInputState.SkillTargetId`, `CastState.TargetNetworkId`, `Homing.TargetNetId`) use this id and resolve via the registry in their respective systems.

---

## 8. `stats.yaml` Key Reference

All gameplay balance values are loaded at startup into `StatsConfig` (registry context). Editing the YAML + restart rebalances the game. The C++ side accesses via `stats(reg)` (helper in `game_config.h`).

Key sections (illustrative; not exhaustive):

| Section | Fields | Consumed by |
| --- | --- | --- |
| (top) | `tick_rate` (default 30), `map_half` (50), `player_id_start`/`bot_id_start`/... | `World::initialize` |
| `player` | `radius`, `speed`, `base_hp`, `base_attack`, `base_attack_speed`, `base_mana`, `mana_regen`, `mana_regen_delay` | `_spawn_player` |
| `bot` | `count`, `radius`, `speed`, `hp`, `base_attack`, `base_attack_speed`, `respawn_time`, `vision_range`, `max_level`, `boss_roll`, `elite_roll`, `mana`, `mana_regen` | `_spawn_bot` |
| `arrow` | `speed`, `lifetime`, `radius` | `arrow_spawner` + `arrow_movement` |
| `progression` | `atk_per_kill`, `asp_per_kill`, `asp_max`, `kill_xp_base`, `kill_xp_high_bonus`, `xp_per_level_base`, `hp_per_level`, `speed_per_level`, `heal_fraction` | `progression_system` + `xp_helper.h` |
| `pickup` | `xp_value`, `heal_value`, `small_heal_value`, respawn times per type, counts per type, radius | `pickup_system` |
| `bot_roles` | per-role level range / weight | `bot_role_rules` |
| `heroes.<name>` | `id`, `name`, `skills[4]`, base stats, per-level deltas, `prefab_id` | `register_builtin_heroes` |
| `skills.<name>` | `id`, `kind`, `cooldown`, `mana_cost`, `cast_time`, `range`, `damage`, `aoe_radius`, etc. | `register_builtin_skills` |

For the exact schema, see the `data/stats.yaml` file itself (the loader in `stats_config.cpp` is the single source of truth).

---

## 9. Adding New Code — Checklist

The patterns to follow when extending the Sim layer. (See also `AGENTS.md §4–5` for the agent-facing rules.)

### 9.1 Add a new component

1. Declare in `src_cpp/sim/components.h`.
2. Emplace in the appropriate `_spawn_*` function in `world_spawn.cpp`.
3. If any system reads it, add the include and `view<...>` access.
4. If the View layer needs the value, extend an existing `Sim*Snap` (see §9.2).
5. Update `docs/DATA_FLOW.md` data flow if it crosses a layer boundary.

### 9.2 Add a new snapshot field

This is the only area with a 3-file ritual; do not skip steps.

1. `src_cpp/sim/snapshot_types/sim_*_snap.h` — add field, getter/setter, and declaration in `_bind_methods()`.
2. `src_cpp/sim/snapshot_bindings.cpp` — `BIND(cls, field)` + `PROP(...)` macro pair.
3. `src_cpp/sim/snapshot_builder.cpp` — populate in the corresponding `_build_*` method.
4. (No `register_types.cpp` change unless introducing a new GDCLASS type.)

### 9.3 Add a new ISkill

1. Implement in `src_cpp/sim/skills/<name>.h` (header-only, `inline` subclass of `ISkill`).
2. Register in `register_builtin_skills(...)` (in `skills/skill_registry.cpp`) with a unique id.
3. Add base stats to `stats.yaml` under `skills.<name>`; the constructor reads them.
4. Add to HeroDef's `SkillIds[4]` for any hero that should have it.
5. Implement only the lifecycle hooks you need; defaults are no-ops.

### 9.4 Add a new HeroDef

1. Add base stats under `heroes.<name>` in `stats.yaml`.
2. Either add a hardcoded `HeroDef` in `hero_registry.cpp` `register_builtin_heroes`, or wire it up to read directly from `stats(reg)`.
3. Reference by `HeroDefId.Value` from spawn code.

### 9.5 Add a new System

1. Header-only `inline void` in `src_cpp/sim/systems/<name>.h`; namespace `sim`.
2. Use `entt::registry &reg` first, then `float dt`, then dependencies.
3. Never `_reg.create()` / `_reg.destroy()` directly — push to `CommandBuffer`.
4. Skip `Dead` entities: `if (reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled) continue;`.
5. Register in `World::tick` (`world.cpp`) at the correct position. The ordering matters — see `docs/DATA_FLOW.md §3`.

### 9.6 Add a new GDCLASS snap type

1. `snapshot_types/sim_*_snap.h` with `GDCLASS` + fields + getters + `_bind_methods()`.
2. `snapshot_bindings.cpp` — `BIND`/`PROP` for every field.
3. `snapshot_builder.cpp` — populate in `_build_*`.
4. `register_types.cpp` — `ClassDB::register_class<sim::Sim*Snap>()`.
5. Add the `TypedArray<Sim*Snap>` field to `SimSnapshot` (§5.1).

---

## 10. Reference Files

| File | Role |
| --- | --- |
| `src_cpp/sim/components.h` | All ECS components (§2). |
| `src_cpp/sim/systems/*.h` | All 22 systems (§3). |
| `src_cpp/sim/world.cpp` | `World::tick` ordering (§3 + `DATA_FLOW.md §3`). |
| `src_cpp/sim/world_spawn.cpp` | `_spawn_player` / `_spawn_bot` initial components. |
| `src_cpp/sim/heroes/*.h` | HeroDef + HeroRegistry (§4.1–4.2). |
| `src_cpp/sim/skills/*.h` | ISkill interface + 4 built-ins (§4.3–4.5). |
| `src_cpp/sim/snapshot_types/*` | 8 GDCLASS types (§5). |
| `src_cpp/sim/snapshot_bindings.cpp` | `_bind_methods()` registrations. |
| `src_cpp/sim/snapshot_builder.cpp` | Registry → SimSnapshot. |
| `src_cpp/sim_server.h/.cpp` | SimServer GDExtension binding (§6). |
| `src_cpp/register_types.cpp` | ClassDB registration (§5). |
| `src_cpp/sim/stats_config.cpp` | `data/stats.yaml` loader (§8). |
| `data/stats.yaml` | Editable balance data (§8). |
