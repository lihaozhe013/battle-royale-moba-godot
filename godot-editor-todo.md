# Godot 编辑器待办事项

> 2026-07-05（更新于 2026-07-05）
> C++ Sim 层 + GDScript 视图层已完成，快照插值已实现。
> 当前阶段：用 Asset Library 素材替代程序化占位 Mesh。

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
│  └─ Camera3D (透视, 55° 俯视)
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
- 改 `scripts/view/entity_view.gd` 的 `apply_snapshot()` / `_process()`：
  - `rotation = Vector3(0, ang, 0)` → `rotation = Vector3(0, -ang, 0)`
  - `_process()` 里的 `lerp_angle` 也要取负
- 或在 C++ 端 `sim/snapshot_builder.cpp` 里对 ang 取负（影响所有实体，推荐在 View 层改）

### 5. 箭头朝向修正

当前箭头用 `CylinderMesh`（默认沿 Y 轴竖立），但应该沿飞行方向（XZ 平面）躺平。

**改 `scripts/view/entity_view.gd` 的箭头分支：**
- 圆柱体旋转 90° 让它沿 X 轴：`m.rotation = Vector3(0, 0, -PI/2)`
- 或改用 `BoxMesh`（细长方体）沿 X 轴方向
- 或用 Asset Library 的箭矢/弩箭模型直接替换

---

## P1 — 建议在编辑器里做（提升体验）

### 6. 实体预制场景（用 Asset Library 素材替代程序化 Mesh）

> **策略变更**：跳过简陋的 capsule/box 占位，直接从 Godot Asset Library 和外部免费素材站获取低多边形角色模型。

#### 6a. 素材获取

**Godot Asset Library**（https://godotengine.org/asset-library/asset）搜索关键词：

| 关键词 | 用途 |
|--------|------|
| `low poly character` | 低多边形角色，适合俯视角 |
| `animated character` | 带骨骼动画的角色（idle/walk/run/attack） |
| `top down` | 专为俯视游戏设计的素材 |
| `archer` / `bowman` | 弓箭手角色，匹配箭矢射击玩法 |
| `robot` / `soldier` | Bot 敌人模型 |
| `gem` / `crystal` / `potion` | 拾取物模型 |

**外部免费素材站**（Asset Library 之外，质量更高）：

| 站点 | 特点 | 格式 |
|------|------|------|
| [Kenney.nl](https://kenney.nl/assets) | CC0 授权，风格统一，大量低多边形包 | GLTF/GLB |
| [Quaternius](https://quaternius.com) | CC0 低多边形角色+动画，可直接导入 | FBX/GLTF |
| [KayKit](https://kaylousberg.com) | 低多边形角色包，含骨骼动画 | GLTF |
| [Mixamo](https://mixamo.com) | 免费角色+动画库（需 Adobe 账号） | FBX |
| [Poly Pizza](https://poly.pizza) | 低多边形模型搜索引擎 | GLTF |

**选材要点**：
- 格式优先 GLTF/GLB（Godot 原生支持最好），FBX 需导入转换
- 授权优先 CC0（免署名），CC-BY 需在游戏内署名
- 角色必须带 idle + walk + run 至少三个动画
- 俯视角不需要高精度，low poly 足够

#### 6b. 创建预制场景

下载素材后，在编辑器里创建以下预制场景：

| 场景文件 | 内容 | 素材建议 |
|---|---|---|
| `scenes/entities/player.tscn` | 根 Node3D + 角色 GLTF + AnimationPlayer + 血条 | 弓箭手/战士角色 |
| `scenes/entities/bot.tscn` | 根 Node3D + 角色 GLTF + AnimationPlayer + 血条 | 机器人/骷髅/士兵 |
| `scenes/entities/arrow.tscn` | 根 Node3D + 箭矢 Mesh（沿 X 轴） | 弩箭/魔法弹模型 |
| `scenes/entities/pickup_xp.tscn` | 根 Node3D + 旋转的水晶/宝石 | 紫色宝石 |
| `scenes/entities/pickup_heal.tscn` | 根 Node3D + 药水瓶/心形 | 绿色药水 |
| `scenes/entities/pickup_small_heal.tscn` | 根 Node3D + 小药水瓶 | 青色小药水 |

#### 6c. 改 entity_manager.gd 加载预制

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
`entity_view.gd` 的 `_create_mesh()` 在预制模式下不再调用（或改为空函数）。

#### 6d. 角色动画驱动

如果素材带骨骼动画，需要根据移动状态切换动画：

- 在 `entity_view.gd` 里加 `_animation_player` 引用
- `apply_snapshot()` 里根据位置变化判断是否在移动，切 walk/idle
- 可用 `AnimationTree` + `AnimationNodeStateMachine` 做平滑过渡（进阶）

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

### 8. ~~快照插值（SnapshotInterpolator）~~ ✅ 已完成

> 已在 `entity_view.gd` 实现 entity 级插值（LERP_DURATION=50ms），
> 并在 `sim_bridge.gd` 中通过 `snap.seq` 去重，避免重复 `apply_snapshot` 破坏插值状态。
> 提交: `6b7d52b`

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
| 10 | 相机缩放 | 滚轮调整 `_height`/`_distance`（已实现） |
| 11 | 相机边界 | 限制相机不超出地图边界 ±50 |
| 12 | 击杀提示 | 快照里 `events` 数组含 kill 事件，可在屏幕中央闪一下 "Killed Bot #1003" |
| 13 | 音效 | Unity 端本身就 out of scope，Godot 侧可后做 |
| 14 | 并跑对齐验证 | Unity vs Godot 同种子跑 30s，比对杀敌数/存活数 |
| 15 | 网络层 | 方案文档里有但未实现（单玩家优先） |
| 16 | 角色动画系统 | AnimationTree + 状态机，walk/idle/run/attack 平滑过渡 |

---

## 编辑器操作流程（推荐顺序）

```
 1. 用 Godot 4.7 打开 godot/topdown-shooter/project.godot
 2. 编辑器会自动加载 addons/topdown_sim/topdown_sim.gdextension
    → 确认输出面板无报错，SimServer/SimSnapshot 等类已注册
 3. 打开 scenes/main.tscn → 保存（生成 .uid）
 4. 加 DirectionalLight3D + WorldEnvironment + 地面 PlaneMesh
 5. 按 F5 运行 → 确认能看到占位方块在移动（插值已生效，应平滑）
 6. 验证旋转方向（P0 #4）
 7. 去 Asset Library / Kenney / Quaternius 下载角色素材
 8. 导入 GLTF，创建实体预制场景（P1 #6）
 9. 改 entity_manager.gd 用预制场景
10. 加血条（P1 #7）
11. 加角色动画驱动（P1 #6d）
```

---

## 已完成清单

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
| GDScript | input_collector.gd (A/D 适配右手系) | ✅ |
| GDScript | entity_manager.gd | ✅ |
| GDScript | entity_view.gd (占位 Mesh + 插值) | ✅ |
| GDScript | camera_controller.gd (固定旋转 + 父节点跟随) | ✅ |
| GDScript | stats_panel.gd (HUD) | ✅ |
| 场景 | main.tscn (手写，需编辑器重存) | ✅ |
| 数据 | default.json | ✅ |
| 验证 | --headless 运行无报错 | ✅ |
| 渲染 | 快照插值 (entity_view 50ms lerp + seq 去重) | ✅ |
| 渲染 | 相机固定角度 (消除跟随摇晃) | ✅ |
| 渲染 | 输入方向修正 (Unity左手→Godot右手) | ✅ |
