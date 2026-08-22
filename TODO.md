# TODO

## Open

### Wife's phone connect loop — mechanism found 2026-08-21/22, root cause OPEN
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

**Still open:** why the registration is missing. `carplay0` is destroyed and
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

## Other open (tracked elsewhere)
- On-unit lifecycle validation (5 fault-replay tests + drive) — `docs/reference/CARPLAY_SESSION_LIFECYCLE_AUDIT.md`.
- Java cleanup groups 1–3 + KOMO reflection (group B) — `docs/reference/JAVA_PRODUCTION_CLEANUP_AUDIT.md`.
- `routeState==5` accept-all-maneuvers logic not implemented (MHI3 has it) — decide if needed.
