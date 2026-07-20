# 约法三章 — AI Agent 操作守则

> 最后更新：2026-07-20

---

## 第一章：项目共识

1. **先读上下文** — 操作前必须阅读 `CONTEXT.md`（原 `AGENTS.md`）。这是项目当前状态的唯一真相来源。C++ Sim ECS (30Hz) + GDScript View (60Hz)，Sim 层零 Godot 依赖。
2. **架构原则不改** — `entt::registry` + header-only `inline` System + `CommandBuffer`（延迟实体创建/销毁）+ `Snapshot`（Sim→View 唯一通道）。不引入全局变量、不跨 System 函数调用通信。
3. **输入只此一家** — MOBA 模式（右键点地板移动 + Q/W/E/R 4 技能 + A 键普攻命令）是唯一输入模式。WASD 模式已永久移除，任何旧文档中相关描述均为过时误导，以 `Docs/Reference/input_system_design.md` 和 `CONTEXT.md` 为准。

## 第二章：守则

1. **先读后改** — 修改文件前必须 Read。优先用 `edit` 而非 `write`（除非新建）。批量独立操作并行。
2. **代码默认无注释** — 不添加任何注释，除非被要求。代码自说明。
3. **不动.git配置** — 不修改 git 配置、不应答交互、不 force-push。
4. **仅应要求提交** — 不主动 commit/push/PR。commit 前检查 `git status` + `git diff`，只 stage 目标文件。
5. **优先 rg/fd** — 搜索文件和内容优先用 `rg`（ripgrep）和 `fd`（fd-find），远比 `grep` / `find` 快。系统找不到 `rg` / `fd` 时才用 `grep` / `find`。
6. **修改后验证** — 运行项目提供的 lint/typecheck/test 命令。若找不到，问用户。
7. **C++ Build 禁用** — 不要擅自运行 C++ 层的构建。如需构建，必须用 `make build` 或 `make rebuild` 命令，不得使用 Python 或 CMake 直接调用。
8. **用词精炼** — 直接回答问题，不加前/后叙、不加解释、不重复用户话。一句话能回答不用两句。杜绝 emoji。
9. **文档跟随代码** — 任何架构变更必须同步更新 `CONTEXT.md` 和相关 `Docs/Reference/*.md`。推荐先用 `todowrite` 列清单再逐项推进。

## 第三章：特别约定

1. **文件路径** — AGENTS.md 仅为本文（操作守则）。项目上下文快照在 `CONTEXT.md`。
2. **Bot AI** — Bot 是 AI 控制的 Hero，不使用独立的 `bot_combat` 系统，而是通过 `HeroInputState` 走与玩家完全相同的战斗链路。详见 `Docs/Reference/bot_ai.md`。
3. **Skill** — 技能是独立的 `ISkill` 实现类，不隶属于 Hero 定义。英雄通过 `SkillComponent.Slots[i].SkillId` 引用技能。详见 `Docs/Reference/hero_skill_architecture.md`。
4. **非侵入** — 设计文档（`Docs/` 目录）不是代码，改动只需与代码事实一致。不因为文档写了什么就去改代码；代码是唯一事实来源。
