# Friday baseline audit — 2026-09-14

> Historical snapshot before the final PRISM decision. Current altScreen uses
> PRISM waiting artwork with direct video; its layout uses libm again. The
> classic assets and desktop video-transition demo described below were removed.
> See [current implementation](ALTSCREEN_PRISM_WAITING_2026_09_14.md).

Baseline: `59646e98f7c9190ed431c23f432dbd48ffff35c9`, Friday 2026-09-11
22:09:46 EDT, `fix(renderer): fit bargraph to VC stage and latch maneuver side`.
Reviewed against HEAD `f5b43db` (13 reachable commits after the baseline) **and
the current working tree**, including relevant untracked files.

The current tree contains navigation improvements worth retaining. The HU cursor
and startup experiment is already removed locally. No additional runtime defect
was reproduced by this audit; the confirmed unnecessary runtime dependency was
altScreen's leftover `libm`, removed and verified below. Vehicle behavior still
requires target validation.

## Findings and cleanup

1. **The vehicle loader is classic again.** The local altScreen implementation
   uses the Apple/spinner/signature assets, identical to Friday's
   `common/loader_assets.h`, with immediate first-picture takeover. PRISM and its
   zoom transitions remain in the standalone desktop preview. This rollback
   predates this audit and is documented in the intermediate classic-loader notes (since removed).
   There is no production PRISM compositor, frame cache, or queue left to remove.

2. **Removed an unused runtime dependency.** Both altScreen build recipes still
   linked `-lm` after PRISM was disconnected. Its ARM ELF had a `libm.so.2`
   `DT_NEEDED` entry despite using no math-library symbols. Removed `-lm` from
   `altscreen_render/Makefile` and the altScreen link in
   `scripts/build_renderers.sh`. The authoritative QNX build succeeds and its
   ELF now has no direct `libm` dependency. Maneuver rendering still needs math
   and retains its linkage.

3. **Removed minor build-documentation leftovers.** Deduplicated the PRISM binary
   and test-output patterns in `.gitignore`; corrected the renderer build usage
   comment from three renderers to two.

4. **Most experiment weight is review material, not runtime code.** Relative to
   Friday, `output/` contains 402 added tracked files totaling 20.47 MiB in the
   working tree. The largest groups are `output/r2` (5.90 MiB),
   `output/arrow-progress` (5.75 MiB), and `output/maneuver-preview` (3.40 MiB).
   These are disassembly evidence, visual comparisons and reports; the vehicle
   build does not consume them. They are candidates for archival if repository
   size becomes a concern, and were preserved in this audit.

5. **The local cleanup is not committed yet.** Required untracked runtime inputs
   include `altscreen_render/pipeline_timing.h`, `common/loader_assets.h`, and
   `java_patch/com/luka/carplay/input/TouchpadController.java`. A future cleanup
   commit must include these and their tests, not just tracked deletions.
   A tracked-only diff misleadingly shows the classic artwork as deleted and
   hides the replacement touchpad controller. Unrelated audio research work is
   also present locally and was preserved.

## Changes since Friday that remain intentional

| Area | Current change and purpose |
| --- | --- |
| Maneuver graphics | Arrow progress replaces the separate bargraph. Retained scenes, road geometry, contact shadows and smooth handoffs support ordinary turns and complex maneuver families. The C++ scene implementation links through a C ABI without a C++ runtime dependency. |
| Lane guidance | A separate lane panel follows the active HUD lane event. Complete lane metadata, hide/reappear behavior, ordering and sentinel handling avoid displaying stale or unrelated lanes. |
| RGI delivery | Route generations distinguish cache reuse after reset. Failed publications retain dirty state and replay one shared snapshot. Queue backpressure preserves accepted maneuver order; source timestamps prevent remaining time from being refreshed by retries. |
| HUD integration | Next-street text is marked explicitly. Stock distance and KOMO graphics APIs replace reflection; graphics activation restores the stock cached data rate. |
| altScreen input | State-only video-config notices preserve avcC state. Local socket receipt timestamps measure queue age; obsolete decoded output may skip presentation while compressed reference frames remain intact. The final/static picture is preserved. |
| altScreen timing | The loader waits only unused frame time when swap returns early. Its thread stops before video presentation. The bounded input wait is 1000 ms, exceeding the 750 ms loader-stop allowance. These are latency/lifecycle corrections, not residual HU animation logic. |
| Artwork | Size-only stream setup is accepted, malformed/other stream setup rejected, and image signatures remain validated. Bounded stream diagnostics aid investigation. |
| Process ownership | Renderer PID identity checks protect start/stop operations against stale files and PID reuse. |
| Touchpad | The DPAD controller was moved from the cursor namespace to `input/TouchpadController`; the DSI sink is cleared before deinitialization. No HU pointer overlay is required. |
| Audio research | The modified HSSR file adds capture-only experiments behind explicit compile-time switches. The shipping build does not enable them. |

## Confirmed removals and compatibility details

`hook/main.c`, the hook export map, `CarPlayApp`, `DisplayManagerMIB2High`,
`carplay_startup.sh`, and `common/loader_layout.h` match Friday exactly. The HU
context is back to `{16, 59}`. The HU renderer/process, Java cursor/startup
packages, main-video observer and shared production PRISM implementation are
absent from the current source paths and build inputs.

The remaining `cursor_render` references in the installer/uninstaller and their
test are intentional: they delete an old installed binary during upgrade/removal.
Historical HU disassembly under `output/r2/` is evidence, not a build input.
Existing GPU lifetime barriers, source rebinding and complex-maneuver paths are
still used; removing them as apparent duplication would change behavior.

## Verification during this audit

- `bash scripts/test_route_info.sh`: passed presentation (1487 checks), mapper,
  delta retention, stock HUD distance/KOMO state, maneuver parity (1088 checks),
  viewport, lane transport/lifecycle, RGI recovery and native C-to-Java contract.
- `bash scripts/test_maneuver_native.sh`: passed native parity, scene ownership,
  contact-shadow and peer-loss checks with ASan/UBSan.
- `make -C tools/maneuver-preview test`: passed scene/cache tests, lane geometry,
  framing and actual host GPU lane-mask tests with ASan/UBSan.
- Ran the unchanged coverart, altScreen and RGD test block from
  `scripts/run_tests.sh`: safety/stream/pipeline, AV config, decoder sink, worker,
  timing, presenter takeover and malformed RGD input tests all passed.
- `python3 scripts/check_local_protocols.py`: 39 matching Java/C constants;
  107 shipping classes at Java 1.4 bytecode version 48.0.
- `bash scripts/build_renderers.sh`: both ARM binaries built, no emutls;
  the scene runtime-dependency guard passed. ELF inspection confirmed altScreen's
  direct `libm` dependency is gone.
- `git diff --check`: passed.

The audit did not deploy or commit changes. Host GL/OMX-stub tests and cross-linking
do not establish Qualcomm timestamp propagation, physical display pacing or
end-to-end latency on the HU. The new 200 ms decoded-output policy is a catch-up
threshold, not a measured or guaranteed latency bound.
