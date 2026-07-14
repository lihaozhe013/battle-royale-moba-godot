# AGENTS.md — 项目上下文快照

> 最后更新：2026-07-13
> 当前阶段：**输入系统 v2 已完整实施**（四层架构 / Chasing / Quick-Normal cast / 普攻独立模式 / 技能升级链路）
> 引擎：Godot 4.7
> 架构：C++ GDExtension ECS (Sim) + GDScript (View)

---

## 项目概况

MOBA 游戏，俯视角。控制方式：**右键点地板移动 + Q/W/E/R 4 技能 + A 键普攻命令**（单一 MOBA 模式，WASD 模式已废弃并移除）。有 Bot、等级/XP/血包/成长系统。

> **历史澄清**：项目早期曾有 WASD + MOBA 双模式，现已完全移除 WASD 模式。如在任何旧文档中读到 WASD 相关内容均为过时误导，以本文件和 `Docs/Reference/input_system_design.md` 为准。

## 架构

```
C++ Sim (entt::registry + 18 systems) 30Hz
  ↓ SimSnapshot (RefCounted, 6 snap types)
GDScript View (interpolation at 60Hz)
```

Sim 层零 Godot 依赖，所有通信走 Snapshot。

## 文件结构（关键路径）

```
AGENTS.md                        ← 本文档
Docs/Archive/
├── bottom_hud_design.md         ← 底部 HUD UI 设计
├── python_map_editor_design.md  ← Python 地图编辑器设计+实现记录
└── camera_control_design.md     ← 视角操控 + 全屏设计方案 ✅

Docs/Reference/
├── prompt.md                    ← 主设计文档 + MOBA 升级方案
├── sim_system_reference.md      ← C++ 层完整参考（组件/系统/快照/常量）
├── bot_ai_optimization.md       ← Bot AI 决策树设计
├── input_system_design.md       ← 输入系统重构设计方案（唯一标准）
└── godot-editor-todo.md         ← 编辑器待办事项

scripts/autoload/
├── game_settings.gd             ← 操作模式/camera/全屏 autoload 单例（ConfigFile 持久化，含 smooth_pan / edge_pan_speed）

scripts/input/
├── input_event_queue.gd         ← Layer 1: 事件队列（_unhandled_input 入口, 射线投影, echo 过滤）
├── input_state_machine.gd       ← Layer 2: 双正交 FSM（MoveAxis × CommandAxis）
├── command.gd                   ← Command 数据类（6 种命令类型）
├── command_builder.gd           ← Layer 3: 事件→命令翻译（含 move_issued signal）
├── command_buffer.gd            ← Layer 4: 跨 tick FIFO + 合并去重
└── cast_settings.gd             ← Quick/Normal cast per-slot 偏好

scripts/ui/
├── health_bar_ui.gd             ← 血条组件（已有）
├── health_bar_manager.gd        ← 血条池管理器（已有）
├── skill_slot_ui.gd             ← 技能槽（已有，已挂载）
├── item_slot_ui.gd              ← 物品槽（已有，已挂载）
├── bottom_hud.gd                ← 底部 HUD（已有，动态 QWER label）
├── stats_panel.gd               ← 旧 HUD 文字面板（已有）
└── settings_panel.gd            ← 设置面板（ESC 开关，双列 HBox 布局，模式/相机/边缘推屏/速度/平滑/全屏 6 项设置）

scripts/view/
├── entity_manager.gd            ← 3D 实体管理（已有）
├── entity_view.gd               ← 3D 实体视图（插值 + 动画，LERP 修复 1/30）
├── camera_controller.gd         ← 相机控制（锁/自由 + 像素精准拖屏 + 边缘推屏 + Smooth Pan + F1/Space 按住居中）
├── skill_vfx.gd                 ← 技能场景 VFX（绿线/灰圈/dash 路径/光环）
├── skill_vfx_attachment.gd      ← 指向性技能命中 VFX 挂载节点（C 刀光等）
└── move_target_vfx.gd           ← 右键 ping 地面标记（新增）

scenes/ui/
├── health_bar_ui.tscn           ← 血条预制（已有，含 ManaBar）
├── skill_slot_ui.tscn           ← 技能槽模板（4 个实例复用）
├── item_slot_ui.tscn            ← 物品槽模板
├── bottom_hud.tscn              ← 底部 HUD（含 4 技能槽 + 6 物品栏 + 6 背包）
└── settings_panel.tscn          ← 设置面板（HBox 双列布局，6 项设置）

tools/map_editor/                ← Python 地图编辑器（pygame + watchdog）
├── __main__.py                  ← 入口
├── viewer.py                    ← 渲染 + 交互
├── map_model.py                 ← JSON 读写
├── watcher.py                   ← 文件热加载
├── commands.py                  ← Undo/Redo 栈
├── map_editor_config.yaml       ← 编辑器配置
└── map_editor_help.txt          ← 帮助面板文本

data/skills/icons/               ← 7 个占位图标 PNG（系统自动导入）

src_cpp/sim/                     ← C++ Sim 层核心
├── components.h                 ← 全部 ECS 组件（含 MovePath）
├── game_config.h                ← 游戏常量（含 PathTurnRate 等）
├── skill_defs.h                 ← 4 技能定义表
├── nav_grid.h                   ← NavGrid A* 寻路（新增）
├── world.cpp                    ← World 初始化 + tick 循环
├── world.h                      ← World 声明
├── snapshot_types.h             ← 快照数据类（暴露给 GDScript）
├── snapshot_builder.h/cpp       ← 快照构建
├── snapshot_bindings.cpp        ← 快照属性绑定
└── systems/*.h                  ← 20 个 inline System
```

## 当前阶段（2026-07-08）

### ✅ 已完成

| 事项                                                                               | 关联文档                                                                                          |
| ---------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------- |
| Python 地图编辑器 P1-P4                                                            | `python_map_editor_design.md`                                                                     |
| MOBA 升级方案评估                                                                  | `prompt.md` §MOBA 大逃杀升级方案                                                                  |
| C++ 层完整参考手册                                                                 | `sim_system_reference.md`                                                                         |
| 代码结构模板 + 待做清单                                                            | `sim_system_reference.md` §11                                                                     |
| 底部 HUD 完整设计                                                                  | `bottom_hud_design.md`                                                                            |
| 底部 HUD 场景+脚本+挂载+测试                                                       | `bottom_hud.tscn` 已挂 `main.tscn`                                                                |
| Mana 系统                                                                          | `sim_system_reference.md` §11-P0-1 ✅                                                             |
| 技能冷却+按键消耗 Mana                                                             | `skill_input.h`                                                                                   |
| UI 脚本创建                                                                        | `skill_slot_ui.gd`, `item_slot_ui.gd`, `bottom_hud.gd`                                            |
| 占位图标生成                                                                       | 4 技能图标 + 3 鼠标指针 PNG                                                                       |
| 旧 skill_bar_hud 清理                                                              | 已删除                                                                                            |
| **4 技能系统完整设计方案**                                                         | **已废弃，由 `input_system_design.md` 取代**                                                      |
| **Q 技能刀光命中 VFX**                                                             | `skill_vfx_attachment.gd`, `entity_view.gd`                                                       |
| **Bot 死亡停留时间 3s→8s**                                                         | `game_config.h` BotRespawnTime                                                                    |
| _*右键点地板移动 + A* 寻路（C++ Sim）_*                                            | `input_system_design.md`                                                                          |
| **单一 MOBA 控制模式（WASD 模式已移除）**                                          | `game_settings.gd`, `input_collector.gd`                                                          |
| **设置面板（ESC 切换 + 退出游戏）**                                                | `settings_panel.gd/tscn`                                                                          |
| **S 键停止（MOBA 模式）**                                                          | `player_movement.h`, `input_collector.gd`                                                         |
| **右键关节流 + 目标死区 + 转向速率**                                               | `nav_grid.h`, `player_movement.h`, `input_collector.gd`                                           |
| **右键长按连点（~6Hz）**                                                           | `input_collector.gd`                                                                              |
| **ConfigFile 持久化偏好（camera/全屏等）**                                         | `game_settings.gd`                                                                                |
| **视角操控方案（锁/自由 + 像素精准拖屏 + 边缘推屏 + Smooth Pan + 全屏/按住居中）** | `camera_control_design.md` → `game_settings.gd`, `camera_controller.gd`, `settings_panel.gd/tscn` |
| **Q 指向性技能（真单位选取）+ 返还**                                               | `skill_cast.h` → `TargetEntity` / `target_alive` / `target_in_range`                              |
| **悬停高亮**                                                                       | `entity_view.gd`, `entity_manager.gd`, `sim_bridge.gd`                                            |
| **施法条 + Channeling 字样**                                                       | `cast_bar.tscn`, `cast_bar.gd`                                                                    |
| **施法错误报错 label**                                                             | `cast_bar.gd` + `cast_error` 快照字段                                                             |
| **瞄准绿线（Aiming VFX）** | `skill_vfx.gd`（已有） |

### ✅ 已实施（输入系统 v2）

| 事项                                                                               | 关联文档                                                                                          |
| ---------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------- |
| **View 四层框架**                                                                  | `input_event_queue.gd` / `input_state_machine.gd` / `command_builder.gd` / `command_buffer.gd`   |
| **Sim 命令 API 重构**                                                              | `set_*_command` 系列；`LocalInputSingleton` 重构为命令式                                          |
| **Sim CastState + Chasing（skill_cast 内分支，无 1 tick 延迟）**                   | `skill_cast.h` +Chasing 分支；tick 顺序重排（skill_cast 提前到 player_pathfinding 前）           |
| **Quick / Normal Cast 双模式**                                                     | per-slot `cast_settings.gd`；绿线仅 normal cast 显示；No target 错误保留 SkillAiming              |
| **普攻命令模式（独立分支 + 直线穿墙追击）**                                       | `AttackTarget.Chasing` + homing 箭 + `wall_collision` 跳过追击玩家                                |
| **移除 v1 普攻虚拟槽**                                                             | `SkillComponent.Slots[5]→[4]`，删 `skill_defs.h` id=5 Attack                                     |
| **技能升级链路（补 v1 完全缺失）**                                                 | 新增 `SkillPoints` 组件 + `SkillSlot::Level` + `systems/skill_level.h` + `SKILL_UPGRADE` 命令    |
| **修正 SimSkillSlotSnap.level 语义 bug**                                           | snapshot 字段改为 `slot.Level`（之前误用 char_level）                                             |
| **Snapshot 扩展**                                                                  | SimPlayerSnap 新增 `is_moving` / `cast_target_id` / `skill_points`                               |
| **WASD 模式彻底移除**                                                             | `player_movement.h` 移除 WASD 分支；`sim_bridge.gd` 移除旧 InputCollector 引用                   |
| **refund 配置化（v2 默认退蓝退 CD）**                                              | `GameConfig::RefundOnCastInterrupt = true` / `RefundOnChaseInterrupt = true`                      |

### ❌ 待做（后续 MOBA 模块，非本次范围）

| #    | 模块                                         | 参考                                           |
| ---- | -------------------------------------------- | ---------------------------------------------- |
| P0-5 | 玩家死亡+淘汰                                | `sim_system_reference.md` §11-P0-5             |
| P0-6 | 缩圈系统                                     | `sim_system_reference.md` §11-P0-6             |
| P1-7 | Bot 技能 AI                                  | `prompt.md` §二                                |
| P1-8 | 装备系统（含装备主动技能槽 slot 10-15 预留） | `prompt.md` §七、`input_system_design.md` §5.4 |

## 关键约定

- 所有 C++ System 是 header-only `inline void` 函数，在 `sim` namespace
- 实体创建/销毁必须通过 `CommandBuffer`（`cb.push`）
- System 间通过组件通信，无全局变量
- 快照是 Sim→View 的唯一通道
- UI 场景模板 + 脚本分离：脚本不创建节点，只引用已有节点

## SimSnapshot 快照字段速查

```
SimSnapshot.seq / t / players[] / bots[] / arrows[] / pickups[] / events[]
SimPlayerSnap: id, x, y, ang, hp, max_hp, mana, max_mana, atk, asp, speed, kills, level, xp, xp_needed, skills[], cast_state, cast_slot, cast_progress, cast_aim_x/y, dash_sx/sy, dash_tx/ty, hit_target_id, cast_error, attack_target_id, cast_target_id, is_moving, skill_points
SimBotSnap:    id, x, y, ang, hp, max_hp, dead, mana, max_mana, atk, asp, kills, level, xp, xp_needed, speed, tier, skills[]
SimArrowSnap:  id, x, y, ang
SimPickupSnap: id, x, y, type(0/1/2), value
SimEventSnap:  killer_id, victim_id
SimAoESnap:    id, x, y, radius, remaining, duration
```

## tick 顺序

**当前 v2（20 systems，详见 `Docs/Reference/input_system_design.md` §13）：**

```
local_input_injection → player_attack_command → skill_cast → player_pathfinding →
player_movement → player_attack_fire → bot_targeting → bot_ai → bot_combat →
arrow_movement → wall_collision → combat → pickup → aoe → status_effect →
mana_regen → skill_cooldown → skill_level → progression → snapshot_export
```

> **关键**：`skill_cast` 提前到 `player_pathfinding` 之前，保证 confirm 同 tick 设 Chasing + 算 A\* + 移动，**无 1 tick 延迟**。

## 新增字段（SimSnapshot）

| Snap               | 新增字段                                                                                              |
| ------------------ | ----------------------------------------------------------------------------------------------------- |
| `SimPlayerSnap`    | `mana`, `max_mana`, `skills[]`, `cast_state`, `cast_slot`, `cast_aim_x/y`, `dash_sx/sy`, `dash_tx/ty` |
| `SimBotSnap`       | `mana`, `max_mana`, `skills[]`                                                                        |
| `SimSkillSlotSnap` | `skill_id`, `level`, `cooldown`, `max_cooldown`, `mana_cost`                                          |
| `SimAoESnap`（新） | `id`, `x`, `y`, `radius`, `remaining`, `duration`                                                     |
| `SimSnapshot`      | `aoes: TypedArray<SimAoESnap>`                                                                        |

## 新增组件（components.h — 重构后目标，部分 v1 已有部分待新增）

| 组件                                             | 字段                                                                                                                                                                                          | v1 状态                                             |
| ------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------- |
| `Mana`                                           | `Cur, Max, RegenRate, RegenDelay, RegenTimer`                                                                                                                                                 | ✅ 已有                                             |
| `SkillSlot`                                      | `SkillId, Level, CooldownTimer, MaxCooldown, ManaCost`                                                                                                                                        | ⚠️ **Level 字段待新增**                             |
| `SkillComponent`                                 | `Slots[4]`                                                                                                                                                                                    | ⚠️ v1 是 `Slots[5]`（含普攻虚拟槽），重构后改 `[4]` |
| `SkillPoints`                                    | `Available`                                                                                                                                                                                   | ❌ **待新增**（v1 完全缺失）                        |
| `CastState`                                      | `State(Phase 枚举含 Chasing), ActiveSlot, SkillId, Timer, SubTimer, AimPos, DashStart, DashTarget, HitTargetId, CastError, TargetEntity, TargetNetworkId, QuickCast`                          | ⚠️ v1 无 Chasing/TargetNetworkId/QuickCast          |
| `StatusEffect`                                   | `Type(StatusType 枚举), Timer`                                                                                                                                                                | ✅ 已有                                             |
| `AoETag`                                         | `OwnerId, SkillId, Radius, Duration, Timer`                                                                                                                                                   | ✅ 已有                                             |
| `MovePath`                                       | `Waypoints[], CurrentIndex, Following, FinalTarget`                                                                                                                                           | ✅ 已有                                             |
| `AttackTarget`                                   | `Target, TargetNetworkId, Chasing`                                                                                                                                                            | ⚠️ v1 无 Chasing 字段                               |
| `Homing`                                         | `Target, TargetNetId`                                                                                                                                                                         | ✅ 已有                                             |
| `LocalInputSingleton`/`PlayerInputState`（重构） | `MoveTarget, MoveIssue, Stop, SkillSlot, SkillConfirm, SkillAim, SkillTargetId, SkillUpgradeSlot, CancelSkill, CancelAttack, AttackTargetId, AttackGround, AttackGroundPos, AttackClear, Seq` | ❌ **完全重构**（v1 是 Move/Aim/Fire/Cast* 旧字段） |
| `ArrowTag`（扩展）                               | `+LifestealRatio`                                                                                                                                                                             | ✅ 已有                                             |

## 新增 System（src_cpp/sim/systems/）

| System                         | 文件                      | 职责                                                           |
| ------------------------------ | ------------------------- | -------------------------------------------------------------- |
| `mana_regen_system`            | `mana_regen.h`            | Mana 每 tick 回复 ✅                                           |
| `skill_cooldown_system`        | `skill_cooldown.h`        | 冷却计时递减 ✅                                                |
| `skill_cast_system`            | `skill_cast.h`            | 施法状态机（v2 +Chasing 分支，无 1 tick 延迟） ⚠️              |
| `status_effect_system`         | `status_effect.h`         | Root/Stun timer 递减 ✅                                        |
| `aoe_system`                   | `aoe.h`                   | AoE 实体生命周期 ✅                                            |
| `player_pathfinding_system`    | `player_pathfinding.h`    | 读 MoveIssue → A* → 写 MovePath（v2 +技能 Chasing A\*） ⚠️     |
| `player_attack_command_system` | `player_attack_command.h` | 处理 ATTACK 命令 + 目标验证 + 清锁 ✅                          |
| `player_attack_fire_system`    | `player_attack_fire.h`    | 锁定目标射 homing 箭 ✅                                        |
| `skill_level_system`           | `skill_level.h`           | **新增**：消费 SKILL_UPGRADE → SkillPoints-- + slot.Level++ ❌ |
