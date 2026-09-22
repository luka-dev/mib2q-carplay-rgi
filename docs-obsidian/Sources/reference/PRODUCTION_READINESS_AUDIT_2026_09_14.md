# Production readiness — 2026-09-14

Update 2026-09-17: the owner confirms connection/reconnection and mode
transitions are closed on the current vehicle setup. Pending deployment and
validation notes below describe the September 14 snapshot, not today's
status. See [the current project status](../../../TODO.md).

## Follow-up: all three findings fixed

The follow-up patch closes all three findings below. The original findings
describe baseline `630acd9`, not the fixed candidate. The fixes have been built
and tested locally; physical HU validation and deployment are still pending.

- `CarPlayApp.applyLifecycle()` publishes its applied generation in a `finally`
  block while still holding `lifecycleLock`. External modules remain outside the
  small state lock. The five new lifecycle regressions cover publication, an edge
  during startup, superseding replug, exception completion and unchanged debounce.
  The publication regression rejects the unchanged baseline and passes the fix.
- The existing altScreen control thread samples presentation readiness about
  every 100 ms, independently of frame input. The provider holds the decoder
  queue lock while checking the connected/current generation and sink readiness;
  no sink callback acquires that queue lock. Native teardown suppresses readiness
  immediately, and the desktop sink resets its readiness between sessions.
  There is no new thread, socket, protocol opcode or static-scene idle timeout.
- altScreen rotates its log and reopens stderr in append mode. The regression
  invokes the actual log opener and actual shell `cp_cap_log`, then confirms the
  next producer write remains below the cap. The QNX supervisor is unchanged.

Validation: real native reader/worker/control-loop tests pass for a delayed
static first picture, health loss/restoration without new input, control reconnect,
stream end and stale generations. Readiness/log and OMX sink tests pass ASan/UBSan.
Full native/APAC/maneuver and Java input/transport/route suites pass, as do Java
stock linkage, 39 wire constants, host build/type checks and QNX ARM renderers.
The private-capture APAC integration skip and physical HU limits still apply.

Reproducible source snapshot and logs: `build/readiness-fix/`. A consistent
14-file package is at `build/releases/carplay-readiness-fix-20260914/`, with source
overlay and artifact hashes in `manifest.json`. It uses unchanged quiet hook/APAC
binaries from the clean baseline audit and newly built renderers/JAR. Concurrent
audio research is excluded. Canonical `build/` renderer/JAR outputs are refreshed;
the separate hook/audio build output was left untouched. Remaining work is an
on-HU validation of this exact package, followed by any needed hardware fixes.

## Original audit

Reviewed commit `630acd9a5084306a29a4025d3b95d5c3e71eea0e`.
Verdict: **not ready for an unconditional production sign-off**. The committed
package builds and its existing tests pass, but three reproducible gaps remain.
No runtime source was changed or deployed during this audit.

## Scope and reproducibility

The audit uses `git archive HEAD` in
`build/production-audit-20260914/Patches/mhi2-carplay/`, excluding the concurrent
uncommitted audio research. All four ARM binaries and the Java JAR were rebuilt
there. The hook was built with `LOG=0`, no raw RGD logging and no state trace.
The clean snapshot still includes the committed audio implementation and model;
their normal native/APAC tests are part of the audit.

`build/production-audit-20260914/artifact-manifest.json` records SHA-256 values
for 14 build outputs and deployment inputs, the revision, options and open gates.
The selected HSSR model matches the documented
`ab01463e9359f013c4b40f311d630bd20af48d761ad361563cb5246f6bd686ae`.
This manifest describes an audit build, not an approved deployment.

## Findings

### P2 — Navigation service edge can be discarded at activation completion

`java_patch/com/luka/carplay/core/CarPlayApp.java:250–260,284–289`.
`applyLifecycle()` releases `lifecycleLock` before the worker publishes
`lifecycleAppliedGeneration`. A Navigation rebind worker entering this gap sees
the older applied generation and returns without replaying the event. RGD can
retain its old service reference until another edge or reconnect.

Reproduced again with the current, unchanged class and a controlled publication
gap. The same event after publication rebinds normally. The test uses stubbed
modules/services; it does not establish occurrence on the physical HU or a cause
of the reported video lag. This predates the cursor cleanup.

Repair: serialize lifecycle application and applied-generation publication under
the same lifecycle monitor. Keep external module calls outside the small state
lock to preserve the Navigation gate lock ordering. Add a worker-level regression
covering a Navigation edge during activation and a superseding disconnect.

Evidence: `probes/carplayapp-src/`, `logs/lifecycle-race.log` under the audit root.
See also [the lifecycle review](CARPLAYAPP_LIFECYCLE_REVIEW_2026_09_14.md).

### P2 — VIDEO_ON depends on a later socket record

`altscreen_render/main.c:446–468` checks decoded readiness only after receiving
a record. Decode/presentation is asynchronous. If the reader queues the last
picture, checks readiness before it is presented and then blocks in the next
read, subsequent successful presentation does not publish VIDEO_ON.

The real `serve_client`, decoder worker and frame parser reproduce this with a
controlled delayed sink: readiness becomes true, but VIDEO_ON remains absent
through a 250 ms observation with no more input. A new empty record immediately
publishes it. The control thread only sends the stored state; it does not sample
the decoder. The hook has no periodic empty-record sender for this loopback link.
Thus the delay can last for the entire legal static-frame interval.

This can prevent Java's video-based VC context recovery during a TerminalMode
bounce. It does not imply every normal activation is black: ordinary connected
state owns the context independently. Physical Qualcomm decode was not simulated.

Repair: publish readiness independently of incoming frame traffic, with current
generation/teardown guards. Preserve static-frame support; do not restore an idle
timeout which disconnects a healthy static stream. Cover first/static picture,
disconnect, stale callbacks and replug in the regression.

Evidence: `probes/alt_static_video_test.c`, `logs/alt-static-video.log` under the
audit root. The probe passes ASan/UBSan while reproducing the missing notification.

### P3 — Renderer log writer defeats the supervisor's truncation

`altscreen_render/main.c:496–503` reopens stderr with `"w"`.
`deploy/smartphone_integrator/carplay_processes.sh:146–156` truncates the pathname
in place and assumes the producer writes with append semantics. Truncation does
not reset the producer's existing file offset.

A host reproduction using the actual `cp_cap_log` helper produced:
614400 bytes before truncation, 143 immediately after it, and 614419 after the
next renderer-style write. The file-size cap therefore does not persist. Actual
RAM allocation for holes on the QNX filesystem was not measured.

Repair: keep initial rotation/truncation, then use append writes for stderr.
The inexpensive supervisor loop does not need more scanning or monitoring.

Evidence: `probes/log_cap_test.py`, `logs/log-cap.log` under the audit root.

## Passed checks

- Full native shipping-hook suite, native maneuver suite and APAC fixture suite.
- Java stock source/linkage audit against MU1316-final and combined: 107 patch
  classes, no linkage errors; all classes target Java 1.4/48.0.
- Java input/transports, route-info and maneuver mapping. Coverage includes
  7890 chain rows, 1088 parity checks, 1083 lane angle/status cases, raw +1000/-1000
  sentinels, independent lane show/hide, CLEAR ordering, backpressure and replug.
- 39 matching Java/C transport constants. Its first run preceded the JAR build;
  rerunning after that required dependency passed.
- Supervisor/PID ownership, SD installation/uninstallation, USB recovery and
  USB bundle installation/rollback simulations.
- Quiet QNX ARM hook/APAC and both renderers; exact export/runtime guards and
  no emutls. Both renderers' QNX host syntax checks pass.
- Real OpenGL PRISM waiting checks across four layouts and six phases, plus
  generated-header freshness. Cursor/startup Java classes and TMDeviceControl
  replacement are absent; the HU context remains `{16,59}`.

Grouped results and individual logs are in `build/production-audit-20260914/`.
Passing suites do not cover the three findings above.

## Remaining validation and compatibility boundaries

One capture-driven APAC integration test skipped because the clean archive has
no private `logs/captures/apac_5_1_live_20260831_0205` recording. APAC fixture tests did
run. No real HU drive, decoder timestamp propagation, long-running Adreno timing
or reconnect endurance run was performed for this revision.

After the fixes, rebuild the complete package from that exact revision, then
check loader-to-first-frame startup, legal static scenes, queue latency recovery,
repeated disconnect/replug, small/full VC views, HUD/maneuver/lanes and input on
the target MU1316 HU. Record binary hashes and actual logs for that package.

The current supervisor enables second-display advertising. The existing
[iOS 26/27 investigation](IOS26_VS_IOS27_BONJOUR.md) records older-phone connection
failures; this audit does not close that compatibility issue or certify every
iOS version. Spatial output still depends on the existing six-channel HU patch.
