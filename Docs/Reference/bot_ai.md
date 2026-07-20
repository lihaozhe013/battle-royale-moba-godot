# Bot AI v3 — Hero 框架下的 AI 行为树

> 最后更新：2026-07-20
> 当前方案：v3 — Hero 统一化架构
> 关联：`Docs/Reference/hero_skill_architecture.md`（Hero + Skill 重构）、`Docs/Reference/sim_system_reference.md`（C++ 层组件/系统/快照参考）

---

## 目录

1. [架构变更](#1-架构变更)
2. [Bot 在统一 Hero 框架中的位置](#2-bot-在统一-hero-框架中的位置)
3. [组件清单（Bot 专用）](#3-组件清单bot-专用)
4. [系统清单](#4-系统清单)
5. [行为树 — Goal 层](#5-行为树--goal-层)
6. [行为树 — Engage 技能子决策](#6-行为树--engage-技能子决策)
7. [Bot 输入注入流程](#7-bot-输入注入流程)
8. [Bot 数值系统](#8-bot-数值系统)
9. [阶梯重生系统](#9-阶梯重生系统)
10. [Tick 顺序](#10-tick-顺序)
11. [文件改动清单](#11-文件改动清单)
12. [风险与对策](#12-风险与对策)

---

## 1. 架构变更

### 1.1 v2 → v3 变更摘要

| 项 | v2（现有） | v3（重构后） |
|---|-----------|-------------|
| 实体类型 | `BotTag` 独立 | `HeroTag{IsLocal=false}` + 保留 `BotAIState` 等 AI 组件 |
| 战斗系统 | `bot_combat_system` 独立射普攻箭 | **删除**；`attack_fire_system` 泛化后统一处理 |
| 移动系统 | `bot_ai_system` 直接写 `Position2D` | 写 `HeroInputState` → `pathfinding` + `movement` 统一处理 |
| 技能使用 | **不使用** | 通过 `bot_skill_decider` + `bot_input_injection` 写 `HeroInputState` → `skill_cast` 统一处理 |
| Snapshot | `SimBotSnap` 独立 | `SimHeroSnap` 统一，通过 `is_local` 字段区分 |
| Prefab | bot.tscn 硬编码 | `hero_def_id` → HeroRegistry 选 prefab |

### 1.2 统一后的 Bot 生命周期

```
Bot 出生
  ├─ _spawn_bot_hero()：创建 HeroTag + BotAI 组件 + SkillComponent（按 HeroDef 查表）
  ├─ BotAIState / BotBehaviorState 初始化为 Wander
  └─ SkillSlot 按 Bot 技能系数缩放

每 tick
  ├─ bot_targeting_system   : 选 TargetEntity（对所有 Damageable）
  ├─ bot_ai_system          : Goal 决策 + 复活等级 roll
  ├─ bot_skill_decider      : Engage 时选技能 → BotCastRequest
  ├─ bot_input_injection    : BotAIState + BotCastRequest → HeroInputState
  ├─ attack_command         : HeroInputState → AttackTarget（泛化）
  ├─ skill_cast             : HeroInputState → ISkill 调度（泛化）
  └─ pathfinding + movement + attack_fire（全泛化）

死亡
  ├─ combat_system 设 Dead，设 RespawnTimer（player = 淘汰，bot = 重生）
  └─ bot_ai_system 复活段：扫描 role 分布 → 选缺失区间 → roll 等级+Tier → 刷新属性
```

---

## 2. Bot 在统一 Hero 框架中的位置

### 2.1 组件归属

| 组件 | Player Hero | Bot Hero |
|------|-------------|----------|
| `HeroTag { IsLocal }` | `true` | `false` |
| `HeroInputState` | ✅（由 LocalInputSingleton 注入） | ✅（由 `bot_input_injection` 填充） |
| `SkillComponent` | ✅（HeroDef） | ✅（HeroDef × Bot 系数） |
| `CastState` | ✅ | ✅（统一 skill_cast 处理） |
| `AttackTarget` | ✅ | ✅（统一 attack_command 处理） |
| `MovePath` | ✅ | ✅（统一 pathfinding + movement 处理） |
| `BotAIState` | ❌ | ✅ |
| `BotBehaviorState` | ❌ | ✅ |
| `BotTier` / `BotRole` | ❌ | ✅ |
| `BotVisionRange` | ❌ | ✅ |

### 2.2 Bot 组件详解（保留自 v2）

```cpp
// components.h
struct BotAIState {
    Vec2 MoveTarget{0.0f};
    float RespawnTimer = 0.0f;
    entt::entity TargetEntity = entt::null;
    float WanderTimer = 0.0f;
    float TargetLockTimer = 0.0f;
};

struct BotBehaviorState {
    enum class Goal : uint8_t {
        Flee = 0,      // 逃跑
        SeekHeal = 1,  // 找血包
        SeekXp = 2,    // 找经验
        Engage = 3,    // 战斗（含技能）
        Wander = 4,    // 随机游走
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
```

### 2.3 新增组件（BotCastRequest）

```cpp
// 临时意图：bot_skill_decider 写入，bot_input_injection 消费后清除
struct BotCastRequest {
    int TargetSlot = -1;        // 0-3, -1=不施法
    Vec2 AimPos{0.0f};
    int TargetNetworkId = -1;   // 指向性技能目标
    bool Valid = false;
};
```

---

## 3. 组件清单（Bot 专用）

| 组件 | 说明 |
|------|------|
| `BotAIState` | 运行时状态（respawn timer/目标/wander timer） |
| `BotBehaviorState` | 决策状态（Goal/PickupTarget/Kiting 滞回） |
| `BotTier` | Normal/Elite/Boss，影响属性倍率 |
| `BotRole` | Fodder/Stalker/Brute，影响等级范围 |
| `BotVisionRange` | 视野半径 |
| `BotCastRequest` | **新增**：Engage 子树的技能选择意图 |

---

## 4. 系统清单

| System | 保留/新增 | 职责 |
|--------|-----------|------|
| `bot_targeting_system` | 保留 | 视野内按 `min HP → min dist → random` 选目标。不变。 |
| `bot_ai_system` | **重写** | Goal 决策 + 复活 roll。**不再写 Position2D**。 |
| `bot_skill_decider_system` | **新增** | Engage 时按优先级选技能 → `BotCastRequest` |
| `bot_input_injection_system` | **新增** | BotAIState + BotCastRequest → `HeroInputState` + `MoveTarget` |
| `bot_combat_system` | **删除** | 被 `attack_fire_system` 统一接管 |

---

## 5. 行为树 — Goal 层

沿用 v2 的 5 种 Goal 优先级体系，保留决策冷却 (`DecisionCooldown = 0.3s`) 和承诺计时 (`GoalCommitTimer = 0.8s`)。

### 5.1 优先级（高→低）

```
PRIORITY 1: FLEE
  条件: hp.Cur < hp.Max * 0.3 且 视野内有 alive 敌人
  动作: MoveTarget = 远离最近敌人的方向 × BotFleeDist(30)
  打断: 永远可打断承诺

PRIORITY 2: SEEK_HEAL
  条件: hp.Cur < hp.Max * 0.6 且 地图上有 Heal/SmallHeal pickup
  动作: PickupTarget = top-3 随机一个血包实体 → MoveTarget

PRIORITY 3: SEEK_XP
  条件: 无战斗目标 或 目标距离 > vision
        且 地图上有 Xp pickup
  动作: PickupTarget = top-3 随机一个 XP 实体 → MoveTarget

PRIORITY 4: ENGAGE（含技能 — 见 §6）
  条件: TargetEntity != null 且在视野内
  动作: 
    ├─ 技能子决策（per §6）
    └─ Kiting 移动（per §5.2）

PRIORITY 5: WANDER
  条件: 无事可做
  动作: 随机地图点 + WanderTimer 刷新
```

### 5.2 Kiting 滞回状态机（Engage 移动，不变）

| 状态 | 进入条件 | 行为 |
|------|----------|------|
| Chase | dist > vision × 0.85 | MoveTarget = target.position |
| Strafe | 中间距离 | 垂直方向横移（Direction 每 DecisionCooldown 随机） |
| Retreat | dist < vision × 0.25 | MoveTarget = pos - dir × 20（撤到安全距离） |

移动执行不再直接算 `Position2D`，而是：
1. `bot_input_injection` 将 Kiting 计算结果写入 `HeroInputState`
2. `HeroInputState.MoveTarget` + `MoveIssue=true`
3. `pathfinding` / `movement` 统一处理

---

## 6. 行为树 — Engage 技能子决策

### 6.1 概览

Engage 状态下，每 `DecisionCooldown=0.3s` 评估一次技能可用性。选中的技能通过 `BotCastRequest` 传递给 `bot_input_injection`，最终写入 `HeroInputState.SkillSlot/Confirm/Aim/TargetId`。

### 6.2 技能优先级规则

```
Engage 技能选择（逐级检查，只选第一个可用的）
├── [P1: 逃生 Dash]
│   条件: hp.Cur < hp.Max * 0.3  且 最近敌人在 Dash 范围内
│         Dash 技能 CD=0 且 Mana 足够
│   动作: BotCastRequest{Slot=Dash_index, Confirm=true,
│          Aim=pos + away_dir * dash_range, TargetId=-1}

├── [P2: 群控 AoE]
│   条件: 2+ 敌人在 AoE 技能范围内
│         AoE 技能 CD=0 且 Mana 足够
│   动作: BotCastRequest{Slot=AoE_index, Confirm=true,
│          Aim=敌人簇中心, TargetId=-1}

├── [P3: 爆发 MeleeSingle]
│   条件: 锁定目标 (ai.TargetEntity) 在 MeleeSingle 范围内
│         或距离 < vision * 0.6（可 Chasing 追击）
│         MeleeSingle 技能 CD=0 且 Mana 足够
│   动作: BotCastRequest{Slot=MeleeSingle_index, Confirm=true,
│          TargetId=target_net_id}

├── [P4: 持续 ChannelBurst]
│   条件: hp.Cur > hp.Max * 0.5（低血量时避免站桩）
│         目标在 ChannelBurst 射程内
│         ChannelBurst 技能 CD=0 且 Mana 足够
│   动作: BotCastRequest{Slot=ChannelBurst_index, Confirm=true}

└── [P5: 普攻] — 无可用技能
    动作: BotCastRequest 留空；bot_input_injection 写入 AttackTargetId
          attack_fire_system 自动射 homing 箭
```

### 6.3 技能可用性检查函数

```cpp
inline bool bot_skill_ready(const SkillSlot &s, const Mana &m, int level,
                             const ISkill *sk) {
    if (!sk) return false;
    if (s.CooldownTimer > 0.0f) return false;
    float effective_cost = sk->mana_cost(level) * GameConfig::BotManaCostMul;
    if (m.Cur < effective_cost) return false;
    return true;
}

inline bool bot_target_in_range(entt::registry &reg, entt::entity caster,
                                 entt::entity target, const ISkill *sk, int level) {
    if (!reg.valid(target)) return false;
    Vec2 delta = reg.get<Position2D>(target).Value -
                 reg.get<Position2D>(caster).Value;
    return vec2_length_sq(delta) <= sk->range(level) * sk->range(level);
}
```

### 6.4 bot_skill_decider_system 流程

```cpp
inline void bot_skill_decider_system(entt::registry &reg, std::mt19937 &rng) {
    auto view = reg.view<
        BotTag, BotBehaviorState, BotAIState, SkillComponent,
        Mana, Position2D, Level>();

    for (auto e : view) {
        if (reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled) continue;
        auto &beh = view.get<BotBehaviorState>(e);
        if (beh.Current != BotBehaviorState::Goal::Engage) continue;

        auto &ai = view.get<BotAIState>(e);
        auto &skills = view.get<SkillComponent>(e);
        auto &mana = view.get<Mana>(e);
        auto &pos = view.get<Position2D>(e);
        auto &lv = view.get<Level>(e);

        auto &rq = reg.get_or_emplace<BotCastRequest>(e);
        rq.Valid = false; // 默认不施法

        if (ai.TargetEntity == entt::null || !reg.valid(ai.TargetEntity)) continue;

        bool target_alive = !(reg.all_of<Dead>(ai.TargetEntity) &&
                              reg.get<Dead>(ai.TargetEntity).enabled);
        if (!target_alive) continue;

        // 收集每个槽的 ISkill 指针
        const ISkill *sk[4] = {
            SkillRegistry::instance().get(skills.Slots[0].SkillId),
            SkillRegistry::instance().get(skills.Slots[1].SkillId),
            SkillRegistry::instance().get(skills.Slots[2].SkillId),
            SkillRegistry::instance().get(skills.Slots[3].SkillId),
        };

        Vec2 to_target = reg.get<Position2D>(ai.TargetEntity).Value - pos.Value;
        float dist = glm::length(to_target);
        Vec2 away_dir = (dist > 0.001f) ? -(to_target / dist) : Vec2{1,0};

        float hp_ratio = (float)reg.get<Health>(e).Cur /
                         (float)reg.get<Health>(e).Max;

        float effective_range[4];
        for (int i = 0; i < 4; ++i) {
            effective_range[i] = sk[i] ? sk[i]->range(skills.Slots[i].Level) : 0.0f;
        }

        // ── P1: Dash 逃生 ──
        if (hp_ratio < 0.3f && dist < effective_range[2] * 1.5f) {
            if (bot_skill_ready(skills.Slots[2], mana, skills.Slots[2].Level, sk[2])) {
                rq.TargetSlot = 2;
                rq.AimPos = pos.Value + away_dir * effective_range[2];
                rq.TargetNetworkId = -1;
                rq.Valid = true;
                continue;
            }
        }

        // ── P2: AoE 群控 ──
        if (sk[1] && bot_skill_ready(skills.Slots[1], mana, skills.Slots[1].Level, sk[1])) {
            int enemy_count = 0;
            Vec2 sum_pos{0,0};
            // 扫描 enemies in AoE range
            auto damageable_view = reg.view<Damageable, Position2D>();
            for (auto t : damageable_view) {
                if (t == e) continue;
                if (reg.all_of<Dead>(t) && reg.get<Dead>(t).enabled) continue;
                Vec2 d = damageable_view.get<Position2D>(t).Value - pos.Value;
                if (vec2_length_sq(d) <= effective_range[1] * effective_range[1]) {
                    enemy_count++;
                    sum_pos = sum_pos + damageable_view.get<Position2D>(t).Value;
                }
            }
            if (enemy_count >= 2) {
                rq.TargetSlot = 1;
                rq.AimPos = sum_pos / (float)enemy_count;
                rq.TargetNetworkId = -1;
                rq.Valid = true;
                continue;
            }
        }

        // ── P3: MeleeSingle 爆发 ──
        if (sk[0] && bot_skill_ready(skills.Slots[0], mana, skills.Slots[0].Level, sk[0])) {
            int target_net = reg.all_of<NetworkId>(ai.TargetEntity)
                ? reg.get<NetworkId>(ai.TargetEntity).Value : -1;
            rq.TargetSlot = 0;
            rq.TargetNetworkId = target_net;
            rq.AimPos = reg.get<Position2D>(ai.TargetEntity).Value;
            rq.Valid = true;
            continue;
        }

        // ── P4: ChannelBurst 持续 ──
        if (hp_ratio > 0.5f && sk[3] &&
            bot_skill_ready(skills.Slots[3], mana, skills.Slots[3].Level, sk[3]) &&
            dist < effective_range[3] * 1.2f) {
            rq.TargetSlot = 3;
            rq.TargetNetworkId = -1;
            rq.AimPos = {0,0};
            rq.Valid = true;
            continue;
        }

        // P5: 啥也不选 → 普攻
        rq.Valid = false;
    }
}
```

---

## 7. Bot 输入注入流程

`bot_input_injection_system` 将 `BotAIState`（移动目标）和 `BotCastRequest`（技能意图）翻译为 `HeroInputState`，使后续 System 无需区分玩家/Bot。

```cpp
inline void bot_input_injection_system(entt::registry &reg) {
    auto view = reg.view<
        HeroTag, HeroInputState, BotAIState, BotBehaviorState>();

    for (auto e : view) {
        if (reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled) continue;
        auto &tag = view.get<HeroTag>(e);
        if (tag.IsLocal) continue; // 不覆盖玩家输入

        auto &input = view.get<HeroInputState>(e);
        auto &ai = view.get<BotAIState>(e);
        auto &beh = view.get<BotBehaviorState>(e);

        // ── 移动 ──
        input.MoveTarget = ai.MoveTarget;
        input.MoveIssue = true;
        input.Stop = false;

        // ── 技能 ──
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

            // 清除 BotCastRequest（一次性消费）
            rq.Valid = false;
            rq.TargetSlot = -1;
        }

        // ── 普攻 ──
        if (input.SkillSlot < 0 && ai.TargetEntity != entt::null &&
            reg.valid(ai.TargetEntity)) {
            int net_id = reg.all_of<NetworkId>(ai.TargetEntity)
                ? reg.get<NetworkId>(ai.TargetEntity).Value : -1;
            if (net_id > 0) {
                input.AttackTargetId = net_id;
                input.AttackClear = false;
            }
        } else {
            // 非 Engage 状态或技能施放中，清空攻击目标
            input.AttackClear = true;
            input.AttackTargetId = -1;
        }

        // ── 升级（Bot 不升级技能，全由 System auto） ──
        input.SkillUpgradeSlot = -1;
        input.CancelSkill = false;
        input.CancelAttack = false;
        input.AttackGround = false;
    }
}
```

---

## 8. Bot 数值系统

### 8.1 基础属性（沿用 v2，已低于玩家）

| 属性 | Bot | Player | 比值 |
|------|-----|--------|------|
| BaseHp | 50 | 100 | 50% |
| BaseAttack | 5.0 | 10.0 | 50% |
| BaseAttackSpeed | 0.8 | 1.0 | 80% |
| BaseMoveSpeed | 2.0 | 5.0 | 40% |
| BaseMana | 80.0 | 300.0 | 27% |

### 8.2 技能系数（新增，此处定义）

```cpp
// game_config.h

// Bot 技能系数（与 HeroDef 中的基础值乘算）
static constexpr float BotSkillDmgMul = 0.7f;        // 技能伤害 ×0.7
static constexpr float BotSkillCooldownMul = 1.3f;    // 技能冷却 ×1.3
static constexpr float BotManaCostMul = 0.6f;          // 蓝耗 ×0.6
```

**应用方式**：

| 系数 | 应用时机 | 位置 |
|------|----------|------|
| `BotSkillCooldownMul` | Hero 初始化时 `SkillSlot.MaxCooldown *= BotSkillCooldownMul` | `_spawn_bot_hero` in `world.cpp` |
| `BotManaCostMul` | `bot_skill_ready` 中 `effective_cost = sk->mana_cost(level) * BotManaCostMul` | `bot_skill_decider.h` |
| `BotSkillDmgMul` | 各 ISkill 的 `damage()` 实现中，检测 caster 是否有 `BotTag` | `skills/*.h` 或 `skill_cast.h` 统一乘 |

> Bot 不是每种英雄都有，通过 Bot 技能系数让同一 HeroDef 的 Bot 版本自动弱于玩家版本。

### 8.3 Tier 倍率（保留 v2）

| Tier | HpMul | AtkMul | AspMul | SpeedMul | VisionMul |
|------|-------|--------|--------|----------|-----------|
| Normal | 1.0 | 1.0 | 1.0 | 1.0 | 1.0 |
| Elite | 2.0 | 1.6 | 1.1 | 1.1 | 1.2 |
| Boss | 4.0 | 2.5 | 1.25 | 1.2 | 1.5 |

Tier 与 BotRole 联动（§9）。

### 8.4 等级成长（Bot vs Player）

| 属性 | Player（HeroDef 默认） | Bot（在 Bot 基础值上 × Tier） |
|------|----------------------|-------------------------------|
| HpPerLevel | 10.0 | 8（`BotHpPerLevel`）|
| AtkPerLevel | 1.0 | 0.8 |
| AspPerLevel | 0.03 | 0.03（同玩家）|
| SpeedPerLevel | 0.5 | 0.3 |

> Bot 成长速率低于玩家，加上 Tier 惩罚和技能系数，同一等级下 Bot 的威胁约为同等级玩家的 50%-70%。

---

## 9. 阶梯重生系统

沿用 v2 §11 的设计，基本不变。

### 9.1 三类 Bot Role

| Role | 等级范围 | Tier 分布 | 权重 |
|------|----------|-----------|------|
| **Fodder（炮灰）** | 1~FodderMaxLv(5) | 100% Normal | 4 |
| **Stalker（追猎者）** | player_lv ± 2 | 5% Boss / 15% Elite / 80% Normal | 4 |
| **Brute（重型）** | BruteMinLv(22)~MaxBotLevel(30) | 60% Elite / 40% Boss | 2 |

### 9.2 复活段算法

```
bot_ai_system 复活分支（dead → respawn）:
  step 1: 扫描所有 alive bot 的 Role 分布 counts[3]
  step 2: density[role] = counts[role] / weight[role]
          选 density 最低的 Role
  step 3: 按 Role roll 等级（§9.1 规则）
  step 4: 按 Role roll Tier（§9.1 规则）
  step 5: 应用属性：Hp/Atk/Asp/Speed/Vision = (base + level_growth) × Tier
  step 6: 刷新技能槽：SkillId 保持 HeroDef 定义，
          MaxCooldown 重新按 BotSkillCooldownMul 缩放
  step 7: 复活位置随机 + 重置 AI 状态
```

### 9.3 首次出生

`World::initialize` 中 `_spawn_bot_hero()` × `BotCount` 次，每个执行 weight-based role roll（同上）。所有 Bot 都用同一 `HeroDefId`（当前默认 Swordsman）。

---

## 10. Tick 顺序

与 Hero 统一后一致（详见 `hero_skill_architecture.md §9`）：

```
# Bot AI 阶段（三个独立 System）
1. bot_targeting_system       — 选 TargetEntity
2. bot_ai_system              — Goal 决策 + 复活 roll（改写 BotAIState）
3. bot_skill_decider_system   — Engage 技能选择 → BotCastRequest（新增）
4. bot_input_injection_system — BotAIState + BotCastRequest → HeroInputState（新增）

# 统一战斗阶段（与玩家相同，泛化后同时处理所有 HeroTag）
5. attack_command_system
6. skill_cast_system
7. pathfinding_system
8. movement_system
9. attack_fire_system

# 物理 & 游戏系统
10. arrow_movement → wall_collision → combat → pickup → aoe → status → mana_regen → skill_cooldown → skill_level → progression → snapshot
```

---

## 11. 文件改动清单

### 修改

| 文件 | 改动 |
|------|------|
| `src_cpp/sim/systems/bot_ai.h` | 拆分：保留 Goal 决策 + 复活 roll；**删除** 扫描 pickup / 移动执行 / Kiting 移动部分（移到 bot_input_injection + 泛化 System） |
| `src_cpp/sim/systems/bot_combat.h` | **删除** |
| `src_cpp/sim/game_config.h` | 新增 3 个 Bot 技能系数常量 |

### 新增

| 文件 | 归属 | 职责 |
|------|------|------|
| `src_cpp/sim/systems/bot_skill_decider.h` | P4 | Engage 子树技能选择，写 BotCastRequest |
| `src_cpp/sim/systems/bot_input_injection.h` | P4 | BotAIState + BotCastRequest → HeroInputState |

### 关联改动（来自 Hero 统一化）

涉及 `bot_ai_system` 拆分后，旧代码中直接写 `Position2D` 的部分需要移除。具体：

| 旧行为 | 新行为 |
|--------|--------|
| `pos.Value = pos + dir * speed * dt` | `bot_input_injection` 写 `input.MoveTarget` + `input.MoveIssue=true` |
| `angle.Radians = atan2(dir.y, dir.x)` | `movement` 系统统一平滑朝向 |
| `ai.MoveTarget` 作为"直接当前位置" | 仅作为 Goal 决定的目标位置，不直接用于移动计算 |
| Wander 直接改 `pos.Value` | `ai.MoveTarget` 随机刷新，`pathfinding` 自动计算路径 |

### 不动

| 文件 | 理由 |
|------|------|
| `bot_targeting.h` | 逻辑不变 |
| `bot_role_rules.h` | 逻辑不变 |
| `wall_collision.h` / `arrow_movement.h` / `combat.h` | 通用 |
| `pickup.h` / `aoe.h` / `status_effect.h` / `mana_regen.h` / `skill_cooldown.h` / `progression.h` | 通用，无关 |
| `snapshot_types.h` / `snapshot_builder.cpp` / `snapshot_bindings.cpp` | 已由 Hero 统一化覆盖 |
| `world.cpp` / `world.h` | 已由 Hero 统一化覆盖 |

---

## 12. 风险与对策

| 风险 | 描述 | 对策 |
|------|------|------|
| Bot 技能选择 0.3s 决策延迟可能 miss 窗口 | 技能确认需等下一 DecisionCooldown 才发出 | MOBA 中 0.3s 可接受；若不够可缩短到 0.15s |
| Bot 在 CastState 内时 Goal 切换 | Engage 中 Fugitive 决定取消技能，但已经在 CastState 内 | `bot_input_injection` 检测 Goal 切换 → 写 `CancelSkill=true`；`skill_cast` 按 refund 策略处理 |
| 普攻与技能冲突 | Bot 同时设 SkillConfirm=true 和 AttackTargetId | `bot_input_injection` 中二选一：有技能时 AttackTargetId=-1；无技能时 SkillSlot=-1 |
| Channeling 期间 Bot 不移动被击杀 | Channeling 不可打断 | 设计上 R 技能就是高风险高回报；Bot 应当在 HP 安全时用，`bot_skill_decider` P3 已过滤 hp_ratio |
| 移动写入 HeroInputState 被 bot_input_injection 覆盖 | 同 bot 的 HeroInputState 只在一处写入 | bot_input_injection 是唯一写入点（不走 LocalInputInjection），无竞争 |
| Pathfinding 无效目标 | Bot 可能 chasing 已死亡的敌人的位置 | `bot_targeting` 每次 decision 刷新 TargetEntity；death check 保护 |

---

## 附录：v2 → v3 Goal 对比

| Goal | v2 行为 | v3 行为 |
|------|---------|---------|
| Flee | 直接写 `pos` 远离 | 写 `HeroInputState.MoveTarget` → 统一移动 |
| SeekHeal | 直接写 `pos` 朝血包 | 同上 + pathfinding A\* |
| SeekXp | 直接写 `pos` 朝 XP | 同上 + pathfinding A\* |
| Engage | Kiting 直接写 `pos` + `angle`；射普攻箭 | Kiting 写 `HeroInputState`；技能选 `SkillSlot/Confirm/Aim`；普攻走 `AttackTargetId` → 统一 attack_fire |
| Wander | 随机 `pos` 写 | 随机 `ai.MoveTarget` → pathfinding A\* |
