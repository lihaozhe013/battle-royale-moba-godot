# Hero + Skill 系统重构架构设计

> 创建：2026-07-20
> 状态：设计评审
> 关联：`Docs/Reference/bot_ai.md`、`Docs/Reference/sim_system_reference.md`、`Docs/Reference/input_system_design.md`

---

## 目录

1. [动机与目标](#1-动机与目标)
2. [现状诊断](#2-现状诊断)
3. [Hero 统一化设计](#3-hero-统一化设计)
4. [Skill 独立化设计](#4-skill-独立化设计)
5. [HeroDef 注册表](#5-herodef-注册表)
6. [Snapshot 统一](#6-snapshot-统一)
7. [View 层迁移指导](#7-view-层迁移指导)
8. [System 泛化与改名](#8-system-泛化与改名)
9. [Tick 顺序（重构后）](#9-tick-顺序重构后)
10. [实施阶段](#10-实施阶段)
11. [文件改动清单](#11-文件改动清单)
12. [风险与对策](#12-风险与对策)

---

## 1. 动机与目标

### 1.1 当前痛点

| 问题 | 具体表现 | 影响 |
|------|----------|------|
| Player/Bot 双轨制 | `PlayerTag` / `BotTag` 两条组件+System 路线，7 个 System 硬编码 `view<PlayerTag>` | Bot 无法使用技能，平白多维护 `bot_combat` |
| Skill 紧耦合 Hero | `skill_cast.h` 521 行单文件，`_trigger_effect` 内联 4 种 SkillKind 的完整伤害/控制/箭矢逻辑 | 新增一种 SkillKind 需改 3+ 处 switch，零复用 |
| Snapshot 重复 | `SimPlayerSnap` + `SimBotSnap` 字段高度重叠，View 层每隔循环必须写两条 fork | 维护成本翻倍 |
| 无法多英雄 | 所有玩家和 Bot 共享同一套 base stats，没有"英雄模板"的概念 | 未来增加不同英雄需要复制粘贴整段 spawn 逻辑 |

### 1.2 重构目标

| # | 目标 | 实现方式 |
|---|------|----------|
| G1 | Player/Bot 统一为 **Hero** | 引入 `HeroTag`；仅靠 `InputSource { Local, AI }` 区分谁是玩家、谁是 AI |
| G2 | Skill 是**完全独立的类**，不属于 Hero | 引入 `ISkill` 接口 + `SkillRegistry`；Hero 仅存 `SkillSlot`（id + 运行时状态） |
| G3 | 技能可轻松复用、组合 | 英雄定义 = HeroDef（基础属性 + 4 个 SkillId），引用已注册的 ISkill 实例 |
| G4 | 支持大量英雄类型 | 新增 `HeroRegistry`，定义英雄基础属性 + 技能组合 |
| G5 | View 层无需区分玩家/Bot | 单 `SimHeroSnap` + `is_local` 字段，一个循环搞定 |

---

## 2. 现状诊断

### 2.1 Player/Bot 双轨制详情

| 组件 | Player | Bot | 统一后 |
|------|--------|-----|--------|
| `PlayerTag { IsLocal }` | ✅ | ❌ | ➜ `HeroTag { IsLocal }` |
| `BotTag` | ❌ | ✅ | ➜ 删除，统一用 `HeroTag` |
| `PlayerInputState` | ✅ | ❌ | ➜ `HeroInputState`（保留结构不变，Bot 也用）|
| `BotAIState` / `BotBehaviorState` / `BotTier` / `BotRole` / `BotVisionRange` | ❌ | ✅ | 保留（Bot AI 专属，仅 AI 控制的 Hero 才有）|
| `MovePath` | ✅ | ❌ | ➜ 所有 Hero 都有 |
| `CastState` | ✅ | ❌ | ➜ 所有 Hero 都有 |
| `AttackTarget` | ✅ | ❌ | ➜ 所有 Hero 都有 |
| `SkillComponent` | ✅ (4 槽) | ✅ (4 槽，同定义但无人消费) | ➜ 不变 |

### 2.2 System 双轨制详情

| System | 仅 Player | 仅 Bot | 统一后 |
|--------|-----------|--------|--------|
| `local_input_injection` | ✅ `view<PlayerTag>` | ❌ | ⇒ 不变（只对 Local hero 写输入）|
| `player_attack_command` | ✅ | ❌ | ⇒ `attack_command`，泛化为 `view<HeroTag>` |
| `skill_cast` | ✅（繁） | ❌ | ⇒ 泛化，调度 ISkill |
| `player_pathfinding` | ✅ | ❌ | ⇒ `pathfinding`，泛化 |
| `player_movement` | ✅ | ❌ | ⇒ `movement`，泛化 |
| `player_attack_fire` | ✅ | ❌ | ⇒ `attack_fire`，泛化；`bot_combat` 删除 |
| `bot_combat` | ❌ | ✅（简） | ⇒ **删除**，被泛化后的 `attack_fire` 接管 |
| `bot_targeting` | ❌ | ✅ | ⇒ 保留（已对所有 Damageable 生效）|
| `bot_ai` | ❌ | ✅ | ⇒ 拆分为 Goal 决策 + BotInputInjection 两阶段 |
| (新) `bot_input_injection` | ❌ | ❌ | ⇒ **新增**：Bot AI → HeroInputState |
| (新) `bot_skill_decider` | ❌ | ❌ | ⇒ **新增**：Engage 子树技能选择 |
| `skill_level` | ✅ `view<PlayerTag>` | ❌ | ⇒ 泛化 |

### 2.3 Skill 紧耦合详情

```
skill_defs.h (static SkillDef table, 36 行)
  └─ 5 个条目，每个 SkillDef 存 Id/Kind/CastTime/ManaCost/Cooldown/Damage/Range/…
  └─ get_skill_def(id) 查表

skill_cast.h (521 行，单文件)
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

**问题**：
1. 新增 SkillKind 需改 `SkillKind` enum + `skill_defs.h` + `skill_cast.h` 中的非确定数量 switch 分支
2. 例如需要一种"召唤小兵"的技能，必须新加 `SkillKind::Summon` 并侵入 state machine
3. 不同技能的同种行为（如伤害公式 `def.Damage + stats.Atk * ratio`）硬编码重复

---

## 3. Hero 统一化设计

### 3.1 组件变更

```cpp
// ── 现有（将删除） ──
struct PlayerTag { bool IsLocal; };           // 删除
struct BotTag {};                              // 删除
struct PlayerInputState { ... };               // 改名 → HeroInputState

// ── 新增 ──
struct HeroTag { bool IsLocal = false; };      // 所有英雄
struct HeroInputState {                        // 同现有 PlayerInputState 内容
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
struct HeroDefId { int Value = 0; };           // 引用 HeroDef 注册表

// ── 保留（Bot AI 专用，仅 AI 控制的 Hero 有）──
struct BotAIState { ... };                     // 不变
struct BotBehaviorState { ... };               // 不变
struct BotTier { ... };                        // 不变
struct BotRole { ... };                        // 不变
struct BotVisionRange { ... };                 // 不变
```

### 3.2 实体出生

**Player（本地英雄）**：

```cpp
void World::_spawn_hero(int hero_def_id, int id) {
    const auto &def = HeroRegistry::instance().get(hero_def_id);

    auto e = _reg.create();
    _reg.emplace<HeroTag>(e, true);       // IsLocal=true
    _reg.emplace<HeroDefId>(e, hero_def_id);
    _reg.emplace<HeroInputState>(e);
    _reg.emplace<NetworkId>(e, id);
    _reg.emplace<Position2D>(e, ...);
    _reg.emplace<Health>(e, def.BaseHp, def.BaseHp);
    _reg.emplace<Mana>(e, def.BaseMana, def.BaseMana, ...);
    _reg.emplace<CombatStats>(e, def.BaseAtk, def.BaseAsp, -999.0);
    _reg.emplace<Damageable>(e);
    _reg.emplace<Dead>(e, false);
    _reg.emplace<Level>(e, 1);
    _reg.emplace<Experience>(e, 0, GameConfig::XpPerLevelBase);
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

**Bot（AI 英雄）**：

```cpp
void World::_spawn_bot_hero() {
    int hero_def_id = ...; // same or different from player
    auto e = _spawn_hero_entity(hero_def_id, bot_id, false); // IsLocal=false

    // Bot AI 专用组件
    _reg.emplace<BotAIState>(e, ...);
    _reg.emplace<BotBehaviorState>(e);
    _reg.emplace<BotTier>(e, tier);
    _reg.emplace<BotRole>(e, role);
    _reg.emplace<BotVisionRange>(e, vision);

    // Bot 数值微调（若 HeroDef 中已包含 Bot 系数则不需要此步）
    auto &hp = _reg.get<Health>(e);
    hp.Max = static_cast<int>(hp.Max * GameConfig::BotHpMul);
    hp.Cur = hp.Max;
    auto &stats = _reg.get<CombatStats>(e);
    stats.Atk *= GameConfig::BotAtkMul;

    // Bot 技能 CD 按系数缩放（在初始化 SkillSlot 时完成，避免运行时额外计算）
    auto &skills = _reg.get<SkillComponent>(e);
    for (int i = 0; i < 4; ++i) {
        skills.Slots[i].MaxCooldown *= GameConfig::BotSkillCooldownMul;
    }
}
```

### 3.3 迁移策略

**过渡期**：保留 `PlayerTag`/`BotTag` 为 `HeroTag` 的同值别名，所有 System 优先改用 `HeroTag`；旧 `view<PlayerTag>` 共存 1 版本。pipeline:

1. 新增 HeroTag，PlayerTag/BotTag 保持存在
2. 所有新 System 用 HeroTag；旧 System 加一条 `if (!reg.all_of<HeroTag>(e)) continue;` 避免双重处理
3. 确定无引用后，删除 PlayerTag/BotTag 定义

---

## 4. Skill 独立化设计

### 4.1 ISkill 接口

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

    // ── 数据查询（替代 SkillDef 静态表）──
    virtual float base_cooldown() const { return 0.0f; }
    virtual float base_mana_cost() const { return 0.0f; }
    virtual float base_cast_time() const { return 0.0f; }
    virtual float base_range(int level) const { return 0.0f; }

    // 等级相关的 scaling
    virtual float cooldown(int level) const { return base_cooldown(); }
    virtual float mana_cost(int level) const { return base_mana_cost(); }
    virtual float cast_time(int level) const { return base_cast_time(); }
    virtual float range(int level) const { return base_range(level); }
    virtual float damage(int level, float atk) const;
    virtual float effect_value(int level) const { return 0.0f; }

    // ── 验证（替代 skill_cast.h 的 inline validate 逻辑）──
    // 返回 error_code: 0=ok, 1=cd, 2=mana, 3=stun, 4=no_target, 5=target_dead
    virtual int validate_cast(
        entt::registry &reg, entt::entity caster,
        const CastContext &ctx
    ) = 0;

    // ── 生命周期 ──

    // 扣蓝后、进 Chasing/Casting 前调用（可做瞬发逻辑）
    virtual void on_cast_start(
        entt::registry &reg, entt::entity caster,
        struct CastState &cs, CommandBuffer &cb, IdState &ids,
        const CastContext &ctx
    ) {}

    // Chasing 期间每 tick 调用
    virtual void on_chase_tick(
        entt::registry &reg, entt::entity caster,
        struct CastState &cs, int level, float dt
    ) {}

    // 从 Chasing → Casting 的"进入范围"检查
    // (vs 在 skill_cast_system 统一检查，由 ISkill 实现者决定语义)
    virtual bool can_enter_casting(
        entt::registry &reg, entt::entity caster,
        const struct CastState &cs, int level
    ) = 0;

    // Casting 完成 → 触发效果
    virtual void on_cast_complete(
        entt::registry &reg, entt::entity caster,
        struct CastState &cs, CommandBuffer &cb, IdState &ids,
        int level
    ) = 0;

    // Channeling 每 tick
    virtual void on_channel_tick(
        entt::registry &reg, entt::entity caster,
        struct CastState &cs, CommandBuffer &cb, IdState &ids,
        int level, float dt
    ) {}

    // Dash 行为
    virtual void on_dash_start(
        entt::registry &reg, entt::entity caster,
        struct CastState &cs, int level
    ) {}
    virtual void on_dash_update(
        entt::registry &reg, entt::entity caster,
        struct CastState &cs, int level, float dt
    ) {}

    // 打断
    virtual bool can_interrupt(CastState::Phase phase) const {
        return phase == CastState::Phase::Chasing ||
               phase == CastState::Phase::Casting;
    }
};

} // namespace sim
```

### 4.2 SkillRegistry

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

void register_builtin_skills() {
    auto &r = SkillRegistry::instance();
    r.register_skill(1, std::make_unique<MeleeStrikeSkill>());
    r.register_skill(2, std::make_unique<AoEFieldSkill>());
    r.register_skill(3, std::make_unique<DashSkill>());
    r.register_skill(4, std::make_unique<ChannelBurstSkill>());
}

} // namespace sim
```

### 4.3 技能文件结构

```
src_cpp/sim/skills/
├── skill_interface.h           # ISkill 纯虚接口
├── skill_registry.h            # SkillRegistry 声明
├── skill_registry.cpp          # 实现 + register_builtin_skills
├── melee_strike.h              # SkillId=1, Kind=MeleeSingle, C 技能
├── aoe_field.h                 # SkillId=2, Kind=AoEField, E 技能
├── dash.h                      # SkillId=3, Kind=Dash, R 技能
└── channel_burst.h             # SkillId=4, Kind=ChannelBurst, F 技能
```

### 4.4 内置技能示例 (MeleeStrike)

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
        return base_cooldown() - (level - 1) * 0.5f; // 每级减 0.5s
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
            return 4; // no target
        if (reg.all_of<Dead>(ctx.target_entity) &&
            reg.get<Dead>(ctx.target_entity).enabled)
            return 5; // target dead
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
            // respawn timer + kill event + kills++ 由 combat system 统一处理
        }
    }
};

} // namespace sim
```

### 4.5 skill_cast_system 退化为调度器

```cpp
// src_cpp/sim/systems/skill_cast.h (重构后)
inline void skill_cast_system(
    entt::registry &reg, float dt, CommandBuffer &cb, IdState &ids, double now
) {
    auto view = reg.view<
        HeroTag, HeroInputState, SkillComponent, Mana,
        Position2D, CombatStats, NetworkId, Level>();

    for (auto e : view) {
        auto &input = view.get<HeroInputState>(e);
        auto &cs = reg.get_or_emplace<CastState>(e);
        auto &skills = view.get<SkillComponent>(e);
        auto &mana = view.get<Mana>(e);

        // Stun gate
        if (reg.all_of<StatusEffect>(e)) {
            auto &st = reg.get<StatusEffect>(e);
            if (st.Type == StatusType::Stun && st.Timer > 0.0f) {
                if (input.SkillSlot >= 0) cs.CastError = 3;
                continue;
            }
        }

        cs.HitTargetId = -1;

        switch (cs.State) {
        case CastState::Phase::None: {
            if (input.SkillSlot < 0 || input.SkillSlot >= 4) break;
            auto &slot = skills.Slots[input.SkillSlot];
            const ISkill *sk = SkillRegistry::instance().get(slot.SkillId);
            if (!sk) break;

            if (!input.SkillConfirm) {
                // Normal cast: 只存 slot/aim，不进任何阶段
                cs.ActiveSlot = input.SkillSlot;
                cs.SkillId = slot.SkillId;
                cs.TargetEntity = resolve_target_by_netid(reg, input.SkillTargetId);
                cs.TargetNetworkId = input.SkillTargetId;
                cs.AimPos = input.SkillAim;
                break;
            }

            // Quick cast / confirm
            if (slot.CooldownTimer > 0.0f) {
                cs.CastError = 1; break;
            }

            CastContext ctx{e, slot, slot.Level, input.SkillAim,
                resolve_target_by_netid(reg, input.SkillTargetId),
                input.SkillTargetId, false};

            int err = sk->validate_cast(reg, e, ctx);
            if (err != 0) {
                cs.CastError = err;
                cs.RejectTimer = 0.3f;
                break;
            }

            // 扣蓝 + 设 CD
            float cd = sk->cooldown(slot.Level);
            float mc = sk->mana_cost(slot.Level);
            mana.Cur -= mc;
            mana.RegenTimer = GameConfig::ManaRegenDelay;
            slot.CooldownTimer = cd;
            slot.MaxCooldown = cd;

            cs.ActiveSlot = input.SkillSlot;
            cs.SkillId = slot.SkillId;
            cs.TargetEntity = ctx.target_entity;
            cs.TargetNetworkId = ctx.target_network_id;
            cs.AimPos = ctx.aim_pos;

            sk->on_cast_start(reg, e, cs, cb, ids, ctx);

            if (sk->can_enter_casting(reg, e, cs, slot.Level)) {
                cs.State = CastState::Phase::Casting;
                cs.Timer = sk->cast_time(slot.Level);
            } else {
                cs.State = CastState::Phase::Chasing;
                cs.Timer = 0.0f;
            }
            cs.CastError = 0;
            break;
        }

        case CastState::Phase::Chasing: {
            const ISkill *sk = SkillRegistry::instance().get(cs.SkillId);
            if (!sk) { cs.State = CastState::Phase::None; break; }

            if (input.CancelSkill && sk->can_interrupt(cs.State)) {
                if (GameConfig::RefundOnChaseInterrupt)
                    // refund logic...
                cs.State = CastState::Phase::None;
                cs.ActiveSlot = -1; cs.SkillId = 0;
                break;
            }

            sk->on_chase_tick(reg, e, cs, skills.Slots[cs.ActiveSlot].Level, dt);

            if (sk->can_enter_casting(reg, e, cs, skills.Slots[cs.ActiveSlot].Level)) {
                cs.State = CastState::Phase::Casting;
                cs.Timer = sk->cast_time(skills.Slots[cs.ActiveSlot].Level);
                cs.CastError = 0;
            }
            break;
        }

        case CastState::Phase::Casting: {
            const ISkill *sk = SkillRegistry::instance().get(cs.SkillId);
            if (!sk) { cs.State = CastState::Phase::None; break; }

            if (input.CancelSkill && sk->can_interrupt(cs.State)) {
                if (GameConfig::RefundOnCastInterrupt)
                    // refund...
                cs.State = CastState::Phase::None; break;
            }

            cs.Timer -= dt;
            if (cs.Timer > 0.0f) break;

            sk->on_cast_complete(reg, e, cs, cb, ids, skills.Slots[cs.ActiveSlot].Level);

            if (sk->kind() == SkillKind::MeleeSingle ||
                sk->kind() == SkillKind::AoEField) {
                cs.State = CastState::Phase::None;
                cs.ActiveSlot = -1; cs.SkillId = 0;
            } else if (sk->kind() == SkillKind::Dash) {
                sk->on_dash_start(reg, e, cs, skills.Slots[cs.ActiveSlot].Level);
                cs.State = CastState::Phase::Dashing;
            } else if (sk->kind() == SkillKind::ChannelBurst) {
                cs.State = CastState::Phase::Channeling;
                cs.Timer = sk->effect_value(skills.Slots[cs.ActiveSlot].Level);
                cs.SubTimer = 0.0f;
            }
            break;
        }

        case CastState::Phase::Dashing: {
            const ISkill *sk = SkillRegistry::instance().get(cs.SkillId);
            if (sk) sk->on_dash_update(reg, e, cs, skills.Slots[cs.ActiveSlot].Level, dt);
            // Fallback: 若 dash 结束则退到 None
            Vec2 delta = cs.DashTarget - reg.get<Position2D>(e).Value;
            if (glm::length(delta) < 0.1f || cs.Timer <= 0.0f) {
                cs.State = CastState::Phase::None;
                cs.ActiveSlot = -1; cs.SkillId = 0;
            }
            break;
        }

        case CastState::Phase::Channeling: {
            const ISkill *sk = SkillRegistry::instance().get(cs.SkillId);
            cs.Timer -= dt;
            if (sk) sk->on_channel_tick(reg, e, cs, cb, ids,
                skills.Slots[cs.ActiveSlot].Level, dt);
            if (cs.Timer <= 0.0f) {
                cs.State = CastState::Phase::None;
                cs.ActiveSlot = -1; cs.SkillId = 0;
            }
            break;
        }
        }
    }
}
```

> 核心变化：具体技能行为从 switch 中提出，通过 `ISkill` 虚函数分发。约 200 行（原 521 行）。

---

## 5. HeroDef 注册表

### 5.1 HeroDef 结构

```cpp
// src_cpp/sim/heroes/hero_def.h
#pragma once

#include <string>

namespace sim {

struct HeroDef {
    int Id = 0;
    std::string Name;

    // 4 技能 ID（引用 SkillRegistry）
    int SkillIds[4] = {0, 0, 0, 0};

    // 基础属性
    int BaseHp = 100;
    float BaseMana = 300.0f;
    float BaseAtk = 10.0f;
    float BaseAsp = 1.0f;
    float BaseMoveSpeed = 5.0f;
    float AttackRange = 8.0f;

    // 成长系数
    float HpPerLevel = 10.0f;
    float AtkPerLevel = 1.0f;
    float AspPerLevel = 0.03f;
    float SpeedPerLevel = 0.5f;

    // 可选：视觉效果
    int PrefabId = 0;        // View 层用
    std::string IconPath;    // 可选
};

} // namespace sim
```

### 5.2 HeroRegistry

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

### 5.3 内置英雄

```cpp
// src_cpp/sim/heroes/hero_registry.cpp
#include "hero_registry.h"

namespace sim {

void register_builtin_heroes() {
    auto &r = HeroRegistry::instance();
    r.register_hero({
        .Id = 1,
        .Name = "Swordsman",
        .SkillIds = {1, 2, 3, 4},         // MeleeStrike, AoEField, Dash, ChannelBurst
        .BaseHp = 100,
        .BaseMana = 300.0f,
        .BaseAtk = 10.0f,
        .BaseAsp = 1.0f,
        .BaseMoveSpeed = 5.0f,
        .AttackRange = 8.0f,
        .HpPerLevel = 10.0f,
        .PrefabId = 0,
    });
    // 可继续注册 Archer, Mage 等（需要先注册对应的技能）
    // r.register_hero({ .Id = 2, .Name = "Archer", ... });
}

} // namespace sim
```

### 5.4 文件结构

```
src_cpp/sim/heroes/
├── hero_def.h
├── hero_registry.h
├── hero_registry.cpp
├── swordsman.h    (可选，或直接在 cpp 中 inline 定义)
└── archer.h       (可选)
```

> **设计原则**：`hero_def.h` 是纯数据，不含行为。行为完全通过 `SkillRegistry` 中的 ISkill 实现。

---

## 6. Snapshot 统一

### 6.1 SimHeroSnap

```cpp
// snapshot_types.h — 新增
class SimHeroSnap : public godot::RefCounted {
    GDCLASS(SimHeroSnap, godot::RefCounted)
public:
    // ── 通用 ──
    int id = 0;
    float x = 0, y = 0, ang = 0;
    int hp = 0, max_hp = 0;
    bool dead = false;                    // ← 之前只存在 bot snap
    float mana = 0, max_mana = 0;
    float atk = 0, asp = 0, speed = 0;
    int kills = 0, level = 0;
    int xp = 0, xp_needed = 0;
    int status = 0;                       // StatusType
    godot::TypedArray<SimSkillSlotSnap> skills;

    // ── 战斗状态（之前只在 PlayerSnap） ──
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

    // ── 等级/稀有度（之前只在 BotSnap） ──
    int tier = 0;                         // 玩家=0, Bot 沿用现有 tier

    // ── 新增 ──
    bool is_local = false;                // 唯一本地玩家
    int hero_def_id = 0;                  // View 层选 prefab

    // ... getter/setter ...
};
```

### 6.2 SimSnapshot 变更

```cpp
// snapshot_types.h — 修改
class SimSnapshot : public godot::RefCounted {
    GDCLASS(SimSnapshot, godot::RefCounted)
public:
    int seq = 0;
    int64_t t = 0;
    godot::TypedArray<SimHeroSnap> heroes;       // ← 替换 players + bots
    godot::TypedArray<SimArrowSnap> arrows;      // 不变
    godot::TypedArray<SimPickupSnap> pickups;    // 不变
    godot::TypedArray<SimEventSnap> events;      // 不变
    godot::TypedArray<SimAoESnap> aoes;          // 不变

    // 迁移助手：返回本地英雄索引（View 层无需再自行查找）
    int get_local_hero_index() const;
};
```

### 6.3 SnapshotBuilder 遍历

```cpp
// snapshot_builder.cpp — 统一构建
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
            ? static_cast<int>(reg.get<BotTier>(e)) : 0;  // 默认 0
        snap_hero->is_local = tag.IsLocal;
        snap_hero->hero_def_id = reg.all_of<HeroDefId>(e)
            ? reg.get<HeroDefId>(e).Value : 0;

        // 战斗状态（仅当有 CastState 组件）
        if (reg.all_of<CastState>(e)) { /* ... */ }

        // 技能
        if (reg.all_of<SkillComponent>(e)) { /* build skill slots */ }

        snap.get_heroes().push_back(snap_hero);
    }
}
```

---

## 7. View 层迁移指导

### 7.1 核心变更

| 模式 | 旧写法 | 新写法 |
|------|--------|--------|
| 找本地玩家 | `snap.players[0]` | `snap.heroes[snap.get_local_hero_index()]` 或 `snap.heroes.filter(func(h): return h.is_local)[0]` |
| 遍历所有敌方 | `for b in snap.bots` | `for h in snap.heroes: if !h.is_local: ...` |
| 遍历所有单位（含自己） | `for p in snap.players` + `for b in snap.bots` | `for h in snap.heroes` |
| 血条 team | `set_team(0)` for player, `set_team(2)` for bot | `set_team(0 if h.is_local else 2)` |
| 血条 tier | player: `update_level(p.level, 0)` \| bot: `update_level(b.level, b.tier)` | `update_level(h.level, h.tier)` |
| Prefab 选择 | `entity_type = 0` (player) / `1` (bot) | `hero_def_id` → 查 HeroDef → 选 prefab |
| 战斗 VFX 定位 | `snap.players[0].cast_aim_x/y` | 同上 |

### 7.2 受影响文件速览

| 文件 | 改动 |
|------|------|
| `sim_bridge.gd` | 替换 `snap.players[0]` ×3 + `for b in snap.bots` → `for h in snap.heroes` |
| `entity_manager.gd` | 合并 player/bot 双循环；prefab 选型改为 `hero_def_id` |
| `entity_view.gd` | `entity_type` 0/1 → 合并为单一 Hero 分支 |
| `health_bar_manager.gd` | 合并双循环；`team`/`tier` 从 `h.is_local`/`h.tier` 派生 |
| `skill_vfx.gd` | `snap.players[0]` → 通过 `is_local` 查找 |
| `bottom_hud.gd` | duck-typed，零改动 |
| `cast_bar.gd` / `skill_slot_ui.gd` | 零改动 |
| `input/input_state_machine.gd` | 接收 `is_moving` / `cast_state` 的参数源改名，无逻辑改动 |

---

## 8. System 泛化与改名

### 8.1 System 改名与泛化

| 旧名 | 新名 | 改动 |
|------|------|------|
| `player_attack_command_system` | `attack_command_system` | `view<HeroTag, HeroInputState, ...>`；Bot 通过 `HeroInputState` 走同路径 |
| `player_attack_fire_system` | `attack_fire_system` | 同上；删除孤立 `bot_combat_system` |
| `player_pathfinding_system` | `pathfinding_system` | 同上，Bot 也走 A\* + Chasing |
| `player_movement_system` | `movement_system` | 同上，Bot 也走 MovePath + AttackTarget Chase + 穿墙 |
| `local_input_injection_system` | 不变 | 只对 `HeroTag{IsLocal}` 写输入 |
| `skill_level_system` | 不变 | `view<HeroTag, ...>` |
| `skill_cast_system` | 不变 | `view<HeroTag, ...>` |
| `bot_ai_system` | 不变 | 仅 Goal 决策 + 复活 roll；**不再写 Position2D** |
| `bot_combat_system` | **删除** | 被 `attack_fire_system` 接管 |
| (新) `bot_input_injection_system` | **新增** | BotAIState + BotBehaviorState → HeroInputState；调用在 `bot_ai_system` 之后 |
| (新) `bot_skill_decider_system` | **新增** | Engage 子树技能选择 → 临时 `BotCastRequest` 组件 → `bot_input_injection` 消费 |

### 8.2 World.tick() 更新

```cpp
void World::tick(double dt) {
    if (_game_over) return;
    _time += dt;
    float fdt = static_cast<float>(dt);
    float map_half = _reg.get<MapBounds>(_map_bounds_entity).Half;
    auto &ids = _reg.get<IdState>(_id_state_entity);

    // ── 输入阶段 ──
    local_input_injection_system(_reg, _local_input_entity);   // 仅 local hero

    // ── Bot AI 阶段 ──
    bot_targeting_system(_reg, _rng, fdt);                     // 目标选择
    bot_ai_system(_reg, fdt, map_half, _rng);                  // Goal + 复活
    bot_skill_decider_system(_reg, _rng);                      // 技能选择 → BotCastRequest
    bot_input_injection_system(_reg);                          // BotAIState → HeroInputState

    // ── 统一战斗阶段 ──
    attack_command_system(_reg, fdt);                          // HeroInputState → AttackTarget
    skill_cast_system(_reg, fdt, _cb, ids, _time);             // HeroInputState → CastState (泛化)
    pathfinding_system(_reg, _nav_grid);                       // MoveIssue + Chasing → MovePath
    movement_system(_reg, fdt, map_half);                      // MovePath + AttackTarget + Chasing → pos
    attack_fire_system(_reg, _time, _cb, ids);                 // AttackTarget → homing 箭

    // ── 物理 ──
    arrow_movement_system(_reg, fdt);
    wall_collision_system(_reg, _cb);
    combat_system(_reg, _cb);

    // ── Game systems ──
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

---

## 9. Tick 顺序（重构后）

```
LocalInputInjection       (LocalInputSingleton → local HeroInputState)
BotTargeting              (AI 目标选择)
BotAI                     (Goal 决策 + 复活等级 roll)
BotSkillDecider           (Engage 子树选技能 → BotCastRequest)    ← 新增
BotInputInjection         (BotAIState + BotCastRequest → HeroInputState)  ← 新增
AttackCommand             (HeroInputState → AttackTarget)         ← 泛化
SkillCast                 (HeroInputState → ISkill 调度)          ← 泛化
Pathfinding               (MoveIssue + Chasing → MovePath)        ← 泛化
Movement                  (MovePath + AttackTarget Chase → pos)   ← 泛化
AttackFire                (AttackTarget → homing 箭)              ← 泛化，bot_combat 删除
ArrowMovement
WallCollision
Combat
Pickup / AoE / StatusEffect / ManaRegen / SkillCooldown / SkillLevel / Progression
SnapshotExport
```

---

## 10. 实施阶段

### P1: 组件重构（低风险）

| 任务 | 涉及文件 |
|------|----------|
| 新增 `HeroTag`、`HeroInputState` | `components.h` |
| 新增 `HeroDefId` 组件 | `components.h` |
| `_spawn_player` 改挂 `HeroTag{IsLocal=true}`，保留 PlayerTag | `world.cpp` |
| `_spawn_bot` 改挂 `HeroTag{IsLocal=false}`，保留 BotTag | `world.cpp` |
| 确定 SimHeroSnap 字段列表（不实现） | `snapshot_types.h` |

### P2: HeroDef + HeroRegistry 搭建（低风险）

| 任务 | 涉及文件 |
|------|----------|
| 新建 `heroes/` 目录 + `hero_def.h` / `hero_registry.h/cpp` | `heroes/*` |
| 注册默认英雄 "Swordsman" | `hero_registry.cpp` |
| `_spawn_player/_spawn_bot` 改为查 HeroDef 初始化 | `world.cpp` |

### P3: Skill 接口化（关键路径，高风险）

| 任务 | 涉及文件 |
|------|----------|
| 新建 `skills/` 目录 + `skill_interface.h` + `skill_registry.h/cpp` | `skills/*` |
| 实现 4 个内置 ISkill 类（`melee_strike.h` / `aoe_field.h` 等） | `skills/*.h` |
| `_register_types` 调用 `register_builtin_skills()` | `register_types.cpp` |
| `skill_cast_system` 改为 ISkill 调度器 | `systems/skill_cast.h` |
| 删除 `skill_defs.h` 静态表 | — |
| 验证 Player 技能仍正常工作 | 手动测试 |

### P4: System 泛化 + BotInputInjection（中风险）

| 任务 | 涉及文件 |
|------|----------|
| `player_attack_command` → `attack_command` + 泛化 | `systems/attack_command.h` |
| `player_attack_fire` → `attack_fire` + 泛化 | `systems/attack_fire.h` |
| `player_pathfinding` → `pathfinding` + 泛化 | `systems/pathfinding.h` |
| `player_movement` → `movement` + 泛化 | `systems/movement.h` |
| 删除 `bot_combat.h` | — |
| 新增 `bot_skill_decider.h` | `systems/bot_skill_decider.h` |
| 新增 `bot_input_injection.h` | `systems/bot_input_injection.h` |
| `bot_ai_system` 拆：保留 Goal + 复活，**不再写 Position2D** | `systems/bot_ai.h` |
| `World::tick()` 更新 | `world.cpp` |

### P5: Snapshot 统一 + View 迁移（中风险）

| 任务 | 涉及文件 |
|------|----------|
| `SimHeroSnap` 实现 | `snapshot_types.h` / `snapshot_bindings.cpp` |
| SnapshotBuilder 统一遍历 `HeroTag` | `snapshot_builder.cpp` |
| `SimSnapshot` 加 `heroes` 数组 | `snapshot_types.h` |
| sim_bridge.gd 切到 `snap.heroes` | `sim_bridge.gd` |
| entity_manager.gd 合并循环 | `entity_manager.gd` |
| entity_view.gd 合并 entity_type | `entity_view.gd` |
| health_bar_manager.gd 合并循环 | `health_bar_manager.gd` |
| skill_vfx.gd 改用 `is_local` | `skill_vfx.gd` |
| 删除 `snap.players` + `snap.bots` | 所有文件 |
| 删除 `PlayerTag`/`BotTag` 组件定义 | `components.h` |

### P6: Bot 行为树补完（中风险）

| 任务 | 涉及文件 |
|------|----------|
| Bot 技能系数写入 GameConfig | `game_config.h` |
| `_spawn_bot` 初始化 SkillSlot 时乘系数 | `world.cpp` |
| `bot_skill_decider` Engage 技能优先级 | `systems/bot_skill_decider.h` |
| `bot_input_injection` 写 HeroInputState | `systems/bot_input_injection.h` |
| `bot_ai.md` v3 文档 | `Docs/Reference/bot_ai.md` |

---

## 11. 文件改动清单

### 新建

| 文件 | 归属阶段 |
|------|----------|
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
| `Docs/Reference/hero_skill_architecture.md` | 本文档 |

### 修改

| 文件 | 改动 |
|------|------|
| `src_cpp/sim/components.h` | 删 `PlayerTag`/`BotTag`/`PlayerInputState`；增 `HeroTag`/`HeroInputState`/`HeroDefId` |
| `src_cpp/sim/game_config.h` | 新增 Bot 技能系数 `BotSkillDmgMul`/`BotSkillCooldownMul`/`BotManaCostMul`；删 XP 经济常量 |
| `src_cpp/sim/world.h` | `_spawn_player/_spawn_bot` → `_spawn_hero`；新增 `_spawn_bot_hero` |
| `src_cpp/sim/world.cpp` | `initialize` / `tick` 更新；spawn 逻辑统一为 HeroDef 查表 |
| `src_cpp/sim/skill_defs.h` | **删除**，由 `skills/` 替代 |
| `src_cpp/sim/systems/skill_cast.h` | 退化为调度器 ~200 行 |
| `src_cpp/sim/systems/player_attack_command.h` | → `attack_command.h` + 泛化 |
| `src_cpp/sim/systems/player_attack_fire.h` | → `attack_fire.h` + 泛化 |
| `src_cpp/sim/systems/player_pathfinding.h` | → `pathfinding.h` + 泛化 |
| `src_cpp/sim/systems/player_movement.h` | → `movement.h` + 泛化 |
| `src_cpp/sim/systems/bot_ai.h` | 拆分 Goal + 复活，不再写 Position2D |
| `src_cpp/sim/systems/bot_combat.h` | **删除** |
| `src_cpp/sim/systems/skill_level.h` | `view<PlayerTag>` → `view<HeroTag>` |
| `src_cpp/sim/systems/local_input_injection.h` | 基本不变 |
| `src_cpp/sim/snapshot_types.h` | 新增 `SimHeroSnap`；`SimSnapshot.heroes` 替代 `players`+`bots` |
| `src_cpp/sim/snapshot_builder.h/cpp` | `_build_heroes` 统一遍历 |
| `src_cpp/sim/snapshot_bindings.cpp` | 注册 `SimHeroSnap` 属性和 `SimSnapshot.get_local_hero_index` |
| `src_cpp/sim/world.h` | 更新 system 声明 |
| `scripts/sim_bridge.gd` | `snap.players[0]` ×3 替代；hover 循环改 `snap.heroes` |
| `scripts/view/entity_manager.gd` | 合并双循环；prefab 按 `hero_def_id` |
| `scripts/view/entity_view.gd` | `entity_type` 0/1 合并 |
| `scripts/ui/health_bar_manager.gd` | 合并双循环 |
| `scripts/view/skill_vfx.gd` | 用 `is_local` 找本地 hero |
| `Docs/Reference/bot_ai.md` | 重写为 v3 |

### 不动（已通用）

| 文件 | 原因 |
|------|------|
| `bot_targeting.h` | 已对所有 `Damageable` 有效 |
| `arrow_movement.h` / `wall_collision.h` / `combat.h` | 无 player/bot 区别 |
| `pickup.h` / `aoe.h` / `status_effect.h` | 同上 |
| `mana_regen.h` / `skill_cooldown.h` | 同上，全组件作用 |
| `progression.h` / `xp_helper.h` | 同上 |
| `bottom_hud.gd` / `cast_bar.gd` / `skill_slot_ui.gd` | duck-typed，零改动 |

---

## 12. 风险与对策

| 风险 | 描述 | 对策 |
|------|------|------|
| ISkill 虚函数性能开销 | 5 hero × 4 槽 ≈ 20 次/30Hz，热点在 `on_cast_complete` 含大量 `cb.push` | 可接受；必要时 per-skill 内联 `on_chase_tick` 等高频函数 |
| Bot 输入写入 HeroInputState 与玩家冲突 | Bot 的 HeroInputState 是直接通过 reg.view 迭代写的，LocalInputSingleton 只影响 IsLocal=true 的 hero | 天然隔离 |
| 过渡期双写 Snapshot 内存翻倍 | P2 双写阶段 `players[]` + `bots[]` + `heroes[]` 同时存在 | 短暂（仅 1 版本），数据小（几十个 hero）可接受 |
| `snap.players[0]` 缺失 | View 代码需在多个位置替换 | 搜索 + blast radius 已列全 3 处，走查即可 |
| 旧存档/配置依赖 PlayerTag/BotTag 值 | C++ 侧无持久化状态，全从 registry 重建 | 无需迁移 |
| 新技能集成 CI 回归 | 改动面大，可能破坏现有技能行为 | 每个技能写一个模拟测试（如 `test_melee_strike` 验证伤害+击杀） |
| Bot 不使用技能或技能循环卡死 | `bot_skill_decider` 与 `bot_input_injection` 交互错漏 | P5 Engage 子树兜底：无可用技能 → 普攻 |
| 现有 View 层预置 (player.tscn / bot.tscn) 硬编码 | entity_manager.gd 的 `PREFAB_PATHS` 数组 | 改为 `hero_prefabs` 字典 keyed by `hero_def_id` + 缺省 fallback |

---

## 附录 A：与现有文档的关系

| 文档 | 受影响 |
|------|--------|
| `Docs/Reference/sim_system_reference.md` | ⚡需更新 → §2 文件结构、§3 组件清单、§4 System 列表、§9 实体出生表、§10 GameConfig |
| `Docs/Reference/bot_ai.md` | 🔄 待重写为 v3（关联文档） |
| `Docs/Reference/input_system_design.md` | 基本不受影响（View→Sim 命令通道不变），仅 §18 Bot 相关澄清可更新 |
| `Docs/Reference/prompt.md` | 不变 |
| `CONTEXT.md` | 已更新（文件改名自 `AGENTS.md`）— 新增 heroes/ + skills/ 目录、P1-P6 阶段规划 |

---

## 附录 B：BOY 与 EOY 架构对比

```
BOY（Before）                             EOY（After）

PlayerTag + BotTag                        HeroTag { IsLocal }
PlayerInputState                          HeroInputState（player + bot 共用）
skill_defs.h 静态表                       SkillRegistry + ISkill
skill_cast.h 521 行巨型 switch            skill_cast.h ~200 行调度器
_build_players + _build_bots             _build_heroes 单循环
snap.players + snap.bots                 snap.heroes
3 处 `snap.players[0]`                    `snap.heroes[local_idx]`
player_* × 4 system                       combatant_* × 4（泛化）
bot_combat（独立于 player_attack_fire）    删除，被 attack_fire 接管
手动魔改_trigger_effect switch            新增技能 = 写一个 .h + 注册 1 行
新增英雄类型 = 复制 _spawn_player 改数值     新增英雄类型 = register_hero 1 次 call
```
