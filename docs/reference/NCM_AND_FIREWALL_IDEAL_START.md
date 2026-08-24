# Ideal NCM start + a firewall that always has the port

Answers three questions, all grounded in `MHI2Q_US_AUG22_P5087_MU1316` boot analysis.

## 1. Firewall: put the port in the SOURCE, drop the runtime dance

**Why our live edits reset every reboot.** `startup.sh:move_etc_to_ramdisk()`:

    cp -r /mnt/system/etc /ramdisk/etc ; ln -Ps /ramdisk/etc /etc

`/etc` is a RAM copy of `/mnt/system/etc`, rebuilt every boot. A `pfctl` edit to the live
`/etc/pf.conf` is thrown away, and boot's `start_network` (`startup.sh:1061`,
`pfctl -ef /etc/pf.conf`) reloads the stock ruleset — which has `6030` but not `6031`.

**The stock rule already passes 6030** (`pf.conf:201`):

    pass in quick on carplay0 proto tcp from any to any port { ..., 6030, ... } allow-opts

**Verified against fstab, not assumed.** `ifs2/ifs_startup/ifs_coreservices2/proc/boot/fstab`:

    /dev/mnand0t177   /mnt/app       qnx6   ro
    /dev/fs0p1        /mnt/system    qnx6   ro
    /dev/fs0p2        /mnt/persist   qnx6   rw

`/mnt/system` is a real qnx6 flash partition (`/dev/fs0p1`), mounted **ro by default**, not an
overlay and not a tmpfs. `pf.conf` exists in exactly one place in the whole image
(`/mnt/system/etc/pf.conf`); `ifs2` carries no `pf.conf` or `etc` that could shadow it. So editing
it is persistent across reboots — the same partition and persistence model as our existing
`smartphone_integrator.json` deploy. It needs `mount -uw /mnt/system` first (the ro default), which
the script does. The one thing that reverts it is an OTA rewriting `fs0p1` — identical caveat to
every other `/mnt/system` and `/mnt/app` edit we ship.

**The source is writable.** We already deploy `smartphone_integrator.json` to
`/mnt/system/etc/eso/production/`, so `/mnt/system/etc/pf.conf` is on the same writable eMMC.

**Fix: widen `6030` -> `6030:6031` in `/mnt/system/etc/pf.conf`, once.**
`install_persistent_pf.sh` does exactly that, idempotently, with a `.stock` backup. Then the
ramdisk copy carries 6031, `start_network` loads the correct ruleset, and **CarPlay never touches
PF**. All firewall logic is now **gone** from the wrapper (146 lines deleted): the
`cp_enable_altscreen_firewall` startup call, the 30 s monitor guard (2x `pfctl -sr` per tick,
~5 s/tick — the churn we measured), and the `cp_active_pf_source` / `cp_altscreen_firewall_ready`
helpers. The port is a deploy prerequisite, exactly like the hook `.so` or the child JSON — if
`install_persistent_pf.sh` was not run, altScreen 6031 is simply blocked, the same failure mode as
a missing renderer binary. No runtime fallback, no re-checks.

Your instinct — "just append the port and stop the re-checks" — is the correct design. The
re-checks only existed because we were fighting a boot-time reload we don't own; edit the source
and the fight disappears.

## Why our earlier pf edits rolled back (they were a different thing)

We *did* try before, and it rolled back — for a reason that the source edit specifically fixes.
Everything shipped so far (`cp_enable_altscreen_firewall`) only ever touched the **live in-kernel
ruleset**:

    CP_PF_RUNTIME=/tmp/pf.carplay_alt.conf      # generated in /tmp (ramdisk)
    sed 's/ 6030,/ 6030:6031,/' <source> > $CP_PF_RUNTIME
    pfctl -f /tmp/pf.carplay_alt.conf           # load into the live kernel ruleset

It never wrote `/mnt/system/etc/pf.conf` (git history confirms: the only mention of that path is
the persistent script). The live ruleset is volatile:

- any `pfctl -ef /etc/pf.conf` wipes it — `start_network` does exactly that at boot, so it is gone
  after a reboot;
- `start_network` re-running mid-boot clobbered it too, which is why the 30 s monitor guard existed
  to fight the reload.

The source edit is a different operation: it changes the file the ramdisk `/etc` is rebuilt from,
so there is nothing to wipe and nothing to fight. Do not confuse the two — a live `pfctl -f` will
always roll back; the source edit will not.

## 2. Move it out of CarPlay: yes

The PF port is a boot-time property of the platform, not a per-session CarPlay concern. It belongs
in the boot source (`pf.conf`), loaded once by `start_network`. `carplay_startup.sh` should assume
`carplay0` already admits 6031 and do zero PF work. This is both cleaner and takes real latency off
dio's startup path.

## 3. "100% ideal" / parallel NCM start

NCM is not ours to parallelize, and that is the point:

- `usblauncher` (OTG) owns it. When the composite gadget presents the NCM interface class
  (`0x02/0x0d/0x00`), usblauncher's rule fires `startncm.sh`, which mounts `devnp-usbdnet.so` and
  brings up `carplay0`. That already happens independently and as early as the class appears —
  there is no serialization on our side to remove.
- The only thing that breaks it is **OTG churn**: a dio generation death makes
  `smartphone_integrator` tear the stack down (`stopncm.sh` -> `ifconfig carplay0 destroy`) and
  restart it, and if the class rule doesn't re-match cleanly the interface is simply absent
  (`USB eligible: N (No Network Interface)` — see `CONNECT_FAILURE_ROOT_CAUSE.md`).

So "ideal NCM start" is not a faster start — it is **not restarting it**. Two levers, in order:

1. **Stop the ~15 s dio generation death** (the `STOCK_BISECT.md` test). That is the actual churn
   source. If dio stops dying, NCM comes up once and stays.
2. **Take front-loaded work off `carplay_startup.sh`** so dio reaches control SETUP inside SI's 10 s
   `startupTimeout` instead of racing it: move the PF work out (item 1 above), and start the two
   renderers *after* `exec` / off the critical path rather than before it with a `sleep 1` between.

Even stock "tupilo" (was slightly slow) because `start_network &` and `start_hmi &` race at boot
(`startup.sh:1555-1561`). The persistent-pf edit also helps there: the ruleset is correct before
either finishes, so there is no window where the port is missing.

## The startncm.sh en-dash: cosmetic, NOT a defect (retracted)

`/etc/scripts/startncm.sh` mounts `devnp-usbdnet.so` then runs:

    if if_up -p ${IF_NAME} <U+2013>s 2 <U+2013>r 1

The `s`/`r` flags really are U+2013 en-dashes, not ASCII hyphens — verified byte-identical across
two independent extractions (same md5 `3135298af5f4ade6df94eff15b486fd9`). An earlier version of
this note called that a defect that makes NCM flaky. **That was wrong.** Reading `if_up`'s own usage
(`ifs2/ifs_coreservices3/bin/if_up`):

    if_up [-ap] [-r retries] [-s seconds] interface...
      -p  wait until present;  -a  wait until ALL present;  default: a SINGLE interface present

`if_up` uses POSIX `getopt`, which stops option parsing at the first non-option operand
(`carplay0`). So `<U+2013>s 2 <U+2013>r 1` become extra interface operands, not flags. But `-a` is
not set, so `if_up` succeeds as soon as **any one** listed interface is present — and `carplay0` was
just mounted, so it is present on the first walk. The en-dash tokens are harmless phantom operands;
the only thing lost is the intended retry tuning, which never matters because the interface is
already there.

Empirical proof: this exact script ships in every production MHI2Q and stock CarPlay works. The
en-dash cannot be breaking NCM. Likely origin: an editor/wiki auto-correcting hyphens to typographic
dashes (Word/Confluence smart-dashes, or a paste from a PDF spec) when the script was authored;
never caught because it does nothing.

No startncm replacement is shipped — it would have been a deploy step and a write to `/mnt/system`
for zero behavioural change.

## What NOT to do## What NOT to do

Do not chase a faster `startncm.sh` or a parallel mount — the mount is already a one-shot the moment
the class appears, and it is not the bottleneck. The bottleneck is teardown/restart. Fix the churn.

## Deploy (nothing here restarts anything)

    ./deploy/smartphone_integrator/install_persistent_pf.sh    # on the unit, as root; idempotent

Takes effect on next boot (ramdisk /etc rebuild) or immediately if followed by a manual
`pfctl -ef /etc/pf.conf`. Then the PF code can be removed from the wrapper.

---

# Stock connect path, re-read from the firmware (2026-08-24)

Source: `Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted`. Everything below was read out of the
stock files or decompiled, not inferred.

## The chain, end to end

1. `startup.sh:start_usblauncher()` launches usblauncher **instance 0** on the host port
   (`-n /dev/io-usb/io-usb -S 0`). Verbosity defaults to **`-vv`** — the flag files only raise it
   to `-vvvvvv`; there is no quiet setting.
2. `usblauncher.lua` detects the MediaConnector hub (`0x0424:0x4040` / `0x0424:0x2530`) and starts
   usblauncher **instance 1** on the OTG port with `-w 10000`.
3. `smartphone_integrator` drives the OTG stack over PPS `/ramdisk/pps/device/usb_ctrl1`
   (`start_stack::host` / `device,2` / `none`), per `smartphone_integrator.json` → `usb.launcher`.
4. Phone plugged in: seen first as an Apple device in **host** mode. `usblauncher_otg.lua` matches
   `product(0x05AC, 0x1200, 0x12FF)` and runs `driver"echo dummy iOS driver"` — a deliberate no-op
   whose only job is to let usblauncher issue `role_swap`.
5. Role reversal → we become the **device**: composite gadget, iAP2 `0xFF/0xF0/0x00` and
   NCM `0x02/0x0d/0x00`.
6. The phone (now host) enumerates us. `usblauncher_otg.lua` fires per interface:
   - iAP2 → `io-otg-cinemo -s /dev/io-usb/otg_dev -lc 212 -v -t 8`
   - NCM  → `startncm.sh /dev/io-usb/otg_dev carplay io-pkt`
7. `startncm.sh` mounts `devnp-usbdnet.so` (`protocol=ncm, npkt=10, mac=020022446688`), then
   `if_up -p carplay0`, then `ifconfig carplay0 up`.
8. The IPv6 link-local `fe80::22ff:fe44:6688%carplay0` comes up — confirmed by the stock
   `CP_get_ip_address.sh`, which documents exactly that address. CarPlay rides link-local IPv6.

## Three suspects cleared (do not re-investigate)

- **`-w 10000` on the second usblauncher is NOT a 10 s delay.** Decompiled `start_usb_ctrl_thread`
  (`usblauncher` @ `0x106b60`): it polls `eaccess()` on the ctrl path every 50 ms and **breaks out
  as soon as the path is absent** — it waits for a previous instance to release `usb_ctrl1`.
  On a clean boot it exits on the first iteration. `-w` is a timeout, not a sleep.
- **The en-dashes in stock `startncm.sh` (`if_up -p ${IF_NAME} –s 2 –r 1`) cost nothing.**
  Decompiled `if_up` main: `v7 = argc - optind; if (!v5) v7 = 1;` — without `-a` it needs
  **one** matching interface out of the operand list and `exit(0)`s on the first hit. The mangled
  `–s 2 –r 1` just become extra operands that are never matched. Defaults (`retries=5`,
  `seconds=1`) only apply while `carplay0` does not exist yet. `-p` additionally skips the
  IPv4-address check, so it returns on interface presence alone.
- **`connect_poll = 100`** in `Device_Stack` is already a stock optimisation — the comment says
  "poll 100ms in order to improve CarPlay connection times".

## Where the time actually goes

### IPv6 DAD — ~1 s, on every connect including the first

`startup.sh` sets **no** `net.inet6.*` sysctl anywhere, so `ip6_dad_count` keeps its default of 1.
Decompiled `nd6_dad_start` (`io-pkt-v6-hc`):

```c
if ( (flags & 1) != 0 || !ip6_dad_count ) {
    ifa->ia6_flags &= ~IN6_IFF_TENTATIVE;   /* DAD skipped, address usable immediately */
    return;
}
...
callout_reset(dp, hz * ND_IFINFO(ifp)->retrans / 1000, nd6_dad_timer, ...);
```

So with `dad_count=1` the link-local stays **tentative for one RETRANS_TIMER (~1 s)**, and a
tentative address cannot be used as a source. With `dad_count=0` the tentative bit is cleared
synchronously and the address is usable at once.

This sits exactly where the phone-side sysdiagnose shows `Session: No Remote IP` for ~2 s before
`Starting Accessory Authentication` (which itself takes only ~330 ms). **Strong hypothesis, not a
measurement**: the code proves DAD costs ~1 s and sits in this path; it does not prove those are
the same 2 s.

`in6if_do_dad()` decompiles to a check on `IFF_LOOPBACK`, `if_type != 242`, and `IFF_UP|IFF_RUNNING`
— there is **no** per-interface opt-out, and `ndp -i` can only display `retrans`, not set it.
So the only lever is the global `net.inet6.ip6.dad_count`.

### Role-reversal power delays — ~2 s, first connect only

`smartphone_integrator.json`: `powerDelayTimeHostMode: 1000` + `powerDelayTimeDeviceMode: 1000`,
plus `deviceRemovalDelay: 1250` and `restartOtgStackDelayTime: 5000`. These bracket the host↔device
switch, which is precisely the first-connect path. SI ships no function symbols (only vtable names
in rodata), so the exact call sites were not traced — the values are config, though, and testable
by A/B.

### Teardown — 1 s per reconnect

`stopncm.sh` opens with `sleep 1`, commented "workaround race condition between removal/unmount".
Not first-connect, and not to be touched without understanding the race.

## Ranked levers

**Tier 1 — does not touch USB at all:**
1. `sysctl -w net.inet6.ip6.dad_count=0` before `start_network`. Saves ~1 s on every connect
   including the first. Risk is global (wlan/ecm lose duplicate detection); on `carplay0`, a
   point-to-point USB link, a duplicate is impossible by construction.
2. Drop usblauncher's default `-vv`. Zero risk, unknown gain, costs only diagnostics.

**Tier 2 — touches USB, needs A/B measurement and a rollback:**
3. `powerDelayTimeHostMode` / `powerDelayTimeDeviceMode` 1000 → 500. Biggest first-connect target
   (~2 s), but it governs port power settling — undershooting can break enumeration of slow hubs
   and devices. Config-only, so trivially revertible.
4. `connect_poll` 100 → 50. Small gain, small risk.

**Tier 3 — the real one:**
5. Stop paying the whole sequence repeatedly. The first connect pays enumeration + role reversal +
   auth + addressing once by definition; our ~15 s dio generation death makes us re-pay it. See
   `STOCK_BISECT.md`. Nothing above matters as much as not re-running the whole chain.
