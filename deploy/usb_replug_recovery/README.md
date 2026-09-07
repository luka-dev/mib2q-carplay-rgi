# MU1316 rapid-replug recovery artifacts

These are offline, reversible candidates for the two independently verified
failure paths. Nothing in this directory deploys to the head unit by itself.

The complete Russian deployment, live A/B, diagnosis, acceptance, and rollback
recipe is in
[`../../docs/USB_REPLUG_RECOVERY_RUNBOOK.md`](../../docs/USB_REPLUG_RECOVERY_RUNBOOK.md).

A self-contained exact-version bundle with ready `payload/`, exact `stock/`, a
SHA-256 manifest, guarded host installers and its own documentation is in
[`bundle_mu1316_p5087/`](bundle_mu1316_p5087/README.md).

## 1. NCM generation fence

Install `startncm.sh` and `stopncm.sh` together with the small
`usblauncher_otg.lua.patch`. The launcher passes `busno/devno/iface` for both
the NCM start and the corresponding removal. `startncm.sh` publishes the active
owner as:

```text
/tmp/ncm_generation_carplay0 = busno:devno:iface:start-script-pid
```

Both scripts take the same per-interface transaction lock. If teardown wins,
it holds the lock across Audi's stock one-second delay and the new start waits
until the old interface is gone. If start wins, it replaces any older
global-name interface and publishes the new generation only after mount/up;
the delayed removal then fails its USB-identity check. This also prevents the
new mount from colliding with an old `carplay0` that still exists during the
delay.

The scripts retain no-argument legacy behavior so they can be rolled back or
staged without breaking CarLife. The strong fence is active only for the
argument-bearing CarPlay commands produced by the matching launcher patch.
Their low-volume decisions are appended to `/tmp/carplay_hook.log` with the
prefix `NCM_FENCE`.

Run the host-side regression test with:

```sh
./scripts/test_usb_replug_recovery.sh
```

Known residual risk: this fence assumes QNX assigns a different `devno` to a
new USB enumeration. The next live capture must record the three launcher
values to prove the assumption on this unit. An abandoned lock is reclaimed
when its recorded PID is dead; an ownerless lock times out after eight seconds
instead of performing an unsafe teardown.

## 2. Experimental SI error re-arm

`patch_si_rearm.py` patches exactly one ARM instruction in the MU1316 P5087
`smartphone_integrator` image. It refuses all other sizes and SHA-256 hashes and
never writes in place.

Stock final CarPlay/new-connector cleanup enqueues `SwitchPort` with
`direction=0`. `MediaConnectionService_OnJobSwitchPort` then executes cleanup,
writes `start_stack::none`, reports completion, and does not start the device
stack. The patch changes the direction expression at VA `0x17FDB4` (file offset
`0x7FDB4`):

```text
stock:   MOV R3, #0
patched: AND R3, R9, LR, LSR #6
```

At that site `R9` is the cleanup error flag (`0` or `1`) and `LR` is `0x40`
only for CarPlay (`REPORT_CARPLAY_SUPPORTED`; it is zero for CarLife). The
result is therefore:

| Cleanup | SwitchPort direction |
|---|---:|
| normal CarPlay removal | 0 |
| CarLife cleanup | 0 |
| CarPlay error/final retry exhaustion | 1 |

Direction 1 reuses SI's existing activate-CarPlay, switch-port, and
`start_stack::device,2` path instead of the terminal stop-only path. It does not
create a new loop or touch `dio_manager` ownership.

Create and reverse a local candidate image:

```sh
./deploy/usb_replug_recovery/patch_si_rearm.py check /path/to/stock/smartphone_integrator
./deploy/usb_replug_recovery/patch_si_rearm.py apply /path/to/stock/smartphone_integrator /tmp/smartphone_integrator.rearm
./deploy/usb_replug_recovery/patch_si_rearm.py revert /tmp/smartphone_integrator.rearm /tmp/smartphone_integrator.stock
```

This SI patch is intentionally marked experimental. It has static verification
and byte-for-byte reversal coverage, but no live result yet. Test the NCM fence
alone first. Only if the dead session remains should the SI image be tested,
with the stock binary retained for immediate rollback.

## Verified image boundary

- Product: `MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398`
- Stock size: `802798`
- Stock SHA-256: `a25dc981e5591cb3c6b09f00fd95de23803dd3daa0f562e7e8d60b6d0e1b0d02`
- Patched SHA-256: `606236a81c7c4dafda6fdd08fe6eb27c43252796784da25d9760a7d552ee2af3`

The MH2p comparison image keeps the same `SwitchPort` policy: its direction-zero
path ends in `MediaConnectionService_WriteStopStackCommand`, while direction
one uses the start path. The binary layout and instruction set differ, so this
MHI2Q patch must not be applied to MH2p.
