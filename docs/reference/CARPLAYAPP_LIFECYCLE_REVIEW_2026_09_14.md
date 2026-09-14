# CarPlayApp lifecycle review — 2026-09-14

Review only; no production lifecycle behavior changed in this pass.
The three-module order, generation checks, always-on transports, reverse-order
cleanup and retry-until-services-ready are still used. No cursor/loader module
remains. TouchpadController's DPAD route is independent and passed the preceding
Java transport/input checks.

## Confirmed race: Navigation edge at activation completion

`applyLifecycle()` releases `lifecycleLock` before `lifecycleLoop()` publishes
`lifecycleAppliedGeneration` under `lock` in its finally block. A waiting
`onNavigationServiceChanged()` thread can acquire `lifecycleLock` in that gap,
observe the old applied generation, and return. The edge is not queued/replayed.
If RGD had already obtained the old service, it can retain that reference until
another Navigation edge or CarPlay reconnect.

The controlled reproducer executes the actual private `applyLifecycle()` body,
holds the publication gap, sends the Navigation event, and then publishes the
applied generation. No RGD rebind occurs. The same event after publication does
rebind. Production occurrence/frequency has not been measured on HU.

A narrow fix is to serialize lifecycle application and publication together
under `lifecycleLock`, preserving the guard against rebinding before initial
activation. Module calls must remain outside the smaller `lock`; NavigationJobs
can call in while holding the navigation gate. Do not simply remove all guards.

## Debounce defers teardown; it does not retain the old session

A deactivate/reactivate of the same IContext within 400 ms drops the inactive
edge, but the subsequent active generation still invokes `stopModules()` and
restarts all three modules. An actual-worker test with a 25 ms bounce confirms
one additional start of each module.

Thus the grace period defers/coalesces teardown; it does not preserve already
running modules through a short bounce. The initial review overstated this as
a defect: dropping the inactive edge is consistent with the debounce's role,
and the next activation has always reset the modules. IContext belongs to the
TerminalMode component and cannot distinguish a genuine fast new phone/session
by identity alone; blindly skipping restart could retain old route state.

## Verified history

The current CarPlayApp.java is byte-identical to the Friday 2026-09-11 baseline
`59646e98f7c9190ed431c23f432dbd48ffff35c9`, before the cursor experiment.
Both SHA-256 values are
`ba3536d8c7dda2d21f12f98a266739f3dcdc0832989b49bb61de4d80783ca19d`.
The cursor removal did not introduce either lifecycle behavior discussed here.

- Before `65b44ec` (2026-08-09), activation/deactivation ran module start/stop
  synchronously from the stock caller under lifecycleLock. Navigation rebinding
  already used its own worker, but there was no applied-generation publication
  barrier and therefore no specific race described above.
- `65b44ec` moved module work off the stock HMI callback into the persistent
  lifecycle worker, adding generation checks and the publication gap/guard.
  This avoids holding up the stock callback while modules initialize or stop.
- `349408f` (2026-08-12) added the 400 ms inactive-edge grace period. Active
  generations continued to restart modules; the debounce did not change that.
- `7fce1c4` (2026-09-14) added cursor lifecycle callbacks, now removed.

These are source-history facts, not proof of which builds were running during
the user's successful drives. The controlled RGD race does not establish the
cause of the recently observed native altScreen startup/video lag.

## Reproduction

`build/carplayapp-audit/src/` contains an unchanged copy of CarPlayApp plus simple
transport/framework/module stubs. Both scenarios run in separate host JVMs.
`build/carplayapp-audit/results.log` records two reproductions and the stable-state
Navigation control. No physical HU behavior was simulated beyond these lifecycle
interleavings.
