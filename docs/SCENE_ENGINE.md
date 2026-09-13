# Maneuver scene engine

The 2026-09-13 decision removes the lane-road experiment while retaining the
C++ scene compiler, shared road style, native camera travel, arrow progress and
contact shadow. Lane guidance remains an independent input for a future panel
below the maneuver. That panel is not drawn yet.

## Ownership and rendering

`scene/scene.h` exposes a C ABI over the cached C++11 compiler. A scene accepts
only a maneuver, its generation and the settled projection. Current and next
scenes own separate geometry and swap when the native motion engine commits.
Pending maneuvers remain owned copies. No scene rebuild or allocation occurs
while the camera travels along the joined route.

Simple turns use retained pavement with thin white curbs and longitudinal
entry/exit fades. Complex families retain the built-in geometry: junction
branches, sharp turns, U-turn overlap lift, full roundabouts, exits, merges,
lane-change maneuvers and arrival flags. LANE_CHANGE remains a maneuver type;
it does not imply drawing a multilane road.

`visual_style.h` remains the shared source for neutral #646464 asphalt, white
curbs, road/arrow dimensions and route height. The 328×181 source, FOV and
existing visible areas are unchanged. The red small frame is a composition
guide, not a clip plane baked into the road. The existing arrival flag has its
own stage-dependent layout.

Arrow progress retracts during the outgoing maneuver, stays empty through the
handoff, then fills to the incoming target. Native camera following, glow and
self-overlap contact shadows are retained.

The approved OEM+ palette uses a bright white resting arrow and the established
HUD-blue fill/blink-on colour. In `render.c`, the resting shader input is
`vec3(1.000000, 0.980392, 0.909804)` (compensated for the cool lighting), and the
active input remains `vec3(0.32, 0.77, 1.0)`. There are no extra outlines or
decorative passes. White is brighter than blue, so blinking reads as a colour
change rather than a brightness increase. See the
[approved comparison](../output/oem-white-study/oem-white.png).

## Independent lane guidance

BAPBridge selects lane data using the same active-event resolver and visibility
gate as HUD FctID 24. `LaneGuidanceSnapshot` copies the raw ordered records for
transport, preserving direction angles, status, position and completeness.
It does not select a primary lane, match a turn angle, infer connectivity or
require a link to the displayed maneuver. Incomplete records and ±1000 remain
explicit; bounded transport does not promote missing fields to recommendations.

`RendererServer` sends an atomic batch of fixed 48-byte records:

- `LANES_BEGIN` (0x0c): token, event ID, count, showing and completeness.
- `LANES_LANE` (0x0d): token, record index, position, status, primary angle and up
  to sixteen signed-degree direction angles; up to eight records.
- `LANES_COMMIT` (0x0e): matching token.

See `maneuver_render/protocol.h` for byte offsets. No MANEUVER packet is part of
this batch. `lane_guidance.h` stages and commits the raw event independently;
malformed committed data clears stale guidance. CLEAR/disconnect resets both
staging and accepted guidance. A normal maneuver update does not clear it.
The current renderer receives and logs this input but does not paint it.

The former SCENE_BEGIN/LANE/COMMIT (0x09–0x0b), road eligibility checks, lane
window selection, lane glyphs, recommendation pavement and roundabout lane
geometry are removed. Maneuver packets and the existing BAP-to-renderer semantic
adapter remain compatible: removing that adapter would also remove the agreed
HUD icon correspondence, slight/off-ramp shape and angle/side handling.

## Verification

```sh
make -C tools/maneuver-preview all test
bash scripts/test_maneuver_native.sh
bash scripts/test_route_info.sh
python3 tools/maneuver-preview/capture.py
make -C maneuver_render all
bash scripts/build_renderers.sh
```

The preview covers 32 ordinary scenes in both stages. CPU/ASan/UBSan checks
cover cache reuse, independent scene ownership, native route parity, invalid
inputs, curbs, progress handoff and the independent lane decoder. Java tests
verify HUD event selection, raw angle/unknown preservation and no maneuver
coupling. The GPU suite checks 64 stills and ten native camera/progress handoffs,
including exact settled endpoints and no scene recompilation during travel.

The removal review additionally compares all thirty existing ordinary
small/wide captures byte-for-byte. A live host-renderer probe sends fragmented
Java lane packets and verifies that partial, committed and hidden guidance
leave maneuver pixels unchanged. No vehicle deployment is implied by these
host checks. QNX builds use the C ABI without a C++ runtime, exceptions or RTTI.

## Java transport cleanup

`RendererMapper.map` is the single maneuver adapter. It uses the resolved HUD
semantics plus original CarPlay roads directly, without a BAP side-road round
trip. Dead compatibility helpers and bargraph sender wrappers were removed;
7,890 before/after HUD and renderer packets are byte-identical. See the
[Java transport audit](reference/RENDERER_JAVA_TRANSPORT_AUDIT.md).
