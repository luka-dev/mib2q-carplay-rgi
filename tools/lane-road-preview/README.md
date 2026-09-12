# Lane-road visual experiment

Standalone host prototype in `experiment/lane-road`, based on `3f87f64`.
Optional renderer helpers expose the camera matrix, draw the red visible-area
border and apply a side fade to flat mask paint (disabled by default).
Lane geometry is host-only; Java, hook and BAP lane transport are unchanged.
The shared progress handoff runs in both the production main loop and this preview.
No HU deployment.
Fixtures are synthetic and explicitly cover the available iAP2 lane fields.
Rendering uses the existing `render.c`, `route_path.c` and `maneuver.c`, including
the two-color progress material and contact shading.

```sh
make -C tools/lane-road-preview all test
build/lane-road-preview/preview --case 1
```

Left/right: animated case handoff. Up: 210×153 crop / 328×180 visible area. Space: fill/glow preview.
D: toggle red frame (on by default). Escape: close. The window shows the full source surface; the capture sheets apply
the actual crop. The demo supplies synthetic level/phase packets to the production
velocity-preserving progress filter. Its blink clock is illustrative; live phase
timing remains authoritative in Java. The settled demo target is level 4 (75% fill).

The main maneuver always fits the small frame. Changing stage reveals more
context with the same camera, primary mesh and lane widths. The nearest true
road edge and all intervening lanes stay visible; only the opposite side may
fade. There are no omission dots. Bold shoulders and thicker pavement arrows
make lane counting and alternative slight-turn directions easier to read.

```sh
python3 tools/lane-road-preview/capture.py
```

Latest outputs are in `output/lane-road-aligned` (previous `output/lane-road`
and `output/lane-road-side-streets` captures are preserved): enlarged and native-resolution captures,
`framing.png` shows four full-source views with the actual GPU-drawn crop border;
`native-size.png` shows them at 328×181 without enlarging the source resolution.
`stage-stability.gif` alternates small/wide on the packed eight-lane case.
`overview.png`, `comparison.png`, and the manifest identify the synthetic inputs
and record projection bounds, camera, lane widths and main mesh hashes. Capture
asserts equal camera, primary mesh and secondary widths across each stage pair,
as well as the 8px framing inset. It ignores keyboard input and hides its window
to keep screenshots deterministic while the user continues working.
`test_layout.c` checks 78,720 status/capacity combinations under ASan/UBSan,
plus both angle sentinels, unknown-slot preservation, ordering, anchor stability
and the complete corridor from the primary lane to the nearest physical edge,
with at most one hidden side.

Side streets now use the same .022-world-unit white shoulder as the main road.
Contiguous incoming lanes sharing the anchor direction form a common exit strip
with continuing dashed dividers. This is schematic lane continuation, not a
surveyed destination-road lane count. Cases 8/9 add two-/three-lane left turns;
case 1 remains the two-lane right exit. Unknown directions or position gaps do
not join groups. Cases 10/11 exercise middle/right primary lanes in a three-lane
left turn. Case 12 adds a shared straight+right lane. Cases 13–17 retain rejected
input scenarios to exercise the fallback. Thirty-six captures check stage
invariants, group counts, projected heads, safety decisions and framebuffer RGBA.

Pavement glyphs fit as a complete multi-direction group, with a common stem and
at least .022 world units of lateral clearance from each lane boundary. Fitting
includes shaft thickness and actual arrowhead corners; it never hides an overlap
by painting over the lane divider. Main 3D arrow width/FOV are unchanged.

The lane overlay uses a shallow prism (base .002, top .008, no extra lift),
not the floating standalone-maneuver profile. Raised head vertices are projected
back onto the road and checked against their own lane's +/- .13 boundaries;
ground-space centerline alignment alone did not catch the prior visual drift.
All thin curbs are painted BEFORE the asphalt surfaces: forward pavement and
turn pavement cover each other's internal contours, leaving an open junction.
Turn-only lanes stop their straight approach at the bend's tangent; a lane
with raw straight+turn directions contributes both surfaces. An alternative
straight glyph in the primary lane sits beyond the fork so the main arrow
does not conceal it.

Multi-lane turns use one common circular bend with a positive inner-curb
radius, instead of offsetting the tiny standalone-arrow fillet into a square
corner. The primary arrow follows its own lane through that bend. Camera and
mesh still remain identical between small and large stages. The single-lane
offset fallback trims its outgoing line at the tangent intersection, avoiding
reversed curbs on short exits. `test_outline.c` covers 48 offset contours,
positive radii across all primary positions, turn-only termination and shared
straight+turn continuation under ASan/UBSan.

No solid white road-shaped underlay is painted beneath translucent asphalt.
Only the thin actual shoulders/curbs are white, preventing a white slab from
showing through the forward and sideways fades.

The transparent window also requires valid associated RGBA. Source-over
blending now accumulates alpha separately instead of squaring source alpha;
FXAA filters alpha with the same samples/weights as RGB. The capture checks
actual final framebuffer RGBA (`RGB <= A`, allowing 2/255 rounding tolerance),
not just the flattened review image. Before the fix, case 0 alone had 4,929
invalid pixels with an excess of 132/255; all 36 current frames have zero.
These are shared renderer changes within this experimental worktree.

The unknown-angle fixture (case 5) has two right-turn lanes: the penultimate
lane is recommended and also permits straight, while the outer right-turn
lane is not recommended. Its previous straight-only default was an inconsistent
synthetic junction. The fixture check covers the complete exit corridor without
altering live input or inferring permissions from recommendation status.

`maneuver_render/lane_safety.h` is the shared, read-only eligibility check used
before building the lane road. It requires an explicitly complete snapshot,
contiguous unique positions, a valid recommended anchor matching the independent
maneuver, and confirmed directions through the exit corridor to the real edge.
Unknown/non-turning outer lanes and unsupported lane connectivity are rejected.
Normal sharp/roundabout rendering is unaffected; only the lane-road overlay is
declined. Clipping excessive lane/angle counts invalidates completeness.

Rejection displays the independent maneuver with a generic unmarked road, without
lane glyphs, lane widths/counts or inferred connectivity. Different rejected lane
snapshots produce pixel-identical fallback captures for the same maneuver.
`tests/lane_safety_test.c` exercises 144 mirrored layouts, sentinels, incomplete
records, action mismatch, unsupported shapes and input immutability, and runs in
both this preview's tests and `scripts/test_maneuver_native.sh`.

This guard is wired into the experimental native lane renderer. Live lane
transport is still unimplemented; its decoder must establish complete, current
event ownership before enabling the lane-road view. Never infer completeness
merely because eight storage slots or a contiguous truncated prefix are present.

Hidden continuation lanes use the same canonical secondary width, center and
glyph geometry as when fully visible. Only their opacity changes. The former
hard-coded 0.26 width / 0.13 center offset made a packed 0.14 lane's glyph grow
when it entered the fade. Capture now compares hashes of actual drawn glyph
paths and stroke widths for every lane shared between small and wide stages,
including the hidden neighbor (quantized to 0.0001 world units for float noise).

See `docs/LANE_ROAD_EXPERIMENT.md` for findings and the proposed live integration.

Both stages now reveal as many additional glyphs as fit after the canonical
small-frame composition is fixed. The packed eight-lane small case shows seven
full lanes plus the faded neighbor; geometry and glyph sizes match the wide
stage. A departing turn-only lane exposes a solid shoulder on the remaining
through branch. Shared straight+turn junctions remain open.

Recommended pavement tint follows the selected exit lane through its bend to
the road end. Permitted but unrecommended directions and unknown primary angles
do not gain a tint. This uses the existing flat road material, not arrow fill.

Case navigation now uses the production `maneuver.c` engine itself. An optional
scene provider supplies the actual lane path and road masks; the existing engine
joins current and next paths, moves ONE arrow continuously along that path,
rotates/follows the camera, then settles into the next scene. The rejected
0.80-second independent arrow/crossfade animation has been removed.

The provider normalizes both path and masks to their canonical settled camera;
the engine receives a nominal -0.55 entry and adds its own animation extensions.
Passing the preview's -1.4 offscreen extension doubled the travel gap. Asphalt
retains that longer extent with a backward fade, so its normally hidden cutoff
does not appear as a hard edge during the camera fly-through. Width, profile,
resolution and the fixed small/wide composition are preserved.

Current/next snapshots are immutable through push and camera settle; quick
requests update one pending target. No alternative preview transition timer or
speed curve remains. The production engine is unchanged when no provider is
registered. Live lane-event transport is still pending.

`capture.py` runs real transitions through right-to-straight, right-to-left and
lane-to-safety-fallback cases. It checks substantial camera travel and yaw,
latest-request coalescing, settled camera pose, and RGBA on every intermediate
frame. After the native light has settled, both endpoint images match the corresponding
settled scene pixel-for-pixel; framing and glyph invariants are unchanged.
Review `output/lane-road-aligned/maneuver-transition.gif`.

Exit pavement now stays opaque through the arrowhead (+0.20 beyond its path
endpoint), then fades smoothly to transparent over another 0.30 along the exit
heading. Asphalt, shoulders, separators and recommended tint share that fade,
including the generic fallback road. The native transition provider reports
the full 0.50 road reach. This reuses the backward mask fade as a directional
plane, adds only the final strip quad, and leaves the arrow material untouched.

During a maneuver handoff, the old fill boundary retreats to zero over 0.40 s
with smooth endpoints. Incoming progress is held until the native engine commits
the next maneuver; it then grows from zero using the existing target filter,
while the camera settles. Old blink glow fades during departure. A queued third
maneuver owns a separate pending target in production and cannot recolor the
second. Geometry refreshes do not restart the retreat. Captures verify monotonic
retreat, an empty commit frame, continuous refill and exact settled endpoints.
