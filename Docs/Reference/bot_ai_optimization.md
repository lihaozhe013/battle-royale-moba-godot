# Bot AI 优化方案

> 最后更新：2026-07-06
> 关联：`Docs/Reference/prompt.md`、`Docs/Reference/godot-editor-todo.md`

---

## 目录

1. [现状诊断](#1-现状诊断)
2. [行为树设计](#2-行为树设计)
3. [Bot 随机刷新（等级+Tier）](#3-bot-随机刷新技术等级tier)
4. [击杀 XP 系统](#4-击杀-xp-系统)
5. [Pickup 经济调优](#5-pickup-经济调优)
6. [快照与视图层补完](#6-快照与视图层补完)
7. [Spawner 点位布局](#7-spawner-点位布局)
8. [编辑器操作清单](#8-编辑器操作清单)
9. [文件改动清单](#9-文件改动清单)
10. [风险与对策](#10-风险与对策)

---

## 1. 现状诊断

### 1.1 Bot AI 问题

当前 bot 行为由三个扁平的 C++ system 组成（`bot_targeting.h` → `bot_ai.h` → `bot_combat.h`），无决策层：

| System | 行为 | 问题 |
|--------|------|------|
| `bot_ai_system` | 随机 wander 到地图随机点 | 不会主动找 XP/血包；复活血量永远 Lv1 基础值 |
| `bot_targeting_system` | 视野内按 min HP → min dist → random 选目标 | 逻辑正常，无需大改 |
| `bot_combat_system` | 向目标射击 | 逻辑正常，射击方向已独立于 FacingAngle |

### 1.2 Bot 属性现状

| 属性 | 存在 | 说明 |
|------|------|------|
| `Level` / `Experience` / `MoveSpeed` | ✅ | `world.cpp:159-161` 已有组件 |
| `CombatStats.Atk/Asp` / `Kills` | ✅ | `world.cpp:156-157` 已有组件 |
| 吃 XP 升级 / 吃血包回血 | ✅ | `pickup_system` 对所有 `Damageable` 生效 |
| 击杀涨 Atk/Asp | ✅ | `progression_system` 对所有 combatant 生效 |
| 主动寻路到 pickup | ❌ | `bot_ai` 只 wander |
| 复活随机等级 / Tier | ❌ | 复活永远 `hp.Cur=BotHp`（`bot_ai.h:31`） |
| 快照暴露 xp/speed | ❌ | `SimBotSnap` 只有 level（`snapshot_types.h:50-52`） |
| 视图层显示等级/tier | ❌ | `StatsPanel` 仅显示玩家；血条等级徽章 todo P2#3 未做 |

### 1.3 Pickup 经济问题

| 配置 | 当前值 | 问题 |
|------|--------|------|
| `XpPerLevelBase` | 100 | 升级门槛太低（Lv1 只需 5 个 XP pickup） |
| `XpPickupValue` | 20 | 单个经验太多 |
| `XpPickupCount` / Respawn | 6 / 8s | 产出 120 xp/8s=15/s → 约 7s 升一级 |
| `HealPickupValue` (大) | 30% max | 回血比例可调 |
| `HealPickupCount` | 3 | 太多 |
| `SmallHealPickupValue` | 25% max | 太强 |
| `SmallHealPickupCount` | 5 | 回血泛滥 |

---

## 2. 行为树设计

### 2.1 架构变更

将 `bot_ai_system` 拆为两个新 system，替代原有的 wander 逻辑：

```
原 tick 顺序（world.cpp:86-90）:
  bot_targeting_system → bot_ai_system → bot_combat_system

新 tick 顺序（不变）:
  bot_targeting_system    # 目标选择（不变）
→ bot_decision_system     # 行为树决策 + 移动（替换 bot_ai_system）
→ bot_combat_system       # 射击（不变）
```

### 2.2 新增组件

```cpp
// components.h

enum class BotTier : uint8_t {
    Normal = 0,
    Elite = 1,
    Boss = 2,
};

struct BotBehaviorState {
    enum class Goal : uint8_t {
        Flee = 0,      // 逃跑（HP<30% + 敌人在视野内）
        SeekHeal = 1,  // 找血包
        SeekXp = 2,    // 找经验
        Engage = 3,    // 战斗（kiting）
        Wander = 4,    // 随机游走
    };
    Goal Current = Goal::Wander;
    entt::entity PickupTarget = entt::null; // 目标 pickup 实体
    float DecisionCooldown = 0.0f;          // 决策冷却（防每 tick 重选）
};
```

`_spawn_bot`（`world.cpp`）中 emplace 两个新组件。

### 2.3 `bot_decision_system` 决策逻辑

每 tick 调用，但只有 `DecisionCooldown <= 0` 时才重新选择 Goal（cooldown=0.3s，避免闪烁）。移动每 tick 执行。

```
initialize:
  cooldown -= dt
  if (!dead && cooldown > 0): 执行当前 Goal 的移动，return
  if (dead): continue
  cooldown = 0.3f

step1 — 扫描地图上所有活跃 pickup:
  view<PickupTag, Position2D>
  按 PickupType 分类，分别存最近的 Heal / SmallHeal / Xp 位置

step2 — 优先级决策（高→低）:
  ┌──────────────────────────────────────────────────────────────────────┐
  │ PRIORITY 1: FLEE                                                     │
  │ 条件: hp.Cur < hp.Max * 0.3                                          │
  │       且 视野内有 alive 敌人                                         │
  │ 动作: MoveTarget = pos + normalize(pos - nearest_enemy_pos) * 30    │
  │       偏移 30 单位（地图边界 clamp）                                  │
  │       （可选朝向最近血包方向偏移 0.3）                               │
  ├──────────────────────────────────────────────────────────────────────┤
  │ PRIORITY 2: SEEK_HEAL                                                │
  │ 条件: hp.Cur < hp.Max * 0.6                                          │
  │       且 地图上有 Heal/SmallHeal pickup                              │
  │ 动作: PickupTarget = 最近的血包实体                                  │
  │       MoveTarget = 该血包位置                                        │
  ├──────────────────────────────────────────────────────────────────────┤
  │ PRIORITY 3: SEEK_XP                                                  │
  │ 条件: TargetEntity == null（无战斗目标）                              │
  │       或 目标距离 > vision（无法交战）                                │
  │       且 地图上有 Xp pickup                                          │
  │ 动作: PickupTarget = 最近的 XP 实体                                  │
  │       MoveTarget = 该 XP 位置                                        │
  ├──────────────────────────────────────────────────────────────────────┤
  │ PRIORITY 4: ENGAGE                                                   │
  │ 条件: TargetEntity != null 且在射程/视野内                           │
  │ 动作: 计算 to_target, dist                                           │
  │       if dist > vision*0.8:    追击   → 朝 target 移动               │
  │       if dist < vision*0.3:    后退   → 远离 target 移动             │
  │       else:                    strafe → 垂直 to_target 方向横移      │
  ├──────────────────────────────────────────────────────────────────────┤
  │ PRIORITY 5: WANDER                                                   │
  │ 条件: 无事可做                                                       │
  │ 动作: 保留现有随机 wander（随机地图点 + WanderTimer 刷新）            │
  └──────────────────────────────────────────────────────────────────────┘

step3 — 防聚堆:
  SeekHeal/SeekXp 时, 不取绝对最近, 而取 top-3 中随机一个:
    收集所有同类型 pickup, 按距离排序, 取前 3 → uniform_int(0,2) 选
    
step4 — 移动执行（沿用原有逻辑）:
  dir = normalize(MoveTarget - pos)
  if dist > 0.01f:
    step = dir * speed * dt
    move_dist = min(dist, length(step))
    pos = clamp_to_map(pos + dir * move_dist, map_half)
    angle.Radians = atan2(dir.y, dir.x)
```

### 2.4 Kiting 战斗位移（ENGAGE 状态详细行为）

| 条件 | 行为 |
|------|------|
| dist > vision × 0.8 | 追击 → MoveTarget = target.position |
| dist < vision × 0.3 | 后撤 → MoveTarget = pos - normalize(to_target) × 20（撤到安全距离） |
| 中间 | 横移 → dir = normalize(perpendicular(to_target))，左右交替（用 std::bernoulli_distribution 每次决策随机） |

`bot_combat` 不受影响——射击方向始终指向 target，独立于移动朝向。

---

## 3. Bot 随机刷新（等级 + Tier）

### 3.1 GameConfig 新增常量

```cpp
// game_config.h

// ── Bot 等级成长 ──
static constexpr int MaxBotLevel = 30;
static constexpr int BotBaseHp = 50;
static constexpr int BotHpPerLevel = 8;      // 每级 +8 HP
static constexpr float BotAtkPerLevel = 0.8f; // 每级 +0.8 ATK
static constexpr float BotAspPerLevel = 0.03f;// 每级 +0.03 ASP
static constexpr float BotSpeedPerLevel = 0.3f;// 每级 +0.3 speed
static constexpr float BotXpPerLevel = 0;     // bot 不通过杀敌涨 XP（要全图吃）
static constexpr float BotAspMax = 5.0f;      // 上限

// ── Tier 倍率（在等级基础之上乘算） ──
static constexpr struct TierMult {
    float HpMul = 1.0f;
    float AtkMul = 1.0f;
    float AspMul = 1.0f;
    float SpeedMul = 1.0f;
    float VisionMul = 1.0f;
} TierNormal   = {1.0f, 1.0f, 1.0f,   1.0f,   1.0f},
  TierElite    = {2.0f, 1.6f, 1.1f,   1.1f,   1.2f},
  TierBoss     = {4.0f, 2.5f, 1.25f,  1.2f,   1.5f};

// ── 决策树参数 ──
static constexpr float BotDecisionCooldown = 0.3f;
static constexpr float BotFleeDist = 30.0f;
static constexpr float BotEngageRangeHigh = 0.8f; // vision 比例
static constexpr float BotEngageRangeLow = 0.3f;
static constexpr float BotKiteStrafeDist = 5.0f;
```

### 3.2 复活逻辑（`bot_ai.h` 重写）

```cpp
if (dead) {
    ai.RespawnTimer -= dt;
    if (ai.RespawnTimer <= 0.0f) {
        reg.get<Dead>(e).enabled = false;

        // 随机等级 1~MaxBotLevel
        int new_lv = std::uniform_int_distribution<int>(1, GameConfig::MaxBotLevel)(rng);

        // Roll Tier
        float r = std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
        BotTier tier;
        if (r < 0.05f)       tier = BotTier::Boss;
        else if (r < 0.20f)  tier = BotTier::Elite;
        else                  tier = BotTier::Normal;

        const auto &mult = _tier_mult(tier); // 查 TierMult

        // 应用属性
        lv.Value = new_lv;
        int base_max_hp = GameConfig::BotHp + (new_lv - 1) * GameConfig::BotHpPerLevel;
        hp.Max = static_cast<int>(base_max_hp * mult.HpMul);
        hp.Cur = hp.Max;
        stats.Atk = (GameConfig::BotBaseAttack + (new_lv - 1) * GameConfig::BotAtkPerLevel) * mult.AtkMul;
        stats.Asp = std::min(
            (GameConfig::BotBaseAttackSpeed + (new_lv - 1) * GameConfig::BotAspPerLevel) * mult.AspMul,
            GameConfig::BotAspMax
        );
        ms.Value = (GameConfig::BotSpeed + (new_lv - 1) * GameConfig::BotSpeedPerLevel) * mult.SpeedMul;
        vision.Value = GameConfig::BotVisionRange * mult.VisionMul;
        exp.Cur = 0;
        exp.Needed = new_lv * GameConfig::XpPerLevelBase;

        // 重生位置 + wander 重置（保留原有代码）
        float half = map_half - GameConfig::BotRadius;
        pos.Value = Vec2{uniform(-half, half), uniform(-half, half)};
        ai.MoveTarget = Vec2{uniform(-half, half), uniform(-half, half)};
        ai.RespawnTimer = 0.0f;
        ai.TargetEntity = entt::null;
        ai.WanderTimer = uniform(GameConfig::BotWanderIntervalMin, GameConfig::BotWanderIntervalMax);
        ai.PickupTarget = entt::null; // 新增
        ai.Current = BotBehaviorState::Goal::Wander;
    }
    continue;
}
```

### 3.3 首次出生也随机

`_spawn_bot`（`world.cpp`）在 bot 首次创建时也执行同一套 random roll，让刚开局就随机出 Elite/Boss，避免"开局全是弱鸡"。

### 3.4 Hp/Atk/Asp/Speed 随等级成长曲线示例

| Level | HP(N) | HP(E) | HP(B) | ATK(N) | ATK(E) | ATK(B) | ASP(N) | ASP(E) | ASP(B) | Speed(N) |
|-------|-------|-------|-------|--------|--------|--------|--------|--------|--------|----------|
| 1     | 50    | 100   | 200   | 5.0    | 8.0    | 12.5   | 0.80   | 0.88   | 1.00   | 2.0      |
| 5     | 82    | 164   | 328   | 8.2    | 13.1   | 20.5   | 0.92   | 1.01   | 1.15   | 3.2      |
| 10    | 122   | 244   | 488   | 12.2   | 19.5   | 30.5   | 1.07   | 1.18   | 1.34   | 4.7      |
| 15    | 162   | 324   | 648   | 16.2   | 25.9   | 40.5   | 1.12   | 1.23   | 1.40   | 6.2      |
| 20    | 202   | 404   | 808   | 20.2   | 32.3   | 50.5   | 1.27   | 1.40   | 1.59   | 7.7      |
| 30    | 282   | 564   | 1128  | 28.2   | 45.1   | 70.5   | 1.67   | 1.84   | 2.09   | 10.7     |

（Speed 较高是因为 PlayerSpeed=5.0，bot 高等级速度超过玩家是正常设计——玩家靠走位和放风筝，不是跑直线拼速度。）

---

## 4. 击杀 XP 系统

### 4.1 击杀 XP 公式

```cpp
// progression.h 新增
static constexpr int KillXpBase = 15;        // 每级基础 XP
static constexpr float KillXpHighBonus = 0.5f; // 每高 1 级 +50%
```

公式：
```
kill_xp = KillXpBase × victim_level × (1 + max(0, victim_level - killer_level) × KillXpHighBonus)
```

### 4.2 实现方式

在 `progression_system` 中处理 `KillEventBuffer` 时，不再只读 `KillEvent` 的 `KillerId/VictimId`，而是：
1. 用 `VictimId` 反查受害者的 `NetworkId` 对应的实体 → 读 `Level` 组件
2. 用 `KillerId` 反查杀手的 `NetworkId` 对应的实体 → 读 `Level` + `Experience` 组件
3. 计算 `kill_xp`，调用 `apply_xp(reg, killer_entity, kill_xp)`

### 4.3 apply_xp helper

抽取 `pickup.h:64-71` 的升级循环为独立函数 `apply_xp(entt::registry &reg, entt::entity e, int xp_amount)`：

```cpp
inline void apply_xp(entt::registry &reg, entt::entity e, int xp_amount) {
    if (!reg.all_of<Experience, Level, MoveSpeed, Health>(e)) return;
    auto &exp = reg.get<Experience>(e);
    auto &lv = reg.get<Level>(e);
    auto &ms = reg.get<MoveSpeed>(e);
    auto &hp = reg.get<Health>(e);

    exp.Cur += xp_amount;
    while (exp.Cur >= exp.Needed) {
        exp.Cur -= exp.Needed;
        lv.Value += 1;
        hp.Max += GameConfig::HpPerLevel;
        hp.Cur = hp.Max;
        ms.Value += GameConfig::SpeedPerLevel;
        exp.Needed = lv.Value * GameConfig::XpPerLevelBase;
    }
}
```

`pickup.h` 和 `progression.h` 都 include 此 helper（可放在独立文件 `systems/xp_helper.h`，或直接用 inline 函数两个文件重复）。

### 4.4 示例（XpPerLevelBase=500）

| 击杀场景 | victim_lv | killer_lv | kill_xp | 占 killer 当前级进度 |
|---------|-----------|-----------|---------|-------------------|
| Lv1 杀 Lv1 | 1 | 1 | 15 | 3% |
| Lv1 杀 Lv10 Boss | 10 | 1 | 825 | 165%（升 1 级 + 余 325） |
| Lv10 杀 Lv1 | 1 | 10 | 15 | 0.3%（Lv10 需 5000） |
| Lv5 杀 Lv10 | 10 | 5 | 187 | —— |
| Lv10 杀 Lv10 | 10 | 10 | 150 | 3% |

### 4.5 不动的部分

- `KillEvent` schema 不变（`KillerId/VictimId`），等级反查在 `progression_system` 中完成。
- `combat.h` 不变，击杀事件写入逻辑不动。
- `progression.h` 中原有 `Atk/Asp` 增长逻辑保留，击杀 XP 是额外追加。

---

## 5. Pickup 经济调优

### 5.1 GameConfig 数值变更

```cpp
// ── XP 经济 ──
XpPerLevelBase          100    → 500    // 升级门槛 ×5
XpPickupValue           20     → 8      // 单个经验 -60%

XpPickupCount           6      → 8      // +2 个刷新点
XpPickupRespawnTime     8      → 10     // 略慢

// ── 血包经济 ──
HealFraction            0.3    → 0.5    // 大血包回血比例 50%
HealPickupCount         3      → 2      // 减少到 2 个
HealPickupRespawnTime   12     → 25     // 25s 刷新

SmallHealPickupValue    0.25   → 0.15   // 小血包回 15% maxHP
SmallHealPickupCount    5      → 2      // 减少到 2 个
SmallHealPickupRespawnTime 8   → 20     // 20s 刷新
```

### 5.2 产出计算对比

| 指标 | 调优前 | 调优后 | 变化 |
|------|--------|--------|------|
| XP 产出 / s | 120 / 8 = 15 | 64 / 10 = 6.4 | -57% |
| Lv1 升级所需时间 | 100/15 ≈ 6.7s | 500/6.4 ≈ 78s | **×12** |
| Lv10 升级所需时间 | 1000/15 ≈ 67s | 5000/6.4 ≈ 780s | **×12** |
| 大血包产出 / s | 3×30% / 12 = 7.5% maxHP | 2×50% / 25 = 4% maxHP | -47% |
| 小血包产出 / s | 5×25% / 8 = 15.6% maxHP | 2×15% / 20 = 1.5% maxHP | -90% |
| 总回血产出 / s | ≈ 23% maxHP | ≈ 5.5% maxHP | **×0.24** |
| 单次大血包价值 | 30% 立刻回 | 50% 立刻回 | **单次更强** |

### 5.3 配合 bot 全图感知的预期

- 玩家和 bot 都会抢血包 → 血包争夺战成为核心博弈
- 血包稀少（全图仅 4 个：2 大 + 2 小），位置固定 → 玩家可以控资源点
- Bot 决策会让高等级 bot 优先抢血包，低等级 bot 抢 XP → 形成食物链
- 玩家"狗着吃 XP"策略：在远离 bot 路线的地方吃 XP，升到一定等级再去碰 Elite/Boss

---

## 6. 快照与视图层补完

### 6.1 SimBotSnap 扩字段

```cpp
// snapshot_types.h
class SimBotSnap : public godot::RefCounted {
    GDCLASS(SimBotSnap, godot::RefCounted)
public:
    int id = 0; float x = 0, y = 0, ang = 0;
    int hp = 0, max_hp = 0; bool dead = false;
    float atk = 0, asp = 0; int kills = 0; int level = 0;
    // ↓ 新增
    int xp = 0, xp_needed = 0;
    float speed = 0;
    int tier = 0; // 0=Normal 1=Elite 2=Boss

    // … getter/setter 同理新增 …
};
```

### 6.2 snapshot_builder.cpp 导出

```cpp
void SnapshotBuilder::_build_bots(...) {
    // … 现有代码 …
    s->xp = reg.all_of<Experience>(e) ? reg.get<Experience>(e).Cur : 0;
    s->xp_needed = reg.all_of<Experience>(e) ? reg.get<Experience>(e).Needed : 0;
    s->speed = reg.all_of<MoveSpeed>(e) ? reg.get<MoveSpeed>(e).Value : 0.0f;
    s->tier = static_cast<int>(reg.get<BotTier>(e));
}
```

### 6.3 snapshot_bindings.cpp

对应 GDCLASS `_bind_methods` 注册新属性（`ADD_PROPERTY` + `PropertyInfo`）。

### 6.4 血条等级徽章（HealthBarUI）

`health_bar_ui.tscn` 场景树变更（详细属性见 [§8 编辑器操作清单](#8-编辑器操作清单)）：

```
HealthBarUI (Control)              -- 根，尺寸 124×16（原 100×10）
├── LevelBadge (ColorRect)         -- 新增，左侧 22×14，tier 颜色
│   └── LevelLabel (Label)         -- 新增，居中显示等级数字
├── Background (ColorRect)         -- 原有，偏移到 (24, 3)
├── DamageBar (ColorRect)          -- 原有，偏移到 (24, 3)
└── Fill (ColorRect)               -- 原有，偏移到 (24, 3)
```

**布局原理**（解耦关键）：
- 根 Control 扩宽到 124px = 徽章 22px + 2px 间隔 + 血条 100px
- 血条三件套（Background/DamageBar/Fill）整体右移 24px、下移 3px（在 16px 高度内垂直居中）
- `set_screen_position` 以**血条中心**（非根中心）对齐实体头顶 → 徽章视觉上挂在血条左侧

`health_bar_ui.gd` 新增方法：
```gdscript
const TIER_COLORS := {
    0: Color(0.8, 0.2, 0.2),  # Normal 红
    1: Color(0.6, 0.2, 0.8),  # Elite 紫
    2: Color(1.0, 0.8, 0.0),  # Boss 金
}

func update_level(lv: int, tier: int) -> void:
    _level_label.text = str(lv)
    _level_badge.color = TIER_COLORS.get(tier, TIER_COLORS[0])
```

### 6.5 health_bar_manager.gd

`sync_bars` 中，对 bot snapshot 传 level/tier：
```gdscript
if snap.bots[i].dead:
    bar.hide()
else:
    bar.update_hp(bot.hp, bot.max_hp)
    bar.update_level(bot.level, bot.tier)  # 新增
    bar.set_team(2)  # 敌方红
    bar.show()
```

### 6.6 stats_panel.gd

**不改**。只显示玩家自己属性，符合 MOBA 习惯。

---

## 7. Spawner 点位布局

`world.cpp` `_spawn_pickup_spawners()` 重排：

### 7.1 XP 生成器（8 个）

```
Vec2{-35,-35},  Vec2{ 35,-35},
Vec2{-35, 35},  Vec2{ 35, 35},
Vec2{-35,  0},  Vec2{ 35,  0},
Vec2{  0,-35},  Vec2{  0, 35},
```
8 个点分散在地图边缘和轴向，玩家和 bot 需要离开中心区域去吃。

### 7.2 大血包生成器（2 个）

```
Vec2{-20,-20},
Vec2{ 20, 20},
```
仅对角 2 个，25s 刷新。玩家需要跨地图才能吃第二个，bot 也会抢 → 血包争夺。

### 7.3 小血包生成器（2 个）

```
Vec2{-10, 10},
Vec2{ 10,-10},
```
中心附近偏 10 单位，20s 刷新。"救命稻草"定位。

---

## 8. 编辑器操作清单

> 本方案只需修改 **1 个场景文件**（`health_bar_ui.tscn`），无需新增场景、无需改 `main.tscn`。
> 所有 bot AI 逻辑在 C++ Sim 层，不涉及 Godot 编辑器。

### 8.1 需要编辑的场景文件

| 场景文件 | 操作 | 原因 |
|---------|------|------|
| `scenes/ui/health_bar_ui.tscn` | **修改**：加 2 个子节点 + 调整根尺寸和血条偏移 | 显示等级徽章 + tier 颜色 |
| `scenes/main.tscn` | **不改** | HealthBarManager 节点已存在（todo #14 已完成） |
| `scenes/entities/*.tscn` | **不改** | bot/player 实体外观不变，tier 通过血条颜色区分而非模型 |

### 8.2 `health_bar_ui.tscn` 详细编辑步骤

#### 步骤 1：修改根节点 HealthBarUI 尺寸

| 属性 | 旧值 | 新值 | 原因 |
|------|------|------|------|
| `custom_minimum_size` | `Vector2(100, 10)` | `Vector2(124, 16)` | 容纳左侧徽章 + 垂直居中 |
| `offset_right` | `100.0` | `124.0` | 匹配最小尺寸 |
| `offset_bottom` | `10.0` | `16.0` | 匹配最小尺寸 |
| `mouse_filter` | `2` (IGNORE) | `2` (IGNORE) | 不变 |

#### 步骤 2：新增 LevelBadge 节点

在 HealthBarUI 根节点下新增子节点：

| 属性 | 值 | 说明 |
|------|-----|------|
| 节点类型 | `ColorRect` | |
| 节点名 | `LevelBadge` | 脚本通过 `$LevelBadge` 引用 |
| `layout_mode` | `1` (Anchors) | |
| `anchors_preset` | `0` (Custom) | 手动定位 |
| `offset_left` | `0.0` | 左上角 |
| `offset_top` | `1.0` | 垂直留 1px 边距 |
| `offset_right` | `22.0` | 宽 22px |
| `offset_bottom` | `15.0` | 高 14px |
| `color` | `Color(0.8, 0.2, 0.2, 1)` | 默认 Normal 红，运行时由脚本覆盖 |
| `mouse_filter` | `2` (IGNORE) | 不拦截鼠标 |

#### 步骤 3：新增 LevelLabel 节点

作为 **LevelBadge 的子节点**新增：

| 属性 | 值 | 说明 |
|------|-----|------|
| 节点类型 | `Label` | |
| 节点名 | `LevelLabel` | 脚本通过 `$LevelBadge/LevelLabel` 引用 |
| `layout_mode` | `1` (Anchors) | |
| `anchors_preset` | `15` (FULL_RECT) | 填满父节点 LevelBadge |
| `anchor_right` | `1.0` | |
| `anchor_bottom` | `1.0` | |
| `grow_horizontal` | `2` (Grow Both) | |
| `grow_vertical` | `2` (Grow Both) | |
| `horizontal_alignment` | `1` (CENTER) | 文字水平居中 |
| `vertical_alignment` | `1` (CENTER) | 文字垂直居中 |
| `text` | `1` | 默认显示 "1" |
| `theme_override_font_sizes/font_size` | `11` | 小字体适配 14px 高度 |
| `theme_override_colors/font_color` | `Color(1, 1, 1, 1)` | 白字 |
| `mouse_filter` | `2` (IGNORE) | 不拦截鼠标 |

#### 步骤 4：调整 Background 节点偏移

| 属性 | 旧值 | 新值 | 原因 |
|------|------|------|------|
| `anchors_preset` | `15` (FULL_RECT) | `0` (Custom) | 改为手动定位 |
| `anchor_right` | `1.0` | `0.0` | 清除锚点 |
| `anchor_bottom` | `1.0` | `0.0` | 清除锚点 |
| `grow_horizontal` | `2` | `0` (Grow Begin) | 清除 |
| `grow_vertical` | `2` | `0` (Grow Begin) | 清除 |
| `offset_left` | （隐含 0） | `24.0` | 右移 24px 让出徽章位置 |
| `offset_top` | （隐含 0） | `3.0` | 下移 3px 垂直居中 |
| `offset_right` | （隐含 100） | `124.0` | 24+100=124 |
| `offset_bottom` | （隐含 10） | `13.0` | 3+10=13 |
| `color` | `Color(0.1, 0.1, 0.1, 0.8)` | 不变 | |
| `mouse_filter` | `2` | `2` | 不变 |

#### 步骤 5：调整 DamageBar 节点偏移

| 属性 | 旧值 | 新值 | 原因 |
|------|------|------|------|
| `offset_left` | `0.0` | `24.0` | 右移 |
| `offset_top` | `0.0` | `3.0` | 下移 |
| `offset_right` | `100.0` | `124.0` | 24+100 |
| `offset_bottom` | `10.0` | `13.0` | 3+10 |
| `color` | `Color(1, 0.8, 0, 1)` | 不变 | |
| `mouse_filter` | `2` | `2` | 不变 |

#### 步骤 6：调整 Fill 节点偏移

| 属性 | 旧值 | 新值 | 原因 |
|------|------|------|------|
| `offset_left` | `0.0` | `24.0` | 右移 |
| `offset_top` | `0.0` | `3.0` | 下移 |
| `offset_right` | `100.0` | `124.0` | 24+100 |
| `offset_bottom` | `10.0` | `13.0` | 3+10 |
| `color` | `Color(0.2, 1, 0.2, 1)` | 不变 | 运行时由脚本覆盖 |
| `mouse_filter` | `2` | `2` | 不变 |

#### 步骤 7：保存场景

Ctrl+S 保存。Godot 会自动生成/更新 `.uid` 文件。

### 8.3 最终场景树验证

编辑完成后，场景树应为：

```
HealthBarUI (Control)              custom_minimum_size = (124, 16)
├── LevelBadge (ColorRect)         offset = (0, 1, 22, 15), color = 红
│   └── LevelLabel (Label)         FULL_RECT, 居中, font_size=11
├── Background (ColorRect)         offset = (24, 3, 124, 13)
├── DamageBar (ColorRect)          offset = (24, 3, 124, 13)
└── Fill (ColorRect)               offset = (24, 3, 124, 13)
```

### 8.4 解耦设计原则

| 原则 | 实现 |
|------|------|
| **场景是视觉布局的唯一真源** | LevelBadge/LevelLabel 在 `.tscn` 中定义，不在代码中 `add_child` |
| **脚本只读不建** | `health_bar_ui.gd` 通过 `@onready var _level_badge = $LevelBadge` 引用，不创建节点 |
| **Manager 不知徽章存在** | `health_bar_manager.gd` 只调 `bar.update_level(lv, tier)`，不知徽章内部结构 |
| **tier 颜色不在场景硬编码** | `.tscn` 中 LevelBadge 颜色只是默认值，运行时由 `update_level` 按 tier 覆盖 |
| **血条定位以血条中心为准** | `set_screen_position` 以血条（非根 Control）中心对齐实体头顶 |
| **fallback 路径同步** | `_create_bar()` 代码 fallback（`health_bar_manager.gd:73-101`）需同步加 LevelBadge/LevelLabel 构建，保持与 `.tscn` 一致 |

### 8.5 `_create_bar()` fallback 同步

`health_bar_manager.gd` 中的 `_create_bar()` 在 `health_bar_scene` 加载失败时用代码构建血条。此 fallback **必须与 `.tscn` 保持同步**，否则 fallback 路径产出的血条无徽章：

```gdscript
# _create_bar() 中，在 add_child(fill) 之后新增：
var badge := ColorRect.new()
badge.name = "LevelBadge"
badge.position = Vector2(0, 1)
badge.size = Vector2(22, 14)
badge.color = Color(0.8, 0.2, 0.2)
badge.mouse_filter = Control.MOUSE_FILTER_IGNORE
bar.add_child(badge)

var lbl := Label.new()
lbl.name = "LevelLabel"
lbl.anchors_preset = Control.PRESET_FULL_RECT
lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
lbl.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
lbl.text = "1"
lbl.add_theme_font_size_override("font_size", 11)
lbl.add_theme_color_override("font_color", Color.WHITE)
lbl.mouse_filter = Control.MOUSE_FILTER_IGNORE
badge.add_child(lbl)

# 同时调整 bg/db/fill 的 position 和 size：
bg.position = Vector2(24, 3)
bg.size = Vector2(100, 10)
db.position = Vector2(24, 3)
db.size = Vector2(100, 10)
fill.position = Vector2(24, 3)
fill.size = Vector2(100, 10)
bar.custom_minimum_size = Vector2(124, 16)
```

### 8.6 `health_bar_ui.gd` 改动

新增 `@onready` 引用 + `update_level` + `reset` 同步 + `set_screen_position` 修正：

```gdscript
const BAR_WIDTH := 100.0
const BAR_HEIGHT := 10.0
const BAR_OFFSET_X := 24.0   # 新增：血条在根内的 x 偏移
const BAR_OFFSET_Y := 3.0    # 新增：血条在根内的 y 偏移
const DAMAGE_LERP_SPEED := 3.0

const TIER_COLORS := {
    0: Color(0.8, 0.2, 0.2),
    1: Color(0.6, 0.2, 0.8),
    2: Color(1.0, 0.8, 0.0),
}

@onready var _level_badge: ColorRect = $LevelBadge
@onready var _level_label: Label = $LevelBadge/LevelLabel

func update_level(lv: int, tier: int) -> void:
    _level_label.text = str(lv)
    _level_badge.color = TIER_COLORS.get(tier, TIER_COLORS[0])

func set_screen_position(screen_pos: Vector2) -> void:
    # 以血条中心（非根中心）对齐 screen_pos
    position = screen_pos - Vector2(BAR_OFFSET_X + BAR_WIDTH * 0.5, BAR_OFFSET_Y + BAR_HEIGHT * 0.5)

func reset() -> void:
    # … 原有重置 …
    _level_label.text = "1"
    _level_badge.color = TIER_COLORS[0]
```

### 8.7 `health_bar_manager.gd` 改动

`sync_bars` 中对 bot 传 level/tier，对 player 传 level=玩家等级/tier=0：

```gdscript
for p in snap.players:
    seen[p.id] = true
    var bar := _get_or_create(p.id)
    bar.update_hp(p.hp, p.max_hp)
    bar.update_level(p.level, 0)  # 玩家 tier=Normal
    bar.set_team(0)

for b in snap.bots:
    seen[b.id] = true
    var bar := _get_or_create(b.id)
    if b.dead:
        bar.visible = false
    else:
        bar.visible = true
        bar.update_hp(b.hp, b.max_hp)
        bar.update_level(b.level, b.tier)  # 新增
        bar.set_team(2)
```

### 8.8 无需编辑器操作的部分

| 项目 | 理由 |
|------|------|
| `main.tscn` | HealthBarManager 节点已存在（`main.tscn:40`），CanvasLayer 已存在 |
| bot/player/arrow 实体场景 | tier 通过血条颜色区分，不改 3D 模型 |
| pickup 场景 | pickup 数量/位置在 C++ `world.cpp` 中定义，非场景 |
| 新增场景文件 | 无需——没有新视觉组件需要独立预制 |
| Project Settings | 无新输入映射、无新 autoload、无新 layer |

---

## 9. 文件改动清单

### C++ Sim 层（11 文件）

| 文件 | 改动 |
|------|------|
| `src_cpp/sim/components.h` | 新增 `BotTier` enum、`BotBehaviorState` struct |
| `src_cpp/sim/game_config.h` | 新增 Bot 等级成长常量、Tier 倍率、决策树参数、击杀 XP 参数；修改 XP/血包经济常量 |
| `src_cpp/sim/world.cpp` | `_spawn_bot` 首次出生 roll tier+level + emplace 新组件；`_spawn_pickup_spawners` 重排点位和数量 |
| `src_cpp/sim/world.h` | 如需新增 private 方法（如 `_roll_tier` / `_rand_level` / `_apply_bot_tier_stats`）则加 |
| `src_cpp/sim/systems/bot_ai.h` | **重写**：`bot_decision_system`（决策树 + 移动）+ 复活 roll tier+level 应用属性 |
| `src_cpp/sim/systems/xp_helper.h` | **新建**：`apply_xp()` 通用函数，抽取自 `pickup.h` 升级循环 |
| `src_cpp/sim/systems/progression.h` | 新增击杀 XP 发放逻辑，调用 `apply_xp`；原有 Atk/Asp 保留 |
| `src_cpp/sim/systems/pickup.h` | XP 拾取循环改为调用 `apply_xp`（复用） |
| `src_cpp/sim/snapshot_types.h` | `SimBotSnap` + `xp/xp_needed/speed/tier` 字段 + getter/setter |
| `src_cpp/sim/snapshot_bindings.cpp` | 注册 `SimBotSnap` 新属性到 GDCLASS `_bind_methods` |
| `src_cpp/sim/snapshot_builder.cpp` | `_build_bots` 导出新字段 |

### Godot 编辑器场景（1 文件，操作步骤见 [§8](#8-编辑器操作清单)）

| 文件 | 改动 |
|------|------|
| `scenes/ui/health_bar_ui.tscn` | 根尺寸 100×10→124×16；新增 LevelBadge + LevelLabel 子节点；Background/DamageBar/Fill 偏移到 (24,3) |

### GDScript 视图层（2 文件）

| 文件 | 改动 |
|------|------|
| `scripts/ui/health_bar_ui.gd` | 新增 `@onready` 引用 LevelBadge/LevelLabel；新增 `update_level(lv, tier)` + `TIER_COLORS` 常量；`set_screen_position` 改为以血条中心对齐；`reset` 同步徽章 |
| `scripts/ui/health_bar_manager.gd` | `sync_bars` 传 `bar.update_level(p.level, 0)` / `bar.update_level(b.level, b.tier)`；`_create_bar()` fallback 同步构建 LevelBadge/LevelLabel |

### 不动的文件

| 文件 | 理由 |
|------|------|
| `scenes/main.tscn` | HealthBarManager 节点已存在（`main.tscn:40`），CanvasLayer 已存在 |
| `scenes/entities/*.tscn` | bot/player/arrow/pickup 实体外观不变，tier 通过血条颜色区分 |
| `bot_targeting.h` | 目标选择逻辑不变（min HP → dist → random） |
| `bot_combat.h` | 射击方向独立于 FacingAngle，逻辑不变 |
| `combat.h` | 伤害/击杀事件逻辑不变 |
| `arrow_movement.h` / `wall_collision.h` / `player*.h` | 无关 |
| `scripts/ui/stats_panel.gd` | 只显示玩家，不改 |
| `scripts/view/entity_manager.gd` / `entity_view.gd` | 不改（SimBotSnap 新增字段只影响 health_bar_manager） |
| `scripts/sim_bridge.gd` | 不改（已调度 all systems，已 preload health_bar_scene） |
| `scripts/input/input_collector.gd` / `scripts/view/camera_controller.gd` | 无关 |

---

## 10. 风险与对策

### 10.1 Bot 聚堆抢同一 pickup

**风险**：全图感知下，多个 bot 可能同时锁定同一个最近 pickup，造成聚堆。

**对策**：决策时取 top-3 最近 pickup 中随机一个（见 2.3 step3）。无法完全消除聚堆但大幅降低概率。

### 10.2 Elite/Boss 数值不平衡

**风险**：Lv30 Boss（HP 1128, ATK 70）可能秒杀玩家。

**对策**：
1. 血条等级徽章 + tier 颜色让玩家**一眼看出威胁等级**
2. 金血条 = Boss → 玩家可以选择规避
3. 调优倍率在落地后试跑，必要时下调 BossAtkMul 到 2.0 等

### 10.3 决策性能

**风险**：每 tick 扫描所有 pickup（全图最多 12 个活跃 pickup）做距离排序，5 个 bot × 12 pickup × 30Hz = 1800 次距离计算/秒，性能无关紧要。决策 cooldown=0.3s 进一步降低到 5×12×3=180/s。

只要不在 `_build_xxx` 或其他高频路径里做，无优化必要。

### 10.4 血包死锁（bot 守血包）

**风险**：高等级 bot 蹲在血包点不走，玩家永远吃不到。

**对策**：`FLEE` 状态优先级高于 `SEEK_HEAL`——只有 HP<60% 且无致命威胁才找血包。战斗中 bot 不会蹲血包。此外 pickup 被吃掉后 20~25s 才刷新，不蹲守。

### 10.5 首次出生 Elite/Boss 堵脸

**风险**：开局就刷 Lv30 Boss 在玩家出生点旁边，还没动就被秒。

**对策**：`_spawn_bot` 初始位置均匀分布在 ±map_half 范围内（`world.cpp:211-213`），Boss 不会故意刷在玩家附近。如果玩家出生点恰好附近有 Boss，运气成分——MOBA 野区设计如此，玩家可以绕路发育。

---

## 附录：数据流变更

```
C++ Sim (30Hz)
  ├─ bot_targeting_system     → 选 TargetEntity（不变）
  ├─ bot_decision_system      → 选 Goal + MoveTarget（新，替换 wander）
  ├─ bot_combat_system        → 射箭（不变）
  ├─ pickup_system            → 吃 XP/Heal → apply_xp（新 helper）
  ├─ progression_system       → 杀敌 XP + Atk/Asp（新）
  └─ snapshot_export_system   → SimBotSnap + xp/spd/tier（新字段）
      ↓ Snapshot (30Hz)
GDScript View (60Hz)
  ├─ health_bar_manager
  │   └─ health_bar_ui        → 血条 + 等级徽章 + tier 颜色（新）
  └─ stats_panel              → 玩家面板（不变）
```
