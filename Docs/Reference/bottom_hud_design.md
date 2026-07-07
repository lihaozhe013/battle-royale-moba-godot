# Bottom HUD 设计文档

> 创建日期：2026-07-07
> 关联：`prompt.md`、`sim_system_reference.md`

---

## 1. 整体布局

```
┌──────────────────────────────────────────────────────────────────────┐
│  ┌──────┐  ┌─────────────────────┐  ┌──┬──┬──┬──┐  ┌──┬──┬──┬──┬──┬──┐ │
│  │      │  │ HP ████████████ 200 │  │Q │W │E │R │  │1 │2 │3 │4 │5 │6 │ │
│  │Avatar│  │ MP ████░░░░░░░  50  │  │  │  │  │  │  │  │  │  │  │  │  │ │
│  │      │  │ Lv5 ATK:10 Kills:0  │  └──┴──┴──┴──┘  └──┴──┴──┴──┴──┴──┘ │
│  └──────┘  └─────────────────────┘   Skills         Items            │
│                                                      ┌──┬──┬──┬──┬──┬──┐│
│                                                      │  │  │  │  │  │  ││
│                                                      └──┴──┴──┴──┴──┴──┘│
│                                                       Backpack (预留)   │
└──────────────────────────────────────────────────────────────────────┘
```

## 2. Scene 划分

| Scene | 用途 | 实例化数量 |
|-------|------|-----------|
| `skill_slot_ui.tscn` | 单个技能槽（Q/W/E/R） | 4 |
| `item_slot_ui.tscn` | 单个物品槽（物品栏+背包通用） | 12 |
| `bottom_hud.tscn` | 底部 HUD 主场景 | 1（挂到 main.tscn） |

## 3. 完整节点树

```
BottomHUD (CanvasLayer)                          ← 根节点，layer=100
└── HUDPanel (Control)                           ← 背景+裁剪容器
    └── HUDContainer (HBoxContainer)             ← 水平排列所有分区
        │
        ├── AvatarSection (VBoxContainer)        ← 英雄头像区
        │   └── Avatar (TextureRect)             ← 头像图片，64×64
        │
        ├── ResourceSection (VBoxContainer)      ← 血条/蓝条/属性区
        │   ├── HPContainer (Control)            ← HP 条容器
        │   │   ├── HPBackground (ColorRect)     ← 深色底
        │   │   ├── HPFill (ColorRect)           ← 绿色填充
        │   │   └── HPLabel (Label)              ← "200/200"
        │   ├── ManaContainer (Control)          ← Mana 条容器
        │   │   ├── ManaBackground (ColorRect)
        │   │   ├── ManaFill (ColorRect)         ← 蓝色填充
        │   │   └── ManaLabel (Label)            ← "50/100"
        │   └── StatsLabel (Label)              ← "Lv5 ATK:10 ASP:1.0 Kills:0"
        │
        ├── SkillSection (HBoxContainer)         ← 技能栏
        │   ├── SkillSlotUI (instance)           ← Q 槽
        │   ├── SkillSlotUI (instance)           ← W 槽
        │   ├── SkillSlotUI (instance)           ← E 槽
        │   └── SkillSlotUI (instance)           ← R 槽
        │
        ├── ItemSection (HBoxContainer)          ← 物品栏（6 格）
        │   ├── ItemSlotUI (instance) × 6
        │
        └── BackpackSection (HBoxContainer)      ← 背包栏（6 格，预留）
            ├── ItemSlotUI (instance) × 6
```

## 4. 各节点类型选型理由

| 节点 | 类型 | 为什么选这个类型 |
|------|------|----------------|
| BottomHUD | `CanvasLayer` | 2D 屏幕空间叠加层，独立于 3D 相机 |
| HUDPanel | `Control` | 纯容器，用户可自行加 ColorRect 做背景 |
| HUDContainer | `HBoxContainer` | 自动水平排列子节点，间距统一 |
| AvatarSection | `VBoxContainer` | 垂直排列（未来可能加等级徽章在头像下） |
| Avatar | `TextureRect` | 显示图片，支持缩放模式 |
| ResourceSection | `VBoxContainer` | 垂直排列 HP/Mana/Stats |
| HPContainer / ManaContainer | `Control` | 固定尺寸容器，内部用绝对定位的 ColorRect |
| HPBackground / HPFill | `ColorRect` | 纯色矩形，Fill 宽度由脚本控制 |
| HPLabel / ManaLabel | `Label` | 显示数值文字 |
| StatsLabel | `Label` | 单行属性文字 |
| SkillSection | `HBoxContainer` | 水平排列 4 个技能槽 |
| ItemSection / BackpackSection | `HBoxContainer` | 水平排列 6 个物品槽 |

## 5. SkillSlotUI 内部结构

```
SkillSlotUI (Control)                  ← 48×48
├── Icon (TextureRect)                 ← 技能图标，填满父节点
├── CooldownMask (ColorRect)           ← 冷却遮罩，宽度由脚本控制
├── LevelLabel (Label)                 ← 右下角，技能等级
├── ManaCostLabel (Label)              ← 右上角，法力消耗
└── KeyHint (Label)                    ← 左下角，"Q"/"W"/"E"/"R"
```

## 6. ItemSlotUI 内部结构

```
ItemSlotUI (Control)                   ← 40×40
├── Icon (TextureRect)                 ← 物品图标
├── CountLabel (Label)                 ← 右下角，堆叠数量
├── CooldownMask (ColorRect)           ← 主动物品冷却遮罩
└── KeyHint (Label)                    ← 左下角，"1"-"6"
```

## 7. 脚本职责

### `bottom_hud.gd`

| 方法 | 参数 | 职责 |
|------|------|------|
| `_ready()` | — | 收集 SkillSection/ItemSection/BackpackSection 下的子节点 |
| `sync_player(p)` | `SimPlayerSnap` | 更新 HP/Mana 条宽度+文字、StatsLabel |
| `sync_skills(skills)` | `Array<SimSkillSlotSnap>` | 更新 4 个技能槽的冷却/等级/法力状态 |
| `sync_items(items)` | `Array` | 更新物品栏图标（预留，暂时空实现） |

### `skill_slot_ui.gd`（已有）

| 方法 | 职责 |
|------|------|
| `set_skill(id, level)` | 设置技能 ID 和等级 |
| `set_cooldown(ratio)` | 设置冷却遮罩宽度 (0.0~1.0) |
| `set_mana_state(enough)` | 法力不足时图标变红 |
| `set_key_hint(text)` | 设置按键提示文字 |
| `reset()` | 重置为空槽 |

### `item_slot_ui.gd`

| 方法 | 职责 |
|------|------|
| `set_item(id, count)` | 设置物品 ID 和堆叠数 |
| `set_cooldown(ratio)` | 主动物品冷却遮罩 |
| `set_key_hint(text)` | 设置按键提示 |
| `reset()` | 重置为空槽 |

## 8. 数据流

```
SimSnapshot (30Hz)
  └─ sim_bridge.gd._process
      └─ bottom_hud.sync_player(snap.players[0])
          ├─ HPFill.size.x = BAR_WIDTH × hp_ratio
          ├─ HPLabel.text = "hp/max_hp"
          ├─ ManaFill.size.x = BAR_WIDTH × mana_ratio
          ├─ ManaLabel.text = "mana/max_mana"
          ├─ StatsLabel.text = "Lv.. ATK.. ASP.. Kills.."
          └─ skill_slots[i].set_cooldown(...)
```

## 9. 需创建的文件清单

| 文件 | 类型 | 说明 |
|------|------|------|
| `scenes/ui/skill_slot_ui.tscn` | Scene | 技能槽模板 |
| `scenes/ui/item_slot_ui.tscn` | Scene | 物品槽模板 |
| `scenes/ui/bottom_hud.tscn` | Scene | 底部 HUD 主场景 |
| `scripts/ui/item_slot_ui.gd` | Script | 物品槽脚本 |
| `scripts/ui/bottom_hud.gd` | Script | HUD 主脚本 |

`scripts/ui/skill_slot_ui.gd` 已存在，无需新建。

## 10. 编辑器后续手动调整项

创建完节点后，用户需在编辑器中调整：

1. **HUDPanel** — 锚点设为 Bottom Wide，调整 offset 使其贴底
2. **HUDContainer** — separation 间距、margin
3. **Avatar** — 拖入头像纹理
4. **HPContainer / ManaContainer** — custom_minimum_size 设为 (200, 14)
5. **HPFill / ManaFill** — color 设为绿/蓝
6. **各 Label** — font_size、font_color
7. **SkillSection / ItemSection** — separation 间距
8. **每个 SkillSlotUI 实例** — 无需改，脚本自动设置 KeyHint
9. **将 bottom_hud.tscn 实例挂到 main.tscn**
