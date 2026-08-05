# Bot AI v4 — Three-Layer State Machine + Scored Skill Selection

> Last updated: 2026-08-02
> Current architecture: **v3 (Hero unification) + v4 (three-layer state machine + scored skill selection) — fully implemented**
> Status: ✅ P1–P6 + v4 all complete
> Related: `hero_skill_architecture.md` (Hero + Skill refactor), `sim_api_reference.md` (Sim layer reference), `docs/DATA_FLOW.md` (end-to-end flow).

---

## Table of Contents

1. [Architecture changes](#1-architecture-changes)
2. [Bot position in unified Hero framework](#2-bot-position-in-unified-hero-framework)
3. [Bot-specific components](#3-bot-specific-components)
4. [Bot systems](#4-bot-systems)
5. [Behavior tree — Goal layer](#5-behavior-tree--goal-layer)
6. [Behavior tree — Combat State layer (v4)](#6-behavior-tree--combat-state-layer-v4)
7. [Behavior tree — Skill Decision layer (v4)](#7-behavior-tree--skill-decision-layer-v4)
8. [Bot input injection flow](#8-bot-input-injection-flow)
9. [Bot numerical system](#9-bot-numerical-system)
10. [Tiered respawn system](#10-tiered-respawn-system)
11. [Tick order](#11-tick-order)
12. [File change list](#12-file-change-list)
13. [Risks and mitigations](#13-risks-and-mitigations)
14. [Appendix A — v2 → v3 Goal comparison](#appendix-a--v2--v3-goal-comparison)
15. [Appendix B — v3 → v4 skill decision comparison](#appendix-b--v3--v4-skill-decision-comparison)

---

## 1. Architecture changes

### 1.1 v2 → v3 summary

| Item | v2 (pre-refactor) | v3 (post-refactor) |
|---|---|---|
| Entity type | Independent `BotTag` | `HeroTag{IsLocal=false}`; legacy `BotTag` retained as empty marker |
| Combat system | `bot_combat_system` fires independent arrows | **Deleted**; replaced by generalized `attack_fire_system` |
| Movement system | `bot_ai_system` writes `Position2D` directly | Writes `HeroInputState` → `pathfinding` + `movement` (unified) |
| Skill usage | Not used | Via `bot_skill_decider` + `bot_input_injection` → `HeroInputState` → `skill_cast` |
| Snapshot | `SimBotSnap` separate from `SimPlayerSnap` | Unified `SimHeroSnap` with `is_local` flag |
| Prefab | `bot.tscn` hardcoded | `hero_def_id` → `HeroRegistry` selects prefab |

### 1.2 v3 → v4 summary

| Item | v3 | v4 |
|---|---|---|
| Skill decision | Hardcoded priority (P1–P5) | Three-layer state machine + scored selection |
| Combat state | Only `KiteSub` (Chase/Strafe/Retreat) | New `BotCombatState` (Approach/Kite/Burst/Sustain/Disengage) |
| Skill selection | First-available by priority | Score-based; pick highest |
| Bot vs bot | Supported but unoptimized | Player priority in target selection |
| Decision cadence | Single `DecisionCooldown` (0.3s) | Per-layer: Goal 0.5s / Combat 0.2s / Skill 0.1s |

### 1.3 Bot lifecycle (unified Hero framework)

```
Bot spawn
  ├─ _spawn_bot_with_role(role, level) — creates HeroTag + Bot* components + SkillComponent (from HeroDef)
  ├─ BotAIState / BotBehaviorState init → Wander
  ├─ BotCombatState init → Approach (v4)
  └─ SkillSlot values scaled by Bot skill coefficients

Each tick
  ├─ bot_targeting_system       : select TargetEntity (prefer local player, else min-HP, else min-dist)
  ├─ bot_ai_system              : Goal FSM + respawn roll
  ├─ bot_combat_state_system    : combat FSM (Approach/Kite/Burst/Sustain/Disengage) (v4)
  ├─ bot_skill_decider_system   : scored skill selection → BotCastRequest (v4)
  ├─ bot_input_injection_system : BotAIState + BotCastRequest → HeroInputState
  ├─ attack_command_system      : HeroInputState → AttackTarget (generalized)
  ├─ skill_cast_system          : HeroInputState → ISkill dispatch (generalized)
  └─ pathfinding + movement + attack_fire (all generalized)

Death
  ├─ combat_system sets Dead + RespawnTimer
  └─ bot_ai_system respawn branch: scan role distribution → pick missing role → roll level + tier → refresh stats
```

---

## 2. Bot position in unified Hero framework

### 2.1 Component ownership

| Component | Player Hero | Bot Hero |
|---|---|---|
| `HeroTag { IsLocal }` | `true` | `false` |
| `HeroInputState` | ✅ (from `LocalInputSingleton`) | ✅ (from `bot_input_injection`) |
| `SkillComponent` | ✅ (HeroDef) | ✅ (HeroDef × bot coefficients) |
| `CastState` | ✅ | ✅ (unified `skill_cast` pipeline) |
| `AttackTarget` | ✅ | ✅ (unified `attack_command`) |
| `MovePath` | ✅ | ✅ (unified `pathfinding` + `movement`) |
| `BotAIState` | ❌ | ✅ |
| `BotBehaviorState` | ❌ | ✅ |
| `BotCombatState` | ❌ | ✅ (v4) |
| `BotCastRequest` | ❌ | ✅ (v4) |
| `BotTier` / `BotRole` | ❌ | ✅ |
| `BotVisionRange` | ❌ | ✅ |

### 2.2 Bot components (see `sim_api_reference.md §2.5` for full field tables)

```cpp
struct BotAIState {
    Vec2 MoveTarget{0.0f};
    float RespawnTimer = 0.0f;
    entt::entity TargetEntity = entt::null;
    float WanderTimer = 0.0f;
    float TargetLockTimer = 0.0f;
};

struct BotBehaviorState {
    enum class Goal : uint8_t {
        Flee = 0,
        SeekHeal = 1,
        SeekXp = 2,
        Engage = 3,
        Wander = 4,
    };
    Goal Current = Goal::Wander;
    entt::entity PickupTarget = entt::null;
    float DecisionCooldown = 0.0f;
    enum class KiteSub : uint8_t { Chase, Strafe, Retreat };
    int StrafeDir = 1;
    KiteSub Kite = KiteSub::Strafe;
    float GoalCommitTimer = 0.0f;
};

enum class BotTier : uint8_t { Normal = 0, Elite = 1, Boss = 2 };
enum class BotRole : uint8_t { Fodder = 0, Stalker = 1, Brute = 2 };

struct BotVisionRange { float Value = 0.0f; };

struct BotCombatState {
    enum class Phase : uint8_t {
        Approach = 0, Kite = 1, Burst = 2, Sustain = 3, Disengage = 4,
    };
    Phase Current = Phase::Approach;
    float PhaseTimer = 0.0f;
    int BurstStep = 0;
    float BurstTimer = 0.0f;
};

struct BotCastRequest {
    int TargetSlot = -1;
    Vec2 AimPos{0.0f};
    int TargetNetworkId = -1;
    bool Valid = false;
    float Score = 0.0f;
};
```

---

## 3. Bot-specific components

| Component | Purpose |
|---|---|
| `BotAIState` | Per-tick runtime state (respawn timer / target / wander / lock) |
| `BotBehaviorState` | Goal layer state (Goal / PickupTarget / kiting hysteresis) |
| `BotCombatState` | **v4** — Combat state machine |
| `BotCastRequest` | Skill decision output (with score) |
| `BotTier` | Normal/Elite/Boss — multiplier on stats |
| `BotRole` | Fodder/Stalker/Brute — determines spawn level range |
| `BotVisionRange` | Targeting scan radius |

---

## 4. Bot systems

| System | Status | Responsibility |
|---|---|---|
| `bot_targeting_system` | preserved (v4 tuned) | Vision scan: `is_local Hero first → min-HP → min-dist` |
| `bot_ai_system` | **rewritten (v3)** | Goal FSM + respawn roll. **No longer writes `Position2D` directly.** |
| `bot_combat_state_system` | **v4 new** | Combat state machine (Approach/Kite/Burst/Sustain/Disengage) |
| `bot_skill_decider_system` | **v4 rewrite** | Score-based skill selection → `BotCastRequest` |
| `bot_input_injection_system` | **v3 new** | `BotAIState` + `BotCastRequest` → `HeroInputState` + `MoveTarget` |
| `bot_combat_system` | **deleted** | Superseded by `attack_fire_system` (generalized) |
| `bot_role_rules.h` | preserved | Spawn level / tier distribution per role |

---

## 5. Behavior tree — Goal layer

Inherited from v2 with 5 priority levels. Decision cooldown `DecisionCooldown = 0.5s`; commitment timer `GoalCommitTimer = 0.8s`.

### 5.1 Priorities (high → low)

```
PRIORITY 1: FLEE
  Condition: hp.Cur < hp.Max * 0.3 AND alive enemy in vision
  Action: MoveTarget = away_from_nearest_enemy * BotFleeDist(30)
  Override: always interrupts commitment

PRIORITY 2: SEEK_HEAL
  Condition: hp.Cur < hp.Max * 0.6 AND Heal/SmallHeal pickup exists
  Action: PickupTarget = random top-3 heal → MoveTarget

PRIORITY 3: SEEK_XP
  Condition: no combat target OR target dist > vision
            AND Xp pickup exists
  Action: PickupTarget = random top-3 XP → MoveTarget

PRIORITY 4: ENGAGE (skills — see §6-7)
  Condition: TargetEntity != null AND in vision
  Action:
    ├─ Combat State decision (§6)
    ├─ Skill scoring (§7)
    └─ Movement execution (§6.2)

PRIORITY 5: WANDER
  Condition: nothing to do
  Action: random map point + refresh WanderTimer
```

---

## 6. Behavior tree — Combat State layer (v4)

### 6.1 Overview

In Engage, `bot_combat_state_system` evaluates combat phase every `0.2s`. Phase decides movement strategy and skill preference.

### 6.2 State transition rules

```
┌─────────────────────────────────────────────────────────┐
│              BotCombatState state machine                 │
└─────────────────────────────────────────────────────────┘

Approach ──┬── dist < attack_range * 0.8 ────────→ Kite
           └── hp < 30% && dash_ready ──────────→ Disengage

Kite ──────┬── dist > attack_range * 1.2 ────────→ Approach
           ├── hp > 70% && burst_skills_ready ──→ Burst
           └── hp < 40% ────────────────────────→ Disengage

Burst ─────└── burst_combo_done ─────────────────→ Sustain

Sustain ───┬── hp < 40% ────────────────────────→ Disengage
           └── dist > attack_range * 1.5 ────────→ Approach

Disengage ─┬── dist > safe_distance ────────────→ Approach
           └── hp > 60% ────────────────────────→ Approach
```

### 6.3 Per-phase behavior

| Phase | Movement | Skill preference |
|---|---|---|
| **Approach** | MoveTarget = target.position | Dash (close gap) > control > burst |
| **Kite** | Lateral strafe | Ranged skill > channel > basic attack |
| **Burst** | Hold distance | Burst skill > control > basic attack |
| **Sustain** | Minor distance adjustments | Channel > ranged > basic attack |
| **Disengage** | MoveTarget = away from target | Dash (escape) > control (self-preserve) > basic attack |

### 6.4 `bot_combat_state_system` flow

```cpp
inline void bot_combat_state_system(entt::registry &reg, float dt) {
    auto view = reg.view<HeroTag, BotCombatState, BotBehaviorState,
                         BotAIState, Health, Position2D>();

    for (auto e : view) {
        if (reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled) continue;
        auto &tag = view.get<HeroTag>(e);
        if (tag.IsLocal) continue;  // bots only
        auto &beh = view.get<BotBehaviorState>(e);
        if (beh.Current != BotBehaviorState::Goal::Engage) continue;

        auto &combat = view.get<BotCombatState>(e);
        auto &ai = view.get<BotAIState>(e);
        auto &hp = view.get<Health>(e);
        auto &pos = view.get<Position2D>(e);

        if (ai.TargetEntity == entt::null || !reg.valid(ai.TargetEntity)) continue;
        if (!reg.all_of<Position2D>(ai.TargetEntity)) continue;

        Vec2 to_target = reg.get<Position2D>(ai.TargetEntity).Value - pos.Value;
        float dist = glm::length(to_target);
        float hp_ratio = (float)hp.Cur / (float)hp.Max;
        float attack_range = 8.0f;  // from CombatStats or config

        combat.PhaseTimer -= dt;
        if (combat.PhaseTimer > 0.0f) continue;  // cooldown

        BotCombatState::Phase new_phase = combat.Current;
        bool changed = false;

        switch (combat.Current) {
        case BotCombatState::Phase::Approach:
            if (dist < attack_range * 0.8f) { new_phase = BotCombatState::Phase::Kite; changed = true; }
            else if (hp_ratio < 0.3f && has_dash_ready(e)) { new_phase = BotCombatState::Phase::Disengage; changed = true; }
            break;
        case BotCombatState::Phase::Kite:
            if (dist > attack_range * 1.2f) { new_phase = BotCombatState::Phase::Approach; changed = true; }
            else if (hp_ratio > 0.7f && has_burst_skills_ready(e)) { new_phase = BotCombatState::Phase::Burst; changed = true; }
            else if (hp_ratio < 0.4f) { new_phase = BotCombatState::Phase::Disengage; changed = true; }
            break;
        case BotCombatState::Phase::Burst:
            if (combat.BurstStep >= 3 || combat.BurstTimer <= 0.0f) { new_phase = BotCombatState::Phase::Sustain; changed = true; }
            break;
        case BotCombatState::Phase::Sustain:
            if (hp_ratio < 0.4f) { new_phase = BotCombatState::Phase::Disengage; changed = true; }
            else if (dist > attack_range * 1.5f) { new_phase = BotCombatState::Phase::Approach; changed = true; }
            break;
        case BotCombatState::Phase::Disengage:
            if (dist > attack_range * 2.0f || hp_ratio > 0.6f) { new_phase = BotCombatState::Phase::Approach; changed = true; }
            break;
        }

        if (changed) {
            combat.Current = new_phase;
            combat.PhaseTimer = 0.2f;  // phase switch cooldown
            if (new_phase == BotCombatState::Phase::Burst) {
                combat.BurstStep = 0;
                combat.BurstTimer = 2.0f;
            }
        }
    }
}
```

---

## 7. Behavior tree — Skill Decision layer (v4)

### 7.1 Overview

In Engage, `bot_skill_decider_system` evaluates every `0.1s`. Replaces hardcoded priority with a **scoring function**; picks the highest-scored skill.

### 7.2 Scoring rule

```cpp
struct SkillScore {
    int slot;
    float score;
};

inline float calculate_skill_score(
    const ISkill *sk,
    const SkillSlot &slot,
    float dist,
    float hp_ratio,
    int enemy_count,
    BotCombatState::Phase phase
) {
    float score = 0.0f;
    float range = sk->range(slot.Level);

    score += 50.0f;  // base: skill is available

    if (dist <= range) score += 30.0f;
    else if (dist <= range * 1.5f) score += 10.0f;
    else score -= 20.0f;  // out of range penalty

    switch (phase) {
    case BotCombatState::Phase::Approach:
        if (sk->kind() == SkillKind::Dash) score += 40.0f;
        if (sk->kind() == SkillKind::MeleeSingle) score += 20.0f;
        break;
    case BotCombatState::Phase::Kite:
        if (sk->kind() == SkillKind::AoEField) score += 30.0f;
        if (sk->kind() == SkillKind::ChannelBurst) score += 25.0f;
        break;
    case BotCombatState::Phase::Burst:
        if (sk->kind() == SkillKind::MeleeSingle) score += 50.0f;
        if (sk->kind() == SkillKind::AoEField) score += 40.0f;
        break;
    case BotCombatState::Phase::Sustain:
        if (sk->kind() == SkillKind::ChannelBurst) score += 45.0f;
        if (sk->kind() == SkillKind::AoEField) score += 20.0f;
        break;
    case BotCombatState::Phase::Disengage:
        if (sk->kind() == SkillKind::Dash) score += 60.0f;
        break;
    }

    if (hp_ratio < 0.3f && sk->kind() == SkillKind::Dash) score += 50.0f;
    if (hp_ratio > 0.7f && sk->kind() == SkillKind::ChannelBurst) score += 20.0f;
    if (hp_ratio < 0.5f && sk->kind() == SkillKind::ChannelBurst) score -= 30.0f;

    if (sk->kind() == SkillKind::AoEField && enemy_count >= 2) {
        score += enemy_count * 25.0f;
    }

    return score;
}
```

### 7.3 `bot_skill_decider_system` flow (v4)

```cpp
inline void bot_skill_decider_system(entt::registry &reg, std::mt19937 &rng) {
    auto view = reg.view<
        HeroTag, BotBehaviorState, BotAIState, BotCombatState,
        SkillComponent, Mana, Position2D, Level, Health>();

    for (auto e : view) {
        if (reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled) continue;
        auto &tag = view.get<HeroTag>(e);
        if (tag.IsLocal) continue;
        auto &beh = view.get<BotBehaviorState>(e);
        if (beh.Current != BotBehaviorState::Goal::Engage) continue;

        auto &ai = view.get<BotAIState>(e);
        auto &combat = view.get<BotCombatState>(e);
        auto &skills = view.get<SkillComponent>(e);
        auto &mana = view.get<Mana>(e);
        auto &pos = view.get<Position2D>(e);
        auto &hp = view.get<Health>(e);
        auto &lv = view.get<Level>(e);

        auto &rq = reg.get_or_emplace<BotCastRequest>(e);
        rq.Valid = false;
        rq.Score = 0.0f;

        if (ai.TargetEntity == entt::null || !reg.valid(ai.TargetEntity)) continue;
        if (!reg.all_of<Position2D>(ai.TargetEntity)) continue;

        bool target_alive = !(reg.all_of<Dead>(ai.TargetEntity) &&
                              reg.get<Dead>(ai.TargetEntity).enabled);
        if (!target_alive) continue;

        Vec2 to_target = reg.get<Position2D>(ai.TargetEntity).Value - pos.Value;
        float dist = glm::length(to_target);
        float hp_ratio = (float)hp.Cur / (float)hp.Max;

        const ISkill *sk[4] = {
            SkillRegistry::instance().get(skills.Slots[0].SkillId),
            SkillRegistry::instance().get(skills.Slots[1].SkillId),
            SkillRegistry::instance().get(skills.Slots[2].SkillId),
            SkillRegistry::instance().get(skills.Slots[3].SkillId),
        };

        int enemy_count = 0;
        auto damageable_view = reg.view<Damageable, Position2D>();
        for (auto t : damageable_view) {
            if (t == e) continue;
            if (reg.all_of<Dead>(t) && reg.get<Dead>(t).enabled) continue;
            Vec2 d = damageable_view.get<Position2D>(t).Value - pos.Value;
            if (vec2_length_sq(d) <= 100.0f) { enemy_count++; }  // 10 unit radius
        }

        std::vector<SkillScore> candidates;
        for (int i = 0; i < 4; ++i) {
            if (!sk[i]) continue;
            if (!bot_skill_ready(skills.Slots[i], mana, skills.Slots[i].Level, sk[i])) continue;

            float score = calculate_skill_score(
                sk[i], skills.Slots[i], dist, hp_ratio, enemy_count, combat.Current
            );
            candidates.push_back({i, score});
        }

        if (candidates.empty()) continue;

        std::sort(candidates.begin(), candidates.end(),
                  [](auto &a, auto &b) { return a.score > b.score; });

        auto &best = candidates[0];
        rq.TargetSlot = best.slot;
        rq.Score = best.score;
        rq.Valid = true;

        if (sk[best.slot]->kind() == SkillKind::MeleeSingle) {
            rq.TargetNetworkId = reg.all_of<NetworkId>(ai.TargetEntity)
                ? reg.get<NetworkId>(ai.TargetEntity).Value : -1;
            rq.AimPos = reg.get<Position2D>(ai.TargetEntity).Value;
        } else if (sk[best.slot]->kind() == SkillKind::Dash) {
            Vec2 away_dir = (dist > 0.001f) ? -(to_target / dist) : Vec2{1, 0};
            if (hp_ratio < 0.3f) {
                rq.AimPos = pos.Value + away_dir * sk[best.slot]->range(skills.Slots[best.slot].Level);
            } else {
                rq.AimPos = reg.get<Position2D>(ai.TargetEntity).Value;
            }
            rq.TargetNetworkId = -1;
        } else if (sk[best.slot]->kind() == SkillKind::AoEField) {
            Vec2 sum_pos{0, 0};
            int count = 0;
            for (auto t : damageable_view) {
                if (t == e) continue;
                if (reg.all_of<Dead>(t) && reg.get<Dead>(t).enabled) continue;
                Vec2 d = damageable_view.get<Position2D>(t).Value - pos.Value;
                if (vec2_length_sq(d) <= sk[best.slot]->range(skills.Slots[best.slot].Level) *
                                         sk[best.slot]->range(skills.Slots[best.slot].Level)) {
                    sum_pos = sum_pos + damageable_view.get<Position2D>(t).Value;
                    count++;
                }
            }
            rq.AimPos = (count > 0) ? sum_pos / (float)count : pos.Value;
            rq.TargetNetworkId = -1;
        } else {
            rq.AimPos = {0, 0};
            rq.TargetNetworkId = -1;
        }
    }
}
```

---

## 8. Bot input injection flow

`bot_input_injection_system` translates `BotAIState` (movement target) and `BotCastRequest` (skill intent) into `HeroInputState`, so downstream systems don't need to distinguish player vs bot.

```cpp
inline void bot_input_injection_system(entt::registry &reg) {
    auto view = reg.view<HeroTag, HeroInputState, BotAIState, BotBehaviorState>();

    for (auto e : view) {
        if (reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled) continue;
        auto &tag = view.get<HeroTag>(e);
        if (tag.IsLocal) continue;  // don't overwrite player input

        auto &input = view.get<HeroInputState>(e);
        auto &ai = view.get<BotAIState>(e);
        auto &beh = view.get<BotBehaviorState>(e);

        // Movement
        input.MoveTarget = ai.MoveTarget;
        input.MoveIssue = true;
        input.Stop = false;

        // Skill
        if (reg.all_of<BotCastRequest>(e)) {
            auto &rq = reg.get<BotCastRequest>(e);
            if (rq.Valid && rq.TargetSlot >= 0 && rq.TargetSlot < 4) {
                input.SkillSlot = rq.TargetSlot;
                input.SkillConfirm = true;
                input.SkillAim = rq.AimPos;
                input.SkillTargetId = rq.TargetNetworkId;
            } else {
                input.SkillSlot = -1;
                input.SkillConfirm = false;
            }
            rq.Valid = false;
            rq.TargetSlot = -1;
        }

        // Basic attack (only if no skill this tick and an alive target)
        if (input.SkillSlot < 0 && ai.TargetEntity != entt::null &&
            reg.valid(ai.TargetEntity)) {
            int net_id = reg.all_of<NetworkId>(ai.TargetEntity)
                ? reg.get<NetworkId>(ai.TargetEntity).Value : -1;
            if (net_id > 0) {
                input.AttackTargetId = net_id;
                input.AttackClear = false;
            }
        } else {
            input.AttackClear = true;
            input.AttackTargetId = -1;
        }

        // Bots don't upgrade skills
        input.SkillUpgradeSlot = -1;
        input.CancelSkill = false;
        input.CancelAttack = false;
        input.AttackGround = false;
    }
}
```

---

## 9. Bot numerical system

### 9.1 Base stats (vs player)

| Stat | Bot | Player | Ratio |
|---|---|---|---|
| BaseHp | 50 | 100 | 50% |
| BaseAttack | 5.0 | 10.0 | 50% |
| BaseAttackSpeed | 0.8 | 1.0 | 80% |
| BaseMoveSpeed | 2.0 | 5.0 | 40% |
| BaseMana | 80.0 | 300.0 | 27% |

### 9.2 Bot skill coefficients

| Coefficient | Default | Applied at | Where |
|---|---|---|---|
| `BotSkillDmgMul` | 0.7 | In each `ISkill::damage()` (caster has `BotTag`) | `skills/*.h` / `skill_cast.h` |
| `BotSkillCooldownMul` | 1.3 | Spawn time: `SkillSlot.MaxCooldown *= …` | `_spawn_bot_with_role` in `world_spawn.cpp` |
| `BotManaCostMul` | 0.6 | `bot_skill_ready`: `effective_cost = sk->mana_cost(level) * BotManaCostMul` | `bot_skill_decider.h` |

Bots use the same `HeroDef` as the player; the coefficients make the bot version automatically weaker.

### 9.3 Tier multipliers (preserved from v2)

| Tier | HpMul | AtkMul | AspMul | SpeedMul | VisionMul |
|---|---|---|---|---|---|
| Normal | 1.0 | 1.0 | 1.0 | 1.0 | 1.0 |
| Elite | 2.0 | 1.6 | 1.1 | 1.1 | 1.2 |
| Boss | 4.0 | 2.5 | 1.25 | 1.2 | 1.5 |

### 9.4 Level scaling (bot vs player)

| Stat | Player (HeroDef) | Bot (over base × Tier) |
|---|---|---|
| HpPerLevel | 10.0 | 8 (`BotHpPerLevel`) |
| AtkPerLevel | 1.0 | 0.8 |
| AspPerLevel | 0.03 | 0.03 |
| SpeedPerLevel | 0.5 | 0.3 |

Combined with Tier penalties and skill coefficients, a same-level bot is ~50–70% the threat of a same-level player.

---

## 10. Tiered respawn system

Inherited from v2 §11.

### 10.1 Three bot roles

| Role | Level range | Tier distribution | Weight |
|---|---|---|---|
| **Fodder** | 1–`FodderMaxLv(5)` | 100% Normal | 4 |
| **Stalker** | player_lv ± 2 | 5% Boss / 15% Elite / 80% Normal | 4 |
| **Brute** | `BruteMinLv(22)`–`MaxBotLevel(30)` | 60% Elite / 40% Boss | 2 |

### 10.2 Respawn algorithm

```
bot_ai_system respawn branch (dead → respawn):
  step 1: scan alive bot Role distribution → counts[3]
  step 2: density[role] = counts[role] / weight[role]
          pick the role with lowest density
  step 3: roll level by Role (§10.1)
  step 4: roll tier by Role (§10.1)
  step 5: apply stats: Hp/Atk/Asp/Speed/Vision = (base + level_growth) × Tier
  step 6: refresh skill slots: SkillId stays from HeroDef;
          MaxCooldown rescaled by BotSkillCooldownMul
  step 7: random respawn position + reset AI state
```

### 10.3 First spawn

`World::initialize` calls `_spawn_bot_with_role(...)` `BotCount` times; each does a weight-based role roll. All bots share one `HeroDefId` (currently Swordsman).

---

## 11. Tick order

See `world.cpp::tick` for the canonical 22-system pipeline. Bot systems are steps 2–6:

```
# Bot AI phase (5 systems)
1. (no-op for bots) local_input_injection_system   — only HeroTag.IsLocal
2. bot_targeting_system                            — pick TargetEntity (prefer local player)
3. bot_ai_system                                   — Goal FSM + respawn roll
4. bot_combat_state_system                         — combat phase FSM (v4)
5. bot_skill_decider_system                        — score skills → BotCastRequest (v4)
6. bot_input_injection_system                      — BotAIState + BotCastRequest → HeroInputState

# Unified combat phase (treats all HeroTag, player or AI)
7. attack_command_system
8. skill_cast_system
9. pathfinding_system
10. movement_system
11. attack_fire_system

# Physics & game systems
12. arrow_movement
13. wall_collision
14. combat
15. pickup
16. aoe
17. status_effect
18. mana_regen
19. skill_cooldown
20. skill_level
21. progression
22. snapshot_export
```

Full sequence diagram: see `docs/DATA_FLOW.md §3`.

---

## 12. File change list

### v4 changes

| File | Change |
|---|---|
| `src_cpp/sim/components.h` | Add `BotCombatState` (v4); extend `BotCastRequest` with `Score` |
| `src_cpp/sim/systems/bot_targeting.h` | Tune target priority: prefer local player |
| `src_cpp/sim/systems/bot_skill_decider.h` | Rewrite as score-based selection |
| `src_cpp/sim/systems/bot_combat_state.h` | **New** — combat phase FSM |
| `src_cpp/sim/game_config.h` | Add combat FSM constants |

### v3 changes (preserved)

| File | Change |
|---|---|
| `src_cpp/sim/systems/bot_ai.h` | Split: keep Goal FSM + respawn; **remove** direct `Position2D` writes; movement execution moves to `bot_input_injection` + unified systems |
| `src_cpp/sim/systems/bot_combat.h` | **Deleted** |
| `src_cpp/sim/systems/bot_skill_decider.h` | **New (v3)** — Engage subtree skill selection; further rewritten in v4 |
| `src_cpp/sim/systems/bot_input_injection.h` | **New (v3)** — BotAIState + BotCastRequest → HeroInputState |
| `src_cpp/sim/game_config.h` | Add 3 bot skill coefficient constants |

### Derived from Hero unification

After `bot_ai_system` was split, code that previously wrote `Position2D` directly had to be removed:

| Old behavior | New behavior |
|---|---|
| `pos.Value = pos + dir * speed * dt` | `bot_input_injection` writes `input.MoveTarget` + `input.MoveIssue = true` |
| `angle.Radians = atan2(dir.y, dir.x)` | `movement_system` smooths orientation uniformly |
| `ai.MoveTarget` as current position | Used only as Goal-decided target; never directly for motion |
| Wander directly mutates `pos.Value` | `ai.MoveTarget` randomly refreshed; `pathfinding` computes path |

### Untouched

| File | Reason |
|---|---|
| `bot_role_rules.h` | Logic unchanged |
| `wall_collision.h` / `arrow_movement.h` / `combat.h` | Generic |
| `pickup.h` / `aoe.h` / `status_effect.h` / `mana_regen.h` / `skill_cooldown.h` / `progression.h` | Generic |
| `snapshot_types/` / `snapshot_builder.cpp` / `snapshot_bindings.cpp` | Covered by Hero unification |
| `world.cpp` / `world.h` | Covered by Hero unification |

---

## 13. Risks and mitigations

| Risk | Description | Mitigation |
|---|---|---|
| Bot skill decision 0.1s lag may miss window | Skill confirm waits for next `DecisionCooldown` | 0.1s is fast enough; can lower to 0.05s if needed |
| Goal switch while in `CastState` | Engage → Flee mid-cast | Any cast cancellation discards pending resources; no refund path is needed |
| Attack vs skill conflict | Bot may set `SkillConfirm = true` and `AttackTargetId` together | `bot_input_injection` makes them mutually exclusive: with skill → `AttackTargetId = -1`; without skill → `SkillSlot = -1` |
| Bot killed during Channeling | Channeling uninterruptible | Designed high-risk / high-reward for R skill; `bot_skill_decider` filters `hp_ratio` |
| HeroInputState overwritten by bot | Same bot's `HeroInputState` only written in one place | `bot_input_injection` is the only writer (does not go through `local_input_injection`); no race |
| Pathfinding to dead target | Bot chasing dead enemy's last position | `bot_targeting` refreshes `TargetEntity` each decision; death-check guards |
| Combat FSM thrashing | Phase switches too often | 0.2s phase switch cooldown |
| Scoring computation cost | All 4 skills scored every 0.1s | Only computed in Engage; max 4 skills |

---

## Appendix A — v2 → v3 Goal comparison

| Goal | v2 behavior | v3 behavior |
|---|---|---|
| Flee | Direct `pos` write away from enemy | Write `HeroInputState.MoveTarget` → unified movement |
| SeekHeal | Direct `pos` write to heal | Same + `pathfinding` A* |
| SeekXp | Direct `pos` write to XP | Same + `pathfinding` A* |
| Engage | Kiting directly writes `pos` + `angle`; fires basic arrows | Kiting writes `HeroInputState`; skill via `SkillSlot/Confirm/Aim`; basic attack via `AttackTargetId` → unified `attack_fire` |
| Wander | Random `pos` write | Random `ai.MoveTarget` → `pathfinding` A* |

---

## Appendix B — v3 → v4 skill decision comparison

| Item | v3 | v4 |
|---|---|---|
| Decision model | Hardcoded priority (P1–P5) | Three-layer state machine + scoring |
| Skill selection | First-available by priority check | Score over distance/HP/phase/enemy count |
| Combat state | Only `KiteSub` | `BotCombatState` (5 phases) |
| Movement strategy | Fixed kiting | Dynamic per phase |
| Bot vs bot | Supported but unoptimized | Player priority in target selection |
| Decision cadence | 0.3s uniform | Per-layer: Goal 0.5s / Combat 0.2s / Skill 0.1s |
