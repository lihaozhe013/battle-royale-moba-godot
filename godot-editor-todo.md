# Godot 编辑器待办事项

> 最后更新：2026-07-06
> 当前阶段：P2 — 后续优化

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
| 12 | 创建 health_bar_ui.tscn（Control 根节点 + 3 个 ColorRect 子节点）| ✅ |
| 13 | 挂 health_bar_ui.gd 脚本到根节点 | ✅ |
| 14 | main.tscn 添加 HealthBarManager 节点 | ✅ |
| 15 | 验证血条（阵营色 / DamageBar 延迟 / 死亡隐藏 / 穿墙可见 / 无抖动）| ✅ |

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
