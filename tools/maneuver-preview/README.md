# Maneuver preview

Standalone preview of the production C++ scene compiler, native camera motion,
arrow progress/glow and contact shadow. There is no multilane-road renderer.
Independent lane data is drawn in a screen-space row at the bottom of the active
visible area. The small frame fits five cells; the wide frame fits all eight.
Arrow size stays constant, and overflow replaces one cell with `…`.

```sh
make -C tools/maneuver-preview all test
build/maneuver-preview/preview --case 14
python3 tools/maneuver-preview/capture.py
python3 tools/maneuver-preview/verify_all_transitions.py
python3 tools/maneuver-preview/verify_blink.py
build/maneuver-preview/preview --lanes 0
python3 tools/maneuver-preview/capture_lanes.py
python3 tools/maneuver-preview/test_lane_transport.py
```

Left/Right changes the maneuver using the native camera handoff. Up switches
small/wide visible areas; Down cycles independent lane events; D toggles the red
frame; Space runs fill/blink; P shows/hides lanes to inspect the camera retreat
and return. Lane events do not wait for maneuver transitions.
The source stays 328×181. Lanes always move the camera slightly backward; the
important arrow must fit the small red frame above the row in both stages.
Wide lane previews also draw the small safety frame for direct visual review.

The lane review covers eight-lane overflow on either edge, shared directions,
multiple best lanes, unknown angles, sharp/U-turn arrows, guidance alongside a
full roundabout and hidden guidance. `output/lane-panel/lane-panel.png` compares
both frame sizes. GPU checks verify premultiplied alpha and unchanged maneuver
pixels outside the bottom panel when switching frame sizes. The CPU suite checks window selection,
sentinels, ordering and all 729 three-direction combinations under ASan/UBSan.
The row is transparent: underlying scene colour and alpha fade away together.
`output/lane-panel/lane-camera.gif` shows the camera reveal/restore; GPU tests
also check a native maneuver handoff with lanes and destination-alpha erasure.

Thirty-two fixtures cover straight, slight, normal and sharp turns, both
U-turns, full roundabouts in both traffic directions, side roads, exits,
merges, lane-change maneuvers, arrival and clear. Capture output is in
`output/maneuver-preview/`; the comparison sheet and native transition GIFs
are the review artifacts. Raw captures and intermediate frames are ignored.

The capture suite checks framing, premultiplied alpha, static camera framing,
exact transition endpoints, unchanged scene caches during travel, and progress
retreat/empty commit/refill. The accepted arrival flag has its own
stage-dependent presentation. The exhaustive handoff check runs every fixture
once as the outgoing scene and once as the incoming scene; its ARRIVED endpoint
allows only the flag/tip presentation region to differ from the early static
sample. The GL blink check
verifies both palette endpoints, smooth tint changes, unchanged road pixels,
and glow fading during camera travel before the next arrow refills.
`--baseline PATH` optionally compares the first
fifteen fixtures with former lane-preview ordinary cases 18–32.
That historical parity check requires matching palettes: the approved white
base intentionally differs from captures made with the former grey-blue base.

See [scene engine](<../../docs/Cluster and RGI/HUD Renderer.md>) for the separate lane protocol.
