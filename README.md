# Battle Royale MOBA (Godot)

类 MOBA 游戏原型。C++ GDExtension 模拟层 + GDScript 视图层，ECS 架构。
单一 MOBA 控制模式（右键点地板移动 + Q/W/E/R 4 技能 + A 键普攻命令），完整相机操控系统。

> **历史说明**：项目早期曾有 WASD + MOBA 双模式，WASD 模式已完全移除。输入系统正在按 `Docs/Reference/input_system_design.md` 重构为四层架构（事件队列 / 状态机 / 命令翻译 / 命令缓冲）。

## 操作方式

| 操作                  | 按键               | 说明                                        |
| --------------------- | ------------------ | ------------------------------------------- |
| **移动**              | 右键点地板         | A\* 寻路自动移动                            |
| **技能**              | Q / W / E / R      | 4 技能槽（Quick / Normal cast 双模式可配）  |
| **普攻命令**          | A 键               | 进入普攻瞄准 + 左键确认；或右键点敌直接攻击 |
| **停止**              | S                  | 清移动路径                                  |
| **取消施法/普攻瞄准** | 右键 / S / ESC / H | —                                           |
| **设置**              | ESC                | 非施法时打开设置面板                        |

MOBA 模式特性：右键连点节流、同区域重算跳过、转向速率平滑、S 键停止、右键长按连点（~6Hz）、普攻穿墙追击、技能超出范围自动 A\* 跟随施法。

## 相机控制

| 功能         | 操作                                | 设置                                     |
| ------------ | ----------------------------------- | ---------------------------------------- |
| **锁/自由**  | Y 键切换                            | ESC → Camera                             |
| **中键拖屏** | 中键拖动（像素精准，1:1 鼠标→世界） | 恒定精准，无选项                         |
| **边缘推屏** | 鼠标贴边自动滚动                    | ESC → Edge Pan On/Off + Speed (1.0-50.0) |
| **平滑开关** | 平滑缓动 / 瞬时跳转                 | ESC → Smooth Pan On/Off                  |
| **全屏**     | 窗口化 / 无边框 / 独占全屏          | ESC → Fullscreen                         |
| **按住居中** | F1 / Space（按住锁定，松开解锁）    | —                                        |

相机锁定跟随 30Hz Sim 数据时使用线性插值（LERP 1/30s），消除抖动。Smooth Pan 关闭时拖屏和推屏均为恒速无加速度。

## 环境需求

构建 C++ GDExtension 需要以下工具（构建脚本会自动检测）：

| 工具      | 版本   | 安装方式                                                                                                                            |
| --------- | ------ | ----------------------------------------------------------------------------------------------------------------------------------- |
| CMake     | ≥ 3.17 | `brew install cmake` (macOS), `apt install cmake` (Linux), 或从 [cmake.org](https://cmake.org) 下载                                 |
| Ninja     | ≥ 1.10 | `brew install ninja` (macOS), `apt install ninja-build` (Linux), `pip install ninja` (全平台)                                       |
| Python    | ≥ 3.13 | 项目使用 uv 管理，参考 `pyproject.toml`                                                                                             |
| C++编译器 | C++17  | macOS: Xcode CLT (`xcode-select --install`), Linux: GCC ≥ 8 或 Clang ≥ 7, Windows: 需从 Visual Studio Developer Command Prompt 执行 |

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
    ├─ SettingsPanel    (ESC: 相机 / 边缘推屏 / 平滑 / 全屏 / Quick-Normal cast)
    └─ CameraController (55° 俯视, 锁/自由 + 像素精准拖屏 + 边缘推屏 + 30Hz 插值)
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

| 决策          | 选择                                               | 理由                                                            |
| ------------- | -------------------------------------------------- | --------------------------------------------------------------- |
| 模拟层语言    | C++ GDExtension                                    | ECS 性能，entt 生态                                             |
| 视图层语言    | GDScript                                           | 快速迭代，Godot 原生集成                                        |
| Sim-View 通信 | Snapshot 数据契约                                  | 完全解耦，可独立测试                                            |
| 客户端插值    | 33ms lerp (1/30) + seq 去重                        | 匹配 Sim tick rate，消除角度 jitter                             |
| 寻路算法      | Sim 层手写 A*（均匀网格 0.5u）                     | 零 Godot 依赖，墙体静态 AABB 天然适配                           |
| 操作模式      | 单一 MOBA 模式（WASD 已移除）                      | 右键移动 + QWER 技能 + A 普攻，符合 MOBA 标准                   |
| 右键仲裁      | 施法中仅取消不下达移动                             | 符合 MOBA 直觉 (LoL/Dota)，Sim 权威判断                         |
| 输入系统      | 四层架构（事件队列/状态机/命令翻译/命令缓冲）      | 不丢指令（30Hz Sim < 60Hz 渲染）+ 状态化打断施法 + 双正交状态轴 |
| 相机拖屏      | 像素精准，直接 delta × world-per-pixel 映射        | 消除透视倾斜带来的世界坐标不一致感，响应无延迟                  |
| 边缘推屏      | 仅全屏/最大化下生效，速度随缩放比例自适应          | 窗口化下鼠标离开窗口边界会产生误触，全屏限定避免反直觉          |
| 平滑开关      | ON = lerp 缓动，OFF = position = _look_at 直接跳转 | 玩家可选顺滑跟拍或即时响应；Locked 模式始终插值消除 30Hz 抖动   |
| 按住居中      | F1/Space 按住切 Locked，松开回 Free                | 与 MOBA 常规手感一致 (space 按住所英雄)                         |

## 文档

- `Docs/Reference/prompt.md` — 完整设计文档（架构、API 契约、扩展路线图）
- `Docs/Reference/sim_system_reference.md` — C++ Sim 层完整参考手册
- `Docs/Reference/input_system_design.md` — **输入系统重构方案（唯一标准）**：四层架构 + 双正交状态轴 + Sim Chasing + Quick/Normal cast + 普攻独立模式
- `Docs/Archive/camera_control_design.md` — 视角操控方案（锁/自由 + 精准拖屏 + 边缘推屏 + 全屏）
- `Docs/Reference/godot-editor-todo.md` — 编辑器操作步骤

## 项目结构

```
scripts/
├── sim_bridge.gd               # 系统调度器
├── input/input_collector.gd    # 输入采集（当前实现，待重构为四层架构，见 input_system_design.md）
├── autoload/game_settings.gd   # camera/全屏/cast 偏好 autoload + ConfigFile
├── view/
│   ├── entity_manager.gd       # 3D 实体 spawn/despawn
│   ├── entity_view.gd          # 3D 实体视图 (插值 + 动画)
│   ├── camera_controller.gd    # 相机控制（锁/自由 + 精准拖屏 + 边缘推屏 + 平滑开关 + 按住居中）
│   ├── skill_vfx.gd            # 技能 VFX
│   └── move_target_vfx.gd      # 右键 ping 标记
├── ui/
│   ├── health_bar_manager.gd   # 血条视图系统
│   ├── health_bar_ui.gd        # 血条视图组件
│   ├── stats_panel.gd          # HUD 面板
│   ├── bottom_hud.gd           # 底部 HUD（技能/物品/背包）
│   ├── settings_panel.gd       # 设置面板（相机/边缘推屏/平滑/全屏/cast 偏好）
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
