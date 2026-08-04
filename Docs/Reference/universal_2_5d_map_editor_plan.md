# Universal 2.5D Map Editor Plan

**Status:** Planned

**Target repository:** A new standalone repository. Do not implement the Electron application inside the Battle Royale MOBA repository.

## 1. Outcome

Build a desktop map authoring tool for fixed-size 3D game assets. The editor must be reusable across games, while individual games supply an asset catalog and an export target.

The canonical map stores placed objects, not stretched wall rectangles. Every placed object has a stable ID, an `asset_id`, and a transform. A target exporter validates the document and produces whatever runtime artifact its game needs.

```text
Asset catalog + source map
            |
            v
Electron + Three.js editor
            |
            +-- canonical map JSON
            +-- target validation report
            +-- game-specific exported artifacts
```

The editor is a 2.5D tool: objects occupy a 3D scene, but most placement occurs on the XZ ground plane with a top-down/isometric camera, grid snapping, and constrained yaw rotation.

## 2. Product Boundaries

### In scope

- Import and preview `.glb`/`.gltf` assets.
- Fixed-size asset placement, selection, movement, rotation, duplication, deletion, undo/redo, save, and autosave recovery.
- Project-local asset catalogs and map documents.
- Orthographic top-down and perspective/isometric viewport presets.
- Layers, tags, validation, and pluggable export targets.
- A Battle Royale MOBA target package as the first integration.

### Out of scope for the first release

- Editing meshes, materials, UVs, terrain sculpting, animation, or skeletal rigs.
- Runtime game simulation, navmesh generation, collaborative editing, cloud sync, and arbitrary script execution.
- Importing unsupported source formats. Assets must be preconverted to glTF/GLB before they enter the catalog.

## 3. Technology Decisions

Use Electron, TypeScript, and Three.js.

- Use a normal web renderer for the UI and a Three.js canvas owned by a viewport controller. React is suitable for panels, menus, and inspectors, but React must not recreate the Three.js scene on state changes.
- Use `GLTFLoader` for assets, `Raycaster` for picking, `TransformControls` for object manipulation, and `InstancedMesh` for large sets of identical static assets. Three.js documents these as the appropriate primitives for glTF loading, picking, transform gizmos, and repeated geometry. [Three.js glTF workflow](https://threejs.org/manual/en/loading-3d-models.html), [TransformControls](https://threejs.org/docs/pages/TransformControls.html), [Raycaster](https://threejs.org/docs/pages/Raycaster.html), [InstancedMesh](https://threejs.org/docs/pages/InstancedMesh.html)
- Enable Electron `contextIsolation` and renderer sandboxing. The renderer must never receive Node.js or unrestricted IPC access; the preload script exposes one typed, allow-listed API per operation. [Electron context isolation](https://www.electronjs.org/docs/latest/tutorial/context-isolation), [Electron IPC](https://www.electronjs.org/docs/latest/tutorial/ipc)
- Serve project assets through a validated custom `mapstudio://` protocol or controlled IPC byte reads. Do not let the renderer construct arbitrary `file://` URLs. Electron's protocol API is designed for this main-process ownership model. [Electron protocol](https://www.electronjs.org/docs/latest/api/protocol)
- Treat glTF/GLB as the only runtime asset format. Godot 4.7 also recommends and directly imports it, keeping the first game target compatible. [Godot 4.7 3D formats](https://docs.godotengine.org/en/4.7/tutorials/assets_pipeline/importing_3d_scenes/available_formats.html)

## 4. Standalone Repository Layout

```text
map-studio/
├── apps/
│   └── desktop/                 Electron main, preload, renderer entry points
├── packages/
│   ├── schema/                  Canonical TypeScript types, JSON schemas, migrations
│   ├── core/                    Commands, document store, validation, serialization
│   ├── viewport-three/          Three.js scene, camera, picking, gizmos, asset cache
│   ├── ui/                      Inspector, hierarchy, asset browser, dialogs
│   ├── target-sdk/              Exporter and validator plugin contracts
│   └── target-brmoba/           First game-specific catalog and exporter
├── examples/
│   └── brmoba-arena/            Fixture project and test map
├── tests/
│   ├── schema/
│   ├── core/
│   ├── viewport/
│   └── targets/
└── docs/
```

`schema` and `core` must not import Electron, React, or Three.js. This makes validation and export runnable in unit tests and later from a command-line build tool.

## 5. Canonical Data Contracts

### 5.1 Editor project file

Each editable project has a small manifest, for example `map-studio.project.json`:

```json
{
  "schema_version": "map-studio-project/v1",
  "name": "Example Arena",
  "asset_catalog": "assets/catalog.json",
  "maps": ["maps/default.map.json"],
  "targets": [
    {
      "id": "brmoba",
      "plugin": "@map-studio/target-brmoba",
      "output_dir": "exports/brmoba"
    }
  ]
}
```

Paths are relative to the project file. The editor must resolve and normalize every path in the main process, reject traversal outside the chosen project root, and pass only opaque asset URLs to the renderer.

### 5.2 Asset catalog

The catalog gives an `asset_id` meaning, placement constraints, preview location, and optional target-specific metadata. The source map stores only `asset_id`, never a mutable filesystem path.

```json
{
  "schema_version": "map-studio-assets/v1",
  "revision": "2026-08-04",
  "assets": [
    {
      "asset_id": "environment.wall.straight_2m",
      "label": "Stone Wall Straight 2m",
      "kind": "model",
      "source": "models/wall_straight_2m.glb",
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
      }
    }
  ]
}
```

Rules:

- `asset_id` is immutable after publication. Renames change only `label`.
- A fixed-size asset has `allow_scale: false`; its placed scale must stay `(1, 1, 1)`.
- A catalog can define custom component metadata, but keys must be namespaced by target, such as `brmoba.collider_2d`.
- The core editor preserves unknown components without interpreting them.

### 5.3 Canonical source map

```json
{
  "schema_version": "map-studio-map/v1",
  "id": "default",
  "name": "Default Arena",
  "units": { "linear": "meter", "up_axis": "y" },
  "world": {
    "bounds": { "min": { "x": -50, "z": -50 }, "max": { "x": 50, "z": 50 } },
    "grid": { "size": 0.5, "snap_enabled": true },
    "ground": { "asset_id": "environment.ground.grass_dirt" }
  },
  "layers": [
    { "id": "terrain", "name": "Terrain", "visible": true, "locked": false },
    { "id": "blockers", "name": "Blockers", "visible": true, "locked": false },
    { "id": "props", "name": "Props", "visible": true, "locked": false }
  ],
  "objects": [
    {
      "id": "wall-north-001",
      "asset_id": "environment.wall.straight_2m",
      "layer_id": "blockers",
      "position": { "x": -14, "y": 0, "z": 20 },
      "rotation": { "x": 0, "y": 90, "z": 0 },
      "scale": { "x": 1, "y": 1, "z": 1 },
      "tags": ["wall", "solid"],
      "components": {
        "brmoba.collider_2d": { "enabled": true, "mode": "asset_default" }
      }
    }
  ]
}
```

This contract intentionally includes the requested `position` and `asset_id` fields. `rotation` and `scale` are always serialized for deterministic diffs. The editor may hide scale controls for constrained assets.

## 6. Editor Experience

The first working version must provide these areas:

| Area | Responsibilities |
| --- | --- |
| Top bar | Open project/map, save, export, undo/redo, validation state, camera preset. |
| Asset browser | Search, filter by tag/kind, GLB thumbnail, drag asset to viewport, show native size and placement rules. |
| Viewport | XZ grid, ground preview, 2.5D camera, selection outline, gizmos, object placement, collision overlay. |
| Hierarchy | Layers and objects, visibility/lock controls, multi-selection. |
| Inspector | Stable ID, asset, transform, tags, target components, validation errors. |
| Validation/export panel | Errors/warnings by object, target selection, deterministic export report. |

Interaction contract:

1. Drag an asset from the browser to the ground plane; the editor snaps it using the catalog's grid and places it at its authored ground pivot.
2. Clicking selects through raycasting. Shift adds to selection. Box selection operates in the XZ plane.
3. Move, rotate, duplicate, delete, and inspector edits each execute one command. A command is the sole unit of undo/redo and dirty-state tracking.
4. Blocking assets can rotate only by their catalog's allowed yaw steps. Decorative assets may have broader transform freedom.
5. The collision overlay displays the target-derived footprint, not an inferred mesh bounding box.

Use an orthographic top-down camera as the default editing mode for exact placement. Provide an isometric/perspective preset for visual review; match a target game's camera angle through a project preset. An orthographic camera preserves on-screen object size with distance, making grid editing predictable. [Three.js OrthographicCamera](https://threejs.org/docs/pages/OrthographicCamera.html)

## 7. Rendering and Performance Design

- Load each GLB once in `AssetCache`; clone its scene graph for normal editable objects.
- Maintain a lightweight selection proxy per object with `object_id` metadata. Do not raycast the full scene hierarchy when a simple proxy or bounding box is sufficient.
- Use `InstancedMesh` only for non-selected, repeated, static props. When an instance becomes selected or edited, temporarily render it as an individual proxy or update its instance matrix.
- Dispose geometries, materials, textures, render targets, and controls when a map/project closes.
- Use a neutral ground, directional light, ambient fill, and shadow toggle for preview. Asset preview lighting is not exported gameplay lighting.
- Render target collision footprints, map bounds, grid, pivots, and selection outlines in separate non-exported scene layers.

## 8. Main/Preload/Renderer Contract

The main process owns filesystem access, native dialogs, project-root permissions, custom protocol registration, and packaging. The renderer owns only UI state and Three.js rendering.

Expose a narrow preload API such as:

```ts
window.mapStudio = {
  project: { open(): Promise<ProjectSnapshot>; save(doc: SaveRequest): Promise<SaveResult> },
  assets: { read(assetToken: string): Promise<ArrayBuffer> },
  files: { export(request: ExportRequest): Promise<ExportResult> },
  shell: { reveal(pathToken: string): Promise<void> }
};
```

Every IPC handler validates its payload with the shared schema package, checks project-root scope, and returns plain serializable data. Never expose raw `ipcRenderer`, `fs`, path strings, or arbitrary shell execution.

## 9. Validation and Export Target SDK

Define a plugin contract:

```ts
interface MapTarget {
  id: string;
  validate(map: MapDocument, catalog: AssetCatalog): ValidationIssue[];
  export(input: ExportInput): ExportResult;
}
```

Core validation covers schema shape, duplicate IDs, missing asset IDs, invalid numbers, world-bound violations, and forbidden transforms. A target adds game rules, such as collision rotation restrictions, spawn clearances, or required layers.

Exports must be deterministic:

- sort objects by stable `id` before output;
- format JSON consistently;
- never include machine-specific absolute paths or timestamps in runtime files;
- write all files to a staging directory and atomically replace outputs only after validation succeeds;
- emit a manifest with source-map hash, catalog revision, target version, generated files, and validation result.

## 10. Delivery Phases

### Phase 0 — Foundation

- Create the standalone TypeScript workspace, Electron shell, strict lint/typecheck/test commands, and secure preload bridge.
- Implement the project, catalog, and source-map schemas plus JSON migrations.
- Add a fixture catalog with cube placeholders and fixture maps.

### Phase 1 — Viewport MVP

- Build the Three.js XZ viewport, orthographic camera, grid, bounds, asset cache, GLB loading, selection, and isometric review preset.
- Implement asset browser drag placement and the hierarchy/inspector shell.

### Phase 2 — Authoring Workflow

- Add command-based undo/redo, multi-selection, duplication, deletion, layer controls, transform gizmos, numeric inspector editing, grid/yaw snapping, save, autosave, and recovery.
- Enforce catalog placement constraints in both UI and validation.

### Phase 3 — Target System

- Implement `MapTarget`, validation panel, export report, deterministic staging writes, and the first Battle Royale MOBA target.
- Add a CLI entry point that validates and exports a project without Electron.

### Phase 4 — Hardening

- Add regression fixtures, schema migration tests, exporter golden-file tests, asset-loading failure tests, and large-map profiling.
- Package macOS, Windows, and Linux builds only after file security and export behavior are verified.

## 11. Acceptance Criteria

The first release is complete when an implementer can:

1. Open a project containing GLB assets and a map document.
2. Place fixed-size assets on a snapped XZ grid without arbitrary scaling.
3. Save a stable, human-reviewable source map containing `position` and `asset_id`.
4. Undo and redo every map mutation exactly.
5. View target-specific collision footprints and validation errors before export.
6. Generate deterministic target artifacts from the same source map.
7. Run source-map validation and export from tests or CLI without launching Electron.
8. Load assets only from the approved project root with no privileged renderer filesystem access.

## 12. Implementation Notes for the Next Agent

- Start with the canonical schema and command model; do not start by building menus.
- Keep target-specific keys inside namespaced `components` so the editor remains universal.
- Make placeholders first-class catalog assets. This permits full workflow development before final GLBs are chosen.
- Do not infer gameplay collision from arbitrary GLB mesh bounds. Collision is authored in catalog metadata and exported explicitly.
- Do not make the source map a generated file. The source map is the reviewed, version-controlled authoring artifact; runtime exports are derived artifacts.
