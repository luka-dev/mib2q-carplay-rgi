# Why a dio cycle takes `carplay0` with it — smartphone_integrator RE

IDA on stock `app/eso/bin/apps/smartphone_integrator` (MHI2Q_US_AUG22_P5087_MU1316).
Findings are saved in `smartphone_integrator.i64` (functions renamed, comments attached).

## The answer

**A clean dio exit leaves the USB stack alone. An error/timeout dio exit makes SI restart the USB
stack, and that restart destroys `carplay0`.**

SI kills and restarts dio routinely — that part is by design. What decides whether NCM survives is
**how the generation ended**, not that it ended.

## Call chain

`CarPlaySupervisor_RunningState_HandleAppState` @ `0x186b00` — reacts to `reportedAppState == 9`
(`STOPPED`) and dispatches on the **previous** state (`AppState_toString` @ `0x172D5C` gives the
names):

| oldAppState | what SI does |
|---|---|
| 6 `DEINITIALIZING` / 7 `DEINITIALIZED` — clean self-termination | `cleanup(error=0, portResetNeeded)` |
| 8 `STOPPING` | `cleanup(error=0, …)` |
| 13 `TIMEOUT_WATCHDOG` / 15 `TIMEOUT_SHUTDOWN` | `cleanup(error=1, …)` |
| 11 `ERROR_SETUP` / 14 `TIMEOUT_STARTUP` / 16 `PREPARING` | retries left ? **`prepare(retry=1)`** : `cleanup(error=1)` |
| 12 `ERROR_RUNTIME` | retries left ? **`prepare(retry=1)`** : `cleanup(error=1)` |

`MediaConnectorHandler_prepare(app, retry)` @ `0x1800B0`:

```c
v23 = connectorType;             /* *(*(a1+40) + 980) */
if (v23 == 1) { if (retry) { MakeJob_RestartHostStack(...);   return; } softPath(); }
if (v23 == 2) { if (retry) { MakeJob_RestartDeviceStack(...); }  else { normal connect job } }
```

The stack-restart job runs `usb.otgStackCleanupScript`, which `smartphone_integrator.json` points
at `/etc/scripts/stopncm.sh` → `ifconfig carplay0 destroy`. The phone then reports
`USB eligible: N (No Network Interface)`.

`MediaConnectorHandler_cleanup(app, error, portReset)` @ `0x17FC48`:

- `children.<app>.cleanupScript` is executed via `system()` **only when `error != 0`**;
- `error == 0 && portReset == 0` → `softPath_noStackRestart()` — the stack is not touched;
- otherwise → `scheduleResetPort()` with `CONNECTION_FLAG_LONG_DELAY` / `NORMAL_DELAY`.

### Correction to an earlier claim

I previously wrote that stock `carplay_cleanup.sh` (which slays `mdnsd`) runs on **every** dio
generation. That is wrong: `cleanup()` only invokes the script on the `error != 0` path. A clean
`DEINITIALIZED → STOPPED` cycle runs no cleanup script and restarts no stack.

## The diagnostic that settles our case

Every transition logs:

```
[CSIStateRunning] onEvent_appState : App=%s with [deviceID=%i|oldAppState=%s|reportedAppState=%s|
  newAppState=%s|oldRemovedFlag=%s|newRemovedFlag=%s|runtimeRetries=%i|setupRetries=%i|
  downTimeRetries=%i|portResetNeeded=%d|teardownIndication=%d|restartMe=%d]
```

Grep the HU slog for `CSIStateRunning` over a failing session and read **`oldAppState`** at the
transition into `STOPPED`:

- `DEINITIALIZED` / `DEINITIALIZING` → dio exits cleanly; NCM loss is coming from somewhere else.
- `ERROR_RUNTIME` / `ERROR_SETUP` / `TIMEOUT_WATCHDOG` / `TIMEOUT_STARTUP` → **this is it**: SI is
  restarting the USB device stack and taking `carplay0` down. The fix then belongs in dio/our hook
  — make the generation end cleanly — not in the USB or firewall layer.

Read-only: no reboot, no redeploy.

## Context-struct offsets (app context = `*(controller + 96)`)

| offset | field |
|---|---|
| +1240 | appId |
| +1244 | current appState |
| +1256 | deviceID |
| +1260 | setupRetries |
| +1264 | runtimeRetries |
| +1276 | selfTermination allowed |
| +1376 | portResetNeeded |
| +1378 | teardownIndication |
| +1380 | restartMe |

---

## Static-only conclusion: the startup-budget path

No HU logs were available, so this is reasoned from the binaries and our own scripts.

`children.carplay.startupTimeout` is **10000 ms**, and SI starts that clock when it spawns the
configured `exec` — for us that is `carplay_startup.sh`, not `dio_manager`. Everything the wrapper
did before `exec` was therefore subtracted from dio's startup budget:

| pre-exec step | cost |
|---|---|
| `cp_usb_consume_pending_reset` | `sleep 1` on the latched USB-reset path (rare) |
| `cp_seed_renderer_pid_files` | one full `pidin ar` process snapshot |
| `start_renderer altscreen_render initial` | `cp_renderer_healthy` → `netstat -an`; `cp_kill_renderer … 1` adds a 1 s grace when the adopted renderer is wedged |
| explicit settle | `sleep 1` |
| `start_renderer maneuver_render initial` | second `netstat -an` on the same path |

Guaranteed floor ≈ 1 s plus a `pidin` and up to two `netstat`s; the bad path reaches ~3-4 s. If dio
then misses the 10 s deadline, SI reports **`TIMEOUT_STARTUP`**, which is exactly the branch proven
above: `prepare(retry=1)` → `RestartDeviceStack` → `stopncm.sh` → `ifconfig carplay0 destroy` →
the phone's `USB eligible: N (No Network Interface)`. The previously observed ~15 s generation
lifetime is consistent with `startupTimeout 10000` + `restartDelay 3000` + overhead.

**Status: hypothesis.** The branch and its consequence are proven by decompilation; that we
actually hit `TIMEOUT_STARTUP` is not — that needs one `[CSIStateRunning]` line from the HU slog.

### Change applied

`carplay_startup.sh` now performs renderer adoption **inside `monitor_renderers`**, which is forked
just before the exec, instead of on the pre-exec path. `pidin ar`, both `netstat -an` probes and the
1 s EGL settle now run concurrently with dio's startup rather than ahead of it, giving dio the full
SI budget. Renderers are persistent services and no dio startup step waits on them, so adopting
them a few hundred ms later is free. `cp_usb_consume_pending_reset` deliberately stays before the
exec — it must not race a dio that is already creating graphics clients.
