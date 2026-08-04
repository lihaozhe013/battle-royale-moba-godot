# AGENTS.md — Operations Guide

> **Project**: Battle Royale MOBA (Godot 4.7, top-down)
> **Architecture**: C++ GDExtension ECS (Sim, 30Hz) + GDScript (View, 60Hz); Sim has zero Godot dependency.
> **Purpose of this file**: rules of engagement for AI agents. The project state-of-truth is `CONTEXT.md` — read it first.

---

## 1. Core Principles (do not violate)

1. **Read `CONTEXT.md` first.** It is the only source of truth for the current project state. Code is truth; design docs (`Docs/`) are secondary and may lag behind.
2. **The architecture is fixed.** `entt::registry` + header-only `inline` Systems + `CommandBuffer` (deferred entity creation/destruction) + `Snapshot` (the only Sim→View channel). Do not introduce globals or cross-System function-call communication.
3. **Input mode is MOBA-only.** Right-click ground to move + Q/W/E/R skills + A for attack command. The WASD mode was permanently removed; any reference to it in older docs is obsolete and misleading. Authoritative source: `Docs/Reference/input_system_design.md` and `CONTEXT.md`.
4. **Code is the source of truth, not docs.** Do not modify code to match what a doc says — only update docs when the code legitimately changes. When in doubt, trust the code.
5. **All written content is in English.** All code comments, commit messages, PR descriptions, and documentation (markdown, inline, doc comments) must be written in English. No Chinese or other languages in code, comments, or `Docs/`. If a user request arrives in another language, the response and any artifacts you create must still be in English.

---

## 2. Workflow Rules

1. **Read before edit.** Always `Read` a file before modifying it. Prefer `Edit` over `Write` (only use `Write` for new files). Run independent operations in parallel.
2. **No comments by default.** Code is self-documenting. Add comments only when explicitly asked.
3. **Do not touch git config.** Do not configure git, do not respond to interactive prompts, do not force-push.
4. **Commit only when asked.** Do not auto-commit, push, or open PRs. Before committing, check `git status` + `git diff` and stage only the intended files.
5. **Prefer `rg` and `fd`.** Much faster than `grep` / `find`. Fall back only when they are unavailable.
6. **Use Git Bash by default.** Assume the user runs commands in Git Bash. Do not add or recommend PowerShell commands, and do not close, restart, or shut down Windows.
7. **Verify after changes.** Run the project's lint / typecheck / test commands. If none are obvious, ask the user.
8. **Never build C++ on your own.** Use `make build` or `make rebuild`. Do not invoke Python or CMake directly.
9. **Be concise.** No preamble, no recap of the user's request, no emojis. One sentence is better than two.
10. **Docs follow code.** Architectural changes must update `CONTEXT.md` and the relevant `Docs/Reference/*.md`. Use `todowrite` to plan multi-step doc sync.

---

## 3. Where to Look First (Documentation Map)

When you need to understand the codebase, follow this priority order.

| Need | Primary doc | Notes |
| --- | --- | --- |
| Current state, file structure, tick order, status of recent work | `CONTEXT.md` | Read first, every session. |
| C++ Sim components, systems, snapshot fields, constants | `Docs/Reference/sim_system_reference.md` | Full reference table; field-by-field. |
| End-to-end data flow (input → sim → snapshot → view) | `docs/DATA_FLOW.md` | Mermaid diagrams; cross-layer sequence examples. |
| Input system (four-layer framework, FSM, command flow) | `Docs/Reference/input_system_design.md` | Sole authority on input behavior. |
| Hero + Skill architecture | `Docs/Reference/hero_skill_architecture.md` | P1–P5 refactor record. |
| Bot AI v4 (three-layer state machine) | `Docs/Reference/bot_ai.md` | Goal / Combat / Skill layers. |
| High-level design + MOBA upgrade roadmap | `Docs/Reference/prompt.md` | Gameplay vision + backlog. |
| Historical designs (HUD, camera, map editor) | `Docs/Archive/` | Reference only; do not act on without checking. |

**Docs may be stale.** Use them to locate the right entry point, then verify against the actual code. If a doc contradicts the code, trust the code and update the doc (or flag the discrepancy to the user) — do not modify code to match a doc.

---

## 4. Special Conventions

1. **File roles.**
   - `AGENTS.md` (this file) — operations guide.
   - `CONTEXT.md` — project context snapshot, the source of truth.
   - `Docs/Reference/` — design + reference docs.
   - `Docs/Archive/` — historical designs; read-only context.
   - `docs/` — operational docs (data flow, build notes).

2. **Bot AI is a Hero.** Bots are AI-controlled heroes. There is no `bot_combat` system; bots go through the exact same combat pipeline as the player via `HeroInputState`. See `Docs/Reference/bot_ai.md`.

3. **Skills are independent of Hero definitions.** A skill is a standalone `ISkill` implementation. Heroes reference skills through `SkillComponent.Slots[i].SkillId`, not by inheriting or composing them. See `Docs/Reference/hero_skill_architecture.md`.

4. **C++ Sim layer has zero Godot dependency.** No `godot::` types inside Systems. Only `sim_server.h/.cpp` and `snapshot_*` glue Godot bindings on the outside.

5. **System signature convention.** All Systems are `inline void` free functions in the `sim` namespace. Parameter order: `entt::registry &reg` first, then `float dt`, then any other dependencies (`CommandBuffer &cb`, RNG refs, etc.).

6. **Deferred entity mutation.** Never `_reg.create()` or `_reg.destroy()` inside a System. Push to `CommandBuffer`; the world flushes at the end of each tick.

7. **Snapshot is the only Sim→View channel.** No shared memory, no signals, no event bus. The `SimSnapshot` is a `RefCounted` produced by `snapshot_export_system` and consumed via `sim.pop_snapshot()`.

---

## 5. Common Anti-Patterns to Avoid

| Don't | Why | Do |
| --- | --- | --- |
| View directly modifies `entt::registry` | Breaks Sim authority, causes 30/60Hz desync | Use `SimServer.set_*_command` + `pop_snapshot` |
| `_reg.create()` / `destroy()` inside a System | Invalidates iterators | `CommandBuffer.push()` then `_cb.flush()` at tick end |
| Cross-System function calls for state sharing | Hidden coupling, hard to refactor | Read/write shared components |
| Per-frame node rebuild in View | Performance disaster | Pool + LERP via `EntityManager.sync_entities` |
| Push input edge events to Sim immediately per frame | Loses events at 30Hz / 60Hz boundary | Queue in `InputEventQueue`, drain via `CommandBuffer` across ticks |
| Bot AI writing `Position2D` directly | Bypasses pathfinding + wall collision | Inject into `HeroInputState`, let `pathfinding` + `movement` handle it |
| Add a snapshot field without updating bindings | GDScript reads `null` | Update `snapshot_types.h` + `snapshot_bindings.cpp` + `snapshot_builder.cpp` together |
| Reference Godot types inside `sim/` | Compile failure or runtime crash | Sim is `glm::vec2`-based; Godot types only at the binding layer |

---

## 6. Quick Verification Checklist

Before declaring a task done, confirm:

- [ ] No new comments added (unless asked); any added comment is in English.
- [ ] All new or modified documentation is in English.
- [ ] `git status` shows only intended files changed.
- [ ] `git diff` is consistent with the requested change.
- [ ] If architecture changed: `CONTEXT.md` + relevant `Docs/Reference/*.md` updated.
- [ ] If input behavior changed: `Docs/Reference/input_system_design.md` still authoritative.
- [ ] If Sim API changed: `Docs/Reference/sim_system_reference.md` §8 (`SimServer` API) updated.
- [ ] If new snapshot field added: `register_types.cpp` registers the new GDCLASS, and the three snapshot files are all updated.
- [ ] No C++ build run on your own.
- [ ] No git config touched, no force-push, no auto-commit.
