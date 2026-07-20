# 输入系统设计方案（v2 — 唯一标准）

> 创建：2026-07-13
> 状态：📋 设计方案（待实施），实施前请将其视为输入相关所有问题的唯一标准
> 取代：`skill_system_design.md`、`targeted_attack_design.md`、`right_click_movement_design.md`、`skill_cast_error_fix.md`（均已删除）
> 关联：`sim_system_reference.md`（C++ 层组件/系统/快照参考）、`prompt.md`（玩法总纲）

---

## 0. 历史澄清与范围

- 本项目早期曾有 **WASD + MOBA 双控制模式**，**WASD 模式已完全废弃移除**。任何旧文档中提到的 WASD 模式、`MoveMode` 枚举、`move_mode` 字段、`mode_changed` 信号、双模式切换面板等均为过时误导，**以本文档为唯一标准**。
- 当前与未来只有一种控制模式：**MOBA 模式**（右键点地板移动 + Q/W/E/R 4 技能 + A 键普攻命令）。
- 本文档职责：**完整重构 input_controller 与 Sim 侧施法/普攻/移动的输入链路**，建立分层、状态化、不丢指令的输入体系。
- 不在本方案范围：Bot 行为重构、装备系统、缩圈、死亡淘汰。Bot 当前为占位单位（其攻击是"非指向性技能"占位，**不是**普攻），未来会重构成与玩家完全相同的英雄单位；**Bot 的当前行为不得影响 input_controller 的设计**。

---

## 目录

1. [设计目标与原则](#1-设计目标与原则)
2. [总体架构（四层）](#2-总体架构四层)
3. [Layer 1 — 输入事件队列（不丢指令）](#3-layer-1--输入事件队列不丢指令)
4. [Layer 2 — Input 状态机（View 侧 FSM）](#4-layer-2--input-状态机view-侧-fsm)
5. [Layer 3 — 命令翻译（Command Builder）](#5-layer-3--命令翻译command-builder)
6. [Layer 4 — 命令缓冲与 Sim 消费](#6-layer-4--命令缓冲与-sim-消费)
7. [Sim 侧 CastState 重构（含 Chasing 跟随施法）](#7-sim-侧-caststate-重构含-chasing-跟随施法)
8. [Quick Cast 与 Normal Cast 流程](#8-quick-cast-与-normal-cast-流程)
9. [普攻命令模式（独立分支）](#9-普攻命令模式独立分支)
10. [移动与寻路（A\* 追击/跟随）](#10-移动与寻路a-追击跟随)
11. [SimServer API（统一命令接口）](#11-simserver-api统一命令接口)
12. [Snapshot 扩展（状态回写同步）](#12-snapshot-扩展状态回写同步)
13. [tick 顺序](#13-tick-顺序)
14. [组件变更清单](#14-组件变更清单)
15. [文件改动清单](#15-文件改动清单)
16. [实施阶段](#16-实施阶段)
17. [边界情况与陷阱](#17-边界情况与陷阱)
18. [与 Bot 单位的关系澄清](#18-与-bot-单位的关系澄清)

---

## 1. 设计目标与原则

| #   | 目标                                      | 对应原则                                                             |
| --- | ----------------------------------------- | -------------------------------------------------------------------- |
| G1  | input_controller 解耦，职责清晰           | 按 **四层** 分离：原始事件 / 状态机 / 命令翻译 / 命令缓冲            |
| G2  | 显式状态机，避免散落条件分支              | View 与 Sim **各自维护 FSM**，通过 snapshot **双向同步**             |
| G3  | 打断施法不引发 bug                        | input 层 **镜像 Sim 的 CastState**，打断 = 状态转移而非多处 flag     |
| G4  | 不丢指令（30Hz Sim < 60Hz 渲染）          | 单一 **CommandBuffer 层** 统一处理跨 tick 指令，**不每操作单独处理** |
| G5  | Quick cast / Normal cast 双模式共存       | 由 **玩家偏好**（per-slot 或全局）决定，input 层翻译时分支           |
| G6  | Normal cast / 普攻命令 / 移动模式相互独立 | **三个独立 FSM 分支**，最解耦最不易出 bug                            |
| G7  | 跟随施法（超出范围 A\* 追踪）             | Sim 侧新增 **Chasing** 阶段，由 Sim 权威推进                         |
| G8  | 普攻能穿墙                                | Sim 侧 `wall_collision` 对追击中玩家 + homing 箭矢跳过               |

**核心原则**：

1. **Sim 权威**：所有 gameplay 状态（CastState / AttackTarget / MovePath）由 Sim 拥有，View 只读 snapshot。
2. **View 镜像**：input_controller 维护一个本地 FSM 副本，**仅用于决定如何翻译下一个输入事件**，绝不作为 gameplay 真值。
3. **命令语义化**：View → Sim 传的是 **命令**（"施法槽 2 在 (x,y) 对 target_id 确认"），不是原始按键。
4. **事件不丢**：所有边沿（key press / release / mouse click）入队，CommandBuffer 持续消费直到清空。

---

## 2. 总体架构（四层）

```
┌──────────────────────────────────────────────────────────────┐
│  Godot 原始输入（InputEvent / Input.is_*_pressed）             │
│  60Hz _process / _physics_process                              │
└──────────────────────────┬───────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────┐
│  Layer 1 ─ Input Event Queue (GDScript)                       │
│  - _input/_unhandled_input 收集边沿事件入队                     │
│  - 持续状态（held keys、mouse pos）每帧采样                     │
│  - 队列元素：{type, key/button, pos, timestamp, seq}           │
│  - 永不丢失，直到 Layer 3 消费                                  │
└──────────────────────────┬───────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────┐
│  Layer 2 ─ Input State Machine (GDScript)                     │
│  - 双正交状态轴：                                               │
│      MoveAxis   : Moving | NotMoving                          │
│      CommandAxis: Idle | SkillAiming | AttackAiming | CastLocked│
│  - 读 Sim snapshot 的 cast_state 反向同步 CastLocked            │
│  - 输出"当前帧应处理哪些事件"                                   │
└──────────────────────────┬───────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────┐
│  Layer 3 ─ Command Builder (GDScript)                         │
│  - 根据 FSM 状态 + 事件队列 → 生成语义命令                       │
│  - 命令类型：MoveCmd / SkillCmd / AttackCmd / CancelCmd / StopCmd│
│  - Quick vs Normal cast 在此分支                                │
└──────────────────────────┬───────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────┐
│  Layer 4 ─ Command Buffer (GDScript + C++)                    │
│  - GDScript 端：FIFO 队列，每帧可入多条                          │
│  - sim_bridge 每 Sim tick 出队 N 条 → 调 SimServer.set_command  │
│  - 跨 tick 持续，直到清空（不丢失）                              │
└──────────────────────────┬───────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────┐
│  C++ Sim（30Hz）                                               │
│  - 消费命令 → 写 LocalInputSingleton                            │
│  - local_input_injection → player_attack_command →             │
│    player_pathfinding → player_movement → player_attack_fire → │
│    skill_cast → ...                                            │
│  - 输出 SimSnapshot（cast_state / attack_target_id / ...）     │
└──────────────────────────┬───────────────────────────────────┘
                           │
                           ▼
            SimSnapshot → View → Layer 2 反向同步
```

---

## 3. Layer 1 — 输入事件队列（不丢指令）

### 3.1 动机

Sim 30Hz，渲染 60Hz。一帧渲染内 sim_bridge 可能跑 0/1/2 个 tick。若每操作各自管"是否传给 Sim"，会出现：

- 边沿事件（key press）发生在 tick 间隙 → 下一 tick 已 release → Sim 永远没看到 → **丢指令**
- 多个事件同帧发生 → 只传最后一个 → **丢指令**

**解决**：所有边沿事件入队，CommandBuffer 持续消费直到清空。

### 3.2 数据结构

```gdscript
# scripts/input/input_event_queue.gd
class_name InputEventQueue
extends Node

enum EType { KEY_PRESS, KEY_RELEASE, MB_PRESS, MB_RELEASE, MOUSE_MOVE }

struct Ev:
    var type: int
    var key: int        # KEY_* 或 MOUSE_BUTTON_*
    var pos: Vector2    # 鼠标世界坐标（MOUSE_MOVE / MB_* 时填）
    var t: float        # Time.get_ticks_msec() / 1000.0
    var seq: int        # 自增序号

var _queue: Array[Ev] = []
var _seq := 0
var mouse_world := Vector2.ZERO      # 持续状态：当前鼠标世界坐标
var held_keys: Dictionary = {}       # 持续状态：当前按住的键 → true
var held_mouse: Dictionary = {}      # 持续状态：当前按住的鼠标键 → true

func push_key_press(k: int) -> void
func push_key_release(k: int) -> void
func push_mb_press(b: int, pos: Vector2) -> void
func push_mb_release(b: int, pos: Vector2) -> void
func push_mouse_move(pos: Vector2) -> void

func pop_all() -> Array[Ev]    # 取出并清空
func peek_all() -> Array[Ev]   # 只读
```

### 3.3 接入方式

- `_input(event)` 或 `_unhandled_input(event)`：捕获 `InputEventKey` / `InputEventMouseButton`，按 press/release 入队；`InputEventMouseMotion` 更新 `mouse_world`（射线投影到地面）+ 入 `MOUSE_MOVE` 事件。
- 持续状态（`held_keys` / `held_mouse`）每帧由 `Input.is_*_pressed` 校准一次，防止失焦等导致的状态漂移。
- 鼠标世界坐标投影（cam ray → y=0 平面）放本层，下游一律用 `mouse_world`，避免重复算。

### 3.4 不丢失保证

- 入队后事件**只能被 Layer 3 pop_all 取走**，不会因 tick 边界丢失。
- Layer 3 一次处理全部 pop 出的事件，可能生成 0~N 条 Command 入 Layer 4。
- Layer 4 是跨 tick FIFO，sim_bridge 每 tick 消费到空为止（或最多 N 条防爆炸）。

---

## 4. Layer 2 — Input 状态机（View 侧 FSM）

### 4.1 双正交状态轴

为支持"施法模式可在移动状态下进入，不会打断移动"，采用**双正交轴**而非单 FSM：

```
MoveAxis（移动轴）：
  NotMoving  — 无活动 MovePath
  Moving     — Sim 侧玩家正在自主移动（右键寻路 / 普攻追击 / 技能 Chasing）
                  由 snapshot `is_moving` 字段判定（非 MovePath.Following，见 §12.1 注意）

CommandAxis（命令轴）：
  Idle          — 无待确认命令
  SkillAiming   — Normal cast 等待左键确认（绿线显示）
  AttackAiming  — A 键 / 右键点敌 等待左键确认（独立模式）
  CastLocked    — Sim 侧 CastState != None（Aiming/Chasing/Casting/Channeling/Dashing）
                  input 层镜像，仅响应 cancel/interrupt
```

**正交含义**：

- `Moving + SkillAiming` 合法 → 玩家右键移动中按 Q 进入瞄准，**移动继续**。
- `Moving + CastLocked` 合法 → Sim 侧 Chasing 阶段，玩家**正在边走边追**以施法。
- `NotMoving + CastLocked` 合法 → Sim 侧 Casting/Channeling/Dashing，玩家**站定施法**。

### 4.2 状态转移表

#### 4.2.1 MoveAxis

| 当前      | 事件                      | 目标              | 备注                                                          |
| --------- | ------------------------- | ----------------- | ------------------------------------------------------------- |
| NotMoving | 右键空地                  | Moving            | 入 MoveCmd                                                    |
| NotMoving | snapshot is_moving==true  | Moving            | Sim 反向同步                                                  |
| Moving    | snapshot is_moving==false | NotMoving         | Sim 反向同步（到达 / Stop / cancel / 目标死亡）               |
| Moving    | 右键空地                  | Moving            | 重发 MoveCmd（覆盖目标）                                      |
| Moving    | S 键 press                | NotMoving（请求） | 入 StopCmd，Sim 清 Following 后下一 snap 才真正切回 NotMoving |

**重要**：MoveAxis **不因进入 SkillAiming/AttackAiming 而改变**。仅由 Sim snapshot 决定（除"右键空地"主动入 Moving 外）。

#### 4.2.2 CommandAxis

| 当前         | 事件                        | 目标                      | 备注                                                                 |
| ------------ | --------------------------- | ------------------------- | -------------------------------------------------------------------- |
| Idle         | 技能键 press（normal cast） | SkillAiming               | 记录 ActiveSlot=Q/W/E/R                                              |
| Idle         | 技能键 press（quick cast）  | CastLocked（待 Sim 确认） | 立即入 SkillCmd{confirm=true}                                        |
| Idle         | A 键 press                  | AttackAiming              | 进入普攻瞄准模式                                                     |
| Idle         | 右键 press + hover 敌       | AttackAiming              | 直接连敌（入 AttackCmd）                                             |
| Idle         | snapshot cast_state != None | CastLocked                | Sim 反向同步                                                         |
| SkillAiming  | 左键 press                  | CastLocked（待 Sim 确认） | 入 SkillCmd{confirm=true}                                            |
| SkillAiming  | 右键 press                  | Idle                      | 入 CancelCmd（仅 cancel skill，**不影响 MoveAxis**）                 |
| SkillAiming  | S / ESC / H press           | Idle                      | 入 CancelCmd                                                         |
| SkillAiming  | 其他技能键 press            | SkillAiming               | 切 ActiveSlot                                                        |
| SkillAiming  | snapshot cast_state != None | CastLocked                | 确认成功，Sim 进 Aiming→Chasing/Casting                              |
| SkillAiming  | snapshot cast_error > 0     | SkillAiming（保留！）     | 指向性 no target 报错，**保留施法模式**                              |
| AttackAiming | 左键 press + hover 敌       | Idle                      | 入 AttackCmd{target_id=hover}（MoveAxis 由 Sim 决定是否进 Moving）   |
| AttackAiming | 左键 press + 空地           | Idle                      | 入 AttackCmd{ground_pos}（找最近敌）                                 |
| AttackAiming | 右键 / S / ESC / H press    | Idle                      | 入 CancelCmd                                                         |
| AttackAiming | A 键 release（可选）        | AttackAiming              | A 键按住 vs 单击由设置决定，默认 **press 触发模式，需左键确认**      |
| CastLocked   | snapshot cast_state == None | Idle                      | Sim 反向同步（施法结束/打断/取消）                                   |
| CastLocked   | 右键 press                  | CastLocked                | 入 CancelCmd（Sim 判断阶段是否可打断，仅 Aiming/Chasing/Casting 可） |
| CastLocked   | S / H press                 | CastLocked                | 入 CancelCmd（同上）                                                 |
| CastLocked   | 技能键 press                | CastLocked                | 忽略（施法中不能换技能）                                             |
| CastLocked   | A 键 press                  | CastLocked                | 忽略（施法中不能普攻）                                               |

### 4.3 与 Sim 的反向同步

每个 `_process` 帧从 `last_snapshot.players[0]` 读取：

```gdscript
var snap_cast_state: int = p.cast_state   # 0=None 1=Aiming 2=Chasing 3=Casting 4=Channeling 5=Dashing
var snap_is_moving: bool = ...             # 新增 snapshot 字段 SimPlayerSnap.is_moving
var snap_attack_target: int = p.attack_target_id

# MoveAxis 同步
move_axis = MoveAxis.Moving if snap_is_moving else MoveAxis.NotMoving

# CommandAxis 同步
if snap_cast_state != 0:
    command_axis = CommandAxis.CastLocked
else:
    # Sim 侧 None 时，若 View 仍是 CastLocked → 解除
    if command_axis == CommandAxis.CastLocked:
        command_axis = CommandAxis.Idle
    # SkillAiming 保留（指向性 no target 报错时 Sim 状态仍可能为 None）
```

**关键**：`SkillAiming` 是 View 侧独立状态，Sim 侧可能仍是 `None`（因为 normal cast 的 Aiming 由 View 维护，直到左键确认才推 Sim 进 Aiming→Chasing/Casting）。需仔细区分两种 Aiming：

- **View Aiming（SkillAiming 状态）**：normal cast 等左键，**Sim 不知情**，sim_bridge 仅持续传 cast_slot + cast_aim。
- **Sim Aiming（snapshot cast_state==Aiming）**：quick cast 或 confirm 后的瞬时阶段，**几乎不可见**（下一 tick 就进 Chasing/Casting）。

**简化决策**：Sim 侧 `Aiming` 阶段**仅用于 quick cast 与 confirm 的同一 tick 内中转**，View 不应观察到 Sim 的 Aiming。Normal cast 的"等左键"完全由 View 维护。详见 §7。

---

## 5. Layer 3 — 命令翻译（Command Builder）

### 5.1 命令类型

```gdscript
# scripts/input/command.gd
class_name Command
extends RefCounted

enum CmdType { MOVE, SKILL, SKILL_UPGRADE, ATTACK, CANCEL, STOP }

var type: int
# MOVE
var move_target: Vector2
# SKILL / SKILL_UPGRADE
var skill_slot: int           # 0-3 = Q/W/E/R 技能槽
                              # 预留扩展：10-15 = 装备主动技能槽（P1-8 装备系统，详见 §5.4）
var skill_confirm: bool       # true=确认, false=仅设 Aiming（normal cast 持续传）
var skill_aim: Vector2
var skill_target_id: int      # hover 单位 NetworkId，-1=无
# ATTACK
var attack_target_id: int     # -1=ground
var attack_ground: Vector2
# CANCEL
var cancel_scope: int         # 0=skill, 1=attack, 2=all
```

### 5.2 翻译规则（按 FSM 状态 + 事件）

每帧 Layer 3 从 Layer 1 `pop_all()` 取全部事件，结合 Layer 2 当前状态，生成 0~N 条 Command 入 Layer 4。

| FSM 状态      | 事件                          | 生成的 Command                                                                                                                               |
| ------------- | ----------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| Idle          | 右键空地 press                | `MOVE{target=mouse_world}`                                                                                                                   |
| Moving        | 右键空地 press                | `MOVE{target=mouse_world}`（覆盖）                                                                                                           |
| Moving        | 右键空地 held（长按连点节流） | `MOVE{target=mouse_world}`（按 6Hz 节流）                                                                                                    |
| Idle / Moving | S press                       | `STOP`                                                                                                                                       |
| Idle / Moving | A press                       | 不发命令，**仅切 CommandAxis=AttackAiming**                                                                                                  |
| AttackAiming  | 左键 press + hover 敌         | `ATTACK{target_id=hover}` → 切 Idle                                                                                                          |
| AttackAiming  | 左键 press + 空地             | `ATTACK{ground=mouse_world}` → 切 Idle                                                                                                       |
| Idle / Moving | 技能键 press（quick cast）    | `SKILL{slot, confirm=true, aim, target_id}` → 切 CastLocked                                                                                  |
| Idle / Moving | 技能键 press（normal cast）   | `SKILL{slot, confirm=false, aim, target_id}` → 切 SkillAiming                                                                                |
| SkillAiming   | 技能键 press（其他槽）        | `SKILL{slot, confirm=false, aim, target_id}` → 切 SkillAiming（换槽）                                                                        |
| SkillAiming   | 左键 press                    | `SKILL{slot, confirm=true, aim, target_id}` → 切 CastLocked                                                                                  |
| SkillAiming   | 右键 / S / ESC / H press      | `CANCEL{scope=skill}` → 切 Idle（**MoveAxis 不变**）                                                                                         |
| SkillAiming   | 每帧持续                      | `SKILL{slot, confirm=false, aim=mouse_world, target_id=hover}`（维持 Sim 侧 Aiming 更新 aim）                                                |
| CastLocked    | 右键 / S / H press            | `CANCEL{scope=skill}`（Sim 自判是否可打断）                                                                                                  |
| AttackAiming  | 右键 / S / ESC / H press      | `CANCEL{scope=attack}` → 切 Idle                                                                                                             |
| Moving        | 右键点敌 press                | `ATTACK{target_id=hover}` → 切 AttackAiming？**否**：直接发 AttackCmd，CommandAxis 保持 Idle（右键点敌 = 立即攻击，无需左键确认，详见 §9.2） |
| Idle / Moving | Ctrl+技能键 press             | `SKILL_UPGRADE{slot}`（详见 §5.4）                                                                                                           |
| CastLocked    | Ctrl+技能键 press             | 忽略（施法中不能升级）                                                                                                                       |

### 5.3 Quick vs Normal Cast 配置

```gdscript
# scripts/input/cast_settings.gd（autoload 或并入 GameSettings）
enum CastMode { NORMAL, QUICK }
var skill_cast_mode: Array[int] = [NORMAL, NORMAL, NORMAL, NORMAL]  # per-slot
# 或全局：var global_cast_mode: CastMode = NORMAL
```

- **可按技能槽单独配置**（如 Q=quick, W=normal），玩家可在设置面板调。
- 默认全 NORMAL（保留指示器，新手友好）。
- Layer 3 翻译时按槽查表分支。

### 5.4 技能槽命名空间与扩展预留

`skill_slot` 字段采用分段命名空间，**为已规划但未实施的功能预留**，避免后期 retrofit：

| 段            | slot 值 | 用途                          | 状态     |
| ------------- | ------- | ----------------------------- | -------- |
| QWER 主动技能 | 0-3     | 玩家 4 技能槽（Q/W/E/R）      | 当前实施 |
| 装备主动技能  | 10-15   | 装备主动效果（P1-8 装备系统） | **预留** |

**普攻虚拟槽移除（已定）**：

- v1 在 `SkillComponent.Slots[5]` 中用 slot 4 作"普攻虚拟槽"（`SkillId=5, SkillKind::Attack`），由 `skill_cast` 处理。
- v2 **移除此虚拟槽**：普攻走独立 `ATTACK` 命令（§9），不占用 `skill_slot` 命名空间。
- `SkillComponent.Slots[5]` → `Slots[4]`（仅 QWER）。
- `skill_defs.h` 删除 id=5 Attack 行；`SkillKind::Attack` 枚举可移除（普攻不再走 skill_cast）。
- `skill_cast.h` 的 `cast_slot < 5` 判断改为 `< 4`，删除 Attack 分支（v1 的 157-182 行）。
- `world.cpp _spawn_player` 删除 slot 4 初始化。
- **未来 P1-8 装备槽**：`SkillComponent` 保持 `Slots[4]`，新增独立 `EquipmentSkillComponent`（6 槽，slot 10-15 映射）。`get_skill_def(id)` 的 SkillId 命名空间与 slot 解耦：QWER=1-4，装备主动=100+。Layer 3 翻译装备主动键时生成 `SKILL{slot=10+i, ...}`，Sim 侧 `skill_cast` 通过 slot 反查 `EquipmentSkillComponent.Slots[i]` → SkillId。
- **本方案不实现装备槽**，仅约定命名空间；实施 P1-8 时无需改 Layer 1-4 框架。

### 5.5 技能升级命令（SKILL_UPGRADE）

> **事实修正**：v1 代码中 `SkillPoints` 组件与 `SkillSlot::Level` 字段**均不存在**（`sim_system_reference.md` 的描述是设计意图，未落地）。本方案需新增。

`SkillPoints` 组件 + `SkillSlot::Level` 字段是 MOBA 核心玩法（升级加技能等级）的基础，v1 完全缺失输入路径。本方案补齐。

**新增组件/字段**：

```cpp
// components.h 新增
struct SkillPoints {
    int Available = 0;
};

// SkillSlot 加字段
struct SkillSlot {
    int SkillId = 0;
    int Level = 1;                 // ← 新增（当前默认 1 级）
    float CooldownTimer = 0.0f;
    float MaxCooldown = 0.0f;
    float ManaCost = 0.0f;
};
```

**命令**：

```gdscript
# command.gd
# SKILL_UPGRADE
var skill_slot: int   # 0-3，复用 SKILL 的 slot 字段
```

**触发**：`Ctrl + Q/W/E/R`（边沿 press）。选 Ctrl 修饰键避免与 cast 触发冲突（裸 Q = 施法，Ctrl+Q = 升级）。

**Layer 3 翻译**：

- 检测 `Ctrl held + 技能键 press` → 生成 `SKILL_UPGRADE{slot=i}`，**不进入 SkillAiming**，CommandAxis 保持当前状态。
- CastLocked 状态下忽略（施法中不能升级）。

**Sim 消费**（新增 `systems/skill_level.h`，独立 system 不并入 skill_cast）：

```cpp
// systems/skill_level.h
inline void skill_level_system(entt::registry &reg) {
    auto view = reg.view<PlayerTag, PlayerInputState, SkillComponent, SkillPoints>();
    for (auto e : view) {
        auto &tag = view.get<PlayerTag>(e);
        if (!tag.IsLocal) continue;
        auto &input = view.get<PlayerInputState>(e);
        auto &skills = view.get<SkillComponent>(e);
        auto &sp = view.get<SkillPoints>(e);
        if (input.SkillUpgradeSlot < 0 || input.SkillUpgradeSlot >= 4) continue;
        if (sp.Available <= 0) continue;
        auto &slot = skills.Slots[input.SkillUpgradeSlot];
        if (slot.SkillId <= 0 || slot.Level >= GameConfig::MaxSkillLevel) continue;
        slot.Level++;
        sp.Available--;
    }
}
```

**LocalInputSingleton 新增字段**：`int SkillUpgradeSlot = -1;`（脉冲）。

**SimServer API**：`void set_skill_upgrade_command(int slot);`

**Snapshot 修正**：

- `SimSkillSlotSnap.level` **语义修正**：v1 在 `snapshot_builder.cpp:33` 填的是 `char_level`（玩家等级）——这是 bug，View 若拿来当技能等级显示会出错。v2 改为填 `slot.Level`（技能等级）。
- `SimPlayerSnap.skill_points` 新增（§12.1 已加），= `SkillPoints.Available`，用于 BottomHUD 显示可分配点数提示。

**`_spawn_player` 补齐**：

```cpp
_reg.emplace<SkillPoints>(e, 0);   // 初始 0 点，升级时 progression 增加
// SkillSlot 初始化加 Level=1
for (int i = 0; i < 4; ++i) {
    sc.Slots[i].SkillId = ...;
    sc.Slots[i].Level = 1;          // ← 新增
    ...
}
```

**`progression_system` 配套**：玩家升级时 `SkillPoints.Available++`（当前 progression 不处理这个，需补；非本方案核心，可后续补但建议同期实施）。

**不在本方案范围**：升级数值曲线、升级时刷新 CD/Mana 的具体策略（由 `skill_defs.h` 后续扩展）。本方案仅打通"按键 → Sim 升级 → snapshot 回写"链路 + 修正 SimSkillSlotSnap.level 语义 bug。

---

## 6. Layer 4 — 命令缓冲与 Sim 消费

### 6.1 GDScript 端队列

```gdscript
# scripts/input/command_buffer.gd
class_name CommandBuffer
extends Node

var _q: Array[Command] = []

func push(cmd: Command) -> void
func pop_all() -> Array[Command]
func empty() -> bool
```

Layer 3 生成的命令 `push` 进来，sim_bridge 每 tick `pop_all` 全部消费。

### 6.2 sim_bridge 消费逻辑

```gdscript
func _physics_process(delta: float) -> void:
    elapsed += delta
    while elapsed >= TICK:
        var cmds := command_buffer.pop_all()
        # 按 type 合并/去重：
        #   - 同帧多条 MOVE → 只保留最后一条
        #   - 同帧多条 SKILL 同 slot → 只保留最后一条
        #   - SKILL confirm + 后续 SKILL no-confirm → 只保留 confirm
        #   - CANCEL → 保留
        #   - ATTACK → 保留最后一条
        #   - STOP → 保留
        var merged := merge_commands(cmds)
        for cmd in merged:
            sim.set_command(cmd)   # 统一入口，详见 §11
        sim.tick(TICK)
        ...
        elapsed -= TICK
```

### 6.3 不丢失 + 不重复

- **不丢失**：事件队列 + 命令缓冲跨 tick 持续。
- **不重复**：边沿事件只入队一次；持续状态（held keys）不生成重复命令，仅 mouse_world 更新由 SkillAiming 的"每帧持续传 aim"机制覆盖。
- **节流**：MOVE 长按连点 6Hz，在 Layer 3 翻译时按时间窗过滤，不入队。
- **去重**：合并规则在 §6.2，防止一帧内多次右键导致 Sim 重复 A*。

---

## 7. Sim 侧 CastState 重构（含 Chasing 跟随施法）

### 7.1 新的 Phase 枚举

```cpp
struct CastState {
    enum class Phase : uint8_t {
        None       = 0,
        Aiming     = 1,  // 仅 quick cast 同 tick 中转，或 normal cast confirm 后的瞬时阶段
        Chasing    = 2,  // 新：confirm 但超出范围，A* 追随目标
        Casting    = 3,  // 前摇
        Channeling = 4,  // 引导（R）
        Dashing    = 5,  // 位移（E）
    };
    Phase State = Phase::None;
    int ActiveSlot = -1;
    int SkillId = 0;
    float Timer = 0.0f;
    float SubTimer = 0.0f;
    Vec2 AimPos{0.0f};
    Vec2 DashStart{0.0f};
    Vec2 DashTarget{0.0f};
    int HitTargetId = -1;
    int CastError = 0;
    entt::entity TargetEntity = entt::null;   // 指向性技能锁定目标
    int TargetNetworkId = -1;
    bool QuickCast = false;                   // 标记本次施法来源
    float RejectTimer = 0.0f;
};
```

### 7.2 状态转移（Sim 权威）

```
Phase::None + 收到 SKILL{confirm=true}
  ├─ SkillKind=MeleeSingle 且 target 无效 → CastError=4，保持 None（View 显示 "No target"，SkillAiming 保留）
  ├─ CD > 0 → CastError=1，保持 None
  ├─ Mana 不足 → CastError=2，保持 None
  ├─ Stun → CastError=3，保持 None
  ├─ 范围内/无需目标 → 扣蓝、设 CD → Phase::Casting（Timer=CastTime）
  └─ 超出范围（MeleeSingle 有 target 但 dist>Range / 非指向性 dist>Range 且有 chase target）
       → 扣蓝、设 CD → Phase::Chasing（Timer=0，TargetEntity 锁定）

Phase::None + 收到 SKILL{confirm=false}
  → 不进 Aiming（Sim 侧不再维护 normal cast 的"等左键"阶段）
  → 仅更新 ActiveSlot / AimPos / TargetEntity（供下次 confirm 用）
  → 状态保持 None

Phase::Chasing + 每 tick（在 skill_cast 内处理，详见 §13 tick 顺序）
   ├─ 目标死亡 → refund + Phase::None + CastError=5
   ├─ 目标在范围内 → Phase::Casting（Timer=CastTime）
   ├─ 收到 CANCEL → refund + Phase::None
   ├─ 移动：本 tick #3 skill_cast 设 Chasing 后，#4 player_pathfinding 同 tick 用 A* 朝 TargetEntity/AimPos 寻路写 MovePath，#5 player_movement 同 tick 跟随（无 1 tick 延迟）
   └─ 非指向性技能：AimPos 每 tick 由 input.SkillAim 更新（追随鼠标）

Phase::Casting + Timer<=0 → 触发 effect → 按 SkillKind 转 Channeling/Dashing/None
Phase::Casting + CANCEL + 非 ChannelBurst → refund + None
Phase::Channeling → 不可打断（CANCEL 无效），Timer 到 → None
Phase::Dashing → 位移推进，到点/撞墙 → None
```

### 7.3 跟随施法（Chasing）的核心规则

| 技能类型                       | Chasing 进入条件                          | Chasing 期间移动 | 结束条件                                       |
| ------------------------------ | ----------------------------------------- | ---------------- | ---------------------------------------------- |
| MeleeSingle（指向性）          | confirm 时 target alive 但 `dist > Range` | A\* 朝 target    | 到 Range 内 → Casting；target 死 → refund+None |
| AoEField（非指向性，鼠标落点） | confirm 时 `dist(AimPos, pos) > Range`    | A\* 朝 AimPos    | 到 Range 内 → Casting                          |
| Dash（位移）                   | 不进 Chasing（dash 自身就是位移）         | —                | 直接 Casting → Dashing                         |
| ChannelBurst（自身周围）       | 无需 Chasing（无范围概念）                | —                | 直接 Casting → Channeling                      |

### 7.4 "No target 报错 + 保留施法模式"

用户明确要求：**指向性技能 confirm 时点不到人 → 报错 "No target" → 保留 SkillAiming 模式，不退出**。

实现：

- Sim 侧：`Phase::None + SKILL{confirm=true}` 验证失败 → `CastError=4`，状态保持 `None`。
- View 侧：检测到 `snapshot.cast_error == 4` 且 `cast_state == None` → **不解除 SkillAiming**（仅显示红色 "No target" 字样），允许玩家继续瞄 + 左键重试。
- View 侧：CastError 由 sim_bridge 检测 `prev_error != cur_error` 触发显示，避免重复弹。
- 解除 SkillAiming 仅由用户主动 cancel（右键 / S / ESC / H）或 confirm 成功（进 CastLocked）。

### 7.5 打断施法的统一处理

打断不再是"散落在各处的 flag"：

| Sim 阶段             | CANCEL 消费   | 行为                                      |
| -------------------- | ------------- | ----------------------------------------- |
| None                 | 忽略          | —                                         |
| Aiming（Sim 内瞬时） | refund + None | 几乎不发生                                |
| Chasing              | refund + None | **退蓝退 CD**（玩家主动取消，未触发效果） |
| Casting              | refund + None | **退蓝退 CD**（前摇打断，v2 默认宽容）    |
| Channeling           | **忽略**      | R 引导不可打断                            |
| Dashing              | **忽略**      | E 位移不可打断                            |

**refund 策略（已定）**：Chasing/Casting 取消都 **退蓝退 CD**（v2 默认，与 v1 的"前摇打断不退"不同——v1 是惩罚性设计，v2 改为宽容，符合现代 MOBA 趋势且原型阶段对新手友好）。**配置化保留口**：`GameConfig::RefundOnCastInterrupt = true` / `RefundOnChaseInterrupt = true`，后续可调为 false 启用惩罚。

### 7.6 input 层镜像的意义

View 的 `CastLocked` 状态直接由 `snapshot.cast_state != None` 决定，意味着：

- 玩家按 H/S/右键 → Layer 3 生成 `CANCEL` 命令 → Sim 消费 → 下一 snapshot `cast_state=None` → View 自动解除 `CastLocked`。
- **不存在"View 已 cancel 但 Sim 还在 Casting"的 desync**，因为 View 状态完全跟随 Sim。
- 唯一例外：`SkillAiming`（View 维护的 normal cast 等待期）Sim 状态为 None，cancel 此阶段时 View 自行切回 Idle，**不发 CANCEL**（Sim 无需知道）。

---

## 8. Quick Cast 与 Normal Cast 流程

### 8.1 Quick Cast（快施）

**触发**：技能键 **press**（边沿，非 held）。

```
玩家按 Q（quick cast 模式）
  ↓ Layer 1 入队 KEY_PRESS{Q}
  ↓ Layer 3 翻译
SKILL{slot=0, confirm=true, aim=mouse_world, target_id=hover}
  ↓ Layer 4 入队
  ↓ sim_bridge 下个 tick
sim.set_command(SKILL{slot=0, confirm=true, ...})
  ↓ Sim skill_cast_system
  ├─ 验证 CD/Mana/Target/Stun
  ├─ 失败 → CastError，State=None（View 显示错误，**View 状态回 Idle**）
  ├─ 范围内 → State=Casting
  └─ 超出范围 → State=Chasing（A* 追随）
  ↓ snapshot
View 看到 cast_state != None → CommandAxis = CastLocked
```

**关键**：Quick cast **无 indicator**（用户明确要求），从按键到施法无视觉过渡，仅施法条/读条在 Casting 阶段显示。

**No target 行为**：与 normal cast 一致 → 报错 + **View 回 Idle**（quick cast 无 Aiming 状态可保留）。详见 §8.3。

### 8.2 Normal Cast（手动施法）

**触发**：技能键 **press**（边沿）→ 进入 SkillAiming（绿线显示）→ 左键确认。

```
玩家按 Q（normal cast 模式）
  ↓ Layer 1 入队 KEY_PRESS{Q}
  ↓ Layer 3 翻译
SKILL{slot=0, confirm=false, aim=mouse_world, target_id=hover}
  ↓ Layer 4
  ↓ Sim
Sim 仅更新 ActiveSlot/AimPos/TargetEntity，State 保持 None
  ↓ snapshot（cast_state 仍 None）
View 切 CommandAxis = SkillAiming（由 Layer 3 在生成命令时同步切，不等 snapshot）

每帧 Layer 3 持续生成 SKILL{slot=0, confirm=false, aim=current_mouse, target_id=current_hover}
  ↓ 维持 Sim 侧 ActiveSlot/AimPos/TargetEntity 实时跟随鼠标

玩家左键
  ↓ Layer 1 入队 MB_PRESS{LEFT}
  ↓ Layer 3 翻译
SKILL{slot=0, confirm=true, aim=mouse_world, target_id=hover}
  ↓ Sim
  ├─ 验证 CD/Mana/Target/Stun
  ├─ 失败 → CastError（4=No target 时 View 保留 SkillAiming）
  ├─ 范围内 → State=Casting
  └─ 超出范围 → State=Chasing
  ↓ snapshot
View 看到 cast_state != None → CommandAxis = CastLocked
（若 CastError=4 且 State=None → View 保留 SkillAiming）
```

### 8.3 两种模式行为对照表

| 行为                    | Normal Cast                             | Quick Cast                                     |
| ----------------------- | --------------------------------------- | ---------------------------------------------- |
| 触发                    | press → aiming → left-click confirm     | press（直接 confirm）                          |
| Indicator（绿线）       | 有（SkillAiming 期间）                  | 无                                             |
| 鼠标实时跟随            | aiming 期间 aim 实时更新                | 仅 press 瞬时 aim                              |
| 超出范围                | confirm 后 → Sim Chasing → A\* 追       | 同                                             |
| 指向性 no target        | **保留 SkillAiming**，弹"No target"     | **回 Idle**，弹"No target"（无 Aiming 可保留） |
| 取消（aiming 阶段）     | 右键/S/ESC/H → 回 Idle（MoveAxis 不变） | 不存在 aiming 阶段                             |
| 取消（Chasing/Casting） | CANCEL → refund + None                  | 同                                             |
| Channeling/Dashing      | 不可打断                                | 同                                             |

**设计依据（用户原话）**：quickcast 的逻辑与 normalcast 中所有鼠标左键的行为一致（也需要超出范围用 A\* 追踪，指向性点不到人报错，保留移动状态）。Normal cast 的"保留 SkillAiming"是 quick cast 没有的额外行为，因 quick cast 没有显式 Aiming 状态。

---

## 9. 普攻命令模式（独立分支）

### 9.1 独立 FSM 分支的必要性

普攻与技能是**完全不同的命令流**：

- 技能：CD + Mana + CastTime + Effect 触发
- 普攻：AttackSpeed 节流 + homing 箭 + 自动追击 + **穿墙**

强行共用 FSM 会引入大量 if/else 与 bug。**独立模式最解耦**。

### 9.2 进入方式（两种）

| 触发               | 行为                                 | 是否需左键确认                |
| ------------------ | ------------------------------------ | ----------------------------- |
| **A 键 press**     | 进入 AttackAiming，等左键确认        | 是（normal-attack 模式）      |
| **右键点敌 press** | **直接锁敌攻击，无需左键**           | 否（MOBA 标准右键点敌即攻击） |
| 右键点空地 press   | 移动（MoveCmd）+ 清锁（AttackClear） | —                             |

**关键**：A 键模式 = 像 normal cast 一样的"瞄准+确认"，右键点敌 = 即时攻击（不进入 AttackAiming）。两者都生成 `ATTACK` 命令，仅 View 侧路径不同。

### 9.3 AttackAiming 子流程

```
玩家按 A
  ↓ Layer 1: KEY_PRESS{A}
  ↓ Layer 3: 切 CommandAxis = AttackAiming（不发命令）
  ↓ View 显示普攻 indicator（可选：射程圈）

玩家左键
  ├─ hover 敌 → Layer 3 生成 ATTACK{target_id=hover}
  └─ 空地     → Layer 3 生成 ATTACK{ground=mouse_world, target_id=-1}
  ↓ 切 CommandAxis = Idle

Sim player_attack_command_system 消费 ATTACK
  ├─ target_id>=0 → resolve → 设 AttackTarget{Target, TargetNetworkId}
  └─ ground        → find_nearest_enemy(AcquisitionRange) → 设 AttackTarget 或无效
  ↓
player_pathfinding_system: AttackTarget 有效且超 Range → A* 朝目标寻路
player_movement_system: 跟随 MovePath 移动
player_attack_fire_system: 到 Range 内 → 射 homing 箭
```

### 9.4 普攻穿墙

**用户明确**：普攻能穿墙，是 system 的逻辑。

| 层                   | 穿墙规则                                                           |
| -------------------- | ------------------------------------------------------------------ |
| `player_pathfinding` | 追击目标时 **不用 A\***（A\* 会绕墙），改用 **直线朝目标方向移动** |
| `player_movement`    | 追击中设 `AttackTarget.Chasing=true`                               |
| `wall_collision`     | Mover 分支跳过 `AttackTarget.Chasing==true` 的玩家（穿墙追击）     |
| `arrow_movement`     | homing 箭矢每 tick 修正速度朝目标当前位置                          |
| `wall_collision`     | Arrow 分支跳过有 `Homing` 组件的箭（穿墙飞行）                     |
| `combat`             | Homing 箭只检测锁定目标的碰撞（不误伤路过的敌人）                  |

**注意**：这与 §7 的技能 Chasing 不同——**技能 Chasing 用 A\* 寻路（绕墙）**，**普攻追击用直线（穿墙）**。这是用户明确的两套不同规则。

### 9.5 取消普攻瞄准

| 状态                        | 取消键                  | 行为                                                                                 |
| --------------------------- | ----------------------- | ------------------------------------------------------------------------------------ |
| AttackAiming                | 右键 / S / ESC / H      | 切 Idle，**不发命令**（Sim 侧无 AttackAiming 状态需清）                              |
| 已锁敌（AttackTarget 有效） | 右键空地 / S / 移动命令 | 发 `ATTACK{clear=true}` 或 Sim 在 player_attack_command 检测 MoveIssue/Stop 自动清锁 |

### 9.6 普攻与施法的互斥

- `player_attack_fire` 与 `skill_cast` 都检查 `CastState != None` 互斥（已有）。
- AttackAiming 期间按技能键 → Layer 3 切 SkillAiming（**AttackAiming 自动取消**，不发 ATTACK）。
- SkillAiming 期间按 A → Layer 3 生成 `CANCEL{scope=skill}` + 切 AttackAiming。

---

## 10. 移动与寻路（A\* 追击/跟随）

### 10.1 三种移动来源

| 来源           | 触发                           | 寻路方式         | 穿墙 |
| -------------- | ------------------------------ | ---------------- | ---- |
| 玩家右键点地板 | `MOVE` 命令                    | A\*（绕墙）      | 否   |
| 技能跟随施法   | Sim `Chasing` 阶段             | A\*（绕墙）      | 否   |
| 普攻追击       | `AttackTarget` 有效 + 超 Range | **直线**（穿墙） | 是   |

### 10.2 player_pathfinding_system 职责扩展

每 tick 顺序：

1. **Sim Chasing 阶段**：若 `CastState.State == Chasing` → 用 A\* 朝 `CastState.TargetEntity`（MeleeSingle）或 `CastState.AimPos`（AoEField）寻路，写 `MovePath`。**优先级最高**（玩家已 confirm 施法，自动追随）。
2. **普攻追击**：若 `AttackTarget.Target` 有效且超 Range → **不走 A\***，由 `player_movement` 直线移动 + 设 `Chasing=true` 穿墙。`player_pathfinding` **不处理此分支**。
3. **玩家右键移动**：`input.MoveIssue == true` 且 `CastState == None` 且 `AttackTarget.Target == null` → A\* 朝 `input.MoveTarget` 寻路。

### 10.3 player_movement_system 优先级

```
每 tick 顺序：
1. 重置 AttackTarget.Chasing = false
2. Status gate (Root/Stun) → continue
3. CastState gate (Casting/Channeling/Dashing) → continue
   （Chasing 不 gate，允许移动）
4. Stop 命令 → 清 MovePath, continue
5. 技能 Chasing: MovePath.Following → 跟随（A* 路径）
6. 普攻追击: AttackTarget 有效 + 超 Range → 直线移动 + Chasing=true, continue
7. 玩家右键移动: MovePath.Following → 跟随（A* 路径）
```

### 10.4 转向速率

所有 path-following 分支用 `PathTurnRate` 平滑朝向（已有）。直线追击分支同样平滑。**仅 quick cast 瞬时转向不平滑**（dash 由 Sim 直接设朝向）。

### 10.5 寻路死区与节流

- `RepathTargetDeadzone`（已有，1.5 单位）：右键连点同区域不重算 A\*。
- Chasing 期间目标移动 → 每 tick 检测目标位移 > 死区才 repath。
- View 端右键长按连点节流 `MOVE_REPEAT_INTERVAL = 0.167s`（6Hz）。

---

## 11. SimServer API（统一命令接口）

### 11.1 单一统一入口（取代散落的 set_*_input）

```cpp
// sim_server.h
enum class CmdType : uint8_t {
    Move = 0, Skill = 1, SkillUpgrade = 2, Attack = 3, Cancel = 4, Stop = 5,
};

// 单一入口，View 传 Command 对象
void set_command(int type,
                 int slot,              // Skill/SkillUpgrade: 0-3 (装备槽 10-15 预留, 见 §5.4)
                 int target_id,         // Skill/Attack: NetworkId, -1=none
                 bool confirm,          // Skill
                 bool ground_attack,    // Attack: true=空地找最近
                 float aim_x, float aim_y,        // Skill/Move/Attack ground
                 bool cancel_skill,     // Cancel
                 bool cancel_attack,
                 int seq);
```

**或更优**：暴露多个细粒度方法，sim_bridge 按 Command.type 路由：

```cpp
void set_move_command(float tx, float ty, bool issue);
void set_skill_command(int slot, bool confirm, float ax, float ay, int target_id);
void set_skill_upgrade_command(int slot);                              // ← 新增
void set_attack_command(int target_id, bool ground, float gx, float gy, bool clear);
void set_cancel_command(bool skill, bool attack);
void set_stop_command();
```

**推荐细粒度方案**：API 自描述，CommandBuffer 合并后直接调对应方法，无需 packed struct 解码。

### 11.2 废弃的旧 API

| 旧 API                                                                | 替代                                                           |
| --------------------------------------------------------------------- | -------------------------------------------------------------- |
| `set_local_input(move, aim, fire, seq)`                               | `set_move_command` + `set_skill_command`（aim 走 skill）       |
| `set_cast_input(slot, confirm, cancel, interrupt, ax, ay, target_id)` | `set_skill_command` + `set_cancel_command`                     |
| `set_attack_command(target_id)`（单参数版）                           | `set_attack_command(target_id, ground, gx, gy, clear)`（扩展） |
| `set_stop(stop)`                                                      | `set_stop_command()`                                           |

`fire` 字段废弃（普攻不再朝鼠标射箭，改锁敌 homing）。`PlayerInputState.Fire` 字段移除。

### 11.3 LocalInputSingleton 重构

```cpp
struct LocalInputSingleton {
    // ── 移动 ──
    Vec2 MoveTarget{0.0f};
    bool MoveIssue = false;
    bool Stop = false;

    // ── 技能 ──
    int  SkillSlot = -1;        // 当前 Aiming 槽，-1=无（0-3 QWER, 10-15 装备预留）
    bool SkillConfirm = false;  // 本 tick 是否确认
    Vec2 SkillAim{0.0f};
    int  SkillTargetId = -1;
    int  SkillUpgradeSlot = -1; // 技能升级脉冲（Ctrl+QWER），-1=无

    // ── 施法取消 ──
    bool CancelSkill = false;
    bool CancelAttack = false;

    // ── 普攻 ──
    int  AttackTargetId = -1;
    bool AttackGround = false;
    Vec2 AttackGroundPos{0.0f};
    bool AttackClear = false;

    // ── 序号 ──
    int Seq = 0;
};
```

`Move` / `Aim` / `Fire` / `CastInterrupt` 字段移除（用命令式替代）。`PlayerInputState` 同步重构。

### 11.4 World 消费命令

```cpp
void World::set_skill_command(int slot, bool confirm, float ax, float ay, int target_id) {
    auto &li = _reg.get<LocalInputSingleton>(_local_input_entity);
    li.SkillSlot = slot;
    li.SkillConfirm = confirm;
    li.SkillAim = {ax, ay};
    li.SkillTargetId = target_id;
}
// ... 其他同理
```

每 tick 起始 `local_input_injection_system` 复制到 `PlayerInputState`，tick 末清脉冲字段（SkillConfirm / SkillUpgradeSlot / MoveIssue / Stop / CancelSkill / CancelAttack / AttackGround / AttackClear）。

---

## 12. Snapshot 扩展（状态回写同步）

### 12.1 SimPlayerSnap 新增字段

| 字段               | 类型  | 来源                             | 用途                                                       |
| ------------------ | ----- | -------------------------------- | ---------------------------------------------------------- |
| `cast_state`       | int   | CastState.Phase                  | 0=None 1=Aiming 2=Chasing 3=Casting 4=Channeling 5=Dashing |
| `cast_slot`        | int   | CastState.ActiveSlot             | View 显示哪个槽                                            |
| `cast_progress`    | float | Timer / max                      | 进度条                                                     |
| `cast_aim_x/y`     | float | CastState.AimPos                 | VFX 位置                                                   |
| `cast_target_id`   | int   | CastState.TargetNetworkId        | View 高亮跟随目标                                          |
| `cast_error`       | int   | CastState.CastError              | 错误弹窗                                                   |
| `dash_sx/sy/tx/ty` | float | DashStart/Target                 | dash 路径 VFX                                              |
| `hit_target_id`    | int   | HitTargetId                      | C 命中 VFX                                                 |
| `attack_target_id` | int   | AttackTarget.TargetNetworkId     | 红色锁定指示器                                             |
| `skill_points`     | int   | SkillPoints.Available            | **新增**：View 显示可分配点数 / 升级提示（P0-2 配套）      |
| `is_moving`        | bool  | 玩家是否正在自主移动（见下注意） | **新增**：View MoveAxis 反向同步                           |

**`is_moving` 字段来源注意**：不可简单取 `MovePath.Following`——普攻追击（§9.4）是直线移动**不走 MovePath**，技能 Chasing 期间 MovePath 由 pathfinding 切换更新。正确来源应在 `player_movement_system` 末尾（或 snapshot_export）按"本 tick 是否实际推进了位置"判定：

```cpp
// snapshot_builder.cpp _build_players 伪代码
s->is_moving = (path.Following)                      // A* 寻路跟随中
    || (at.Chasing)                                   // 普攻直线追击中
    || (cs.State == CastState::Phase::Chasing);       // 技能跟随施法中
// 注：Casting/Channeling/Dashing 不算 is_moving（站定/位移由 dash 自管）
```

### 12.2 View 同步流程

```gdscript
# input_state_machine.gd
func sync_from_snapshot(p: SimPlayerSnap) -> void:
    # MoveAxis
    move_axis = MoveAxis.Moving if p.is_moving else MoveAxis.NotMoving

    # CommandAxis
    if p.cast_state != 0:   # != None
        command_axis = CommandAxis.CastLocked
    else:
        if command_axis == CommandAxis.CastLocked:
            command_axis = CommandAxis.Idle
        # SkillAiming 由 View 自维护，不解除（除非收到 cancel 事件）
```

### 12.3 错误显示

```gdscript
# sim_bridge.gd
if prev_cast_error != p.cast_error and p.cast_error > 0:
    cast_error_layer.show_error(p.cast_error)
prev_cast_error = p.cast_error
```

错误码：1=On Cooldown, 2=Not enough Mana, 3=Stunned, 4=No target, 5=Target unavailable（confirm 后目标死亡）。

---

## 13. tick 顺序

```
local_input_injection        #  1. 注入命令到 PlayerInputState
player_attack_command        #  2. 处理 ATTACK 命令 + pending + 目标验证 + 清锁
skill_cast                   #  3. 施法状态机（None→Aiming/Chasing/Casting；Casting timer 推进+effect；Channeling；Dashing）
player_pathfinding           #  4. A* 寻路（右键移动 + 技能 Chasing 跟随）—— 同 tick 看到 #3 设的 Chasing
player_movement              #  5. 移动（Chasing 用 MovePath 跟随；AttackTarget 直线追击 + 设 Chasing 标志；Casting/Channeling/Dashing gate）
player_attack_fire           #  6. 锁定目标射 homing 箱
bot_targeting                #  7
bot_ai                       #  8
bot_combat                   #  9
arrow_movement               # 10. +Homing 追踪
wall_collision               # 11. 跳过 AttackTarget.Chasing 玩家 + Dashing 玩家 + Homing 箭
combat                       # 12. Homing 只命中锁定目标
pickup                       # 13
aoe                          # 14
status_effect                # 15
mana_regen                   # 16
skill_cooldown               # 17
skill_level                  # 18. 消费 SkillUpgradeSlot → SkillPoints-- + slot.Level++（新增，§5.5）
progression                  # 19
snapshot_export              # 20
```

**顺序要点（无 1 tick 延迟设计）**：

- `skill_cast`(#3) 必须在 `player_pathfinding`(#4) 与 `player_movement`(#5) **之前**：confirm 本 tick 在 #3 设 `State=Chasing` + `TargetEntity`/`AimPos`，#4 同 tick 看到 Chasing → 调 `nav.find_path` 写 MovePath，#5 同 tick 跟随 MovePath 移动。**confirm → A\* → 移动全在同 tick 完成，无 33ms 延迟**。
- `player_movement`(#5) 在 `skill_cast`(#3) 之后：读 CastState 是本 tick 最新值（Casting/Channeling/Dashing 即时 gate；effect 触发后 State=None 即时解除 gate）。
- `player_attack_command`(#2) 在 `skill_cast`(#3) 之前：先处理 AttackCmd 设/清 AttackTarget，再由 skill_cast 决定施法（施法中 AttackCmd 走 pending，下 tick CastState=None 时消费）。
- `player_attack_fire`(#6) 在 `skill_cast`(#3) 之后：读 CastState!=None 即时 skip 普攻。
- `wall_collision`(#11) 在 `player_movement`(#5) 之后：读 `AttackTarget.Chasing` 标志跳过穿墙追击，读 `CastState::Dashing` 跳过 dash 位移。
- `combat`(#12) 在 `arrow_movement`(#10) 之后：Homing 箭已追踪到目标附近。
- `skill_level`(#18) 独立于 skill_cast，放 skill_cooldown 附近；不参与状态机，仅消费 SkillUpgradeSlot 脉冲。

---

## 14. 组件变更清单

### 14.1 新增

```cpp
struct Homing {
    entt::entity Target = entt::null;
    int TargetNetId = -1;
};

struct SkillPoints {        // ← 新增（v1 缺失，sim_system_reference.md 描述但未落地）
    int Available = 0;
};
```

### 14.2 修改

```cpp
struct CastState {
    enum class Phase : uint8_t {
        None = 0, Aiming = 1, Chasing = 2,  // ← 新增 Chasing
        Casting = 3, Channeling = 4, Dashing = 5,
    };
    // ... 新增 TargetNetworkId, QuickCast
};

struct AttackTarget {
    entt::entity Target = entt::null;
    int TargetNetworkId = -1;
    bool Chasing = false;   // 每 tick 由 player_movement 设置，wall_collision 读取
};

struct SkillSlot {
    int SkillId = 0;
    int Level = 1;                  // ← 新增字段（v1 缺失）
    float CooldownTimer = 0.0f;
    float MaxCooldown = 0.0f;
    float ManaCost = 0.0f;
};

struct SkillComponent {
    SkillSlot Slots[4];             // ← Slots[5] → Slots[4]（移除普攻虚拟槽）
};

struct LocalInputSingleton {
    // 完全重构为命令式，见 §11.3（含 SkillUpgradeSlot）
};
// PlayerInputState 同步重构（含 SkillUpgradeSlot）
```

### 14.3 移除

- `PlayerInputState.Move / Aim / Fire / CastInterrupt / CastSlot / CastConfirm / CastCancel / CastAim / CastTargetId` → 全部移除，由命令字段替代。
- `LocalInputSingleton` 同上。
- `SkillComponent.Slots[4]`（普攻虚拟槽，`SkillId=5`）→ 移除，`Slots[5]` 缩为 `Slots[4]`。
- `skill_defs.h` id=5 Attack 行 → 移除。
- `SkillKind::Attack` 枚举 → 移除（普攻不再走 skill_cast）。
- `game_config.h` `SkillCooldowns[4]` / `SkillManaCosts[4]` / `SkillCount` → 移除（统一读 `skill_defs.h`）。
- `ArrowTag.LifestealRatio` 保留（F 大招吸血仍需要）。

---

## 15. 文件改动清单

### 15.1 GDScript 新增

| 文件                                   | 职责                                  |
| -------------------------------------- | ------------------------------------- |
| `scripts/input/input_event_queue.gd`   | Layer 1：原始事件队列 + 持续状态采样  |
| `scripts/input/input_state_machine.gd` | Layer 2：双轴 FSM + snapshot 反向同步 |
| `scripts/input/command.gd`             | Command 数据类                        |
| `scripts/input/command_builder.gd`     | Layer 3：FSM + 事件 → Command         |
| `scripts/input/command_buffer.gd`      | Layer 4：跨 tick FIFO                 |
| `scripts/input/cast_settings.gd`       | Quick / Normal cast 偏好（per-slot）  |

### 15.2 GDScript 修改

| 文件                                | 改动                                                                                                                                                   |
| ----------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `scripts/input/input_collector.gd`  | **完全重写**为 input_event_queue + input_state_machine + command_builder + command_buffer 的组合入口（或拆分为上述多文件，原 input_collector.gd 删除） |
| `scripts/sim_bridge.gd`             | 改为从 command_buffer pop 命令 → 调 `set_*_command`；移除 set_local_input/set_cast_input/set_attack_command 旧调用                                     |
| `scripts/autoload/game_settings.gd` | 移除 move_mode/MoveMode/mode_changed（已废弃），保留 camera/fullscreen 配置                                                                            |
| `scripts/ui/settings_panel.gd/tscn` | 移除模式切换 OptionButton；新增 quick/normal cast 偏好设置                                                                                             |
| `scripts/ui/bottom_hud.gd`          | 移除 KEY_HINTS 按模式切换逻辑，固定 QWER + A 普攻                                                                                                      |
| `scripts/view/skill_vfx.gd`         | 绿线仅在 SkillAiming 显示（normal cast）；Chasing 期间可显示"路径线"或保持绿线                                                                         |
| `scripts/view/entity_view.gd`       | attack_targeted 红色指示器（已有，保留）                                                                                                               |

### 15.3 C++ 修改

| 文件                              | 改动                                                                                                                                                                                                                                                                                                     |
| --------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `components.h`                    | CastState +Chasing/TargetNetworkId/QuickCast；AttackTarget +Chasing；**新增 `SkillPoints` 组件**；`SkillSlot` +`Level` 字段；`SkillComponent.Slots[5]`→`Slots[4]`（移除普攻虚拟槽）；LocalInputSingleton/PlayerInputState 完全重构（含 SkillUpgradeSlot）；移除 Move/Aim/Fire/CastInterrupt；新增 Homing |
| `game_config.h`                   | +RefundOnCastInterrupt(=true) / RefundOnChaseInterrupt(=true) / SkillChaseRepathDeadzone / MaxSkillLevel(=4)；移除 SkillCooldowns/SkillManaCosts/SkillCount（统一读 skill_defs.h）                                                                                                                       |
| `skill_defs.h`                    | 删 id=5 Attack 行；移除 `SkillKind::Attack` 枚举                                                                                                                                                                                                                                                         |
| `systems/local_input_injection.h` | 复制新命令字段（含 SkillUpgradeSlot）                                                                                                                                                                                                                                                                    |
| `systems/player_pathfinding.h`    | +技能 Chasing 分支（A\* 朝 TargetEntity/AimPos）；**移除现有 AttackTarget A\* 追击分支**（v1 的 50-82 行，普攻改直线穿墙交给 player_movement）                                                                                                                                                           |
| `systems/player_movement.h`       | +Chasing 阶段移动（用 MovePath）；+AttackTarget 直线追击 + 设 Chasing=true；移除 WASD 分支；**末尾写 is_moving 标志供 snapshot**                                                                                                                                                                         |
| `systems/player_attack_command.h` | 重构为消费 AttackCmd；处理 clear / ground / target_id                                                                                                                                                                                                                                                    |
| `systems/player_attack_fire.h`    | 保留（homing 箭逻辑）                                                                                                                                                                                                                                                                                    |
| `systems/skill_cast.h`            | **内加 Chasing 分支**（不拆分独立 system，无 1 tick 延迟）：None+confirm 超范围→Chasing；Chasing+目标进范围→Casting；Chasing+目标死→refund+None；Chasing+CANCEL→refund+None；`cast_slot<5`→`<4`；删 Attack 分支（v1 157-182 行）；refund 读 `RefundOnCastInterrupt`/`RefundOnChaseInterrupt` 配置        |
| `systems/skill_level.h`           | **新增**：消费 SkillUpgradeSlot → SkillPoints.Available-- + slot.Level++（§5.5）                                                                                                                                                                                                                         |
| `systems/wall_collision.h`        | +跳过 AttackTarget.Chasing 玩家（穿墙追击）+ 跳过 Homing 箭；保留现有跳过 Dashing                                                                                                                                                                                                                        |
| `systems/combat.h`                | +Homing 只命中锁定目标                                                                                                                                                                                                                                                                                   |
| `systems/arrow_movement.h`        | +Homing 追踪                                                                                                                                                                                                                                                                                             |
| `arrow_spawner.h`                 | ArrowSpawnContext +homing 字段                                                                                                                                                                                                                                                                           |
| `world.h/.cpp`                    | tick 顺序（§13：skill_cast 提前到 player_pathfinding 之前；插 skill_level_system）；set_*_command 实现（含 set_skill_upgrade_command）；_spawn_player emplace `SkillPoints{0}` + slot.Level=1 + 移除 slot 4 初始化；移除 set_local_input/set_cast_input/set_skill_input                                  |
| `sim_server.h/.cpp`               | +set_move_command / set_skill_command / set_skill_upgrade_command / set_attack_command / set_cancel_command / set_stop_command；移除旧 API                                                                                                                                                               |
| `register_types.cpp`              | 绑定新 API                                                                                                                                                                                                                                                                                               |
| `snapshot_types.h`                | SimPlayerSnap +is_moving/cast_target_id/skill_points                                                                                                                                                                                                                                                     |
| `snapshot_bindings.cpp`           | 注册新字段                                                                                                                                                                                                                                                                                               |
| `snapshot_builder.cpp`            | 填新字段（is_moving 来源见 §12.1 注意）；**修正 `SimSkillSlotSnap.level` 语义 bug**：`char_level` → `slot.Level`                                                                                                                                                                                         |

### 15.4 C++ 删除

| 文件                                | 原因                                     |
| ----------------------------------- | ---------------------------------------- |
| `src_cpp/sim/systems/skill_input.h` | 已被 skill_cast 取代（若仍存在）         |
| `src_cpp/sim/systems/player_fire.h` | 已被 player_attack_fire 取代（若仍存在） |

### 15.5 Scene

| 文件                            | 改动                                                                                                                                |
| ------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| `scenes/main.tscn`              | 拆分 InputCollector 节点为 InputEventQueue / InputStateMachine / CommandBuilder / CommandBuffer 子节点（或保持单节点 + 多脚本组合） |
| `scenes/ui/settings_panel.tscn` | 移除模式 OptionButton，新增 cast mode 偏好                                                                                          |

---

## 16. 实施阶段

### 阶段 A — Layer 1+2+3+4 框架（View 侧，无 Sim 改动）

| 步骤 | 文件                     | 说明                                                           |
| ---- | ------------------------ | -------------------------------------------------------------- |
| A1   | `input_event_queue.gd`   | 事件队列 + 持续状态采样                                        |
| A2   | `command.gd`             | Command 数据类                                                 |
| A3   | `input_state_machine.gd` | 双轴 FSM（仅 Idle/Moving/SkillAiming/AttackAiming/CastLocked） |
| A4   | `command_builder.gd`     | FSM + 事件 → Command（quick/normal cast 分支）                 |
| A5   | `command_buffer.gd`      | FIFO 队列                                                      |
| A6   | `cast_settings.gd`       | per-slot 偏好                                                  |
| A7   | `sim_bridge.gd` 临时适配 | command_buffer pop → 仍调旧 set_*_input（兼容）                |

**验收**：编译通过，事件队列不丢指令（按 Q 100 次 → Sim 收到 100 条 skill 命令）。

### 阶段 B — Sim 命令 API 重构

| 步骤 | 文件                                                  | 说明                                                             |
| ---- | ----------------------------------------------------- | ---------------------------------------------------------------- |
| B1   | `components.h`                                        | LocalInputSingleton/PlayerInputState 重构（含 SkillUpgradeSlot） |
| B2   | `sim_server.h/.cpp` + `world.h/.cpp`                  | set_*_command 系列（含 set_skill_upgrade_command）               |
| B3   | `local_input_injection.h`                             | 复制新字段                                                       |
| B4   | `sim_bridge.gd`                                       | 切换到 set_*_command                                             |
| B5   | 移除旧 set_local_input/set_cast_input/set_skill_input | —                                                                |

**验收**：编译通过，旧 input_collector 删除，新链路工作。

### 阶段 C — Sim CastState + Chasing（skill_cast 内加分支，无 1 tick 延迟）

| 步骤 | 文件                                | 说明                                                                                                                                                                                                                                |
| ---- | ----------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| C1   | `components.h` CastState            | +Chasing/TargetNetworkId/QuickCast                                                                                                                                                                                                  |
| C2   | `skill_cast.h`                      | **内加 Chasing 分支**（不拆分独立 system）：None+confirm 超范围→Chasing；Chasing+进范围→Casting；Chasing+目标死→refund+None；Chasing+CANCEL→refund+None；refund 读 `RefundOnCastInterrupt`/`RefundOnChaseInterrupt` 配置（均=true） |
| C3   | `world.cpp`                         | **tick 顺序调整**：`skill_cast` 提前到 `player_pathfinding` 之前（§13），保证 confirm 同 tick 算 A\* + 移动                                                                                                                         |
| C4   | `player_pathfinding.h`              | +Chasing 分支 A\* 寻路（同 tick 看到 skill_cast 设的 Chasing）；移除现有 AttackTarget A\* 追击分支                                                                                                                                  |
| C5   | `player_movement.h`                 | +Chasing 移动（用 MovePath）；移除 WASD 分支；末尾写 is_moving                                                                                                                                                                      |
| C6   | `snapshot_types.h/bindings/builder` | +cast_state(含 Chasing)/cast_target_id/is_moving/skill_points                                                                                                                                                                       |

**验收（关键：无 1 tick 延迟）**：

- Q（MeleeSingle）确认超 Range → **同 tick** Sim 进 Chasing + player_pathfinding 算 A* + player_movement 跟随 → 后续 tick 到 Range → Casting → effect
- Chasing 中按右键 → refund（退蓝退 CD）+ None
- Chasing 中目标死亡 → refund + None + CastError=5

### 阶段 D — Quick Cast / Normal Cast 双模式

| 步骤 | 文件                                             | 说明                                            |
| ---- | ------------------------------------------------ | ----------------------------------------------- |
| D1   | `command_builder.gd`                             | 按 cast_settings 分支                           |
| D2   | `skill_vfx.gd`                                   | 绿线仅 normal cast SkillAiming 期间显示         |
| D3   | `cast_settings.gd` + settings_panel              | 玩家可切换 per-slot                             |
| D4   | No target 报错 + 保留 SkillAiming（normal cast） | sim_bridge 检测 cast_error=4 不解除 SkillAiming |

**验收**：

- Normal cast Q → 绿线 → 左键 → 范围内 → Casting → 命中
- Normal cast Q → 绿线 → 左键 → 超 Range → Chasing → 追到 → Casting
- Normal cast Q → 绿线 → 左键空地（MeleeSingle）→ "No target" 红字，**绿线保留**
- Quick cast Q → 无绿线 → 直接 Casting
- Quick cast Q 超 Range → Chasing → 追到 → Casting

### 阶段 E — 普攻命令模式

| 步骤 | 文件                                               | 说明                                           |
| ---- | -------------------------------------------------- | ---------------------------------------------- |
| E1   | `components.h` AttackTarget +Chasing；+Homing 组件 | —                                              |
| E2   | `player_attack_command.h`                          | 消费 ATTACK 命令（target/ground/clear）        |
| E3   | `player_pathfinding.h`                             | 普攻追击**不走 A\***，由 player_movement 直线  |
| E4   | `player_movement.h`                                | +普攻追击分支 + 设 Chasing=true                |
| E5   | `player_attack_fire.h`                             | homing 箭（已有，保留）                        |
| E6   | `wall_collision.h`                                 | 跳过 Chasing 玩家 + Homing 箭                  |
| E7   | `combat.h` + `arrow_movement.h`                    | Homing 命中 + 追踪                             |
| E8   | `command_builder.gd`                               | A 键 → AttackAiming；右键点敌 → 直接 AttackCmd |
| E9   | `entity_view.gd`                                   | 红色锁定指示器（已有，保留）                   |

**验收**：

- A → 左键点 bot → 锁定 → 直线穿墙追击 → 到 Range → homing 箭命中
- 右键点 bot → 直接锁定（无 A 键）→ 同上
- 右键空地 → 移动 + 清锁
- A → 右键 → 取消 AttackAiming

### 阶段 E2 — 技能升级链路（SKILL_UPGRADE）

> 独立小阶段，不依赖 D/E，可在 B 后任意时机插入。补齐 v1 完全缺失的 SkillPoints 输入路径 + SimSkillSlotSnap.level 语义 bug。

| 步骤 | 文件                                       | 说明                                                                                                                            |
| ---- | ------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------- |
| E2-1 | `components.h`                             | **新增 `SkillPoints` 组件**；`SkillSlot` +`Level` 字段；LocalInputSingleton/PlayerInputState +SkillUpgradeSlot（B1 已加则跳过） |
| E2-2 | `game_config.h`                            | +MaxSkillLevel(=4)                                                                                                              |
| E2-3 | `systems/skill_level.h` **新增**           | 消费 SkillUpgradeSlot：验证 SkillPoints.Available>0 && slot.Level<Max → Level++ / Available--                                   |
| E2-4 | `world.cpp`                                | tick 顺序插 `skill_level_system`（§13 #18，skill_cooldown 之后）；`_spawn_player` emplace `SkillPoints{0}` + slot.Level=1       |
| E2-5 | `progression.h`                            | 玩家升级时 `SkillPoints.Available++`（当前 progression 不处理，需补）                                                           |
| E2-6 | `sim_server.h/.cpp` + `register_types.cpp` | +set_skill_upgrade_command 绑定                                                                                                 |
| E2-7 | `command_builder.gd`                       | Ctrl+Q/W/E/R press → SKILL_UPGRADE{slot}                                                                                        |
| E2-8 | `snapshot_types.h/bindings/builder`        | SimPlayerSnap +skill_points；**修正 `SimSkillSlotSnap.level` 语义 bug**：`char_level` → `slot.Level`                            |
| E2-9 | `bottom_hud.gd`                            | 显示可分配点数提示（skill_points>0 时高亮技能槽）                                                                               |

**验收**：

- 升级后击杀 bot 拿 XP → level up → SkillPoints.Available++ → Ctrl+Q → Q 技能 Level++ → snapshot 回写 → HUD 显示
- 无点数时 Ctrl+Q 忽略
- 满级时 Ctrl+Q 忽略
- 施法中 Ctrl+Q 忽略（CommandAxis=CastLocked）

### 阶段 F — 打磨与回归

| #   | 项目                | 方法                                                    |
| --- | ------------------- | ------------------------------------------------------- |
| 1   | Refund 策略         | 试 Casting 不退蓝 vs 退蓝，选手感                       |
| 2   | Chasing repath 死区 | 防目标抖动导致每 tick repath                            |
| 3   | A 键 hold vs press  | 按 A 持续 vs 单击行为差异                               |
| 4   | ESC 行为            | 施法中 ESC = cancel；非施法 ESC = 设置面板              |
| 5   | S 键语义            | Stop（停止移动）+ Cancel（取消施法/普攻瞄准）的双重角色 |
| 6   | 跨 tick 命令合并    | 同帧多条 MOVE/SKILL 合并正确                            |
| 7   | 失焦恢复            | 窗口失焦后 held_keys 校准                               |

---

## 17. 边界情况与陷阱

### 17.1 跨 tick 命令丢失

- **场景**：60Hz 渲染帧内按 Q + 左键（同帧），Sim tick 30Hz，下一 tick 才跑。
- **风险**：若 Q press 与 left-click press 同帧入队 → Layer 3 生成两条 SKILL（confirm=false + confirm=true）→ Layer 4 合并 → Sim 收到 confirm=true 一条 → 正常。
- **风险**：若 left-click 在 Q press 的下一渲染帧（同一 Sim tick 内）→ 同上合并 → 正常。
- **风险**：若 left-click 在 Q press 的下一 Sim tick 之后 → 两条 SKILL 分属不同 tick → Sim 第一 tick 收到 confirm=false（进 SkillAiming 但 Sim 不维护）→ 第二 tick 收到 confirm=true → 正常。
- **保证**：CommandBuffer 跨 tick 持续，不丢失。

### 17.2 Sim Aiming 阶段的瞬时性

- v2 中 Sim 的 `Aiming` 仅用于 quick cast 同 tick 中转（confirm 后立即转 Chasing/Casting）。
- **Normal cast 的"等左键"由 View 维护**，Sim 状态保持 None。
- 这避免了 v1 中"Sim Aiming 与 View Aiming 双源真值"的 desync bug。

### 17.3 No target 报错后的状态保留

- View SkillAiming 期间收到 `cast_error=4` 且 `cast_state=None` → **不解除 SkillAiming**。
- 实现要点：sim_bridge 检测到该条件时**不触发"CastLocked → Idle"的转移**，仅显示错误。
- 玩家继续左键重试 → 新 SKILL{confirm=true} → Sim 再次验证。
- 玩家按右键/S/ESC/H → Layer 3 生成 CANCEL{scope=skill} → 切 Idle。

### 17.4 移动状态进入施法不打断移动

- 玩家右键移动中按 Q（normal cast）→ MoveAxis 保持 Moving，CommandAxis 切 SkillAiming。
- Sim 侧：MovePath.Following 保持（is_moving=true），player_pathfinding 看到 `CastState==None` 不清 Following → 玩家继续走。
- 玩家左键确认 → Sim 进 Chasing/Casting：
  - Chasing：player_pathfinding 切换为朝 TargetEntity/AimPos 的 A\*（覆盖原 MovePath）→ 玩家改向施法目标走。
  - Casting：player_movement gate → 玩家停步前摇。
- **关键**：MoveAxis 仅由 snapshot `is_moving` 决定，不受 CommandAxis 影响。

### 17.5 S 键双重语义

- 非施法/非瞄准：S = Stop（清 MovePath，停步）。
- SkillAiming：S = Cancel skill（回 Idle）。
- AttackAiming：S = Cancel attack（回 Idle）。
- CastLocked（Chasing/Casting）：S = Cancel skill（refund + None）。
- CastLocked（Channeling/Dashing）：S 忽略。
- **Layer 3 翻译时按 CommandAxis 分支决定 S 的语义**，不传到 Sim 再判断。

### 17.6 ESC 键三重语义

- SkillAiming：ESC = Cancel skill。
- AttackAiming：ESC = Cancel attack。
- CastLocked：ESC = Cancel skill（若可打断）。
- Idle/Moving：ESC = 打开/关闭设置面板（settings_panel._unhandled_input 接管）。
- **优先级**：input_event_queue 入队后，Layer 3 先消费 ESC；若 CommandAxis != Idle → 生成 CANCEL，**accept 事件阻止 settings_panel 收到**；若 CommandAxis == Idle → 不消费，事件继续传到 settings_panel。

### 17.7 Refund 策略

| 阶段                       | 默认                     | 备注                               |
| -------------------------- | ------------------------ | ---------------------------------- |
| Chasing cancel             | 退蓝退 CD                | 玩家主动放弃，未触发效果           |
| Casting cancel             | **退蓝退 CD**（v2 默认） | v1 是不退（惩罚），可配置          |
| Channeling cancel          | 不可打断                 | —                                  |
| Dashing cancel             | 不可打断                 | —                                  |
| No target (MeleeSingle)    | 不扣蓝不设 CD            | 验证失败，从未进入 Chasing/Casting |
| Target dead during Chasing | 退蓝退 CD                | 非玩家过错                         |

### 17.8 homing 箭与墙

- Homing 箭穿墙飞行（wall_collision 跳过）。
- Homing 箭只命中锁定目标（combat 过滤）。
- 目标死亡 → Homing 箭保持当前速度直线飞 → Lifetime 过期销毁。

### 17.9 普攻追击穿墙 vs 技能 Chasing 绕墙

- **普攻追击**：直线移动 + Chasing=true + wall_collision 跳过 = **穿墙**。
- **技能 Chasing**：A\* 寻路 + MovePath 跟随 = **绕墙**。
- 用户明确两者不同，**不要统一**。

### 17.10 失焦与 held_keys 漂移

- 窗口失焦时 Godot 可能不触发 release 事件 → held_keys 残留。
- Layer 1 每帧用 `Input.is_key_pressed` 校准 held_keys，防止残留。
- 失焦时不生成新的 press 事件（仅校准）。

---

## 18. 与 Bot 单位的关系澄清

### 18.1 当前状态

- Bot 与 Player 是**不同的单位类型**：
  - Player：`PlayerTag` + `PlayerInputState` + `CastState` + `AttackTarget` + `SkillComponent`（4 技能槽）
  - Bot：`BotTag` + `BotAIState` + `BotBehaviorState` + `SkillComponent`（占位，不施法）+ 无 `AttackTarget`
- Bot 的"攻击"是 `bot_combat_system` 调 `try_fire` 朝 `BotAIState.TargetEntity` 方向直射箭矢（**非指向性技能占位**，**不是普攻**）。
- Bot 不走 `skill_cast_system`，不走 `player_attack_command_system`，不走 `player_pathfinding_system`。

### 18.2 未来重构方向（不在本方案范围）

- Bot 将重构为**与玩家完全相同的英雄单位**：
  - 共用 `PlayerTag`（或泛化的 `HeroTag`）+ `CastState` + `AttackTarget` + `SkillComponent`
  - Bot AI 通过注入 `LocalInputSingleton` 等价物（per-bot input singleton）下达命令
  - Bot 走同一套 skill_cast / player_attack_command / player_pathfinding
- **因此 input_controller 设计必须假设未来 Bot 也会走同一套命令流**，View 侧的 input 链路与 Bot AI 完全解耦——input_controller 只服务本地玩家，Bot AI 通过 Sim 内部注入命令，不经过 View。

### 18.3 设计约束

- **不要**为了适配当前 Bot 行为而在 input_controller 中加 special case。
- **不要**假设 Bot 不会施法 / 不会普攻追击。
- Sim 侧的 skill_cast / player_attack_command 等系统**已经按 `PlayerTag.IsLocal` 过滤**，未来 Bot 重构时只需放宽此过滤或加 `IsAI` 标志。
- input_controller 的所有状态与命令都是"本地玩家"概念，与 Sim 的单位类型层正交。

---

## 19. 总结

本方案的核心改进：

1. **四层分离**：事件队列 / 状态机 / 命令翻译 / 命令缓冲，各司其职。
2. **双正交状态轴**：MoveAxis × CommandAxis，支持"施法模式不打断移动"。
3. **Sim Chasing 阶段**：跟随施法的权威实现，View 不参与寻路逻辑；**skill_cast 内加分支，tick 顺序 skill_cast 提前到 player_pathfinding 之前，confirm 同 tick 算 A\* + 移动，无 1 tick 延迟**。
4. **Quick / Normal cast 双模式**：per-slot 可配置，行为统一（左键 confirm 语义一致）。
5. **普攻独立模式**：AttackAiming + homing 箭 + 直线穿墙追击（区别于技能 Chasing 的 A\* 绕墙），与技能解耦；**移除 v1 普攻虚拟槽**。
6. **不丢指令**：CommandBuffer 跨 tick 持续，统一处理 30Hz vs 60Hz 差异。
7. **状态镜像**：View CastLocked 完全由 snapshot 决定，打断施法是状态转移而非 flag。
8. **WASD 模式彻底移除**：单一 MOBA 模式，文档与代码一致。
9. **技能升级链路补齐**：新增 `SkillPoints` 组件 + `SkillSlot::Level` 字段 + `skill_level.h` + `SKILL_UPGRADE` 命令，修正 v1 完全缺失的输入路径 + `SimSkillSlotSnap.level` 语义 bug。
10. **Refund 宽容**：Chasing/Casting 取消均退蓝退 CD（v2 默认，配置化留口）。

实施按 §16 阶段 A→F + E2 推进，最大风险在阶段 C（Sim Chasing 的 tick 顺序——skill_cast 必须在 player_pathfinding 之前保证无延迟）与阶段 D（No target 报错后 SkillAiming 保留的状态同步）。建议每阶段独立验收后再合并。
