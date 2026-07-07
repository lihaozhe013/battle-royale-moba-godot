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
└── godot-editor-todo.md         ← 编辑器待办事项

scripts/ui/
├── health_bar_ui.gd             ← 血条组件（已有）
├── health_bar_manager.gd        ← 血条池管理器（已有）
├── skill_slot_ui.gd             ← 技能槽（已创建，等待挂载）
├── item_slot_ui.gd              ← 物品槽（已创建，等待挂载）
├── bottom_hud.gd                ← 底部 HUD（已创建，等待挂载）
└── stats_panel.gd               ← 旧 HUD 文字面板（已有）

scenes/ui/
├── health_bar_ui.tscn           ← 血条预制（已有，含 ManaBar）
├── skill_slot_ui.tscn           ← 技能槽模板（4 个实例复用）
├── item_slot_ui.tscn            ← 物品槽模板
└── bottom_hud.tscn              ← 底部 HUD（含 4 技能槽 + 6 物品栏 + 6 背包）

data/skills/icons/               ← 7 个占位图标 PNG（系统自动导入）
src_cpp/sim/                     ← C++ Sim 层核心
├── components.h                 ← 全部 ECS 组件
├── game_config.h                ← 游戏常量
├── world.cpp                    ← World 初始化 + tick 循环
├── snapshot_types.h             ← 快照数据类（暴露给 GDScript）
└── systems/*.h                  ← 12 个 inline System
```

## 当前阶段（2026-07-07）

### ✅ 已完成

| 事项 | 关联文档 |
|------|---------|
| MOBA 升级方案评估 | `prompt.md` §MOBA 大逃杀升级方案 |
| C++ 层完整参考手册 | `sim_system_reference.md` |
| 代码结构模板 + 待做清单 | `sim_system_reference.md` §11 |
| 底部 HUD 完整设计 | `bottom_hud_design.md` |
| UI 场景模板创建（待调参） | `skill_slot_ui.tscn`, `item_slot_ui.tscn`, `bottom_hud.tscn` |
| UI 脚本创建 | `skill_slot_ui.gd`, `item_slot_ui.gd`, `bottom_hud.gd` |
| 占位图标生成 | 4 技能图标 + 3 鼠标指针 PNG |
| 旧 skill_bar_hud 清理 | 已删除 |

### ❌ 待做（编辑器手动操作）

这些都是场景文件已创建但参数空着，需要你在 Godot 编辑器中手动调整：

| 事项 | 打开哪个场景 | 做什么 |
|------|-------------|--------|
| HP/Mana 条尺寸 | `bottom_hud.tscn` | 设 HPContainer/ManaContainer 的 custom_minimum_size |
| HP/Mana 填充颜色 | `bottom_hud.tscn` | HPFill=绿, ManaFill=蓝 |
| HP/Mana 字体 | `bottom_hud.tscn` | 调 Label 的 font_size/font_color |
| 头像 | `bottom_hud.tscn` | 拖纹理到 Avatar |
| 技能图标 | `bottom_hud.tscn` | 逐个拖入 4 个 SkillSlot 的 Icon.texture |
| HUDPanel 锚点 | `bottom_hud.tscn` | 设 Bottom Wide + offset 贴底 |
| 各容器间距 | `bottom_hud.tscn` | HBoxContainer.separation |
| 挂到 main.tscn | `main.tscn` | Instance bottom_hud.tscn 到场景 |

### ❌ 待做（C++ Sim 层，按优先级）

| # | 模块 | 参考 |
|---|------|------|
| P0-1 | Mana 系统 | `sim_system_reference.md` §11-P0-1 |
| P0-2 | 技能系统框架（1 个 skillshot 跑通） | `sim_system_reference.md` §11-P0-2 |
| P0-3 | 施法指示器 | `prompt.md` §三 |
| P0-4 | 技能栏 HUD 数据绑定 | `bottom_hud.gd` 的 sync_skills |
| P0-5 | 玩家死亡+淘汰 | `sim_system_reference.md` §11-P0-5 |
| P0-6 | 缩圈系统 | `sim_system_reference.md` §11-P0-6 |
| P1-7 | 多技能类型 | `prompt.md` §二 |
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

## 当前 tick 顺序（12 systems）

```
local_input_injection → player_movement → player_fire → bot_targeting →
bot_ai → bot_combat → arrow_movement → wall_collision → combat →
pickup → progression → snapshot_export
```
