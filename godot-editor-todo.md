# Godot 编辑器待办事项

> 最后更新：2026-07-06
> 当前阶段：P1 血条系统（2D 屏幕空间，MOBA 风格）

---

## 当前任务：P1 血条系统（2D 屏幕空间）

> 完整设计见 `prompt.md` — 血条系统设计部分

### Step 1. 创建 `scenes/ui/health_bar_ui.tscn`

1. 场景 → New Scene → 根节点选 `Control` → 命名 `HealthBarUI`
2. 保存到 `scenes/ui/health_bar_ui.tscn`
3. 挂 `scripts/ui/health_bar_ui.gd` 脚本
4. 根节点属性：
   - **Custom Minimum Size**: `Vector2(100, 10)`
   - **Mouse Filter**: `Ignore`

### Step 2. 添加 Background 子节点

1. 右键 HealthBarUI → Add Child Node → `ColorRect` → 命名 `Background`
2. 属性：
   - **Anchors Preset**: `Full Rect`（铺满父节点）
   - **Color**: `Color(0.1, 0.1, 0.1, 0.8)`（深色半透明）
   - **Mouse Filter**: `Ignore`

### Step 3. 添加 DamageBar 子节点

1. 右键 HealthBarUI → Add Child Node → `ColorRect` → 命名 `DamageBar`
2. 属性：
   - **Anchors Preset**: `Top Left`（左上角对齐，宽度由脚本控制）
   - **Position**: `Vector2(0, 0)`
   - **Size**: `Vector2(100, 10)`
   - **Color**: `Color(1.0, 0.8, 0.0)`（黄色）
   - **Mouse Filter**: `Ignore`

### Step 4. 添加 Fill 子节点

1. 右键 HealthBarUI → Add Child Node → `ColorRect` → 命名 `Fill`
2. 属性：
   - **Anchors Preset**: `Top Left`
   - **Position**: `Vector2(0, 0)`
   - **Size**: `Vector2(100, 10)`
   - **Color**: `Color(0.2, 1.0, 0.2)`（绿色，运行时由 `set_team()` 覆盖）
   - **Mouse Filter**: `Ignore`

### Step 5. 保存 `health_bar_ui.tscn`

确认节点树：
```
HealthBarUI (Control) [health_bar_ui.gd]
├── Background (ColorRect)   — Full Rect, 深色
├── DamageBar (ColorRect)    — Top Left, 黄色
└── Fill (ColorRect)         — Top Left, 绿色
```

### Step 6. 在 `main.tscn` 中添加 HealthBarManager 节点

1. 打开 `scenes/main.tscn`
2. 右键 Main 根节点 → Add Child Node → `Node` → 命名 `HealthBarManager`
3. 挂 `scripts/ui/health_bar_manager.gd` 脚本
4. 检查器：
   - **Health Bar Scene**: 拖入 `scenes/ui/health_bar_ui.tscn`
5. 确认节点树结构：
```
Main (Node, sim_bridge.gd)
├── InputCollector (Node)
├── CameraController (Node3D)
│   └── Camera3D
├── EntityManager (Node)
├── HealthBarManager (Node)          ← 新增
├── CanvasLayer
│   └── StatsPanel (Control)
├── DirectionalLight3D
├── MeshInstance3D
└── WorldEnvironment
```
6. 保存

### Step 7. 验证

1. 按 F5 运行
2. 血条应显示在角色和 Bot 头顶（上方约 2 米处）
3. Player 血条为绿色，Bot 血条为红色
4. Bot 掉血时：
   - Fill 立即从右往左缩短，颜色随 HP 变化
   - DamageBar（黄色）延迟追赶 Fill
5. Bot 死亡时血条隐藏
6. 血条始终在屏幕上面（穿墙可见）
7. 血条跟随角色移动（无抖动，与 3D 模型同步）

---

## 已完成 ✅

| # | 事项 | 状态 |
|---|------|------|
| 1 | 重新保存 main.tscn（生成 .uid）| ✅ |
| 2 | DirectionalLight3D + WorldEnvironment 灯光 | ✅ |
| 3 | 地面 PlaneMesh 100×100 | ✅ |
| 4 | 旋转方向手性验证（Unity 左手 → Godot 右手）| ✅ |
| 5 | 角色素材导入 + 骨骼动画（player.tscn / bot.tscn）| ✅ |
| 6 | Bot 与 Player 运动逻辑镜像 | ✅ |
| 7 | Bot 射击角度独立计算 | ✅ |
| 8 | 快照插值（entity_view 50ms lerp + seq 去重）| ✅ |
| 9 | 相机固定角度（消除跟随摇晃）| ✅ |
| 10 | 输入方向修正 | ✅ |
| 11 | 相机缩放（滚轮）| ✅ |

---

## P2 — 后续优化

| # | 事项 | 说明 |
|---|------|------|
| 1 | HP 分段线 | 每 100 HP 一条深色线，用 Control._draw() |
| 2 | 资源条 (Mana) | HP 下方蓝色条，需 Sim 层加 mana 数据 |
| 3 | 等级徽章 | HP 左侧小圆形 + Label，需 Sim 层加 level 到实体快照 |
| 4 | 护盾条 | Fill 右侧延伸，白色/紫色 |
| 5 | 状态图标 | buff/debuff 图标行 |
| 6 | 击杀提示 | 快照 events 数组含 kill 事件，屏幕中央闪现 |
| 7 | 设置面板 | ESC 切换，自动开火 / 相机锁定开关 |
| 8 | 音效 | 射击 / 命中 / 升级 |
| 9 | 角色动画系统 | AnimationTree + 状态机，平滑过渡 |
| 10 | 相机边界 | 限制相机不超出地图 ±50 |
