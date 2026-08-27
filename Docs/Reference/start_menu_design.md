# Start Menu Design

**Status:** Implemented on 2026-08-27.

**Scope:** Full-screen application entry UI, shared settings access, and
navigation between the start menu and the existing gameplay scene. Simulation
rules, map loading, and the gameplay UI remain unchanged.

## 1. Scene Ownership

`res://scenes/start_menu.tscn` is the project entry scene. Its root is a
full-viewport `Control` driven by `scripts/ui/start_menu.gd`.

The start menu owns:

- the black background and optional future artwork texture slot;
- the responsive two-column navigation layout;
- `Start Match`, `Settings`, and `Quit Game` actions;
- the menu-context instance of `SettingsPanelUI`.

`res://scenes/main.tscn` remains the gameplay scene. It is loaded only after
`Start Match`, so `SimServer`, map wall visuals, gameplay input nodes, and
health-bar prewarming do not run while the application is idle at the menu.

## 2. Layout and Input

The default layout places the title and navigation column on the left and
reserves the right side for future character or background artwork. The
current artwork texture is unset, so the screen remains black. Below the
navigation buttons, the menu shows the current MOBA input hints:

```text
Right-click to move · Q / W / E / R to cast · A to attack
```

The artwork column is hidden below the desktop-width breakpoint and the
navigation column is centered. Margins are recalculated when the viewport
changes, allowing the menu to survive resizing and fullscreen mode changes.

The focused `Start Match` button is the default keyboard target. Enter,
keypad Enter, and Space activate it. Escape closes an open menu settings
panel and has no action when that panel is hidden.

## 3. Settings Contract

`SettingsPanelUI` is code-built and supports two contexts:

- `MAIN_MENU`: camera, edge-pan, edge speed, smooth-pan, fullscreen, and cast
  mode settings, with a Back action;
- `GAMEPLAY`: the same settings plus Quit Game and Main Menu actions.

Camera, edge-pan, edge speed, smooth-pan, fullscreen, and cast mode are stored
through the `GameSettings` autoload. Cast mode is a global Normal/Quick
preference. `CastSettings` copies it into all four skill slots when created or
bound to the gameplay UI, preserving the current all-slots behavior.

Changing settings never pauses the scene tree or the active simulation.

## 4. Scene Transitions

The menu starts the current default match with:

```text
StartMenu → Main / sim_bridge.gd
```

In gameplay, `Main Menu` first opens a confirmation dialog. Cancel keeps the
current match active. Confirming emits `SettingsPanelUI.main_menu_requested`;
`sim_bridge.gd` owns the actual transition:

```text
Main / sim_bridge.gd → StartMenu
```

The transition handler clears a stale `SceneTree.paused` value left by the
existing game-over path before loading the menu. It also restores the normal
cursor. A failed transition logs an error with the `[start_menu]` feature
prefix and leaves the current scene active where possible.

The current implementation discards the local match on confirmed navigation.
The signal boundary is intentional so a future online session owner can
perform disconnect/leave cleanup before loading the menu.

## 5. Verification

Verify the following manually and with the normal project validation commands:

1. Launching the project shows the start menu before any `SimServer` startup
   log.
2. The menu fills the viewport at 16:9, 4:3, resized, windowed, borderless,
   and exclusive fullscreen display modes.
3. Mouse and keyboard focus activate all menu actions correctly.
4. Menu settings persist after entering gameplay, returning to the menu, and
   restarting the application.
5. Cast mode is copied to all four runtime skill slots.
6. Opening gameplay settings does not pause the simulation.
7. Canceling the leave confirmation keeps gameplay active; confirming returns
   to a fresh start menu.
8. Starting another match creates a fresh gameplay scene and simulation.
