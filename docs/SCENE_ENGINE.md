# Maneuver scene engine

The lane-road renderer now lives in `maneuver_render/scene`, as a C++11 scene
compiler with a C ABI. The host preview supplies fixtures and window controls;
it no longer owns road geometry, glyph fitting, camera layout or painting rules.
The visual baseline is commit `d15e6bf` on `experiment/lane-road`.

## Responsibilities

| Layer | Responsibility |
| --- | --- |
| `scene/scene.h`, `scene.cpp` | Owned scene, semantic cache key, validation/fallback policy, C API, backend replay |
| `scene/lane_model.h` + `lane_safety.h` | Normalization, recommendation/window selection, eligibility without invented permissions |
| `scene/layout.cpp` | Fixed small-frame composition, projected clearance, secondary lane width and visible context |
| `scene/geometry.cpp` | Path construction, shared bends, pavement/curbs, lane glyphs, fades, ordinary-family adapter |
| `visual_style.h` | Shared C/C++ asphalt/curb colors, road and arrow widths, route profile |
| `scene/style.hpp` | Scene-specific clearances, source/crop bounds and animation road extents |
| `scene/scene_internal.hpp` | Bounded command list, primitive encoding, compiler scratch and internal declarations |
| existing `maneuver.c` | Joined route, camera follow/release/settle, complete built-in icon families |
| existing `route_path.c`, `render.c` | Extrusion, progress mapping, contact shading, masks and GL backend |
| `tools/lane-road-preview/fixtures.h` | Synthetic input only; no fixture identifiers in the engine |

The pipeline is:

```text
independent maneuver + optional complete lane snapshot + settled projection
    -> normalize / validate / select road representation
    -> fit the canonical small frame
    -> compile route + retained pavement commands
    -> keep separate immutable current and next scenes
    -> native joined-route camera animation + replay of cached pavement
```

C++ manages scene construction and ownership. The tested C transition engine,
GL backend, protocol and shader pipeline remain in place. This is not an ECS,
a second animation engine, or a conversion of all existing C sources to C++.

## Lifetime and caching

Create current/next scenes once with `cr_scene_create()`. Call
`cr_scene_prepare()` when a maneuver, lane snapshot or visible stage changes.
It copies the inputs; callers can immediately reuse their input storage.
The returned route and diagnostics are borrowed until the next prepare/destroy.
Use separate scene instances during a push; swap their owners at commit.

The semantic key includes generation/event ownership, maneuver fields, used
junction angles, normalized lane records, previous preferred lane position,
settled projection and stage. Unused array tails and struct padding do not enter
the key. Repeated inputs reuse the retained scene. A new ownership generation
intentionally invalidates it even if the geometry happens to be identical.

A stage change recompiles context, keeping the same canonical camera, main
route and secondary widths. Progress/glow and animated camera pose are not scene
inputs. A lane-only refresh therefore does not start a maneuver transition or
reset progress; that decision remains with the caller's event ownership logic.

`cr_scene_configure_provider()` supplies the shared route profile and road
extents to `maneuver_scene_provider_t`. Its callbacks resolve immutable current
and next snapshots. `handles` returns false for `cr_scene_is_native()` scenes,
so the complete built-in painter handles those states. A missing `handles`
callback preserves the previous provider API's handle-all behavior.

Call prepare/paint on the renderer thread: existing route extrusion has shared
scratch state. Compilation has no GL calls, no clock and no fixture dependency.
`cr_scene_paint()` only replays the prepared pavement list through the existing
GL mask backend. Ordinary built-in scenes retain their existing painter/cache.

## Bounded work and failure

Each scene allocates its storage and compiler scratch at creation. Prepare and
paint allocate no heap memory. The compiler has bounded point/command buffers;
paint does not search for camera poses, fit glyphs or rebuild lane paths.
On the host ABI the display list is 425,992 bytes and compiler scratch is
373,808 bytes, plus the owned route/input/diagnostics (about 0.8 MB per scene).
Current/next need two instances. This is a memory bound, not a timing claim for
an actual head unit. The 33-fixture sweep currently peaks at 742/8,192 commands.

Malformed maneuver/projection input invalidates the old scene and returns zero;
`cr_scene_route()` then returns NULL and paint does nothing. Do not display the
previous scene after a failed prepare. Excessive lane counts, truncated angle
vectors or invalid status invalidate completeness. Both ±1000 angle sentinels
remain unknown. Failed geometry, command capacity or lane framing falls back
to the independent built-in maneuver, with a diagnostic reason.

Absent/rejected lanes use a simple unmarked turn only when no extra family
geometry is needed. Junction branches, sharp turns, U-turns, full roundabouts,
merge/exit/lane-change paths and destination details use the full built-in
family. NONE clears the scene. Native route diagnostics preserve the original
pose and test the stock crop; their bounds exclude decoration and hidden entry
extension. Lane scenes use the stricter 8-pixel inset and lane-clearance checks.

## Verification and builds

```sh
make -C tools/lane-road-preview all test
bash scripts/test_maneuver_native.sh
python3 tools/lane-road-preview/capture.py
make -C maneuver_render all
./scripts/build_renderers.sh
```

The tests cover 78,720 lane-window combinations, 144 mirrored safety layouts,
48 offset contours, 33 scene fixtures, 33,000 cache hits, owned current/next
snapshots, unused input tails, native route parity, bad inputs and bounded
fallback. Geometry and API tests run under ASan/UBSan without a GL context.

The capture suite checks 66 real GL stills and seven native camera transitions:
lanes-to-lanes, lane safety fallback, lanes-to-ordinary, ordinary-to-lanes and
ordinary-to-roundabout. It checks associated RGBA on every frame, exact settled
endpoints, progress retreat/empty commit/refill and no scene rebuilds during
camera travel. Before the later visual unification, all 36 original stills matched the
pre-refactor baseline pixel-for-pixel
(`output/lane-road-aligned/refactor-parity-before-style.json`). Current frames
intentionally include the user-requested shared gray road style; added ordinary
fixtures extend the coverage. The preview loads the production flag
atlas beside its binary, including when opened as an app bundle.

The QNX script compiles the C++ sources for ARM and writes
`build/libmaneuver_scene.a`. The objects link through the C linker with no C++
runtime, exceptions or RTTI. The script rejects accidental runtime dependencies.
The host renderer and preview also build the same scene sources.

Live lane transport is still pending: the production packet stream carries no
lane snapshot, so the production main loop continues using built-in scenes.
The QNX linker can discard unused scene code from that executable; the separate
archive contains the compiled engine for integration. This refactor does not
enable lanes on the car or claim on-unit performance measurements.

## Shared visual design

User decision after reviewing the refactor: use the new lane-road design for
both presentations, with the old neutral asphalt. `visual_style.h` is now the
single source for #646464 asphalt, 0.26 primary road width, 0.022 shoulder,
0.14 route width and 0.002/0.008 route base/top. Both the built-in C renderer and
the C++ scene compiler consume these values. Optional lane recommendation tint
is derived from the shared gray base, not a separate dark-blue asphalt color.

Simple unmarked roads use the same curved curb strips and directional exit fade
as the lane roads. Built-in fade segments also paint thin shoulders instead of
putting translucent gray over a full-width white backing. Roundabouts, sharp
turns, side branches and U-turn centerlines retain their family geometry.
Destination geometry has its own radius instead of depending on road thickness.
The road color/profile change also applies to ordinary production rendering;
live lane-event transport is still a separate integration.

Review `output/lane-road-aligned/unified-style.png` for matched ordinary/lane
scenes. Camera travel, progress handoff and source/crop dimensions are unchanged.

Palette follow-up: neutral near-white shoulders (0.98), brighter neutral lane
dividers (0.80), and neutral inactive glyphs (0.65). Blue remains the active
direction cue. These values remove the former blue bias in the base paint.

The complete physical road, turning bundle, tint and glyphs share the same
paint list in both stages. The small frame is a composition guide for the main
route, not a clip or opacity boundary. No viewport plane is baked into a road
mask: during native camera travel it would rotate with the pavement and expose
artificial diagonal/rectangular cut corners. Only the ordinary distance fades
at the road entry and exits remain. Neither source size nor camera/FOV changed.

Removed the projected side-fade shader, its two uniforms and retained commands;
the earlier 18-way lateral fade also remains removed. Fixture peak is now 742
commands. Mirrored CPU regressions require identical complete small/wide paint
lists; all 12 GPU lane scenes must match across the full source frame except
for the diagnostic red border. The capture suite also checks native travel,
including the three-left-turn-lane handoff.

Review `output/lane-road-aligned/09-crop-source.png` and
`output/lane-road-aligned/transition/contact-sheet.png` for the current result.
