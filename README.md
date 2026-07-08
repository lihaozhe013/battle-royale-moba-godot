# Battle Royale MOBA (Godot)

类 MOBA 游戏原型。C++ GDExtension 模拟层 + GDScript 视图层，ECS 架构。
支持 WASD 双摇杆和右键点地板两种操作模式。

## 双操作模式

| 模式 | 移动 | 技能键 | 设置 |
|------|------|--------|------|
| **WASD**（默认） | WASD / 方向键 | C / E / R / F | ESC → 操作模式 |
| **MOBA** | 右键点地板（A* 寻路） | Q / W / E / R | 模式偏好持久化到 ConfigFile |

MOBA 模式特性：右键连点节流、同区域重算跳过、转向速率平滑、S 键停止、右键长按连点（~6Hz）。

## 环境需求

构建 C++ GDExtension 需要以下工具（构建脚本会自动检测）：

| 工具 | 版本 | 安装方式 |
|------|------|----------|
| CMake | ≥ 3.17 | `brew install cmake` (macOS), `apt install cmake` (Linux), 或从 [cmake.org](https://cmake.org) 下载 |
| Ninja | ≥ 1.10 | `brew install ninja` (macOS), `apt install ninja-build` (Linux), `pip install ninja` (全平台) |
| Python | ≥ 3.13 | 项目使用 uv 管理，参考 `pyproject.toml` |
| C++编译器 | C++17 | macOS: Xcode CLT (`xcode-select --install`), Linux: GCC ≥ 8 或 Clang ≥ 7, Windows: 需从 Visual Studio Developer Command Prompt 执行 |

首次构建前，复制对应的 `build_env.*.example` 为 `build_env.yaml` 并按需配置。

## 架构

```
C++ Sim (entt::registry + 18 systems, 30Hz)
    ↓ SimSnapshot (RefCounted)
sim_bridge.gd (系统调度器)
    ├─ EntityManager    → EntityView (3D: 位置插值 + 骨骼动画)
    ├─ HealthBarManager → HealthBarUI (2D: 屏幕空间血条)
    ├─ StatsPanel       (HUD: 等级 / HP / 击杀 / XP)
    ├─ BottomHUD        (技能栏 + 物品栏 + 背包)
    ├─ SettingsPanel    (ESC: 操作模式切换 + 退出)
    └─ CameraController (55° 俯视, 跟随玩家)
```

- **Sim 层** (C++): 纯逻辑，无渲染。entt ECS，30Hz tick，输出 `SimSnapshot`
- **View 层** (GDScript): 纯渲染，无逻辑。消费快照，60Hz 插值
- **解耦**: Sim 不知道 Godot，View 不知道 ECS 内部结构，通过 Snapshot 数据契约通信

## 技术栈

- Godot 4.7 (Forward Plus, D3D12)
- C++ GDExtension (godot-cpp + entt)
- CMake 构建 (`src_cpp/`)
- GDScript 视图层 (`scripts/`)

## 运行

```bash
# 1. 构建 C++ GDExtension
cd src_cpp && python build.py

# 2. 用 Godot 4.7 打开项目
# 3. F5 运行
```

## 关键设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 模拟层语言 | C++ GDExtension | ECS 性能，entt 生态 |
| 视图层语言 | GDScript | 快速迭代，Godot 原生集成 |
| Sim-View 通信 | Snapshot 数据契约 | 完全解耦，可独立测试 |
| 客户端插值 | 33ms lerp (1/30) + seq 去重 | 匹配 Sim tick rate，消除角度 jitter |
| 寻路算法 | Sim 层手写 A*（均匀网格 0.5u） | 零 Godot 依赖，墙体静态 AABB 天然适配 |
| 操作模式 | 实时切换 WASD / MOBA | 双模式互不干扰，HUD 自动更新键位标签 |
| 右键仲裁 | 施法中仅取消不下达移动 | 符合 MOBA 直觉 (LoL/Dota)，Sim 权威判断 |

## 文档

- `Docs/Reference/prompt.md` — 完整设计文档（架构、API 契约、扩展路线图）
- `Docs/Reference/sim_system_reference.md` — C++ Sim 层完整参考手册
- `Docs/Reference/right_click_movement_design.md` — 右键点地板移动 + 双模式设计方案
- `Docs/Reference/skill_system_design.md` — 4 技能系统设计方案
- `Docs/Reference/godot-editor-todo.md` — 编辑器操作步骤

## 项目结构

```
scripts/
├── sim_bridge.gd               # 系统调度器
├── input/input_collector.gd    # 输入采集（双模式感知）
├── autoload/game_settings.gd   # 操作模式 autoload + ConfigFile
├── view/
│   ├── entity_manager.gd       # 3D 实体 spawn/despawn
│   ├── entity_view.gd          # 3D 实体视图 (插值 + 动画)
│   ├── camera_controller.gd    # 相机控制
│   ├── skill_vfx.gd            # 技能 VFX
│   └── move_target_vfx.gd      # 右键 ping 标记
├── ui/
│   ├── health_bar_manager.gd   # 血条视图系统
│   ├── health_bar_ui.gd        # 血条视图组件
│   ├── stats_panel.gd          # HUD 面板
│   ├── bottom_hud.gd           # 底部 HUD（技能/物品/背包）
│   ├── settings_panel.gd       # 设置面板（模式切换 + 退出）
│   ├── skill_slot_ui.gd        # 技能槽 UI
│   └── item_slot_ui.gd         # 物品槽 UI
src_cpp/                        # C++ Sim 层 (entt ECS)
scenes/
├── main.tscn                   # 主场景
├── entities/                   # 实体预制 (player, bot, arrow, pickups)
└── ui/                         # UI 预制
```

## 寻路系统

- **NavGrid**: 200×200 均匀网格（0.5u/格），墙体膨胀烘焙
- **A***: 8 方向 + octile 启发 + 二叉堆
- **平滑**: string-pulling（视线法），消除网格锯齿
- **跟随**: 转角速率限制（12 rad/s），位置即时响应、朝向平滑追赶
- **节流**: 右键时间死区 80ms + 目标死区 1.5u + 长按连点 6Hz
