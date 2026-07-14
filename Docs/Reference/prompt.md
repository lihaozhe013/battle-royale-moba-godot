# 项目设计文档

## 项目概况

- **类型**：大逃杀 MOBA 游戏（当前非指向性射击为占位符，后续按 `MOBA 大逃杀升级方案` 扩展）
- **引擎**：Godot 4.7，C++ GDExtension (Sim 层) + GDScript (View 层)
- **架构**：ECS — C++ Sim (entt::registry) → Snapshot → GDScript View Systems
- **相机**：55° 俯视透视投影，FOV=40，跟随玩家
- **物理帧率**：30Hz（Sim tick rate），视图层 60Hz 插值

---

## 整体架构

```
C++ Sim (entt::registry + 12 systems)
	↓ SimSnapshot (30Hz: players/bots/arrows/pickups)
sim_bridge.gd (系统调度器)
	├─ EntityManager.sync_entities(snap)     → 3D 实体 spawn/despawn + 位置插值
	├─ HealthBarManager.sync_bars(snap)      → 2D 血条数据更新
	├─ StatsPanel.update(snap.players[0])    → HUD 文字
	└─ CameraController.follow_target(...)   → 相机跟随
```

### ECS 对齐

| ECS 概念 | 本项目对应 |
|---------|-----------|
| World | C++ `entt::registry` |
| Systems | C++ 12 systems (movement, combat, spawning…) |
| Components | C++ 21 ECS components |
| Component data | `SimSnapshot`（序列化到 GDScript 层）|
| View Systems | GDScript: `EntityManager`, `HealthBarManager` |
| View Entities | `EntityView` (3D), `HealthBarUI` (2D) |

### 数据流

```
SimSnapshot
  ├→ EntityManager → EntityView       (3D: 位置、旋转、骨骼动画)
  ├→ HealthBarManager → HealthBarUI   (2D: 血条填充、颜色、屏幕位置)
  └→ StatsPanel                       (HUD: 等级、击杀、XP)
```

---

## 血条系统设计

### 设计决策：2D 屏幕空间（Screen-Space Overlay）

**选择 2D 屏幕空间血条**，不使用 Sprite3D 世界空间血条。

| 维度 | 2D 屏幕空间 ✅ | Sprite3D 世界空间 |
|------|--------------|-------------------|
| MOBA 标准 | LoL / DOTA / HotS 均用此方案 | 多用于 ARPG (Diablo / PoE) |
| 像素清晰度 | 像素完美，不受 3D 分辨率影响 | 受 pixel_size 和相机距离影响 |
| 分段线 / 图标 | 容易用 Control + `_draw()` 实现 | 需要 shader 或额外纹理 |
| 穿墙可见 | 天然在 3D 之上（MOBA 硬需求）| 需 `no_depth_test` |
| 位置更新 | 需每帧 `unproject_position` | 自动跟随父节点 |
| 扩展性 | 加 Mana / Shield / 状态图标只需加子节点 | 每个功能需额外 Sprite3D + 材质 |

MOBA 血条需要始终可见（玩家需随时看到所有实体血量），"穿墙可见"是优点而非缺点。

### 组件架构

```
HealthBarManager (Node, main.tscn 子节点)
├── CanvasLayer (layer=10, _ready 中代码创建)
│   └── (HealthBarUI 实例池 — 动态 add_child / 回收)
│
HealthBarUI (Control, health_bar_ui.tscn 预制场景)
├── Background (ColorRect, FULL_RECT 锚点, 深色底)
├── DamageBar (ColorRect, TOP_LEFT 锚点, 黄色延迟条)
└── Fill       (ColorRect, TOP_LEFT 锚点, 阵营色主条)
```

### HealthBarManager（视图系统）

**职责**：
1. 管理 HealthBarUI 对象池（创建/回收/复用）
2. 从 Snapshot 读取 HP / 阵营数据，更新到 HealthBarUI
3. 每帧查询 EntityManager 获取实体插值位置，投影到屏幕，定位 HealthBarUI

**两条更新路径（关注点分离）**：

| 路径 | 频率 | 数据来源 | 职责 |
|------|------|---------|------|
| `sync_bars(snap)` | 30Hz（仅新快照时）| SimSnapshot | HP 数据、阵营、显隐 |
| `_process(delta)` | 60Hz（每帧）| EntityManager → EntityView.global_position | 屏幕坐标投影定位 |

**位置查询路径**：
```
HealthBarManager._process
  → EntityManager.get_entity(id)                        // 查实体
  → EntityView.global_position + Vector3(0, 2.0, 0)     // 头顶偏移
  → Camera3D.unproject_position(world_pos)               // 3D → 2D 投影
  → HealthBarUI.set_screen_position(screen_pos)          // 2D 定位
```

**为什么查询 EntityView 而非直接读 Snapshot 位置？**
- EntityView 做了客户端插值（60Hz lerp），3D 模型位置是平滑的
- 如果血条用 Snapshot 原始位置（30Hz），会与 3D 模型产生视觉错位（抖动）
- 查询 `EntityView.global_position` 保证血条与 3D 模型完全同步

### HealthBarUI（视图组件）

**职责**：纯展示组件 — 接收数据并渲染，不含任何游戏逻辑。

| 方法 | 参数 | 说明 |
|------|------|------|
| `update_hp` | `hp: int, max_hp: int` | 更新 Fill 宽度 + 颜色 |
| `set_team` | `team: int` | 设置阵营色（0=自己绿, 2=敌方红）|
| `set_screen_position` | `pos: Vector2` | 设置 2D 屏幕位置（居中对齐）|
| `reset` | — | 回收到池前重置状态 |
| `_process` | `delta: float` | DamageBar lerp 追赶 Fill |

**HealthBarUI 不知道的事情（解耦边界）**：
- 不知道 Sim / Snapshot / EntityManager 的存在
- 不知道 3D 世界坐标 / 相机
- 只接收：`hp`, `max_hp`, `team`, `screen_position`

### 对象池策略

```
活跃实体出现在快照 → _get_or_create(id)
  ├─ 池中有空闲 HealthBarUI → 取出，设 visible=true
  └─ 池空 → instantiate health_bar_ui.tscn, add_child 到 CanvasLayer

实体从快照消失 → _release_bar(id)
  └─ 设 visible=false, 放回池中

实体死亡 (dead=true) → 隐藏但保留在 _active（实体仍在快照中）
实体复活 → 重新设 visible=true
```

- 池只增不减（达到峰值后不再 instantiate）
- 避免频繁创建/销毁 Control 节点

### 阵营色系统

| 阵营 | Team ID | Fill 颜色 | 当前映射 |
|------|---------|----------|---------|
| 自己 | 0 | `Color(0.2, 1.0, 0.2)` 亮绿 | entity_type=0 (Player) |
| 友方 | 1 | `Color(0.2, 0.6, 1.0)` 蓝 | 未来扩展（多玩家时）|
| 敌方 | 2 | `Color(1.0, 0.3, 0.3)` 红 | entity_type=1 (Bot) |
| 中立 | 3 | `Color(1.0, 0.8, 0.2)` 黄 | 未来扩展（野怪）|

**HP 颜色渐变规则**（MOBA 惯例）：
- **自己/友方**：>60% 阵营色 → 25-60% 黄 → <25% 红
- **敌方**：固定红色，不随 HP 变化（敌方血条始终红色便于识别）

### DamageBar 延迟动画

掉血时 Fill 立即缩短，DamageBar（黄色）用 `move_toward` 缓慢追赶 Fill，营造"掉血延迟"视觉效果。

```
Fill:     立即跳到新 ratio
DamageBar: move_toward(_damage_ratio, _hp_ratio, SPEED * delta)
```

---

## API 契约

### HealthBarManager

```gdscript
class_name HealthBarManager
extends Node

# 由 sim_bridge 在 _ready 中注入
var entity_manager: EntityManager
var health_bar_scene: PackedScene  # preload("res://scenes/ui/health_bar_ui.tscn")

# 由 sim_bridge 在新快照时调用 (30Hz)
func sync_bars(snap: SimSnapshot) -> void

# 自动调用 (60Hz) — 更新所有活跃血条的屏幕位置
func _process(delta: float) -> void
```

### HealthBarUI

```gdscript
class_name HealthBarUI
extends Control

const BAR_WIDTH := 100.0
const BAR_HEIGHT := 10.0
const DAMAGE_LERP_SPEED := 3.0

# 由 HealthBarManager 调用
func update_hp(hp: int, max_hp: int) -> void
func set_team(team: int) -> void
func set_screen_position(screen_pos: Vector2) -> void
func reset() -> void  # 回收前重置

# 自动调用
func _process(delta: float) -> void  # DamageBar lerp
```

### EntityManager（新增方法）

```gdscript
func get_entity(id: int) -> EntityView:
	return _entities.get(id)
```

### sim_bridge.gd（集成点）

```gdscript
# 新增 @onready
@onready var health_bar_manager = $HealthBarManager

# _ready 末尾新增
health_bar_manager.entity_manager = entity_manager

# _process 中, sync_entities 后新增
if last_snapshot.seq != _last_snap_seq:
	_last_snap_seq = last_snapshot.seq
	entity_manager.sync_entities(last_snapshot)
	health_bar_manager.sync_bars(last_snapshot)  # ← 新增
```

---

## 文件结构

```
scripts/
├── sim_bridge.gd                   # 系统调度器
├── view/
│   ├── entity_manager.gd           # 3D 实体管理（含 get_entity）
│   ├── entity_view.gd              # 3D 实体视图（插值 + 骨骼动画）
│   └── camera_controller.gd        # 相机控制
├── ui/
│   ├── health_bar_manager.gd       # 血条视图系统（对象池 + 屏幕投影）
│   ├── health_bar_ui.gd            # 血条视图组件（Fill/DamageBar）
│   └── stats_panel.gd              # HUD 面板

scenes/
├── main.tscn                       # 主场景（含 HealthBarManager 节点）
└── ui/
	└── health_bar_ui.tscn          # 血条预制场景
```
---

## HealthBarUI 场景布局

```
HealthBarUI (Control)
  custom_minimum_size = Vector2(100, 10)
  mouse_filter = MOUSE_FILTER_IGNORE
  │
  ├── Background (ColorRect)
  │   anchors_preset = PRESET_FULL_RECT
  │   color = Color(0.1, 0.1, 0.1, 0.8)
  │   mouse_filter = MOUSE_FILTER_IGNORE
  │
  ├── DamageBar (ColorRect)
  │   anchors_preset = PRESET_TOP_LEFT
  │   position = Vector2(0, 0)
  │   size = Vector2(100, 10)
  │   color = Color(1.0, 0.8, 0.0)
  │   mouse_filter = MOUSE_FILTER_IGNORE
  │
  └── Fill (ColorRect)
	  anchors_preset = PRESET_TOP_LEFT
	  position = Vector2(0, 0)
	  size = Vector2(100, 10)
	  color = Color(0.2, 1.0, 0.2)   # 由 set_team() 运行时覆盖
	  mouse_filter = MOUSE_FILTER_IGNORE
```

**Fill/DamageBar 宽度由脚本设置**：
```gdscript
_fill.size = Vector2(BAR_WIDTH * _hp_ratio, BAR_HEIGHT)
```
TOP_LEFT 锚点确保左边缘固定，宽度缩放时从右往左缩。

---

## 扩展路线图

### Phase 1（已完成）：基础血条
- Background + DamageBar + Fill（3 个 ColorRect）
- 阵营色（自己=绿，敌方=红）
- DamageBar 延迟动画
- 对象池
- 屏幕坐标定位（unproject_position）

### Phase 2+：MOBA 完整升级
- 完整升级方案见下方 `MOBA 大逃杀升级方案`
- 优先级：Mana → 技能系统 → 指示器 → 技能栏 → 大逃杀机制 → 装备 → 小地图/迷雾 → 多英雄

---

## GDScript 约束（必须遵守）

Godot 4 中 `Vector2` / `Vector3` / `Rect2` 属性返回副本，子字段赋值不生效：

```
# ❌ 禁止
node.scale.x = val
node.position.x = val
control.size.x = val
sprite.region_rect.size.x = val
global_rotation.y = val

# ✅ 必须整体赋值
node.scale = Vector3(x, y, z)
node.position = Vector3(x, y, z)
control.size = Vector2(x, y)
sprite.region_rect = Rect2(x, y, w, h)
rotation = Vector3(x, y, z)
# 或用 look_at() 设置旋转
```

---

## MOBA 大逃杀升级方案

> 评估日期：2026-07-07
> 关联：`Docs/Reference/bot_ai_optimization.md`、`Docs/Reference/godot-editor-todo.md`、`Docs/Reference/sim_system_reference.md`

### 现有能力盘点

| 系统 | 可用于MOBA | 说明 |
|------|-----------|------|
| 右键点地板移动 + 鼠标瞄准 | ✅ | 单一 MOBA 模式，瞄准已投影到地面平面 |
| 非指向型普攻（箭矢） | ✅ | 保留为 Q 技能或右键技能占位 |
| HP 系统 + 血条 | ✅ | 后续需加 ManaBar / ShieldBar |
| 等级/XP/成长 | ✅ | 保留，扩展技能点分配 |
| Pickup 拾取 | ✅ | 扩展为装备/消耗品体系 |
| Bot AI（决策树） | ✅ | 后期扩展使用技能的 AI |
| 3D 实体 + 60Hz 插值 | ✅ | 保留 |
| **玩家死亡逻辑** | ❌ | **当前完全缺失** |
| **游戏结束条件** | ❌ | **当前无限运行，bot 无限重生** |

---

### 一、法力系统（Mana）

引入法力作为技能资源，与 HP 并列。

#### Sim 层

**新增组件（`components.h`）：**
```cpp
struct Mana {
    float Cur = 0.0f;
    float Max = 100.0f;
    float RegenRate = 5.0f;  // 每秒回复
    float RegenDelay = 3.0f; // 使用后延迟恢复
    float RegenTimer = 0.0f;
};
```

**新增 System（`systems/mana_regen.h`）：**
- 每 tick 恢复 Mana：
  - 若 `Mana.RegenTimer <= 0`：`Mana.Cur = min(Mana.Cur + Mana.RegenRate * dt, Mana.Max)`
  - 若使用技能（消耗 Mana）：`Mana.RegenTimer = Mana.RegenDelay`
  - 每 tick `Mana.RegenTimer -= dt`

**Snapshot 扩展（`snapshot_types.h`）：**
- `SimPlayerSnap` / `SimBotSnap` 新增 `mana / max_mana`

#### View 层

**HealthBarUI 扩展：**
```
HealthBarUI (Control)
├── LevelBadge / LevelLabel          -- 已有
├── Background / DamageBar / Fill    -- 已有（HP 条）
└── ManaBar (ColorRect)              -- 新增，蓝色，HP 下方
    anchors_preset = TOP_LEFT, offset = (24, 13, 124, 17)
    color = Color(0.2, 0.4, 1.0, 0.8)
```
- 新增 `update_mana(mana, max_mana)` 方法
- Mana 无 DamageBar，无阵营色，固定蓝色

#### 涉及文件

| 层级 | 文件 | 改动 |
|------|------|------|
| C++ | `components.h` | 新增 `Mana` 组件 |
| C++ | `systems/mana_regen.h` | 新建 |
| C++ | `game_config.h` | 新增 Mana 相关常量 |
| C++ | `world.cpp` | Player/Bot 出生时 emplace `Mana` |
| C++ | `snapshot_types.h` | SimPlayerSnap/SimBotSnap 加 mana/max_mana |
| C++ | `snapshot_bindings.cpp` | 注册新属性 |
| C++ | `snapshot_builder.cpp` | 导出新字段 |
| C++ | `world.h` | 添加 tick 顺序中的 mana_regen |
| GDScript | `health_bar_ui.tscn` | 新增 ManaBar 子节点 |
| GDScript | `health_bar_ui.gd` | 新增 `update_mana` / `ManaBar` 引用 |
| GDScript | `health_bar_manager.gd` | sync_bars 传 mana 数据 |
| GDScript | `stats_panel.gd` | 可选加 Mana 显示 |

---

### 二、技能系统（Skill System）

当前战斗单一（箭矢=普攻），需重构为「技能驱动，普攻只是技能之一」的通用模型。

#### 2.1 技能定义与数据模型

**技能模板定义：**
```cpp
enum class SkillTargetType : uint8_t {
    Skillshot,     // 非指向弹道（类似当前箭矢）
    Targeted,      // 锁定目标
    AoEGround,     // 地面范围
    SelfBuff,      // 自身增益
    Dash,          // 位移
};

struct SkillDef {
    int Id;
    SkillTargetType TargetType;
    float Range;
    float Cooldown;
    float ManaCost;
    float CastTime;       // 前摇
    float ChannelTime;    // 引导时间（0=无需引导）
    float Damage;
    float AoERadius;      // AoE 范围
    float ProjectileSpeed;
    float Duration;       // Buff/Debuff 持续
    float EffectValue;    // Buff 数值 / Dash 距离
    bool InterruptOnMove;
};
```

#### 2.2 新增组件

```cpp
struct SkillSlot {
    int SkillId = 0;            // 技能模板 ID, 0=未分配
    int Level = 0;              // 技能等级 (0-5)
    float CooldownTimer = 0.0f; // 当前冷却剩余
    float MaxCooldown = 0.0f;   // 最大冷却
};

struct SkillComponent {
    SkillSlot Slots[4];  // Q=0, W=1, E=2, R=3
};

struct CastState {
    bool IsCasting = false;
    int SkillSlot = -1;     // 正在施放的技能槽
    float CastTimer = 0.0f; // 施法进度
    Vec2 AimPosition;       // 技能瞄准目标位置
    int TargetEntityId = 0; // 锁定目标ID（Targeted 类型）
};

struct SkillPoints {
    int Available = 0;  // 未分配的技能点
};
```

#### 2.3 新增 Systems

| System | 文件 | 职责 |
|--------|------|------|
| `SkillInputSystem` | `systems/skill_input.h` | 从 LocalInputSingleton 读取技能按键（Q/W/E/R），触发 `CastState` |
| `SkillCastSystem` | `systems/skill_cast.h` | 管理施法流程：前摇阶段 → 释放时创建技能效果 → 进入冷却 |
| `SkillEffectSystem` | `systems/skill_effect.h` | 执行技能效果（生成投射物 / AoE 区域 / 冲撞检测 / 添加 Buff）|
| `SkillCooldownSystem` | `systems/skill_cooldown.h` | 每 tick 递减 CooldownTimer |
| `SkillLevelSystem` | `systems/skill_level.h` | 升级时分配技能点，处理技能升级属性成长 |

#### 2.4 技能效果实体 — 泛化投射物系统

当前仅有 `ArrowTag`，需扩展：

```cpp
enum class ProjectileType : uint8_t {
    BasicAttack,  // 普攻弹道
    Skillshot,    // 技能弹道
    Missile,      // 追踪弹道
};

struct ProjectileTag {
    int OwnerId;
    int SkillId;
    float Damage;
    ProjectileType Type;
    entt::entity HomingTarget; // 追踪目标（Missile 类型）
};

struct AoETag {
    int OwnerId;
    int SkillId;
    float Radius;
    float Duration;    // 持续时间
    float Timer;       // 剩余时间
    float TickRate;    // 伤害间隔
    float TickTimer;
    float Damage;
    Position2D;        // 固定位置
};
```

将 `arrow_movement.h` 泛化为 `projectile_movement.h`，同时处理 Arrow + Skillshot + Missile 弹道。

#### 2.5 输入扩展

`input_collector.gd` 增加技能键：
- Q / W / E / R 映射到 `skill[4] bool`
- 技能瞄准位置 = 鼠标世界坐标（与普攻共用 aim_world）
- 锁定目标 = 鼠标下方最近的 Damageable 实体（Targeted 类型专用）

`LocalInputSingleton` 扩展：
```cpp
struct LocalInputSingleton {
    // … 已有 move / aim / fire
    bool SkillQ = false;
    bool SkillW = false;
    bool SkillE = false;
    bool SkillR = false;
    int SkillSlotPressed = -1;  // 0-3, -1=无
    int SkillTargetEntityId = 0; // Targeted 类型时的目标
};
```

#### 2.6 Snapshot 扩展

新增 `SimSkillSlotSnap` 与 `SimAoESnap`：
```cpp
class SimSkillSlotSnap : public godot::RefCounted {
    int skill_id = 0;
    int level = 0;
    float cooldown = 0.0f;
    float max_cooldown = 0.0f;
};

// SimPlayerSnap 增加
SimSkillSlotSnap **skills;  // 4 slots
int skill_points = 0;

// SimAoESnap — 供 View 层渲染 AoE 指示器
class SimAoESnap {
    int id; float x, y; float radius; float duration;
};
```

#### 涉及文件

| 层级 | 文件 | 改动 |
|------|------|------|
| C++ | `components.h` | 新增 SkillSlot / SkillComponent / CastState / SkillPoints / ProjectileTag / AoETag |
| C++ | `game_config.h` | 技能常量定义表 |
| C++ | `systems/skill_input.h` | 新建 |
| C++ | `systems/skill_cast.h` | 新建 |
| C++ | `systems/skill_effect.h` | 新建 |
| C++ | `systems/skill_cooldown.h` | 新建 |
| C++ | `systems/skill_level.h` | 新建 |
| C++ | `systems/projectile_movement.h` | 从 `arrow_movement.h` 重写泛化 |
| C++ | `systems/player_fire.h` | 改为通过技能系统触发普攻 |
| C++ | `systems/bot_combat.h` | 后期改为技能调用 |
| C++ | `systems/combat.h` | 扩展 AoE 碰撞检测 |
| C++ | `snapshot_types.h` | SimSkillSlotSnap / SimAoESnap |
| C++ | `snapshot_bindings.cpp` | 注册新类型 |
| C++ | `snapshot_builder.cpp` | 导出技能快照 |
| C++ | `world.cpp` | 注册新 system |
| GDScript | `input_collector.gd` | 新增 Q/W/E/R 映射 |
| GDScript | `entity_view.gd` | 新增 AoE / 技能投射物视觉 |
| GDScript | `entity_manager.gd` | 新增实体类型 |

---

### 三、施法指示器（Cast Indicator）

纯 View 层，无 Sim 依赖。

#### 3.1 指示器类型

| 类型 | 渲染方式 | 触发条件 |
|------|---------|---------|
| **范围圈** | 3D 半透明圆环（`MeshInstance3D` + `CylinderMesh` 或 `CSGPolygon`） | 按下技能时显示在角色脚底，半径 = SkillDef.Range |
| **方向线** | 从角色到鼠标位置的线段或箭头 | Skillshot 类型技能按下时 |
| **AoE 预览** | 鼠标位置的半透明圆/矩形（绿色=可释放，红色=超出范围）| AoEGround 类型技能瞄准时 |
| **目标锁定框** | 目标实体脚下圆圈或高亮边框 | Targeted 类型技能选目标 |
| **施法进度条** | 角色头顶细长条（类似血条）| 前摇阶段（CastTime > 0） |
| **鼠标指针** | 技能激活时替换为对应光标 | 技能按下后到释放前 |

#### 3.2 架构

```
scripts/view/
├── indicator_manager.gd      -- 指示器管理器
├── range_indicator.gd         -- 范围圈
├── aoe_preview_indicator.gd   -- AoE 预览
├── direction_indicator.gd     -- 方向线
└── cast_bar_ui.gd             -- 施法条 UI（CanvasLayer）

scenes/view/
└── indicators/                -- 指示器 3D 预制体
```

`indicator_manager.gd` 职责：
- 监听 `input_collector` 的技能按键
- 根据技能类型创建/更新/销毁对应指示器
- 每帧更新指示器位置（鼠标移动/技能范围变更）

#### 涉及文件

| 层级 | 文件 | 改动 |
|------|------|------|
| GDScript | `scripts/view/indicator_manager.gd` | 新建 |
| GDScript | `scripts/view/range_indicator.gd` | 新建 |
| GDScript | `scripts/view/aoe_preview_indicator.gd` | 新建 |
| GDScript | `scripts/view/direction_indicator.gd` | 新建 |
| GDScript | `scripts/ui/cast_bar_ui.gd` | 新建 |
| GDScript | `scenes/main.tscn` | 添加 IndicatorManager 节点 |
| GDScript | `scripts/input/input_collector.gd` | 扩展 emit 技能按下信号 |

---

### 四、技能栏 UI（Skill Bar HUD）

底部居中 HUD，玩家操作核心入口。

#### 场景结构

```
SkillBarHUD (CanvasLayer, layer=100)
└── SkillBarContainer (HBoxContainer, bottom center)
    ├── SkillSlotUI_Q    (Button/TextureRect)
    │   ├── Icon         (技能图标)
    │   ├── CooldownMask (ColorRect)   -- 冷却遮罩
    │   ├── LevelLabel   (Label)       -- 技能等级角标
    │   └── KeyHint      (Label)       -- "Q" 提示
    ├── SkillSlotUI_W    (同上)
    ├── SkillSlotUI_E    (同上)
    └── SkillSlotUI_R    (同上)
```

#### 组件方法

```gdscript
class_name SkillSlotUI extends Control

var slot_index: int  # 0=Q, 1=W, 2=E, 3=R

func set_skill(skill_id: int, level: int) -> void
func set_cooldown(ratio: float) -> void  # 0.0=ready, 1.0=full CD
func set_cooldown_text(seconds: float) -> void
func set_mana_state(enough: bool) -> void
func set_level(lv: int) -> void
func reset() -> void
```

#### 技能点分配界面

升级时每个技能槽上方浮现 "+" 按钮：

```
SkillPointIndicator (Control)
├── "+" Button × 4    -- 每个技能槽上方
└── PointsLabel        -- "可用技能点: 2"
```

#### 涉及文件

| 文件 | 改动 |
|------|------|
| `scripts/ui/skill_bar_hud.gd` | 新建 |
| `scripts/ui/skill_slot_ui.gd` | 新建 |
| `scenes/ui/skill_bar_hud.tscn` | 新建 |
| `scripts/ui/stats_panel.gd` | 合并/嵌入技能栏区域 |
| `scripts/sim_bridge.gd` | 桥接技能快照到 SkillBarHUD |

---

### 五、冷却系统（Cooldown）

#### Sim 层

`SkillSlot` 已含 CooldownTimer / MaxCooldown，`SkillCooldownSystem` 每 tick 递减：
```cpp
auto &slot = reg.get<SkillComponent>(e).Slots[i];
slot.CooldownTimer = max(0.0f, slot.CooldownTimer - dt);
```

释放技能时设 CooldownTimer：
```cpp
slot.CooldownTimer = slot.MaxCooldown * (1.0f - cdr); // CDR = 冷却缩减
```

#### View 层

`SkillSlotUI.set_cooldown(ratio)` 中：
- ratio = slot.CooldownTimer / slot.MaxCooldown
- CooldownMask.width = Icon.width × ratio
- 若 ratio > 0，叠加半透明黑色遮罩 + 中央倒计时文本
- ratio 转为 0 时闪烁提示

---

### 六、大逃杀核心机制（Battle Royale）

从竞技场到 BR 的本质差异。

#### 6.1 缩圈系统（Safe Zone / Storm）

**新增组件（`components.h`）：**
```cpp
struct SafeZone {
    Vec2 Center{0.0f};
    float CurrentRadius = 50.0f;
    float TargetRadius = 10.0f;
    float ShrinkSpeed = 2.0f;
    float DamagePerTick = 2.0f;
    float WaitTime = 60.0f;
    float ShrinkTime = 30.0f;
    float WaitTimer = 0.0f;
    float ShrinkTimer = 0.0f;
    int Phase = 0;
};
```

**新增 Systems（`systems/safe_zone.h`）：**
- `SafeZoneShrinkSystem`：定时缩圈，分阶段：等待 → 缩圈 → 等待…
- `SafeZoneDamageSystem`：对圈外 Damageable 实体造成伤害

**View 层：**
- 安全区视觉：地面半透明圆柱墙（`MeshInstance3D` + `CylinderMesh`）
- 下一阶段预览：白色虚线圈（缩圈前提示）
- 安全区边缘闪烁警告（毒圈来临前）
- 玩家 HUD：距安全区距离 + 方向指示器

**Snapshot 扩展：**
```cpp
class SimZoneSnap : public godot::RefCounted {
    float center_x, center_y;
    float current_radius, target_radius;
    float next_radius;
    int phase;
    bool is_shrinking;
};
```

#### 6.2 地图扩展

- 当前地图 100×100（`MapHalf=50`）→ 扩展至 500×500+
- 多区域命名（POI），每个区域有不同形状的墙/建筑群
- 地形元素：
  - 草丛（`BushTag`）：进入后对视野外敌人不可见
  - 水域（`WaterTag`）：减速效果
  - 高低地（`HeightTag`）：高地远程攻击增伤/减伤
- 地图数据格式从 `default.json` 扩展，新增 region / terrain layers

#### 6.3 生死规则

**玩家死亡逻辑（当前完全缺失）：**
- 玩家 `Dead` 组件标记为 true 时触发淘汰
- 死亡状态持续到游戏结束（BR 模式下无重生）
- 死亡后切换为观战模式（或回到大厅）

**新增组件：**
```cpp
struct Eliminated {
    bool Value = false;
};

struct SurvivingCount {
    int Count;
};
```

**新增 System：**
- `GameOverSystem`：检测存活数量 ≤ 1 → 触发游戏结束
- 游戏结束事件写入 `SimEventSnap`

**View 层：**
- 存活人数 HUD（右上角 `"存活: 12/20"`）
- 淘汰提示（屏幕中央 `"玩家A 淘汰了 玩家B"`）
- 游戏结束画面（胜利 / 失败）
- 死亡观战（自由视角或跟随存活实体）

#### 6.4 出生

- 所有玩家从地图边缘或随机点出生
- 无固定出生点，地图随机分布

---

### 七、装备/物品系统（Item System）

大逃杀需求局内装备成长，当前 Pickup 系统过于简单。

#### 7.1 物品定义

```cpp
enum class ItemType : uint8_t {
    Weapon,
    Armor,
    Consumable,
    PassiveMod,
};

struct ItemDef {
    int Id;
    ItemType Type;
    StringName Name;
    float AtkBonus = 0.0f;
    float AspBonus = 0.0f;
    float ManaBonus = 0.0f;
    float SpeedBonus = 0.0f;
    float CDRBonus = 0.0f;
    int ActiveSkillId = 0;
};
```

#### 7.2 组件扩展

```cpp
struct Inventory {
    int Items[6];           // ItemId 数组，0=空
    int Count;
};

struct EquipmentBonuses {
    float AtkBonus;
    float AspBonus;
    float ManaBonus;
    float SpeedBonus;
    float CDRBonus;
};
```

#### 7.3 Systems

- `ItemSpawnSystem`：地面物品生成（开局全图撒装备 + 空投盒）
- `ItemPickupSystem`：拾取 → 存入 Inventory → 更新 EquipmentBonuses
- `ItemDropSystem`：丢弃/替换
- `ItemPassiveSystem`：每 tick 应用被动效果（装备属性修正）

#### 7.4 View 层

- 物品栏 HUD（技能栏旁 6 格）
- 地面物品 3D 模型（箱子/光柱/图标）
- 悬停提示框（物品属性详情）
- 拾取按钮提示（按 F 拾取）

---

### 八、视野与战争迷雾（Fog of War）

#### Sim 层

**新增组件（`components.h`）：**
```cpp
struct Vision {
    float Radius = 20.0f;
    std::vector<int> VisibleEntityIds;
};

struct FogReveal {
    float Radius;
    Vec2 Center;
};
```

**新增 System（`systems/vision_system.h`）：**
- 计算每个实体视野范围内的其他实体
- 写入 `VisibleEntityIds`
- 不可见实体不在 snapshot 中输出
- 墙遮挡计算（射线检测或 NavMesh）

#### View 层

- **战争迷雾渲染**：
  - `CanvasLayer` + 黑色 `ColorRect` 覆盖全屏
  - 用 `_draw()` 或 shader 在可见区域挖透明洞
- **不可见实体**不创建 `EntityView`

---

### 九、小地图（Minimap）

#### View 层

```
Minimap (CanvasLayer, 右上角, 200×200)
├── Background (ColorRect, 半透明黑)
├── MapDisplay (TextureRect, 俯视地图渲染)
│   ├── PlayerDot (标记玩家位置)
│   ├── EnemyDot × N (标记可见敌方)
│   ├── ZoneCircle (安全区范围)
│   └── PickupDot × N (标记拾取物)
└── Border (ColorRect, 外框)
```

**实现方案：**
- 推荐 `_draw()` 手动绘制（轻量），数据来源：
  - 玩家位置：`snapshot.players[0].x/y`
  - 安全区：`SimZoneSnap`
  - 可见敌方：通过 EntityManager 查询
- 点击小地图 → 移动相机到对应位置

---

### 十、战斗反馈系统

| 功能 | 实现层 | 说明 |
|------|--------|------|
| **伤害数字** | View | 浮动数字从受击实体头顶弹出，颜色=伤害类型 |
| **击杀通知** | View | 右上角滚动消息 |
| **技能命中特效** | View | 粒子 + 屏幕震动 + 闪光 |
| **音效** | View | 通过 SimEventSnap 触发 |
| **连杀提示** | View | "Double Kill!" 等横幅 |

**SimEventSnap 扩展：**
```cpp
enum class EventType : uint8_t {
    Kill,
    Damage,
    SkillCast,
    ItemPickup,
    LevelUp,
    ZoneDamage,
};

class SimEventSnap {
    EventType type;
    int killer_id, victim_id;
    int value;          // 伤害量 / 经验量
    int skill_id;
    int x, y;
};
```

---

### 十一、多英雄/职业系统

#### Sim 层

```cpp
struct HeroTag {
    int HeroId;
};

struct HeroDef {
    int Id;
    StringName Name;
    float BaseHp, BaseMana, BaseAtk, BaseAsp, BaseSpeed;
    int SkillIds[4];
};
```

#### View 层

- 英雄选择界面（大厅场景）
- 不同英雄不同 3D 模型 / 配色
- 英雄特定技能图标

---

### 十二、其他必要补完

| 功能 | Sim/View | 说明 |
|------|----------|------|
| **护盾系统** | Sim | `Shield` 组件（Cur/Max），伤害先扣盾后扣血 |
| **护盾血条** | View | Fill 右侧延伸白色段 |
| **状态效果** | Sim | `StatusEffect` 组件（Stun/Slow/Silence/Burn），DurationTimer |
| **状态图标** | View | 血条上方图标行 |
| **玩家死亡** | Sim | 玩家死亡后触发淘汰流程 |
| **草丛隐身** | Sim | 实体进入 BushArea → Stealth 状态 |
| **草丛视觉** | View | 3D 草丛 + 进入后透明度变化 |

---

### 优先级建议

```
P0 — 核心闭环（先能玩）:
  1. Mana 系统（Sim + View）                  — ~12 文件，小
  2. 技能系统框架（1 个 skillshot 跑通全流程） — ~20 文件，大
     (按键→前摇→释放→效果→冷却→UI)
  3. 施法指示器（方向线 + AoE 圆）             — ~5 文件，中
  4. 技能栏 UI                                — ~5 文件，中
  5. 玩家死亡 + 淘汰逻辑                       — ~6 文件，小
  6. 缩圈系统（基础 2 阶段）                   — ~8 文件，中

P1 — 局内体验:
  7. 多技能类型（Targeted/AoE/Buff/Dash）      — ~6 文件，中
  8. 装备/物品系统                            — ~12 文件，大
  9. 小地图                                   — ~4 文件，中
  10. 战争迷雾                                 — ~6 文件，大
  11. 伤害数字 + 击杀通知                      — ~4 文件，中

P2 — 完整 MOBA:
  12. 多英雄/职业                              — ~6 文件，中
  13. 状态效果系统（Stun/Slow/Silence 等）      — ~5 文件，中
  14. 护盾系统                                 — ~5 文件，小
  15. 草丛/隐身                                — ~4 文件，中
  16. 地图扩展（500×500 + POI + 地形）         — ~5 文件，大
```

### 最大架构挑战

**技能系统**是核心重构——当前整个战斗模型是「单一箭矢=普攻」，四个紧耦合系统（`player_fire.h` → `arrow_movement.h` → `wall_collision.h` → `combat.h`）都直接面向箭矢工作。需重构为分层架构：

```
当前:
  player_fire → arrow_movement → wall_collision → combat (arrow→hp)
  bot_combat  ↗

目标:
  skill_cast → projectile_movement (泛化) → wall_collision → combat
  player_fire → skill_effect (作为普攻技能) ↗
  bot_combat  ↗
```

重构关键点：
1. `player_fire.h` 变成 `skill_cast.h` 的特例（普攻 = SkillId=0 的 skillshot）
2. `arrow_movement.h` 泛化为 `projectile_movement.h`，支持不同速度/跟踪/碰撞
3. `combat.h` 扩展 AoE 区域伤害检测（Circle-Circle 改为 Circle-Rect 等）
4. 新类型技能效果（Buff/Dash）不走 `combat.h`，走 `skill_effect.h` 直接写组件
