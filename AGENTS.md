# AGENTS.md — 项目上下文快照

> 最后更新：2026-07-07
> 引擎：Godot 4.7
> 架构：C++ GDExtension ECS (Sim) + GDScript (View)

---

## 项目概况

大逃杀 MOBA 游戏。当前为俯视角射击原型，左键普攻（箭矢），5 个 Bot，有等级/XP/血包/成长系统。正在向完整 MOBA 技能系统+大逃杀机制升级。

## 架构

```
C++ Sim (entt::registry + 12 systems) 30Hz
  ↓ SimSnapshot (RefCounted, 6 snap types)
GDScript View (interpolation at 60Hz)
```

Sim 层零 Godot 依赖，所有通信走 Snapshot。

## 文件结构（关键路径）

```
AGENTS.md                        ← 本文档
Docs/Reference/
├── prompt.md                    ← 主设计文档 + MOBA 升级方案
├── sim_system_reference.md      ← C++ 层完整参考（组件/系统/快照/常量）
├── bottom_hud_design.md         ← 底部 HUD UI 设计
├── bot_ai_optimization.md       ← Bot AI 决策树设计
├── skill_system_design.md            ← 4 技能系统完整设计方案（C/E/R/F）
├── godot-editor-todo.md              ← 编辑器待办事项
└── python_map_editor_design.md       ← Python 地图编辑器设计+实现记录

scripts/ui/
├── health_bar_ui.gd             ← 血条组件（已有）
├── health_bar_manager.gd        ← 血条池管理器（已有）
├── skill_slot_ui.gd             ← 技能槽（已有，已挂载）
├── item_slot_ui.gd              ← 物品槽（已有，已挂载）
├── bottom_hud.gd                ← 底部 HUD（已有，已挂载 main.tscn 测试通过）
└── stats_panel.gd               ← 旧 HUD 文字面板（已有）

scripts/view/
├── entity_manager.gd            ← 3D 实体管理（已有）
├── entity_view.gd               ← 3D 实体视图（已有）
├── camera_controller.gd         ← 相机控制（已有）
└── skill_vfx.gd                 ← 技能 VFX（待创建：绿线/灰圈/dash 路径/光环）

scenes/ui/
├── health_bar_ui.tscn           ← 血条预制（已有，含 ManaBar）
├── skill_slot_ui.tscn           ← 技能槽模板（4 个实例复用）
├── item_slot_ui.tscn            ← 物品槽模板
└── bottom_hud.tscn              ← 底部 HUD（含 4 技能槽 + 6 物品栏 + 6 背包）

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
├── components.h                 ← 全部 ECS 组件
├── game_config.h                ← 游戏常量
├── skill_defs.h                 ← 4 技能定义表（待创建）
├── world.cpp                    ← World 初始化 + tick 循环
├── snapshot_types.h             ← 快照数据类（暴露给 GDScript）
└── systems/*.h                  ← 12 个 inline System
```

## 当前阶段（2026-07-07）

### ✅ 已完成

| 事项 | 关联文档 |
|------|---------|
| Python 地图编辑器 P1-P4 | `python_map_editor_design.md` |
| MOBA 升级方案评估 | `prompt.md` §MOBA 大逃杀升级方案 |
| C++ 层完整参考手册 | `sim_system_reference.md` |
| 代码结构模板 + 待做清单 | `sim_system_reference.md` §11 |
| 底部 HUD 完整设计 | `bottom_hud_design.md` |
| 底部 HUD 场景+脚本+挂载+测试 | `bottom_hud.tscn` 已挂 `main.tscn`，`sim_bridge.gd:91-92` 已调 `sync_player`/`sync_skills` |
| Mana 系统 | `sim_system_reference.md` §11-P0-1 ✅ |
| 技能冷却+按键消耗 Mana | `skill_input.h`（占位，将被 `skill_cast.h` 取代） |
| UI 脚本创建 | `skill_slot_ui.gd`, `item_slot_ui.gd`, `bottom_hud.gd` |
| 占位图标生成 | 4 技能图标 + 3 鼠标指针 PNG |
| 旧 skill_bar_hud 清理 | 已删除 |
| **4 技能系统完整设计方案** | **`skill_system_design.md`**（C/E/R/F + 手动施法 + VFX） |

### ❌ 待做（C++ Sim 层 — 4 技能实施）

> 完整方案见 `skill_system_design.md`，按 §12 阶段 A→E 推进

| 阶段 | 模块 | 关键产出 |
|------|------|---------|
| A | C 技能全链路最小闭环 | `skill_defs.h` + `skill_cast.h`(C) + CastState + input 重写 + 绿线 VFX |
| B | R 位移 | Dashing 分支 + dash 推进 + 路径 VFX |
| C | E AoE+定身 | AoEField effect + StatusEffect + AoE 实体 + 灰圈 VFX |
| D | F 大招 | Channeling + 16 方向弹幕 + Lifesteal + 光环 VFX |
| E | 数值 & 打磨 | Mana Max=300，调参 |

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
SimPlayerSnap: id, x, y, ang, hp, max_hp, atk, asp, speed, kills, level, xp, xp_needed
SimBotSnap:    id, x, y, ang, hp, max_hp, dead, atk, asp, kills, level, xp, xp_needed, speed, tier
SimArrowSnap:  id, x, y, ang
SimPickupSnap: id, x, y, type(0/1/2), value
SimEventSnap:  killer_id, victim_id
```

## tick 顺序

**当前（15 systems，`skill_input` 为占位将被取代）：**

```
local_input_injection → player_movement → player_fire → skill_input →
bot_targeting → bot_ai → bot_combat → arrow_movement → wall_collision →
combat → pickup → mana_regen → skill_cooldown → progression → snapshot_export
```

**4 技能实施后（18 systems）：**

```
local_input_injection → player_movement → player_fire → skill_cast →
bot_targeting → bot_ai → bot_combat → arrow_movement → wall_collision →
combat → pickup → aoe → status_effect → mana_regen → skill_cooldown →
progression → snapshot_export
```

详见 `skill_system_design.md` §6

## 新增字段（SimSnapshot）

| Snap | 新增字段 |
|------|---------|
| `SimPlayerSnap` | `mana`, `max_mana`, `skills[]` |
| `SimBotSnap` | `mana`, `max_mana`, `skills[]` |
| `SimSkillSlotSnap` | `skill_id`, `level`, `cooldown`, `max_cooldown`, `mana_cost` |

**4 技能实施后新增（见 `skill_system_design.md` §7）：**

| Snap | 新增字段 |
|------|---------|
| `SimPlayerSnap` | `cast_state`, `cast_slot`, `cast_progress`, `cast_aim_x/y`, `dash_sx/sy`, `dash_tx/ty` |
| `SimBotSnap` | `root_timer` |
| `SimAoESnap`（新） | `id`, `x`, `y`, `radius`, `remaining`, `duration` |
| `SimSnapshot` | `aoes: TypedArray<SimAoESnap>` |

## 新增组件（components.h）

| 组件 | 字段 |
|------|------|
| `Mana` | `Cur, Max, RegenRate, RegenDelay, RegenTimer` |
| `SkillSlot` | `SkillId, Level, CooldownTimer, MaxCooldown, ManaCost` |
| `SkillComponent` | `Slots[4]` |
| `SkillPoints` | `Available` |

**4 技能实施后新增（见 `skill_system_design.md` §3）：**

| 组件 | 字段 |
|------|------|
| `CastState` | `State(Phase 枚举), ActiveSlot, SkillId, Timer, SubTimer, AimPos, DashStart, DashTarget` |
| `StatusEffect` | `Type(StatusType 枚举), Timer` |
| `AoETag` | `OwnerId, SkillId, Radius, Duration, Timer` |
| `ArrowTag`（修改） | `+LifestealRatio` |
| `LocalInputSingleton`/`PlayerInputState`（修改） | `+CastSlot, CastConfirm, CastCancel, CastAim` |

## 新增 System（src_cpp/sim/systems/）

| System | 文件 | 职责 |
|--------|------|------|
| `mana_regen_system` | `mana_regen.h` | Mana 每 tick 回复 |
| `skill_input_system` | `skill_input.h` | （占位，将被 `skill_cast.h` 取代） |
| `skill_cooldown_system` | `skill_cooldown.h` | 冷却计时递减 |
| `skill_cast_system` | `skill_cast.h`（待创建） | 施法状态机：None↔Aiming↔Casting↔Channeling/Dashing，含 effect 触发 |
| `status_effect_system` | `status_effect.h`（待创建） | Root timer 递减 |
| `aoe_system` | `aoe.h`（待创建） | AoE 实体生命周期 |
