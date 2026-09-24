# TODO

## Current status — 2026-09-17

The owner confirmed that connection/reconnection stability and transitions
between music, calls/Siri and the stock/parking UI are **closed** for the current
vehicle setup. Do not carry older "vehicle validation pending" notes forward
as current blockers. This confirmation is the current acceptance source;
individual historical fault-injection and NCM/SI A/B recipes below are not a
newly recorded test run.

September 22: the owner no longer sees the alarm-associated interruption
(CarPlay picture + audio loss with session teardown, reported September 17).
Closed as not reproducing; the cause was never captured. If it returns, see the
capture checklist in git history (`TODO.md` at `8814f22`).

Keeping CarPlay visible beside OPS is fixed. On September 22 the owner's photo
with r3 shows the APS warning and the clock/LTE footer gone, but a black block
left centred at the bottom (the entertainment-drawer glass plate). Fix r4 is
built, not yet on HU. Intended presentation (owner): with a partial popup beside
CarPlay there are **no** bottom strips at all.

Implemented and accepted feature set: maneuver graphics, lane guidance, HUD,
altScreen projection and audio. Cover art, metadata and steering-wheel input
are also implemented. The September 17 rebuild uses the repaired GCC 8.5
toolchain; rebuilding artifacts alone does not change what is installed on HU.

### Open: "Navigation is initializing" over the CarPlay cluster map

The owner still sees the VC's "Initialising navigation system. Please wait..." over the CarPlay
map (2026-09-23), so the September 16 deferral (`7d35b53`) does not work. Static analysis: the text
is the VC Nav NA screen, selected by NavSD FSG_OperationState OP 3 (INITIALIZING). The HU sends it
not only from `showInitializingScreen` (`appIsReady`, the only input the deferral touches) but also
while the "Navi" app has not started, during the BAP init sequence, a software download or a BAP
stack drop. A clamp in a patched `InitializationManagerNavi` (`b4a661a`) did not help on the car
and was removed 2026-09-24: it can only act after CarPlay activates, and the screen shows from cold
boot until then. Open: needs an earlier "CarPlay will own the cluster" signal.
Check: cold boot with CarPlay shows the CarPlay cluster map and no NA screen; without CarPlay stock
navigation still shows its own "Initialising" screen. [Details](docs/cluster-and-rgi/NAV_INITIALIZING_SCREEN.md).

### Closed: CarPlay telephony on the cluster tab

Working on the car, confirmed by the owner on 2026-09-23 on the first try:
[design and status](docs/cluster-and-rgi/CARPLAY_TELEPHONY.md). The cluster phone tab shows
CarPlay calls (state, name, number, timer) with accept/hangup/hold/dial, mic mute, recents and
favorites, the missed-call indication, signal, register state and carrier. Caller picture: not
possible, iAP2 CallStateUpdate carries no image.

### Closed: connections and recovery

- `13801f2`: generation-scoped native/Java transports, decoder configuration +
  IDR recovery, stale-worker fencing and supervisor ownership.
- `8ca2c73`: lifecycle publication and video readiness independent of further
  input frames, including static scenes and reconnects.
- `d61e37f`: immediate `exec dio_manager`, asynchronous monitor, persistent
  renderers, exact process identity and stale-generation ownership checks.
- Current implementation: [supervisor](deploy/smartphone_integrator/README.md).
  The [USB runbook](docs/sources/USB_REPLUG_RECOVERY_RUNBOOK.md) preserves the original
  diagnosis and per-component A/B procedure.

### Closed: mode transitions

- `44c772a`: stock main audio, including calls/Siri, takes priority over buffered
  media; music can reclaim the route only after the stock owner releases it.
- `fc9d4a5`: buffered transport and PCM lifecycle hardening.
- `55e532d`: parking focus transitions and keeping CarPlay visible beside OPS.
  [PDC history](<docs/cluster-and-rgi/PDC_SMALL_STAGE.md>) already records the
  owner's on-car confirmation/photo from September 16. This acceptance does
  not cover the lower warning strip or the suppression attempt in `efc3020`.
- `BAPBridge.forceGfxAvailable()` now uses the stock KOMO callbacks and a typed
  `ClusterViewMode` fallback; the old reflection-ladder TODO is complete.

Local recheck on September 17 passed: supervisor lifecycle/PID identity,
USB generation fencing, PDC resource policy and external events against the
rebuilt shipping JAR, media-route call/Siri priority, and actual audio-interposer
setup/teardown races. Commands: `scripts/test_supervisor_lifecycle.sh`,
`tests/renderer_pid_identity_test.sh`, `scripts/test_usb_replug_recovery.sh`,
`scripts/test_pdc.sh`, `tests/media_route_test.c` and
`scripts/test_audio_interposers.sh`. Logs: `build/status-review-20260917/`.

### Remaining work / optional extensions

- OPS lower strip: **r4 built, HU acceptance pending**. On HU, APS masking
  (model 538) removed the warning and r3 removed status line 62; the remaining
  black block is the entertainment drawer, which stock toggles together with 62
  (parking choice 3915=1 -> TerminalMode `0x30D400` stub `renderStyle 0` ->
  `showPopup(62)` + drawer visible). r4 hides the drawer under the same guard:
  `PartialPopupManagerEvoHigh.hideFooter()` when OPS opens, and the patched
  `StatusBarStubController` turns a guarded drawer-visible request into hidden.
  Check on car: side OPS leaves nothing at the bottom; reverse still opens the
  full-screen camera; HOME/camera show the stock footer.
  [Details](<docs/cluster-and-rgi/PDC_SMALL_STAGE.md>).
- Upmix: **accepted by the owner on 2026-09-22** ("everything was great") with the
  shipped calibration in `carplay_startup.sh`. The retention r* grid and other
  ideas in `docs/audio/UPMIX_CALIBRATION_AND_TUNING.md` are optional, not debt.
- CarPlay Video: receiver/player integration remains unimplemented; optional
  next feature. [Research](<docs/altscreen-and-video/CARPLAY_VIDEO_PLAYBACK.md>).
- Partial 360-camera beside CarPlay: deliberately deferred by the owner on
  September 14; resume only on a separate request.
  [Preflight](<docs/altscreen-and-video/PARTIAL_360_CAMERA_PREFLIGHT.md>).
- Optional MHI3 parity: decide whether the special `routeState==5`
  accept-all-maneuvers behavior is useful. This is not a current failure of
  rerouting or a release blocker.
- Release housekeeping: refresh the existing SD package/documentation for the
  chosen release; no new installer implementation is required.

## Historical investigation — August 2026

The snapshot below preserves observations, rejected hypotheses and useful
capture procedures. Its open questions describe that investigation, not the
current project's acceptance status above.

### Wife's phone connect loop — mechanism found 2026-08-21/22
**The head unit was never publishing its Bonjour service, so the phone had
nothing to connect to. A head-unit reboot fixed it; the phone was innocent.**

**How wired CarPlay actually starts** (from the phone's sysdiagnose, with Apple's
CarPlay Diagnostics profile installed):

```
accessoryd : Accessory identification accepted and identified for CarPlay
ACCCarPlayService : iAP NCM Start -- success
airplayd   : Created APEndpointCarPlay 'CarPlay' id '2C:DC:AD:56:64:3E'
airplayd   : APBrowserCarBonjour DNS = 'CarPlay._airplay._tcp.local.%31'
airplayd   : Connect over DirectLink
airplayd   : APTransportConnectionHTTP 'CarPlay-ctrl' created  -> GET /info -> RTSP
```

The session is started by **`airplayd`**, not `carkitd`, and it is gated on
**mDNS discovery of `CarPlay._airplay._tcp.local` on the NCM link**. `carkitd`
only does navigation/connection-time bookkeeping.

**Proof it was the HU.** In the whole evening's log archive
`Created APEndpointCarPlay` appears **exactly once** — in the single successful
session. In every failing cycle `airplayd` starts, brings up `CarPlayAVVC Server`,
and never creates the endpoint. Meanwhile on the HU `carplay0` carried ~5 packets
per cycle (IPv6 ND only) — no mDNS advertisement ever left the unit.

After an HU reboot, with the same phone and cable, `dns-sd -B _airplay._tcp` on
the unit finally showed the missing line and the session came up immediately:

```
Add  if 8  local.  _airplay._tcp.  CarPlay      <- registered on carplay0
GENS=3 IDENT=3 INFO=1 SETUP=2 ALT111=1   carplay0 7318/3998 packets
```

**Confirmed after the reboot (2026-08-22, same phone and cable):** the fix holds
across replugs. `NOSETUP=0` — zero failed generations, where every attempt all day
had produced 3-6. One generation, immediate success, cluster stream up, and
`dns-sd` shows `Add if 8 CarPlay` on each connect and `Rmv` on each unplug. So the
broken state accumulates over a session and a head-unit reboot clears it; it is
not a permanent property of the phone or the hardware.

**Still open:** why the registration goes missing in the first place. `carplay0` is destroyed and
recreated by `stopncm.sh`/`startncm.sh` on every dio generation, so `mdnsd` must
re-register the service each cycle while SI kills dio at ~15 s. That is a race,
and it is self-feeding: the more the loop churns the less chance it has to win.
It also explains "works once after a reboot, then never" — the first attempt on a
clean state wins. Not yet proven; next step is `dns-sd -B _airplay._tcp` on the
unit during a failing cycle, plus checking whether our PF ruleset change (never
reverted in any test all day) affects outbound mDNS on `carplay0`.

**Hypotheses that died, in order — do not re-test these:**
1. NCM function not starting (`drivers_running::1`) — that value is NORMAL; NCM is
   a `start"..."` one-shot in `usblauncher_otg.lua`, only `io-otg-cinemo` is a
   long-lived `driver"..."`.
2. Cable orientation — same cable works for the other phone.
3. `usblauncher -r` ("treat suspend as cable removed") — removing it does not
   help; `devu-usbrndis-msm8960-ci.so` itself declares the suspend a disconnect,
   before usblauncher sees it. Removing `-r` also left the device side dead.
4. USB bus speed — both phones negotiate `Full`, and CarPlay works fine on it.
5. Different USB controllers between iPhone models — both are 17 Pro-class, same
   silicon.
6. Phone-side settings, Bluetooth pairing, iOS version, Screen Time, pairing
   record — all symmetric between the two phones or excluded by measurement.

**Also learned:** the guarded USB reset (`reset port 3 250 1`) makes this state
WORSE — measured live, `drivers_running` went 1 -> 0. Do not widen
`cp_usb_stuck_pre_setup` to `running < 2`.

**On-unit measurement traps** (each cost a capture):
- QNX `grep` has no `\|` alternation — one pattern per call.
- No `tail` on the wrapper PATH — use `grep`/`cat`/`sed -n`/`awk`.
- `netstat -in` truncates the interface name: the row reads `carpl`.
- `/ramdisk/pps/device/usb-1.0.1` is the HU's **own** device-mode gadget
  ("AUDI AG / iAP2 NCM Accessory", VID `0x1a96`). In wired CarPlay the iPhone is
  the USB host, so no Apple VID `0x05ac` node exists in the tree at all.
- The slog ring is 1 MB and rolls in ~2 minutes with USB verbosity on — read it
  within ~20 s of the event.
- `io-usb-dcd`'s `-vv` does not raise the device-controller DLL's verbosity; that
  needs `verbose=` in `/etc/scripts/start_usb_device_stack.sh` (stock is 0).
- Never kill the OTG `usblauncher`: a hand-restarted instance loses the
  media-connector event chain and the unit stops seeing any phone until reboot.
  There is no `reboot`/`shutdown` binary on the unit — it must be power-cycled by
  holding the MMI knob.

**Phone-side capture recipe that worked:** Apple's CarPlay Diagnostics profile
from developer.apple.com/bug-reporting/profiles-and-logs, reboot, reproduce, then
sysdiagnose (both volume keys + side, ~1.5 s), collect from Settings → Privacy &
Security → Analytics & Improvements → Analytics Data. Read
`system_logs.logarchive` with `log show --archive`, and
`crashes_and_spins/accessoryd-packets-*.log.ips` for the raw iAP2 packet trace.

Historical lifecycle test recipes remain in
`docs/connect-and-usb/SESSION_LIFECYCLE.md`; Java cleanup findings are
preserved in `docs/build-and-deploy/PRODUCTION_AUDITS.md`. Use the current
status above when deciding what work remains.
