# CarPlay USB startup-recovery diagnosis

> **Correction (2026-09-04):** the state-12 comparison in `verifyPortReset` is
> real but is not the controlling CarPlay branch: CarPlay is app ID 0 and uses
> special new-connector cleanup. That cleanup can itself issue the terminal
> `start_stack::none` after retries are exhausted. See
> [`SI_RAPID_REPLUG_RACE_RE.md`](SI_RAPID_REPLUG_RACE_RE.md).

> Deployment status (2026-08-04): the experimental shell-driven automatic reset
> was rolled back after the targeted reset failed to recover the wedged phone/OTG
> path. Production scripts use the simpler renderer supervisor from `afa498d`.
> This document remains diagnostic evidence, not the deployed recovery design.

## Verdict

The current reconnect loop happens before RTSP, libairplay, or the altScreen
hook can participate. The phone sees the HU's `iAP2 NCM Accessory`, but does not
enable both USB interfaces. `io-otg-cinemo` times out, and
`smartphone_integrator` classifies the failure as `TIMEOUT_STARTUP`.

The stock recovery policy does not request a physical media-connector port
reset for `TIMEOUT_STARTUP`. It only stops and starts the OTG device stack. Once
the phone/bridge is stuck in this state, repeating `start_stack none` followed by
`start_stack device,2` cannot recover it.

Removing `ALTSCREEN_CLUSTER_URL` cannot affect this failure: no control SETUP,
RTSP request, stream 111 setup, or renderer connection occurs in the failing
generations.

## On-unit evidence

The live PPS objects during the failure show:

- `/ramdisk/pps/device/usb-1.0.1`: `product::iAP2 NCM Accessory`,
  `drivers_matched::2`, but `drivers_running::1`;
- `/ramdisk/pps/device/usb-0.0.2`: the new media connector `0424:2530`, with
  `upstream_port::3` and `upstream_device_address::1`;
- `/dev/media-con-ctrl` exists and is writable.

The captured slog repeats the same sequence. Representative entries are in
`/tmp/diag_slog_latest.txt`:

- line 9405: `appState=TIMEOUT_STARTUP`;
- lines 9406-9420: dio is killed and the OTG stack is cycled;
- no `ResetPort`/`onJob_resetPort` entry appears anywhere in the capture.

A fresh 2026-08-04 capture repeated this for every generation: device stack
start, `io-otg-cinemo ERROR: The interface was not enabled in time`,
`TIMEOUT_STARTUP`, `start_stack none`, and `start_stack device,2`. The hook log
contained no RTSP/control-SETUP activity.

## Why stock never performs the required reset

The stock P5087 `smartphone_integrator` binary is:

`MU1316-appimg/eso/bin/apps/smartphone_integrator`

SHA-256:
`a25dc981e5591cb3c6b09f00fd95de23803dd3daa0f562e7e8d60b6d0e1b0d02`.

Relevant ARM addresses in that image:

1. `sub_172D5C` at `0x172D5C` maps app-state 12 to `ERROR_RUNTIME` and
   app-state 14 to `TIMEOUT_STARTUP` (`0x172E28` and `0x172E10`).
2. `CSIStateRunning::onEvent_appState` at `0x186B00` handles reported states
   7, 12, and 14 together at `0x18752C`. It calls `verifyPortReset` at
   `0x18753C`, then passes the resulting byte at session offset `+0x560` to
   connector cleanup at `0x187554`.
3. `verifyPortReset` is `sub_184030` at `0x184030`. Its only assignment to the
   event's `portResetNeeded` byte is at `0x184124`:
   `portResetNeeded = (reportedAppState == 12)`. Therefore state 14 can never
   request a port reset through this path.
4. `CMediaConnectorHandler::cleanup` is `sub_17FC48` at `0x17FC48`. It copies
   the third argument to `R10` at `0x17FC64`; only a nonzero value takes the
   reset branch at `0x17FDD8-0x17FEEC`. A zero value merely completes cleanup
   and allows the OTG stack restart.
5. The real stock reset job exists. `onJob_resetPort` starts at `0x194294` and
   calls `sub_192D10` at `0x194534`. `sub_192D10` calls the media-connector
   controller at `0x192D34`.
6. `sub_19B984` at `0x19B984` builds the exact controller command
   `reset port <port> <resetTime> <device>` and writes it through
   `sub_19B1BC`. `sub_19B168` at `0x19B168` opens `/dev/media-con-ctrl`.

Thus the hardware-reset implementation is present and usable; the stock state
policy simply does not select it for the failure we are observing.

## Contributing local timing change

The extracted stock child config uses `restartDelay: 2000` and
`portResetTime: 250` at `smartphone_integrator.json:161-162`. Our child briefly
used `restartDelay: 250`; `carplay_child.json:20` has now been restored to the
stock 2,000 ms value.

The shorter delay is not the primary cause: cycles with a five-second gap also
fail with the same unenabled-interface error. It does, however, reduce settling
time during a rapid reconnect and makes repeated soft retries less robust. The
first reliability correction should restore the stock 2,000 ms delay.

## Safe recovery design

Use the stock media-connector controller, not the GPIO power scripts and not a
second process that independently restarts `dio_manager`.

For the current new connector, the live topology resolves to:

- port: `3`;
- upstream hub device: `1`;
- reset time: the configured `250` ms.

The equivalent controller payload is:

```text
reset port 3 250 1
```

This command has **not** been sent during this investigation. On QNX PD ksh,
`printf` is unavailable but `print` is a builtin, so the direct diagnostic form
would be:

```sh
print -n 'reset port 3 250 1' > /dev/media-con-ctrl
```

Production recovery now uses PID-scoped control-SETUP markers rather than parsing
`sloginfo`. A dio generation is counted only when it lives for at least eight
seconds, never creates its marker, and the live PPS object still reports
`drivers_matched=2` with `drivers_running=1`. After three consecutive matches,
the monitor queues (but does not execute) the reset. The next SI-owned wrapper
consumes that request before starting renderers or exec'ing dio, writes the
command above, waits one second, and then continues the normal launch. A latch
prevents another hardware reset until a successful control SETUP, confirmed
non-stuck/detached PPS state, or reboot.

Do not use `/etc/scripts/disable_usb_pwr_0.sh` as the first recovery method: it
power-cycles the connector GPIO rather than the one bridge port and therefore
has a wider blast radius.

## Ranked corrections

1. Keep `restartDelay` at the restored stock 2,000 ms value.
2. Add one guarded `/dev/media-con-ctrl` reset after repeated startup timeouts.
3. Do not change the altScreen hook, URL, renderer, Java process, or libairplay
   for this failure; none is reached before the USB interface timeout.
