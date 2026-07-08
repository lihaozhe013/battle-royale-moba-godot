# AGENTS.md — 项目上下文快照

> 最后更新：2026-07-08
> 当前阶段：视角操控方案（锁/自由 + 中键双模式 + 边缘推屏 + 全屏设置）已实施
> 引擎：Godot 4.7
> 架构：C++ GDExtension ECS (Sim) + GDScript (View)

---

## 项目概况

大逃杀 MOBA 游戏。当前为俯视角射击原型，左键普攻（箭矢），5 个 Bot，有等级/XP/血包/成长系统。
已新增**右键点地板寻路移动**、双控制模式（WASD / MOBA 点地板）、设置面板、右键长按连点、视角操控方案（锁/自由 + 中键双模式 + 边缘推屏 + 全屏设置）。

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
├── right_click_movement_design.md ← 右键点地板移动 + 双模式设计方案 ✅
├── bottom_hud_design.md         ← 底部 HUD UI 设计
├── python_map_editor_design.md  ← Python 地图编辑器设计+实现记录
└── camera_control_design.md     ← 视角操控 + 全屏设计方案 ✅

Docs/Reference/
├── prompt.md                    ← 主设计文档 + MOBA 升级方案
├── sim_system_reference.md      ← C++ 层完整参考（组件/系统/快照/常量）
├── bot_ai_optimization.md       ← Bot AI 决策树设计
├── skill_system_design.md       ← 4 技能系统完整设计方案（C/E/R/F）
└── godot-editor-todo.md         ← 编辑器待办事项

scripts/autoload/
└── game_settings.gd             ← 操作模式/camera/全屏 autoload 单例（ConfigFile 持久化）

scripts/ui/
├── health_bar_ui.gd             ← 血条组件（已有）
├── health_bar_manager.gd        ← 血条池管理器（已有）
├── skill_slot_ui.gd             ← 技能槽（已有，已挂载）
├── item_slot_ui.gd              ← 物品槽（已有，已挂载）
├── bottom_hud.gd                ← 底部 HUD（已有，动态 QWER/CERF label）
├── stats_panel.gd               ← 旧 HUD 文字面板（已有）
└── settings_panel.gd            ← 设置面板（ESC 开关，模式/相机/中键/边缘推屏/全屏切换）

scripts/view/
├── entity_manager.gd            ← 3D 实体管理（已有）
├── entity_view.gd               ← 3D 实体视图（插值 + 动画，LERP 修复 1/30）
├── camera_controller.gd         ← 相机控制（锁/自由 + 中键双模式 + 边缘推屏）
├── skill_vfx.gd                 ← 技能场景 VFX（绿线/灰圈/dash 路径/光环）
├── skill_vfx_attachment.gd      ← 指向性技能命中 VFX 挂载节点（C 刀光等）
└── move_target_vfx.gd           ← 右键 ping 地面标记（新增）

scenes/ui/
├── health_bar_ui.tscn           ← 血条预制（已有，含 ManaBar）
├── skill_slot_ui.tscn           ← 技能槽模板（4 个实例复用）
├── item_slot_ui.tscn            ← 物品槽模板
├── bottom_hud.tscn              ← 底部 HUD（含 4 技能槽 + 6 物品栏 + 6 背包）
└── settings_panel.tscn          ← 设置面板（VBox，5 项设置）

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
└── systems/*.h                  ← 18 个 inline System
```

## 当前阶段（2026-07-08）

### ✅ 已完成

| 事项 | 关联文档 |
|------|---------|
| Python 地图编辑器 P1-P4 | `python_map_editor_design.md` |
| MOBA 升级方案评估 | `prompt.md` §MOBA 大逃杀升级方案 |
| C++ 层完整参考手册 | `sim_system_reference.md` |
| 代码结构模板 + 待做清单 | `sim_system_reference.md` §11 |
| 底部 HUD 完整设计 | `bottom_hud_design.md` |
| 底部 HUD 场景+脚本+挂载+测试 | `bottom_hud.tscn` 已挂 `main.tscn` |
| Mana 系统 | `sim_system_reference.md` §11-P0-1 ✅ |
| 技能冷却+按键消耗 Mana | `skill_input.h` |
| UI 脚本创建 | `skill_slot_ui.gd`, `item_slot_ui.gd`, `bottom_hud.gd` |
| 占位图标生成 | 4 技能图标 + 3 鼠标指针 PNG |
| 旧 skill_bar_hud 清理 | 已删除 |
| **4 技能系统完整设计方案** | **`skill_system_design.md`**（C/E/R/F + 手动施法 + VFX） |
| **C 技能刀光命中 VFX** | `skill_vfx_attachment.gd`, `entity_view.gd` |
| **Bot 死亡停留时间 3s→8s** | `game_config.h` BotRespawnTime |
| **右键点地板移动 + A* 寻路（C++ Sim）** | `right_click_movement_design.md` |
| **双控制模式（WASD / MOBA QWER）** | `game_settings.gd`, `input_collector.gd` |
| **设置面板（ESC 切换 + 退出游戏）** | `settings_panel.gd/tscn` |
| **S 键停止（MOBA 模式）** | `player_movement.h`, `input_collector.gd` |
| **右键关节流 + 目标死区 + 转向速率** | `nav_grid.h`, `player_movement.h`, `input_collector.gd` |
| **右键长按连点（~6Hz）** | `input_collector.gd` |
| **ConfigFile 持久化模式偏好** | `game_settings.gd` |
| **视角操控方案（锁/自由 + 中键双模式 + 边缘推屏 + 全屏设置）** | `camera_control_design.md` → `game_settings.gd`, `camera_controller.gd`, `settings_panel.gd/tscn` |

### ❌ 待做（C++ Sim 层 — 4 技能实施剩余）

> 完整方案见 `skill_system_design.md`，按 §12 阶段 A→E 推进

| 阶段 | 模块 | 关键产出 |
|------|------|---------|
| A | C 技能全链路最小闭环 | `skill_defs.h` + `skill_cast.h`(C) + CastState + input 重写 + 绿线 VFX |
| B | R 位移 | Dashing 分支 + dash 推进 + 路径 VFX |
| C | E AoE+眩晕 | AoEField effect + StatusEffect(Stun) + AoE 实体 + 灰圈 VFX |
| D | F 大招 | Channeling + 16 方向弹幕 + Lifesteal + 光环 VFX |
| E | 数值 & 打磨 | 调参 |

### ❌ 待做（后续 MOBA 模块，非本次范围）

| # | 模块 | 参考 |
|---|------|------|
| P0-5 | 玩家死亡+淘汰 | `sim_system_reference.md` §11-P0-5 |
| P0-6 | 缩圈系统 | `sim_system_reference.md` §11-P0-6 |
| P1-7 | Bot 技能 AI | `prompt.md` §二 |
| P1-8 | 装备系统 | `prompt.md` §七 |

## 关键约定

- 所有 C++ System 是 header-only `inline void` 函数，在 `sim` namespace
- 实体创建/销毁必须通过 `CommandBuffer`（`cb.push`）
- System 间通过组件通信，无全局变量
- 快照是 Sim→View 的唯一通道
- UI 场景模板 + 脚本分离：脚本不创建节点，只引用已有节点

## SimSnapshot 快照字段速查

```
SimSnapshot.seq / t / players[] / bots[] / arrows[] / pickups[] / events[]
SimPlayerSnap: id, x, y, ang, hp, max_hp, mana, max_mana, atk, asp, speed, kills, level, xp, xp_needed, skills[], cast_state, cast_slot, cast_aim_x/y, dash_sx/sy, dash_tx/ty
SimBotSnap:    id, x, y, ang, hp, max_hp, dead, mana, max_mana, atk, asp, kills, level, xp, xp_needed, speed, tier, skills[]
SimArrowSnap:  id, x, y, ang
SimPickupSnap: id, x, y, type(0/1/2), value
SimEventSnap:  killer_id, victim_id
SimAoESnap:    id, x, y, radius, remaining, duration
```

## tick 顺序

**当前（18 systems）：**

```
local_input_injection → player_pathfinding → player_movement → player_fire →
skill_cast → bot_targeting → bot_ai → bot_combat → arrow_movement →
wall_collision → combat → pickup → aoe → status_effect → mana_regen →
skill_cooldown → progression → snapshot_export
```

详见 `right_click_movement_design.md` §12

## 新增字段（SimSnapshot）

| Snap | 新增字段 |
|------|---------|
| `SimPlayerSnap` | `mana`, `max_mana`, `skills[]`, `cast_state`, `cast_slot`, `cast_aim_x/y`, `dash_sx/sy`, `dash_tx/ty` |
| `SimBotSnap` | `mana`, `max_mana`, `skills[]` |
| `SimSkillSlotSnap` | `skill_id`, `level`, `cooldown`, `max_cooldown`, `mana_cost` |
| `SimAoESnap`（新） | `id`, `x`, `y`, `radius`, `remaining`, `duration` |
| `SimSnapshot` | `aoes: TypedArray<SimAoESnap>` |

## 新增组件（components.h）

| 组件 | 字段 |
|------|------|
| `Mana` | `Cur, Max, RegenRate, RegenDelay, RegenTimer` |
| `SkillSlot` | `SkillId, Level, CooldownTimer, MaxCooldown, ManaCost` |
| `SkillComponent` | `Slots[4]` |
| `SkillPoints` | `Available` |
| `CastState` | `State(Phase 枚举), ActiveSlot, SkillId, Timer, SubTimer, AimPos, DashStart, DashTarget` |
| `StatusEffect` | `Type(StatusType 枚举), Timer` |
| `AoETag` | `OwnerId, SkillId, Radius, Duration, Timer` |
| `MovePath` | `Waypoints[], CurrentIndex, Following, FinalTarget` |
| `LocalInputSingleton`/`PlayerInputState`（扩展） | `MoveTarget, MoveIssue, Stop` |
| `ArrowTag`（扩展） | `+LifestealRatio` |

## 新增 System（src_cpp/sim/systems/）

| System | 文件 | 职责 |
|--------|------|------|
| `mana_regen_system` | `mana_regen.h` | Mana 每 tick 回复 |
| `skill_cooldown_system` | `skill_cooldown.h` | 冷却计时递减 |
| `skill_cast_system` | `skill_cast.h` | 施法状态机 |
| `status_effect_system` | `status_effect.h` | Root/Stun timer 递减 |
| `aoe_system` | `aoe.h` | AoE 实体生命周期 |
| `player_pathfinding_system` | `player_pathfinding.h` | 读 MoveIssue → A* → 写 MovePath |
