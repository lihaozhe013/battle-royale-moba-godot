# 技能系统设计方案（4 技能原型）

> 创建：2026-07-07
> 关联：`prompt.md` §MOBA 升级方案、`sim_system_reference.md` §11-P0-2
> 范围：玩家 4 技能（Q/W/E/R）+ 手动施法模式 + 简易 GDScript VFX
> 不在本方案范围：Bot 技能、装备、缩圈、死亡淘汰

---

## 0. 设计决策摘要（已与用户确认）

| 项 | 决策 |
|----|------|
| 施法模式 | 手动施法：按技能键→进入 Aiming→左键确认→前摇→释放 |
| 快速施法 | ❌ 不做 |
| 施法指示器 | 仅一条绿线从英雄中心到鼠标指针（Dota2 早期风格） |
| 取消施法（Aiming 阶段） | 右键 / ESC / S(stop) / H 任一 |
| 前摇打断 | 可被移动键（WASD 任一）打断；**不退蓝不退 CD**（惩罚） |
| F 引导打断 | 完全不可打断（移动/伤害/右键均无效）；伤害照常扣血 |
| Q 目标选取 | 指向性技能：真单位选取（悬停高亮 + 点人选人） |
| R 位移 | 朝鼠标方向**固定距离**滑行（非闪现），墙阻挡即停 |
| F 弹幕 | 每 0.5s 向 16 方向齐射，5s 共 10 波 = 160 发 |
| Mana 上限 | Player Max Mana 100 → **300** |
| 技能键映射 | C=slot0, E=slot1, R=slot2, F=slot3（沿用现有 SkillQ/W/E/R 字段名） |
| Bot 技能 | ❌ 本方案不做。Bot 的 SkillComponent 仅作 UI 占位 |
| VFX 实现 | 全 GDScript（`_draw` / `ImmediateMesh` / 半透明 MeshInstance3D），编辑器无需手动做特效 |

---

## 1. 4 技能规格表

| 槽 | 键 | SkillId | 名称 | 蓝耗 | CD | 前摇 | 类型 | 核心参数 |
|----|----|---------|------|------|----|----|------|---------|
| 0 | C | 1 | 打击 | 20 | 5s | 0.5s | 近战单体 | 伤害 40，鼠标点半径 1.5 内最近敌人 |
| 1 | E | 2 | 眩晕领域 | 100 | 20s | 1.0s | AoE+控制 | 半径 4.0，眩晕 2s，伤害 15 |
| 2 | R | 3 | 滑行 | 40 | 10s | 0.2s | 位移 | 距离 8，速度 20，无伤害 |
| 3 | F | 4 | 弹幕风暴 | 230 | 60s | 1.0s | 引导弹幕 | 引导 5s，每 0.5s 16 方向齐射，子弹伤=玩家 Atk，100% 吸血 |

**F 期间**：玩家 Root（不可移动、不可取消、不可被打断流程），伤害照扣血，子弹命中回血=伤害值。

---

## 2. 架构总览

```
input_collector.gd（状态机版）
  ├─ 技能键边沿检测 → 进/切 Aiming
  ├─ 取消键（RMB/ESC/S/H）→ 退 Aiming
  ├─ 左键：Aiming 中=确认；否则=普攻
  └─ 移动键：Aiming 中=取消；Casting 中=打断前摇
        ↓ set_local_input + set_cast_input
LocalInputSingleton（扩字段）
        ↓
skill_cast_system（新，替代 skill_input_system）
  ├─ 状态机：None↔Aiming↔Casting↔(Channeling|Dashing)→None
  ├─ 进 Casting：扣蓝 + 设 CD
  ├─ 前摇结束：按 SkillKind 触发 effect
  ├─ Channeling：每 0.5s spawn 16 方向子弹（Lifesteal=1.0）
  └─ Dashing：每 tick 推进位置
        ↓
player_movement / player_fire / bot_ai（加 gate）
        ↓
combat.h（加 Lifesteal 回血）
        ↓
SimSnapshot（扩 cast/aoe/root 字段）
        ↓
GDScript VFX（绿线/灰圈/位移路径/光环）
```

---

## 3. 新增 / 修改组件

### 3.1 新增组件（`components.h`）

```cpp
// ── SkillEffect components ──

enum class SkillKind : uint8_t {
    MeleeSingle  = 0,  // C
    AoEField     = 1,  // E
    Dash         = 2,  // R
    ChannelBurst = 3,  // F
};

enum class StatusType : uint8_t {
    None = 0,
    Root = 1,  // 禁锢：不能移动，可射击/放技能
    Stun = 2,  // 眩晕：不能移动，不能攻击，不能放技能
};

struct StatusEffect {
    StatusType Type = StatusType::None;
    float Timer = 0.0f;  // 剩余秒
};

struct CastState {
    enum class Phase : uint8_t {
        None       = 0,
        Aiming     = 1,  // 已按技能键，等左键确认
        Casting    = 2,  // 前摇中
        Channeling = 3,  // F 引导中
        Dashing    = 4,  // R 滑行中
    };
    Phase State = Phase::None;
    int ActiveSlot = -1;     // 0-3
    int SkillId = 0;
    float Timer = 0.0f;      // 前摇剩余 / 引导剩余
    float SubTimer = 0.0f;  // F 子弹波次计时器
    Vec2 AimPos{0.0f};      // 确认时鼠标世界坐标（C/E 用）
    Vec2 DashStart{0.0f};   // R 滑行起点
    Vec2 DashTarget{0.0f};  // R 滑行终点
};

struct AoETag {
    int OwnerId = 0;
    int SkillId = 0;
    float Radius = 0.0f;
    float Duration = 0.0f;
    float Timer = 0.0f;  // 剩余
};
```

### 3.2 修改组件

**`ArrowTag`** — 加吸血字段：
```cpp
struct ArrowTag {
    int OwnerId = 0;
    entt::entity OwnerEntity = entt::null;
    float Dmg = 0.0f;
    float LifestealRatio = 0.0f;  // ← 新增，0=无吸血，1=100%
};
```

**`LocalInputSingleton` / `PlayerInputState`** — 加施法输入字段：
```cpp
int CastSlot = -1;       // -1=无 Aiming，0-3=Aiming 中
bool CastConfirm = false;
bool CastCancel = false;
Vec2 CastAim{0.0f};
```

### 3.3 组件挂载清单

| 实体 | 新增组件 | 时机 |
|------|---------|------|
| Player | `CastState`, `StatusEffect` | `_spawn_player` |
| Bot | `StatusEffect` | `_spawn_bot`（Bot 不要 CastState，bot 不施法） |
| AoE 实体 | `Position2D, AoETag, NetworkId` | E 释放时由 CommandBuffer 创建 |
| Arrow | `ArrowTag.LifestealRatio` | F 子弹 spawn 时设 1.0；普攻/R 子弹（如有）设 0.0 |

---

## 4. 技能定义表（新文件 `skill_defs.h`）

```cpp
// src_cpp/sim/skill_defs.h
#pragma once
#include "components.h"

namespace sim {

struct SkillDef {
    int Id = 0;
    SkillKind Kind;
    float CastTime = 0.0f;
    float ManaCost = 0.0f;
    float Cooldown = 0.0f;
    float Damage = 0.0f;
    float Range = 0.0f;        // C=鼠标点半径；E/AoE=落地半径；R=dash 距离
    float EffectValue = 0.0f;  // E=眩晕时长；F=引导时长
};

inline const SkillDef& get_skill_def(int id) {
    static const SkillDef table[] = {
        {0, SkillKind::MeleeSingle, 0,0,0,0,0,0},                                    // [0] 占位
        {1, SkillKind::MeleeSingle,  0.5f, 20.0f,  5.0f, 40.0f, 1.5f, 0.0f},          // C
        {2, SkillKind::AoEField,     1.0f, 100.0f, 20.0f, 15.0f, 4.0f, 2.0f},         // E (EffectValue=root 2s)
        {3, SkillKind::Dash,         0.2f, 40.0f, 10.0f,  0.0f, 8.0f, 0.0f},          // R (Range=distance)
        {4, SkillKind::ChannelBurst, 1.0f, 230.0f, 60.0f, 0.0f, 0.0f, 5.0f},          // F (EffectValue=channel 5s)
    };
    static_assert(sizeof(table)/sizeof(table[0]) == 5);
    return table[id];
}

} // namespace sim
```

**注意**：`game_config.h` 中旧的 `SkillCooldowns[]` / `SkillManaCosts[]` 仍保留（_spawn_player 用它初始化 SkillSlot.MaxCooldown/ManaCost），但实际施法流程读 `get_skill_def()`。两边数值需保持一致——`_spawn_player` 改为从 `get_skill_def()` 初始化以避免双源真值。

---

## 5. 新增 / 修改 System

### 5.1 新增 `skill_cast_system`（替代 `skill_input_system`）

**文件**：`systems/skill_cast.h`

**签名**：
```cpp
inline void skill_cast_system(entt::registry &reg, float dt,
                              CommandBuffer &cb, IdState &ids, double now);
```

**状态机逻辑**：

```
Phase::None + CastSlot>=0 (按下技能键)
  → 检查 SkillId>0、CD==0、Mana>=cost
  → Phase::Aiming, ActiveSlot=CastSlot, SkillId=slot.SkillId

Phase::Aiming + CastConfirm
  → 扣蓝、设 CD、AimPos=CastAim
  → Phase::Casting, Timer=CastTime

Phase::Aiming + CastCancel  → Phase::None
Phase::Aiming + |Move|>0    → Phase::None  (移动取消 Aiming)

Phase::Casting + Timer<=0
  → 按 SkillKind 触发 effect（见 §5.1.1）
  → MeleeSingle/AoEField: Phase::None
  → Dash: Phase::Dashing, Timer=distance/speed, DashStart=pos, DashTarget=pos+dir*distance
  → ChannelBurst: Phase::Channeling, Timer=channel时长, SubTimer=0

Phase::Casting + |Move|>0 (非 F)
  → Phase::None  (前摇打断，不退蓝不退 CD)

Phase::Dashing + Timer<=0  → Phase::None
Phase::Dashing  → 每 tick: pos += normalize(DashTarget-pos) * speed * dt

Phase::Channeling + Timer<=0 → Phase::None
Phase::Channeling + SubTimer<=0
  → spawn 16 方向子弹（LifestealRatio=1.0, Dmg=owner.Atk）
  → SubTimer = 0.5
Phase::Channeling  → SubTimer -= dt, Timer -= dt

Phase::None → 无操作
```

**注意 F 期间所有打断信号（Move/Cancel）被忽略**：Channeling 分支不读 Move/CastCancel。

#### 5.1.1 Effect 触发（按 SkillKind）

**C — MeleeSingle**：
- 在 `AimPos` 周围 `Range`(1.5) 内找最近 `Damageable` 且非 owner
- `hp.Cur -= Damage`(40)；若死亡走正常 KillEvent 流程
- 无 AoE 实体，无子弹

**E — AoEField**：
- 一次性伤害：在 `AimPos` 周围 `Range`(4.0) 内所有 `Damageable`（非 owner）扣 `Damage`(15) HP
- 对每个命中实体设 `StatusEffect.Type=Stun, Timer=EffectValue`(2.0)
- 创建 AoE 实体（Position2D=AimPos, AoETag{OwnerId,SkillId,Radius=4,Duration=2,Timer=2}）供 View 渲染灰圈

**R — Dash**：
- 不在 effect 触发阶段做伤害，仅设 DashTarget
- 实际位移在 Dashing 阶段每 tick 推进（见上）
- 无子弹、无 AoE

**F — ChannelBurst**：
- 进 Channeling 时不立即射子弹
- 每 0.5s（SubTimer 到 0）spawn 16 方向 ArrowTag：
  - 角度 = i * (2π/16), i=0..15
  - `Dmg = owner CombatStats.Atk`
  - `LifestealRatio = 1.0`
  - 复用 `try_fire` 的 spawn 路径（扩展 ArrowSpawnContext 加 LifestealRatio 字段）

### 5.2 新增 `dash_system`（或并入 skill_cast_system）

**推荐并入 `skill_cast_system`**（Dashing 分支直接写位置），不单独建文件，避免跨 system 顺序依赖。

### 5.3 新增 `status_effect_system`

**文件**：`systems/status_effect.h`

```cpp
inline void status_effect_system(entt::registry &reg, float dt) {
    auto view = reg.view<StatusEffect>();
    for (auto e : view) {
        auto &s = view.get<StatusEffect>(e);
        if (s.Type == StatusType::None) continue;
        s.Timer -= dt;
        if (s.Timer <= 0.0f) {
            s.Type = StatusType::None;
            s.Timer = 0.0f;
        }
    }
}
```

### 5.4 新增 `aoe_system`（AoE 实体生命周期）

**文件**：`systems/aoe.h`

```cpp
inline void aoe_system(entt::registry &reg, float dt, CommandBuffer &cb) {
    auto view = reg.view<AoETag>();
    std::vector<entt::entity> to_destroy;
    for (auto e : view) {
        auto &a = view.get<AoETag>(e);
        a.Timer -= dt;
        if (a.Timer <= 0.0f) to_destroy.push_back(e);
    }
    for (auto e : to_destroy) cb.push([e](entt::registry &r){ r.destroy(e); });
}
```

**注意**：E 伤害在释放时一次性结算，AoE 实体**仅作 visual**（灰圈），无持续伤害。若未来要持续伤害，扩展此处。

### 5.5 修改 `player_fire_system`

入口加 gate：
```cpp
if (reg.all_of<CastState>(e)) {
    auto &cs = reg.get<CastState>(e);
    if (cs.State != CastState::Phase::None) continue;
}
```
Aiming/Casting/Channeling/Dashing 期间都禁普攻。

### 5.6 修改 `player_movement_system`

入口加 gate：
```cpp
// Root
if (reg.all_of<StatusEffect>(e)) {
    auto &st = reg.get<StatusEffect>(e);
    if (st.Type == StatusType::Root && st.Timer > 0.0f) continue;
}
// Casting / Channeling / Dashing 期间禁移动
if (reg.all_of<CastState>(e)) {
    auto &cs = reg.get<CastState>(e);
    if (cs.State == CastState::Phase::Casting) continue;
    if (cs.State == CastState::Phase::Channeling) continue;
    if (cs.State == CastState::Phase::Dashing) continue;  // dash 自己管位置
}
// Aiming 期间：若玩家按移动键 → 取消（由 skill_cast_system 处理），本 tick 不移动
if (reg.all_of<CastState>(e)) {
    auto &cs = reg.get<CastState>(e);
    if (cs.State == CastState::Phase::Aiming) continue;
}
```

**注意**：Aiming 阶段移动取消的检测在 `skill_cast_system` 做（读 input.Move），player_movement 仅"不移动"。

### 5.7 修改 `bot_ai_system`

入口加 Status gate：
```cpp
if (reg.all_of<StatusEffect>(e)) {
    auto &st = reg.get<StatusEffect>(e);
    if (st.Timer > 0.0f && (st.Type == StatusType::Root || st.Type == StatusType::Stun)) continue;
}
```
Bot 控制期间不能移动；Root(禁锢) 可瞄准射击，Stun(眩晕) 不可攻击。

### 5.8 修改 `combat_system`（Lifesteal）

命中后加：
```cpp
if (arrow.LifestealRatio > 0.0f && arrow.OwnerEntity != entt::null
    && reg.all_of<Health>(arrow.OwnerEntity)) {
    auto &ohp = reg.get<Health>(arrow.OwnerEntity);
    int heal = (int)(arrow.Dmg * arrow.LifestealRatio);
    ohp.Cur = std::min(ohp.Max, ohp.Cur + heal);
}
```

### 5.9 修改 `arrow_spawner.h`

`ArrowSpawnContext` 加 `float LifestealRatio = 0.0f`，spawn 时写入 `ArrowTag.LifestealRatio`。

### 5.10 删除 / 弃用 `skill_input_system`

完全被 `skill_cast_system` 取代。**删除文件** `systems/skill_input.h`，从 `world.cpp` tick 顺序移除。

---

## 6. tick 顺序（更新后）

```
local_input_injection → player_movement → player_fire → skill_cast →
bot_targeting → bot_ai → bot_combat → arrow_movement → wall_collision →
combat → pickup → aoe → status_effect → mana_regen → skill_cooldown →
progression → snapshot_export
```

**顺序要点**：
- `skill_cast` 在 `player_fire` 之后（player_fire 先 gate 掉 cast 期间的普攻，再由 skill_cast 推进状态机）
- `skill_cast` 在 `player_movement` 之后（player_movement gate 掉 cast 期间的移动；Dashing 的位置推进在 skill_cast 内做，需在 wall_collision 之前）
- `aoe` / `status_effect` 在 combat 之后（E 伤害已在 skill_cast 内即时结算，aoe 仅倒计时 visual；status 递减 root timer）
- `wall_collision` 在 skill_cast 之后（dash 推进位置后，墙推回；若撞墙，下一 tick skill_cast 检测 pos 未推进则提前结束 dash）

**dash 撞墙处理**：Dashing 分支每 tick 计算 `remaining = |DashTarget - pos|`，若 `remaining` 不减反增（被墙推回）或 `remaining < threshold` → 结束。简单实现：`if (distance(pos, DashTarget) < 0.5 || Timer <= 0) → None`。

---

## 7. Snapshot 扩展

### 7.1 `SimPlayerSnap` 新增字段

| 字段 | 类型 | 来源 | 用途 |
|------|------|------|------|
| `cast_state` | int | CastState.Phase | 0-4，View 切换 VFX |
| `cast_slot` | int | CastState.ActiveSlot | 显示哪个槽在施法 |
| `cast_progress` | float | Timer / (CastTime 或 channel 时长) | 0-1，进度条 |
| `cast_aim_x` | float | CastState.AimPos.x | C/E 灰圈位置 |
| `cast_aim_y` | float | CastState.AimPos.y | 同上 |
| `dash_sx` | float | CastState.DashStart.x | R 路径起点 |
| `dash_sy` | float | CastState.DashStart.y | 同上 |
| `dash_tx` | float | CastState.DashTarget.x | R 路径终点 |
| `dash_ty` | float | CastState.DashTarget.y | 同上 |

### 7.2 `SimBotSnap` 新增字段

| 字段 | 类型 | 来源 | 用途 |
|------|------|------|------|
| `status` | int | StatusEffect.Type | 0=None 1=Root(禁锢) 2=Stun(眩晕) |

### 7.3 新增 `SimAoESnap`

```cpp
class SimAoESnap : public godot::RefCounted {
    GDCLASS(SimAoESnap, godot::RefCounted)
public:
    int id; float x, y; float radius; float remaining; float duration;
    // getters/setters + _bind_methods
};
```

### 7.4 `SimSnapshot` 新增

```cpp
godot::TypedArray<SimAoESnap> aoes;
```

### 7.5 绑定 & 构建

- `snapshot_bindings.cpp`：BIND/PROP 注册新字段
- `snapshot_builder.cpp`：
  - `_build_player` 填 cast_*/dash_* 字段（若实体有 CastState）
  - `_build_player`/`_build_bot` 都填 status（若实体有 StatusEffect）
  - 新增 `_build_aoes`：遍历 `AoETag` 实体填 SimAoESnap
- `register_types.cpp`：`ClassDB::register_class<SimAoESnap>()`

---

## 8. SimServer API 扩展

新增方法（不改 `set_skill_input` 签名，避免破坏现有调用）：

```cpp
// sim_server.h
void set_cast_input(int cast_slot, bool confirm, bool cancel, float aim_x, float aim_y);
```

`world.cpp` 加对应 setter，写入 `LocalInputSingleton.CastSlot/Confirm/Cancel/CastAim`。

**`set_skill_input` 保留**：仍传 skill_q/w/e/r（input_collector 用它告知哪些键被按住，但实际触发改由 set_cast_input 的 cast_slot 驱动）。或者**简化**：废弃 `set_skill_input`，全部走 `set_cast_input`。

**推荐**：废弃 `set_skill_input`，`input_collector` 内部把技能键边沿逻辑算好后，只通过 `set_cast_input` 传 `cast_slot`（-1 或 0-3）+ confirm + cancel + aim。更干净。

---

## 9. input_collector.gd 重写

```gdscript
extends Node

var move_input := Vector2.ZERO
var aim_world := Vector2.ZERO
var fire := false
var input_seq := 0

# 施法状态
var cast_slot := -1          # -1=无 Aiming, 0-3=Aiming 中
var cast_confirm := false    # 本帧左键确认（Aiming 中）
var cast_cancel := false     # 本帧取消
var cast_aim := Vector2.ZERO

var _prev_skill := [false, false, false, false]  # 边沿检测
const SKILL_KEYS := [KEY_C, KEY_E, KEY_R, KEY_F]

func _process(_delta: float) -> void:
    input_seq += 1
    _read_move()
    _read_aim()
    _read_skill_input()

func _read_move() -> void:
    var h := 0.0
    var v := 0.0
    if Input.is_key_pressed(KEY_W) or Input.is_key_pressed(KEY_UP):   v += 1
    if Input.is_key_pressed(KEY_S) or Input.is_key_pressed(KEY_DOWN): v -= 1
    if Input.is_key_pressed(KEY_A) or Input.is_key_pressed(KEY_LEFT): h += 1
    if Input.is_key_pressed(KEY_D) or Input.is_key_pressed(KEY_RIGHT):h -= 1
    var raw := Vector2(h, v)
    move_input = raw.normalized() if raw.length_squared() > 1.0 else raw

func _read_aim() -> void:
    var cam := get_viewport().get_camera_3d()
    if cam:
        var mouse_pos := get_viewport().get_mouse_position()
        var from := cam.project_ray_origin(mouse_pos)
        var dir := cam.project_ray_normal(mouse_pos)
        if abs(dir.y) > 0.001:
            var t := -from.y / dir.y
            var hit := from + dir * t
            aim_world = Vector2(hit.x, hit.z)
    cast_aim = aim_world

func _read_skill_input() -> void:
    cast_confirm = false
    cast_cancel = false

    # 1. 技能键边沿 → 进/切 Aiming
    for i in 4:
        var pressed = Input.is_key_pressed(SKILL_KEYS[i])
        if pressed and not _prev_skill[i]:
            # 进 Aiming（即使已在 Aiming 也允许换槽）
            cast_slot = i
        _prev_skill[i] = pressed

    # 2. 取消键（Aiming 阶段才生效）
    #    S 在 cast mode 下=取消，非 cast mode 下=向下移动（_read_move 已处理）
    if cast_slot >= 0:
        if Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT) \
           or Input.is_key_pressed(KEY_ESCAPE) \
           or Input.is_key_pressed(KEY_H) \
           or (Input.is_key_pressed(KEY_S) and not Input.is_key_pressed(KEY_W)):
            cast_cancel = true
            cast_slot = -1

    # 3. 左键：Aiming 中=确认；否则=普攻
    if Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT):
        if cast_slot >= 0:
            cast_confirm = true
            # 确认后 cast_slot 保持，由 Sim 在 Casting 阶段消费
            # 实际确认后下一帧应清 cast_slot（避免重复确认）
            # → Sim skill_cast 读到 confirm 后进 Casting，下帧 input_collector 见 cast_slot 仍>=0 但已非 Aiming
            # 简化：confirm 当帧消费，下帧 cast_slot 清零
            var confirmed_slot = cast_slot
            cast_slot = -1  # 清除，Sim 用 confirmed_slot 通过 set_cast_input 传
            # 但需把 confirm 信号传出去：用临时变量
            _pending_confirm_slot = confirmed_slot
        else:
            fire = true
    else:
        fire = false
```

**注意**：上述伪代码有状态一致性细节需在实现时打磨：
- `cast_confirm` + `cast_slot` 需在同一帧传给 Sim，建议 input_collector 输出一个 `(cast_slot, confirm, cancel)` 三元组，sim_bridge 一次性 `set_cast_input`。
- S 键的双重语义（移动 vs 取消）需仔细判断：cast_slot>=0 时 S=取消，否则 S=向下移动。上面用 `not W` 排除上下同按。
- `fire` 仍用轮询（held），普攻本就是持续射击。

**sim_bridge 调用**：
```gdscript
sim.set_local_input(input_collector.move_input, input_collector.aim_world,
                    input_collector.fire, input_collector.input_seq)
sim.set_cast_input(input_collector.cast_slot,
                   input_collector.cast_confirm,
                   input_collector.cast_cancel,
                   input_collector.cast_aim.x, input_collector.cast_aim.y)
```

---

## 10. View / VFX 层（新文件）

### 10.1 新增 `scripts/view/skill_vfx.gd`

挂 `main.tscn` 作为 `sim_bridge` 子节点 `SkillVFX`（Node3D）。

**职责**：读 `SimSnapshot`，渲染所有技能视觉。

```gdscript
class_name SkillVFX
extends Node3D

# 子节点（_ready 中代码创建，避免编辑器手动加）
var _cast_line: ImmediateMesh  # 绿线（Aiming 时）
var _aoe_meshes: Array[MeshInstance3D]  # AoE 灰圈池
var _dash_line: ImmediateMesh  # R 位移路径
var _player_aura: MeshInstance3D  # F 光环

func sync(snap: SimSnapshot, player_entity_view) -> void:
    # 1. cast_state 决定绿线显示
    # 2. aoes[] 决定灰圈显示
    # 3. cast_state==Dashing 显示 dash 路径
    # 4. cast_state==Channeling 显示光环
    pass
```

**各 VFX 实现**：
- **绿线**（Aiming）：`ImmediateMesh` 画一条 Line from `player.pos` to `cast_aim`，绿色 `Color(0.2,1.0,0.2,0.8)`，每帧重建。
- **E 灰圈**（AoE）：MeshInstance3D + CylinderMesh，半径=snap.aoes[i].radius，半透明灰 `Color(0.5,0.5,0.5,0.3)`，位置=(x, 0.05, y)。对象池。
- **R 路径**（Dashing）：`ImmediateMesh` 画 Line from `dash_start` to `player.pos`（动态缩短），蓝色 `Color(0.3,0.5,1.0,0.6)`。
- **F 光环**（Channeling）：player 脚下圆柱环，半透明红 `Color(1.0,0.3,0.3,0.3)`，半径 1.5。
- **C 命中闪**（可选）：释放时在 aim_pos 短暂灰圈 0.2s。
- **C 命中刀光**：详见 §10.4。

### 10.2 `sim_bridge.gd` 集成

```gdscript
@onready var skill_vfx = $SkillVFX

# _process 中新增
if last_snapshot.players.size() > 0:
    var p = last_snapshot.players[0] as SimPlayerSnap
    var ev = entity_manager.get_entity(p.id)
    skill_vfx.sync(last_snapshot, ev)
```

### 10.3 `main.tscn` 改动

仅加 `SkillVFX` (Node3D) 节点，挂 `skill_vfx.gd` 脚本。无其他编辑器操作。

### 10.4 指向性技能命中 VFX 挂载节点

**新增 `scripts/view/skill_vfx_attachment.gd`：**

所有指向性技能命中效果（C 光柱、以及未来的 E/R/F 命中反馈）挂在此节点下，跟随目标英雄移动。

```gdscript
class_name SkillVfxAttachment
extends Node3D

func show_c_slash() -> void:
    # 创建从地面延伸到天空的深蓝光柱：
    #   - 核心 CylinderMesh (radius=0.15, height=80.0) 亮蓝 alpha 0.85
    #   - 外层 CylinderMesh (radius=0.4, height=80.0) 浅蓝光晕 alpha 0.25
    #   - 无 billboard，始终垂直
    #   - 0-0.1s 淡入 → 保持 1.0s → 0.2s 淡出 → 清理
    var root := Node3D.new()
    root.position = Vector3(0, 0.05, 0)
    add_child(root)
    var cyl := CylinderMesh.new()
    cyl.top_radius = 0.15
    cyl.bottom_radius = 0.15
    cyl.height = 80.0
    var mat := StandardMaterial3D.new()
    mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    mat.albedo_color = Color(0.1, 0.4, 0.9, 0.0)
    mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
    var beam := MeshInstance3D.new()
    beam.mesh = cyl
    beam.material_override = mat
    beam.position = Vector3(0, 40.0, 0)
    root.add_child(beam)
    # ... glow + tween
```

**生命期：** 作为 EntityView 的子节点创建（`_ready()` 后在 `find_children` 收集 `_child_meshes` 之后创建），不参与模型 mesh 列表 → 英雄死亡隐藏模型时不隐藏此节点。

**Bot 死亡停留：** `BotRespawnTime` 从 3.0s 改为 8.0s（`game_config.h`），死亡后 bot 位置冻结在死亡点，模型隐藏但 SkillVfxAttachment 保持可见，VFX 可完整播放。当前 C 技能 VFX 总时长 1.3s（淡入 0.1s + 保持 1.0s + 淡出 0.2s）。

---

## 11. 文件改动清单

### 11.1 C++ 新增

| 文件 | 内容 |
|------|------|
| `src_cpp/sim/skill_defs.h` | SkillDef 表 + get_skill_def() |
| `src_cpp/sim/systems/skill_cast.h` | 施法状态机 + effect 触发 + dash 推进 |
| `src_cpp/sim/systems/status_effect.h` | Root timer 递减 |
| `src_cpp/sim/systems/aoe.h` | AoE 实体生命周期 |

### 11.2 C++ 修改

| 文件 | 改动 |
|------|------|
| `components.h` | +SkillKind/StatusType 枚举, +StatusEffect/CastState/AoETag 组件, ArrowTag +LifestealRatio, LocalInputSingleton/PlayerInputState +CastSlot/Confirm/Cancel/CastAim |
| `game_config.h` | PlayerBaseMana 100→300, SkillCooldowns/SkillManaCosts 数值对齐 skill_defs.h（或删表改读 get_skill_def） |
| `arrow_spawner.h` | ArrowSpawnContext +LifestealRatio, spawn 时写入 |
| `systems/player_fire.h` | +CastState gate |
| `systems/player_movement.h` | +Root+Stun/CastState gate |
| `systems/bot_ai.h` | +Root+Stun gate |
| `systems/player_fire.h` | +Stun gate |
| `systems/skill_cast.h` | +Stun gate |
| `systems/bot_combat.h` | +Stun gate |
| `systems/combat.h` | +Lifesteal 回血 |
| `world.h/.cpp` | tick 顺序加 skill_cast/aoe/status_effect, _spawn_player/bot 加 CastState/StatusEffect, SkillSlot 初始化改读 get_skill_def, 新增 set_cast_input |
| `sim_server.h/.cpp` | +set_cast_input 方法 |
| `snapshot_types.h` | SimPlayerSnap +cast_*/dash_*/status 字段, SimBotSnap +status(替代 root_timer), +SimAoESnap, SimSnapshot +aoes |
| `snapshot_bindings.cpp` | 注册新字段 + SimAoESnap |
| `snapshot_builder.cpp` | _build_player/bot 填新字段(status 替代 root_timer), +_build_aoes |
| `register_types.cpp` | register SimAoESnap |

### 11.3 C++ 删除

| 文件 | 原因 |
|------|------|
| `src_cpp/sim/systems/skill_input.h` | 被 skill_cast_system 取代 |

### 11.4 GDScript 新增

| 文件 | 内容 |
|------|------|
| `scripts/view/skill_vfx.gd` | 全部技能 VFX（绿线/灰圈/dash 路径/光环） |
| `scripts/view/skill_vfx_attachment.gd` | 指向性技能命中 VFX 挂载节点（C 刀光等） |

### 11.5 GDScript 修改

| 文件 | 改动 |
|------|------|
| `scripts/input/input_collector.gd` | 重写：施法状态机 + 边沿检测 + S 键双重语义 |
| `scripts/sim_bridge.gd` | +skill_vfx @onready, _process 调 skill_vfx.sync, _physics_process 调 set_cast_input；修复 _trigger_c_slash 跳过死亡 bot 的 bug |
| `scripts/view/entity_view.gd` | +SkillVfxAttachment 子节点，死亡改为仅隐藏模型 mesh（保留 VFX 可见），+_dead 标志处理重生 snap |

### 11.6 场景

| 文件 | 改动 |
|------|------|
| `scenes/main.tscn` | +SkillVFX (Node3D) 节点，挂 skill_vfx.gd |

---

## 12. 实施顺序（建议）

**阶段 A — 跑通 C 技能全链路（最小闭环）**
1. `skill_defs.h`（仅 id=1 C 的定义）
2. `components.h` 加 CastState/StatusEffect/SkillKind/StatusType + ArrowTag.Lifesteal（暂不用）+ LocalInput 扩字段
3. `skill_cast.h` 仅实现 None↔Aiming↔Casting + C 的 MeleeSingle effect
4. `player_fire`/`player_movement` 加 gate
5. `sim_server` + `world` 加 set_cast_input + tick 顺序
6. `input_collector` 重写（仅 C 槽 + 取消键）
7. `snapshot` 扩 cast_state/cast_aim 字段 + bindings/builder
8. `skill_vfx.gd` 仅绿线
9. `main.tscn` 加 SkillVFX
10. **测试**：按 C → 绿线 → 左键 → 前摇 0.5s → 周围 bot 掉血 40

**阶段 B — 加 R 位移**
1. skill_defs 加 id=3
2. skill_cast 加 Dashing 分支 + dash 推进
3. player_movement 加 Dashing gate
4. snapshot 加 dash_* 字段
5. skill_vfx 加 dash 路径
6. **测试**：R → 左键 → 滑行 8 单位，撞墙停止

**阶段 C — 加 E AoE+眩晕**
1. skill_defs 加 id=2
2. skill_cast 加 AoEField effect（即时伤害 + 创建 AoE 实体 + 设 Stun）
3. status_effect.h + aoe.h
4. bot_ai 加 Status gate
5. snapshot 加 SimAoESnap + status
6. skill_vfx 加 E 灰圈 + bot stun 图标
7. **测试**：E → 左键 → 1s 前摇 → 落地灰圈 2s + bot 眩晕 2s

**阶段 D — 加 F 大招**
1. skill_defs 加 id=4
2. skill_cast 加 Channeling 分支 + 16 方向子弹 spawn
3. arrow_spawner 加 LifestealRatio
4. combat 加 Lifesteal 回血
5. skill_vfx 加 F 光环
6. **测试**：F → 左键 → 1s 前摇 → 5s 引导弹幕 + 命中回血

**阶段 E — 数值 & 打磨**
1. Mana Max=300，regen 调参
2. VFX 颜色/透明度调参
3. input_collector S 键边界 case 测试

---

## 13. 注意事项 & 陷阱

### 13.1 输入边沿
- `Input.is_key_pressed` 是 held，必须存 `_prev` 数组做 rising edge。
- `cast_confirm` 是**单帧脉冲**，sim_bridge 每 tick 调 set_cast_input，Sim 消费后下帧自动清（input_collector 下帧 cast_confirm=false）。
- 风险：30Hz Sim tick 与 60Hz _process 不同步，confirm 脉冲可能跨 tick。建议 input_collector 在 `_physics_process` 里读输入（与 sim_bridge 同频），或 sim_bridge 每 tick 读最新值（held 语义）。

### 13.2 S 键双重语义
- cast_slot>=0（Aiming）时 S=取消；<0 时 S=向下移动。
- 实现：`_read_move` 不读 S 当 cast_slot>=0；`_read_skill_input` 单独检测 S 做取消。
- 边界：玩家同时按 W+S（一般 MOBA 不允许），建议 W 优先（S 取消失效）。

### 13.3 前摇打断的"不退蓝不退 CD"
- 进入 Casting 时已扣蓝 + 设 CD。若 Casting 被移动打断，仅 `CastState.State=None`，**不动 SkillSlot.CooldownTimer / Mana.Cur**。
- 玩家体验：打断=浪费蓝+CD。符合 MOBA 惩罚惯例。

### 13.4 F 不可打断
- Channeling 分支**不读** input.Move / CastCancel。
- 但 F 期间仍受伤害（combat 正常跑），仅流程不中断。
- F 期间玩家 Root：player_movement gate Channeling。简单起见：E 的 AoE 排除 owner，所以 F 期间自己不会被 stun。

### 13.5 dash 撞墙
- skill_cast Dashing 分支推进位置 → wall_collision 推回 → 下一 tick skill_cast 检测 `distance(pos, DashTarget)` 未减小 → 提前结束。
- 或：dash 期间若 `wall_collision` 推回量 > 0 → 立即结束（需 wall_collision 与 dash 通信，复杂）。推荐前者（距离阈值）。

### 13.6 AoE 实体 NetworkId
- AoE 实体需要 NetworkId 吗？View 用 snap.aoes 数组渲染，不需查 entity_manager，**可以不要 NetworkId**。但为一致性建议加（从 IdState 加 NextAoEId，起始 4001）。

### 13.7 F 子弹 owner
- F 的子弹用 owner CombatStats.Atk 作为伤害。若期间玩家 Atk 变化（升级），每波子弹取当前 Atk。无问题。
- Lifesteal 回血写到 owner Health.Cur，需 owner_entity 有效。ArrowTag.OwnerEntity 已存。

### 13.8 Snapshot 性能
- F 期间 160 发子弹同时在飞，snap.arrows 数组会膨胀。当前无对象池上限，可能有 GC 压力。原型阶段可接受，后续优化。

### 13.9 SkillSlot 初始化双源真值
- 旧 `game_config.h` 的 `SkillCooldowns[]`/`SkillManaCosts[]` 与新 `skill_defs.h` 的 `get_skill_def().Cooldown/ManaCost` 重复。
- 推荐：`_spawn_player` 改为 `for i: sc.Slots[i] = {SkillId=i+1, Level=1, MaxCooldown=get_skill_def(i+1).Cooldown, ManaCost=get_skill_def(i+1).ManaCost}`，删 `game_config` 旧表。

### 13.10 Bot SkillComponent
- Bot 仍挂 SkillComponent（UI 占位），但 bot 不走 skill_cast（skill_cast 只处理 PlayerTag.IsLocal）。Bot 的 SkillSlot 永远不触发。
- skill_cooldown_system 仍递减 bot 的 CooldownTimer（无意义但无害）。

---

## 14. 难度复评（基于本方案）

| 模块 | 难度 | 工时 |
|------|------|------|
| C++ 施法状态机框架 | 中高 | 1 天 |
| C 闭环节点 | 低 | 0.5 天 |
| R dash | 中 | 0.5 天 |
| E AoE+Stun | 中 | 0.5 天 |
| F channel+吸血 | 中 | 0.5 天 |
| Snapshot 扩字段 | 低 | 0.3 天 |
| input_collector 重写 | 中 | 0.5 天 |
| View VFX | 低 | 0.5 天 |
| 联调 & 数值 | 中 | 0.5 天 |
| **合计** | **中高** | **~4.5 工作日** |

最大风险：施法状态机跨 tick + 输入边沿同步。建议严格按 §12 阶段 A 跑通 C 全链路再扩展。
