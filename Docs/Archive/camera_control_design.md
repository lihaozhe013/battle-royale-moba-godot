# 视角操控方式 + 全屏设置 — 设计方案

> 创建：2026-07-08
> 关联：`Archive/right_click_movement_design.md`（设置面板与 GameSettings 由来）、`prompt.md`、`sim_system_reference.md`
> 状态：📋 设计阶段，未实施

---

## 1. 概述

### 目标

把当前硬编码在 `camera_controller.gd` 中的「锁视角 / 自由拖屏」二态升级为可配置项，并扩展相机交互，统一放在设置面板里：

1. **视角操控方式**：`Locked（锁视角）/ Free（自由拖屏）` 两种模式
2. **中键拖屏种类**：`PixelPrecise（像素精准）/ Smooth（当前平滑世界对齐）` 两种
3. **屏幕边缘拖屏**：鼠标贴近窗口边缘时地图反向滚动（仅在全屏/最大化下有意义）
4. **全屏模式**：`Windowed / Borderless / Exclusive` 三种窗口模式（其中 Borderless 与 Exclusive 都可作为"假独占/真独占全屏"）

### Pure View 层

本方案**完全在 View 层**完成，不触碰任何 C++ Sim 代码、不动 Snapshot、不改 `components.h`：
- 相机视角与玩家移动权威完全独立（Sim 只关心 `SimPlayerSnap.x/y`，View 层决定跟随方式）
- 输入采集（`input_collector.gd`）的瞄准逻辑依赖当前 Camera3D 的投影矩阵，所以相机变化不能影响 `aim_world` 推导的正确性——只要 `_screen_to_ground()`/`project_ray_*` 沿用同一相机即可，无需改动 input

✅ 结论：方案完全契合项目「Sim 权威 + Snapshot 通信 + View 自治」的架构铁律。

---

## 2. 架构决策

### 决策 1：CamMode 用枚举而非 bool `follow`

当前 `camera_controller.gd` 用 `var _follow: bool = true` + Y 键 toggle。改为：

```gdscript
enum CamMode { LOCKED, FREE }
var _mode: CamMode = CamMode.LOCKED
```

- Y 键仍是快捷切换（保留现有手感）
- 设置面板 OptionButton 同步 `_mode`
- `$Unblock` 路径：`Free` 模式下不读 `follow_target()`，`Locked` 模式每帧 `look_at = Vector3(_target_x, 0, _target_z)`
- 中键拖动时若当前为 Locked，自动切到 Free（保留现有 `delta.length() > 2.0 → _follow = false` 行为，改为切 `_mode = FREE` 并 emit 信号回写 GameSettings）

### 决策 2：中键拖屏两模式拆为 `MiddleDragStyle` 枚举

| 模式 | 实现原理 | 视觉手感 |
|------|---------|---------|
| **Smooth（默认，当前实现）** | 取起止两点的世界坐标，`_look_at -= (world_b - world_a)`（场景内容"跟着鼠标反方向走"） | 像"抓地图"拖动，地图跟着光标走，缩放有透视变化 |
| **PixelPrecise** | 鼠标屏幕位移 Δp 直接映射到 `_look_at` 的世界位移，比例由当前相机高度决定（`world_per_pixel = 2 * _height * tan(FOV/2) / viewport_height`，再加 X 方向按 aspect 修正） | 不论缩放如何，1 像素移动恒定世界距离；透视失真更轻、更"老派 RTS" |

```gdscript
enum MiddleDragStyle { SMOOTH, PIXEL }
var _drag_style: MiddleDragStyle = MiddleDragStyle.SMOOTH
```

> **PixelPrecise 的关键公式**（透视相机）：
> `world_per_pixel_v = 2 * _height * tan(deg_to_rad(FOV * 0.5)) / viewport_h`
> `world_per_pixel_u = world_per_pixel_v / aspect_ratio`

### 决策 3：屏幕边缘拖屏依赖 DisplayServer 全屏状态

- 边缘推屏 Check 在 `camera_controller._process(delta)` 中，先读 `get_viewport().get_mouse_position()` 与 `get_viewport().size` 比对阈值
- 阈值常量：`EDGE_PAN_PIXEL := 8`（贴边范围）、`EDGE_PAN_SPEED := 14.0`（世界单位/秒，按 _height 缩放）
- 仅在 `GameSettings.edge_pan_enabled == true` **且** `DisplayServer.window_get_mode()` 为 `FULLSCREEN`/`MAXIMIZED`/`Borderless` 风格时生效（窗口化下鼠标离开窗口会马上无边界，容易误触发；打印一行警告即可，不强制）
- 自由视角下边缘推屏持续平移 `_look_at`；锁视角下边缘推屏自动切到 Free（同中键拖屏）

### 决策 4：全屏三种模式

Godot 4.7 的 `DisplayServer` API：

| 模式 | API 调用 | 行为 |
|------|---------|------|
| **Windowed** | `window_set_mode(WINDOW_MODE_WINDOWED)` + `window_set_flag(WINDOW_FLAG_BORDERLESS, false)` | 普通窗口（默认） |
| **Borderless（假全屏）** | `window_set_mode(WINDOW_MODE_FULLSCREEN)` + `window_set_flag(WINDOW_FLAG_BORDERLESS, true)` | 无边框窗口覆盖桌面区，DWM 合成，分辨率=virtual desktop res，**alt+tab 平滑、不抢独占** |
| **Exclusive（真独占）** | `window_set_mode(WINDOW_MODE_EXCLUSIVE_FULLSCREEN)` + `window_set_flag(WINDOW_FLAG_BORDERLESS, false)` | 真独占全屏，由显卡切换显示模式，性能略高但切窗口闪屏 |

切换时机：
- OptionButton `item_selected` 信号触发 `@on_main_tree` 里立刻调用 DisplayServer API
- `GameSettings` 持久化 `fullscreen` 枚举，启动时 `_ready` 即应用

> **注意**：Godot 4 的 `WINDOW_MODE_FULLSCREEN` 默认即为无边框（带 borderless flag），与 `WINDOW_MODE_EXCLUSIVE_FULLSCREEN` 才是真独占；所以只需两个枚举值就能区分"假全屏/真独占"，加上 Windowed 一共 3 项。

---

## 3. 设置项设计总览

| 设置 ID | 显示名 | 枚举 | 默认 | 快捷键 | 持久化 key |
|--------|--------|------|------|--------|-----------|
| `camera_mode` | 视角操控 | `LOCKED / FREE` | `LOCKED` | **Y**（toggle） | `controls.camera_mode` |
| `middle_drag` | 中键拖屏 | `SMOOTH / PIXEL` | `SMOOTH` | — | `controls.middle_drag` |
| `edge_pan` | 边缘拖屏 | `OFF / ON` | `OFF` | — | `controls.edge_pan` |
| `fullscreen` | 全屏模式 | `WINDOWED / BORDERLESS / EXCLUSIVE` | `WINDOWED` | — | `display.fullscreen` |

> 全部在已存在的 `user://settings.cfg` 里追加 key，**沿用同一文件、同一节扩展**，不新建配置文件。
> 现有 `move_mode` 不变；阶段 1-2 与本方案独立，不互相影响存储结构。

---

## 4. `game_settings.gd` 扩展

在现有 33 行脚本上追加枚举与持久化字段，保留 `move_mode` / `mode_changed` 不动。

```gdscript
extends Node

enum MoveMode { WASD, MOBA }
var move_mode: MoveMode = MoveMode.WASD: ...        # 已有

signal mode_changed(m: MoveMode)                   # 已有
signal camera_mode_changed(m: CamMode)             # 新增
signal middle_drag_changed(s: MiddleDragStyle)     # 新增
signal edge_pan_changed(on: bool)                  # 新增
signal fullscreen_changed(m: FullscreenMode)       # 新增

enum CamMode { LOCKED, FREE }
enum MiddleDragStyle { SMOOTH, PIXEL }
enum FullscreenMode { WINDOWED, BORDERLESS, EXCLUSIVE }

var camera_mode: CamMode = CamMode.LOCKED
var middle_drag: MiddleDragStyle = MiddleDragStyle.SMOOTH
var edge_pan: bool = false
var fullscreen: FullscreenMode = FullscreenMode.WINDOWED

const CFG_SECTION_CTRL := "controls"
const CFG_SECTION_DISP := "display"
```

读写逻辑沿用一个 `_save()` 扫所有 key；保持已有按字段 setter 写盘的轻量做法，但改成"全键一次写回"以避免多次 I/O：

```gdscript
func _load() -> void:
    var cfg := ConfigFile.new()
    if cfg.load(CFG_PATH) == OK:
        move_mode   = cfg.get_value(CFG_SECTION_CTRL, "move_mode", int(MoveMode.WASD)) as MoveMode
        camera_mode = cfg.get_value(CFG_SECTION_CTRL, "camera_mode", int(CamMode.LOCKED)) as CamMode
        middle_drag = cfg.get_value(CFG_SECTION_CTRL, "middle_drag", int(MiddleDragStyle.SMOOTH)) as MiddleDragStyle
        edge_pan    = bool(cfg.get_value(CFG_SECTION_CTRL, "edge_pan", false))
        fullscreen  = cfg.get_value(CFG_SECTION_DISP, "fullscreen", int(FullscreenMode.WINDOWED)) as FullscreenMode

func _save() -> void:
    var cfg := ConfigFile.new()
    cfg.set_value(CFG_SECTION_CTRL, "move_mode", int(move_mode))
    cfg.set_value(CFG_SECTION_CTRL, "camera_mode", int(camera_mode))
    cfg.set_value(CFG_SECTION_CTRL, "middle_drag", int(middle_drag))
    cfg.set_value(CFG_SECTION_CTRL, "edge_pan", edge_pan)
    cfg.set_value(CFG_SECTION_DISP, "fullscreen", int(fullscreen))
    cfg.save(CFG_PATH)
```

setter 模式：每个 var 都用 `set(value): ...emit... _save()`（同 `move_mode` 写法）。

**启动时全屏应用**：在 `_ready` 中，`_load()` 之后立刻调一次：
```gdscript
_apply_fullscreen(fullscreen)
func _apply_fullscreen(m: FullscreenMode) -> void:
    var ds := DisplayServer
    match m:
        FullscreenMode.WINDOWED:
            ds.window_set_mode(ds.WINDOW_MODE_WINDOWED)
            ds.window_set_flag(ds.WINDOW_FLAG_BORDERLESS, false)
        FullscreenMode.BORDERLESS:
            ds.window_set_mode(ds.WINDOW_MODE_FULLSCREEN)
            ds.window_set_flag(ds.WINDOW_FLAG_BORDERLESS, true)
        FullscreenMode.EXCLUSIVE:
            ds.window_set_mode(ds.WINDOW_MODE_EXCLUSIVE_FULLSCREEN)
            ds.window_set_flag(ds.WINDOW_FLAG_BORDERLESS, false)
```
> 此函数同时由 setter 调用，做到配置面板 switch → 即时生效。

---

## 5. Settings Panel UI 扩展

### 5.1 tscn 布局追加

在现有 `PanelBg`（`anchor_left=0.3 right=0.7 top=0.25 bottom=0.75`）的固定 460×500 区域内追加：

| Label | OptionButton id | items |
|------|-----------------|-------|
| `ModeLabel` "Mode" | `ModeOption` | WASD / MOBA（已有） |
| `CameraModeLabel` "Camera" | `CameraModeOption` | Locked / Free |
| `MiddleDragLabel` "Mid Drag" | `MiddleDragOption` | Smooth / Pixel |
| `EdgePanLabel` "Edge Pan" | `EdgePanOption` | Off / On |
| `FullscreenLabel` "Fullscreen" | `FullscreenOption` | Windowed / Borderless / Exclusive |

> 由于面板当前没有滚动容器，新增 4 项垂直排列可能超出 `anchor_bottom=0.75`。简单解决方式有两种，**择一**：
> - **(推荐)** 把 PanelBg 四边直接放大：`anchor_top=0.15 anchor_bottom=0.85`，并把已有 `QuitButton` 的 `offset_top` 下移到约 460。
> - 或者把内容区改成 `VBoxContainer` 居中排布，去除绝对 offset，可读性更高且不再手算位置。
>
> 若选择 VBox 方案，连带可把已有 `ModeOption` 一并搬入 VBox，避免 layout 出现叠层 bug。最终选择放在阶段 5.1 决定，但 VBox 优先。

### 5.2 `settings_panel.gd` 扩展

```gdscript
func _ready() -> void:
    visible = false
    # 已有
    _mode_option.add_item("WASD", 0); _mode_option.add_item("MOBA", 1)
    _mode_option.select(GameSettings.move_mode)
    # 新增
    _camera_mode_option.add_item("Locked", 0); _camera_mode_option.add_item("Free", 1)
    _camera_mode_option.select(GameSettings.camera_mode)
    _middle_drag_option.add_item("Smooth", 0); _middle_drag_option.add_item("Pixel", 1)
    _middle_drag_option.select(GameSettings.middle_drag)
    _edge_pan_option.add_item("Off", 0); _edge_pan_option.add_item("On", 1)
    _edge_pan_option.select(int(GameSettings.edge_pan))
    _fullscreen_option.add_item("Windowed", 0); _fullscreen_option.add_item("Borderless", 1); _fullscreen_option.add_item("Exclusive", 2)
    _fullscreen_option.select(GameSettings.fullscreen)

func _on_camera_mode_selected(i: int) -> void: GameSettings.camera_mode = i as GameSettings.CamMode
func _on_middle_drag_selected(i: int) -> void:  GameSettings.middle_drag = i as GameSettings.MiddleDragStyle
func _on_edge_pan_selected(i: int) -> void:      GameSettings.edge_pan = (i == 1)
func _on_fullscreen_selected(i: int) -> void:    GameSettings.fullscreen = i as GameSettings.FullscreenMode
```

tscn `connection` 段同步追加 4 行 `item_selected` → 对应方法。

---

## 6. `camera_controller.gd` 重构

### 6.1 字段替换

```diff
- var _follow: bool = true
+ enum CamMode { LOCKED, FREE }    # 与 GameSettings 同定义，但脚本内可有本地副本常量
+ var _mode: CamMode = CamMode.LOCKED
+ var _drag_style: int = GameSettings.MiddleDragStyle.SMOOTH
+ var _edge_pan: bool = false
```

### 6.2 信号订阅（`_ready`）

```gdscript
func _ready() -> void:
    # 已有相机初始化
    _mode       = GameSettings.camera_mode
    _drag_style = GameSettings.middle_drag
    _edge_pan   = GameSettings.edge_pan
    GameSettings.camera_mode_changed.connect(_on_camera_mode_changed)
    GameSettings.middle_drag_changed.connect(_on_middle_drag_changed)
    GameSettings.edge_pan_changed.connect(_on_edge_pan_changed)
```

### 6.3 Y 键 / 中键切到 Free 时回写 GameSettings

```gdscript
func _unhandled_input(event):
    if event is InputEventKey and event.pressed and event.keycode == KEY_Y:
        GameSettings.camera_mode = (_mode + 1) % 2   # setter 内会发信号，回写到这里
    # ... 中键按下时若 _mode == LOCKED，类似 GameSettings.camera_mode = FREE
```

> 这条把"硬编码 toggle"变成"经过 GameSettings 的真相源"——避免 Y 键状态与设置面板 OptionButton 不一致。Y 键继续保留作为快捷键。

### 6.4 中键拖屏分支

```gdscript
if event is InputEventMouseMotion and _dragging:
    var delta := event.position - _drag_start_mouse
    if delta.length() > 2.0 and _mode == CamMode.LOCKED:
        GameSettings.camera_mode = CamMode.FREE   # 触发回写
    match _drag_style:
        GameSettings.MiddleDragStyle.SMOOTH:
            # 保留现有 world-aligned 实现
            var world_a = _screen_to_ground(_drag_start_mouse)
            var world_b = _screen_to_ground(event.position)
            _look_at = _drag_start_look_at - (world_b - world_a)
        GameSettings.MiddleDragStyle.PIXEL:
            var ratio = _world_per_pixel_ratio()
            _look_at = _drag_start_look_at - Vector3(delta.x * ratio.x, 0, delta.y * ratio.y)

func _world_per_pixel_ratio() -> Vector2:
    var vp := get_viewport().size
    var tan_h = tan(deg_to_rad(FOV * 0.5))
    var wpix_v = 2.0 * _height * tan_h / vp.y
    var wpix_u = wpix_v  # X 用同样数值；考虑到相机的俯视，更严格的话用 aspect 比修正
    return Vector2(wpix_u, wpix_v)
```

> PixelPrecise 简化版直接用屏幕 Δp 映射到 X/Z 等量。由于相机有 55° tilt，"严格像素投影"会略有误差；在原型阶段足够，且正是 RTS 老式手感的来源。若日后追求严谨可在 stage E 做校准。

### 6.5 边缘拖屏（`_process`）

```gdscript
func _process(delta: float) -> void:
    # 已有 _rotation_initialized / follow lerp 不变
    if _edge_pan and _ok_for_edge_pan():
        var mp := get_viewport().get_mouse_position()
        var sz := get_viewport().size
        var push := Vector2.ZERO
        if mp.x <= EDGE_PAN_PIXEL:               push.x -= 1.0
        elif mp.x >= sz.x - EDGE_PAN_PIXEL:      push.x += 1.0
        if mp.y <= EDGE_PAN_PIXEL:               push.y -= 1.0
        elif mp.y >= sz.y - EDGE_PAN_PIXEL:      push.y += 1.0
        if push != Vector2.ZERO:
            if _mode == CamMode.LOCKED:
                GameSettings.camera_mode = CamMode.FREE
            var spd := EDGE_PAN_SPEED * (_height / HEIGHT)   # 缩放越大推得越快
            _look_at += Vector3(push.x * spd * delta, 0, push.y * spd * delta)
    position = position.lerp(_look_at, delta * FOLLOW_SPEED)
    _camera.position = Vector3(0, _height, -_distance)

func _ok_for_edge_pan() -> bool:
    var mode = DisplayServer.window_get_mode()
    return mode in [DisplayServer.WINDOW_MODE_FULLSCREEN, DisplayServer.WINDOW_MODE_EXCLUSIVE_FULLSCREEN, DisplayServer.WINDOW_MODE_MAXIMIZED]
```

注意：`Camera3D` 仍是相机节点的子节点，且 `_look_at` 是地图水平面坐标；`Vector3(push.x, 0, push.y)` 把屏幕 X 映射成世界 X、屏幕 Y 映射成世界 Z。在 55° 倾斜相机下屏幕 Y 与世界 Z 方向近似但不严格正交，原型可接受；阶段 E 可校准。

---

## 7. 全屏生效路径

```
启动 →  game_settings._ready() → _load() → _apply_fullscreen(fullscreen) → DisplayServer
面板切 → _on_fullscreen_selected → GameSettings.fullscreen= ... (setter)
                              → fullscreen_changed.emit
                              → _apply_fullscreen(...)
                              → _save()
```

`setter` 持久化时机：与 `move_mode` 一致——setter 即写 `settings.cfg`。

### project.godot 改动

无需改动 `[display]` 静态配置；窗口模式由 GameSettings 在运行时 apply。**可选**：
```ini
[display]
window/size/initial_mode=0    # 0=windowed，初始窗口；后续被 runtime 覆盖
```
维持现状 `aspect="expand"` 已能兼容三种全屏模式。

---

## 8. 变更汇总

### 8.1 修改文件

| 文件 | 改动 |
|------|------|
| `scripts/autoload/game_settings.gd` | +3 enum（`CamMode/MiddleDragStyle/FullscreenMode`）、4 字段+setter、3 信号、`_apply_fullscreen()`、`_load/_save` 扩展多键 |
| `scripts/ui/settings_panel.gd` | +4 OptionButton 绑定、4 个 `_on_*_selected` 回调；面板 `_ready` 初始化 4 个 OptionButton 当前选中 |
| `scenes/ui/settings_panel.tscn` | 加 4 Label + 4 OptionButton；建议用 VBox 重排；新增 4 行 connection |
| `scripts/view/camera_controller.gd` | `_follow` 换 `_mode`、+订阅 GameSettings 三信号、中键分支按 `_drag_style` 拆分、+边缘推屏、Y 键改成走 GameSettings setter |

### 8.2 不动

| 不动 | 理由 |
|------|------|
| C++ Sim 全部 | 纯 View 层功能 |
| `input_collector.gd` | 瞄准逻辑用 `get_viewport().get_camera_3d()` 实时获取相机，相机切换模式不影响 ray 投影 |
| `sim_bridge.gd` | 不涉及新输入字段 |
| `bottom_hud.gd` | 与视角无关 |
| Snapshot / 组件 / System | 无 Sim 语义变化 |

### 8.3 ConfigFile 新增 key

```
[controls]
move_mode=...        # 已有
camera_mode=0        # 新增  0=LOCKED 1=FREE
middle_drag=0        # 新增  0=SMOOTH 1=PIXEL
edge_pan=false       # 新增
[display]
fullscreen=0         # 新增  0=WINDOWED 1=BORDERLESS 2=EXCLUSIVE
```

---

## 9. 分阶段实施计划

### 阶段 A — GameSettings 扩展（最底层）

| 步骤 | 文件 | 说明 |
|------|------|------|
| A.1 | `game_settings.gd` | 加 3 enum + 4 var+setter + 3 signal |
| A.2 | `game_settings.gd` | `_load/_save` 改成全键读写；新增 `_apply_fullscreen()` |
| A.3 | `game_settings.gd` | `_ready` 末尾 `call_deferred("_apply_fullscreen", fullscreen)`（保证 DisplayServer 已就绪） |

**验收：** 启动游戏，`user://settings.cfg` 出现 4 个新键；改 `fullscreen=1` 启动时窗口变无边框全屏。

### 阶段 B — CameraController 重构（行为层）

| 步骤 | 文件 | 说明 |
|------|------|------|
| B.1 | `camera_controller.gd` | `_follow` → `_mode`，Y/中键改调 `GameSettings.camera_mode = ...` |
| B.2 | `camera_controller.gd` | 订阅 `camera_mode_changed / middle_drag_changed / edge_pan_changed` |
| B.3 | `camera_controller.gd` | 中键拖屏按 `_drag_style` 分两支（Smooth 沿用旧代码；Pixel 加 `_world_per_pixel_ratio()`） |
| B.4 | `camera_controller.gd` | `_process` 加 `_edge_pan` 分支 + `_ok_for_edge_pan()` |

**验收：** Y 键与中键状态共享；中切 Smooth ↔ Pixel 时拖动手感不同；开 Edge Pan 后鼠标贴边在窗口化下不触发、在全屏下持续推动相机。

### 阶段 C — 设置面板 UI

| 步骤 | 文件 | 说明 |
|------|------|------|
| C.1 | `settings_panel.tscn` | 评估 VBox 重排 或扩 PanelBg；加 4 Label + 4 OptionButton |
| C.2 | `settings_panel.tscn` | 追加 4 行 connection |
| C.3 | `settings_panel.gd` | `_ready` 初始化 4 OptionButton；加 4 个回调 |

**验收：** 打开面板可见 5 项设置；切换任一项瞬时生效到相机 / 全屏；刷新面板 selected 与 GameSettings 当前值一致；Y 键切换后回到面板可见 CameraMode 同步变 Free。

### 阶段 D — 打磨与边界

| 步骤 | 说明 |
|------|------|
| D.1 | EdgePan 视角对角线方向的多轴组合（push.x && push.y 是否正常加速） |
| D.2 | Pixel 精确拖屏在 ZOOM_MIN / ZOOM_MAX 端点的手感校准 |
| D.3 | 独占全屏切换闪屏提示 / AltRestrict；改回窗口化恢复窗口大小 |
| D.4 | 设置面板布局在 1920×1080 / 2560×1440 不同分辨率下无溢出 |

---

## 10. 风险与取舍

| 风险 | 影响 | 缓解 |
|------|------|------|
| **Pixel 精确拖屏与相机 55° tilt 不严格正交** | 缩放很大时 X/Z 视觉位移略不一致 | 阶段 D 校准系数；够用即可不深究 |
| **独占全屏切回窗口化丢失窗口尺寸** | 玩家手动缩放过窗口后切 Exclusive→Windowed 会回到默认尺寸 | `_apply_fullscreen` 切到 Windowed 前先 `window_set_size(saved)`；可省略，仅作已知小问题 |
| **EdgePan 在窗口化下误触发** | 鼠标在窗口标题栏滑动也能命中边缘 | `_ok_for_edge_pan()` 限定 `window_get_mode()`，打印警告而非硬崩溃 |
| **Y 键与中键自动切 Free 会让"开启时是 Locked"设置"被偷偷改掉"** | 玩家开了 Locked，按一下 Y 又变 Free，刷新面板看到选中变 | 这是预期行为（Y 就是切换）；中键拖动从 Locked 切 Free 也是 RTS/MOBA 通识，不算 bug。若不希望持久化，可在中键拖动结束后把 `GameSettings.camera_mode` 还原回 LOCKED——但当前不实施该还原 |
| **设置面板垂直空间不足** | 4 项新增可能超出原 460×300 区域 | VBox 重排优先；或放大 `anchor_top/bottom` 到 0.15/0.85 |
| **`process_mode = 3` 的设置面板在 ESC 关闭后 Y 键应能工作** | 面板可见时 Y 仍命中相机？检查输入优先级 | camera_controller 走 `_unhandled_input`，设置面板也是 `_unhandled_input`，二者同优先级，按节点先后；ESC 面板可见时若 Y 被吃掉属意外，可让面板 `_unhandled_input` 只吞 ESC、其他键继续传播 |
| **C++ 层完全无关** | 0 风险（正面） | 保证打包不会失败，sim_bridge 不需重新编译 |

---

## 11. 与已有功能的关系

- **不动 `move_mode` 与 MOBA/WASD 双模式**：本方案放在 GameSettings 同一 singleton，互不冲突。
- **不动 `move_target_vfx`**：视角切换不影响右键 ping VFX（VFX 由 input_collector `move_issued` 信号触发，与相机模式无关）。
- **不动 `bottom_hud`**：HUD 按模式显示 QWER，与相机模式正交。
- **不动 `skill_vfx`**：技能 VFX 同样依赖 viewport 当前相机，切视角时 ray origin 仍正确。

---

## 12. 总体评估结论

- **可行性：高**。所有 API（Camera3D 投影、DisplayServer 全屏、ConfigFile、Signal）Godot 4.7 都开箱即用；不引入第三方依赖。
- **架构契合度：高**。完全 View 层，零 Sim 改动，与"Snapshot 唯一通信"无冲突。
- **工作量估计**：约 250~350 行 GDScript/tscn 改动；1 个工作日内可完成阶段 A+B+C，D 视手感而异。
- **风险：低**。最大变数为 setings_panel 在多分辨率下的布局，纯 UI 调参。
- **推荐执行顺序**：A → B → C → D，且每个阶段都可独立提交验收。

> 实施完成后把本文件归入 `Docs/Archive/`，并在 `AGENTS.md` 「已完成」表里追加一行。