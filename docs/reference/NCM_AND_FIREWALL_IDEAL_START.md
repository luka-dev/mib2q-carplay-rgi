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
PF**. This deletes:

- `cp_enable_altscreen_firewall` from `carplay_startup.sh` (pre-exec work in dio's startup budget);
- the 30 s monitor PF guard that runs 2x `pfctl -sr` per tick (~5 s/tick — the churn we measured).

Your instinct — "just append the port and stop the re-checks" — is the correct design. The
re-checks only existed because we were fighting a boot-time reload we don't own; edit the source
and the fight disappears.

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

## What NOT to do

Do not chase a faster `startncm.sh` or a parallel mount — the mount is already a one-shot the moment
the class appears, and it is not the bottleneck. The bottleneck is teardown/restart. Fix the churn.

## Deploy (nothing here restarts anything)

    ./deploy/smartphone_integrator/install_persistent_pf.sh    # on the unit, as root; idempotent

Takes effect on next boot (ramdisk /etc rebuild) or immediately if followed by a manual
`pfctl -ef /etc/pf.conf`. Then the PF code can be removed from the wrapper.
