# 右键点地板移动 + 双模式 + 设置面板 — 设计方案

> 最后更新：2026-07-08
> 关联：`sim_system_reference.md`、`skill_system_design.md`、`prompt.md`
> 状态：✅ 已批准，待实施

---

## 目录

1. [概述](#1-概述)
2. [架构决策](#2-架构决策)
3. [双模式控制方案](#3-双模式控制方案)
4. [右键仲裁规则](#4-右键仲裁规则)
5. [寻路设计](#5-寻路设计)
6. [S 键停止移动](#6-s-键停止移动)
7. [设置面板](#7-设置面板)
8. [ConfigFile 持久化](#8-configfile-持久化)
9. [移动 ping VFX](#9-移动-ping-vfx)
10. [变更汇总](#10-变更汇总)
11. [分阶段实施计划](#11-分阶段实施计划)
12. [Tick 顺序](#12-tick-顺序)
13. [风险与取舍](#13-风险与取舍)
14. [附录：组件/System/API 变更详情](#14-附录组件systemapi-变更详情)

---

## 1. 概述

### 目标

在现有 WASD 原型基础上，新增「右键点地板→A* 寻路自动移动」的传统 MOBA 操作方式，两个模式通过设置面板实时切换，保持当前 WASD 模式完全不受影响。

### Scope

| 纳入 | 不纳入 |
|------|--------|
| C++ Sim 层手写 A* 寻路（均匀网格） | Bot 寻路（Bot 保持撞墙现状） |
| 双控制模式：WASD ↔ MOBA（QWER 技能+右键移动） | hold-to-steer（按住右键持续转向） |
| 设置面板（模式切换 + 退出游戏 + ESC 开关） | 路径线 VFX |
| ConfigFile 持久化模式偏好 | 卡住重寻路 |
| 右键仲裁（施法中仅取消不下达移动） | 导航网格（Recast/NavMesh） |
| S 键停止移动（MOBA "stop"） | 网络状态下的移动（单机原型） |
| 移动目标点 ping VFX | |

---

## 2. 架构决策

### 决策 1：寻路放在 C++ Sim 层（不放 View）

**理由：**

- 项目铁律「Sim 层权威 + Snapshot 唯一通信」。
- 玩家移动权威在 `player_movement.h`，墙壁数据已在 registry（`world.cpp:44-52`）。
- 若在 GDScript View 算路径再反喂 `set_local_input`，会与 `wall_collision.h` 的推回打架、产生 desync。
- 未来网络化时，移动路径必须在 Sim 层权威执行。

**结论：** 右键点击发送"移动指令目标"（目标点坐标），Sim 层接收后调用 A* 计算路径并跟随。

### 决策 2：手写 A*（不引入外部库）

| 方案 | 评估 |
|------|------|
| **手写 A*（推荐）** | 地图仅 100×100 单位，墙体为静态 AABB → 均匀网格 A* 约 120 行；保持 Sim 层零 Godot 依赖（Godot 的 `AStarGrid2D` 是 GDScript API，C++ Sim 用不了）；项目已手写 JSON 解析器（`json_util.h`）风格一致；二叉堆 + 8 方向 + octile 启发，4 万格子单次查询亚毫秒 |
| 引入 C++ 库（micropather/Recast） | 杀鸡用牛刀；增加 CMake 构建依赖；Recast 是导航网格，对 AABB 墙体过度复杂 |
| 用 Godot NavigationServer | 属于 View 层 Godot API，违背 Sim 零依赖原则，且仍需把路径回传 Sim |

**结论：** 手写均匀网格 A* + 视线法（string-pulling）路径平滑。

---

## 3. 双模式控制方案

| | **WASD 模式（默认，当前原型）** | **MOBA 模式（点地板）** |
|---|---|---|
| 移动 | WASD / 方向键 | 右键点地板 → A* 寻路 |
| 技能键 | **C / E / R / F** | **Q / W / E / R** |
| HUD 技能 label | C E R F | Q W E R |
| 瞄准 | 鼠标（自由） | 鼠标（自由） |
| 开火 / 确认 | 左键 | 左键 |
| 右键 | 取消施法（cast_cancel） | 空闲=移动指令；瞄准/施法中=仅取消施法（不下达移动） |
| S 键 | 向后移动（WASD 的一部分） | **停止移动**（清路径） |
| 施法打断 | H（移除 ESC） | H / S（S 有优先含义见下） |
| ESC | 原 cast_interrupt 的一部分 | **打开/关闭设置面板** |
| 模式切换 | — | 设置面板 OptionButton / 存 ConfigFile |

### 模式切换时机

- 设置面板中 OptionButton 调 `GameSettings.set_move_mode()`。
- 切换实时生效，`input_collector` 和 `bottom_hud` 通过 `mode_changed` 信号立即响应。
- 不暂停游戏、不重启 Sim、无需重置玩家位置。

---

## 4. 右键仲裁规则

右键在 MOBA 模式下有双重职责，按以下规则仲裁：

| 当前施法状态 | 右键效果 |
|-------------|---------|
| `cast_state == None`（空闲） | 下达移动指令：`MoveIssue=true`，Sim 算 A* |
| `cast_state == Aiming`（瞄准中） | **仅取消**：`cast_cancel` 触发，取消施法**不下达移动** |
| `cast_state == Casting`（前摇中） | **仅取消**：`cast_cancel` 触发，取消施法不下达移动 |
| `cast_state == Channeling` / `Dashing` | **无效果**（右键在此阶段无定义） |

**Sim 侧落地：**

1. `LocalInputSingleton` 中 `MoveIssue` 随 `set_move_command` 每帧到达 Sim。
2. `player_pathfinding_system` 执行时检查 `cast_state`：
   - 若 `cast_state != None` → 丢弃 `MoveIssue`（不下达移动、不触发 A*）。
   - 若 `cast_state == None` → 执行 A* 填充 `MovePath`。
3. 此时 `skill_cast_system` 尚未执行（`player_pathfinding` 在 `skill_cast` 之前），但 `cast_state != None` 的检查保证了正在施法的 tick 不会下达移动。
4. 施法取消后，`cast_state` 变为 `None`，后续新的右键即可下达移动。

**边沿触发：** `MoveIssue` 由 input_collector 右键 press edge 触发一次；sim_bridge 在帧循环的首个 tick 发送 `issue=true`，同帧 catch-up 后续 tick 发送 `issue=false`，确保一次点击只触发一次 A*。

**清理规则：** `cast_state != None` 时，`player_movement_system` 自动清 `MovePath.Following=false`（玩家施法时停步，符合 MOBA 手感）。

---

## 5. 寻路设计

### 5.1 NavGrid

**源文件：** `src_cpp/sim/nav_grid.h`

```cpp
struct NavGrid {
    float CellSize = 0.5f;           // 格子大小（世界单位）
    int Width = 0;                   // 水平格数
    int Height = 0;                  // 垂直格数
    float OriginX = -50.0f;          // 格子 (0,0) 左下角世界 X
    float OriginY = -50.0f;          // 格子 (0,0) 左下角世界 Y
    std::vector<uint8_t> Blocked;    // 1=阻挡，0=可通行

    // 烘焙
    void build(float map_half, const std::vector<WallBounds>& walls,
               float cell_size, float agent_radius);

    // 查询
    bool is_blocked(int cx, int cy) const;
    bool world_to_cell(Vec2 w, int &cx, int &cy) const;
    Vec2 cell_to_world(int cx, int cy) const;
    Vec2 snap_to_nearest_free(Vec2 w) const;

    // A* 寻路（返回世界坐标路径，已平滑）
    std::vector<Vec2> find_path(Vec2 start, Vec2 goal) const;
};
```

### 5.2 烘焙（`build`）

- 计算格数：`Width = Height = ceil(map_half * 2 / CellSize)`（100 / 0.5 = 200）。
- 标记阻挡：
  - 地图边界外 → 阻挡。
  - 每个 `WallBounds` 按 `Min - agent_radius - margin` 到 `Max + agent_radius + margin` 范围标记对应格子阻挡（`agent_radius = PlayerRadius = 0.5`，`margin = 0.25`，膨胀总量 = 0.75 单位）。
- 调用时机：`World::initialize` 中创建所有墙壁实体之后。

### 5.3 A* 搜索

- **方向：** 8 方向（+ 对角），对角 movement cost = `sqrt(2) ≈ 1.414`。
- **启发函数：** octile distance。
- **数据结构：** 开放集用 `std::priority_queue`（pair `<heuristic+cost, index>`），关闭集用 `vector<int> closed`（格子索引）。
- **结果：** 返回世界坐标路径点列表（含起点和终点，共 N 点）。若终点不可达返回空路径。

### 5.4 路径平滑（string-pulling）

- 遍历路径点，维护一个"当前保留点"。
- 从当前保留点向外试探 `candidate`（从后往前扫描），检查 `current → candidate` 直线是否与任何膨胀墙 AABB 相交。
- 若不相交 → 跳过中间点，`current = candidate`。
- 若相交 → 保留 `candidate` 的前一个点，`current = 前一个点`。
- 结果：消除网格锯齿，路径近似直线转弯，符合 MOBA 走位感。

### 5.5 路径跟随

**组件：** `MovePath { vector<Vec2> Waypoints; int CurrentIndex; bool Following; Vec2 FinalTarget; }`

- 每 tick，`player_movement_system` 检查 `MovePath.Following && cast_state == None && 无 WASD 输入`。
- 朝 `Waypoints[CurrentIndex]` 方向移动 `speed * dt`。
- 进入 `arrive_radius = CellSize * 0.75 ≈ 0.4` 后 `CurrentIndex++`。
- 越过末点或抵近 `FinalTarget` → `Following = false`。
- `FacingAngle` 由移动方向设置（与现有 WASD 逻辑一致）。
- `wall_collision` 仍兜底（推回后的位置由 Sim 自动处理；路径无需重算）。

---

## 6. S 键停止移动

### 动机

MOBA 标准操作（LoL/Dota 的 S = stop）：立刻停步、打断当前移动路径、不触发任何技能。

### 设计

**input_collector.gd（MOBA 模式）：**

- 检测 `Input.is_key_pressed(KEY_S)` 的**按下边沿**（press edge），设为 `stop_pulse = true`。
- S 在 MOBA 模式下不为技能键（技能是 QWER），也不为移动键（移动是右键），完全空闲。

**LocalInputSingleton / PlayerInputState 新增：**

```cpp
bool Stop = false;  // 帧脉冲
```

**SimServer/World 新增方法：**

```cpp
void set_stop(bool stop);
```

**player_movement_system 逻辑（添加到头）：**

```cpp
if (input.Stop) {
    path.Following = false;
    // 不设置 FacingAngle（保持当前朝向）
}
```

**施法打断复用：** S 同时保持 `cast_interrupt` 功能——在施法前摇（Casting）阶段按下 S，打断施法 + 停止移动。

### 键盘冲突说明

- **WASD 模式：** S 是「向后移动」（WASD 的一部分），保持现状。`Stop` 脉冲不由 input_collector 发送。
- **MOBA 模式：** S 不是技能键（QWER），不是移动键（右键移动），可作为独立 stop 键。

---

## 7. 设置面板

### 交互

- **打开：** ESC 键切换 `visible`。
- **不暂停游戏：** `get_tree().paused = false`（Sim 持续 tick）。
- **关闭：** ESC 键 / 面板右上角「×」按钮。

### 面板布局

```
╔══════════════════════════╗
║         设置             ║
║                          ║
║   操作模式: [▼ WASD    ] ║
║              ├ WASD      ║
║              └ 点地板    ║
║                          ║
║   [         退出游戏    ] ║
║                          ║
║              [×] 关闭    ║
╚══════════════════════════╝
```

### ESC 冲突解决

- 当前 `input_collector.gd:73` 中 `cast_interrupt` 包含 `KEY_ESCAPE`。
- 改为：`cast_interrupt` 仅包含 `KEY_H`（WASD 和 MOBA 模式）+ MOBA 模式下加 `KEY_S`。
- 移除 `KEY_ESCAPE` 从 cast_interrupt，ESC 全局归设置面板接管。

### 场景结构

- 新文件 `scripts/ui/settings_panel.gd` + `scenes/ui/settings_panel.tscn`。
- CanvasLayer 独立层，`process_mode = ALWAYS`（即使暂停也不影响，但当前不暂停）。
- `z_index` 高于 HUD 层。
- 挂到 `main.tscn`。

---

## 8. ConfigFile 持久化

### 位置

`user://settings.cfg`

### 读写时机

| 操作 | 时机 |
|------|------|
| **读** | `GameSettings._ready()` |
| **写** | `GameSettings.set_move_mode()` 每次切换模式时 |
| 退出 | 无需额外保存（切换时已写） |

### 实现

```gdscript
# scripts/autoload/game_settings.gd
extends Node

enum MoveMode { WASD, MOBA }
var move_mode: MoveMode = MoveMode.WASD
signal mode_changed(m: MoveMode)

const CFG_PATH := "user://settings.cfg"
const CFG_SECTION := "controls"
const CFG_KEY := "move_mode"

func _ready() -> void:
    var cfg := ConfigFile.new()
    if cfg.load(CFG_PATH) == OK:
        var val = cfg.get_value(CFG_SECTION, CFG_KEY, int(MoveMode.WASD))
        move_mode = val as MoveMode
    # 默认 WASD（文件缺失或读取失败）

func set_move_mode(m: MoveMode) -> void:
    if m == move_mode:
        return
    move_mode = m
    mode_changed.emit(m)
    var cfg := ConfigFile.new()
    cfg.set_value(CFG_SECTION, CFG_KEY, int(m))
    cfg.save(CFG_PATH)
```

### project.godot 注册

```ini
[autoload]
GameSettings="*res://scripts/autoload/game_settings.gd"
```

---

## 9. 移动 ping VFX

### 触发

- `input_collector.gd` 在 MOBA 模式右键边沿发射信号 `move_issued(target: Vector2)`。
- `move_target_vfx.gd` 连接该信号。

### 视觉效果

- 在目标位置显示一个半透明蓝色圆环（`CylinderMesh`，半径 0.3，高 0.05），y 提升 0.025 紧贴地面。
- 0.5 秒内 Z 轴缩小 + alpha 淡出至 0。
- 用 `Tween` 或每帧手动 lerp。

### 复用

- 借用 `skill_vfx.gd` 的 `StandardMaterial3D` 无光照材质 + `CylinderMesh` 模式（已有 AoE 池的类似做法）。
- 独立 Node3D `move_target_vfx.gd`，挂到 `main.tscn`。

### 文件

- `scripts/view/move_target_vfx.gd`（新增）
- 无 tscn（代码创建 MeshInstance3D）

---

## 10. 变更汇总

### 10.1 新增文件

| 文件 | 用途 |
|------|------|
| `src_cpp/sim/nav_grid.h` | NavGrid 结构 + 烘焙 + A* + 平滑 |
| `src_cpp/sim/systems/player_pathfinding.h` | 寻路触发 System（读 MoveIssue → 调 A* → 写 MovePath） |
| `scripts/autoload/game_settings.gd` | 操作模式 autoload 单例 + ConfigFile 持久化 |
| `scripts/ui/settings_panel.gd` | 设置面板逻辑 |
| `scenes/ui/settings_panel.tscn` | 设置面板场景 |
| `scripts/view/move_target_vfx.gd` | 右键 ping 标记 VFX |

### 10.2 修改文件

| 文件 | 改动 |
|------|------|
| `src_cpp/sim/components.h` | `LocalInputSingleton` + `MoveTarget/MoveIssue/Stop`；`PlayerInputState` + 对应字段；新增 `MovePath` 组件 |
| `src_cpp/sim/world.h` | + `NavGrid _nav_grid` 成员；+ `set_move_command()` / `set_stop()` 声明；+ `_build_nav_grid()` |
| `src_cpp/sim/world.cpp` | + `_build_nav_grid()` 实现（墙体烘焙）；+ `set_move_command()` / `set_stop()` 实现；tick 中插 `player_pathfinding_system`；`_spawn_player` emplace `MovePath` |
| `src_cpp/sim/systems/player_movement.h` | + `MovePath` 跟随分支 + Stop 检测 + cast_state!=None 时清 Following |
| `src_cpp/sim/systems/local_input_injection.h` | + 复制 MoveTarget/MoveIssue/Stop 到 PlayerInputState |
| `src_cpp/sim/sim_server.h/cpp` | + `set_move_command(tx, ty, issue)` / `set_stop(stop)` |
| `src_cpp/sim/register_types.cpp` | + 绑定新方法 |
| `scripts/input/input_collector.gd` | 模式感知：MOBA 下 QWER 技能 + 右键 move edge + S=stop + 移除 ESC cast_interrupt；`_read_move` MOBA 下置零；发射 `move_issued` 信号 |
| `scripts/sim_bridge.gd` | 帧首 tick 传 `set_move_command` + `set_stop`；catch-up tick 传 issue=false |
| `scripts/ui/bottom_hud.gd` | `KEY_HINTS` 按模式动态返回；`_on_mode_changed` 刷新 label |
| `scenes/main.tscn` | + settings_panel 实例 + move_target_vfx 实例 |
| `project.godot` | + autoload `GameSettings` |

### 10.3 组件变更

| 组件 | 字段变动 |
|------|---------|
| `LocalInputSingleton` | + `Vec2 MoveTarget; bool MoveIssue; bool Stop;` |
| `PlayerInputState` | + `Vec2 MoveTarget; bool MoveIssue; bool Stop;` |
| **新增** `MovePath` | `vector<Vec2> Waypoints; int CurrentIndex; bool Following; Vec2 FinalTarget;` |

### 10.4 Snapshot 变更

**无。** View 只需玩家位置（已有 `SimPlayerSnap.x/y`），ping 由 View 本地触发不依赖 Sim。

---

## 11. 分阶段实施计划

### 阶段 1 — Sim 寻路基础（C++）

| 步骤 | 文件 | 说明 |
|------|------|------|
| 1.1 | `nav_grid.h` **新增** | NavGrid 结构、烘焙、is_blocked、world_to_cell/cell_to_world、snap_to_nearest_free |
| 1.2 | `nav_grid.h` 续 | `find_path` A* 实现（二叉堆 + 8 方向 + octile）、string-pulling 平滑 |
| 1.3 | `components.h` 修改 | `LocalInputSingleton`/`PlayerInputState` + MoveTarget/MoveIssue/Stop；新增 `MovePath` |
| 1.4 | `player_pathfinding.h` **新增** | `player_pathfinding_system(reg, nav)` |
| 1.5 | `player_movement.h` 修改 | MovePath 跟随 + Stop + cast gate 清 Following |
| 1.6 | `local_input_injection.h` 修改 | 复制新字段 |
| 1.7 | `world.h/cpp` 修改 | `_nav_grid` 成员、`_build_nav_grid`、`set_move_command`/`set_stop`、tick 插 system、`_spawn_player` emplace MovePath |
| 1.8 | `sim_server.h/cpp` / `register_types.cpp` | 绑定新 API |

**验收：** 编译通过，View 层手动发 `set_move_command` 看玩家 A* 移动。

### 阶段 2 — View 输入双模式（GDScript）

| 步骤 | 文件 | 说明 |
|------|------|------|
| 2.1 | `game_settings.gd` **新增** | autoload，move_mode + mode_changed + ConfigFile 读写 |
| 2.2 | `project.godot` 修改 | 注册 autoload |
| 2.3 | `input_collector.gd` 修改 | 读 mode、MOBA 下 WASD=ZERO/SKILL_KEYS=QWER/右键 edge=move_cmd/S=stop/移除 ESC |
| 2.4 | `sim_bridge.gd` 修改 | 帧首 tick set_move_command + set_stop；catch-up 发 issue=false |

**验收：** WASD 模式跟原版完全一致；切 MOBA 模式后右键点地板模拟器打印 MoveTarget。

### 阶段 3 — UI（GDScript + tscn）

| 步骤 | 文件 | 说明 |
|------|------|------|
| 3.1 | `settings_panel.gd` + `.tscn` **新增** | 面板布局 + ESC 切换 + OptionButton + 退出按钮 |
| 3.2 | `bottom_hud.gd` 修改 | KEY_HINTS 按模式返回、连接 mode_changed 刷新 |
| 3.3 | `main.tscn` 修改 | 挂 settings_panel |

**验收：** ESC 开/关面板、模式切换实时生效、HUD label 变 QWER、退出按钮可用。

### 阶段 4 — VFX（GDScript）

| 步骤 | 文件 | 说明 |
|------|------|------|
| 4.1 | `move_target_vfx.gd` **新增** | 接收 input_collector.move_issued → 显示圆环 + 淡出 |
| 4.2 | `main.tscn` 修改 | 挂 move_target_vfx、connect 信号 |
| 4.3 | `input_collector.gd` 加信号 | `signal move_issued(target: Vector2)` |

**验收：** 右键点地板时目标点出现蓝色圆环 0.5s 消失。

### 阶段 5 — 调参与打磨

| # | 项目 | 方法 |
|---|------|------|
| 1 | 网格分辨率 | 试 0.5 / 1.0，选路径顺滑度/性能平衡点 |
| 2 | 墙体膨胀余量 | 试 0.5 / 0.75 / 1.0，选人不会被挤出墙的最小值 |
| 3 | arrive_radius | 试 0.3 / 0.4 / 0.5，选转弯自然的值 |
| 4 | 平滑 aggressiveness | 试 string-pulling 直线检测阈值（可跳过 AABB 半边的最小值） |
| 5 | 右键→移动手感 | 实测 cancel 过渡、连点响应、S 停止反馈 |

---

## 12. Tick 顺序

```
local_input_injection        #  1. 注入输入（含 MoveTarget/MoveIssue/Stop）
↓
player_pathfinding (新)      #  2. 若 MoveIssue && cast_state==None → A* → 写 MovePath
↓
player_movement              #  3. MovePath 跟随 / WASD 移动 / Stop
↓
player_fire
↓
skill_cast
↓
bot_targeting
...
```

原 #2 `player_movement` 提到 #3，新 #2 `player_pathfinding` 插入。

---

## 13. 风险与取舍

| 风险 | 缓解 |
|------|------|
| A* 在极端窄缝（0.5 格）无解 | 墙体膨胀 + `snap_to_nearest_free` 保证起/终点有效 |
| 路径跟随与 wall_collision 推回冲突 | `wall_collision` 已做 AABB 推回（`vec2.h:resolve_circle_aabb`），跟随仅每 tick 改 pos，推回同样修改 pos → Sim 同步状态一致，View 插值平滑 |
| 右键 cancel 与 move 的 1-tick 过渡在手感上"仿佛移动了" | 边沿触发 + player_pathfinding 的 cast_state 检查保证 cancel tick 不下达移动 |
| ConfigFile 文件被手动删除 | `cfg.load` 失败时回退默认 WASD，不抛异常 |
| ESC 切换面板时触发其他 UI | `settings_panel._unhandled_input` 用 `accept()` 阻止事件传递 |

### 明确不做的

- Bot 寻路（后续独立 task）。
- hold-to-steer（后续 GDScript 实现）。
- 路径线 VFX（仅保留 ping）。
- 网络多玩家（单机原型）。

---

## 14. 附录：组件/System/API 变更详情

### 14.1 新增组件

```cpp
struct MovePath {
    std::vector<Vec2> Waypoints;
    int CurrentIndex = 0;
    bool Following = false;
    Vec2 FinalTarget{0.0f};
};
```

### 14.2 扩展组件

```cpp
// LocalInputSingleton
struct LocalInputSingleton {
    Vec2 Move{0.0f};
    Vec2 Aim{0.0f};
    bool Fire = false;
    int Seq = 0;
    int CastSlot = -1;
    bool CastConfirm = false;
    bool CastCancel = false;
    bool CastInterrupt = false;
    Vec2 CastAim{0.0f};
    // ↓ 新增
    Vec2 MoveTarget{0.0f};
    bool MoveIssue = false;
    bool Stop = false;
};

// PlayerInputState（同上增加 MoveTarget/MoveIssue/Stop）
```

### 14.3 新增 System

```cpp
// systems/player_pathfinding.h
namespace sim {
inline void player_pathfinding_system(
    entt::registry &reg, const NavGrid &nav
) {
    auto view = reg.view<
        PlayerTag, Position2D, PlayerInputState, CastState, MovePath>();
    for (auto e : view) {
        auto &tag = view.get<PlayerTag>(e);
        if (!tag.IsLocal) continue;
        auto &input = view.get<PlayerInputState>(e);
        auto &cs = view.get<CastState>(e);
        auto &pos = view.get<Position2D>(e);
        auto &path = view.get<MovePath>(e);
        // 施法中 → 丢弃 move 指令
        if (cs.State != CastState::Phase::None) {
            // 不清 path（玩家可能想 cancel 后继续走）
            // 但清 Following（停止跟随，sync 下条规则）
            path.Following = false;
            continue;
        }
        // 有新的移动指令
        if (input.MoveIssue) {
            auto waypoints = nav.find_path(pos.Value, input.MoveTarget);
            if (!waypoints.empty()) {
                path.Waypoints = std::move(waypoints);
                path.CurrentIndex = 0;
                path.FinalTarget = input.MoveTarget;
                path.Following = true;
            }
        }
    }
}
} // namespace sim
```

### 14.4 新增 SimServer API

```cpp
// sim_server.h
void set_move_command(float target_x, float target_y, bool issue);
void set_stop(bool stop);
// sim_server.cpp → World::set_move_command / World::set_stop
```

### 14.5 修改的 System 逻辑

**`player_movement_system` 新增（按优先级）：**

1. **Stop：** `if (input.Stop) → path.Following = false; return;`（MOBA 模式 S 键，停在原地）
2. **Cast/Status gate：** 已有（Casting/Channeling/Dashing/Root/Stun → continue），新增 `if (!cs.None) path.Following = false;`
3. **WASD：** `if (length(input.Move) > 0.01)` → 移动（已有），新增 `path.Following = false;`
4. **MovePath 跟随：** `if (path.Following)` → 朝 `waypoints[currentIndex]` 移动，索引前进，到达终点后 `Following = false`
5. **Facing：** 分支 3/4 都设置 `angle.Radians = atan2(dir.y, dir.x)`

---

## 15. 高频右键抖动优化（v1.1）

> 2026-07-08 追加：解决 MOBA 模式下快速连点右键导致角色朝向抽搐的问题。

### 15.1 根因

每次右键 → `player_pathfinding` 重算 A* → `CurrentIndex = 0`，第一航点落在玩家当前格
→ 跳到第二航点 → `angle.Radians = atan2(...)` 瞬时赋值。
玩家位置每次点击间微移，A* 从不同起始格出发，第二航点方向可能跟上一帧不同
→ 朝向在两个方向间来回跳 → 抽搐。

### 15.2 三层组合方案

| 层 | 位置 | 治理场景 | 字段 |
|----|------|---------|------|
| A 时间死区 | `input_collector.gd` | 机器枪式连点（<80ms）过滤 | `MIN_MOVE_INTERVAL` |
| B 目标死区 | `player_pathfinding.h` | 同区域连点跳过 A* 重算 | `RepathTargetDeadzone` |
| C 转向速率 | `player_movement.h` | 任何残留方向突变平滑处理 | `PathTurnRate` |

A 是减负，B + C 是核心治理。

### 15.3 Fix A — 时间死区（View）

`input_collector.gd` 右键边沿检测加节流：

```gdscript
const MIN_MOVE_INTERVAL := 0.08  # 秒
var _last_move_time := 0.0

if right_now and not _prev_right:
    var now := Time.get_ticks_msec() / 1000.0
    if now - _last_move_time < MIN_MOVE_INTERVAL:
        pass  # 丢弃，不发脉冲
    else:
        _last_move_time = now
        move_cmd_target = aim_world
        move_cmd_issue = true
        move_issued.emit(move_cmd_target)
```

### 15.4 Fix B — 目标死区（Sim）

`player_pathfinding_system` 重算前比对新旧目标距离：

```cpp
if (input.MoveIssue) {
    bool need_repath = true;
    if (path.Following) {
        Vec2 d = input.MoveTarget - path.FinalTarget;
        if (vec2_length_sq(d) < GameConfig::RepathTargetDeadzoneSq) {
            need_repath = false;
        }
    }
    if (need_repath) {
        auto waypoints = nav.find_path(pos.Value, input.MoveTarget);
        ...
    }
}
```

### 15.5 Fix C — 转向速率（Sim）

`player_movement_system` 中，**仅寻路跟随分支**用角速度限制替代瞬时赋值：

```cpp
inline float angle_diff(float target, float current) {
    float d = target - current;
    while (d > M_PI) d -= 2 * M_PI;
    while (d < -M_PI) d += 2 * M_PI;
    return d;
}

// 替换所有 angle.Radians = atan2(...)（仅 path-following 分支）
auto apply_turn_rate = [&](Vec2 move_dir) {
    float target = std::atan2(move_dir.y, move_dir.x);
    float diff = angle_diff(target, angle.Radians);
    float max_turn = GameConfig::PathTurnRate * dt;
    if (std::abs(diff) > max_turn)
        diff = (diff > 0 ? max_turn : -max_turn);
    angle.Radians += diff;
};
```

位置仍按真实 `dir` 立即移动（方向不变），只有朝向平滑追赶。WASD 分支保持瞬时朝向（双摇杆手感）。

### 15.6 新增常量（game_config.h）

```cpp
static constexpr float RepathTargetDeadzone = 1.5f;
static constexpr float RepathTargetDeadzoneSq = RepathTargetDeadzone * RepathTargetDeadzone;
static constexpr float PathTurnRate = 12.0f;  // rad/s
```
