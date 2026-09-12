# Arrow progress experiment

Branch: `experiment/arrow-progress`, isolated from master at `59646e9`.

## Behavior contract

- Replace the VC bargraph with a grey-blue 3D arrow filled from entry to tip in ice blue. HUD BAP data and maneuver geometry remain unchanged.
- Fill follows the projected centerline from its first entry into the actual VC crop to its last visible point, including bends and the arrowhead. World-distance tags preserve traversal order at crossings; screen-distance mapping makes the 16 increments approximately equal in visible length despite perspective. Hidden entry/exit extensions do not consume progress.
- The soft boundary spans 11% of that length (user preference, 2026-09-12). At zero it starts entirely behind the visible entry. Endpoint feather coverage is normalized so the first nonzero step does not suddenly paint the whole tail. No bright stripe or autonomous moving highlight. Width/3D geometry stay unchanged, so equal length does not imply equal pixel area.
- Projection uses the current animated camera/crop; big/small changes retain their existing smooth transition and never reset the target animation.
- Source levels fall from 16 to 0; fill grows from 0 to 1. Retargeting preserves position and velocity through a critically damped response (95% of a stationary step in ~300 ms). Frequent targets keep the motion going; arrival brakes and clamps without overshooting. Duplicates do not restart animation. Off is distinct from complete.
- Explicit low/high blink phases come from the same Java emission as HUD, every 600 ms. Native easing raises whole-arrow glow in 100 ms and lowers it in 150 ms; it never generates phases. There are only two palette endpoints: base grey-blue and filled ice blue. Blink low, off and empty share the exact base color; blink high matches full fill. No separate dim tint or bloom pass.
- A new maneuver preserves the currently visible tint, resets fill momentum and begins blending to its own latest progress immediately with route motion. There is no original-blue interlude or delayed post-settle reveal. Roads-only refresh retains progress. CLEAR resets all fill/glow state.
- Arrow fill is the only progress presentation (approved 2026-09-12). The old column drawing, side selection, collision preview, shadows, local blink timer, comparison CLI and capture tool are removed. Visible-area animation remains in `visible_area.h`. `--port=N` allows isolated host captures (default 19800).

## Wire compatibility

Fixed 48-byte packets and legacy level/mode fields remain unchanged. Flag 0x20 marks explicit progress state: MANEUVER payload[42], PROGRESS (command 0x06) payload[2]. Values: 0 off, 1 filling, 2 blink low, 3 blink high. Unknown values or inconsistent mode use the quiet off arrow. Old receivers ignore this flag/byte. Level-only senders with mode 1 drive ordinary arrow fill; mode 0 is off. Legacy mode 2 has no explicit phase and is treated as off; it never starts a native blink clock. MANEUVER carries an explicit off state as well as on. TCP order determines the latest presentation, including a blink tick received after a maneuver in one drain.

## Acceptance and release gate

Exercise source state -> BAP -> actual renderer packets, malformed/legacy packets, duplicate/retarget/reset animation, queued maneuvers, all path families, self-overlap and partial meshes. Capture actual GL frames at split/big crop, fill levels, blink and transitions. Build Java 1.4, host renderer and ARM/QNX. Vehicle visual/performance validation is required before merging or deployment; this isolated experiment does not install itself.

## Implemented and verified (2026-09-12)

- `scripts/test_route_info.sh`: Java 1.4 build; presentation 1363 checks, maneuver parity 977 checks, actual stock BAP serializer, explicit blink phases/cache replay, viewport and parser regressions passed.
- `scripts/test_java_transports.sh`: actual loopback transport, backpressure, CLEAR ordering and 30 restart regressions passed.
- `scripts/test_maneuver_native.sh`: ASan/UBSan; all native icon families, partial meshes, retraced paths, velocity continuity under 40 Hz targets, duplicate storms at 200 Hz, 30/60 Hz equivalence, reverse/closer targets, queued transitions, actual TCP FIN/RST, crop-relative origin, projected distance under unequal clip-W, repeated crossings, numerically integrated feather coverage, blink-edge timing, interrupted tint transitions and immediate maneuver dimming passed.
- Host GL captures: six route shapes at five fill levels, split/big crop, fill/blink/queued transition GIFs, level-only arrow compatibility, malformed/unphased quiet state, no local blink, exact palette endpoint equality, glow-off during maneuver motion and reconnect clearing passed. First six fill steps measured 23/22/22/21/22/21 pixels on the incoming shaft in the 656×362 capture, for both split and big crops (`first-steps.png`). Source hashes and measurements: `output/arrow-progress/checks.json`.
- `scripts/build_renderers.sh`: ARM/QNX builds passed, ARM ELF and no emutls verified. The HUD/VC phase relationship is verified at the BAP emission and renderer packet boundary; physical display latency and GPU timing still need vehicle measurement.

The reconnect capture exposed a pre-existing native bug: RST and event-write failures dropped TCP without clearing content or invalidating FRAME_READY. They now share the FIN cleanup path. EINTR no longer tears down a healthy socket.

Additional rendering cost: one float per mesh vertex (28.1 KiB per full mesh), up to 481 cached centerline samples (7.5 KiB per mesh) and a shared projection scratch map (~5.7 KiB). Only a visible spatial fill projects the centerline and looks up each vertex’s distance; settled off/blink skip that work. One attribute and two scalar varyings cancel perspective interpolation inside long triangles, followed by the existing soft color blend. Arrow fill adds no texture/FBO/draw pass or progress-driven mesh rebuild. Fill uses one analytic exponential per active tick; phase edges use a finite smoothstep with continuous retargeted color and stop when settled. Blinking has no native phase timer.

## Contact shadow at self-overlaps

A narrow contact strip now separates the upper arrow from the lower route face
without changing path geometry, height ramps, tint endpoints or alpha. It uses
actual top-surface boundaries and clips to lower receivers. An edge must
actually overlie a receiving triangle with positive height clearance. Adjacent
sloping faces are rejected geometrically; there is no source-path-distance
cutoff to leave the start of a crease unshadowed. Solid intersections are
handled too: some accepted sharp-turn meshes intersect rather than having an
open air gap. Internal triangulation edges do not cast a crease.

The maximum strip radius is 0.025 world units; larger height gaps weaken it.
The radius starts at zero at the crease and grows with clearance at the
projected casting edge. Clearance is normalized once across the mesh, so width
cannot restart at triangle or edge boundaries. The first 15% also feathers
opacity; the cross-strip edge stays soft. Width is evaluated per fragment to
avoid interpolation artifacts on long triangles. This replaces the earlier
constant-width strip and the source-distance cutoff that left the beginning
of the crease unshadowed. No extra attributes, uniforms or draw calls are
needed for the width taper.
Cached shadow geometry is rebuilt with the route mesh, so blink/fill updates
do not repeat the geometry computation. Coplanar meshes exit before edge
hashing. A multiplicative draw preserves alpha and uses the existing depth
buffer to protect the upper surface. No new texture or framebuffer; one extra
draw for the tested overlaps (at most 396 vertices in the 73-angle sweep).
Storage is bounded to 1536 contact vertices per mesh plus shared CPU scratch.
The host ASan/UBSan test measured about 0.30 ms/build for a 160-degree turn;
this is not a measurement of the vehicle CPU/GPU.

`tools/capture_contact_shadow.py --before <pre-contact-host-binary>` compares
eight real GL scenes, including partial and full fill. Ordinary 90-degree turns
and the non-overlapping U-turn render identically. FXAA can redistribute a few
pixels at the new edge. Evidence: `output/arrow-progress/contact-shadow/`.
`--origin --before <previous-contact-host-binary>` compares against the earlier
delayed onset and asserts new shadow coverage in the first quarter of both
mirrored sharp creases and the full-fill case. See
`output/arrow-progress/contact-origin/full-sharp-right.png` and `checks.json`.
`--taper --before <constant-width-host-binary>` verifies the narrowing at the
start. Latest visual: `output/arrow-progress/contact-taper/full-sharp-right.png`.
The CPU regression verifies a continuous width ramp from contact to full width,
an initially touching fold, coplanar slopes and nearby faces without actual
overlap. `contact-softening/` is historical
output from the previous source-distance-based attempt.

## Review and next step

Review `output/arrow-progress/fill.gif`, `fast-retarget.gif`, `blink.gif`, `blink-off-maneuver.gif`, `two-color-palette.png`, `maneuver-transition.gif`, `all-maneuvers.png`, `first-steps.png` and `big-screen.png`. These are captures of the actual renderer, not mockups.

Reproduce in this worktree:

```sh
make -C maneuver_render BUILD_DIR=../output/arrow-progress/host
python3 tools/capture_arrow_progress.py
python3 tools/demo_arrow_progress.py --interval .08
bash scripts/test_route_info.sh
bash scripts/test_java_transports.sh
bash scripts/test_maneuver_native.sh
bash scripts/build_renderers.sh
```

The arrow-only design and two-color glow are approved; source and paired Java/native binaries are ready for vehicle validation. Test the paired artifacts on the HU in both VC layouts (including sunlight/night brightness, blink timing and maneuver transitions), then integrate the isolated branch. The previous bargraph is fully removed from the native renderer; the stock HUD BAP bargraph data remains unchanged.
