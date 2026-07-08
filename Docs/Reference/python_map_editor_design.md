# Python 地图编辑器设计文档

> 创建：2026-07-07
> 状态：设计稿（待实现）
> 目标：为 `data/maps/*.json` 提供一个轻量、实时预览的可视化编辑工具

---

## 1. 背景与动机

当前地图文件 `data/maps/default.json` 格式极简（仅 `name` / `bounds.half` / `walls[]`），开发时只能纯手打 JSON，盲改坐标、靠运行游戏才能看效果，迭代效率低。

目标：做一个**外部独立**的 Python 工具，不侵入 Godot 侧代码，实现：

1. **实时渲染** JSON 地图，所见即所得（接近游戏内俯视效果）
2. **Watchdog 热加载**：用任意编辑器（VSCode/vim）手改 JSON，窗口即时刷新
3. **（评估后追加）鼠标+键盘交互式编辑**：选中墙体、平移/加长/加粗/删除，自动写回 JSON

不在目标内：成为通用关卡编辑器、与 Sim 层联调玩法、运行游戏模拟。本工具只关心**地图几何**。

---

## 2. 地图数据模型（现状）

`data/maps/*.json`：

```json
{
  "name": "default",
  "bounds": { "half": 50 },
  "walls": [
    { "minX": -15, "minY": -1, "maxX": -10, "maxY": 1 }
  ]
}
```

语义（与 `src_cpp/sim/json_util.h` / `scripts/sim_bridge.gd` 一致）：

- `bounds.half`：世界为正方形 `[-half, half] × [-half, half]`
- `walls[i]`：AABB（轴对齐矩形），`min/max` 无须严格有序，加载时会 `std::min/std::max` 归一化
- 游戏内渲染：BoxMesh，center=`((minX+maxX)/2, 0.5, (minY+maxY)/2)`，size=`(maxX-minX, 1.0, maxY-minY)`
- 俯视角：屏幕 **X ↔ 世界 X**，屏幕 **Y ↔ 世界 Y**（Y 方向可选上/下，默认镜像成「Y 向下」以贴近 Godot 默认相机俯视观感）

编辑器**只读 / 只写**这三个字段，不引入新字段，保证生成的 JSON 能被 C++ 的 `parse_map_json` 直接吃掉。

---

## 3. 技术栈评估

| 方案 | 渲染 | 文件监听 | 交互编辑 | 学习/依赖成本 | 结论 |
|------|------|---------|---------|--------------|------|
| **pygame + watchdog**（推荐） | 原生快、抗锯齿矩形足够 | `watchdog` 库 | 原生鼠标/键盘事件 | 极低，单文件可跑 | ✅ MVP + 交互都能做 |
| PySide6/PyQt6 QGraphicsView | 工业级、可做属性面板 | 同上 | 选区/手柄原生支持 | 中，样板代码多 | 杀鸡用牛刀 |
| tkinter + Pillow | 需 Canvas 自绘 | 同上 | 麻烦，刷新闪烁 | 低但难看 | ❌ |
| 纯 CLI（不渲染，只校验） | — | — | — | — | 不满足「可视化」诉求 |

**结论**：采用 **pygame-ce**（社区维护版，Python 3.13 友好）+ **watchdog**。两个依赖即可。

> 复用项目已有的 `uv` 工作流：新增到 `pyproject.toml` 的 `[project.optional-dependencies] mapeditor = ["pygame-ce>=2.5", "watchdog>=4.0"]`，用 `uv run --extra mapeditor ...` 调起，不污染 default 依赖。

---

## 4. 目录布局

```
tools/
└── map_editor/
    ├── __init__.py
    ├── __main__.py          # 入口：`python -m tools.map_editor <map.json>`
    ├── viewer.py            # 渲染循环 + 摄像机
    ├── map_model.py         # JSON 读写 + Wall 数据类
    ├── watcher.py           # watchdog 封装（后台线程 → 通知主循环）
    ├── controller.py        # 鼠标/键盘事件 → 命令
    ├── commands.py          # Command 模式（Move/Resize/Add/Delete/Undo 队列）
    └── README.md            # 用法 + 快捷键速查
```

放置于仓库根的 `tools/` 下，与 `src_cpp/` / `scripts/` 平级。打包/构建不受影响（`project.godot` 不会扫描此目录）。

---

## 5. 坐标变换

世界到屏幕（视图层）：

```
screen_x = cx + world_x * zoom
screen_y = cy + world_y * zoom   # world_y 正方向默认朝下；可按 `--flip-y` 切换朝上
```

- `cx, cy`：屏幕中心对应的相机偏移；鼠标右键拖拽平移；滚轮缩放
- `zoom`：像素/单位，默认按窗口大小自动适配 `bounds.half`（让整张地图刚好放下）

屏幕到世界（命中测试用）：

```
world_x = (screen_x - cx) / zoom
world_y = (screen_y - cy) / zoom
```

墙体用 AABB 命中测试（含一点点拾取容差，1 像素级精度即可）。

---

## 6. MVP（Phase 1）—— 纯渲染 + Watchdog 热加载

**且仅做以下事**：

### 6.1 功能清单

- [x] 命令行：`uv run --extra mapeditor python -m tools.map_editor data/maps/default.json`
- [x] 加载 JSON → 渲染：bounds 外框、所有 walls（与游戏同色调 `Color(0.4,0.4,0.45)`）
- [x] watchdog 监听该文件：保存后毫秒级重解析、重绘
- [x] 解析失败时**不崩溃**，画一个红色「SYNTAX ERROR」覆盖层 + 上一份有效地图虚化保留
- [x] 相机：右键拖动平移、滚轮缩放、`F` 键复位到 Fit-Map、`G` 切换网格
- [x] HUD 文字：左上 `name | walls: N | half: H`；右下 `zoom: 1.0x`；底部解析状态
- [x] 窗口标题：`Map Editor — <path>`

### 6.2 渲染细节

- 背景：游戏地板暗色（#1a1a1f）
- bounds：虚线框 `#3a3a3f`
- 网格：每 1 单位一条浅灰，每 10 单位一条深灰
- 墙体：填充 `#666673`（≈游戏内 0.4/0.4/0.45），描边 `#9999a3`
- 选中预览（MVP 不做，留 Phase 2）
- 坐标轴：原点十字 `#444`，标 `X` / `Y`

### 6.3 Watchdog 设计

```python
# watcher.py 核心：后台线程，避免阻塞 pygame 主循环
class MapWatcher(FileSystemEventHandler):
    def __init__(self, path, on_change): self._cb = on_change; ...
    def on_modified(self, e):
        if Path(e.src_path).resolve() == self._target:
            # 防抖：编辑器多次 fsync 合并成一次回调
            schedule_debounced(self._cb, delay_ms=80)
```

关键点：
- 防抖 80ms（vim/VSCode 会写临时文件 + rename，避免抖动）
- 重解析在主循环 tick 里调用（watchdog 回调只设一个 dirty flag），避免跨线程访问 pygame
- 文件被原子替换（rename）也能命中：watchdog 推荐**同时监听目录**，过滤目标是 `.json` 的 modify/create/moved-to 事件

### 6.4 错误处理

`json.JSONDecodeError` 或字段缺失 → 进入 ERR 状态：

- 保留上一份 valid 的地图虚化呈现
- 顶部贴一条红色横幅显示错误行/列（`json` 标准库可拿到 line/col）
- 主循环照常跑，下一秒用户改对就自动恢复

---

## 7. Phase 2 评估 ── 鼠标/键盘交互编辑

### 7.1 可行性结论

**完全可行**，pygame 的鼠标拾取 + 键盘事件非常原生。复杂度低于 MVP 之外，但写回 JSON 有几处要小心：

- **格式保真**：用户可能手维护对齐空格、注释分组（虽然 JSON 没注释）。建议缩进用 `indent=2`，`walls` 数组每项一行紧凑（`{"minX":..., "minY":..., "maxX":..., "maxY":...}`），通过自定义 `json.dumps` 或后处理正则保持紧凑数组风格 ── 与现有 `default.json` 风格一致。
- **min/max 归一化**：交互中 `minX` 可能大于 `maxX`，写出时统一 `min=*, max=*`，避免 C++ 端碰运气。
- **删除壁等效**：直接 `walls.pop(i)`，记得清空 selection。

### 7.2 交互命令集

**选择**

| 操作 | 行为 |
|------|------|
| 单击墙体（空白处） | 选中（取消选中） |
| `Ctrl`/`Shift` + 单击 | 多选 / 加选 / 区域框选 |
| `Esc` | 清空选中 |

**平移**（保证 AABB 完整移动）

| 输入 | 行为 |
|------|------|
| 鼠标左键拖拽选中墙体 | 自由平移（snap 到 0.5 单位可切） |
| `W/A/S/D` 或方向键 | 每次按 1 单位移动 |
| `Shift` + 方向键 | 每次 5 单位 |
| `Alt` + 方向键 | 每次 0.1 单位（精细） |

**缩放（加长/加粗）**——保持中心不动，对称扩展

| 输入 | 行为 |
|------|------|
| `]` / `[` | 沿 X 轴加长/缩短 1 单位（对称） |
| `;` / `'` | 沿 Y 轴加粗/减薄 1 单位（对称） |
| `Shift` + 上述 | 步长 5 单位 |
| `Alt` + 上述 | 幅度 0.1（精细） |
| `R` | 旋转 90°（对 AABB 等于交换 w/h） |

> 「中心对称扩展」最符合直觉：墙体重心不变，只是变长变粗 → 不破坏对称布局可读性。
> 单边拉拽（拖角点）作为后续增强，需要 hit-test 角点。

**修改 / 删除**

| 输入 | 行为 |
|------|------|
| `Delete` / `Backspace` | 删除选中 |
| `N` | 在当前相机中心新建 1×1 单位墙体（自动选中） |
| `D` | 复制选中并平移 1 格（duplicate） |

**保存**

| 输入 | 行为 |
|------|------|
| `Ctrl+S` | 写回原文件（触发 watchdog 又会刷新一次，无害） |
| `Ctrl+Shift+S` | 另存为（弹文件路径输入框） |
| `Ctrl+Z` / `Ctrl+Y` | 撤销/重做（见 §7.4） |
| `Ctrl+Space` | 弹「inspect」浮层显示当前选中墙体的 4 个坐标值，可直接复制走 |

**冲突避免**：交互保存写回 JSON 后，watchdog 照样热重载，无需特殊处理（重解析结果一致）。

### 7.3 选中态视觉

- 描边高亮 `#ffd166`（暖色），2px
- 双击墙体进入「数值编辑」浮层（4 个数字输入框，回车应用）

### 7.4 撤销栈

- Command 模式，每个操作产生 `Command`（do/undo），入栈
- 节流：连续按键不合并；连续**同方向**移动每 200ms 切一段
- 栈上限 200，超出丢弃最旧
- 保存动作亦不可撤销（但会清空 dirty 标记）

### 7.5 ⚠️ 与人工手编 JSON 的协作

交互编辑和手打并存会有「手动改了但没看编辑器」的情况：

- **写入策略**：每次 `Ctrl+S` 用**最小化重排**（仅美化，不动用户已有顺序），并保留原 `name` / `bounds` 块顺序
- **读优先**：若 watchdog 检测到磁盘文件 mtime 新于 in-memory 模型且本地有未保存改动 → 弹出提示「外部已修改，覆盖 / 放弃本地？」，不静默吞
- 实际开发推荐：**纯手打靠 Phase 1，需要批量几何操作时再开 Phase 2 写回**，两种 Workflow 自然分离

---

## 8. 与 Godot 的关系

完全解耦。Sim 端 `sim_bridge.gd` 在 `_ready()` 时从 `res://data/maps/default.json` 一次性加载，**不监听文件**。因此：

- 编辑器改 JSON → 需要 Godot 调试场景**重启**（或重新进入 play）才生效
- 想真·联动热重载需要 Sim 端加文件监听 + 重建地图，**不在本期范围**
- 但 Phase 1 已经能极大缩短「改 JSON → 重启 → 看效果」循环中的「看效果」成本：编辑器侧秒级预览，确认 OK 再去 Godot 重启验证

---

## 9. 依赖与运行

`pyproject.toml` 增加：

```toml
[project.optional-dependencies]
mapeditor = [
    "pygame-ce>=2.5",
    "watchdog>=4.0",
]
```

运行：

```bash
uv run --extra mapeditor python -m tools.map_editor data/maps/default.json
# 可选参数
#   --watch / --no-watch       默认 watch 开
#   --flip-y                   Y 轴朝上（默认朝下以贴近 Godot 俯视）
#   --grid 1                   主网格单位
#   --zoom 8                    初始像素/单位
#   --width 1280 --height 800
```

兼容 Python 3.13（与项目一致），与现有 `build.py` / `main.py` 工具链同环境。

---

## 10. 实施路线

| 阶段 | 内容 | 验收标准 | 工作量估 |
|------|------|---------|---------|
| **P1** | §6 MVP：渲染 + watchdog + 相机 | 手改 JSON 0.1s 内刷新、错不崩 | 0.5d |
| **P2** | §7 选择 + 平移 + 加长加粗 + 删除 + 保存 | 能纯鼠标键盘改完一张图并写回 | 1d |
| **P3** | 撤销栈 + 多选 + 框选 + float 浮层 | 撤销正确、多选命令对单物体也成立 | 0.5d |
| **P4**（可选） | 缩圈预览圈层（`SimPlayerSnap` 半径圆）/ 生成点位置可视化 | 与 AI 设计文档对齐显示 spawn 区 | 视后续需求 |

---

## 11. 风险与取舍

| 项 | 风险 | 取舍 |
|----|------|------|
| JSON 不支持注释 | 用户想写分组注释 → 编辑器拒绝加载 | 让用户用文件名分隔或将「分组」信息存到 `walls[i].tag`（C++ 端会忽略未知 key）| 
| watchdog 在 macOS FSEvents 偶发漏 nvim swap | 极少 | 同时监听 **目录** 而非单文件，对 `*.json` 任何 modify/moved-to 都触发 |
| 写回破坏用户手编对齐 | 用户辛苦对齐被重排 | 自定义 serializer：walls 数组每项一行紧凑，顶层字段保持 indent=2 |
| 跑游戏时编辑器改了文件、Sim 没发现 | 用户预期落空 | README 说明清楚需重启 Godot 场景才生效 |

---

## 12. 不在本期范围（明确排除）

- 不能编辑 `SpawnPoint` / `Pickup` 等运行期实体（地图 JSON 里当前没有这些字段）
- 不集成到 Godot 编辑器做插件（`@tool` script），那是另一条路
- 不做网战级动画/粒子，编辑器保持 wireframe 风格
- 不做版本对比 / diff

---

## 13. 验收清单（实现后自测用）

- [ ] `default.json` 加载无误，渲染与游戏俯视观感相符（中心结构对称、四边 4 块墙位的布局正确呈现）
- [ ] 在 VSCode 改一个 `maxX` 保存后，编辑器窗口 100ms 内更新
- [ ] 故意删掉一个 `}`，编辑器不闪退、显示红色错误带，恢复后自动刷新
- [ ] `Ctrl+C` 终止不残留 watchdog 线程
- [ ] P2：选中第 3 块墙，`Shift+]` 长度加 5、`Alt+;` 加粗 0.1，`Ctrl+S` 写回，文件结构与原文件风格一致
- [ ] 写回的 JSON 被 C++ `parse_map_json` 直接食用，运行游戏墙位正确