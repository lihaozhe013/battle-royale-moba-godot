# 指向性普攻系统设计方案

> 创建：2026-07-10
> 关联：`skill_system_design.md`、`right_click_movement_design.md`、`sim_system_reference.md`
> 范围：玩家普攻改为指向性（锁定目标 + 追踪箭矢 + 自动追击穿墙）；Bot 普攻不变
> 状态：设计方案（待实施）

---

## 目录

1. [设计决策摘要](#1-设计决策摘要)
2. [架构总览](#2-架构总览)
3. [新增 / 修改组件](#3-新增--修改组件)
4. [新增 / 修改 System](#4-新增--修改-system)
5. [tick 顺序](#5-tick-顺序)
6. [新增常量](#6-新增常量)
7. [SimServer API 扩展](#7-simserver-api-扩展)
8. [Snapshot 扩展](#8-snapshot-扩展)
9. [GDScript 改动](#9-gdscript-改动)
10. [文件改动清单](#10-文件改动清单)
11. [边界情况处理](#11-边界情况处理)
12. [实施顺序](#12-实施顺序)
13. [难度评估](#13-难度评估)

---

## 1. 设计决策摘要（已与用户确认）

| 项 | 决策 |
|----|------|
| WASD 模式攻击键 | **Q**（不冲突 WASD 移动 / CERF 技能） |
| MOBA 模式攻击键 | **A**（不冲突 QWER 技能） |
| 右键点敌方单位 | **两模式都直接攻击**（非施法时） |
| 箭矢飞行方式 | **追踪必中**（homing），每 tick 修正方向朝目标当前位置 |
| 箭矢与墙 | **穿墙**（`wall_collision` 跳过 Homing 箭矢） |
| 自动追击移动 | **直线朝目标方向移动，不走 A*，不绕墙** |
| 追击中与墙 | **穿墙**（`wall_collision` 跳过追击中的玩家，与 Dashing 穿墙逻辑一致） |
| 目标锁定 | **不丢失**（追击穿墙不会因路径失败 / 墙阻挡而丢失目标） |
| 普攻射程 | `PlayerAttackRange = 8.0` |
| Bot 普攻 | **不变**（当前非指向性弹道，朝目标位置直射） |
| 左键直接射箭 | **废弃**（`fire` 不再触发朝鼠标射箭，普攻改为锁定制） |

### 核心原则

**"四不"保证**（用户明确要求）：
1. **不用 A* 绕路** — 追击移动直接朝目标方向直线移动，不调用 NavGrid 寻路
2. **不被墙壁退回** — 追击中 `wall_collision` 跳过玩家实体（`AttackTarget.Chasing == true` 时），与 Dashing 穿墙机制一致（`wall_collision.h:30-32`）
3. **不会丢失目标** — 目标锁仅因目标死亡 / 玩家下达新指令（移动 / 停止）而清除，墙阻挡不会清锁
4. **100% 必中** — Homing 箭矢每 tick 修正速度朝目标当前位置，穿墙飞行，`combat` 中只检测锁定目标

---

## 2. 架构总览

```
input_collector.gd
  ├─ Q/WASD 或 A/MOBA 边沿 → attack_command_mode = true
  ├─ 左键 + attack_command_mode → 选目标(hover) 或 点地板找最近
  ├─ 右键 + hover敌人 → 直接攻击（两模式）
  ├─ 右键 + 空地(MOBA) → 移动指令 + 清锁
  └─ 移动/停止 → attack_clear
        ↓ set_attack_command(...)
LocalInputSingleton（扩字段）
        ↓ local_input_injection
PlayerInputState（扩字段）
        ↓
player_attack_command_system（新，tick #2）
  ├─ 处理 attack 命令 → 设/清 AttackTarget 组件
  ├─ pending 机制（施法中暂存，CastState=None 时消费）
  └─ 验证目标 alive → 无效则清锁
        ↓
player_pathfinding_system（不变，仅右键 A* 移动）
        ↓
player_movement_system（改）
  ├─ 新增追击分支：AttackTarget 有效 && 超射程 && 无WASD → 直线移向目标
  ├─ 追击时设 AttackTarget.Chasing = true（供 wall_collision 跳过）
  └─ 优先级：Stop > Cast/Status gate > WASD > 追击 > MovePath
        ↓
player_attack_fire_system（新，替代 player_fire_system，tick #5）
  ├─ AttackTarget 有效 && 在射程内 → 朝目标发射 homing 箭矢
  └─ Stun/CastState gate
        ↓
arrow_movement_system（改）
  ├─ 有 Homing 组件 → 每 tick 修正速度朝目标当前位置
  └─ 无 Homing → 直线飞行（不变）
        ↓
wall_collision_system（改）
  ├─ Mover 分支：跳过 AttackTarget.Chasing == true 的玩家（穿墙追击）
  └─ Arrow 分支：跳过有 Homing 的箭矢（穿墙飞行）
        ↓
combat_system（改）
  └─ 有 Homing 的箭矢只检测锁定目标的碰撞（不误伤路过的敌人）
        ↓
SimSnapshot（扩 attack_target_id）
        ↓
GDScript View（攻击目标红色指示器）
```

---

## 3. 新增 / 修改组件（`components.h`）

### 3.1 新增组件

```cpp
// 玩家普攻锁定目标（挂 Player 实体）
struct AttackTarget {
    entt::entity Target = entt::null;
    int TargetNetworkId = -1;

    // Pending 机制：施法中收到 attack 命令时暂存，CastState=None 时消费
    int  PendingTargetId = -1;       // 直接目标 NetworkId
    bool PendingGround    = false;   // A/Q 点地板标志
    Vec2 PendingGroundPos{0.0f};     // 地板坐标

    // 追击标志：由 player_movement_system 每 tick 设置，
    // wall_collision_system 读取以跳过追击中玩家（穿墙）
    bool Chasing = false;
};

// 追踪箭矢标记（挂 Arrow 实体，仅玩家普攻箭矢有）
struct Homing {
    entt::entity Target = entt::null;
    int TargetNetId = -1;
};
```

### 3.2 扩展 `LocalInputSingleton` / `PlayerInputState`

追加到现有 struct 末尾：

```cpp
int  AttackTargetId  = -1;        // hover 选中的 NetworkId，-1=无
bool AttackGround    = false;     // A/Q 点地板 → 找最近敌人
Vec2 AttackGroundPos{0.0f};       // 地板坐标
bool AttackClear     = false;     // 移动/停止 → 清锁
```

### 3.3 组件挂载清单

| 实体 | 新增组件 | 时机 |
|------|---------|------|
| Player | `AttackTarget` | `_spawn_player` |
| Arrow（玩家普攻） | `Homing` | `try_fire` 中按 `ArrowSpawnContext.homing_target` 决定 |

---

## 4. 新增 / 修改 System

### 4.1 新增 `player_attack_command_system`（tick #2）

**文件**：`systems/player_attack_command.h`

**签名**：
```cpp
inline void player_attack_command_system(entt::registry &reg, float dt);
```

**逻辑**：

```
遍历 PlayerTag(IsLocal) + PlayerInputState + Position2D + AttackTarget + CastState

1. Stun gate：眩晕中跳过（不清目标，不清 pending）

2. 存储 pending 命令（无论 CastState 如何，保证不丢命令）：
   - input.AttackTargetId >= 0
     → at.PendingTargetId = input.AttackTargetId
     → at.PendingGround = false
   - input.AttackGround
     → at.PendingGround = true
     → at.PendingGroundPos = input.AttackGroundPos

3. 清锁信号：
   - input.AttackClear || input.Stop || input.MoveIssue
     → at.Target = null
     → at.TargetNetworkId = -1
     → at.PendingTargetId = -1
     → at.PendingGround = false

4. CastState != None → 跳过后续（pending 已存储，等 CastState=None 时消费）

5. 消费 pending（CastState == None 时）：
   a. PendingTargetId >= 0
      → resolve_target(NetworkId) → 若 alive → 设 Target / TargetNetworkId
      → 清 PendingTargetId
   b. PendingGround
      → find_nearest_enemy(PendingGroundPos, AcquisitionRange, exclude=self)
      → 若找到 → 设 Target / TargetNetworkId
      → 清 PendingGround

6. 验证当前 Target：
   → !reg.valid(Target) || !alive → 清 Target / TargetNetworkId
```

**辅助函数**：

```cpp
// 在半径内找最近 alive Damageable（排除 owner）
inline entt::entity find_nearest_enemy(
    entt::registry &reg, Vec2 pos, float max_radius, entt::entity exclude
) {
    entt::entity best = entt::null;
    float best_sq = max_radius * max_radius;
    auto view = reg.view<Damageable, Position2D>();
    for (auto e : view) {
        if (e == exclude) continue;
        if (reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled) continue;
        Vec2 delta = view.get<Position2D>(e).Value - pos;
        float dist_sq = vec2_length_sq(delta);
        if (dist_sq < best_sq) {
            best_sq = dist_sq;
            best = e;
        }
    }
    return best;
}

// 通过 NetworkId 解析 entity
inline entt::entity resolve_target_by_netid(
    entt::registry &reg, int net_id
) {
    if (net_id < 0) return entt::null;
    auto view = reg.view<NetworkId, Damageable>();
    for (auto e : view) {
        if (view.get<NetworkId>(e).Value == net_id) return e;
    }
    return entt::null;
}
```

### 4.2 新增 `player_attack_fire_system`（tick #5，替代 `player_fire_system`）

**文件**：`systems/player_attack_fire.h`

**签名**：
```cpp
inline void player_attack_fire_system(
    entt::registry &reg, double now, CommandBuffer &cb, IdState &ids
);
```

**逻辑**：

```
遍历 PlayerTag(IsLocal) + Position2D + CombatStats + NetworkId + AttackTarget

1. Stun gate → skip
2. CastState != None → skip（施法中禁普攻）
3. AttackTarget.Target == null → skip（无锁定目标）
4. 计算 dist = |target_pos - pos|
5. dist > PlayerAttackRange → skip（超射程，由追击分支处理移动）
6. dist <= PlayerAttackRange：
   - aim_angle = atan2(target_pos - pos)
   - 构造 ArrowSpawnContext：
       spawn_pos    = pos.Value
       angle        = aim_angle
       owner_id     = net.Value
       owner_entity = e
       dmg          = stats.Atk
       homing_target         = at.Target        ← 关键：设 homing
       homing_target_net_id  = at.TargetNetworkId
   - 调用 try_fire(stats, ctx)
```

**注意**：不再读 `input.Fire` / `input.Aim`。`fire` 字段废弃。

### 4.3 删除 `player_fire_system`

完全被 `player_attack_fire_system` 取代。

- 删除 `systems/player_fire.h`
- 从 `world.h` 移除 `#include "systems/player_fire.h"`
- 从 `world.cpp` tick 顺序移除 `player_fire_system(...)` 调用

### 4.4 修改 `player_movement_system`（新增追击分支 + Chasing 标志）

**关键改动**：在 WASD 分支之后、MovePath 跟随分支之前，插入追击分支。追击时设 `AttackTarget.Chasing = true`，供 `wall_collision_system` 跳过。

```cpp
inline void player_movement_system(entt::registry &reg, float dt, float map_half) {
    auto view = reg.view<
        PlayerTag, Position2D, FacingAngle, PlayerInputState, MoveSpeed>();
    for (auto e : view) {
        auto &tag = view.get<PlayerTag>(e);
        if (!tag.IsLocal) continue;
        auto &pos = view.get<Position2D>(e);
        auto &angle = view.get<FacingAngle>(e);
        auto &input = view.get<PlayerInputState>(e);
        auto &speed = view.get<MoveSpeed>(e);

        // ── 每帧重置 Chasing 标志 ──
        if (reg.all_of<AttackTarget>(e)) {
            reg.get<AttackTarget>(e).Chasing = false;
        }

        // Status gate (Root/Stun) — 不移动
        if (reg.all_of<StatusEffect>(e)) {
            auto &st = reg.get<StatusEffect>(e);
            if (st.Timer > 0.0f &&
                (st.Type == StatusType::Root || st.Type == StatusType::Stun))
                continue;
        }

        // Cast state gate (Aiming allows movement)
        bool cast_block = false;
        if (reg.all_of<CastState>(e)) {
            auto &cs = reg.get<CastState>(e);
            if (cs.State == CastState::Phase::Casting ||
                cs.State == CastState::Phase::Channeling ||
                cs.State == CastState::Phase::Dashing)
                cast_block = true;
        }

        // Stop command
        if (input.Stop) {
            if (reg.all_of<MovePath>(e))
                reg.get<MovePath>(e).Following = false;
            continue;
        }

        // WASD movement (highest priority)
        if (glm::length(input.Move) > 0.01f) {
            Vec2 dir = vec2_normalize(input.Move);
            angle.Radians = std::atan2(dir.y, dir.x);
            pos.Value = vec2_clamp_to_map(pos.Value + dir * speed.Value * dt, map_half);
            if (reg.all_of<MovePath>(e))
                reg.get<MovePath>(e).Following = false;
            continue;
        }

        // ── 追击分支（新增）──
        // 条件：无 WASD + CastState 允许移动 + 有 AttackTarget + 超射程
        if (!cast_block && reg.all_of<AttackTarget>(e)) {
            auto &at = reg.get<AttackTarget>(e);
            if (at.Target != entt::null && reg.valid(at.Target) &&
                !(reg.all_of<Dead>(at.Target) && reg.get<Dead>(at.Target).enabled)) {

                auto &target_pos = reg.get<Position2D>(at.Target).Value;
                Vec2 delta = target_pos - pos.Value;
                float dist = glm::length(delta);

                if (dist > GameConfig::PlayerAttackRange) {
                    // 直线追击（不走 A*，不绕墙）
                    Vec2 dir = delta / dist;

                    // 朝向用 turn rate 平滑（与 MovePath 分支一致）
                    float target_ang = std::atan2(dir.y, dir.x);
                    float diff = pm_angle_diff(target_ang, angle.Radians);
                    float max_turn = GameConfig::PathTurnRate * dt;
                    if (std::abs(diff) > max_turn)
                        diff = (diff > 0 ? max_turn : -max_turn);
                    angle.Radians += diff;

                    // 移动（clamp_to_map 防止飞出地图边界，但不检查墙）
                    Vec2 step = dir * speed.Value * dt;
                    pos.Value = vec2_clamp_to_map(pos.Value + step, map_half);

                    // 设 Chasing 标志 → wall_collision 跳过此实体（穿墙）
                    at.Chasing = true;

                    // 清除 MovePath（追击优先于旧路径）
                    if (reg.all_of<MovePath>(e))
                        reg.get<MovePath>(e).Following = false;

                    continue;
                }
            }
        }

        // Path-following movement (existing, unchanged)
        // ...
    }
}
```

**优先级总览**（修改后）：

```
1. 重置 Chasing = false
2. Status gate (Root/Stun) → continue
3. Stop → clear path, continue
4. WASD (input.Move > 0.01) → move, clear path, continue
5. 追击 (AttackTarget valid && dist > range && !cast_block)
   → 直线移动, Chasing=true, clear path, continue
6. MovePath following → existing logic
```

**WASD 模式 kiting**：玩家按 WASD 时走分支 4（手动移动），AttackTarget 保持锁定（不清除）；松开 WASD 后走分支 5（自动追击）；在射程内由 `player_attack_fire` 射箭。

### 4.5 修改 `arrow_movement_system`（Homing 追踪）

```cpp
inline void arrow_movement_system(entt::registry &reg, float dt) {
    auto view = reg.view<ArrowTag, Position2D, Velocity2D, Lifetime>();
    for (auto e : view) {
        auto &pos  = view.get<Position2D>(e);
        auto &vel  = view.get<Velocity2D>(e);
        auto &life = view.get<Lifetime>(e);

        // Homing: 每 tick 修正速度朝目标当前位置
        if (reg.all_of<Homing>(e)) {
            auto &hom = reg.get<Homing>(e);
            if (reg.valid(hom.Target) &&
                !(reg.all_of<Dead>(hom.Target) && reg.get<Dead>(hom.Target).enabled)) {

                Vec2 target_pos = reg.get<Position2D>(hom.Target).Value;
                Vec2 to_target = target_pos - pos.Value;
                float dist = glm::length(to_target);
                if (dist > 0.001f) {
                    Vec2 dir = to_target / dist;
                    vel.Value = dir * GameConfig::ArrowSpeed;
                    if (reg.all_of<FacingAngle>(e))
                        reg.get<FacingAngle>(e).Radians = std::atan2(dir.y, dir.x);
                }
            }
            // 目标死亡 → 保持当前速度直线飞 → Lifetime 过期销毁
        }

        pos.Value = pos.Value + vel.Value * dt;
        life.Remaining -= dt;
    }
}
```

### 4.6 修改 `wall_collision_system`（追击穿墙 + Homing 穿墙）

**两处跳过**：

```cpp
inline void wall_collision_system(entt::registry &reg, CommandBuffer &cb) {
    // ... gather walls ...

    // Movers (player/bot): push out of walls
    auto mover_view = reg.view<Damageable, Position2D>();
    for (auto e : mover_view) {
        bool dead = reg.all_of<Dead>(e) && reg.get<Dead>(e).enabled;
        if (dead) continue;

        // Skip wall collision during dash (existing)
        if (reg.all_of<CastState>(e) &&
            reg.get<CastState>(e).State == CastState::Phase::Dashing)
            continue;

        // ── 新增：跳过追击中的玩家（穿墙追击）──
        if (reg.all_of<AttackTarget>(e)) {
            auto &at = reg.get<AttackTarget>(e);
            if (at.Chasing) continue;
        }

        // ... existing push-out logic ...
    }

    // Arrows: destroy if inside any wall
    auto arrow_view = reg.view<ArrowTag, Position2D>();
    for (auto e : arrow_view) {
        // ── 新增：跳过 Homing 箭矢（穿墙飞行）──
        if (reg.all_of<Homing>(e)) continue;

        auto &pos = arrow_view.get<Position2D>(e);
        for (auto &w : walls) {
            if (point_inside_aabb(pos.Value, w.Min, w.Max)) {
                cb.push([e](entt::registry &r) { r.destroy(e); });
                break;
            }
        }
    }
}
```

**设计依据**：与现有 Dashing 穿墙（`wall_collision.h:30-32`）完全一致的机制——追击中的玩家被视为"穿透墙壁"状态，`wall_collision` 不推回。

### 4.7 修改 `combat_system`（Homing 只命中锁定目标）

```cpp
for (auto target : target_view) {
    if (target == arrow_tag.OwnerEntity) continue;
    if (reg.all_of<Dead>(target) && reg.get<Dead>(target).enabled) continue;

    // ── 新增：Homing 箭矢只检测锁定目标 ──
    if (reg.all_of<Homing>(arrow)) {
        auto &hom = reg.get<Homing>(arrow);
        if (target != hom.Target) continue;
    }

    // ... existing collision check / damage / lifesteal / kill event ...
}
```

**效果**：Homing 箭矢穿过非目标敌人时不误伤，只命中锁定的目标。

### 4.8 修改 `arrow_spawner.h`（ArrowSpawnContext 扩展）

```cpp
struct ArrowSpawnContext {
    CommandBuffer &cb;
    IdState &id_state;
    double now;
    Vec2 spawn_pos;
    float angle;
    int owner_id;
    entt::entity owner_entity;
    float dmg;
    float lifesteal_ratio = 0.0f;
    // 新增：
    entt::entity homing_target       = entt::null;
    int            homing_target_net_id = -1;
};
```

`try_fire` 修改：

```cpp
ctx.cb.push([arrow_id, ctx](entt::registry &reg) {
    auto e = reg.create();
    Vec2 vel{
        std::cos(ctx.angle) * GameConfig::ArrowSpeed,
        std::sin(ctx.angle) * GameConfig::ArrowSpeed};
    reg.emplace<Position2D>(e, ctx.spawn_pos);
    reg.emplace<Velocity2D>(e, vel);
    reg.emplace<FacingAngle>(e, ctx.angle);
    reg.emplace<ArrowTag>(
        e, ctx.owner_id, ctx.owner_entity, ctx.dmg, ctx.lifesteal_ratio);
    reg.emplace<Lifetime>(e, GameConfig::ArrowLifetime);
    reg.emplace<NetworkId>(e, arrow_id);

    // 新增：Homing 标记
    if (ctx.homing_target != entt::null) {
        reg.emplace<Homing>(e, ctx.homing_target, ctx.homing_target_net_id);
    }
});
```

### 4.9 修改 `local_input_injection_system`

追加复制 4 个新字段：

```cpp
state.AttackTargetId  = input.AttackTargetId;
state.AttackGround     = input.AttackGround;
state.AttackGroundPos  = input.AttackGroundPos;
state.AttackClear      = input.AttackClear;
```

---

## 5. tick 顺序

```
local_input_injection        #  1. 注入输入（含 attack 字段）
player_attack_command (新)    #  2. 处理 attack 命令 + pending + 验证目标
player_pathfinding           #  3. 右键 A* 移动（不变）
player_movement              #  4. +追击分支（设 Chasing 标志）
player_attack_fire (新)       #  5. 替代 player_fire，发射 homing 箭
skill_cast                   #  6. 施法状态机（不变）
bot_targeting                #  7
bot_ai                       #  8
bot_combat                   #  9. Bot 普攻（不变）
arrow_movement               # 10. +Homing 追踪
wall_collision               # 11. 跳过 Chasing 玩家 + 跳过 Homing 箭矢
combat                       # 12. Homing 只命中锁定目标
pickup                       # 13
aoe                          # 14
status_effect                # 15
mana_regen                   # 16
skill_cooldown               # 17
progression                  # 18
snapshot_export              # 19
```

**顺序要点**：
- `player_attack_command`(#2) 在 `player_pathfinding`(#3) 之前：先处理 attack 命令设 AttackTarget，再由 pathfinding 处理右键移动（若 MoveIssue=true 则 AttackClear 已清锁）
- `player_movement`(#4) 在 `player_attack_fire`(#5) 之前：先追击移动拉近距离，再判断是否在射程内射击
- `wall_collision`(#11) 在 `player_movement`(#4) 之后：读 Chasing 标志跳过追击中玩家
- `combat`(#12) 在 `arrow_movement`(#10) 之后：Homing 箭矢已追踪到目标附近，碰撞检测命中

---

## 6. 新增常量（`game_config.h`）

```cpp
// ── 指向性普攻 ──
static constexpr float PlayerAttackRange = 8.0f;
static constexpr float PlayerAttackRangeSq = PlayerAttackRange * PlayerAttackRange;
static constexpr float AttackAcquisitionRange = 15.0f;  // A/Q 点地板搜索半径
```

（`ArrowSpeed = 20.0` / `ArrowLifetime = 2.0` 复用现有值。射程 8 + 速度 20 → 最远飞行 0.4s 到达，远小于 Lifetime 2.0s，保证必中。）

---

## 7. SimServer API 扩展

### 7.1 新增方法

```cpp
// sim_server.h
void set_attack_command(int target_id, bool attack_ground,
                        float ground_x, float ground_y, bool attack_clear);
```

### 7.2 World 实现

```cpp
// world.h — 声明
void set_attack_command(int target_id, bool attack_ground,
                        float ground_x, float ground_y, bool attack_clear);

// world.cpp — 实现
void World::set_attack_command(
    int target_id, bool attack_ground,
    float ground_x, float ground_y, bool attack_clear
) {
    if (_local_input_entity != entt::null) {
        auto &li = _reg.get<LocalInputSingleton>(_local_input_entity);
        li.AttackTargetId  = target_id;
        li.AttackGround     = attack_ground;
        li.AttackGroundPos  = Vec2{ground_x, ground_y};
        li.AttackClear      = attack_clear;
    }
}
```

### 7.3 绑定（`register_types.cpp` / `sim_server.cpp`）

```cpp
ClassDB::bind_method(
    D_METHOD("set_attack_command", "target_id", "attack_ground",
             "ground_x", "ground_y", "attack_clear"),
    &SimServer::set_attack_command
);
```

### 7.4 `LocalInputSingleton` 构造更新

`world.cpp` 中 `_local_input_entity` 的 `emplace<LocalInputSingleton>` 需追加 4 个默认参数：

```cpp
_reg.emplace<LocalInputSingleton>(
    _local_input_entity,
    Vec2{0.0f},    // Move
    Vec2{0.0f},    // Aim
    false,         // Fire
    0,             // Seq
    -1,            // CastSlot
    false,         // CastConfirm
    false,         // CastCancel
    false,         // CastInterrupt
    Vec2{0.0f},    // CastAim
    -1,            // CastTargetId
    Vec2{0.0f},    // MoveTarget
    false,         // MoveIssue
    false,         // Stop
    // ── 新增 ──
    -1,            // AttackTargetId
    false,         // AttackGround
    Vec2{0.0f},    // AttackGroundPos
    false          // AttackClear
);
```

`_spawn_player` 中 `emplace<PlayerInputState>` 同理追加 4 个默认参数。

---

## 8. Snapshot 扩展

### 8.1 `SimPlayerSnap` 新增字段

```cpp
// snapshot_types.h — SimPlayerSnap
int attack_target_id = -1;  // 锁定目标的 NetworkId，-1=无

int get_attack_target_id() const { return attack_target_id; }
void set_attack_target_id(int v) { attack_target_id = v; }
```

### 8.2 `snapshot_bindings.cpp`

```cpp
// SimPlayerSnap::_bind_methods
BIND(SimPlayerSnap, attack_target_id);
PROP(SimPlayerSnap, Variant::INT, attack_target_id);
```

### 8.3 `snapshot_builder.cpp` — `_build_players`

```cpp
s->attack_target_id = -1;
if (reg.all_of<AttackTarget>(e)) {
    s->attack_target_id = reg.get<AttackTarget>(e).TargetNetworkId;
}
```

### 8.4 `_spawn_player` 修改

```cpp
_reg.emplace<AttackTarget>(e);  // 默认 Target=null, TargetNetworkId=-1, Chasing=false
```

---

## 9. GDScript 改动

### 9.1 `input_collector.gd`（核心改动）

**新增字段**：

```gdscript
# 攻击命令
var attack_command_mode := false   # Q/A 按下后等待左键选目标
var attack_target_id := -1         # 脉冲：右键点敌 / A+左键确认时设
var attack_ground := false         # 脉冲：A/Q 点地板时设
var attack_ground_pos := Vector2.ZERO
var attack_clear := false          # 脉冲：移动/停止时设

var _attack_key := KEY_Q
var _prev_attack := false
const ATTACK_KEY_WASD := KEY_Q
const ATTACK_KEY_MOBA := KEY_A
```

**模式切换**：

```gdscript
func _on_mode_changed(m: int) -> void:
    if m == GameSettings.MoveMode.MOBA:
        _skill_keys = SKILL_KEYS_MOBA
        _attack_key = ATTACK_KEY_MOBA
    else:
        _skill_keys = SKILL_KEYS_WASD
        _attack_key = ATTACK_KEY_WASD
```

**`_read_skill_input()` 重构**：

```gdscript
func _read_skill_input() -> void:
    # 每帧清脉冲
    cast_confirm = false
    cast_interrupt = false
    attack_target_id = -1
    attack_ground = false
    attack_clear = false

    # 1. 技能键 held → cast_slot（现有逻辑不变）
    var any_held := -1
    for i in 4:
        var pressed = Input.is_key_pressed(_skill_keys[i])
        if pressed:
            any_held = i
        _prev_skill[i] = pressed
    cast_slot = any_held
    cast_target_id = hovered_entity_id if any_held >= 0 else -1
    if cast_slot >= 0:
        cast_aim = aim_world
        attack_command_mode = false  # 技能键优先，取消 attack 模式

    # 2. 攻击键边沿 → attack_command_mode
    var attack_now := Input.is_key_pressed(_attack_key)
    if attack_now and not _prev_attack:
        attack_command_mode = true
    _prev_attack = attack_now

    # 3. 取消 attack_command_mode（右键 / S / ESC / H）
    if attack_command_mode:
        if Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT) \
           or Input.is_key_pressed(KEY_ESCAPE) \
           or Input.is_key_pressed(KEY_S) \
           or Input.is_key_pressed(KEY_H):
            attack_command_mode = false

    # 4. 左键 = 确认（attack / skill / 无）
    var left_now := Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT)
    if attack_command_mode and left_now:
        # Attack confirm
        if hovered_entity_id >= 0:
            attack_target_id = hovered_entity_id
        else:
            attack_ground = true
            attack_ground_pos = aim_world
        attack_command_mode = false
        cast_confirm = false
        fire = false
    elif cast_slot >= 0 and left_now:
        # Skill confirm（现有逻辑）
        cast_confirm = true
        fire = false
    else:
        # 普通左键 → 无操作（不再朝鼠标射箭）
        cast_confirm = false
        fire = false

    # 5. 右键处理（两模式通用）
    var right_now := Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT)
    var right_edge := right_now and not _prev_right

    if right_edge:
        cast_cancel = true  # 始终设（Sim 仅施法中消费）

    if cast_slot < 0:  # 非施法时才处理攻击/移动
        if hovered_entity_id >= 0 and right_edge:
            # 右键点敌人 → 攻击（两模式）
            attack_target_id = hovered_entity_id
        elif hovered_entity_id < 0 and GameSettings.move_mode == GameSettings.MoveMode.MOBA:
            # MOBA 右键空地 → 移动（边沿 + 长按重复，现有逻辑）
            var now := Time.get_ticks_msec() / 1000.0
            var should_issue := false
            if right_edge:
                should_issue = now - _last_move_time >= MIN_MOVE_INTERVAL
            elif right_now:
                should_issue = now - _last_move_time >= MOVE_REPEAT_INTERVAL
            if should_issue:
                _last_move_time = now
                move_cmd_target = aim_world
                move_cmd_issue = true
                move_issued.emit(move_cmd_target)
                attack_clear = true
    _prev_right = right_now

    # 6. 打断键（现有逻辑不变）
    if GameSettings.move_mode == GameSettings.MoveMode.MOBA:
        cast_interrupt = Input.is_key_pressed(KEY_H) or Input.is_key_pressed(KEY_S)
    else:
        cast_interrupt = Input.is_key_pressed(KEY_H)

    # 7. S 键 stop 脉冲（MOBA 模式）+ 清锁
    if GameSettings.move_mode == GameSettings.MoveMode.MOBA:
        var s_now := Input.is_key_pressed(KEY_S)
        if s_now and not _prev_s:
            stop = true
            attack_clear = true  # ← 新增：S 清锁
        _prev_s = s_now
```

### 9.2 `sim_bridge.gd`

**`_physics_process` 新增**：

```gdscript
sim.set_attack_command(
    input_collector.attack_target_id,
    input_collector.attack_ground,
    input_collector.attack_ground_pos.x,
    input_collector.attack_ground_pos.y,
    input_collector.attack_clear and first_tick
)
```

**帧末清理脉冲**（在现有清理处追加）：

```gdscript
input_collector.attack_target_id = -1
input_collector.attack_ground = false
input_collector.attack_clear = false
```

**`_process` 新增攻击目标指示器**（在 `sync_entities` 之后）：

```gdscript
if last_snapshot.players.size() > 0:
    var p = last_snapshot.players[0] as SimPlayerSnap
    if p:
        entity_manager.set_attack_target_id(p.attack_target_id)
```

### 9.3 `entity_manager.gd`（攻击目标指示器）

```gdscript
var _attack_target_id := -1

func set_attack_target_id(id: int) -> void:
    if _attack_target_id == id:
        return
    if _attack_target_id >= 0 and _entities.has(_attack_target_id):
        _entities[_attack_target_id].set_attack_targeted(false)
    _attack_target_id = id
    if id >= 0 and _entities.has(id):
        _entities[id].set_attack_targeted(true)
```

### 9.4 `entity_view.gd`（红色指示器）

**`_ready` 新增**：

```gdscript
_attack_target_mat = StandardMaterial3D.new()
_attack_target_mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
_attack_target_mat.albedo_color = Color(1.0, 0.2, 0.2, 0.4)  # 红色半透明
_attack_target_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
```

**新增状态**：

```gdscript
var _attack_targeted := false

func set_attack_targeted(v: bool) -> void:
    _attack_targeted = v
```

**`_process` 材质更新修改**（优先级：受击红闪 > 攻击锁定(红) > 悬停高亮(黄) > 无）：

```gdscript
if _flash_timer > 0.0:
    for m in _child_meshes:
        m.material_override = _red_mat
else:
    var mat = _attack_target_mat if _attack_targeted else (_highlight_mat if _hovered else null)
    for m in _child_meshes:
        m.material_override = mat
```

---

## 10. 文件改动清单

### 10.1 C++ 新增

| 文件 | 内容 |
|------|------|
| `src_cpp/sim/systems/player_attack_command.h` | 攻击命令处理 + pending + 目标验证 + 最近敌搜索 |
| `src_cpp/sim/systems/player_attack_fire.h` | 锁定目标射 homing 箭（替代 player_fire） |

### 10.2 C++ 修改

| 文件 | 改动 |
|------|------|
| `components.h` | +`AttackTarget`(含 `Chasing` 字段) +`Homing` 组件；`LocalInputSingleton`/`PlayerInputState` +4 attack 字段 |
| `game_config.h` | +`PlayerAttackRange`/`Sq` +`AttackAcquisitionRange` |
| `arrow_spawner.h` | `ArrowSpawnContext` +homing 字段；`try_fire` 按 `homing_target` emplace `Homing` |
| `systems/arrow_movement.h` | +Homing 追踪分支（每 tick 修正 vel 朝目标当前位置） |
| `systems/wall_collision.h` | Mover 分支：跳过 `AttackTarget.Chasing==true`（穿墙追击）；Arrow 分支：跳过 `Homing`（穿墙飞行） |
| `systems/combat.h` | `Homing` 箭矢只检测锁定目标碰撞 |
| `systems/player_movement.h` | 每帧重置 `Chasing=false`；+追击分支（直线移动 + 设 `Chasing=true`） |
| `systems/local_input_injection.h` | +复制 4 个 attack 字段 |
| `world.h` | +`#include player_attack_command.h` +`player_attack_fire.h`；-`#include player_fire.h`；+`set_attack_command` 声明 |
| `world.cpp` | tick 顺序：插 `player_attack_command`(#2) + 替 `player_fire`→`player_attack_fire`(#5)；+`set_attack_command` 实现；`_spawn_player` +emplace `AttackTarget`；`LocalInputSingleton`/`PlayerInputState` 构造加 4 个默认参数 |
| `sim_server.h/.cpp` | +`set_attack_command` 方法 + 绑定 |
| `register_types.cpp` | 绑定 `set_attack_command` |
| `snapshot_types.h` | `SimPlayerSnap` +`attack_target_id` + getter/setter |
| `snapshot_bindings.cpp` | BIND/PROP `attack_target_id` |
| `snapshot_builder.cpp` | `_build_players` 填 `attack_target_id` |

### 10.3 C++ 删除

| 文件 | 原因 |
|------|------|
| `src_cpp/sim/systems/player_fire.h` | 被 `player_attack_fire_system` 取代 |

### 10.4 GDScript 修改

| 文件 | 改动 |
|------|------|
| `scripts/input/input_collector.gd` | +attack_command_mode + Q/A 键 + 右键攻击 + 左键重构(fire=false) + attack_clear |
| `scripts/sim_bridge.gd` | +`set_attack_command` 调用 + 帧末清理 attack 脉冲 + `set_attack_target_id` 传 View |
| `scripts/view/entity_manager.gd` | +`set_attack_target_id` + `_attack_target_id` 管理 |
| `scripts/view/entity_view.gd` | +`set_attack_targeted` + `_attack_target_mat` 红色指示器 + 材质优先级 |

---

## 11. 边界情况处理

| 场景 | 处理 |
|------|------|
| 目标死亡 | `player_attack_command` 验证 alive → 清 Target → 停止追击/射击 |
| 目标死亡时箭矢在飞 | `arrow_movement` Homing 分支：目标 Dead → 保持当前速度直线 → Lifetime 过期销毁。`combat` 中 Dead 目标被 skip → 箭矢不命中 |
| 追击中穿墙 | `player_movement` 设 `Chasing=true` → `wall_collision` 跳过 → 玩家穿过墙朝目标移动 |
| 到达射程停止追击 | `Chasing` 重置为 false → `wall_collision` 恢复正常推回（若玩家在墙内则被推出） |
| 施法中收到攻击命令 | Pending 机制暂存 → `skill_cast` 取消施法(CastState=None) → 下一 tick `player_attack_command` 消费 pending |
| WASD 模式 kiting | WASD 按住走分支 4（手动移动），AttackTarget 保持锁定；松开 WASD 走追击分支 5；在射程内 `player_attack_fire` 射箭 |
| A/Q 点地板无敌人 | `find_nearest_enemy` 返回 null → 不设 Target → 无操作 |
| 右键敌人同时施法中 | `cast_cancel` 取消施法 + `attack_target_id` 设为 pending → 下一 tick 攻击 |
| 箭矢穿过非目标敌人 | `combat` Homing 分支：`target != hom.Target → continue` → 不误伤 |
| Bot 重生（同 entity） | 箭矢 Lifetime(2s) < BotRespawnTime(8s) → 箭矢已过期，不会追踪重生后的 Bot |
| 追击中目标移动到墙后 | 玩家直线穿墙追击 → 不丢失目标 → 到射程后射 homing 箭穿墙命中 |
| WASD 移动时被推回墙 | 正常推回（`Chasing=false`，`wall_collision` 正常执行） |

---

## 12. 实施顺序

### 阶段 A — C++ 核心链路（最小闭环）

| 步骤 | 文件 | 说明 |
|------|------|------|
| A1 | `components.h` | +`AttackTarget`(含 `Chasing`) +`Homing` + 输入字段 |
| A2 | `game_config.h` | +`PlayerAttackRange`/`Sq` +`AttackAcquisitionRange` |
| A3 | `arrow_spawner.h` | `ArrowSpawnContext` +homing；`try_fire` emplace `Homing` |
| A4 | `player_attack_command.h` **新增** | 命令处理 + pending + 验证 + 最近敌搜索 |
| A5 | `player_attack_fire.h` **新增** | 锁定目标射 homing 箭 |
| A6 | `player_movement.h` | 重置 `Chasing` + 追击分支（直线移动 + 设 `Chasing=true`） |
| A7 | `arrow_movement.h` | +Homing 追踪 |
| A8 | `wall_collision.h` | Mover 跳过 `Chasing` + Arrow 跳过 `Homing` |
| A9 | `combat.h` | Homing 只命中锁定目标 |
| A10 | `local_input_injection.h` | +复制 attack 字段 |
| A11 | `world.h/.cpp` | tick 顺序 + `set_attack_command` + `_spawn_player` +`AttackTarget` + 构造参数 |
| A12 | `sim_server.h/.cpp` + `register_types.cpp` | 绑定 API |
| A13 | `snapshot_types.h` + `bindings` + `builder` | +`attack_target_id` |
| A14 | 删除 `player_fire.h` | — |

**验收**：编译通过，View 手动调 `set_attack_command(target_id, ...)` 看玩家穿墙追击 + 射 homing 箭必中。

### 阶段 B — GDScript 输入

| 步骤 | 文件 | 说明 |
|------|------|------|
| B1 | `input_collector.gd` | Q/A 键 + attack_command_mode + 右键攻击 + 左键重构 |
| B2 | `sim_bridge.gd` | `set_attack_command` 调用 + 帧末清理 |

**验收**：
- MOBA 模式 A+左键点 bot → 穿墙追击 → 射箭命中
- 右键 bot → 直接攻击
- 右键空地 → 移动（清锁）
- WASD 模式 Q+左键点 bot → 追击 + 射箭

### 阶段 C — View 指示器

| 步骤 | 文件 | 说明 |
|------|------|------|
| C1 | `entity_view.gd` | +`_attack_target_mat` + `set_attack_targeted` |
| C2 | `entity_manager.gd` | +`set_attack_target_id` |
| C3 | `sim_bridge.gd` | 传 `attack_target_id` 到 View |

**验收**：锁定目标时目标显示红色高亮，取消锁定时恢复。

### 阶段 D — 调参 & 测试

| # | 项目 | 方法 |
|---|------|------|
| 1 | `PlayerAttackRange` | 试 6 / 8 / 10 |
| 2 | `AttackAcquisitionRange` | 试 10 / 15 / 20 |
| 3 | Homing 追踪手感 | 箭矢是否自然弯曲、是否必中 |
| 4 | 追击穿墙 | 确认追击中不被墙推回，到达射程后恢复碰撞 |
| 5 | WASD 模式 Q 键 kiting | WASD 移动 + Q 锁定 + 自动射击 |
| 6 | 追击速度 | 是否需要减速追击（比正常移动慢） |

---

## 13. 难度评估

| 模块 | 难度 | 工时 |
|------|------|------|
| C++ 组件 + 输入扩展 | 低 | 0.3 天 |
| `player_attack_command`（pending + 搜索） | 中 | 0.5 天 |
| `player_attack_fire`（homing 箭） | 低 | 0.3 天 |
| `player_movement` 追击分支 + `Chasing` 标志 | 低 | 0.2 天 |
| `arrow_movement` + `wall_collision` + `combat` | 低 | 0.3 天 |
| Snapshot + API 绑定 | 低 | 0.2 天 |
| `input_collector` 重构 | 中 | 0.5 天 |
| View 指示器 | 低 | 0.2 天 |
| 联调 & 调参 | 中 | 0.5 天 |
| **合计** | **中** | **~3 工作日** |

**最大风险**：`input_collector` 的右键仲裁（攻击/移动/取消三重语义）+ pending 机制跨 tick 时序。建议严格按阶段 A→B 顺序，先跑通 C++ 核心链路再接 GDScript 输入。
