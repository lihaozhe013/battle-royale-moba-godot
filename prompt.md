# 项目设计文档

## 项目概况

- **类型**：类 MOBA 游戏（当前非指向性射击为占位符，后续扩展为 MOBA 技能系统）
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
│   ├── entity_manager.gd           # 3D 实体管理（新增 get_entity 方法）
│   ├── entity_view.gd              # 3D 实体视图（移除旧血条代码）
│   └── camera_controller.gd        # 相机控制
├── ui/
│   ├── health_bar_manager.gd       # 血条视图系统（新建）
│   ├── health_bar_ui.gd            # 血条视图组件（新建）
│   └── stats_panel.gd              # HUD 面板（已有）

scenes/
├── main.tscn                       # 主场景（新增 HealthBarManager 节点）
└── ui/
    └── health_bar_ui.tscn          # 血条预制场景（新建）
```

### 需要清理的旧文件

| 文件 | 操作 | 原因 |
|------|------|------|
| `scripts/view/health_bar.gd` | **删除** | 旧 Sprite3D 方案，已被 2D UI 替代 |
| `scenes/entities/health_bar.tscn` | **删除** | 旧 Sprite3D 场景 |
| `scripts/view/entity_view.gd` | **移除血条代码** | 血条由 HealthBarManager 独立管理，不再挂在 EntityView 上 |

### entity_view.gd 需要移除的代码

以下 3 处是上一轮 Sprite3D 方案加的，需要移除：

1. `var _hp_bar: HealthBar = null` — 删除变量
2. `_ready()` 中 `if entity_type == 0 or entity_type == 1: _hp_bar = find_child(...)` — 删除
3. `apply_snapshot()` 中两处 `if _hp_bar: _hp_bar.update_hp(hp, max_hp)` — 删除

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

### Phase 1（当前）：基础血条
- Background + DamageBar + Fill（3 个 ColorRect）
- 阵营色（自己=绿，敌方=红）
- DamageBar 延迟动画
- 对象池
- 屏幕坐标定位（unproject_position）

### Phase 2：HP 分段线
- 在 Fill 上叠加分段标记（每 100/200 HP 一条深色线）
- 用 `Control._draw()` 自定义绘制
- 分段数 = `ceil(max_hp / segment_size)`

### Phase 3：资源条（Mana）
- HP 下方加 ManaBar（100×4，蓝色填充）
- 需要 Sim 层添加 `mana` / `max_mana` 到快照

### Phase 4：等级徽章 + 护盾 + 状态图标
- 等级徽章：HP 左侧小圆形 + Label
- 护盾条：Fill 右侧延伸，白色/紫色
- 状态图标：buff/debuff 图标行
- 需要 Sim 层添加 `shield` / `level` / `status_effects` 到快照

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

## 新会话提示词

```
# 任务：实现 2D 屏幕空间血条系统（MOBA 风格）

## 项目位置
C:\Users\Li Haozhe\Documents\dev\topdown-shooter-godot

## 上下文
类 MOBA 游戏。C++ GDExtension Sim 层已完成，GDScript View 层已完成。
血条数据已到位：entity_manager.gd 已传 hp/max_hp 到 apply_snapshot()。
需要新建 HealthBarManager + HealthBarUI，用 2D 屏幕空间方案。

## 完整设计文档
见 prompt.md — 包含架构、API 契约、文件结构、场景布局、扩展路线图。

## 编辑器步骤
见 godot-editor-todo.md — Phase 1 血条系统部分。

## 核心原则
1. HealthBarManager 是独立视图系统，读 Snapshot 更新 HP 数据，查 EntityManager 获取插值位置
2. HealthBarUI 是纯展示组件，不知道 Sim/Snapshot/3D 世界
3. 对象池管理，不频繁创建/销毁
4. 两条更新路径：sync_bars(30Hz 数据) + _process(60Hz 定位)
5. 阵营色：自己=绿，敌方=红
```
