# Map Asset Schema and Game Integration Plan

**Status:** Planned

**Scope:** Battle Royale MOBA map content integration. This document does not implement the standalone Electron editor; that design lives in `universal_2_5d_map_editor_plan.md`.

## 1. Goal

Replace the current authoring model of arbitrary-width `walls[]` rectangles with fixed-size asset placements while preserving the simulation's existing AABB/nav-grid/collision contract during the first migration.

The game will consume two artifacts derived from the same authored source map:

```text
default.map.json (authored source map)
        |                         \
        |                          \
        v                           v
Godot MapVisuals               default.sim.json (generated)
asset_id + transform            legacy bounds + walls[]
                                      |
                                      v
                              C++ SimServer.initialize()
```

This is deliberate:

- The Godot view needs asset IDs and transforms.
- The C++ simulation currently needs only map bounds and 2D AABBs.
- Keeping the generated simulator artifact in the legacy shape avoids a C++ map-parser rewrite in the first phase.
- The standalone editor owns conversion from fixed-size asset colliders to simulator walls.

`default.sim.json` is generated, checked in for reproducible builds, and never hand-edited. `default.map.json` is canonical and hand-authored only through the new editor.

## 2. Current Constraints

- `World::initialize()` parses map bounds and `walls[]`, turns each rectangle into `WallBounds`, builds a 0.5-unit nav grid, and uses those bounds for collision. [world.cpp](/Users/lihaozhe/dev/battle-royale-moba-godot/src_cpp/sim/world.cpp:25)
- Nav-grid walls are already inflated by agent radius plus 0.25 units. The exporter must not pre-inflate them. [nav_grid.cpp](/Users/lihaozhe/dev/battle-royale-moba-godot/src_cpp/sim/nav_grid.cpp:18)
- `sim_bridge.gd` currently draws the same `walls[]` data as uniform gray `BoxMesh` nodes. [sim_bridge.gd](/Users/lihaozhe/dev/battle-royale-moba-godot/scripts/sim_bridge.gd:106)
- `WorldBootstrap` currently creates a generic 100×100 ground plane. Map-specific ground ownership must move to map visuals, while light and environment remain bootstrap concerns. [world_bootstrap.gd](/Users/lihaozhe/dev/battle-royale-moba-godot/scripts/view/world_bootstrap.gd:15)
- Input aims at the mathematical `y = 0` ground plane, not at Godot physics colliders. Imported map models must not become an input dependency. [input_event_queue.gd](/Users/lihaozhe/dev/battle-royale-moba-godot/scripts/input/input_event_queue.gd:30)

## 3. File Layout

```text
data/maps/
├── default.map.json              Canonical authored source map
├── default.sim.json              Generated C++ input; legacy shape only
├── default.sim.manifest.json     Generated source hash and object-to-wall mapping
└── brmoba_asset_catalog.json     Versioned game asset registry for the editor/exporter

resources/maps/
├── ground/
│   └── grass_dirt_01.glb or material textures
├── walls/
│   ├── wall_straight_2m.glb
│   ├── wall_straight_4m.glb
│   ├── wall_corner_2m.glb
│   └── wall_endcap_2m.glb
└── props/
    ├── rock_01.glb
    ├── bush_01.glb
    └── tree_01.glb
```

Keep source GLBs and all textures referenced by non-embedded glTF files together under `resources/maps/`. Prefer `.glb` for a single portable asset file; Godot 4.7 directly supports and recommends glTF/GLB workflows. [Godot 4.7 3D formats](https://docs.godotengine.org/en/4.7/tutorials/assets_pipeline/importing_3d_scenes/available_formats.html)

## 4. Canonical Game Source Map

`default.map.json` follows the universal editor document with the `brmoba` component namespace.

```json
{
  "schema_version": "map-studio-map/v1",
  "id": "default",
  "name": "Default Arena",
  "units": { "linear": "meter", "up_axis": "y" },
  "world": {
    "bounds": { "min": { "x": -50, "z": -50 }, "max": { "x": 50, "z": 50 } },
    "grid": { "size": 0.5, "snap_enabled": true },
    "ground": { "asset_id": "brmoba.ground.grass_dirt_01" }
  },
  "layers": [
    { "id": "blockers", "name": "Blockers", "visible": true, "locked": false },
    { "id": "props", "name": "Props", "visible": true, "locked": false }
  ],
  "objects": [
    {
      "id": "wall-west-001",
      "asset_id": "brmoba.wall.straight_2m",
      "layer_id": "blockers",
      "position": { "x": -41.5, "y": 0, "z": -29 },
      "rotation": { "x": 0, "y": 90, "z": 0 },
      "scale": { "x": 1, "y": 1, "z": 1 },
      "tags": ["wall", "solid"],
      "components": {
        "brmoba.collider_2d": { "enabled": true, "mode": "asset_default" }
      }
    },
    {
      "id": "rock-east-001",
      "asset_id": "brmoba.prop.rock_01",
      "layer_id": "props",
      "position": { "x": 18, "y": 0, "z": 12 },
      "rotation": { "x": 0, "y": 35, "z": 0 },
      "scale": { "x": 1, "y": 1, "z": 1 },
      "tags": ["decorative"],
      "components": {}
    }
  ]
}
```

The required placement fields are `id`, `asset_id`, `position`, `rotation`, and `scale`. Fixed-size blockers use scale `(1, 1, 1)` and a 0/90/180/270-degree yaw. Decorative objects may have arbitrary yaw, but must remain visually readable from the game camera.

## 5. Battle Royale MOBA Asset Catalog

`brmoba_asset_catalog.json` is a target-specific catalog consumed by the editor's `target-brmoba` plugin and mirrored by Godot's visual registry.

```json
{
  "schema_version": "map-studio-assets/v1",
  "revision": "1",
  "assets": [
    {
      "asset_id": "brmoba.wall.straight_2m",
      "label": "Stone Wall Straight 2m",
      "kind": "model",
      "source": "../../resources/maps/walls/wall_straight_2m.glb",
      "native_size": { "x": 2, "y": 1.4, "z": 1 },
      "placement": {
        "surface": "ground",
        "grid": 0.5,
        "allowed_yaw_degrees": [0, 90, 180, 270],
        "allow_scale": false
      },
      "components": {
        "brmoba.collider_2d": {
          "shape": "box",
          "center": { "x": 0, "z": 0 },
          "size": { "x": 2, "z": 1 }
        }
      },
      "godot": {
        "scene_path": "res://resources/maps/walls/wall_straight_2m.glb"
      }
    }
  ]
}
```

Catalog rules:

- `asset_id` is stable and never includes a local absolute path.
- Blocking asset size and collision footprint are authored once in the catalog, never inferred from the visible GLB.
- The visual model's pivot must be at its ground-contact center. If an asset is authored differently, fix it in Blender or wrap it in a Godot scene; do not hide pivot correction in map placement.
- Use discrete module lengths such as 1 m, 2 m, 4 m, corner, end cap, and solid block. Long barriers are built by repeated modules, not non-uniform scaling.
- Props without `brmoba.collider_2d` are visual-only. They must be placed outside the playable lanes or given an explicit solid asset/collider if intended to block movement.

## 6. Generated Simulator Artifact

The exporter writes `default.sim.json` in the exact current shape:

```json
{
  "name": "default",
  "bounds": { "half": 50 },
  "walls": [
    { "minX": -42.5, "minY": -30, "maxX": -40.5, "maxY": -29 }
  ]
}
```

No additional fields are allowed in this file until the C++ parser has been deliberately replaced. The existing hand-written parser does not safely skip arbitrary unknown values. The exporter therefore writes a separate `default.sim.manifest.json` containing:

- source map hash;
- asset catalog revision;
- exporter version;
- object ID to generated wall-index mapping;
- validation report;
- generated file hash.

### Collision export algorithm

For every object where `brmoba.collider_2d.enabled` is true:

1. Resolve `asset_id` in the catalog.
2. Read its local `center` and `size` box.
3. Reject non-unit scale, non-ground placement, or yaw not divisible by 90 degrees.
4. Rotate the local box by the object's yaw. A 90/270-degree rotation swaps X/Z size.
5. Translate by `position.x` and `position.z`.
6. Emit an uninflated `{minX, minY, maxX, maxY}` wall using `z` as simulation Y.
7. Sort output by source object ID for deterministic output.

Do not merge adjacent walls in the first version. Keeping one source object per generated wall makes the manifest, debugging, and visual alignment straightforward. Merge only after a profiler shows that wall count is a real cost and a merge preserves per-object diagnostics.

## 7. Godot View Integration

Introduce a view-only `MapVisuals` node owned by `sim_bridge.gd` or explicitly added as a `Main` child. It replaces `_spawn_wall_visuals()`.

Responsibilities:

1. Read `default.map.json` before simulation initialization.
2. Read `brmoba_asset_catalog.json` and build an `asset_id -> PackedScene` registry using the catalog's `godot.scene_path`.
3. Create the map-specific ground from `world.ground.asset_id`.
4. Instantiate each map object once, apply its X/Y/Z position, degree-based rotation, and scale, then attach it below a persistent `MapVisuals` root.
5. Report missing catalog entries or missing Godot resources with a `[map_visuals]` prefix.
6. Never create `StaticBody3D`/`CollisionShape3D` nodes for simulation authority. The C++ AABBs remain authoritative.

`WorldBootstrap` retains the directional light and `WorldEnvironment`; it no longer owns the generic ground after this migration. The existing screen-space health bars remain valid because they intentionally remain visible through map geometry.

The startup order becomes:

1. `WorldBootstrap` creates lighting and environment.
2. `sim_bridge.gd` reads `default.map.json`, `default.sim.json`, stats, and catalog.
3. `MapVisuals` creates the ground and object visuals from the source map.
4. `SimServer.initialize(default.sim.json, stats_yaml)` initializes C++ with generated walls.
5. Input, snapshots, entities, UI, and VFX run unchanged.

## 8. Migration Plan

### Phase 0 — Freeze the legacy editor

- Keep `tools/map_editor/` operational for the old `default.json` only.
- Do not extend the pygame editor with asset-placement features.
- Mark it as legacy only after the Electron tool has exported a validated replacement map.

### Phase 1 — Establish source assets and catalog

- Select one coherent CC0 low-poly kit and add only a small initial set: one ground, two straight wall sizes, one corner, one end cap, one solid block, and three decorative props.
- Normalize every GLB pivot, unit scale, texture packaging, and forward orientation before cataloging it.
- Add `brmoba_asset_catalog.json` and a source-map fixture using placeholder assets first.

### Phase 2 — Build and validate the exporter

- Implement the Battle Royale MOBA editor target outside this repository.
- Export `default.sim.json` and its manifest from a fixture source map.
- Compare generated walls with the legacy `default.json` until navigation and collision behavior are intentionally changed.
- Commit source map, generated sim map, manifest, catalog, and assets together.

### Phase 3 — Add Godot map visuals

- Add `scripts/view/map_visuals.gd` and a small asset-registry helper.
- Replace `_spawn_wall_visuals()` with source-map visual instantiation.
- Move ground construction from `WorldBootstrap` to `MapVisuals`.
- Keep a `debug_draw_sim_walls` toggle that renders the generated AABB footprint over the GLB placement.

### Phase 4 — Switch runtime inputs

- Change `sim_bridge.gd` to pass `default.sim.json` to `SimServer.initialize()` and `default.map.json` to `MapVisuals`.
- Remove the legacy gray `BoxMesh` wall renderer only after the debug overlay shows exact alignment.
- Update `CONTEXT.md`, `game_main_process_execution_order.md`, and `sim_api_reference.md` after the code change, not before.

### Phase 5 — Retire the legacy path

- Rename the old `data/maps/default.json` only after all callers use the new file names.
- Remove `make edit-map` and the Python editor only after the standalone editor, exporter, and game migration have been accepted.
- Preserve a legacy-map conversion command in the standalone tool for historical maps.

## 9. Validation Rules

The `brmoba` target must fail export for:

- duplicate object IDs;
- missing `asset_id` or unknown catalog asset;
- an object outside world bounds;
- a blocking asset with missing collider metadata;
- a blocking asset with non-unit or non-uniform scale;
- a blocking asset with a yaw other than 0/90/180/270 degrees;
- a blocking asset with nonzero Y position unless the catalog explicitly supports it;
- collider footprint outside map bounds;
- an asset catalog revision mismatch when the source map pins a revision.

It should warn, but allow export, for:

- decorative props that overlap a simulation wall;
- visual overlap between props;
- very dense prop clusters;
- a source map whose generated simulation artifact is stale.

## 10. Verification Checklist

Before switching the default runtime map:

- The source map and catalog validate in the standalone tool and CLI.
- Exporter golden tests produce byte-stable `default.sim.json` for the fixture map.
- Every blocking GLB footprint matches the debug AABB overlay.
- Right-click movement and A* paths respect all generated walls.
- Hero collision resolves outside every visual blocker without visible gaps.
- Decorative GLBs do not affect mouse-to-ground targeting.
- Godot imports every GLB and texture without missing-resource errors.
- `git diff` contains only intended map source, generated artifacts, GDScript, assets, and documentation changes.
- C++ verification uses the project command `make build`; do not invoke CMake or a compiler directly.

## 11. Deferred C++ Schema Migration

Do not make C++ parse source-map objects in the first implementation. It adds no gameplay capability and would require replacing the current limited JSON parser to safely navigate nested/unknown values.

Only consider direct C++ source-map parsing when all of these are true:

1. Generated runtime artifacts are creating a proven workflow problem.
2. The simulation needs semantic components beyond 2D blocker AABBs.
3. A robust, tested JSON parser is introduced with versioned schema validation.
4. C++ still receives only simulation data and remains independent of Godot and GLB files.

Until then, the source-map/editor/exporter boundary is the simpler and safer architecture.
