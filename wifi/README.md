# Wi-Fi driver — RX reorder + timer hardening

The stock Marvell 88W8787 driver (`devnp-mrvl_wlan-sdiorm.so`,
`/mnt/app/armle/lib/dll/`) holds a received packet behind a missing sequence
number with no bounded release, so a single lost frame can stall the block-ack
reorder window until the window itself wraps. This folder holds the reference
binaries and the reproducible patch that bounds that wait.

The full reverse-engineering writeup, the host-side BA-window analysis and the
on-unit runtime facts live in `docs/wireless-carplay/WIFI_DRIVER_RE.md`; this
README is the deploy/patch summary.

## Files

| file | sha256 | note |
|------|--------|------|
| `devnp-mrvl_wlan-sdiorm.so.stock`   | `d7fbf49b…1f394a` | stock driver, 418361 bytes |
| `devnp-mrvl_wlan-sdiorm.so.patched` | `ca529b2b…64b2e7` | patched driver, same size, 40 bytes changed |

## The patch (3 fragments, 40 bytes)

Applied by the shared signature patcher `deploy/binpatch/binpatch.py` (entry
`wifi`), which finds each site by its surrounding code, not by offset:

1. **RX-reorder expiry** (`@0x47368`, 16 B) — a 50 ms deadline releases the
   packets held behind a hole so a lost sequence number cannot stall the window.
2. **Wrapped-window fix** (`@0x46d68`, 28 B) — correct release across the
   sequence-number wrap.
3. **Timer harden** (`@0x46ed8`, 4 B) — the one-shot reorder timer finishes
   cleanly.

Activation is a reboot: `io-pkt` loads the driver from disk at boot.

## Regions

- **P5087/MU1316 and P5092/MU1329**: the whole driver is byte-identical
  (`d7fbf49b`). P5087 patched equals the shipped `…so.patched` (`ca529b2b`).
- **K1004/MU1003**: a different overall driver build, but the three reorder
  sites are byte-identical in context, so the signature patcher applies the same
  40-byte fix there too (verified = exactly 40 bytes; result sha is CN-specific
  because the rest of the binary differs). Not yet run on CN hardware.
- Any build lacking the sites is left stock (binpatch returns "no match").

## Build / verify

Two reproducible paths, kept in sync by `scripts/test_binpatch.sh`:

```sh
# on-device / installer path (signature, patches the unit's own driver in place)
python3 deploy/binpatch/binpatch.py wifi devnp-mrvl_wlan-sdiorm.so.stock /tmp/out
sha256sum /tmp/out        # -> ca529b2b…64b2e7

# RE builder (recompiles the fragments from source, sha/span guarded)
python3 -m wireless.driver_re.build_patched_so STOCK OUT
```

`test_binpatch.sh` asserts the `wifi` entry changes exactly 40 bytes on
P5087/P5092/K1004, that P5087 equals the shipped patched driver, and that a
patched driver is reported as already patched (idempotent) — under both
MicroPython (as on the unit) and CPython.

## Status

Patched driver has been **live on the owner HU since 2026-10-18** (deployed as a
whole file then; this branch adds the installer wiring). Verified boot-clean,
`uap0` RUNNING, 40.7 Mbit/s one-way UDP, two × 60 s heavy-UDP runs with no wedge,
no watchdog, no stack restart, no speed regression. **Not yet shown:** the
reorder benefit under real packet loss — the clean −27 dBm lab channel has no
holes; a lossy/range run is still owed.

Recovery if a Wi-Fi break locks out SSH: Tailscale over the car's own LTE
(`root@100.127.17.86`), independent of this driver.

## Deploy (owner reboot required; installer does this via `patch_wifi`)

```sh
cp "$WIFI_SO" "$WIFI_SO.carplay-stock"              # backup once
python3 deploy/binpatch/binpatch.py wifi "$WIFI_SO.carplay-stock" "$WIFI_SO.new"
chmod 755 "$WIFI_SO.new" && mv -f "$WIFI_SO.new" "$WIFI_SO"
# WIFI_SO=/mnt/app/armle/lib/dll/devnp-mrvl_wlan-sdiorm.so ; effective on next boot
```

Roll back by restoring `.carplay-stock` (the standalone uninstaller auto-finds
it; the in-script uninstall restores it explicitly).
