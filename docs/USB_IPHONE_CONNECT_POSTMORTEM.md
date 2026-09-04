# One quick replug, and CarPlay never came back — what was established and what was not

> **Correction (2026-09-04):** later offline SI analysis shows that the missing
> `io-usb-dcd` is a terminal state SI can deliberately create after exhausting
> CarPlay setup retries; it is not proof that the DCD crashed first. The local
> startup/launcher guard described below has been rolled back. See
> [`reference/SI_RAPID_REPLUG_RACE_RE.md`](reference/SI_RAPID_REPLUG_RACE_RE.md).

Session of 2026-09-03. Written because most of what was learned here is negative knowledge,
the kind that is expensive to re-derive and easy to re-derive wrongly. Four of my own
hypotheses are recorded as refuted, with the data that refuted them.

## The symptom

CarPlay was working. The phone was unplugged and replugged once, quickly. It never
reconnected — through roughly 500 retry cycles over 45 minutes, until a reboot.

## What is measured, and how

| observation | how it was read |
|---|---|
| `carplay0` UP but **without the `RUNNING` flag** | `ifconfig carplay0` — compare `ecm0`, which has it. No `RUNNING` = no carrier, i.e. a link/stack problem, not our software |
| `dio_manager` killed at `appState=TIMEOUT_STARTUP`, ~500 dumps | `smartphone_integrator_error_*` in slog |
| the iPhone **was** enumerated: `05ac:12a8`, four configurations including `iPod USB Interface` | `usb` — so the cable, the port and the phone are fine |
| the hook log stops right after `Captured stock ICinemoIAP`, with no `NmeTransport::Recv` | iAP2 never starts. In a healthy boot, `Recv [FF 55 02 00]` follows within 20 ms, then Identify and Auth |
| **`/dev/io-usb/otg_dev` missing**; `io-usb-dcd` not running | `ls`. Only the host stack `/dev/io-usb/io-usb` was present |

The chain: no `io-usb-dcd` → no `otg_dev` → `startncm.sh` mounts NCM on a node that does not
exist → `carplay0` has no carrier → iAP2 never starts → dio misses its startup budget → SI
kills and relaunches it, forever.

## Why it never recovers on its own

`/etc/usblauncher.lua`, the media-connector block for our hub (`0x0424:0x2530`):

```lua
bLsResult = os.execute("ls /tmp/usblauncherFlag_mediaconnector_*")
os.execute("touch /tmp/usblauncherFlag_mediaconnector_2530_" .. obj.devno)
if bLsResult then
    return 'echo "... NoOp because either 0x4040 or 0x2530 already detected"'
else
    return "usblauncher ... -c /etc/usblauncher_otg.lua ... -n /dev/io-usb/otg"
end
```

The launcher is started **once**, gated on a flag in `/tmp`. The flag is removed only in the
*removed* handler — that is, when the **hub** is unplugged. The hub is soldered in. `/tmp` is
a RAM disk. So the state survives every replug and clears only on reboot, which is exactly
what was observed.

## The startup guard that was tested, then rolled back

The platform already carries the recovery, so the guard just calls it —
`deploy/smartphone_integrator/carplay_startup.sh`:

```sh
if ! ls /dev/io-usb/otg_dev >/dev/null 2>&1; then
    /etc/scripts/start_otg_stack_device_mode.sh      # start_stack::device,2 -> PPS usb_ctrl1
fi
```

Verified live: with `otg_dev` missing, this brought `io-usb-dcd` back and recreated the node.

It is in **startup**, not `carplay_cleanup.sh`, because startup runs on every launch including
each retry of the failure loop, so the system heals itself on the next attempt. Cleanup may
not run at all when SI kills dio on a startup timeout.

## Mechanism notes worth keeping

* The two OTG stacks are **mutually exclusive** — one controller, one mode:
  `start_otg_stack_host_mode.sh` → `start_stack::host` → `/dev/io-usb/otg`;
  `start_otg_stack_device_mode.sh` → `start_stack::device,2` → `/dev/io-usb/otg_dev`.
* `usblauncher` does **not** create a stack. It attaches to one. Restarting it against a dead
  stack achieves nothing — the stack comes from `io-usb-dcd` (`start_usb_device_stack.sh`).
* The physical port is switched between the main hub and the OTG hub through
  `/dev/media-con-ctrl`: `switch port <1|2> <1=OTG|0=Main> 1`. `switchMediaConnectorPort.sh`
  is a diagnostic wrapper; the real switching is done by a binary. `startup.sh` does it at
  boot — gated on the same `/tmp` flag.

## The missing piece — found by comparing with MH2P

Restoring the device stack did not, by itself, bring CarPlay back: `otg_dev` was present,
`dio_manager` started, and iAP2 still never began. Comparing the USB chain against
`Firmwares/HU/MH2p_US_LB636_P2712` explains why.

`/etc/usblauncher_otg.lua` declares:

```lua
Device_Stack = {
    cmd  = '/etc/scripts/start_usb_device_stack.sh';
    path = '/dev/io-usb/otg_dev';
    connect_poll = 100;   -- "improve CarPlay connection times"
    descriptors = {usbdesc_carlife, usbdesc_carplay, rndis};
}
```

**The OTG `usblauncher` owns the device stack.** It starts `io-usb-dcd` itself, chooses the
gadget descriptor, and mounts NCM via `startncm.sh` when the gadget appears. Poking the PPS
object directly puts the node back *behind the owner's back*, leaving the launcher holding
stale state — so the node returns and nothing else does.

The guard now restarts that launcher after restoring the node, so it redoes its own
bring-up. Its argv comes from `pidin`, not `/proc`: on QNX `/proc/<pid>/cmdline` is **empty**
for this process, which is why an earlier version scanned `/proc`, matched nothing, and
silently did nothing.

Two further differences worth knowing, both showing MH2P as the later design:

| | MHI2Q (ours) | MH2P |
|---|---|---|
| OTG launcher binds to | `-n /dev/io-usb/otg` | `-n /dev/io-usb/otg_dev` |
| port mode at connector insertion | not set | explicit `start_usblauncher_otg_port_device_mode.sh` |
| connector IDs handled | `0x4040`, `0x2530` | `0x4040`, `0x2530`, `0x4940`, via one shared function |

MH2P setting the port mode on insertion is what our unit lacks: MHI2Q depends on that being
done elsewhere, and when it was not redone, nothing else would.

Still unproven: whether node + launcher restart together are sufficient. That needs the
failure to recur, which cannot be summoned on demand.

## Where I was wrong

Recorded deliberately — each of these looked convincing and cost time.

1. **"Memory is exhausted."** `FreeMem` was ~100 MB of 2 GB and `memmonit` warned constantly.
   But sampled over time it was *stable*, oscillating 97–105 MB. My "116 → 94 declining" was
   sampling noise read as a trend.
2. **"tailscaled leaked it."** It was the largest process, but its size never moved across
   samples. (Separately true and later fixed: it really did hold ~516 MB — see
   `qnx_go_map_lazy` — but that is a different problem and not this failure's cause.)
3. **"The CPU is pegged."** The heartbeat line reads `CPU=81…100%`, but sampling
   `pidin times` twice showed the busiest process at **3% of one core**. Whatever that
   heartbeat field means, it is not what I assumed.
4. **"`usblauncher.lua` was modified."** A `.orig` sat beside it, which looked damning. The
   only difference was `transmit=256,receive=256` on the **LTE modem's** ecmplus line.

Two coding errors made the first version of the guard silently do nothing:

* `/proc/<pid>/cmdline` is **empty** for `usblauncher`, though `pidin -F "%a %A"` shows its
  full command line. The pid scan therefore never matched.
* `usblauncher` is not on `PATH` (it lives in `/mnt/app/armle/bin/`), so the relaunch could
  not have run either.

And one claim I asserted without testing: that `[ -e ]` returns true on QNX resource-manager
paths that `ls` cannot open. **It does not.** All of `-e -f -c -r -x -s -d` correctly return
false for a missing `/dev/io-usb/otg`. `-x` is the one that misleads — a character device is
not executable, so it returns false on a *live* node too.

Finally, an action that made things worse: switching media-connector port 1 to the OTG hub
moved the phone off the main host controller, after which it stopped enumerating at all.
Switching back did not immediately restore it. The command was reversible; the state was not.

## Reading the logs here

`sloginfo`'s ring buffer is flushed in about **two minutes** by a
`devp-iso-mmx[isoTX2]` / videoCore open-close storm — 59% of all lines. The originating event
is therefore usually gone by the time anyone looks. Absence of evidence in slog proves
nothing; find the moment in `/tmp/carplay_hook.log` instead, which is ours and much quieter.

Related: `si_stack_restart_kills_carplay0` and `usb_device_stack_replug_wedge` in memory.
