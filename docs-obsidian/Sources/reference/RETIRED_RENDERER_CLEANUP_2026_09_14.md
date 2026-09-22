# Retired renderer cleanup — 2026-09-14

Completed the cleanup after the HU cursor removal and the decision to retain
PRISM only as altScreen waiting artwork.

## Removed

- HU cursor renderer, Java cursor/startup packages, custom `TMDeviceControl`,
  main-video observer, corresponding hook interposes, protocols, context 103,
  process supervision, preview and retired tests. These removals were already
  underway; this pass verified their source/build/package closure.
- Remaining PRISM video zoom/reveal shader branches, handoff state and animation,
  demo video texture/PNG/generator, transition capture tools and cache tests.
  Production and desktop preview now share a single waiting shader and geometry.
  Runtime loader code, shader source and assets live in `altscreen_render/loader/`;
  the preview consumes them directly. `common/` retains only the managed-window helper.
- Orphan cursor PNG, unused classic signature offset, framebuffer/cache-only
  hostcheck declarations, obsolete preview test binaries and generated transition
  demos. Updated and rebuilt the desktop PRISM app.

## Intentional remaining paths

Touchpad movement still produces DPAD input through `input/TouchpadController`.
Touchscreen, rotary and selection use stock DSI paths. Stock `TMDeviceControl`
owns the main menu identity; HU display context 78 is `{16,59}`.

The card installer/uninstaller still names `cursor_render` to remove the old
executable from an upgraded HU. Their regression test checks that removal.
Those are the only current source/build/test references to the retired renderer.
The cheap QNX monitor supervises only maneuver_render and altscreen_render.

The native video ownership barriers, source UV refresh, loader stop watchdog,
input timeout, decoded-output catch-up and receipt timestamps remain in use.
Maneuver animation, HUD/BAP, lane panel and cover art paths were not changed.
Firmware research and deployment logs remain as evidence.

## Validation

- Java stock audit: 33 sources, 107 patch classes; both MU1316-final and combined
  stock JAR linkage checks have zero errors. No cursor/startup/TMDeviceControl
  replacement class is present in the shipping JAR.
- Java input/transport tests and 39 matching C/Java wire constants pass.
- Native bus and real-presenter tests pass with ASan/UBSan; QNX hostcheck passes.
- Both renderers build for ARM without emutls. The isolated hook/APAC build
  passes exact export/runtime guards (26 hook exports, no main-video interposes).
  That hook validation used HEAD's `hssr_dsp.c` to exclude unrelated ongoing
  audio research; it did not overwrite the normal hook build output.
- Supervisor/PID identity and install/uninstall/legacy-removal tests pass.
- Real GL waiting tests cover four layouts and six phases. Four 180-frame
  captures verify animation, black margins, signature and fullscreen style parity.
- After moving the loader into altScreen, the shader and artwork hashes remain
  identical; ARM build, hostcheck, presenter/sink ASan/UBSan and real GL checks pass.
- `git diff --check` passes.

Logs: `build/cleanup-*.log`. Current altScreen candidate:
`build/releases/altscreen-prism-cleanup-20260914/`. Isolated hook validation:
`build/cleanup-hook-validation/` (see scope.json; not a deployment package).
Nothing was deployed by this cleanup. Physical HU timing remains unverified.
