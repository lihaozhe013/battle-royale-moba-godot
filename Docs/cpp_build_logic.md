# C++ Build Logic

> Build system: CMake + Ninja
> Entry point: `uv run build.py build`

---

## 快速起步

```bash
# 一键构建
uv run build.py build

# 如果更改了 CMake 配置或 build_profile，需要先清理缓存
uv run build.py distclean
uv run build.py build

# 更多选项
uv run build.py build --target template_release --jobs 8 -v
```

---

## 构建流程

```
build.py
  └─ 读取 build_env.yaml（工具链路径）
  └─ CMake Configure (-B build -G Ninja)
  └─ godot-cpp 绑定生成
  └─ CMake Build (Ninja)
       └─ godot-cpp（静态库）
       └─ topdown_sim（DLL → addons/topdown_sim/）
```

`build.py` 是对 CMake + Ninja 的轻量封装，读取 `build_env.yaml` 获取 MSVC/cmake/python 的路径。

---

## 二进制体积优化

项目使用三个手段将 GDExtension DLL 控制在 ~1.5 MB：

### 1. Build Profile（绑定裁剪）

`src_cpp/build_profile.json` — 只编译项目实际用到的 Godot 类绑定。

godot-cpp 默认为**全部 1051 个 Godot 类**各生成一个 .cpp，构成 434 MB 的静态库。实际上本项目只用到了 `RefCounted`（所有 Snap 类型 + SimServer 的基类）。

```json
{
  "enabled_classes": ["RefCounted"]
}
```

系统自动补全 `Object`（父类）以及 `ClassDB`、`WorkerThreadPool`、`FileAccess`、`Image`、`XMLParser`、`Semaphore` 等基础设施类。

**效果**：绑定代码从 1051 个 .cpp → 约 30 个，静态库从 434 MB → ~10 MB。

### 2. Release 模式

`template_release` → `-DCMAKE_BUILD_TYPE=Release` → `/O2 /Ob2 /DNDEBUG`

- 开启优化，关闭调试符号
- 没有 PDB（43 MB）和 ILK（23 MB）文件

### 3. LTO（链接时优化）

`set_property(TARGET topdown_sim PROPERTY INTERPROCEDURAL_OPTIMIZATION ON)`

- MSVC 下给编译器加 `/GL`，链接器加 `/LTCG`
- 跨模块消除死代码，再缩减 15-25%

---

## 构建目标（target）

| 目标               | CMake 配置     | 优化        | 调试符号 | 用途                                           |
| ------------------ | -------------- | ----------- | -------- | ---------------------------------------------- |
| `editor`           | Debug          | `/Od`       | 完整 PDB | **不需要**。在 Godot 编辑器中调试 C++ 断点时用 |
| `template_debug`   | RelWithDebInfo | `/O2`       | 完整 PDB | **不需要**。需要 attach 调试器时用             |
| `template_release` | Release        | `/O2 + LTO` | 无       | 日常使用。只跑游戏看效果，不调试 C++           |

---

## 文件结构

```
src_cpp/
├── build_profile.json      ← godot-cpp 绑定裁剪配置
├── CMakeLists.txt          ← CMake 主文件
├── SConstruct              ← SCons 配置（未使用，保留兼容）
├── godot-cpp/              ← GDExtension C++ 绑定（本地方修改）
├── register_types.cpp      ← Godot 类注册入口
├── sim_server.cpp          ← SimServer（GDScript 调用的 API 层）
├── sim/                    ← C++ Sim 层（ECS + Systems）
│   ├── world.cpp           ← 主循环
│   ├── components.h        ← ECS 组件
│   ├── snapshot_types.h    ← 快照数据类型（暴露给 GDScript）
│   ├── snapshot_bindings.cpp ← 快照的 _bind_methods
│   ├── snapshot_builder.cpp   ← 快照构建
│   └── systems/            ← 19 个 header-only System
└── third_party/
    ├── entt/               ← ECS 框架（header-only）
    └── glm/                ← 数学库（header-only）
```

---

## build_env.yaml 参考

```yaml
toolchain: msvc
msvc:
  vs_install_dir: 'C:/Program Files/Microsoft Visual Studio/2022/Community'
cmake_bin_dir: '' # 留空自动从 VS 或 PATH 检测
python3_executable: '...' # 默认用 .venv 或 uv 管理的 Python
build:
  target: template_release
  jobs: 0 # 0 = 自动（CPU 数 - 1）
  verbose: false
```

---

## SCons vs CMake

- **CMake**（当前主构建系统）：通过 `build.py` + Ninja 调用，速度快
- **SConstruct**（遗留）：项目早期使用，现在保留未删但不再维护

如果直接运行 `scons` 会导致编译目标不一致或体积膨胀。始终用 `uv run build.py build`。

---

## 常见问题

### Q: 改了什么后需要 `distclean`？

需要重新生成绑定的情况：

- 修改了 `build_profile.json`
- 更新了 godot-cpp 版本
- 修改了 CMakeLists.txt 中的 GODOTCPP_* 选项

`build.py distclean` 会删除整个 `build/` 目录和已有的 DLL，下次 `build.py build` 从头构建。

### Q: 需要调试 C++ 怎么办？

临时切到 `template_debug`：

```bash
uv run build.py build --target template_debug
```

用 VS 打开 `build/` 目录（有 CMake 预设），附加到 Godot 进程，打断点调试。

### Q: LTO 让编译变慢怎么办？

LTO 只在最终链接阶段耗时，日常开发可以注释掉 `INTERPROCEDURAL_OPTIMIZATION` 那行来加速。发布前再打开。
