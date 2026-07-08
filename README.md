# Battle Royale MOBA (Godot)

类 MOBA 游戏原型。C++ GDExtension 模拟层 + GDScript 视图层，ECS 架构。

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
C++ Sim (entt::registry + 12 systems, 21 components)
    ↓ SimSnapshot (30Hz)
sim_bridge.gd (系统调度器)
    ├─ EntityManager    → EntityView (3D: 位置插值 + 骨骼动画)
    ├─ HealthBarManager → HealthBarUI (2D: 屏幕空间血条)
    ├─ StatsPanel       (HUD: 等级 / HP / 击杀 / XP)
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
| 客户端插值 | 50ms lerp + seq 去重 | 平滑运动，抗网络抖动 |
| 血条方案 | 2D 屏幕空间 | MOBA 标准 (LoL/DOTA)，像素清晰，易扩展 |
| 血条管理 | Manager + 对象池 | 独立视图系统，ECS 对齐 |

## 文档

- `Docs/Reference/prompt.md` — 完整设计文档（架构、API 契约、血条系统设计、扩展路线图）
- `Docs/Reference/godot-editor-todo.md` — 编辑器操作步骤

## 项目结构

```
scripts/
├── sim_bridge.gd               # 系统调度器
├── view/
│   ├── entity_manager.gd       # 3D 实体 spawn/despawn
│   ├── entity_view.gd          # 3D 实体视图 (插值 + 动画)
│   └── camera_controller.gd    # 相机控制
├── ui/
│   ├── health_bar_manager.gd   # 血条视图系统
│   ├── health_bar_ui.gd        # 血条视图组件
│   └── stats_panel.gd          # HUD 面板
src_cpp/                        # C++ Sim 层 (entt ECS)
scenes/
├── main.tscn                   # 主场景
├── entities/                   # 实体预制 (player, bot, arrow, pickups)
└── ui/                         # UI 预制 (health_bar_ui)
```
