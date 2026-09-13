# Maneuver preview

Standalone preview of the production C++ scene compiler, native camera motion,
arrow progress/glow and contact shadow. There is no multilane-road renderer.
Independent lane data is accepted by production for a future panel below the
maneuver; the panel is intentionally not implemented yet.

```sh
make -C tools/maneuver-preview all test
build/maneuver-preview/preview --case 14
python3 tools/maneuver-preview/capture.py
```

Left/Right changes the maneuver using the native camera handoff. Up switches
small/wide visible areas; D toggles the red frame; Space runs fill/blink.
The preview retains the original 328×181 source and camera scale.

Thirty-two fixtures cover straight, slight, normal and sharp turns, both
U-turns, full roundabouts in both traffic directions, side roads, exits,
merges, lane-change maneuvers, arrival and clear. Capture output is in
`output/maneuver-preview/`; the comparison sheet and native transition GIFs
are the review artifacts. Raw captures and intermediate frames are ignored.

The capture suite checks framing, premultiplied alpha, static camera framing,
exact transition endpoints, unchanged scene caches during travel, and progress
retreat/empty commit/refill. The accepted arrival flag has its own
stage-dependent presentation. `--baseline PATH` optionally compares the first
fifteen fixtures with former lane-preview ordinary cases 18–32.
That historical parity check requires matching palettes: the approved white
base intentionally differs from captures made with the former grey-blue base.

See [scene engine](../../docs/SCENE_ENGINE.md) for the separate lane protocol.
