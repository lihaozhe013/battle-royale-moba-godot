# Skill VFX Architecture

## Purpose

Skill VFX is a view-layer presentation system. It does not change simulation timing, damage, targeting, or snapshot data. The simulation remains authoritative and communicates hit results through `SimSnapshot`.

## Ownership

```text
sim_bridge.gd
└── SkillVFX
    ├── snapshot transition tracking
    ├── skill ID to VFX scene dispatch
    └── target EntityView lookup
        └── SkillVfxAttachment
            └── per-skill VFX instance
```

`SkillVFX` is responsible for shared presentation concerns only:

- consuming the local hero snapshot;
- detecting a completed cast with a valid `hit_target_id`;
- resolving the authoritative skill ID from the previous cast slot;
- resolving the target view through `EntityManager`;
- instantiating the registered skill VFX scene.

`SkillVfxAttachment` is a generic world-space anchor attached to each entity. It must not contain skill-specific meshes, materials, or gameplay rules.

## Per-skill layout

Each skill VFX has an independent wrapper scene and script:

```text
resources/vfx/skills/<skill>/<skill>_vfx.tscn
scripts/view/skill_vfx/<skill>_vfx.gd
```

The wrapper scene owns asset composition and tunable transforms. The wrapper script owns playback lifecycle, including one-shot configuration and cleanup. Vendor assets remain under `resources/vfx/` and are referenced by the wrapper rather than embedded in the shared dispatcher.

For Melee Strike:

```text
Skill ID 1
└── resources/vfx/skills/melee_strike/melee_strike_vfx.tscn
    └── BinbunVFX_Vol2 VFXZapLightning_01
```

## Event contract

The current snapshot contract exposes `cast_state`, `cast_slot`, `hit_target_id`, and each slot's `skill_id`. A VFX hit is emitted when the local hero transitions from an active cast state to `None` and the snapshot contains a valid hit target. The VFX layer does not infer hits from HP changes.

The dispatcher gates event processing by snapshot sequence, so a render frame cannot instantiate the same effect repeatedly. The skill wrapper is attached to the target view, allowing the target's existing interpolation to carry the effect with the target during playback.

## Extension rule

To add a skill VFX:

1. Create the skill's wrapper script and scene.
2. Register the scene by authoritative `skill_id` in `scripts/view/skill_vfx.gd`.
3. Keep asset-specific configuration inside that skill's wrapper scene/script.
4. Do not add skill-specific branches to `sim_bridge.gd`, `EntityView`, or the C++ simulation.

If a skill needs a new simulation event or timing guarantee, extend the snapshot contract explicitly instead of reading simulation state from the view.
