# mcd — detector-thread CPU coalescing

Stock `mcd` (the QNX media mount/content daemon, `/mnt/app/armle/usr/sbin/mcd`)
burns up to ~35 % of one Krait core in bursts, even with no media inserted. This
folder holds the root-cause analysis, the reference binaries, and a reproducible
one-site binary patch that removes the burst.

## Files

| file | sha256 | note |
|------|--------|------|
| `mcd.stock`       | `f11d13ff…441dd593` | stock daemon, imagebase 0x100000, not stripped |
| `mcd.custom`      | `e18f3055…5c5387ee` | patched daemon (built by `binpatch`, IDA-verified) |
| `mcd.conf.stock`  | `c653517d…4272092`  | stock config, 13 active device-watch sections |
| `mcd.conf.custom` | `d0a33ae2…68a8033`  | optional: detector priority lowered 11,10 -> 11,3 (device list unchanged) |

All four are byte-identical across the US (P5087/MU1316), ER (P5092/MU1329) and
CN (K1004/MU1003) trains — verified by carving each from the firmware `app.img`
and `system.img`. One patch covers all three regions.

## Root cause

`mcd` is event-driven, not a busy-poll. The cost comes from an **unfiltered
fan-out**:

1. Any namespace change fires `procmgr_event_notify(PROCMGR_EVENT_PATHSPACE)`.
2. The `multiplexor` thread broadcasts that one event to **every** watcher
   thread with `pthread_kill(signal 58)` — there is no per-path filtering.
3. Each of the 13 `[section]`s in `mcd.conf` runs its own `PATH_MEDIA_PROCMGR`
   detector thread. Woken, each does a full `opendir`/`readdir64`/`fnmatch`
   directory scan.

So one irrelevant pathspace event = 13 directory scans. Under namespace churn
(USB, CarPlay bring-up, mounts) the scans stack up into the 35 % burst.

The detector loop (`PATH_MEDIA_PROCMGR`, 0x1075ac) is:

```c
while (1) {
    dir = opendir(path);
    while ((e = readdir64(dir)))
        if (!fnmatch(pattern, e->d_name, FNM_PERIOD)) names(...);
    names(..., 0, 0, 0);
    sigwaitinfo(&sig58, NULL);   // block until the next broadcast
}
```

Signal 58 is masked (`sigfillset`+`sigprocmask`), so it is only ever consumed
through this `sigwaitinfo`.

## The patch (one site, 5 bytes)

Replace the blocking `sigwaitinfo` with a fixed `delay(200)` poll. The daemon
already imports `delay`, so no new code is needed:

```
0x107730  ADD R0, SP, #0x218   e28d0f86  ->  MOV R0, #200   e3a000c8
0x107734  MOV R1, #0           e3a01000      (unchanged; delay ignores R1)
0x107738  BL  sigwaitinfo      ebffeb73  ->  BL  delay       ebffebf7
```

Result:

```c
while (1) { opendir/readdir/fnmatch scan …; delay(200, 0); }
```

- Hard cap of ~5 scans/sec/thread → the burst cannot form.
- The pathspace pulse just stays pending harmlessly; it is never waited on.
- Media detection still works, now with ≤200 ms latency (invisible in use).
- `delay()` is a cancellation point, so the shutdown `pthread_cancel` path is
  unaffected.

Verified in IDA: the loop body is otherwise untouched and now ends in
`delay(200, 0)` where `sigwaitinfo` was.

## Build / verify

The patch is registered in the shared, signature-based patcher
`deploy/binpatch/binpatch.py` (same tool used for the SI patch; runs on-device
under M.I.B. MicroPython and on a host under CPython). It locates the site by
surrounding code, not by offset, so a train whose code differs is left alone.

```sh
python3 deploy/binpatch/binpatch.py mcd mcd.stock /tmp/mcd.custom
sha256sum /tmp/mcd.custom     # -> e18f3055…5c5387ee
```

Exit 0 = patched, 3 = already patched (nothing written), 1 = no unique match.

## Config lever: detector priority (`mcd.conf.custom`)

Shipped alongside the binary patch. It lowers every active section's
`Priority = 11,10` to `11,3`, so whatever scanning remains runs on slack CPU
instead of near the media priority band. The device list is left exactly as
stock — nothing is pruned.

The installer applies it in place (`patch_mcd_conf` in custom.sh) with a single
`sed` that only rewrites the active `Priority` lines; it is idempotent and
train-agnostic. `mcd.conf.custom` is the reference result of that transform on
`mcd.conf.stock`.

**Not done: pruning.** Dropping dead sections would cut detector threads
(e.g. the two `cd` sections — this unit has no optical drive, and `/fs/online`,
`ipod*`, `pfs*`, `upnp*`, `/mnt/media`), but that needs a live check of which
`/dev` and `/fs` nodes the unit actually creates, or media detection breaks.
Left for a hardware audit on the car.

## Deploy (owner reboot required; not done here)

Binary (writable app partition):

```sh
cp /mnt/app/armle/usr/sbin/mcd /mnt/app/armle/usr/sbin/mcd.stock   # backup once
cp mcd.custom /mnt/app/armle/usr/sbin/mcd
chmod 755 /mnt/app/armle/usr/sbin/mcd
```

Config (read-only system partition):

```sh
mount -uw /mnt/system
cp /mnt/system/etc/mcd.conf /mnt/system/etc/mcd.conf.stock          # backup once
sed 's/^\(Priority[[:space:]]*=[[:space:]]*11\),10$/\1,3/' \
    /mnt/system/etc/mcd.conf.stock > /mnt/system/etc/mcd.conf
mount -ur /mnt/system
```

`mcd` is launched once at boot (`/etc/boot/startup.sh`), so both changes take
effect on the next boot. Roll back by restoring the `.stock` copies.
