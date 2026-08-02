# Hero + Skill System Refactor — Architecture Design

> Last updated: 2026-08-02
> Status: ✅ **P1–P6 fully implemented** (Hero unification + Skill interface + Bot behavior tree + v4 combat state machine)
> This document is the **design rationale** for the refactor. For the **operational reference** of the current state, see `sim_api_reference.md` (components / systems / API) and `docs/DATA_FLOW.md` (end-to-end data flow).
> Runtime balance values come from `data/stats.yaml`; numeric examples in this doc describe historical design intent, not the edit surface.

---

## Table of Contents

1. [Motivation and goals](#1-motivation-and-goals)
2. [Current-state diagnosis (pre-refactor)](#2-current-state-diagnosis-pre-refactor)
3. [Hero unification design](#3-hero-unification-design)
4. [Skill independence design](#4-skill-independence-design)
5. [HeroDef registry](#5-herodef-registry)
6. [Snapshot unification](#6-snapshot-unification)
7. [View layer migration guide](#7-view-layer-migration-guide)
8. [System generalization and rename](#8-system-generalization-and-rename)
9. [Tick order (post-refactor)](#9-tick-order-post-refactor)
10. [Implementation phases](#10-implementation-phases)
11. [File change list](#11-file-change-list)
12. [Risks and mitigations](#12-risks-and-mitigations)
13. [Appendix A — Relationship to other docs](#appendix-a--relationship-to-other-docs)
14. [Appendix B — Before vs. After architecture comparison](#appendix-b--before-vs-after-architecture-comparison)

---

## 1. Motivation and goals

### 1.1 Pain points

| Problem | Concrete symptoms | Impact |
|---|---|---|
| Player/Bot dual track | `PlayerTag` / `BotTag` two component+system lines; 7 systems hardcode `view<PlayerTag>` | Bots cannot use skills; maintains redundant `bot_combat` |
| Skill hard-coded to Hero | `skill_cast.h` 521-line monolith; `_trigger_effect` inlines 4 `SkillKind` branches with full damage/control/arrow logic | Adding a new `SkillKind` requires touching 3+ switches with zero reuse |
| Snapshot duplication | `SimPlayerSnap` + `SimBotSnap` field overlap; View layer forks loops | Maintenance cost doubled |
| No multi-hero support | Player and Bots share one base-stats set; no "hero template" concept | Future heroes = copy-paste of the entire spawn logic |

### 1.2 Refactor goals

| # | Goal | How |
|---|---|---|
| G1 | Unify Player/Bot as **Hero** | Introduce `HeroTag`; only `IsLocal` distinguishes the local player |
| G2 | Skill is a **fully independent class**, not part of Hero | `ISkill` interface + `SkillRegistry`; Hero stores only `SkillSlot` (id + runtime) |
| G3 | Skills are reusable and composable | `HeroDef = base stats + 4 SkillIds` referencing registered `ISkill` instances |
| G4 | Support many hero types | `HeroRegistry` with definitions |
| G5 | View layer needs no Player/Bot fork | Single `SimHeroSnap` + `is_local` field; one loop suffices |

---

## 2. Current-state diagnosis (pre-refactor)

### 2.1 Player/Bot dual track — component view

| Component | Player | Bot | After unification |
|---|---|---|---|
| `PlayerTag { IsLocal }` | ✅ | ❌ | → `HeroTag { IsLocal }` |
| `BotTag` | ❌ | ✅ | → Delete; unified `HeroTag` |
| `PlayerInputState` | ✅ | ❌ | → `HeroInputState` (same struct; bots reuse) |
| `BotAIState` / `BotBehaviorState` / `BotTier` / `BotRole` / `BotVisionRange` | ❌ | ✅ | Preserve (AI-specific) |
| `MovePath` | ✅ | ❌ | → All heroes have one |
| `CastState` | ✅ | ❌ | → All heroes have one |
| `AttackTarget` | ✅ | ❌ | → All heroes have one |
| `SkillComponent` | ✅ (4 slots) | ✅ (4 slots, no consumer) | → Unchanged |

### 2.2 Player/Bot dual track — system view

| System | Player only | Bot only | After unification |
|---|---|---|---|
| `local_input_injection` | ✅ `view<PlayerTag>` | ❌ | Unchanged (only Local hero) |
| `player_attack_command` | ✅ | ❌ | → `attack_command`; generalized `view<HeroTag>` |
| `skill_cast` | ✅ (heavy) | ❌ | → Generalized; dispatches `ISkill` |
| `player_pathfinding` | ✅ | ❌ | → `pathfinding`; generalized |
| `player_movement` | ✅ | ❌ | → `movement`; generalized |
| `player_attack_fire` | ✅ | ❌ | → `attack_fire`; generalized; `bot_combat` deleted |
| `bot_combat` | ❌ | ✅ (light) | → **Delete**; replaced by generalized `attack_fire` |
| `bot_targeting` | ❌ | ✅ | Preserve (already operates on all `Damageable`) |
| `bot_ai` | ❌ | ✅ | → Split into Goal decision + `BotInputInjection` |
| (new) `bot_input_injection` | ❌ | ❌ | → **New**; Bot AI → `HeroInputState` |
| (new) `bot_skill_decider` | ❌ | ❌ | → **New**; Engage subtree skill selection |
| `skill_level` | ✅ `view<PlayerTag>` | ❌ | → Generalized |

### 2.3 Skill tight-coupling detail

```
skill_defs.h (static SkillDef table, 36 lines)
  └─ 5 entries; each SkillDef has Id/Kind/CastTime/ManaCost/Cooldown/Damage/Range/…
  └─ get_skill_def(id) table lookup

skill_cast.h (521 lines, single file)
  ├── skill_cast_system (state machine)
  │   ├── Phase::None → validate → Phase::Casting/Chasing
  │   ├── Phase::Chasing → tick → Phase::Casting/None
  │   ├── Phase::Casting → timer → _trigger_effect → Phase::None/Dashing/Channeling
  │   ├── Phase::Dashing → position update → Phase::None
  │   └── Phase::Channeling → tick → timer → Phase::None
  └── _trigger_effect (single switch over SkillKind)
      ├── SkillKind::MeleeSingle → direct damage + kill event
      ├── SkillKind::AoEField → area damage + stun + AoE entity
      ├── SkillKind::Dash → (hands off to Dashing phase)
      └── SkillKind::ChannelBurst → spawn arrow array
```

**Problems**:

1. Adding a new `SkillKind` requires editing the enum + `skill_defs.h` + non-fixed number of switches in `skill_cast.h`.
2. A "summon minions" skill would need `SkillKind::Summon` and would intrude into the state machine.
3. Damage formulas (`def.Damage + stats.Atk * ratio`) are duplicated across skills with no shared abstraction.

---

## 3. Hero unification design

### 3.1 Component changes

```cpp
// ── Removed ──
struct PlayerTag { bool IsLocal; };           // alias to HeroTag during transition
struct BotTag {};                              // alias/empty during transition
struct PlayerInputState { ... };               // renamed → HeroInputState

// ── New ──
struct HeroTag { bool IsLocal = false; };      // all heroes
struct HeroInputState {                        // same content as the old PlayerInputState
    Vec2 MoveTarget{0.0f};
    bool MoveIssue = false;
    bool Stop = false;
    int  SkillSlot = -1;
    bool SkillConfirm = false;
    Vec2 SkillAim{0.0f};
    int  SkillTargetId = -1;
    int  SkillUpgradeSlot = -1;
    bool CancelSkill = false;
    bool CancelAttack = false;
    int  AttackTargetId = -1;
    bool AttackGround = false;
    Vec2 AttackGroundPos{0.0f};
    bool AttackClear = false;
    int  Seq = 0;
};
struct HeroDefId { int Value = 0; };           // lookup key in HeroRegistry

// ── Preserved (AI-specific) ──
struct BotAIState { ... };
struct BotBehaviorState { ... };
struct BotTier { ... };
struct BotRole { ... };
struct BotVisionRange { ... };
```

For complete current field tables, see `sim_api_reference.md §2`.

### 3.2 Entity spawn

**Player (local hero):**

```cpp
void World::_spawn_player(int player_id, bool is_local) {
    const auto &def = HeroRegistry::instance().get(/*default hero id*/ 1);

    auto e = _reg.create();
    _reg.emplace<HeroTag>(e, is_local);
    _reg.emplace<HeroDefId>(e, /*hero_def_id*/ 1);
    _reg.emplace<HeroInputState>(e);
    _reg.emplace<NetworkId>(e, player_id);
    _reg.emplace<Position2D>(e, /*random spawn*/);
    _reg.emplace<Health>(e, def.BaseHp, def.BaseHp);
    _reg.emplace<Mana>(e, def.BaseMana, def.BaseMana, /*RegenRate*/ ...);
    _reg.emplace<CombatStats>(e, def.BaseAtk, def.BaseAsp, -999.0);
    _reg.emplace<Damageable>(e);
    _reg.emplace<Dead>(e, false);
    _reg.emplace<Level>(e, 1);
    _reg.emplace<Experience>(e, 0, /*XpPerLevelBase*/);
    _reg.emplace<MoveSpeed>(e, def.BaseMoveSpeed);
    _reg.emplace<SkillPoints>(e, 0);
    _reg.emplace<CastState>(e);
    _reg.emplace<AttackTarget>(e);
    _reg.emplace<MovePath>(e);

    SkillComponent sc;
    for (int i = 0; i < 4; ++i) {
        int sid = def.SkillIds[i];
        const auto *sk = SkillRegistry::instance().get(sid);
        sc.Slots[i].SkillId = sid;
        sc.Slots[i].Level = 1;
        sc.Slots[i].MaxCooldown = sk->base_cooldown();
        sc.Slots[i].ManaCost = sk->base_mana_cost();
    }
    _reg.emplace<SkillComponent>(e, sc);
}
```

**Bot (AI hero):**

```cpp
void World::_spawn_bot_with_role(BotRole role, int level) {
    int hero_def_id = 1;  // same as player default
    auto e = _spawn_hero(hero_def_id, /*bot_id*/ ..., /*IsLocal*/ false);

    // AI-specific components
    _reg.emplace<BotAIState>(e, ...);
    _reg.emplace<BotBehaviorState>(e);
    _reg.emplace<BotTier>(e, /*rolled*/ tier);
    _reg.emplace<BotRole>(e, role);
    _reg.emplace<BotVisionRange>(e, /*stats.*/ ...);

    // Bot stat scaling (apply at spawn time, not per-frame)
    auto &hp = _reg.get<Health>(e);
    hp.Max = static_cast<int>(hp.Max * /*BotHpMul*/);
    hp.Cur = hp.Max;
    auto &stats = _reg.get<CombatStats>(e);
    stats.Atk *= /*BotAtkMul*/;

    // Bot skill CD scaling (apply at SkillSlot init)
    auto &skills = _reg.get<SkillComponent>(e);
    for (int i = 0; i < 4; ++i) {
        skills.Slots[i].MaxCooldown *= /*BotSkillCooldownMul*/;
    }
}
```

### 3.3 Migration strategy

**Transition period**: keep `PlayerTag` / `BotTag` as `HeroTag` aliases. All new systems use `HeroTag`; old `view<PlayerTag>` coexists for one release. Pipeline:

1. Add `HeroTag`; keep `PlayerTag` / `BotTag` definitions.
2. All new systems use `HeroTag`; old systems add `if (!reg.all_of<HeroTag>(e)) continue;` to avoid double-processing.
3. Once no references remain, delete `PlayerTag` / `BotTag`.

---

## 4. Skill independence design

### 4.1 `ISkill` interface

```cpp
// src_cpp/sim/skills/skill_interface.h
#pragma once

#include "../components.h"
#include "../command_buffer.h"
#include <entt/entt.hpp>

namespace sim {

struct CastContext {
    entt::entity caster;
    const struct SkillSlot &slot;
    int level;
    Vec2 aim_pos;
    entt::entity target_entity;
    int target_network_id;
    bool quick_cast;
};

class ISkill {
public:
    virtual ~ISkill() = default;
    virtual int id() const = 0;
    virtual SkillKind kind() const = 0;

    // Stat query (replaces static SkillDef table)
    virtual float base_cooldown() const { return 0.0f; }
    virtual float base_mana_cost() const { return 0.0f; }
    virtual float base_cast_time() const { return 0.0f; }
    virtual float base_range(int level) const { return 0.0f; }

    // Per-level scaling
    virtual float cooldown(int level) const { return base_cooldown(); }
    virtual float mana_cost(int level) const { return base_mana_cost(); }
    virtual float cast_time(int level) const { return base_cast_time(); }
    virtual float range(int level) const { return base_range(level); }
    virtual float damage(int level, float atk) const;
    virtual float effect_value(int level) const { return 0.0f; }

    // Validation (replaces inline validate in skill_cast.h)
    // Returns error code: 0=ok, 1=cd, 2=mana, 3=stun, 4=no_target, 5=target_dead
    virtual int validate_cast(
        entt::registry &reg, entt::entity caster,
        const CastContext &ctx
    ) = 0;

    // Lifecycle hooks
    virtual void on_cast_start(
        entt::registry &reg, entt::entity caster,
        struct CastState &cs, CommandBuffer &cb, IdState &ids,
        const CastContext &ctx
    ) {}

    virtual void on_chase_tick(
        entt::registry &reg, entt::entity caster,
        struct CastState &cs, int level, float dt
    ) {}

    // Chasing → Casting entry check (skill-specific semantics)
    virtual bool can_enter_casting(
        entt::registry &reg, entt::entity caster,
        const struct CastState &cs, int level
    ) = 0;

    virtual void on_cast_complete(
        entt::registry &reg, entt::entity caster,
        struct CastState &cs, CommandBuffer &cb, IdState &ids,
        int level
    ) = 0;

    virtual void on_channel_tick(
        entt::registry &reg, entt::entity caster,
        struct CastState &cs, CommandBuffer &cb, IdState &ids,
        int level, float dt
    ) {}

    virtual void on_dash_start(
        entt::registry &reg, entt::entity caster,
        struct CastState &cs, int level
    ) {}
    virtual void on_dash_update(
        entt::registry &reg, entt::entity caster,
        struct CastState &cs, int level, float dt
    ) {}

    virtual bool can_interrupt(CastState::Phase phase) const {
        return phase == CastState::Phase::Chasing ||
               phase == CastState::Phase::Casting;
    }
};

} // namespace sim
```

### 4.2 `SkillRegistry`

```cpp
// src_cpp/sim/skills/skill_registry.h
#pragma once

#include "skill_interface.h"
#include <memory>
#include <unordered_map>

namespace sim {

class SkillRegistry {
public:
    static SkillRegistry &instance();

    void register_skill(int id, std::unique_ptr<ISkill> skill);
    const ISkill *get(int id) const;
    bool has(int id) const;

private:
    SkillRegistry() = default;
    std::unordered_map<int, std::unique_ptr<ISkill>> _skills;
};

} // namespace sim
```

```cpp
// src_cpp/sim/skills/skill_registry.cpp
#include "skill_registry.h"
#include "melee_strike.h"
#include "aoe_field.h"
#include "dash.h"
#include "channel_burst.h"

namespace sim {

SkillRegistry &SkillRegistry::instance() {
    static SkillRegistry inst;
    return inst;
}

void SkillRegistry::register_skill(int id, std::unique_ptr<ISkill> skill) {
    _skills[id] = std::move(skill);
}

const ISkill *SkillRegistry::get(int id) const {
    auto it = _skills.find(id);
    return it != _skills.end() ? it->second.get() : nullptr;
}

bool SkillRegistry::has(int id) const { return _skills.contains(id); }

void register_builtin_skills(const StatsConfig &config) {
    auto &r = SkillRegistry::instance();
    r.register_skill(1, std::make_unique<MeleeStrikeSkill>());
    r.register_skill(2, std::make_unique<AoEFieldSkill>());
    r.register_skill(3, std::make_unique<DashSkill>());
    r.register_skill(4, std::make_unique<ChannelBurstSkill>());
}

} // namespace sim
```

### 4.3 Skill file structure

```
src_cpp/sim/skills/
├── skill_interface.h           # ISkill abstract interface
├── skill_registry.h            # SkillRegistry declaration
├── skill_registry.cpp          # implementation + register_builtin_skills
├── melee_strike.h              # SkillId=1, Kind=MeleeSingle (Q)
├── aoe_field.h                 # SkillId=2, Kind=AoEField    (E)
├── dash.h                      # SkillId=3, Kind=Dash        (R)
└── channel_burst.h             # SkillId=4, Kind=ChannelBurst (F)
```

### 4.4 Built-in skill example (MeleeStrike)

```cpp
// src_cpp/sim/skills/melee_strike.h
#pragma once

#include "skill_interface.h"

namespace sim {

class MeleeStrikeSkill : public ISkill {
public:
    int id() const override { return 1; }
    SkillKind kind() const override { return SkillKind::MeleeSingle; }

    float base_cooldown() const override { return 5.0f; }
    float base_mana_cost() const override { return 20.0f; }
    float base_cast_time() const override { return 0.2f; }
    float base_range(int) const override { return 8.0f; }

    float cooldown(int level) const override {
        return base_cooldown() - (level - 1) * 0.5f;  // 0.5s per level
    }
    float mana_cost(int level) const override {
        float reduction_per_level = 0.05f;
        return base_mana_cost() * std::max(0.3f, 1.0f - (level - 1) * reduction_per_level);
    }
    float damage(int level, float atk) const override {
        return 40.0f + (level - 1) * 15.0f + atk * 0.9f;
    }

    int validate_cast(entt::registry &reg, entt::entity caster,
                      const CastContext &ctx) override {
        if (!ctx.target_entity || !reg.valid(ctx.target_entity))
            return 4;  // no target
        if (reg.all_of<Dead>(ctx.target_entity) &&
            reg.get<Dead>(ctx.target_entity).enabled)
            return 5;  // target dead
        return 0;
    }

    bool can_enter_casting(entt::registry &reg, entt::entity caster,
                           const CastState &cs, int level) override {
        if (!cs.TargetEntity || !reg.valid(cs.TargetEntity))
            return false;
        bool dead = reg.all_of<Dead>(cs.TargetEntity) &&
                    reg.get<Dead>(cs.TargetEntity).enabled;
        if (dead) return false;
        Vec2 delta = reg.get<Position2D>(cs.TargetEntity).Value -
                     reg.get<Position2D>(caster).Value;
        return vec2_length_sq(delta) <= range(level) * range(level);
    }

    void on_cast_complete(entt::registry &reg, entt::entity caster,
                          CastState &cs, CommandBuffer &cb, IdState &ids,
                          int level) override {
        entt::entity tgt = cs.TargetEntity;
        if (!reg.valid(tgt)) return;
        auto &hp = reg.get<Health>(tgt);
        float dmg = damage(level, reg.get<CombatStats>(caster).Atk);
        hp.Cur -= static_cast<int>(dmg);

        if (reg.all_of<NetworkId>(tgt))
            cs.HitTargetId = reg.get<NetworkId>(tgt).Value;

        if (hp.Cur <= 0) {
            hp.Cur = 0;
            if (reg.all_of<Dead>(tgt))
                reg.get<Dead>(tgt).enabled = true;
            // respawn timer + kill event + kills++ handled by combat_system
        }
    }
};

} // namespace sim
```

### 4.5 `skill_cast_system` reduced to dispatcher

```cpp
// src_cpp/sim/systems/skill_cast.h (post-refactor)
inline void skill_cast_system(
    entt::registry &reg, float dt, CommandBuffer &cb, IdState &ids, double now
) {
    auto view = reg.view<
        HeroTag, HeroInputState, SkillComponent, Mana,
        CastState
    >();

    for (auto e : view) {
        // ... handle Confirm / Cancel / state transitions ...
        // dispatch to ISkill::validate_cast / on_cast_start / on_chase_tick /
        // on_cast_complete / on_channel_tick / on_dash_* by phase ...
    }
}
```

(For the full current implementation, see `src_cpp/sim/systems/skill_cast.h` directly.)

---

## 5. HeroDef registry

### 5.1 `HeroDef` struct

```cpp
// src_cpp/sim/heroes/hero_def.h
#pragma once

#include <string>

namespace sim {

struct HeroDef {
    int Id = 0;
    std::string Name;

    // 4 skill ids (reference SkillRegistry)
    int SkillIds[4] = {0, 0, 0, 0};

    // Base stats
    int BaseHp = 100;
    float BaseMana = 300.0f;
    float BaseAtk = 10.0f;
    float BaseAsp = 1.0f;
    float BaseMoveSpeed = 5.0f;
    float AttackRange = 8.0f;

    // Level scaling
    float HpPerLevel = 10.0f;
    float AtkPerLevel = 1.0f;
    float AspPerLevel = 0.03f;
    float SpeedPerLevel = 0.5f;

    // Optional: visual
    int PrefabId = 0;        // used by View layer
    std::string IconPath;    // optional
};

} // namespace sim
```

### 5.2 `HeroRegistry`

```cpp
// src_cpp/sim/heroes/hero_registry.h
#pragma once

#include "hero_def.h"
#include <unordered_map>

namespace sim {

class HeroRegistry {
public:
    static HeroRegistry &instance();
    const HeroDef &get(int id) const;
    void register_hero(const HeroDef &def);

private:
    std::unordered_map<int, HeroDef> _heroes;
};

} // namespace sim
```

### 5.3 Built-in hero

```cpp
// src_cpp/sim/heroes/hero_registry.cpp
#include "hero_registry.h"

namespace sim {

void register_builtin_heroes(const StatsConfig &config) {
    auto &r = HeroRegistry::instance();
    r.register_hero({
        .Id = 1,
        .Name = "Swordsman",
        .SkillIds = {1, 2, 3, 4},  // MeleeStrike, AoEField, Dash, ChannelBurst
        .BaseHp = 100,
        .BaseMana = 300.0f,
        .BaseAtk = 10.0f,
        .BaseAsp = 1.0f,
        .BaseMoveSpeed = 5.0f,
        .AttackRange = 8.0f,
        .HpPerLevel = 10.0f,
        .PrefabId = 0,
    });
    // To add more heroes, register here AND in stats.yaml under heroes.<name>
    // r.register_hero({ .Id = 2, .Name = "Archer", ... });
}

} // namespace sim
```

### 5.4 File structure

```
src_cpp/sim/heroes/
├── hero_def.h
├── hero_registry.h
├── hero_registry.cpp
├── swordsman.h    (optional; can be inlined in cpp)
└── archer.h       (optional)
```

> **Design principle**: `hero_def.h` is pure data with no behavior. All behavior lives in `SkillRegistry`'s `ISkill` instances.

---

## 6. Snapshot unification

### 6.1 `SimHeroSnap`

```cpp
// snapshot_types/sim_hero_snap.h
class SimHeroSnap : public godot::RefCounted {
    GDCLASS(SimHeroSnap, godot::RefCounted)
public:
    // ── Common ──
    int id = 0;
    float x = 0, y = 0, ang = 0;
    int hp = 0, max_hp = 0;
    bool dead = false;                    // previously only on bot snap
    float mana = 0, max_mana = 0;
    float atk = 0, asp = 0, speed = 0;
    int kills = 0, level = 0;
    int xp = 0, xp_needed = 0;
    int status = 0;                       // StatusType
    godot::TypedArray<SimSkillSlotSnap> skills;

    // ── Combat state (previously only on PlayerSnap) ──
    int cast_state = 0;                   // CastState::Phase
    int cast_slot = -1;
    float cast_progress = 0.0f;
    float cast_aim_x = 0.0f, cast_aim_y = 0.0f;
    float dash_sx = 0.0f, dash_sy = 0.0f;
    float dash_tx = 0.0f, dash_ty = 0.0f;
    int hit_target_id = -1;
    int cast_error = 0;
    int attack_target_id = -1;
    int cast_target_id = -1;
    bool is_moving = false;
    int skill_points = 0;

    // ── Level / rarity (previously only on BotSnap) ──
    int tier = 0;                         // player=0, bot uses existing tier

    // ── New ──
    bool is_local = false;                // sole local player
    int hero_def_id = 0;                  // View picks prefab

    // ... getter/setter ...
};
```

### 6.2 `SimSnapshot` changes

```cpp
// snapshot_types/sim_snapshot.h
class SimSnapshot : public godot::RefCounted {
    GDCLASS(SimSnapshot, godot::RefCounted)
public:
    int seq = 0;
    int64_t t = 0;
    godot::TypedArray<SimHeroSnap> heroes;       // ← replaces players + bots
    godot::TypedArray<SimArrowSnap> arrows;      // unchanged
    godot::TypedArray<SimPickupSnap> pickups;    // unchanged
    godot::TypedArray<SimEventSnap> events;      // unchanged
    godot::TypedArray<SimAoESnap> aoes;          // unchanged

    // Migration helper: returns the local hero index (View layer doesn't have to search)
    int get_local_hero_index() const;
};
```

### 6.3 `SnapshotBuilder` traversal

```cpp
// snapshot_builder.cpp — unified _build_heroes
void SnapshotBuilder::_build_heroes(
    entt::registry &reg, SimSnapshot &snap
) {
    auto view = reg.view<HeroTag, Position2D, FacingAngle, Health,
                         Mana, Level, MoveSpeed, CombatStats,
                         NetworkId, Kills, Experience>();

    for (auto e : view) {
        auto snap_hero = SimHeroSnap::create();
        auto &tag = view.get<HeroTag>(e);

        snap_hero->id = view.get<NetworkId>(e).Value;
        snap_hero->x = view.get<Position2D>(e).Value.x;
        snap_hero->y = view.get<Position2D>(e).Value.y;
        snap_hero->ang = view.get<FacingAngle>(e).Radians;
        snap_hero->hp = view.get<Health>(e).Cur;
        snap_hero->max_hp = view.get<Health>(e).Max;
        snap_hero->mana = view.get<Mana>(e).Cur;
        snap_hero->max_mana = view.get<Mana>(e).Max;
        snap_hero->atk = view.get<CombatStats>(e).Atk;
        snap_hero->asp = view.get<CombatStats>(e).Asp;
        snap_hero->kills = view.get<Kills>(e).Value;
        snap_hero->level = view.get<Level>(e).Value;
        snap_hero->xp = view.get<Experience>(e).Cur;
        snap_hero->xp_needed = view.get<Experience>(e).Needed;
        snap_hero->speed = view.get<MoveSpeed>(e).Value;
        snap_hero->dead = reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled;
        snap_hero->tier = reg.all_of<BotTier>(e)
            ? static_cast<int>(reg.get<BotTier>(e)) : 0;  // default 0
        snap_hero->is_local = tag.IsLocal;
        snap_hero->hero_def_id = reg.all_of<HeroDefId>(e)
            ? reg.get<HeroDefId>(e).Value : 0;

        if (reg.all_of<CastState>(e)) { /* populate cast_* fields */ }
        if (reg.all_of<SkillComponent>(e)) { /* build skill slots */ }

        snap.get_heroes().push_back(snap_hero);
    }
}
```

---

## 7. View layer migration guide

### 7.1 Core changes

| Pattern | Old | New |
|---|---|---|
| Find local player | `snap.players[0]` | `snap.heroes[snap.get_local_hero_index()]` or `snap.heroes.filter(func(h): return h.is_local)[0]` |
| Iterate enemies | `for b in snap.bots` | `for h in snap.heroes: if !h.is_local: ...` |
| Iterate all units | `for p in snap.players` + `for b in snap.bots` | `for h in snap.heroes` |
| Health bar team | `set_team(0)` for player, `set_team(2)` for bot | `set_team(0 if h.is_local else 2)` |
| Health bar tier | player: `update_level(p.level, 0)` / bot: `update_level(b.level, b.tier)` | `update_level(h.level, h.tier)` |
| Prefab selection | `entity_type = 0` (player) / `1` (bot) | `hero_def_id` → look up HeroDef → pick prefab |
| Combat VFX anchor | `snap.players[0].cast_aim_x/y` | same |

### 7.2 Affected files

| File | Change |
|---|---|
| `sim_bridge.gd` | Replace `snap.players[0]` (3 sites) + `for b in snap.bots` → `for h in snap.heroes` |
| `entity_manager.gd` | Merge player/bot loops; prefab selection uses `hero_def_id` |
| `entity_view.gd` | `entity_type` 0/1 → merged Hero branch |
| `health_bar_manager.gd` | Merge loops; `team`/`tier` derived from `h.is_local`/`h.tier` |
| `skill_vfx.gd` | `snap.players[0]` → look up by `is_local` |
| `bottom_hud.gd` | duck-typed; zero changes |
| `cast_bar.gd` / `skill_slot_ui.gd` | zero changes |
| `input/input_state_machine.gd` | Field source renamed for `is_moving` / `cast_state`; no logic changes |

---

## 8. System generalization and rename

### 8.1 System rename + generalize

| Old | New | Change |
|---|---|---|
| `player_attack_command_system` | `attack_command_system` | `view<HeroTag, HeroInputState, …>`; bot goes through `HeroInputState` |
| `player_attack_fire_system` | `attack_fire_system` | Same; replaces lone `bot_combat_system` |
| `player_pathfinding_system` | `pathfinding_system` | Same; bot uses A* + Chasing |
| `player_movement_system` | `movement_system` | Same; bot uses MovePath + AttackTarget Chase + wall collision |
| `local_input_injection_system` | unchanged | Only `HeroTag{IsLocal}` |
| `skill_level_system` | unchanged | `view<HeroTag, …>` |
| `skill_cast_system` | unchanged | `view<HeroTag, …>` |
| `bot_ai_system` | unchanged | Goal decision + respawn only; **no longer writes `Position2D`** |
| `bot_combat_system` | **deleted** | Replaced by `attack_fire_system` |
| (new) `bot_input_injection_system` | **new** | BotAIState + BotBehaviorState → HeroInputState; called after `bot_ai_system` |
| (new) `bot_skill_decider_system` | **new** | Engage subtree skill selection → `BotCastRequest` → `bot_input_injection` consumes |

### 8.2 `World::tick()` update

```cpp
void World::tick(double dt) {
    if (_game_over) return;
    _time += dt;
    float fdt = static_cast<float>(dt);
    float map_half = _reg.get<MapBounds>(_map_bounds_entity).Half;
    auto &ids = _reg.get<IdState>(_id_state_entity);

    // Input
    local_input_injection_system(_reg, _local_input_entity);   // local hero only

    // Bot AI
    bot_targeting_system(_reg, _rng, fdt);                     // target selection
    bot_ai_system(_reg, fdt, map_half, _rng);                  // Goal + respawn
    bot_skill_decider_system(_reg, _rng);                      // skill selection → BotCastRequest
    bot_input_injection_system(_reg);                          // BotAIState → HeroInputState

    // Unified combat
    attack_command_system(_reg, fdt);                          // HeroInputState → AttackTarget
    skill_cast_system(_reg, fdt, _cb, ids, _time);             // HeroInputState → CastState (generalized)
    pathfinding_system(_reg, _nav_grid);                       // MoveIssue + Chasing → MovePath
    movement_system(_reg, fdt, map_half);                      // MovePath + AttackTarget + Chasing → pos
    attack_fire_system(_reg, _time, _cb, ids);                 // AttackTarget → homing arrow

    // Physics
    arrow_movement_system(_reg, fdt);
    wall_collision_system(_reg, _cb);
    combat_system(_reg, _cb);

    // Game systems
    pickup_system(_reg, fdt, _cb, ids);
    aoe_system(_reg, fdt, _cb);
    status_effect_system(_reg, fdt);
    mana_regen_system(_reg, fdt);
    skill_cooldown_system(_reg, fdt);
    skill_level_system(_reg);
    progression_system(_reg);
    snapshot_export_system(_reg, _tick_counter, _latest_snapshot);

    _cb.flush(_reg);
}
```

> Note: as of the v4 Bot refactor, the tick order also includes `bot_combat_state_system` (between `bot_ai_system` and `bot_skill_decider_system`). See `bot_ai.md §11` and `docs/DATA_FLOW.md §3` for the full 22-system sequence.

---

## 9. Tick order (post-refactor)

```
LocalInputInjection       (LocalInputSingleton → local HeroInputState)
BotTargeting              (AI target selection)
BotAI                     (Goal FSM + respawn roll)
BotCombatState            (combat phase FSM — v4)
BotSkillDecider           (Engage subtree skill selection → BotCastRequest)
BotInputInjection         (BotAIState + BotCastRequest → HeroInputState)
AttackCommand             (HeroInputState → AttackTarget)         ← generalized
SkillCast                 (HeroInputState → ISkill dispatch)      ← generalized
Pathfinding               (MoveIssue + Chasing → MovePath)        ← generalized
Movement                  (MovePath + AttackTarget Chase → pos)   ← generalized
AttackFire                (AttackTarget → homing arrow)            ← generalized; bot_combat deleted
ArrowMovement
WallCollision
Combat
Pickup / AoE / StatusEffect / ManaRegen / SkillCooldown / SkillLevel / Progression
SnapshotExport
```

---

## 10. Implementation phases

### P1: Component refactor (low risk)

| Task | Files |
|---|---|
| Add `HeroTag`, `HeroInputState` | `components.h` |
| Add `HeroDefId` component | `components.h` |
| `_spawn_player` mounts `HeroTag{IsLocal=true}`; keep `PlayerTag` | `world_spawn.cpp` |
| `_spawn_bot` mounts `HeroTag{IsLocal=false}`; keep `BotTag` | `world_spawn.cpp` |
| Lock down `SimHeroSnap` field list (no implementation yet) | `snapshot_types.h` |

### P2: HeroDef + HeroRegistry (low risk)

| Task | Files |
|---|---|
| Create `heroes/` dir + `hero_def.h` / `hero_registry.h/.cpp` | `heroes/*` |
| Register default hero "Swordsman" | `hero_registry.cpp` |
| `_spawn_player` / `_spawn_bot` use HeroDef for initialization | `world_spawn.cpp` |

### P3: Skill interface (critical path, high risk)

| Task | Files |
|---|---|
| Create `skills/` dir + `skill_interface.h` + `skill_registry.h/.cpp` | `skills/*` |
| Implement 4 built-in `ISkill` classes (`melee_strike.h` / `aoe_field.h` / `dash.h` / `channel_burst.h`) | `skills/*.h` |
| `register_builtin_skills()` called from `World::initialize` | `world.cpp` |
| `skill_cast_system` becomes `ISkill` dispatcher | `systems/skill_cast.h` |
| Delete `skill_defs.h` static table | — |
| Verify player skills still work | manual test |

### P4: System generalization + BotInputInjection (medium risk)

| Task | Files |
|---|---|
| `player_attack_command` → `attack_command` + generalized | `systems/attack_command.h` |
| `player_attack_fire` → `attack_fire` + generalized | `systems/attack_fire.h` |
| `player_pathfinding` → `pathfinding` + generalized | `systems/pathfinding.h` |
| `player_movement` → `movement` + generalized | `systems/movement.h` |
| Delete `bot_combat.h` | — |
| New `bot_skill_decider.h` | `systems/bot_skill_decider.h` |
| New `bot_input_injection.h` | `systems/bot_input_injection.h` |
| `bot_ai_system` split: keep Goal + respawn; **no longer write `Position2D`** | `systems/bot_ai.h` |
| `World::tick()` update | `world.cpp` |

### P5: Snapshot unification + View migration (medium risk)

| Task | Files |
|---|---|
| `SimHeroSnap` implementation | `snapshot_types.h` / `snapshot_bindings.cpp` |
| `SnapshotBuilder` unified `HeroTag` traversal | `snapshot_builder.cpp` |
| `SimSnapshot.heroes` array | `snapshot_types.h` |
| `sim_bridge.gd` switches to `snap.heroes` | `sim_bridge.gd` |
| `entity_manager.gd` loop merge | `entity_manager.gd` |
| `entity_view.gd` `entity_type` merge | `entity_view.gd` |
| `health_bar_manager.gd` loop merge | `health_bar_manager.gd` |
| `skill_vfx.gd` uses `is_local` | `skill_vfx.gd` |
| Delete `snap.players` + `snap.bots` | all files |
| Delete `PlayerTag` / `BotTag` component defs | `components.h` |

### P6: Bot behavior tree completion (medium risk)

| Task | Files |
|---|---|
| Bot skill coefficients into `StatsConfig` | `stats.yaml` |
| `_spawn_bot` multiplies `SkillSlot` by coefficients at init | `world_spawn.cpp` |
| `bot_skill_decider` Engage skill priority | `systems/bot_skill_decider.h` |
| `bot_input_injection` writes `HeroInputState` | `systems/bot_input_injection.h` |
| `bot_ai.md` v3 doc | `Docs/Reference/bot_ai.md` |

---

## 11. File change list

### New

| File | Phase |
|---|---|
| `src_cpp/sim/heroes/hero_def.h` | P2 |
| `src_cpp/sim/heroes/hero_registry.h` | P2 |
| `src_cpp/sim/heroes/hero_registry.cpp` | P2 |
| `src_cpp/sim/skills/skill_interface.h` | P3 |
| `src_cpp/sim/skills/skill_registry.h` | P3 |
| `src_cpp/sim/skills/skill_registry.cpp` | P3 |
| `src_cpp/sim/skills/melee_strike.h` | P3 |
| `src_cpp/sim/skills/aoe_field.h` | P3 |
| `src_cpp/sim/skills/dash.h` | P3 |
| `src_cpp/sim/skills/channel_burst.h` | P3 |
| `src_cpp/sim/systems/bot_skill_decider.h` | P4 |
| `src_cpp/sim/systems/bot_input_injection.h` | P4 |
| `Docs/Reference/hero_skill_architecture.md` | this doc |

### Modified

| File | Change |
|---|---|
| `src_cpp/sim/components.h` | Delete `PlayerTag`/`BotTag`/`PlayerInputState`; add `HeroTag`/`HeroInputState`/`HeroDefId`; keep `using PlayerTag = HeroTag;` and `using PlayerInputState = HeroInputState;` aliases during transition |
| `src_cpp/sim/stats_config.h` | Add Bot skill coefficients `BotSkillDmgMul`/`BotSkillCooldownMul`/`BotManaCostMul`; remove XP economy constants (now in `stats.yaml`) |
| `src_cpp/sim/world.h` | `_spawn_player`/`_spawn_bot` → unified HeroDef-based; add `_spawn_bot_with_role` |
| `src_cpp/sim/world_spawn.cpp` | `initialize` / spawn logic unified via HeroDef lookup |
| `src_cpp/sim/skill_defs.h` | **Deleted** — replaced by `skills/` |
| `src_cpp/sim/systems/skill_cast.h` | Reduced to ~200-line dispatcher |
| `src_cpp/sim/systems/player_attack_command.h` | → `attack_command.h` + generalized |
| `src_cpp/sim/systems/player_attack_fire.h` | → `attack_fire.h` + generalized |
| `src_cpp/sim/systems/player_pathfinding.h` | → `pathfinding.h` + generalized |
| `src_cpp/sim/systems/player_movement.h` | → `movement.h` + generalized |
| `src_cpp/sim/systems/bot_ai.h` | Split: Goal + respawn only; no `Position2D` write |
| `src_cpp/sim/systems/bot_combat.h` | **Deleted** |
| `src_cpp/sim/systems/skill_level.h` | `view<PlayerTag>` → `view<HeroTag>` |
| `src_cpp/sim/systems/local_input_injection.h` | mostly unchanged |
| `src_cpp/sim/snapshot_types.h` | Add `SimHeroSnap`; `SimSnapshot.heroes` replaces `players`+`bots` |
| `src_cpp/sim/snapshot_builder.h/.cpp` | `_build_heroes` unified traversal |
| `src_cpp/sim/snapshot_bindings.cpp` | Register `SimHeroSnap` properties and `SimSnapshot.get_local_hero_index` |
| `src_cpp/sim/world.h` | Update system includes |
| `scripts/sim_bridge.gd` | Replace `snap.players[0]` (3 sites); hover loop uses `snap.heroes` |
| `scripts/view/entity_manager.gd` | Merge loops; prefab by `hero_def_id` |
| `scripts/view/entity_view.gd` | `entity_type` 0/1 merged |
| `scripts/ui/health_bar_manager.gd` | Merge loops |
| `scripts/view/skill_vfx.gd` | Uses `is_local` to find local hero |
| `Docs/Reference/bot_ai.md` | Rewritten for v3 + v4 |

### Untouched (already generic)

| File | Reason |
|---|---|
| `bot_targeting.h` | Already operates on all `Damageable` |
| `arrow_movement.h` / `wall_collision.h` / `combat.h` | No player/bot distinction |
| `pickup.h` / `aoe.h` / `status_effect.h` | Same |
| `mana_regen.h` / `skill_cooldown.h` | Component-only, no tag filter |
| `progression.h` / `xp_helper.h` | Same |
| `bottom_hud.gd` / `cast_bar.gd` / `skill_slot_ui.gd` | duck-typed; zero changes |

---

## 12. Risks and mitigations

| Risk | Description | Mitigation |
|---|---|---|
| `ISkill` virtual call overhead | 5 heroes × 4 slots ≈ 20 calls/30Hz; hot path in `on_cast_complete` with many `cb.push` | Acceptable; inline high-frequency hooks (`on_chase_tick`) per-skill if needed |
| Bot input vs player conflict on `HeroInputState` | Bot writes via `reg.view` iteration; `LocalInputSingleton` only affects `IsLocal=true` | Natural isolation |
| Snapshot memory double-write during transition | During dual-write, `players[]` + `bots[]` + `heroes[]` coexist | Brief (one release); small data (~dozens of heroes); acceptable |
| `snap.players[0]` missing | View code needs replacement in multiple places | Search + blast radius documented; 3 sites total, listed in §7.2 |
| Old saves/configs depend on `PlayerTag`/`BotTag` values | C++ side has no persisted state; full rebuild from registry on load | No migration needed |
| New skill integration CI regressions | Wide blast radius could break existing skill behavior | One sim test per skill (e.g. `test_melee_strike` verifies damage + kill) |
| Bot doesn't use skills or skill loop deadlocks | `bot_skill_decider` ↔ `bot_input_injection` interaction bug | P5 Engage fallback: no available skill → basic attack |
| Existing View prefabs (`player.tscn` / `bot.tscn`) hardcoded | `entity_manager.gd`'s `PREFAB_PATHS` array | Switch to `hero_prefabs` dict keyed by `hero_def_id` + default fallback |

---

## Appendix A — Relationship to other docs

| Doc | Impact |
|---|---|
| `Docs/Reference/sim_api_reference.md` | ✅ Already updated to current post-refactor state (this was the result of the same refactor) |
| `Docs/Reference/bot_ai.md` | ✅ Rewritten for v3 + v4 (aligned with this doc) |
| `Docs/Reference/input_system_design.md` | Largely unaffected (View→Sim command channel unchanged); §18 Bot-related clarifications updated |
| `Docs/Reference/prompt.md` | Unchanged |
| `CONTEXT.md` | ✅ Updated (renamed from `AGENTS.md`); added `heroes/` + `skills/` directory entries and P1–P6 status |
| `docs/DATA_FLOW.md` | ✅ New doc covering end-to-end flow across the refactored components |

---

## Appendix B — Before vs. After architecture comparison

```
BEFORE                                    AFTER

PlayerTag + BotTag                        HeroTag { IsLocal }
PlayerInputState                          HeroInputState (player + bot shared)
skill_defs.h static table                 SkillRegistry + ISkill
skill_cast.h 521-line switch              skill_cast.h ~200-line dispatcher
_build_players + _build_bots             _build_heroes single loop
snap.players + snap.bots                 snap.heroes
3 sites of `snap.players[0]`             `snap.heroes[local_idx]`
player_* × 4 systems                     combatant_* × 4 (generalized)
bot_combat (parallel to player_attack_fire)  deleted; attack_fire takes over
manual `_trigger_effect` switch          new skill = write one .h + register one line
new hero = copy _spawn_player and edit   new hero = one register_hero() call
```
