# Sim System Reference — C++ ECS 层完整参考

> 最后更新：2026-07-07
> 关联：`Docs/Reference/prompt.md`、`Docs/Reference/bot_ai_optimization.md`

---

## 目录

1. [架构总览](#1-架构总览)
2. [文件结构](#2-文件结构)
3. [组件清单（Components）](#3-组件清单components)
4. [System 执行顺序与签名](#4-system-执行顺序与签名)
5. [System 详细参考](#5-system-详细参考)
6. [Singleton 组件](#6-singleton-组件)
7. [快照数据格式（Snapshot → GDScript）](#7-快照数据格式snapshot--gdscript)
8. [Godot 绑定层（SimServer）](#8-godot-绑定层simserver)
9. [实体出生组件表](#9-实体出生组件表)
10. [GameConfig 常量参考](#10-gameconfig-常量参考)
11. [MOBA 升级待做清单](#11-moba-升级待做清单)

---

## 1. 架构总览

```
┌────────────────────────────────────────────────────────────┐
│                  C++ Sim Layer (entt ECS)                  │
│                                                            │
│  World (entt::registry)                                    │
│    ├── 12 Systems (header-only, inline 函数)               │
│    ├── 21+ Component types                                 │
│    ├── 4 Singleton entities                                │
│    ├── CommandBuffer (延迟实体创建/销毁)                    │
│    └── 30Hz fixed tick                                     │
│                                                            │
│  输出: SimSnapshot (RefCounted, 每 tick 生成)              │
└──────────────────────┬─────────────────────────────────────┘
                       │ SimSnapshot
                       ▼
┌────────────────────────────────────────────────────────────┐
│              SimServer (GDExtension Binding)                │
│  RefCounted → GDScript 可直接访问 .players/.bots/...       │
│  4 个方法: initialize / set_local_input / tick / pop_snap  │
└──────────────────────┬─────────────────────────────────────┘
                       │
                       ▼
              sim_bridge.gd (GDScript)
```

**核心设计原则：**
- Sim 层零 Godot 渲染依赖，纯逻辑
- 所有 System 是 header-only inline 函数，无 `.cpp` 文件（除 `world.cpp` / `snapshot_builder.cpp` / `snapshot_bindings.cpp` / `sim_server.cpp`）
- 实体创建/销毁通过 `CommandBuffer` 延迟到 tick 末尾执行（防止迭代器失效）
- 快照是 Sim 与 View 的唯一通信通道

---

## 2. 文件结构

```
src_cpp/
├── register_types.h / .cpp        # GDExtension 入口，注册 SimServer + 所有 Snap 类型
├── sim_server.h / .cpp            # Godot 绑定层，Ref<SimServer> 暴露给 GDScript
│
└── sim/
    ├── components.h               # 全部 ECS 组件定义（21 个 struct + 3 个 enum）
    ├── game_config.h              # 全部游戏常量（GameConfig 静态 constexpr）
    ├── vec2.h                     # Vec2 = glm::vec2 + 碰撞/工具函数
    ├── command_buffer.h           # 延迟操作队列
    ├── arrow_spawner.h            # try_fire() 辅助函数
    ├── json_util.h                # 手写 JSON 解析器（仅解析 map 定义）
    │
    ├── world.h / .cpp             # World 类：初始化、tick 循环、实体出生
    │
    ├── snapshot_types.h           # Godot-exposed 快照数据类（6 个 GDCLASS）
    ├── snapshot_builder.h / .cpp  # 从 registry 构建 SimSnapshot
    ├── snapshot_bindings.cpp      # _bind_methods 注册所有快照属性
    │
    └── systems/
        ├── local_input_injection.h  # 1. 输入注入
        ├── player_movement.h        # 2. 玩家移动
        ├── player_fire.h            # 3. 玩家射击
        ├── bot_targeting.h          # 4. Bot 目标选择
        ├── bot_ai.h                 # 5. Bot 决策+移动
        ├── bot_combat.h             # 6. Bot 射击
        ├── arrow_movement.h         # 7. 箭矢运动
        ├── wall_collision.h         # 8. 墙壁碰撞
        ├── combat.h                 # 9. 伤害判定
        ├── pickup.h                 # 10. 拾取物
        ├── progression.h            # 11. 击杀成长
        ├── xp_helper.h              # 共用 XP 升级逻辑
        └── snapshot_export.h        # 12. 快照导出
```

---

## 3. 组件清单（Components）

### 3.1 通用组件

| 组件 | 字段 | 用途 | 依附实体 |
|------|------|------|---------|
| `Position2D` | `Vec2 Value` | 2D 世界坐标 | Player, Bot, Arrow, Pickup |
| `Velocity2D` | `Vec2 Value` | 2D 速度向量 | Arrow |
| `FacingAngle` | `float Radians` | 朝向弧度 | Player, Bot, Arrow |
| `Health` | `int Cur, Max` | 生命值 | Player, Bot |
| `Dead` | `bool enabled` | 死亡标记 | Player, Bot |
| `Lifetime` | `float Remaining` | 剩余存活时间 | Arrow |
| `NetworkId` | `int Value` | 网络唯一 ID | Player, Bot, Arrow, Pickup |
| `Damageable` | _(空标记)_ | 可受伤 | Player, Bot |

### 3.2 玩家组件

| 组件 | 字段 | 用途 |
|------|------|------|
| `PlayerTag` | `bool IsLocal` | 玩家标记 + 是否本地 |
| `PlayerInputState` | `Vec2 Move, Aim; bool Fire; int Seq` | 当前帧输入 |
| `CombatStats` | `float Atk, Asp; double LastFireTime` | 战斗属性 |
| `Kills` | `int Value` | 击杀计数 |

### 3.3 Bot 组件

| 组件 | 字段 | 用途 |
|------|------|------|
| `BotTag` | _(空标记)_ | Bot 标记 |
| `BotTier` | `enum: Normal/Elite/Boss` | 难度等级 |
| `BotBehaviorState` | `Goal Current; entity PickupTarget; float DecisionCooldown; KiteSub Kite; int StrafeDir; float GoalCommitTimer` | 行为决策状态 |
| `BotAIState` | `Vec2 MoveTarget; float RespawnTimer; entity TargetEntity; float WanderTimer; float TargetLockTimer` | AI 运行时状态 |
| `BotVisionRange` | `float Value` | 视野半径 |

### 3.4 箭矢组件

| 组件 | 字段 | 用途 |
|------|------|------|
| `ArrowTag` | `int OwnerId; entity OwnerEntity; float Dmg` | 箭矢标记 + 伤害归属 |

### 3.5 墙壁组件

| 组件 | 字段 | 用途 |
|------|------|------|
| `WallTag` | _(空标记)_ | 墙壁标记 |
| `WallBounds` | `Vec2 Min, Max` | AABB 边界 |

### 3.6 拾取物组件

| 组件 | 字段 | 用途 |
|------|------|------|
| `PickupTag` | `PickupType Type; int Value` | 拾取物标记 |
| `PickupSpawner` | `PickupType Type; int Value; Vec2 Position; float RespawnTime; float CurrentTimer; bool Active; int CurrentEntityId` | 生成器 |

### 3.7 成长组件

| 组件 | 字段 | 用途 |
|------|------|------|
| `Level` | `int Value` | 当前等级 |
| `Experience` | `int Cur, Needed` | 经验值 |
| `MoveSpeed` | `float Value` | 移动速度 |

### 3.8 枚举

| 枚举 | 值 | 用途 |
|------|-----|------|
| `BotTier` | `Normal=0, Elite=1, Boss=2` | Bot 难度 |
| `BotBehaviorState::Goal` | `Flee=0, SeekHeal=1, SeekXp=2, Engage=3, Wander=4` | 行为目标 |
| `BotBehaviorState::KiteSub` | `Chase, Strafe, Retreat` | 战斗子状态 |
| `PickupType` | `Xp=0, Heal=1, SmallHeal=2` | 拾取物类型 |

---

## 4. System 执行顺序与签名

在 `World::tick()` 中按以下顺序执行，每 tick 调用一次：

| # | System | 函数签名 | 源文件 | 读写概要 |
|---|--------|---------|--------|---------|
| 1 | LocalInputInjection | `void (registry&, entity)` | `local_input_injection.h` | 读 LocalInputSingleton → 写 PlayerInputState |
| 2 | PlayerMovement | `void (registry&, float dt, float map_half)` | `player_movement.h` | 读 PlayerInputState/MoveSpeed → 写 Position2D/FacingAngle |
| 3 | PlayerFire | `void (registry&, double now, CommandBuffer&, IdState&)` | `player_fire.h` | 读 PlayerInputState/CombatStats → 写 CommandBuffer(创建箭矢) |
| 4 | BotTargeting | `void (registry&, mt19937&, float dt)` | `bot_targeting.h` | 读 Position2D/Health/BotVisionRange → 写 BotAIState.TargetEntity |
| 5 | BotAI | `void (registry&, float dt, float map_half, mt19937&)` | `bot_ai.h` | 读 BotBehaviorState/Health → 写 Position2D/FacingAngle/BotAIState |
| 6 | BotCombat | `void (registry&, double now, CommandBuffer&, IdState&)` | `bot_combat.h` | 读 BotAIState/CombatStats → 写 CommandBuffer(创建箭矢) |
| 7 | ArrowMovement | `void (registry&, float dt)` | `arrow_movement.h` | 读 Velocity2D → 写 Position2D/Lifetime |
| 8 | WallCollision | `void (registry&, CommandBuffer&)` | `wall_collision.h` | 读 WallBounds → 写 Position2D(推回) / CommandBuffer(销毁箭矢) |
| 9 | Combat | `void (registry&, CommandBuffer&)` | `combat.h` | 读 ArrowTag/Position2D → 写 Health/Dead/KillEventBuffer |
| 10 | Pickup | `void (registry&, float dt, CommandBuffer&, IdState&)` | `pickup.h` | 读 PickupSpawner/PickupTag → 写 Health/Experience/CommandBuffer |
| 11 | Progression | `void (registry&)` | `progression.h` | 读 KillEventBuffer → 写 CombatStats/Experience/Level/MoveSpeed/Health |
| 12 | SnapshotExport | `bool (registry&, int&, Ref<SimSnapshot>&)` | `snapshot_export.h` | 读全部 → 写 SimSnapshot |

**所有 System 执行完毕后**，`CommandBuffer::flush()` 执行延迟的实体创建/销毁。

---

## 5. System 详细参考

### 5.1 LocalInputInjectionSystem

```
文件: systems/local_input_injection.h (22 行)
函数: local_input_injection_system(registry &reg, entity local_input_entity)
```

**逻辑：**
1. 读取 `LocalInputSingleton` 组件（从 `local_input_entity`）
2. 遍历所有 `PlayerTag + PlayerInputState`
3. 若 `PlayerTag.IsLocal == true`：将 Singleton 的 Move/Aim/Fire/Seq 复制到 PlayerInputState

**设计意图：** 将输入源与消费者解耦——未来网络模式下，远程玩家输入从网络包注入，不经过此 System。

---

### 5.2 PlayerMovementSystem

```
文件: systems/player_movement.h (30 行)
函数: player_movement_system(registry &reg, float dt, float map_half)
```

**逻辑：**
1. 遍历 `PlayerTag + Position2D + FacingAngle + PlayerInputState + MoveSpeed`
2. 仅处理 `IsLocal == true` 的玩家
3. 若 `length(input.Move) > 0.01`：
   - `dir = normalize(input.Move)`
   - `angle.Radians = atan2(dir.y, dir.x)`
   - `pos.Value = clamp_to_map(pos + dir * speed * dt, map_half)`

---

### 5.3 PlayerFireSystem

```
文件: systems/player_fire.h (37 行)
函数: player_fire_system(registry &reg, double now, CommandBuffer &cb, IdState &ids)
```

**逻辑：**
1. 遍历 `PlayerTag + Position2D + PlayerInputState + CombatStats + NetworkId`
2. 仅处理 `IsLocal == true` 且 `input.Fire == true`
3. 计算 `aim_dir = normalize(input.Aim - pos.Value)`
4. 构造 `ArrowSpawnContext`，调用 `try_fire(stats, ctx)`

**`try_fire()` 逻辑（`arrow_spawner.h`）：**
1. 冷却检查：`now - stats.LastFireTime < 1.0 / stats.Asp` → 跳过
2. 更新 `stats.LastFireTime = now`
3. 分配 `arrow_id = ids.NextArrowId++`
4. 通过 `CommandBuffer` 延迟创建箭矢实体：
   - `Position2D(spawn_pos)`, `Velocity2D(cos/sin * ArrowSpeed)`, `FacingAngle(angle)`
   - `ArrowTag(owner_id, owner_entity, dmg)`, `Lifetime(ArrowLifetime)`, `NetworkId(arrow_id)`

---

### 5.4 BotTargetingSystem

```
文件: systems/bot_targeting.h (100 行)
函数: bot_targeting_system(registry &reg, mt19937 &rng, float dt)
```

**逻辑：**
1. 遍历所有 Bot（`BotTag + Position2D + BotVisionRange + BotAIState`）
2. 跳过已死亡的 Bot
3. **目标锁定：** 若当前 `TargetEntity` 有效且仍在视野内 → 锁定 `BotTargetLockTime`(2s)
4. 若锁定期满或目标无效 → 重新扫描：
   - 收集视野内所有 alive 的 `Damageable` 实体
   - 选择优先级：**min HP** → **min 距离** → **random**
5. 设置 `ai.TargetEntity` + 重置 `TargetLockTimer`

---

### 5.5 BotAISystem

```
文件: systems/bot_ai.h (364 行)
函数: bot_ai_system(registry &reg, float dt, float map_half, mt19937 &rng)
```

**逻辑（最复杂的 System）：**

**死亡处理（优先）：**
- `RespawnTimer -= dt`，归零后：
  - 随机等级 1~30，Roll Tier (Boss 5% / Elite 15% / Normal 80%)
  - 按 Tier 倍率计算 HP/ATK/ASP/Speed/Vision
  - 随机重生位置，重置 AI 状态

**决策（每 0.3s 一次，DecisionCooldown）：**
1. 扫描全图 Pickup（Heal + SmallHeal 合并排序 + XP）
2. 计算视野内最近敌人
3. **Goal 选择（优先级高→低）：**
   - `Flee`：HP<30% 且敌人在视野 → 远离敌人 30 单位
   - `SeekHeal`：HP<60% 且有血包 → top-3 随机选一个
   - `SeekXp`：无战斗目标或有 XP pickup → top-3 随机选
   - `Engage`：有目标 → Kiting 状态机
   - `Wander`：随机地图点
4. **Goal 承诺计时器 (0.8s)：** 防止频繁切换，仅紧急条件(Flee)可打断

**Kiting 状态机（滞回）：**
| 状态 | 进入条件 | 行为 |
|------|---------|------|
| Chase | dist > vision×0.85 | 朝目标移动 |
| Strafe | 中间距离 | 垂直方向横移（随机左右） |
| Retreat | dist < vision×0.25 | 远离目标 |

**移动执行：**
- `dir = normalize(target_pos - pos)`
- `pos = clamp_to_map(pos + dir * min(move_dist, speed*dt), map_half)`
- `angle.Radians = atan2(dir.y, dir.x)`

---

### 5.6 BotCombatSystem

```
文件: systems/bot_combat.h (41 行)
函数: bot_combat_system(registry &reg, double now, CommandBuffer &cb, IdState &ids)
```

**逻辑：**
1. 遍历 `BotTag + Position2D + BotAIState + NetworkId + CombatStats`
2. 跳过死亡 / 无目标 / 目标无效
3. 计算 `to_target` 方向（射击方向独立于移动朝向）
4. 构造 `ArrowSpawnContext`，调用 `try_fire(stats, ctx)`

---

### 5.7 ArrowMovementSystem

```
文件: systems/arrow_movement.h (20 行)
函数: arrow_movement_system(registry &reg, float dt)
```

**逻辑：**
1. 遍历 `ArrowTag + Position2D + Velocity2D + Lifetime`
2. `pos.Value += vel.Value * dt`
3. `life.Remaining -= dt`

---

### 5.8 WallCollisionSystem

```
文件: systems/wall_collision.h (50 行)
函数: wall_collision_system(registry &reg, CommandBuffer &cb)
```

**逻辑：**
1. 收集所有 `WallBounds` 到 `std::vector`
2. **Mover 碰撞（Player/Bot）：**
   - `Damageable + Position2D`，跳过死亡
   - 对每面墙调用 `resolve_circle_aabb(pos, radius, wall.Min, wall.Max)`
   - 推出重叠，更新 `Position2D`
3. **Arrow 碰撞：**
   - `ArrowTag + Position2D`
   - 若箭矢中心在任何墙内 → `CommandBuffer` 延迟销毁

---

### 5.9 CombatSystem

```
文件: systems/combat.h (77 行)
函数: combat_system(registry &reg, CommandBuffer &cb)
```

**逻辑：**
1. 遍历 `ArrowTag + NetworkId + Position2D + Lifetime`
2. 过期箭矢（`Lifetime <= 0`）→ `CommandBuffer` 销毁
3. 对每支箭矢，遍历 `Damageable + Position2D + Health`：
   - 跳过自身 `OwnerEntity`
   - 跳过已死亡
   - `circles_overlap(arrow_pos, ArrowRadius, target_pos, target_radius)` 检测
4. **命中：**
   - `hp.Cur -= arrow_tag.Dmg`
   - 若 `hp.Cur <= 0`：标记 `Dead`，设置 `RespawnTimer`
   - 写入 `KillEvent(KillerId, VictimId)` 到 `KillEventBuffer`
   - 递增击杀者 `Kills`
   - `CommandBuffer` 销毁箭矢

---

### 5.10 PickupSystem

```
文件: systems/pickup.h (88 行)
函数: pickup_system(registry &reg, float dt, CommandBuffer &cb, IdState &ids)
```

**Phase 1 — Spawner Tick：**
1. 遍历 `PickupSpawner`，跳过 `Active`
2. `CurrentTimer -= dt`，归零后：
   - 分配 pickup ID
   - 标记 `Active = true`
   - `CommandBuffer` 创建 `PickupTag + Position2D + NetworkId` 实体

**Phase 2 — Mover Overlap：**
1. 遍历 `PickupTag + Position2D + NetworkId`，对每个 pickup：
2. 遍历 `Damageable + Position2D`（跳过自身和死亡）：
3. `circles_overlap` 检测
4. **效果：**
   - XP → `apply_xp(reg, mover, value)`
   - Heal/SmallHeal → `hp.Cur = min(hp.Cur + ceil(hp.Max * fraction), hp.Max)`
5. `CommandBuffer` 销毁 pickup
6. 反查 Spawner → `Active = false`，重置 `CurrentTimer = RespawnTime`

---

### 5.11 ProgressionSystem

```
文件: systems/progression.h (62 行)
函数: progression_system(registry &reg)
```

**逻辑：**
1. 读取 `KillEventBuffer` 中的所有事件
2. 对每个 KillEvent：
   - **ATK/ASP 成长：** 查找 `NetworkId == KillerId` → `Atk += AtkPerKill`, `Asp += AspPerKill`（上限 AspMax）
   - **击杀 XP：** 查找 victim 和 killer 的 Level
   - `kill_xp = KillXpBase × victim_lv × (1 + max(0, victim_lv - killer_lv) × KillXpHighBonus)`
   - 调用 `apply_xp(reg, killer_entity, kill_xp)`
3. 清空 `KillEventBuffer`

---

### 5.12 SnapshotExportSystem

```
文件: systems/snapshot_export.h (18 行)
函数: snapshot_export_system(registry &reg, int &tick_counter, Ref<SimSnapshot> &out_snap)
```

**逻辑：**
1. `tick_counter++`
2. 每 tick 都生成快照（`tick_counter % 1 == 0`）
3. 调用 `SnapshotBuilder::build(reg, tick_counter)` 构建完整快照
4. 返回 `true` 表示有新快照

---

## 6. Singleton 组件

这些组件挂载在专用实体上，全局唯一：

| 实体变量 | 组件 | 用途 |
|---------|------|------|
| `_local_input_entity` | `LocalInputSingleton` | 当前帧输入：Move/Aim/Fire/Seq |
| `_map_bounds_entity` | `MapBounds` | 地图半径 Half |
| `_id_state_entity` | `IdState` | 自增 ID 分配器（4 种实体类型） |
| `_kill_event_entity` | `KillEventBuffer` | 击杀事件队列（tick 末清空） |

**ID 分配范围：**
| 类型 | 起始值 | 变量 |
|------|--------|------|
| Player | 1 | `IdState::NextPlayerId` |
| Bot | 1001 | `IdState::NextBotId` |
| Arrow | 2001 | `IdState::NextArrowId` |
| Pickup | 3001 | `IdState::NextPickupId` |

---

## 7. 快照数据格式（Snapshot → GDScript）

### 7.1 类型关系

```
SimSnapshot (RefCounted)
├── seq: int                    # 快照序号
├── t: int64                    # 时间戳 ms
├── players: TypedArray<SimPlayerSnap>
├── bots: TypedArray<SimBotSnap>
├── arrows: TypedArray<SimArrowSnap>
├── pickups: TypedArray<SimPickupSnap>
└── events: TypedArray<SimEventSnap>
```

### 7.2 SimPlayerSnap

| 字段 | GDScript 类型 | 来源组件 | 说明 |
|------|-------------|---------|------|
| `id` | `int` | `NetworkId.Value` | 唯一 ID |
| `x` | `float` | `Position2D.Value.x` | 世界 X |
| `y` | `float` | `Position2D.Value.y` | 世界 Y |
| `ang` | `float` | `FacingAngle.Radians` | 朝向弧度 |
| `hp` | `int` | `Health.Cur` | 当前 HP |
| `max_hp` | `int` | `Health.Max` | 最大 HP |
| `atk` | `float` | `CombatStats.Atk` | 攻击力 |
| `asp` | `float` | `CombatStats.Asp` | 攻速 |
| `speed` | `float` | `MoveSpeed.Value` | 移动速度 |
| `kills` | `int` | `Kills.Value` | 击杀数 |
| `level` | `int` | `Level.Value` | 等级 |
| `xp` | `int` | `Experience.Cur` | 当前经验 |
| `xp_needed` | `int` | `Experience.Needed` | 升级所需 |

### 7.3 SimBotSnap

| 字段 | GDScript 类型 | 来源组件 | 说明 |
|------|-------------|---------|------|
| `id` | `int` | `NetworkId.Value` | 唯一 ID |
| `x` | `float` | `Position2D.Value.x` | 世界 X |
| `y` | `float` | `Position2D.Value.y` | 世界 Y |
| `ang` | `float` | `FacingAngle.Radians` | 朝向弧度 |
| `hp` | `int` | `Health.Cur` | 当前 HP |
| `max_hp` | `int` | `Health.Max` | 最大 HP |
| `dead` | `bool` | `Dead.enabled` | 是否死亡 |
| `atk` | `float` | `CombatStats.Atk` | 攻击力 |
| `asp` | `float` | `CombatStats.Asp` | 攻速 |
| `kills` | `int` | `Kills.Value` | 击杀数 |
| `level` | `int` | `Level.Value` | 等级 |
| `xp` | `int` | `Experience.Cur` | 当前经验 |
| `xp_needed` | `int` | `Experience.Needed` | 升级所需 |
| `speed` | `float` | `MoveSpeed.Value` | 移动速度 |
| `tier` | `int` | `BotTier` (cast) | 0=Normal 1=Elite 2=Boss |

### 7.4 SimArrowSnap

| 字段 | GDScript 类型 | 来源组件 | 说明 |
|------|-------------|---------|------|
| `id` | `int` | `NetworkId.Value` | 唯一 ID |
| `x` | `float` | `Position2D.Value.x` | 世界 X |
| `y` | `float` | `Position2D.Value.y` | 世界 Y |
| `ang` | `float` | `FacingAngle.Radians` | 朝向弧度 |

### 7.5 SimPickupSnap

| 字段 | GDScript 类型 | 来源组件 | 说明 |
|------|-------------|---------|------|
| `id` | `int` | `NetworkId.Value` | 唯一 ID |
| `x` | `float` | `Position2D.Value.x` | 世界 X |
| `y` | `float` | `Position2D.Value.y` | 世界 Y |
| `type` | `int` | `PickupTag.Type` (cast) | 0=Xp 1=Heal 2=SmallHeal |
| `value` | `int` | `PickupTag.Value` | 数值 |

### 7.6 SimEventSnap

| 字段 | GDScript 类型 | 来源 | 说明 |
|------|-------------|------|------|
| `killer_id` | `int` | `KillEvent.KillerId` | 击杀者 NetworkId |
| `victim_id` | `int` | `KillEvent.VictimId` | 受害者 NetworkId |

### 7.7 GDScript 访问模式

```gdscript
# sim_bridge.gd 中
var snap: SimSnapshot = sim.pop_snapshot()
for p in snap.players:
    print(p.id, p.hp, "/", p.max_hp, " Lv", p.level)
for b in snap.bots:
    if not b.dead:
        print(b.id, " tier=", b.tier, " hp=", b.hp)
for a in snap.arrows:
    print(a.id, a.x, a.y)
for pk in snap.pickups:
    print(pk.id, pk.type, pk.value)
for ev in snap.events:
    print(ev.killer_id, " killed ", ev.victim_id)
```

---

## 8. Godot 绑定层（SimServer）

### 8.1 SimServer API

| 方法 | 签名 | 说明 |
|------|------|------|
| `initialize` | `(map_json: String) -> void` | 解析地图 JSON，创建所有初始实体 |
| `set_local_input` | `(move: Vector2, aim: Vector2, fire: bool, seq: int) -> void` | 设置当前帧输入 |
| `tick` | `(delta: float) -> void` | 执行一个 tick（30Hz） |
| `pop_snapshot` | `() -> RefCounted` | 取出最新快照（消费后清空） |

### 8.2 注册的 GDCLASS 类型

```cpp
// register_types.cpp
ClassDB::register_class<SimServer>();
ClassDB::register_class<sim::SimSnapshot>();
ClassDB::register_class<sim::SimPlayerSnap>();
ClassDB::register_class<sim::SimBotSnap>();
ClassDB::register_class<sim::SimArrowSnap>();
ClassDB::register_class<sim::SimPickupSnap>();
ClassDB::register_class<sim::SimEventSnap>();
```

### 8.3 绑定宏

`snapshot_bindings.cpp` 中使用两个宏简化注册：

```cpp
#define BIND(cls, field) \
    ClassDB::bind_method(D_METHOD("get_" #field), &cls::get_##field); \
    ClassDB::bind_method(D_METHOD("set_" #field, "v"), &cls::set_##field);

#define PROP(cls, type, field) \
    ADD_PROPERTY(PropertyInfo(type, #field), "set_" #field, "get_" #field);
```

每个 Snap 类型的 `_bind_methods()` 中对每个字段调用 `BIND` + `PROP`。

---

## 9. 实体出生组件表

### 9.1 Player（`_spawn_player`）

| 组件 | 初始值 |
|------|--------|
| `PlayerTag` | `IsLocal=true` |
| `NetworkId` | `player_id` (从 1 开始) |
| `Position2D` | 随机地图位置 |
| `FacingAngle` | `0.0f` |
| `Health` | `100 / 100` |
| `CombatStats` | `Atk=10, Asp=1.0, LastFireTime=0` |
| `Kills` | `0` |
| `PlayerInputState` | `Move=(0,0), Aim=(0,0), Fire=false, Seq=0` |
| `Damageable` | _(空)_ |
| `Dead` | `false` |
| `Level` | `1` |
| `Experience` | `Cur=0, Needed=500` |
| `MoveSpeed` | `5.0` |

### 9.2 Bot（`_spawn_bot`）

| 组件 | 初始值 |
|------|--------|
| `BotTag` | _(空)_ |
| `NetworkId` | `bot_id` (从 1001 开始) |
| `Position2D` | 随机地图位置 |
| `FacingAngle` | `0.0f` |
| `Health` | `(BotHp + (lv-1)*HpPerLv) * TierHpMul` |
| `BotAIState` | `MoveTarget=random, RespawnTimer=0, TargetEntity=null, WanderTimer=random` |
| `BotBehaviorState` | 默认（Goal=Wander） |
| `BotTier` | 随机 roll |
| `BotVisionRange` | `BotVisionRange * TierVisionMul` |
| `CombatStats` | `Atk/Asp 按等级+Tier计算` |
| `Kills` | `0` |
| `Damageable` | _(空)_ |
| `Dead` | `false` |
| `Level` | 随机 1~30 |
| `Experience` | `Cur=0, Needed=lv*500` |
| `MoveSpeed` | `(BotSpeed + (lv-1)*SpeedPerLv) * TierSpeedMul` |

### 9.3 Arrow（`try_fire` → CommandBuffer 创建）

| 组件 | 初始值 |
|------|--------|
| `Position2D` | `spawn_pos` |
| `Velocity2D` | `(cos/sin * ArrowSpeed)` |
| `FacingAngle` | `aim_angle` |
| `ArrowTag` | `OwnerId, OwnerEntity, Dmg` |
| `Lifetime` | `2.0s` |
| `NetworkId` | `arrow_id` (从 2001 开始) |

### 9.4 Pickup（`_spawn_one_spawner` → PickupSystem 创建）

| 组件 | 初始值 |
|------|--------|
| `NetworkId` | `pickup_id` (从 3001 开始) |
| `Position2D` | spawner.Position |
| `PickupTag` | `Type, Value` |

---

## 10. GameConfig 常量参考

### 10.1 核心

| 常量 | 值 | 说明 |
|------|-----|------|
| `TickRate` | 30.0 | tick 频率 |
| `SnapshotRate` | 20.0 | 快照频率（未使用，当前每 tick 都导出） |
| `MapHalf` | 50.0 | 地图半径 |

### 10.2 玩家

| 常量 | 值 | 说明 |
|------|-----|------|
| `PlayerRadius` | 0.5 | 碰撞半径 |
| `PlayerSpeed` | 5.0 | 基础移动速度 |
| `PlayerBaseHp` | 100 | 基础 HP |
| `BaseAttack` | 10.0 | 基础攻击力 |
| `BaseAttackSpeed` | 1.0 | 基础攻速 |

### 10.3 箭矢

| 常量 | 值 | 说明 |
|------|-----|------|
| `ArrowSpeed` | 20.0 | 箭矢速度 |
| `ArrowLifetime` | 2.0 | 箭矢存活时间 |
| `ArrowRadius` | 0.3 | 箭矢碰撞半径 |

### 10.4 Bot

| 常量 | 值 | 说明 |
|------|-----|------|
| `BotCount` | 5 | 初始 Bot 数量 |
| `BotRadius` | 0.5 | 碰撞半径 |
| `BotSpeed` | 2.0 | 基础速度 |
| `BotHp` | 50 | 基础 HP |
| `BotBaseAttack` | 5.0 | 基础攻击 |
| `BotBaseAttackSpeed` | 0.8 | 基础攻速 |
| `BotRespawnTime` | 3.0 | 重生时间 |
| `BotVisionRange` | 20.0 | 视野范围 |
| `MaxBotLevel` | 30 | 随机等级上限 |
| `BossRoll` | 0.05 | Boss 出现概率 |
| `EliteRoll` | 0.20 | Elite 出现概率 |

### 10.5 成长

| 常量 | 值 | 说明 |
|------|-----|------|
| `AtkPerKill` | 2.0 | 每次击杀 ATK 增量 |
| `AspPerKill` | 0.05 | 每次击杀 ASP 增量 |
| `AspMax` | 4.0 | ASP 上限 |
| `KillXpBase` | 15 | 击杀 XP 基数 |
| `KillXpHighBonus` | 0.5 | 越级击杀奖励系数 |
| `XpPerLevelBase` | 500 | 升级 XP = level × 500 |
| `HpPerLevel` | 10 | 升级 MaxHP 增量 |
| `SpeedPerLevel` | 0.5 | 升级 Speed 增量 |
| `HealFraction` | 0.5 | 大血包回血比例 |

### 10.6 Pickup

| 常量 | 值 | 说明 |
|------|-----|------|
| `XpPickupValue` | 16 | XP pickup 经验值 |
| `HealPickupValue` | 30 | 大血包值（未直接用，用 HealFraction） |
| `SmallHealPickupValue` | 25 | 小血包值（用于 15% 计算） |
| `XpPickupRespawnTime` | 10.0 | XP 刷新时间 |
| `HealPickupRespawnTime` | 25.0 | 大血包刷新时间 |
| `SmallHealPickupRespawnTime` | 20.0 | 小血包刷新时间 |
| `PickupRadius` | 0.5 | 拾取碰撞半径 |
| `XpPickupCount` | 120 | XP 生成器数量 |
| `HealPickupCount` | 2 | 大血包生成器数量 |
| `SmallHealPickupCount` | 2 | 小血包生成器数量 |

---

## 11. MOBA 升级待做清单

> 对应 `prompt.md` §MOBA 大逃杀升级方案
> 每项标注：涉及文件、新增组件、新增 System、Snapshot 扩展、SimServer API 扩展

### P0-1: Mana 系统

**新增组件：**
```cpp
// components.h 新增
struct Mana {
    float Cur = 0.0f;
    float Max = 100.0f;
    float RegenRate = 5.0f;
    float RegenDelay = 3.0f;
    float RegenTimer = 0.0f;
};
```

**新增 System：**
```cpp
// systems/mana_regen.h
inline void mana_regen_system(entt::registry &reg, float dt) {
    auto view = reg.view<Mana>();
    for (auto e : view) {
        auto &mana = view.get<Mana>(e);
        mana.RegenTimer -= dt;
        if (mana.RegenTimer <= 0.0f) {
            mana.Cur = std::min(mana.Cur + mana.RegenRate * dt, mana.Max);
        }
    }
}
```

**新增 GameConfig：**
```cpp
static constexpr float PlayerBaseMana = 100.0f;
static constexpr float PlayerManaRegen = 5.0f;
static constexpr float PlayerManaRegenDelay = 3.0f;
static constexpr float BotBaseMana = 100.0f;
static constexpr float BotManaRegen = 3.0f;
```

**Snapshot 扩展：**
- `SimPlayerSnap` + `mana: float, max_mana: float`
- `SimBotSnap` + `mana: float, max_mana: float`
- `snapshot_builder.cpp` 对应导出
- `snapshot_bindings.cpp` 对应 BIND + PROP

**World 改动：**
- `_spawn_player` 中 emplace `Mana(PlayerBaseMana, PlayerBaseMana, ...)`
- `_spawn_bot` 中 emplace `Mana(BotBaseMana, ...)`
- `tick()` 中插入 `mana_regen_system` 顺序（在 PlayerFire 之后、BotCombat 之后，使技能消耗先扣，再回复）
- Bot 重生时重置 Mana

**SimServer 改动：** 无新 API

**涉及文件：** `components.h`, `game_config.h`, `world.h`, `world.cpp`, `systems/mana_regen.h`(新), `snapshot_types.h`, `snapshot_builder.cpp`, `snapshot_bindings.cpp`

---

### P0-2: 技能系统框架

**新增组件：**
```cpp
// components.h 新增
enum class SkillTargetType : uint8_t {
    Skillshot = 0,
    Targeted  = 1,
    AoEGround = 2,
    SelfBuff  = 3,
    Dash      = 4,
};

struct SkillDef {
    int Id = 0;
    SkillTargetType TargetType = SkillTargetType::Skillshot;
    float Range = 10.0f;
    float Cooldown = 1.0f;
    float ManaCost = 20.0f;
    float CastTime = 0.0f;
    float Damage = 0.0f;
    float AoERadius = 0.0f;
    float ProjectileSpeed = 20.0f;
    float EffectValue = 0.0f;
    bool InterruptOnMove = true;
};

struct SkillSlot {
    int SkillId = 0;
    int Level = 0;
    float CooldownTimer = 0.0f;
    float MaxCooldown = 0.0f;
};

struct SkillComponent {
    SkillSlot Slots[4];
};

struct CastState {
    bool IsCasting = false;
    int ActiveSlot = -1;
    float CastTimer = 0.0f;
    Vec2 AimPosition{0.0f};
    int TargetEntityId = 0;
};

struct SkillPoints {
    int Available = 0;
};

// 泛化投射物
enum class ProjectileType : uint8_t {
    BasicAttack = 0,
    Skillshot   = 1,
    Missile     = 2,
};

struct ProjectileTag {
    int OwnerId = 0;
    int SkillId = 0;
    float Damage = 0.0f;
    ProjectileType Type = ProjectileType::BasicAttack;
    float Radius = 0.3f;
    float Speed = 20.0f;
    float Lifetime = 2.0f;
    entt::entity HomingTarget = entt::null;
};

struct AoETag {
    int OwnerId = 0;
    int SkillId = 0;
    float Radius = 3.0f;
    float Duration = 2.0f;
    float Timer = 0.0f;
    float TickRate = 0.5f;
    float TickTimer = 0.0f;
    float Damage = 10.0f;
    bool HasTicked = false;
};
```

**新增 System：**
```cpp
// systems/skill_input.h — 读取技能按键 → 触发施法
inline void skill_input_system(entt::registry &reg, entt::entity local_input_entity);

// systems/skill_cast.h — 管理前摇 → 释放 → 冷却
inline void skill_cast_system(entt::registry &reg, double now, CommandBuffer &cb, IdState &ids);

// systems/skill_effect.h — 执行技能效果
inline void skill_effect_system(entt::registry &reg, float dt, CommandBuffer &cb);

// systems/skill_cooldown.h — 冷却递减
inline void skill_cooldown_system(entt::registry &reg, float dt);

// systems/skill_level.h — 技能点分配
inline void skill_level_system(entt::registry &reg);

// systems/projectile_movement.h — 泛化投射物运动（替换 arrow_movement）
inline void projectile_movement_system(entt::registry &reg, float dt);

// systems/aoe_tick.h — AoE 区域伤害 tick
inline void aoe_tick_system(entt::registry &reg, float dt, CommandBuffer &cb);
```

**输入扩展：**
```cpp
// LocalInputSingleton 扩展
struct LocalInputSingleton {
    Vec2 Move{0.0f};
    Vec2 Aim{0.0f};
    bool Fire = false;
    int Seq = 0;
    bool SkillQ = false;   // 新增
    bool SkillW = false;   // 新增
    bool SkillE = false;   // 新增
    bool SkillR = false;   // 新增
};

// PlayerInputState 同步扩展
struct PlayerInputState {
    Vec2 Move{0.0f};
    Vec2 Aim{0.0f};
    bool Fire = false;
    int Seq = 0;
    bool SkillQ = false;   // 新增
    bool SkillW = false;   // 新增
    bool SkillE = false;   // 新增
    bool SkillR = false;   // 新增
};
```

**SimServer API 扩展：**
```cpp
// sim_server.h 新增
void set_local_input(const godot::Vector2 &move, const godot::Vector2 &aim,
                     bool fire, int seq,
                     bool skill_q, bool skill_w, bool skill_e, bool skill_r);  // 新增重载
void set_skill_levelup(int slot_index);  // 技能加点
```

**Snapshot 扩展：**
```cpp
class SimSkillSlotSnap : public godot::RefCounted {
    GDCLASS(SimSkillSlotSnap, godot::RefCounted)
public:
    int skill_id = 0;
    int level = 0;
    float cooldown = 0.0f;
    float max_cooldown = 0.0f;
    float mana_cost = 0.0f;
    // ... getter/setter + _bind_methods
};

// SimPlayerSnap 新增
TypedArray<SimSkillSlotSnap> skills;
int skill_points = 0;

// SimBotSnap 新增
TypedArray<SimSkillSlotSnap> skills;

// SimSnapshot 新增
TypedArray<SimAoESnap> aoes;  // AoE 区域快照

class SimAoESnap : public godot::RefCounted {
    GDCLASS(SimAoESnap, godot::RefCounted)
public:
    int id = 0;
    float x = 0, y = 0;
    float radius = 0;
    int skill_id = 0;
    float duration = 0;
    float timer = 0;
    int owner_id = 0;
    // ... getter/setter + _bind_methods
};
```

**World tick 新顺序：**
```
1.  local_input_injection_system
2.  player_movement_system
3.  skill_input_system          ← 新增
4.  skill_cast_system           ← 新增
5.  player_fire_system          (改为普攻技能触发，或保持为独立 System)
6.  bot_targeting_system
7.  bot_ai_system
8.  bot_combat_system
9.  projectile_movement_system  ← 替换 arrow_movement_system
10. aoe_tick_system             ← 新增
11. wall_collision_system
12. combat_system               (扩展 AoE + ProjectileTag 碰撞)
13. pickup_system
14. mana_regen_system           ← 新增
15. skill_cooldown_system       ← 新增
16. progression_system
17. skill_level_system          ← 新增
18. snapshot_export_system
```

**重构 `player_fire.h`：**
- 普攻 = SkillId=0 的 skillshot，仍通过 `try_fire` 创建
- 或将 `ArrowTag` 全面替换为 `ProjectileTag`，`player_fire` 改为构造 `ProjectileTag`

**重构 `arrow_movement.h` → `projectile_movement.h`：**
- 支持 `ProjectileTag` + 可选 `HomingTarget` 追踪逻辑
- 保留 `ArrowTag` 兼容期，或一步到位替换

**重构 `combat.h`：**
- 扩展碰撞检测：`ProjectileTag` → `Damageable`（与现有 Arrow 逻辑相同）
- 新增 `AoETag` → `Damageable` 碰撞（circle-circle）

**涉及文件：** `components.h`, `game_config.h`, `world.h`, `world.cpp`, `arrow_spawner.h`(重构), `systems/player_fire.h`(重构), `systems/arrow_movement.h`(重构→projectile_movement.h), `systems/combat.h`(扩展), `systems/bot_combat.h`(适配), `systems/skill_input.h`(新), `systems/skill_cast.h`(新), `systems/skill_effect.h`(新), `systems/skill_cooldown.h`(新), `systems/skill_level.h`(新), `systems/projectile_movement.h`(新), `systems/aoe_tick.h`(新), `snapshot_types.h`, `snapshot_builder.h`, `snapshot_builder.cpp`, `snapshot_bindings.cpp`, `sim_server.h`, `sim_server.cpp`, `register_types.cpp`

---

### P0-3: 施法指示器

**纯 View 层，无 Sim 改动。**

**新增 GDScript 文件：**
```
scripts/view/indicator_manager.gd     — 指示器生命周期管理
scripts/view/range_indicator.gd       — 范围圈（3D MeshInstance3D）
scripts/view/aoe_preview_indicator.gd — AoE 预览（3D 半透明圆）
scripts/view/direction_indicator.gd   — 方向线（3D 线段）
scripts/ui/cast_bar_ui.gd             — 施法进度条（2D CanvasLayer）
```

**依赖：** P0-2 的 `SimSkillSlotSnap` 数据（技能 Range / CastTime / TargetType）

---

### P0-4: 技能栏 HUD

**纯 View 层。**

**新增 GDScript 场景：**
```
scripts/ui/skill_bar_hud.gd
scripts/ui/skill_slot_ui.gd
scenes/ui/skill_bar_hud.tscn
```

**依赖：** P0-2 的 `SimPlayerSnap.skills` + `SimPlayerSnap.skill_points`

---

### P0-5: 玩家死亡 + 淘汰

**Sim 层改动：**
- `combat.h` 中玩家 `hp.Cur <= 0` 时，当前仅设置 `Dead.enabled=true`
- 需新增：BR 模式下玩家死亡 = 淘汰（不再重生）
- 新增 `Eliminated` 组件或标记
- 新增 `SurvivingCount` 单例
- 新增 `GameOverSystem` 检测存活人数

**Snapshot 扩展：**
- `SimPlayerSnap` + `eliminated: bool`
- `SimSnapshot` + `alive_count: int`

---

### P0-6: 缩圈系统

**新增组件：**
```cpp
struct SafeZone {
    Vec2 Center{0.0f};
    float CurrentRadius = 50.0f;
    float TargetRadius = 10.0f;
    float ShrinkSpeed = 2.0f;
    float DamagePerTick = 2.0f;
    float WaitTimer = 60.0f;
    float ShrinkTimer = 0.0f;
    int Phase = 0;
    bool IsShrinking = false;
};
```

**新增 System：**
```cpp
// systems/safe_zone.h
inline void safe_zone_system(entt::registry &reg, float dt, CommandBuffer &cb);
```

**Snapshot 扩展：**
```cpp
class SimZoneSnap : public godot::RefCounted {
    GDCLASS(SimZoneSnap, godot::RefCounted)
public:
    float center_x, center_y;
    float current_radius, target_radius;
    float next_radius;
    int phase;
    bool is_shrinking;
    float damage_per_tick;
};
// SimSnapshot 新增
SimZoneSnap zone;  // 或 Ref<SimZoneSnap>
```

**View 层：** 安全区圆柱墙渲染 + 小地图圈 + HUD 距离提示

---

### P1-7~11: 后续模块

| # | 模块 | 新增组件 | 新增 System | Snapshot 扩展 |
|---|------|---------|------------|--------------|
| 7 | 多技能类型 | _(P0-2 已定义)_ | skill_effect 扩展 | SimProjectileSnap 替代 SimArrowSnap |
| 8 | 装备系统 | `Inventory`, `ItemDef`, `EquipmentBonuses`, `ItemTag` | `item_spawn_system`, `item_pickup_system`, `item_passive_system` | `SimItemSnap`, `SimPlayerSnap.items` |
| 9 | 小地图 | _(无)_ | _(无，纯 View)_ | _(无)_ |
| 10 | 战争迷雾 | `Vision`, `FogReveal`, `StealthTag` | `vision_system` | `SimSnapshot.visible_*` 过滤 |
| 11 | 战斗反馈 | _(无)_ | _(无)_ | `SimEventSnap` 扩展 type/value/skill_id/x/y |

---

### System 模板（新增 System 的代码结构规范）

所有新 System 遵循现有 header-only inline 模式：

```cpp
#pragma once

#include <entt/entt.hpp>
#include "../components.h"
#include "../game_config.h"
#include "../vec2.h"
#include "../command_buffer.h"

namespace sim {

inline void xxx_system(entt::registry &reg, float dt, CommandBuffer &cb) {
    auto view = reg.view<SomeTag, SomeComponent>(entt::exclude<Dead>);
    for (auto e : view) {
        auto &comp = view.get<SomeComponent>(e);
        // ... logic ...
        // 延迟实体操作通过 cb.push()
    }
}

} // namespace sim
```

**约定：**
1. 所有 System 是 `inline void` 自由函数，放在 `sim` namespace
2. 参数顺序：`registry &reg` 必在最前，然后 `float dt`，然后其他引用
3. 实体创建/销毁必须通过 `CommandBuffer`，绝不在 System 内直接操作 registry
4. 读组件用 `view.get<T>(e)`，写组件直接赋值（ECS 无需标记脏）
5. 不使用 `reg.view` 的 `each()` 方法（避免隐藏的性能问题），用 `for (auto e : view)` + `view.get`
6. 跳过死亡实体：`if (reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled) continue;`
7. System 间通过组件通信，不通过全局变量或函数调用
8. 新增组件必须同时更新 `_spawn_player` / `_spawn_bot` / `SnapshotBuilder` / `snapshot_bindings`

**新增 Snapshot 类型的完整步骤：**
1. `snapshot_types.h` 中新增 `GDCLASS` 类（继承 `RefCounted`）
2. 添加字段 + getter/setter + `static void _bind_methods();` 声明
3. `snapshot_bindings.cpp` 中实现 `_bind_methods()`（BIND + PROP 宏）
4. `snapshot_builder.cpp` 中在对应 `_build_xxx` 方法中填充数据
5. `SimSnapshot` 中添加 `TypedArray<NewSnap>` 字段
6. `register_types.cpp` 中 `ClassDB::register_class<NewSnap>()`
