# Maneuver scene engine

The 2026-09-13 decision removes the lane-road experiment while retaining the
C++ scene compiler, shared road style, native camera travel, arrow progress and
contact shadow. Independent lane guidance is drawn in a transparent panel below
the maneuver, with a full-width fade and animated camera framing.

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
The C++ `scene/lane_panel.cpp` paints a separate screen-space row against the
bottom edge of the current visible rectangle. The row has a transparent
background. Immediately before the lane glyphs, one screen-space mask pass
multiplies destination RGB and alpha by the same factor. The maneuver fades
to transparency under the row (12px feather above), preserving
premultiplied alpha and showing the real cluster background through the gap.
Even one visible lane masks the full 328px source width, without horizontal
feathering inside the frame. The erased region continues to the physical bottom of the 181px source texture,
including the final row below the 180px VC crop; glyphs stay crop-anchored.
No opaque plate, extra FBO or scene-mask rebuild is involved.

The 36-source-pixel cell pitch is fixed: the 210px frame fits five cells, the
328px frame fits all eight. Width determines capacity without resizing glyphs.
Overflow replaces the first or last cell with an ellipsis, retaining one end
of the ordered input list. The nearest best recommendation selects that end;
ties/default choose the right end, matching the native HUD fallback. The raw
renderer transport has no HUD expansion-description flags, so those are not
inferred. Known unique positions establish left-to-right order; incomplete or
duplicate positions preserve received order without inventing absent lanes.

Each cell combines up to three canonical vector directions. Only a valid
primary direction with status 2 is cyan; status 0/1 and extra branches are
neutral grey. ±1000 and other out-of-range angles never become straight/U-turn
arrows. Valid extra directions can still be shown in grey when the primary is
unknown; a cell with no valid direction uses a short neutral dash. The original
raw snapshot is not mutated.
An event with no valid directions at all hides the panel and restores the camera,
as the HUD path has no drawable lanes. A valid direction anywhere in the event
does not make an unknown primary a cyan recommendation or an overflow anchor.

HUD and renderer share `laneGuidanceVisible`: iOS `lane_guidance_showing` is
authoritative, with route teardown as the common clear condition. No approach,
distance, maneuver type, maneuver identity or transition-phase gate is added.
Lane-only deltas wake both outputs; route-state changes reevaluate the shared gate.
Show/hide keeps cached event data available for reappearance during the same
maneuver. Prefetch updates never select a future event. A new lg slot event ID
clears the previous slot content; explicit count zero never revives old arrays.
Missing modern lg events cannot borrow lanes from a legacy maneuver cache.
The Java writer preserves the latest atomic lane snapshot (including hide)
under progress queue pressure, replacing only older lane snapshots. CLEAR
barriers remain ordered. The raw renderer transport retains both ±1000 sentinels;
the HUD converter explicitly returns 0xFF for both before direction quantization.
A known fallback branch remains grey when the primary angle is unknown.
Missing angles for one lane never borrow the first lane's directions. Lifecycle
regressions cover the real Java delta parser and both output boundaries, plus
TCP show/hide/reappearance during native maneuver travel and queued hide under
progress traffic; results are in `output/lane-panel/lifecycle-verification.json`.

Meshes rebuild only when the visible semantic layout changes. A 250ms crossfade
handles event/capacity changes; the row follows the existing 250ms frame animation
and is clipped only within its own overlay pass. Hidden/malformed events and
CLEAR/disconnect remove stale guidance immediately. Scene masks remain intact
for native camera travel and the outgoing/retracting arrow fill.

While lanes are shown the camera always dollies back: normally 12% farther from
its look-at point, bounded at 18% if framing requires it. The planner reprojects
retained importance vertices and the complete arrowhead/final shaft, then applies
a small lens shift so the important route fits **the small frame in both stages**,
above the feathered row. FOV and source dimensions stay fixed. Framing targets
are cached by scene identity/build generation and do not rebuild geometry.
The current and incoming settled scenes participate during a handoff, while
native camera travel continues underneath the additional presentation transform.
Dolly and shift use the same 250ms easing; hiding guidance smoothly restores the
ordinary pose, and CLEAR/disconnect resets immediately while the scene is blank.
The fill origin follows the row's visible entry boundary with the same animation,
so progress does not start inside the newly hidden part of the arrow shaft.

The former SCENE_BEGIN/LANE/COMMIT (0x09–0x0b), road eligibility checks,
recommendation pavement and roundabout lane geometry are removed. Maneuver
packets and the existing BAP-to-renderer semantic
adapter remain compatible: removing that adapter would also remove the agreed
HUD icon correspondence, slight/off-ramp shape and angle/side handling.

## Verification

```sh
make -C tools/maneuver-preview all test
bash scripts/test_maneuver_native.sh
bash scripts/test_route_info.sh
python3 tools/maneuver-preview/capture.py
python3 tools/maneuver-preview/capture_lanes.py
python3 tools/maneuver-preview/test_lane_transport.py
make -C maneuver_render all
bash scripts/build_renderers.sh
```

The preview covers 32 ordinary scenes in both stages. CPU/ASan/UBSan checks
cover cache reuse, independent scene ownership, native route parity, invalid
inputs, curbs, progress handoff and the independent lane decoder. Java tests
verify HUD event selection, raw angle/unknown preservation and no maneuver
coupling. The GPU suite checks 64 stills and ten native camera/progress handoffs,
including exact settled endpoints and no scene recompilation during travel.

The historical removal review additionally compared all thirty existing ordinary
small/wide captures byte-for-byte. A live host-renderer probe sends fragmented
Java lane packets and verified that partial, committed and hidden guidance
left maneuver pixels unchanged before the separate panel was implemented.
The current panel review verifies actual destination-alpha erasure, 32 maneuvers
fitting the small frame above the lane mask, a 60-frame camera reveal/restore,
and native maneuver travel with lanes. Stage changes affect only the bottom
strip; hidden guidance matches the ordinary renderer after camera settle.
Atomic lane commits do not rebuild the maneuver. No vehicle deployment is implied by these
host checks. QNX builds use the C ABI without a C++ runtime, exceptions or RTTI.

## Java transport cleanup

`RendererMapper.map` is the single maneuver adapter. It uses the resolved HUD
semantics plus original CarPlay roads directly, without a BAP side-road round
trip. Dead compatibility helpers and bargraph sender wrappers were removed;
7,890 before/after HUD and renderer packets are byte-identical. See the
[Java transport audit](reference/RENDERER_JAVA_TRANSPORT_AUDIT.md).
