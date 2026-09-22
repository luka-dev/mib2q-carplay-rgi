# Reconnect and Lifecycle

*Why CarPlay once took three replugs to come back (lifecycle churn with no debounce), how the debounce fixed it, plus the advert correction and the main-screen UUID fix.*

---

## 🔍 The symptom

An on-unit diagnosis (build `34c4c9d`, 2026-08-11) looked at an intermittent reconnect: "после перевтыка не работает" ("doesn't work after a replug"), which took 3 replugs. The key finding: **the altScreen pipeline was not the problem**. Across 4 dio sessions the phone offered stream 111 reliably within 2–3 s, and video decoded and rendered fine; one session ran 13,056 frames in 5 min. The "no 111 stream in request" log lines are just the phone's *other* SETUP requests (main and audio), not a failure. **[car]**

## ⚠️ Root cause: lifecycle churn with no debounce

`TerminalModeBapCombi.updateActiveDeviceState()` drives `CarPlayApp.onActivate/onDeactivate` from the stock TMDevice ConnectionState (`java_patch/de/audi/app/terminalmode/combi/TerminalModeBapCombi.java:89-142`). On a fast replug the device bounces **ACTIVATING ↔ INVALID ↔ ACTIVE**. At the time, each edge made `CarPlayApp.lifecycleLoop` run a full `BAPBridge` takeover teardown and re-init, plus a cluster context flap **81 ↔ 74**. Ten `onActivate/onDeactivate` generations were observed. **[car]**

```mermaid
stateDiagram-v2
  accTitle: Reconnect device-state bounce and the deactivate debounce
  accDescr: A fast replug bounces the device through activating, invalid and active; a deactivate edge now waits up to 400 ms and is dropped if a newer activate edge arrives, otherwise it tears down.
  [*] --> activating
  activating --> active: settled
  activating --> invalid: fast replug bounce
  invalid --> grace: deactivate edge
  grace --> active: newer activate within 400 ms
  grace --> teardown: no newer edge in 400 ms
  teardown --> activating: next replug
  active --> [*]: session persists
  classDef danger_state fill:#fee2e2,stroke:#dc2626,stroke-width:2px,color:#7f1d1d
  classDef success_state fill:#dcfce7,stroke:#16a34a,stroke-width:2px,color:#14532d
  classDef warning_state fill:#fef9c3,stroke:#ca8a04,stroke-width:2px,color:#713f12
  class teardown danger_state
  class active success_state
  class grace warning_state
```

At that build, `lifecycleLoop` coalesced edges **only** when a newer edge landed while `applyLifecycle` was already running. A deactivate that was picked up first ran the full teardown and then re-initialised when the activate arrived. The 3rd replug worked only because the device state finally settled into a stable ACTIVE. **[car]**

### Implemented fix: deactivate debounce

The debounce is **implemented** in `lifecycleLoop` (`java_patch/com/luka/carplay/core/CarPlayApp.java:228-268`). **[code]**

- `DEACTIVATE_DEBOUNCE_MS = 400L` (`:230`). On `wantActive == false` the loop waits up to 400 ms on the lifecycle lock.
- If a newer generation (the re-activation) arrives within that window, the stale inactive edge is marked applied and dropped. The loop then re-snapshots the newer edge (`:252-265`).
- `applyLifecycle` runs under `lifecycleLock`. It re-checks that the edge is still current before and after each expensive step (`:182-222`), and it publishes `lifecycleAppliedGeneration` before releasing the lock.
- `updateActiveDeviceState` also re-arms from `ACTIVE` when dio returns straight to `ACTIVE` without a second `ACTIVATING` (`TerminalModeBapCombi.java:114-125`). It deactivates only when the device is no longer a CarPlay device (`:127-142`).

Java does not gate the `viewArea` send on `videoAvailable` (`java_patch/com/luka/carplay/core/AltScreenModule.java:204-215`). The hook covers that case instead: it caches the zone mode and replays it at the first IDR (`hook/altscreen/altscreen_hook.c:1425`). An early `viewArea ack status=-6735` therefore stays harmless. How well the debounce works on the unit under repeated fast replugs is **[open]**. See `memory/reconnect_flak_lifecycle_debounce.md`.

## 🔧 Main-screen 110 black: the UUID fix

This is a separate failure mode that happened at the same time. iOS fetches `/info` **once** at session start and builds all display and stream config from that snapshot, with no re-fetch. The hook's `main_disp_modernize` used to inject a **fabricated** main `uid` / `initialVideoStreams` UUID (`A0000000-…`). iOS 26.1 routed main video on that uid while the stock decoder waited on the **real** `ScreenCopyMain` UUID, so main 110 stayed black. Cluster 111 worked because the hook owns both its descriptor and its receiver. **[decompile]**

The fix was implemented and verified on the unit on 2026-08-11. The live log confirmed the real uuid and a **stable** session: onActivate=1 / onDeactivate=0, no churn, no teardown, 1792+ frames. Luka confirmed 110 works. **[car]** Current code reads the real main uuid with `cf_getval(mainDisp, "uuid")` and sets `uid` + `initialVideoStreams` to it. If the stock uuid is missing there is **no fallback constant**: it refuses the altScreen advert (`hook/altscreen/altscreen_hook.c:547-581`). **[code]**

## 💡 The advert correction

An older hook-era note said "the altScreen advert must stay OFF or the cluster flaps 80 ↔ 74". That note is **superseded**. The shipped startup script exports `ALTSCREEN_HANDLE_ALT=1` and `ALTSCREEN_ADVERT_DISPLAYS=1`, so the advert is ON (`deploy/smartphone_integrator/carplay_startup.sh:53-54`), and altScreen streams with it on. Both gates check the value, so `=0` really disables them (`env_on`, `altscreen_hook.c:704-712`). The advert is **not** what drives reconnects; the lifecycle churn above was. Keep the old caveat in mind only for the pure-hook cluster-context path in [[Cluster Contexts]]. **[car]** / **[code]**

## ⚡ Fast-replug recovery timing

Recovery took about 3 s because SI (Smartphone Integrator) serialises stop → relaunch (Controller state machine, DownTimer and RestartDeviceStack jobs). It waits for the old child to stop fully, plus `restartDelay`. It recovers, just slowly. **[car]** What the current source actually does:

| # | Item | Current state |
|---|---|---|
| 1 | Hook `read_exact` poll tick | **60 ms**, so the 111 receiver notices a teardown within 60 ms (`hook/altscreen/altscreen_hook.c:1207-1213`). **[code]** |
| 2 | Renderer stop on dio exit | **Renderers are not stopped.** They are persistent: every ordinary `dio_manager` exit leaves both alive, and only a missing renderer is restarted, after a 5 s backoff (`deploy/smartphone_integrator/README.md:72-89`, `carplay_monitor.sh:74`, `:117-124`). `SIGTERM` → grace → `SIGKILL` applies only to an adopted `altscreen_render` that is known to be wedged (`deploy/smartphone_integrator/carplay_processes.sh:143-168`). There is no `cp_stop_renderer_snapshot`. **[code]** |
| 3 | SI child timeouts | `carplay_child.json` sets `stopTimeout` **8000** and `restartDelay` **3000** (`deploy/smartphone_integrator/carplay_child.json:10`, `:20`). The stock reference is 3000 / 2000 (`carplay_child_stock.json:10`, `:20`). The earlier 2000 / 500 tuning is not in the tree. **[code]** |

Physical USB replug recovery is kept out of these scripts on purpose. It lives in the NCM generation fence and the `deploy/experimental/usb_replug_recovery` candidate (`deploy/smartphone_integrator/README.md:95-97`). See [[USB Replug Recovery]].

---

## 📌 Deploy notes

- `deploy/smartphone_integrator/README.md` lists the install set for `/mnt/app/root/hooks/`: the startup/monitor/cleanup/process scripts, `libcarplay_hook.so`, both renderers, and so on. `children.carplay` in `smartphone_integrator.json` is replaced with `carplay_child.json`. The hook is `LD_PRELOAD`ed only into `dio_manager` (`carplay_startup.sh:45-50`).
- Renderers are built with `scripts/build_renderers.sh`. The note's old `scripts/deploy_unit.sh` does **not** exist in the tree, and the current per-file deploy helper is **[open]**.
- The unit's SSH target defaults to `root@10.173.189.1` (`scripts/carplay_state_trace.sh:5`). Pass ssh options inline, not through a shell variable, because macOS ssh breaks otherwise (operational note, **[open]**).
- The live Java tree is `java_patch/`. Never reboot the unit without asking.

Related notes: [[Cluster Loader]], [[Video Pipeline]], [[Deploy and Release]], and the section map in [[AltScreen and Video MOC]].
