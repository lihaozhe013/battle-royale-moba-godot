# AGENTS.md

Instructions for agents working in this repository. Keep this file concise and
project-specific. Code and configuration are authoritative; documentation is
only a short navigation aid.

## Project structure

```text
src_cpp/                C++20 GDExtension simulation and ECS
scripts/                GDScript bridge, input, UI, camera, and view/VFX code
scenes/                 Godot scenes
resources/              Runtime assets and VFX
data/                   Map, skill, and gameplay data
addons/                 Godot extension files and generated native output
tools/                  Python tooling, including the map editor
Docs/cpp_build_logic.md Short native-build reference
README.md               High-level project orientation
Makefile                Build, format, and tooling entry points
project.godot           Godot project configuration
```

Inspect the code when a detail is not obvious. Do not assume a design document
describes current behavior.

## Architecture constraints

- C++ simulation is authoritative; GDScript is the Godot-facing view and input
  layer.
- Simulation systems under `src_cpp/sim/` must not depend on Godot types.
- Use EnTT components for shared state. Do not add globals or direct
  cross-system calls.
- Systems are `inline void` free functions in the `sim` namespace and receive
  the registry first.
- Defer entity creation and destruction through `CommandBuffer`; do not mutate
  the registry directly while systems iterate it.
- `SimSnapshot` is the only simulation-to-view channel. The view sends commands
  through the `SimServer` API and never accesses the ECS registry.
- Player and bot heroes use the same gameplay pipeline.
- Keep the MOBA input model: ground right-click movement, Q/W/E/R skills, and A
  attack commands.

## Working rules

- Read a file before editing it. Preserve existing structure and formatting.
- Keep changes focused. Do not refactor unrelated code or add speculative
  abstractions.
- Add comments only when they explain something the code cannot make clear;
  write all comments and other repository text in English.
- Prefer `rg` and `fd` for discovery. Use `uv run` for Python tooling.
- Do not build C++ unless the task requires it. When a native build is needed,
  use the Makefile (`make build` or `make rebuild`); do not invoke CMake or raw
  build scripts directly.
- Use Git Bash-compatible commands in project workflows. Do not add
  PowerShell or cmd.exe commands to project documentation or tooling.
- Do not modify Git configuration. Commit, push, or open a PR only when asked.
  Commits must use Conventional Commits and include a best-effort
  `Co-Authored-By` trailer.
- After changes, run the relevant checks when available and inspect
  `git status --short` and `git diff --check`.
- When debugging, prefix relevant logs with `[feature_name]`. Use
  `DebugLogger` for runtime logs rather than `print()`, and provide a focused
  command that runs the relevant flow and filters that prefix into a log file.

## Documentation policy

- Do not create documentation unless it is needed for recurring agent work.
- Documentation is written for agents, not as a human-oriented design record.
  Keep it short, factual, and free of duplicated implementation detail.
- Prefer code, configuration, and tests over explanatory documents. If a
  document conflicts with the repository, follow the repository and remove or
  correct the stale guidance when requested.
- Keep only the project structure and the few operational documents that are
  genuinely useful. Do not recreate detailed reference, archive, or planning
  documents.
- New or modified documentation must be in English.
