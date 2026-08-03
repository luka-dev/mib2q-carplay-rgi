# Run analysis — 2026-08-01 20:09 (unit clock ~00:xx Jan 01)

## State after this session's fixes
- ✅ **Bus (hook→Java) FIXED** — the multi-day blocker. Robust 2-thread reader (accept/read split,
  new connection preempts a stuck one) + hook `bus_shutdown` fork-safe (PID owner guard) + bus gated
  to dio_manager. Cover art + RGI frames now reach Java.
- ✅ **Cover art WORKS** — ported `AppConnectorTerminalMode` (+`$CoverArtProvider`) from the old
  working impl (git), derived from the CURRENT MU1316 stock + only the `pushCoverArt` patch.
  `updateCurrentStation → pushCoverArt → getPictureManager().responseCoverArt(listRef,21,resource,true)`.
- ✅ **AppState navignore WORKS** — ported `org/dsi/ifc/carplay/AppState` (`getOwner`/`getAppStateID`→0)
  so the stock system doesn't grab nav ownership from our BAP.
- ✅ **Disconnect → stock** — onDeactivate now fires on the real disconnect state (`carplay=false`,
  observed via TMDev diag log; the real state is INVALID, not NOT_ATTACHED).
- ❌ **RGI still not shown.**
- ❌ **AltScreen black** (no video; ADVERT_DISPLAYS off — enabling it -6740's dio, separate).

## RGI root cause (from carplay_java.log)
```
106  BAPBridge: REPLACE takeover engaged
114  RouteGuidance Bus update: count=0 dist=-1m
115  BAPBridge: Stopped (lightweight, renderer + BAP kept alive)
116  [BAP] updateRGStatus(0)
117  [BAP] updateActiveRGType(0)
118  [BAP] updateManeuverDescriptor(count=1 NO_SYMBOL)
...  (zeroing everything)
126  KOMO: gfxAvailable=false
129  BAPBridge: Shutdown (full teardown)   <-- RGI fully torn down
130+ cover art keeps arriving (CarPlay STILL connected)
```
1. **BAPBridge starts (takeover) then is Stopped + fully Shutdown while CarPlay stays connected, and
   never restarts.** So RG is deactivated (updateRGStatus 0). Likely a lifecycle bug: RgdModule/
   RouteGuidance tears BAPBridge down (re-activate safety churn from the connect/disconnect cycles,
   or a count=0/route_state handling that stops instead of holding).
   - Note memory: transient `count=0` at route start must NOT clear/stop — only `route_state=0` /
     `source_supports_rg=0` should. Verify the current RgdModule honours this.
2. **maneuver_render receives NO commands** — its log ends at "ready, waiting for commands on :19800",
   no client ever connects. So even if BAPBridge ran, the renderer popup (window 98 / ctx 80) has no
   data. Java-side `RendererServer`/BAPBridge → maneuver_render(:19800) push is not happening.
3. `gfxAvailable=false` (KOMO) — the known cluster-map blocker; on FPK not required for BAP, but the
   KOMO LVDS video path needs it.

## maneuver_render.log
Renderer is healthy (EGL 1.4, window id=98 ctx=80, FXAA, flag atlas loaded), **waiting on :19800** —
no client, no CMD_MANEUVER.

## Next step for RGI
Deep-compare the RGI LIFECYCLE with the old working impl:
- `com/luka/carplay/rgd/BAPBridge.java` (current) vs `com/luka/carplay/routeguidance/BAPBridge.java` (old)
  — WHY does the current shut BAPBridge down + not restart; when does the old start/stop.
- `com/luka/carplay/rgd/RouteGuidance.java` + `RgdModule` vs old — the start/stop decision on route state.
- `RendererServer` — why maneuver_render(:19800) never gets a client/commands.
Plus the ctx-80 tension: old impl did NOT take over the cluster (no ctx-80), RG showed on the stock VC.
