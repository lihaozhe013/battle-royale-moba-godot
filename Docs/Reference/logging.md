# Runtime Logging Reference

## Purpose

The project keeps runtime diagnostics in `debug.log` without sending normal project log messages through the Godot editor Output panel. This avoids the editor debugger and UI processing cost of high-volume stdout messages while retaining a local file for debugging.

## Ownership and flow

```text
Project scripts
    ↓ DebugLogger.log()
DebugLogger autoload
    ↓ buffered FileAccess writes
res://debug.log
```

`DebugLogger` is defined in `scripts/autoload/debug_logger.gd` and registered in `project.godot`. Messages are buffered and flushed every 250 ms, then flushed again when the autoload exits. If `res://debug.log` cannot be opened, the logger falls back to `user://debug.log`.

Runtime project diagnostics use `DebugLogger.log()` with explicit prefixes such as `[ERROR]` and `[WARNING]`. High-volume simulation and command traces retain their existing prefixes, including `[TICK]`, `[CMD]`, `[APPLY]`, and `[CAST]`.

## Project settings

The logging-related settings in `project.godot` are:

| Setting | Value | Purpose |
| --- | --- | --- |
| `application/run/disable_stdout` | `true` | Prevents project stdout messages from reaching the editor Output panel. |
| `application/run/flush_stdout_on_print` | `false` | Avoids per-print stdout flushing; project logs do not use stdout. |
| `debug/file_logging/enable_file_logging` | `false` | Prevents Godot's file logger from opening the same file as `DebugLogger`. |
| `debug/file_logging/log_path` | `res://debug.log` | Primary path used by `DebugLogger`. |

Godot engine warnings and errors that use stderr can still appear in the editor Output panel. This is intentional so resource and native-extension failures remain visible. Do not disable stderr globally unless hiding those failures is explicitly required.

## Performance notes

The previous `[TICK]` trace was emitted through stdout on every 30 Hz simulation tick. That path could update the remote debugger, append to the editor Output panel, and flush the log file for every message. The current path avoids editor-panel delivery and batches file flushes, but message formatting and file logging still have a cost. Disable or reduce high-volume trace calls when profiling gameplay performance.

## Verification

Run a short headless project session and inspect only the logging prefixes:

```bash
/Applications/Godot.app/Contents/MacOS/Godot --headless --path . --quit-after 90
rg '^\[(TICK|CMD|APPLY|CAST|ui_bootstrap|arcane_character)\]' debug.log > logging_debug.log
```

The command should leave project messages in `debug.log` without printing the normal project log stream to stdout. Engine stderr warnings may still be printed by the first command.
