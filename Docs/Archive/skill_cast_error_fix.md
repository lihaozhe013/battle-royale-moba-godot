# 施法错误弹窗修复记录

> 创建：2026-07-09
> 关联：`skill_system_design.md` §13（输入边沿/状态机陷阱）
> 涉及：`src_cpp/sim/systems/skill_cast.h`、`scripts/sim_bridge.gd`

---

## 问题描述

按下技能键（C/E/R/F）后左键确认施法失败时，CastErrorLabel 无法显示错误提示（如"On Cooldown"、"No target selected"）。但**施法成功的读条可以正常工作**。

## 根因分析

Bug 位于 `skill_cast.h` 的状态机中：**验证逻辑在两个阶段重复执行，但错误显示只检测其中一种状态转换路径**。

### 修复前的验证逻辑

```
None 阶段（按技能键时）:
  ├─ 检查 CD → 失败 → CastError=1, 状态保持 None(0)
  ├─ 检查 Mana → 失败 → CastError=2, 状态保持 None(0)
  └─ 检查 Target(MeleeSingle) → 失败 → CastError=4, 状态保持 None(0)
       ↓ 以上全部通过
Aiming 阶段（左键确认时）:
  ├─ 再次检查 Target(MeleeSingle) → 失败 → State=None + CastError=4
  └─ 全部通过 → CD/Mana扣除 → 进入 Casting 读条
```

**问题**：None 阶段的验证失败时，`CastState` **从未进入 Aiming**，状态一直为 `None(0)`。`sim_bridge.gd` 的错误显示逻辑检测的是从 `Aiming(1)/Casting(2)` → `None(0)` 的转换：

```gdscript
if prev_state >= 1 and p.cast_state == 0:  # ← 需要 prev_state 曾是 >= 1
    if p.cast_error > 0:
        cast_error_layer.show_error(p.cast_error)
```

由于 `prev_state` 从未达到 `>= 1`，条件不满足 → 错误永远不会弹出。瞄准绿线也不会显示（`skill_vfx.gd:37` 仅在 `cast_state == 1` 时绘制）。

**成功路径能工作的原因**：None→Aiming→Casting 状态转换完整，`cast_state >= 2` 触发施法条。

## 修复方案

### 核心思路

按用户预期：**按技能键 = 不检测任何东西；左键确认 = 只检测一次**。

将**所有验证**从 None 阶段移到 Aiming 阶段的 `cast_confirm` 分支：
- None 阶段只初始化 Aiming 状态（不检查 CD/Mana/Target）
- Aiming 确认时统一检查 CD(1)、Mana(2)、Target(4)，任一失败 → 状态回到 None + 错误码 + 触发 sim_bridge 的 `prev_state>=1→0` 路径 → 错误弹窗

### 改动 1：简化 None 阶段

**文件**：`src_cpp/sim/systems/skill_cast.h:121-163`

**前**：按技能键时检查 CD/Mana/Target，失败则保持 None
**后**：按技能键只验证 `SkillId > 0`，直接进入 Aiming

```cpp
case CastState::Phase::None: {
    if (cast_slot >= 0 && cast_slot < 4 && cs.RejectTimer <= 0.0f) {
        auto &slot = skills.Slots[cast_slot];
        if (!(slot.SkillId > 0)) break;
        entt::entity tgt = resolve_target(input.CastTargetId);
        cs.State = CastState::Phase::Aiming;
        cs.ActiveSlot = cast_slot;
        cs.EnteredSlot = cast_slot;
        cs.RejectTimer = 0.15f;
        cs.SkillId = slot.SkillId;
        cs.TargetEntity = tgt;
        cs.Timer = 0.0f;
        cs.SubTimer = 0.0f;
        cs.AimPos = cast_aim;
        cs.CastError = 0;
    }
    break;
}
```

### 改动 2：扩展 Aiming 确认验证

**文件**：`src_cpp/sim/systems/skill_cast.h:167-202`

**前**：Aiming 确认只检查 Target（MeleeSingle），CD/Mana 假设 None 阶段已通过
**后**：Aiming 确认检查 CD→Mana→Target（按此顺序），任一失败→回到 None+错误

验证顺序和错误码：
| # | 检查 | 错误码 | 说明 |
|---|------|--------|------|
| 1 | `CooldownTimer > 0` | 1 (On Cooldown) | CD 尚未结束 |
| 2 | `Mana.Cur < cost` | 2 (Not enough Mana) | 法力不足 |
| 3 | `target_alive + target_in_range` | 4 (No target selected) | 目标无效/超出范围 |

全部通过 → 扣除 Mana + 设置 CD + 进入 Casting（原有逻辑不变）。

### 改动 3（可选改进）：简化 Aiming 槽位切换

**文件**：`src_cpp/sim/systems/skill_cast.h:211-225`

**前**：Aiming 中切换技能槽时会检查新槽的 CD 和 Mana
**后**：Aiming 中切换技能槽不检查 CD/Mana（统一在确认时验证），只验证 `SkillId > 0`

## 验证要点

- **按 C（无目标）**：None→Aiming（绿线显示）→左键确认→Target 失败→None+CastError=4→错误弹窗 ✅
- **按 C（有目标）→左键→施法**：None→Aiming→Casting→读条 ✅（回归）
- **E/R/F（无目标需求）**：None→Aiming→左键确认→CD/Mana 检查→通过→Casting ✅
- **CD 中按 C**：None→Aiming（绿线保持）→左键→CD 失败→错误弹窗 ✅
- **Mana 不足按 C**：同上 ✅
- **打断返还**：Casting 中移动/H→`refund_cast` 返还 Mana+清 CD → 行为不变 ✅
- **错误码映射**：1=On Cooldown, 2=Not enough Mana, 3=Stunned, 4=No target selected, 5=Target unavailable → 全部正确 ✅
- **连续同错误**：失败后返回 None → 再次按 C 进 Aiming → `CastError=0` → `_prev_player_cast_error` 重置 → 下次失败仍可弹窗 ✅

## 涉及文件

| 文件 | 改动类型 |
|------|---------|
| `src_cpp/sim/systems/skill_cast.h` | ✅ 3 处修改（None/Aiming/slot-switch） |
| `scripts/sim_bridge.gd` | ❌ 无需改动（错误显示逻辑正确） |
| `scripts/ui/cast_error.gd` | ❌ 无需改动 |
| `scenes/ui/cast_error.tscn` | ❌ 无需改动 |
| `scripts/view/skill_vfx.gd` | ❌ 无需改动（绿线已依赖 cast_state==1） |
| `src_cpp/sim/snapshot_builder.cpp` | ❌ 无需改动（cast_error 已导出） |
