# TODO

## Open

### ~~Wife's phone — CarPlay connect restart-loop~~ CLOSED 2026-08-21
**Cause: stale iOS state on that phone. A phone reboot fixed it on the first
attempt.** Nothing on the head unit was at fault.

Proven live on-unit, 13 consecutive failing dio generations:

```
GENS=13  IDENT=13  AUTH=13  COVER=13   INFO=0  SETUP=0
carplay0 up, fe80::22ff:fe44:6..., Ipkts/Opkts ~5 per generation (IPv6 ND only)
usb-1.0.1 "AUDI AG / iAP2 NCM Accessory" drivers_matched::2 drivers_running::2
```

So the HU side was fully up every single time — the NCM gadget ran, `carplay0`
existed with an IPv6 link-local, dio served, and iAP2 completed Identify + Auth +
cover art on 13 of 13 attempts. The phone answered Neighbor Discovery, i.e. it saw
us, and then never issued a single `GET /info`, mDNS query or TCP connection. iOS
simply did not start its CarPlay client.

After a phone reboot, same cable, same port, same build:

```
GENS=1  IDENT=1  AUTH=1  INFO=1  SETUP=2  ALT111=1
/info #1 -> control SETUP -> altScreen 111 -> 5376+ frames @ ~44 fps, no restarts
carplay0 Ipkts 59459
```

**Signature to recognise this again:** iAP2 100% healthy (music, artwork, Identify,
Auth) + `INFO=0` + `carplay0` up with only a handful of ND packets ⇒ phone-side,
reboot it. It is NOT the cable, the port, the firewall, or the hook.

**Kept from this investigation:**
- `AirPlayCopyServerInfo` logs every call unconditionally (`altscreen_hook.c`).
  This is the seam that proved the phone never asked for `/info`; it used to log
  only inside the advert branch, which needs `advert_ready_for_session()` and so
  only runs after control SETUP.
- `cp_usb_record_generation_result` logs `drivers_*` and the `carplay0` row for
  every generation without control SETUP.
- `CP_USB_LIVED_TICKS` threshold 4 -> 2. A monitor tick is not two seconds: while
  the boot PF guard is armed it runs two `pfctl -sr` snapshots per iteration, so a
  tick costs ~5 s wall and a full ~15 s generation reports `ticks=3`. The old
  `-lt 4` bailed out before incrementing on every real generation, so the guarded
  USB reset could never arm.

**On-unit measurement traps** (all three silently produced empty output and cost a
capture each):
- QNX `grep` has no `\|` alternation — use one pattern per call.
- QNX has no `tail` on the wrapper PATH — use `grep`/`cat`/`sed -n`.
- `netstat -in` truncates the interface name to the column width: the row reads
  `carpl`, never `carplay0`. Match `^carpl`.
- `/ramdisk/pps/device/usb-1.0.1` is the HU's **own** device-mode gadget
  ("AUDI AG / iAP2 NCM Accessory", VID `0x1a96`) — in wired CarPlay the iPhone is
  the USB host and the HU is the device, so there is no Apple VID `0x05ac` node in
  the tree at all. A rapid re-read of a PPS object can also return no fields, so
  sampling it right after dio dies is unreliable.

## Other open (tracked elsewhere)
- On-unit lifecycle validation (5 fault-replay tests + drive) — `docs/reference/CARPLAY_SESSION_LIFECYCLE_AUDIT.md`.
- Java cleanup groups 1–3 + KOMO reflection (group B) — `docs/reference/JAVA_PRODUCTION_CLEANUP_AUDIT.md`.
- `routeState==5` accept-all-maneuvers logic not implemented (MHI3 has it) — decide if needed.
