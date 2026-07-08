# Python 地图编辑器 — 设计文档 & 实现记录

> 创建：2026-07-07 | 最后更新：2026-07-07
> 状态：✅ **P1-P4 全部完成**
> 源码：`tools/map_editor/`

---

## 1. 背景

地图文件 `data/maps/*.json` 格式极简（仅 `name` / `bounds.half` / `walls[]`），开发时需手打 JSON 盲改坐标。本工具提供可视化预览 + 交互编辑，不侵入 Godot 侧代码。

---

## 2. 数据模型

```json
{
  "name": "default",
  "bounds": { "half": 50 },
  "walls": [
    { "minX": -15, "minY": -1, "maxX": -10, "maxY": 1 }
  ]
}
```

- `bounds.half`：世界 `[-half, half] × [-half, half]`
- `walls[i]`：AABB，`min/max` 无须严格有序（编辑器写回时自动归一化）
- 游戏内渲染：BoxMesh，center=`((minX+maxX)/2, 0.5, (minY+maxY)/2)`，size=`(maxX-minX, 1.0, maxY-minY)`

---

## 3. 目录结构

```
tools/
├── map_editor_config.yaml          ← 编辑器配置（地图路径/窗口/zone 等）
├── map_editor_config.example.yaml  ← 配置模板
├── map_editor_help.txt             ← 帮助面板文本（按 H 显示）
└── map_editor/
    ├── __init__.py
    ├── __main__.py          ← 入口
    ├── viewer.py            ← 渲染循环 + 交互逻辑
    ├── map_model.py         ← JSON 读写 + 数据类
    ├── watcher.py           ← watchdog 热加载
    └── commands.py          ← 撤销/重做栈
```

---

## 4. 运行

```bash
uv run python -m tools.map_editor
# 或
make edit-map
```

配置在 `tools/map_editor_config.yaml`，无需命令行参数。

---

## 5. 功能清单（全部已完成）

| Phase | 内容 | 状态 |
|-------|------|------|
| **P1** | 渲染 + watchdog 热加载 + 相机（平移/缩放/Fit/网格） | ✅ |
| **P2** | 选中/拖拽/WASD 移动/[]缩放/R 旋转/N 新建/D 复制/Delete 删除/Ctrl+S 保存 | ✅ |
| **P3** | 撤销/重做(Ctrl+Z/Y) + 多选(Ctrl/Shift+点击) + 框选 + 双击数值编辑 + 坐标检视(Ctrl+Space) + 另存为(Ctrl+Shift+S) | ✅ |
| **P4** | 缩圈预览圈层（Z 开关，yaml 配置圆心/半径） | ✅ |

### 快捷键

| 操作 | 按键 |
|------|------|
| 选中 | 左键单击 |
| 多选切换 | Ctrl+点击 |
| 加选 | Shift+点击 |
| 框选 | 空白处拖拽 |
| 移动选中 | WASD / 方向键（Shift×5 / Alt×0.1） |
| 缩放选中 | ]/[ X 轴 ;/' Y 轴（Shift×5 / Alt×0.1） |
| 旋转 90° | R |
| 新建墙体 | N |
| 复制 | Ctrl+D |
| 删除 | Delete / Backspace |
| 保存 | Ctrl+S |
| 另存为 | Ctrl+Shift+S |
| 撤销/重做 | Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z |
| 坐标检视 | Ctrl+Space |
| 数值编辑 | 双击墙体 |
| 缩放圈 | Z |
| 网格 | G |
| Fit 地图 | F |
| 帮助面板 | H |
| 退出 | Esc |

---

## 6. 与 Godot 的关系

完全解耦。Sim 端 `sim_bridge.gd` 在 `_ready()` 时一次性加载地图 JSON。编辑器改文件后需重启 Godot 场景才生效。

---

## Archive（历史设计笔记）

以下为开发过程中的设计笔记，保留供参考。

### 技术栈评估过程

| 方案 | 结论 |
|------|------|
| **pygame-ce + watchdog** | ✅ 采用 |
| PySide6/PyQt6 | ❌ 样板多，杀鸡用牛刀 |
| tkinter | ❌ 效果差 |
| 纯 CLI | ❌ 不满足可视化诉求 |

### 实施路线（原始规划）

```
P1 渲染+watchdog+相机      0.5d  ✅
P2 选择+平移+缩放+删除+保存  1d    ✅
P3 撤销+多选+框选+浮层      0.5d  ✅
P4 缩圈预览圈层             可选  ✅
```

### 关键取舍（已解决）

- **JSON 格式保真**：自定义 `serialize_map()`，walls 数组每项一行紧凑，不引入新字段
- **watchdog 防抖**：80ms 防抖 + 监听目录而非单文件
- **键盘重复**：pygame 内置 `set_repeat(1000, 50)`，一发触发 + 1s 后连发，单发键跳过重复
- **撤销栈**：Command 模式，上限 200 步，保存清空
- **保存→watchdog 冲突**：`_suppress_reload` 标记避免保存触发重加载

### 不在本次实现范围（未来可能）

- 编辑 `SpawnPoint` / `Pickup` 等运行期实体（地图 JSON 当前不含这些字段）
- Godot 编辑器插件（`@tool` script）
- 单边拉拽（拖墙体角点）
- 版本对比 / diff
- 与 Sim 层联动热重载地图
