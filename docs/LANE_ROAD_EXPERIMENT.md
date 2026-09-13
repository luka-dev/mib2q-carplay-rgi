# Lane-road experiment — 2026-09-12

Status: C++11 scene compiler integrated into the host preview, cross-compiled for
QNX/ARM; live lane transport remains pending. Worktree: `mhi2-carplay-lane-road`;
branch: `experiment/lane-road`; committed visual baseline: `d15e6bf`.
Current architecture and verification: [SCENE_ENGINE.md](SCENE_ENGINE.md).
The follow-up sections below retain the visual experiment's history.

User constraints after the initial visual review: preserve the old source size,
keep every important cue inside the existing small hidden red crop rectangle,
do not zoom out heavily, and center on the primary recommended lane. Always
show the nearest true road edge and every lane between it and the primary lane.
Only the opposite side may fade. The larger viewport reveals additional lanes;
it must not move, resize, or repack the main maneuver. Showing every lane is unnecessary.
The preview now draws that exact red frame, with full-source and native-size sheets.

## Required integration: ordinary maneuvers remain first-class

User clarification, 2026-09-12: maneuvers without lane information are frequent
and must look coherent with lane scenes. Lane data is optional detail, not a
requirement for rendering a maneuver. Missing, hidden, stale or rejected lanes
must retain the independent maneuver and its known junction/side-road data.

The reusable compiler now has three outputs: a connected lane road, a simple
unmarked turn and a complete built-in family. The latter preserves side roads,
full circles, sharp/U-turn geometry and destination details. The per-state scene
provider can decline a state so the original painter renders it; built-in
elevation is retained while lane paths use the shallow overlay profile.

Cases 18–32 supply explicit empty-lane states. The capture suite also crosses
between lanes, ordinary junctions and full roundabouts using the native camera
engine. Lane-only preparation does not invoke animation or progress control;
production event handling must preserve that distinction when transport is added.

### Side-street / optional-arrow review follow-up

User's 2026-09-12 screenshots identified missing bold side-street shoulders and
an optional-arrow tip overlapping a dashed lane boundary. Fixed in the host
preview only; prior renderer/hook/audio work is untouched.

- Exit shoulders now use the main road's .022 thickness and white material.
  The shared road fill opens the junction, instead of leaving a solid white
  separator across the mouth. No artificial end-cap is added to an exit.
- Adjacent incoming lanes with the same valid primary/optional direction form
  one wider exit. Lane separators follow the turn; dashes are measured by path
  distance, including long straight segments. Unknown/different-direction lanes
  and discontinuous position indices break the group. This infers a schematic
  continuation only: the input does not establish the destination's lane count.
- Optional/recommended pavement arrows are fitted jointly per lane, preserving
  their common stem. Their full shaft/head bounds leave .022 lateral clearance;
  crowded cells compress the glyph path before shifting the whole group. Stroke
  width is bounded for packed lanes. Main route mesh width and lens are unchanged.
- ASan/UBSan layout tests pass all 78,720 combinations plus exit-group tests.
  Twenty actual GL captures pass framing, minimum glyph clearance, exit counts,
  equivalent-scene equality and small/wide camera/main-mesh/lane-width invariants.
  Visually inspected single left, double right, double left and triple left.
  Artifacts: `output/lane-road-side-streets/`; old `output/lane-road/` retained.

Live lane-event transport, QNX timing and vehicle integration remain pending.

### Projected lane alignment and disappearing curve follow-up

The next screenshots exposed two defects the previous geometry-only checks
missed. The world-space route centerline already preserved its lane index, but
the standalone route's `.03/.06` base/top plus `.035` progressive lift moved the
visible head toward another lane after perspective projection. In the lane
preview only, use base `.002`, top `.012`, zero additional lift: still a shallow
3D prism, unchanged `.14` width/FOV/path. Do not shift every exit to the center
or force every selected lane to an outside edge.

New camera-matrix tests project the actual head vertices onto the ground plane
and require them to stay in the anchor lane's +/- `.13` lateral bounds. The
12 fixtures / 24 small-wide captures include three-lane left turns with left,
middle and right primary lanes. Middle stays middle; ordering is not reversed.

The outgoing white arc was also overwritten by subsequent approach-road fill,
fade and tint passes. Paint the departing curve/leg's curbs after those fills,
without extending an internal solid line down the straight approach. When a
road offset exceeds the arrow's tight fillet radius, collapse the inner contour
to the offset tangent intersection; a negative-radius arc produced a white hook.
The junction remains uncapped. Latest artifacts: `output/lane-road-aligned/`;
both prior capture directories are preserved. Renderer stderr now has its own
log, preventing buffered debug text from corrupting machine-parsed capture rows.

## What is already available

`hook/routeguidance/rgd_tlv.c:rgd_parse_lane_information_value` parses 0x5204
LaneInformations: index/position, status, all angles and the highlighted angle.
The current native storage holds eight lanes and sixteen angles per lane.
The highlight is authoritative; when absent, the native parser uses the first
angle as a directional fallback. `1000` is explicitly preserved. The experiment
treats **both ±1000 and every angle outside [-180,180] as unknown**. It does not
normalize them to straight or U-turn. Unknown lane slots remain present.

`RouteGuidance.State` already carries positions, directions, statuses and full
angle vectors in the independent `lg*` event cache. `BAPBridge` resolves the
active lane event using the top-level lane-guidance index, validating its cached
event ID. Lane-cache slot numbers are not maneuver-cache slot numbers.

`laneGuidanceShowing == 1` is the existing authoritative display gate. Lane
guidance can legitimately appear well before approach, including highway
pre-positioning. The archived `plan_lane_guidance_renderer.md` is outdated: its
approach gate was removed, and command 0x07 is now CLEAR, not available for lanes.

Current renderer packets contain no lane state. No changes to hook parsing or
the HUD's BAP path are necessary just to prototype a different presentation.

The fields do **not** provide lane widths, a vehicle lane position, or a full
lane-to-lane road topology. A representative recommended lane must never be
presented as the vehicle's measured current lane. If status is absent/unknown,
we must not promote that lane into a route recommendation.

## What the Audi references establish

HUD firmware assets:
`Firmwares/HUD/extracted/FL_8W6919617G_1952_APPL_V000/RE/navigation_icons.md`
and `navigation_icons/maneuvers_manifest.json` identify 158 small lane-arrow
variants, with one/two/three directions composed inside one lane cell. This is
separate from the main 3D maneuver. BAP lane status supports not recommended,
recommended and best recommendation; both recommended classes can use cyan.

The decompiled **MHI2 HMI** source provides concrete omission behavior:
`Tools/jxe2jar/out/MU1316-combined-vf/de/audi/tghu/navi/app/map/routeinfo/LaneGuidanceWrapper.java`,
`filterLaneCell` (line 144): if there are more than six cells, selects a six-cell
window using `hasBlueArrow`; changes an omitted edge cell's `arrowType` to zero.
`MixedListLaneGuidanceController.java`, `LaneData`, maps that type to the dedicated
`OMIT_LANE=78` atlas entry. Without a blue arrow, it keeps the first six and
replaces the last cell with the omission symbol. Both edges can be omitted.

This confirms the OEM presentation pattern. **The exact six-cell limit and
selection algorithm have not been independently proved in HUD MCU machine code**;
do not attribute this HMI implementation directly to the HUD firmware runtime.

## Visual proposal implemented in the host preview

1. One perspective road surface with dashed lane separators. Small pavement
   arrows show permitted directions; no second row beneath the maneuver.
2. Retain grey-blue base and ice-blue route progress. Use the same hue at lower
   intensity for recommended lane pavement. Muted lane glyphs are not suitable
   directions for this route, not closed lanes or prohibited movements.
3. Start the large route arrow in one representative preferred lane. Other
   suitable lanes remain visibly recommended. Preserve the chosen lane while
   it remains equally preferred (the pure layout helper supports this).
4. Build a canonical layout for the small rectangle `(59,27,210,153)`, with
   the primary lane centered and at most a small horizontal compromise.
   Include the nearest physical road edge and the complete contiguous corridor
   from that edge to the primary lane; ties choose the left edge. Three lanes
   is a soft initial preference, not a limit that may remove the true edge.
   If needed, remove optional context, then reduce secondary lane widths from
   0.26 to a minimum of 0.14. Keep the primary lane width 0.26 and arrow width
   0.14. For the full 328×180 visible area, retain exactly the same camera,
   primary mesh, lane positions and widths; append context where it fits.
5. No dots or omission glyphs. At most one side is hidden: show the actual
   adjacent lane continuing parallel and fading across its width. Its raw
   directions and recommendation status remain its own, rather than an
   aggregate of farther hidden lanes. The boundary is a dashed lane separator.
   Draw the actual outer road edge as a bright 0.022-wide solid line, compared
   with 0.010 dividers, so lanes can be counted from that edge.
6. Keep unknown lanes empty, not straight. With no valid recommendation, the
   prototype now falls back to the independent maneuver without lane geometry.
7. Increase pavement glyph stroke widths to 0.034 (muted) and 0.040
   (recommended), with larger heads and brighter colors. Place them closer
   to the viewer so secondary slight-turn branches remain legible.

The prototype reuses the existing filleted TURN route shape (scaled/translated)
and progress shader. The road is schematic. Its shape and outgoing pavement
are not evidence of real lane connectivity. No roundabout lane spiral is inferred.

Source size stays **328×181** (180 content rows plus the existing reserved row).
The macOS framebuffer is the original **656×362** Retina size, not an increased
source resolution. FOV is the original **40°**, and route width is **0.14**.
No camera height/tilt/focal-length changes. Only horizontal pan, at most ±0.08
depth translation, and the same vertical lens shift (`g_3d_offset_adjust=-0.16`)
in both small and large stages.
TURN segment length is 90% of the canonical geometry, with unchanged arrow width.

Framing checks project actual elevated route vertices above the hidden lower
entry extension, pavement glyph strokes/heads, a meaningful true-edge segment,
and the start of the continuation through the real camera matrix. Require 8px
padding inside the small rectangle for the canonical layout. Additional wide
context must fit the full source with the same padding and camera. The
long entry and secondary pavement may continue below/outside the crop, as with
the existing renderer. The camera does not try to fit the whole asphalt surface.
Every capture aborts on a framing failure; `captures.json` records bounds,
camera, lane count/width, hidden side and primary mesh hash.

Captured: single right exit; two recommended lanes; fourth lane from the real
left edge in an eight-lane road; eight-lane right exit; left turn; unknown directions; no recommended
lane; separated recommendations. All sixteen crop/wide captures ran without GL
errors and passed framing with the eight-pixel inset. Equivalent visible scenes
also produced pixel-identical captures with fixed progress. Each small/wide pair
has exactly equal camera values, secondary lane width and primary mesh hash;
the wider stage shows at least as many lanes. `stage-stability.gif` demonstrates
the fixed primary composition while the frame and surrounding context change.
The pure layout helper passed 78,720 combinations under ASan/UBSan, including
the complete nearest-edge corridor and prohibition of two hidden sides.

The renderer source changes add a camera-matrix snapshot, a diagnostic
visible-area border draw, and an optional world-space side fade for flat mask
paint. The fade uses one shader uniform and no extra framebuffer, texture or
pass. It defaults to disabled and resets every frame; no production caller
enables it. The Java/hook/protocol remain unchanged.

## Live integration after visual review

- Share the existing active-event resolver/gate with a separate lane snapshot
  for RendererServer. Do not derive renderer data from lossy BAP glyph codes.
- Use a new command ID (0x09 is currently unused). Preserve raw lane angles:
  preferably versioned per-lane records plus an atomic commit keyed by event ID
  and generation. Eight lanes × sixteen raw angles do not fit in 46 bytes.
  Reject incomplete generations, apply latest complete state only, and replay
  it on reconnect. Do not silently quantize or truncate to the old plan's three
  directions without an explicit reviewed policy.
- Route clear/disconnect/hidden lane guidance must clear lane geometry. Future
  cached events must not replace the currently displayed event. Showing a
  pre-positioning lane event must not draw the current maneuver into a lane
  whose highlighted direction belongs to a different action.
- Validate that the active maneuver and selected lane describe compatible
  actions before shifting its arrow. Otherwise retain the normal maneuver.
- Validate that live lane records form a complete ordered road cross-section
  before identifying a true physical edge. The fixtures are contiguous and
  complete; a truncated or noncontiguous live list must not invent a shoulder.
  Fall back to the normal maneuver if completeness cannot be established.
- Integrate the road and arrow transform with the current maneuver transition
  engine; outgoing/incoming geometry needs its own immutable lane snapshot.
  No late side jump after an arrow transition. Fit each maneuver to the small
  rectangle once; viewport changes reveal context without changing the primary
  mesh, camera or lane widths. Lane-event changes occur with the maneuver.
- Cache road masks and route meshes by state/layout, retaining per-frame work
  only for existing progress/glow. The prototype uses the same road FBO and
  mesh shading, but its 33ms host loop is not a QNX performance measurement.
- Add Java→packet→native checks for stale events, hide/clear, reconnect,
  status-only updates and maneuver transitions; then build QNX and measure on HU.

No merge/deploy yet. Next is visual review of `output/lane-road/framing.png` and
`stage-stability.gif` at the existing source size, then integrate the chosen
presentation with live event ownership.

## Follow-up: shared exits, alignment and outline repair

The current preview additionally groups contiguous lanes sharing the maneuver
direction into a common exit with continuing dashed separators. Fixtures 8–11
cover two/three left-turn lanes and each primary position. Glyph groups fit
within their lanes with a 0.022 inset. The main arrow uses a shallow prism
(base 0.002, top 0.012, no extra lift); projecting its raised head back onto the
ground verifies that it stays in its own exit lane. These changes are preserved.

The white outline regression came from a full white exit surface underneath
the translucent approach and hidden-lane fades. Removed that backing surface;
actual shoulders and final outgoing curbs retain their 0.022 width and color.
Also trim the outgoing inner contour against its collapsed tangent corner:
otherwise a short wide exit can double back and paint a white spur across the
approach. The 48-contour regression detects four reversed segments in the old
code and none after the fix (ASan/UBSan).

Latest artifacts are `output/lane-road-aligned/`: 24 real GL captures covering
12 cases in both stages, with framing, camera/mesh equality, lane-group counts,
glyph clearance and projected head checks. `00-crop-source.png` and
`09-crop-source.png` show the repaired right exit and three-lane left turn.

## Follow-up: transparent-window halo

The outline repair did not resolve a separate alpha error visible in the macOS
window/screenshot. `glBlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA)` also multiplied
source alpha by itself: one layer produced premultiplied RGB but squared alpha.
FXAA then filtered RGB while keeping the center alpha, creating additional
invalid associated pixels. Flattening the captured RGB onto a dark matte hid
this defect, so the earlier visual verification was incomplete.

The shared renderer now uses `glBlendFuncSeparate` with source-over alpha
(`ONE, ONE_MINUS_SRC_ALPHA`), including restoration after contact AO. FXAA
filters RGBA together without additional texture fetches. Mask painting still
uses overwrite mode with straight color; association happens during composite.
No extra framebuffer or output resolution change.

Capture validates the final window RGBA with `RGB <= A` (2/255 tolerance) and
records results under `window_alpha` in `captures.json`. The previous binary
failed on its first frame: 4,929 invalid pixels, max excess 132/255. All 24
corrected frames have zero invalid pixels and zero excess. Native CPU regressions
also pass. A bundled host app under `build/lane-road-preview/Lane Road Preview.app`
allows inspection of the actual window through CUA. No HU deployment.

## Current junction geometry after the user's clarification

The user's main complaint concerned road topology, not the separately verified
alpha issue. The old pass order painted exit curbs on top of forward pavement;
straight rectangles also continued behind turn-only lanes. Offsetting the tiny
main-arrow fillet across three lanes collapsed the inner lane into a square.

All thin contours now precede every asphalt surface. The overlapping fills
erase internal contours, so shared straight+turn lanes form an open junction.
Turn-only lanes and their straight dividers/tints end at the common approach
tangent. Only lanes with a forward direction inside the departing group keep
their forward continuation. Other through lanes remain visible. A primary
lane's alternative straight glyph appears beyond the fork instead of beneath
the main arrow.

Multi-lane bundles use a common circular bend: primary radius is the distance
to the group's inner edge plus 0.055, leaving positive radius even at the
0.022-wide inner curb. The common approach tangent stays at y=-0.04; outer
lanes finish the turn farther ahead. This removes the folded/square lane while
keeping lane order and steering the primary arrow through its own lane. The
canonical small-frame fit can still pack secondary lanes as before; no FOV,
resolution or arrow-width change. Primary top height is 0.008 (base 0.002) to
keep the projected head inside its lane on the farther outer bend. Small and
large stages retain identical primary mesh, camera and lane widths.

Case 12 explicitly covers straight+right in one lane. The latest 26 captures
cover all 13 cases in both stages. `test_outline.c` additionally verifies
positive inner-curb radii for two/three lanes across every primary position,
turn-only termination, and shared straight+turn continuation. Latest review:
`output/lane-road-aligned/11-crop-source.png` and `12-crop-source.png`.

Case 5's synthetic data was subsequently corrected: its selected penultimate
lane permits straight+right; the outer lane permits right without recommendation.
Previously the outer lane accidentally retained the fixture's straight-only
default, making the rendered exit cross a through lane. The ±1000 unknown
lanes remain unknown. Tests now check the fixture corridor to the relevant
edge and distinguish permitted direction from recommendation. Do not repair
real CarPlay records by inventing turn permissions: ambiguous connectivity
requires a simpler maneuver presentation until supported by actual topology.

## Native lane-road safety gate

`maneuver_render/lane_safety.h` now provides a pure, read-only guard, called by
the native prototype before building any lane-connected geometry. It requires:

- An explicitly complete current-event snapshot; valid counts; contiguous,
  unique zero-based lane positions. Normalization clears completeness if lane
  or angle storage truncates the input.
- A valid recommended anchor whose highlighted direction agrees with the
  independent current maneuver (within the existing 5-degree lane matching
  tolerance, retaining half-degree maneuver inputs).
- Every outer lane between the anchor and the corresponding real road edge
  explicitly permits the turn. Unknown directions do not grant permission;
  recommendation status does not determine allowed directions.
- Geometry supported by this simple lane-road view. It cannot repaint a known
  different turn-only lane as straight. Sharp, U-turn and roundabout actions
  retain ordinary maneuver rendering instead of a fabricated lane topology.

Failure clears the lane presentation and draws only the independently supplied
maneuver on a generic unmarked road. No lane count, position, recommendation or
lane-derived branch affects that fallback. The raw input is never modified.
Case 5 is the corrected valid fixture; cases 13–17 deliberately retain blocked
exits, unknown outer lanes, incomplete snapshots, missing positions and a
different maneuver action. Case 6 (no recommendation) also uses fallback.

The shared guard's ASan/UBSan test covers 144 mirrored layouts plus malformed
records, both ±1000 sentinels, nonfinite maneuver angles, unsupported shapes,
half-degree angles and input immutability. It is included in the normal native
regression script. All 36 GL captures pass safety decisions, framing, stage
invariants and RGBA checks; different rejected snapshots with the same maneuver
produce identical images. Outputs remain in `output/lane-road-aligned`.

This is connected to the experimental renderer only. The future live lane
decoder still needs complete-generation/event ownership checks before it can
assert completeness; a full eight-slot buffer or contiguous prefix is not proof
that all physical lanes were received. No live lane integration or HU deployment.

## Stable hidden-lane glyphs across viewport changes

The packed eight-lane case exposed another preview bug: `continuation()` used
a fixed lane width 0.26 and center offset 0.13 while fully visible secondary
lanes used the canonical packed width 0.14. The hidden recommended arrow was
therefore larger and shifted sideways in the small stage. Continuation now
uses the canonical width and half-width center offset for pavement, tint and
glyphs, scaling only the fade span with that width.

Captures record hashes of the actual glyph paths, head direction and stroke
width for each lane. All common lane IDs must match between small and wide
stages, including the faded neighbor; opacity is intentionally excluded. The
36-frame suite passes, including lane 4 in case 2. `stage-stability.gif` now
demonstrates this packed eight-lane scenario.

## Follow-up: spare viewport space, branch shoulders, recommendation tint

Small mode previously kept only the minimum canonical lane window while wide
mode alone expanded context. Both now test remaining room after the same fixed
canonical camera/width fit. Case 2 reveals seven full lanes and its faded eighth
neighbor in the small stage; the primary pose and shared glyph hashes remain
identical to wide mode. The faded lane still uses its own canonical geometry.

When a turn-only lane leaves, the remaining through branch now gains its newly
exposed solid shoulder, starting at the junction. These thin contours still
precede all asphalt fills: the exit cuts its opening without a white internal
barrier. Mirrored turns, multi-lane departures and shared straight+turn fixtures
are tested. No global antialiasing change was made for this geometry correction.

Recommended lane tint now follows the selected turn strip to its end rather
than fading on the straight approach at y=.05. Permission and recommendation
remain distinct; a different or unknown primary angle does not color the exit.
Tests explicitly retain both ±1000 safety sentinels and unrecommended permissions.

## Native joined-route transitions in the lane preview

User rejected the substituted 0.80-second preview crossfade: the established
behavior is a camera following one arrow through the turn and into the next
maneuver. The substitute timer, independent outgoing/incoming route windows and
its queue-only test were removed.

The preview now calls the actual production `maneuver_start_push`,
`maneuver_prepare_frame`, `maneuver_draw` and `maneuver_commit_pushed_state`.
An optional scene provider in maneuver.c/h supplies prebuilt route paths and
road masks while retaining the same joined-route construction, curvature speed,
spatial road handoff, camera FOLLOW/RELEASE/settle and light settle. Without a
provider all built-in drawing and transition behavior remains unchanged.

Each immutable current/next scene expresses its path and masks in canonical
camera-local coordinates; the renderer applies one consistent rigid transform
to the next scene. The provider paints masks only, and the engine restores its
camera after any layout calculation. It keeps the lane prism at .002/.008 and
width .14. The engine gets the nominal entry at -.55 and applies its existing
animation extension, not the preview's already extended -1.4 entry. The road
remains extended to -1.4, with an optional backward mask fade so a hard cutoff
never enters view during camera motion. This adds one disabled-by-default mask
uniform, no framebuffer or resolution change.

Pending case requests coalesce without replacing the current/next snapshots.
New cases begin after the native camera settles. The provider is disabled for
conservative built-in FBO envelope calculation: a resize must not query an
immutable scene provider with unrelated synthetic sample state pointers.

Capture checks retain all 36 settled scenarios and stage invariants. Three real
native transition sequences additionally validate camera travel >1 world unit,
yaw >0.5 radians, the final settled pose and premultiplied RGBA on every frame.
Rapid queued requests are injected during push and must not interrupt it or
replace the eventual latest destination. Captures include fully idle frames after the light finishes settling; both
endpoint images must match the corresponding settled scene pixel-for-pixel. Native CPU regressions and provider-aware layout/outline tests pass.
The current GIF is `output/lane-road-aligned/maneuver-transition.gif`.

Live event/generation ownership and Java/native lane transport remain pending;
this is still an uncommitted experiment, without HU deployment.

## Transparent side-road ends

The lane prototype had stopped each outgoing road abruptly at arrow endpoint
+0.20. Restored the established road presentation: that reach remains solid
under the head, followed by a 0.30 smooth fade along the outgoing heading. The
same fade applies to asphalt, white curbs, dashed separators, recommended lane
tint and generic fallback pavement. Straight-road fade and the open road union
remain unchanged; the arrow itself stays opaque.

The existing entry-fade uniform now supports any directional plane. The tail
uses one extra strip quad instead of dozens of alpha slices, then restores the
backward entry fade. The native camera engine receives the complete 0.50 road
reach for its spatial handoff. All settled/stage and native transition captures
pass, including exact endpoint images and premultiplied-alpha checks.

## Shared-fork shoulder junction

At a shared straight/turn fork, the straight shoulder previously faded before
meeting the still-opaque diagonal shoulder. The preview now adds a small tangent
fillet at that exterior corner and starts the forward fade beyond the join.
Both strokes therefore meet at the same opacity; asphalt still covers interior
curbs. Tangential turn-only departures retain their existing contour. This is
local mask geometry, with no extra render pass or change to camera/arrow scale.
Mirrored 45/90-degree joins are checked for symmetry and tangency. The 36 settled
captures and 377 native transition frames pass, including exact endpoints.

## Progress retreat and refill at maneuver handoff

User paused the requested master merge after observing that the next maneuver's
fill appeared during camera travel. The shared arrow-progress controller now
retracts the old spatial front to zero over 0.40 s, holding spatial brightness
constant while the boundary moves. Old blink glow fades during departure. New
packets update a held target until the native engine commits the next maneuver;
then the existing velocity-preserving filter grows the new fill from zero while
the camera settles. No route/camera timing or projection math was substituted.

Production calls begin/finish at actual engine push/commit, rather than resetting
progress when any maneuver command arrives. Progress for a third pending maneuver
has separate ownership and cannot overwrite the entering second maneuver's
target. Geometry refreshes do not start another retreat. CLEAR/NONE reset the
controller, and an unusually early commit still completes the retreat smoothly.

The preview now uses this same controller with synthetic level/phase packets;
its settled target is level 4 (75%), replacing the old constant 72% shader input.
Native CPU tests cover frame-rate invariance, early commit, incoming retargets,
blink/off and actual engine pending-target ownership. All 36 static captures and
377 transition frames pass, including visible retreat, empty commit, monotonic
refill, premultiplied RGBA and exact static endpoints. Refactor/live lane
integration and the master merge remain pending after the user's stop.


Viewport context follow-up: retain the complete physical lane geometry in both
stages. The user subsequently requested removal of the small-viewport crop and
reported cut corners during camera travel. Viewport clipping/fading is now
removed from the road mask entirely: a baked-in projection plane would rotate
with the moving scene. Only entry/exit distance fades remain. The red small
frame is a composition guide, while all available source pixels can show road
context. Camera, main route and glyph geometry remain fixed. See
`SCENE_ENGINE.md` and the current `output/lane-road-aligned` captures.
