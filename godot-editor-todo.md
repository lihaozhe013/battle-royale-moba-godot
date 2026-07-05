# Godot 编辑器待办事项

> 2026-07-05
> C++ Sim 层 + GDScript 视图层已完成，但视觉是程序化生成的占位。
> 以下是需要在 Godot 编辑器里完成的事项，按优先级排序。

---

## P0 — 必须在编辑器里做（否则游戏不完整）

### 1. 重新保存 main.tscn

当前 `scenes/main.tscn` 是手写的文本文件。首次在编辑器里打开后：

- 编辑器可能会提示"重新保存场景"
- 保存一次让它生成 `.uid` 文件
- 确认节点树结构：

```
Main (Node, sim_bridge.gd)
├─ InputCollector (Node, input_collector.gd)
├─ CameraController (Node3D, camera_controller.gd)
│  └─ Camera3D (正交, y=75 俯视)
├─ EntityManager (Node, entity_manager.gd)
└─ CanvasLayer
   └─ StatsPanel (Control, stats_panel.gd)
      └─ Label
```

### 2. 加灯光 + 环境

当前场景没有光源，Mesh 会全黑。

**在 main.tscn 里添加：**
- `DirectionalLight3D` — 朝下斜照，模拟太阳光
- `WorldEnvironment` — 带 `Environment` 资源，设 ambient light + 天空盒
- 或简化：只加一盏 `DirectionalLight3D`，rotation 设为 `(-45, 30, 0)` 度

### 3. 加地面

50×50 的地图边界目前看不出来。

**在 main.tscn 里添加：**
- `MeshInstance3D` — `PlaneMesh`，size = `Vector2(100, 100)`
- 材质用深灰色 `StandardMaterial3D`，`albedo_color = Color(0.15, 0.15, 0.18)`
- 或加 `StaticBody3D` + `CollisionShape3D`（可选，sim 不走物理引擎）

### 4. 验证旋转方向手性

Unity 左手坐标系 vs Godot 右手坐标系，箭头/玩家朝向**可能反向**。

**测试方法：**
1. 运行游戏
2. 鼠标移到玩家右侧，按左键射击
3. 观察箭头是否朝右飞

**如果反向：**
- 改 `scripts/view/entity_view.gd` 第 65 行：
  - `rotation = Vector3(0, ang, 0)` → `rotation = Vector3(0, -ang, 0)`
- 或在 C++ 端 `sim/snapshot_builder.cpp` 里对 ang 取负（影响所有实体，推荐在 View 层改）

### 5. 箭头朝向修正

当前箭头用 `CylinderMesh`（默认沿 Y 轴竖立），但应该沿飞行方向（XZ 平面）躺平。

**改 `scripts/view/entity_view.gd` 的箭头分支：**
- 圆柱体旋转 90° 让它沿 X 轴：`m.rotation = Vector3(0, 0, -PI/2)`
- 或改用 `BoxMesh`（细长方体）沿 X 轴方向

---

## P1 — 建议在编辑器里做（提升体验）

### 6. 实体预制场景（替代程序化 Mesh）

把 `entity_view.gd` 里的程序化 Mesh 改为加载预制 `.tscn`。

**在编辑器里创建以下预制场景：**

| 场景文件 | 内容 |
|---|---|
| `scenes/entities/player.tscn` | 根 Node3D + MeshInstance3D（蓝色胶囊/盒子）+ 可选血条子节点 |
| `scenes/entities/bot.tscn` | 根 Node3D + MeshInstance3D（红色盒子）+ 血条子节点 |
| `scenes/entities/arrow.tscn` | 根 Node3D + MeshInstance3D（黄色细圆柱，沿 X 轴） |
| `scenes/entities/pickup_xp.tscn` | 根 Node3D + MeshInstance3D（紫色小盒子） |
| `scenes/entities/pickup_heal.tscn` | 根 Node3D + MeshInstance3D（绿色大盒子） |
| `scenes/entities/pickup_small_heal.tscn` | 根 Node3D + MeshInstance3D（青色小盒子） |

**改 `scripts/view/entity_manager.gd` 的 `_get_or_spawn()`：**
```gdscript
const PREFABS = {
    0: preload("res://scenes/entities/player.tscn"),
    1: preload("res://scenes/entities/bot.tscn"),
    2: preload("res://scenes/entities/arrow.tscn"),
    3: preload("res://scenes/entities/pickup_xp.tscn"),  # 按 pickup_type 分派
}

func _get_or_spawn(id: int, type: int, ptype: int) -> EntityView:
    if _entities.has(id):
        return _entities[id]
    var prefab = PREFABS[type]
    var view = prefab.instantiate() as EntityView
    view.init(id, type, ptype)
    add_child(view)
    _entities[id] = view
    return view
```

预制场景的根节点仍挂 `entity_view.gd` 脚本，Mesh 作为子节点手动在编辑器里摆。

### 7. 血条（HealthBar）

Unity 有 `HealthBar.cs` / `HealthBarManager.cs`，目前未移植。

**在编辑器里做：**
- 在 `player.tscn` / `bot.tscn` 里加子节点 `Sprite3D` 或 `Control`（用 `SubViewport` 贴在头顶）
- 绿色背景 + 红色前景，宽度按 `hp / max_hp` 缩放

**新建 `scripts/view/health_bar.gd`：**
```gdscript
extends Sprite3D

func update_hp(hp: int, max_hp: int) -> void:
    var ratio = float(hp) / float(max_hp) if max_hp > 0 else 0.0
    scale.x = ratio  # 或改 region_rect
    visible = ratio < 1.0
```

**在 `entity_view.gd` 的 `apply_snapshot()` 里调用：**
```gdscript
if _hp_bar:
    _hp_bar.update_hp(hp, max_hp)
```

### 8. 快照插值（SnapshotInterpolator）

Unity 有 `SnapshotInterpolator.cs` 做 20Hz 快照间的位置 lerp。当前 Godot 版直接用最新快照，30Hz 下可能轻微抖动。

**新建 `scripts/net/snapshot_interpolator.gd`：**
- 缓存上一帧快照 `_prev` + 当前快照 `_curr`
- `push_snapshot(snap)`：`_prev = _curr; _curr = snap`
- `get_entities()`：按 `elapsed / lerp_duration` lerp 位置，其他字段 snap

**改 `sim_bridge.gd`：**
- `_physics_process` 里 `pop_snapshot` → `_interpolator.push_snapshot(snap)`
- `_process` 里 `_interpolator.get_entities()` → `entity_manager.sync_entities()`

### 9. 设置面板（SettingsPanel）

Unity 有 `SettingsPanel.cs`（自动开火 / 相机锁定开关）。

**在编辑器里做：**
- `CanvasLayer` 下加 `Control` + `CheckBox` × 2 + `Button`（关闭）
- 用 `ConfigFile` 持久化到 `user://settings.cfg`

**新建 `scripts/ui/settings_panel.gd`：**
- 按 ESC 切换可见性
- 自动开火 → `input_collector.fire_mode`
- 相机锁定 → `camera_controller.locked`

---

## P2 — 锦上添花（可后做）

| # | 事项 | 说明 |
|---|------|------|
| 10 | 相机缩放 | 滚轮调整 `camera.size`（正交相机的视野大小） |
| 11 | 相机边界 | 限制相机不超出地图边界 ±50 |
| 12 | 击杀提示 | 快照里 `events` 数组含 kill 事件，可在屏幕中央闪一下 "Killed Bot #1003" |
| 13 | 音效 | Unity 端本身就 out of scope，Godot 侧可后做 |
| 14 | 并跑对齐验证 | Unity vs Godot 同种子跑 30s，比对杀敌数/存活数 |
| 15 | 网络层 | 方案文档里有但未实现（单玩家优先） |

---

## 编辑器操作流程（推荐顺序）

```
1. 用 Godot 4.7 打开 godot/topdown-shooter/project.godot
2. 编辑器会自动加载 addons/topdown_sim/topdown_sim.gdextension
   → 确认输出面板无报错，SimServer/SimSnapshot 等类已注册
3. 打开 scenes/main.tscn → 保存（生成 .uid）
4. 加 DirectionalLight3D + WorldEnvironment + 地面 PlaneMesh
5. 按 F5 运行 → 确认能看到占位方块在移动
6. 验证旋转方向（P0 #4）
7. 创建实体预制场景（P1 #6）
8. 改 entity_manager.gd 用预制场景
9. 加血条（P1 #7）
10. 加快照插值（P1 #8）
```

---

## 已完成清单（无需编辑器操作）

| 层 | 模块 | 状态 |
|---|------|------|
| C++ | Vec2 + math | ✅ |
| C++ | GameConfig (constexpr) | ✅ |
| C++ | 21 个 ECS 组件 | ✅ |
| C++ | entt::registry World + 单例 + seeding | ✅ |
| C++ | CommandBuffer (ECB 等效) | ✅ |
| C++ | ArrowSpawner | ✅ |
| C++ | 12 个 System | ✅ |
| C++ | SnapshotBuilder + 6 RefCounted 类 | ✅ |
| C++ | SimServer (GDExtension 入口) | ✅ |
| 构建 | CMake + build.py + build_env.yaml | ✅ |
| GDScript | sim_bridge.gd | ✅ |
| GDScript | input_collector.gd | ✅ |
| GDScript | entity_manager.gd | ✅ |
| GDScript | entity_view.gd (占位 Mesh) | ✅ |
| GDScript | camera_controller.gd | ✅ |
| GDScript | stats_panel.gd (HUD) | ✅ |
| 场景 | main.tscn (手写，需编辑器重存) | ✅ |
| 数据 | default.json | ✅ |
| 验证 | --headless 运行无报错 | ✅ |
