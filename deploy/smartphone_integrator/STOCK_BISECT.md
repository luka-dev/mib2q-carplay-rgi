# Bisecting our own supervisor against stock

> **CORRECTION 2026-08-24 (user).** SI kills dio routinely — on disconnect and around the
> normal connection cycle. Stock `children.carplay` sets `selfTermination: true` and a
> `cleanupScript` that slays `mdnsd` every time, i.e. a dying dio generation is the DESIGN, not an
> anomaly. So "the ~15 s dio churn" below is the wrong frame: dio generations ending is normal and
> stopping them is not the goal. The real question is **why a normal dio cycle takes `carplay0`
> with it**. See "What actually has to be answered" at the end. The wrapper-cost measurements in
> this doc are still valid as measurements; only their significance changes.

`CONNECT_FAILURE_ROOT_CAUSE.md` established that the phone fails with
`USB eligible: N (No Network Interface)` and that the trigger is the ~15 s dio generation churn.
The churn may well be self-inflicted. This is how to find out without guessing.

## What we actually changed vs stock

Stock `children.carplay` (from `MHI2Q_US_AUG22_P5087_MU1316`) against our `carplay_child.json`:

| field | stock | ours |
|---|---|---|
| `exec` / `path` | `dio_manager` in `/mnt/app/eso/bin/apps` | `carplay_startup.sh` in `/mnt/app/root/hooks` |
| `stopTimeout` | 3000 | **8000** |
| `restartDelay` | 2000 | **3000** |
| `cleanupScript` | `/etc/scripts/carplay_cleanup.sh` | our wrapper |

Everything else is identical, including `startupTimeout: 10000`.

## Why the wrapper is the prime suspect

`startupTimeout` is unchanged at 10 s, but dio no longer starts at t=0. Before `exec dio_manager`
the wrapper runs, in order:

1. `cp_enable_altscreen_firewall` — reads the active PF profile, generates a modified ruleset,
   syntax-checks it with `pfctl`, loads it;
2. `cp_usb_consume_pending_reset`;
3. `cp_seed_renderer_pid_files` — a full `pidin ar` process snapshot;
4. `start_renderer altscreen_render` — Qualcomm EGL/OMX init;
5. `sleep 1`;
6. `start_renderer maneuver_render` — a second EGL init.

And once the monitor is up it runs **two `pfctl -sr` snapshots per iteration** for the first 30 s.
Our own note in `carplay_processes.sh` records that this makes a tick cost **~5 s** of wall time —
concurrent with dio's startup, on an embedded box. Observed generations lasted ~15 s with
`ticks=3`.

The `exec` handoff itself is correct: the wrapper `exec`s dio, so the PID SI launched is the PID
that registers the DSI service.

## The test

Restore the child to bone stock — no wrapper, no PF work, no renderers, stock timings — and connect
the phone that reproduces the failure.

    # on the unit, as root
    mount -uw /mnt/app
    cp /mnt/system/etc/eso/production/smartphone_integrator.json \
       /mnt/app/root/hooks/smartphone_integrator.json.ours     # keep ours
    # then replace children.carplay with deploy/smartphone_integrator/carplay_child_stock.json

Outcomes:

- **connects reliably** — the churn is ours. Then bisect forward: put the wrapper back but move the
  PF work and the renderer starts *after* `exec` (or off the startup path entirely), and restore
  `stopTimeout`/`restartDelay` to stock.
- **still fails** — the churn is not ours, and the ~15 s death is in dio/SI itself. Then the next
  question is what makes dio exit, which `carplay_wrapper.log` already records
  (`no control SETUP pid=… ticks=…`).

Note this is a stronger test than the existing `/mnt/app/carplay_stock_mode` marker, which only
drops `LD_PRELOAD` — the wrapper and all of its startup work still run under that marker.

Nothing here restarts anything on its own: replacing the JSON takes effect on the next CarPlay
start.


---

## What actually has to be answered (post-correction)

A dio generation ending is normal. `carplay0` disappearing is not — it is created by
`startncm.sh` when the phone enumerates our NCM interface, and destroyed by `stopncm.sh`
(`ifconfig carplay0 destroy`). There are exactly two callers of `stopncm.sh`:

1. **USB removal** — `usblauncher_otg.lua` `func_removal` on the NCM interface. Fires when the
   USB device stack actually goes away.
2. **smartphone_integrator itself** — `smartphone_integrator.json` sets
   `"otgStackCleanupScript": "/etc/scripts/stopncm.sh"`. SI runs it as part of restarting the OTG
   stack (`CMediaConnectionServiceJob_RestartDeviceStack` / `_RestartHostStack` exist in the
   binary's vtables).

So SI has a direct path to destroying `carplay0` that has nothing to do with unplugging the
phone. The open question is what escalates SI onto that path. Candidate: retry exhaustion —
`maxRuntimeRetries: 2`, `retriesBeforeDowntime: 3`, `downtime: 30000`, `portResetTime: 250`.

### The one log line that settles it

SI prints, on every app-state transition:

```
[CSIStateRunning] %s : App=%s with [deviceID=%i|oldAppState=%s|reportedAppState=%s|newAppState=%s|
  oldRemovedFlag=%s|newRemovedFlag=%s|runtimeRetries=%i|setupRetries=%i|downTimeRetries=%i|
  portResetNeeded=%d|teardownIndication=%d|restartMe=%d]
```

plus `AppState=%s for app=%s, retry triggered...!` and `... ne more runtime retries left!`.

Grep the HU slog for `CSIStateRunning` across a failing session and read `runtimeRetries`,
`portResetNeeded` and `teardownIndication`. That says directly whether SI is escalating to a port
reset / stack restart (which would explain `No Network Interface`), or whether `carplay0` is being
lost some other way. This is a read-only diagnostic — no reboot, no redeploy.
