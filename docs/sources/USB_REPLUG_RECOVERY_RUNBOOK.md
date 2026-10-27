# Complete runbook for fixing the CarPlay hang after a USB replug

Update 2026-09-17: the owner confirmed that connect and reconnect are resolved
on the current car. What follows preserves the historical status of the individual
investigations and the A/B procedure from September 7; it is not the current list of blockers.
Consolidated status: [TODO](../../TODO.md).

Status as of 2026-09-07: both failure points are confirmed statically, but which of them
triggered the specific dead session that was captured has not been proven live. Both
fixes are prepared and reversible, but have not yet passed an independent live A/B on the
head unit. The sections below therefore separate proven facts, fix
candidates, and the criteria after which they can be considered verified.

Target system:

- HU: Audi MHI2Q US, MU1316;
- firmware: `MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398`;
- media connector: `0x0424:0x2530`;
- phone transport: iAP2 + NCM, interface `carplay0`.

This document concerns USB/reconnect. It does not change the audio hook, APAC, the renderers,
Java, the firewall, or the `dio_manager` ownership logic.

## Short diagnosis

There are two independent races, and one can mask the other:

1. The old `stopncm.sh` sleeps for one second and then unconditionally destroys
   the interface by its global name `carplay0`. Within that second a new phone may already
   have created a new generation of `carplay0`; the old teardown will destroy that one instead.
2. If a CarPlay start ends in `TIMEOUT_STARTUP` several times, SI
   exhausts `maxSetupRetries`, issues a final
   `SwitchPort(direction=0)`, writes `start_stack::none` and never starts the
   device stack again. The phone is already plugged in, and the board-soldered `0x2530` hub did not
   survive a new insertion event, so the system stays in a stable dead end.

Typical terminal chain:

```text
fast replug / USB role swap / NCM failure
                    ↓
carplay0 is missing or has no carrier
                    ↓
iAP2 does not start, dio gets TIMEOUT_STARTUP
                    ↓
two stock SI stop → 5 s → start attempts
                    ↓
final SwitchPort(direction=0)
                    ↓
start_stack::none, no io-usb-dcd and no /dev/io-usb/otg_dev
                    ↓
SI waits for an event that has already happened
```

The absence of `io-usb-dcd` after the failure does not prove that the DCD crashed first: it
may be the normal result of SI's final stop-only cleanup.

## What exactly each component fixes

| Problem | Fix | Status |
|---|---|---|
| old delayed teardown destroys the new `carplay0` | generation identity + shared transaction lock in `startncm.sh`/`stopncm.sh` | host tests passed, live needed |
| a new mount collides with the old interface of the same name | the same lock serializes destroy and mount | host tests passed, live needed |
| `startncm.sh` tries to mount on a missing `otg_dev` | mandatory node check via `ls` | implemented |
| the stock script carries on after a failed mount | mount return-code check, cleanup of its own transaction only | implemented |
| UTF-8 en-dash in `if_up` arguments | ASCII `-s 2 -r 1` | fixed; this was not considered the root of the failure |
| CarPlay's final error cleanup only does stop | SI patch routes only the CarPlay error cleanup into the stock `direction=1` | statically verified, live needed |
| `sloginfo` loses the moment of breakage to the video storm | NCM decisions are written to the quiet `/tmp/carplay_hook.log` with the prefix `NCM_FENCE` | implemented |
| an unbounded external reset/relaunch may fight SI | do not add a USB reset loop to `rc.local`; SI remains the sole owner of OTG and `dio_manager` | mandatory constraint |

The fixes are located in
[`deploy/experimental/usb_replug_recovery`](../../deploy/experimental/usb_replug_recovery/README.md).
The four ready payload files, the exact stock files and the protected host scripts
are collected separately in
[`bundle_mu1316_p5087`](../../deploy/experimental/usb_replug_recovery/bundle_mu1316_p5087/README.md).

## Scope and reference hashes

These files must not be carried over to MH2p or another MU version. MH2p confirms the same
direction 0/1 policy, but uses a different ISA/layout.

Stock MU1316 P5087:

| File | SHA-256 |
|---|---|
| `startncm.sh` | `c533b63fa550126576cf3948b8060ef3f0f6d430ebe12c39769ef898e583713f` |
| `stopncm.sh` | `596b349c00ee992fa9755c76274eb14ab06062f512ad79f604527c5c8e520008` |
| `usblauncher_otg.lua` | `21ca8cb904448d24c0d13684d582ccd641139dd96758b8956080b9529b9a6b5c` |
| `smartphone_integrator` | `a25dc981e5591cb3c6b09f00fd95de23803dd3daa0f562e7e8d60b6d0e1b0d02` |

Candidates:

| File | SHA-256 |
|---|---|
| generation-aware `startncm.sh` | `e40ef9031ecbed1a34074a4213d76a0672aaeba2e7c18dc5ca88d52ce9a806ba` |
| generation-aware `stopncm.sh` | `804b3854a2e8460cf509f7eee3529cec8e0821766c273ee19d42e3344a033d68` |
| patched `usblauncher_otg.lua` | `76843b2dc0495e587336bc907bf2b3358b454ff2cad6e17c62ced42ae84de329` |
| SI re-arm candidate | `606236a81c7c4dafda6fdd08fe6eb27c43252796784da25d9760a7d552ee2af3` |

`patch_si_rearm.py` additionally checks the SI size `802798` and the exact instruction
context, and refuses an in-place patch. Any mismatch is a reason to stop the
installation, not to bypass the check.

## Rules for a safe trial

1. First record a fully stock baseline.
2. Test the NCM set on its own, leaving SI stock.
3. Before testing SI on its own, return the whole NCM set to stock.
4. Do not install both candidates at the same time before an independent A/B: otherwise a successful replug
   will not show which defect was eliminated.
5. Replace each set atomically and only as a whole. Never leave the new Lua
   with the old `stopncm.sh`, or one new NCM script alongside the other old one.
6. Modify the persistent copies, then activate them with a full HU reboot.
   Do not try to retrain an already running `usblauncher` by replacing the RAM copy of `/etc`.
7. The reboot is performed by the car owner. Scripts from the repository do not
   invoke it.

## Step 0. Host-side artifact check

On the Mac:

```sh
cd <workspace>/Patches/mhi2-carplay

sh -n deploy/experimental/usb_replug_recovery/startncm.sh
sh -n deploy/experimental/usb_replug_recovery/stopncm.sh
sh -n scripts/test_usb_replug_recovery.sh
./scripts/test_usb_replug_recovery.sh

python3 -B deploy/experimental/usb_replug_recovery/patch_si_rearm.py check \
  <workspace>/Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/eso/bin/apps/smartphone_integrator
```

Expected: `USB replug recovery shell tests: PASS` and
`state=stock ... sha256=a25dc9...`.

The NCM test covers:

- normal removal of the current generation;
- stale removal from an old USB address;
- overlap, where stop is holding the stock `sleep 1` and a new start has already arrived;
- a repeated start of the same generation;
- reclaiming a lock whose owner PID has already died;
- a mount failure without a false success result.

## Step 1. Read-only preflight of the head unit

Before any remount and write, pull the actual persistent files from the HU and verify
them on the Mac. This does not change the device.

The following phases assume that the variables `PROJECT_DIR`, `SSH` and `WORK_DIR`
remain set in the same shell window.

```sh
PROJECT_DIR=<workspace>/Patches/mhi2-carplay
SSH=<workspace>/audi_ssh.sh
WORK_DIR=$(mktemp -d /tmp/mhi2-usb-recovery.XXXXXX)

"$SSH" exec 'cat /mnt/system/etc/scripts/startncm.sh' \
  > "$WORK_DIR/startncm.hu.stock"
"$SSH" exec 'cat /mnt/system/etc/scripts/stopncm.sh' \
  > "$WORK_DIR/stopncm.hu.stock"
"$SSH" exec 'cat /mnt/system/etc/usblauncher_otg.lua' \
  > "$WORK_DIR/usblauncher_otg.hu.stock"
"$SSH" exec 'cat /mnt/app/eso/bin/apps/smartphone_integrator' \
  > "$WORK_DIR/smartphone_integrator.hu.stock"

shasum -a 256 \
  "$WORK_DIR/startncm.hu.stock" \
  "$WORK_DIR/stopncm.hu.stock" \
  "$WORK_DIR/usblauncher_otg.hu.stock"
python3 -B "$PROJECT_DIR/deploy/experimental/usb_replug_recovery/patch_si_rearm.py" \
  check "$WORK_DIR/smartphone_integrator.hu.stock"
```

All four results must match the stock-hash table above. If they do not,
first find out which modification is already installed on the HU. Do not create a backup
named `stock` from an unknown file, and do not force the patch.

## Phase A. Fixing the NCM race

### A1. Build the patched Lua locally

```sh
FW_SYSTEM=<workspace>/Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-system

cp "$FW_SYSTEM/etc/usblauncher_otg.lua" \
  "$WORK_DIR/usblauncher_otg.lua.stock"
cp "$FW_SYSTEM/etc/usblauncher_otg.lua" \
  "$WORK_DIR/usblauncher_otg.lua"
(
  cd "$WORK_DIR" &&
  patch --fuzz=0 -p0 \
    < "$PROJECT_DIR/deploy/experimental/usb_replug_recovery/usblauncher_otg.lua.patch"
)

shasum -a 256 \
  "$PROJECT_DIR/deploy/experimental/usb_replug_recovery/startncm.sh" \
  "$PROJECT_DIR/deploy/experimental/usb_replug_recovery/stopncm.sh" \
  "$WORK_DIR/usblauncher_otg.lua"
```

The results must match the three candidate hashes from the table.

### A2. Create one-time persistent backups

This step already writes to the HU, but does not yet activate the new files:

```sh
"$SSH" exec '
  set -e
  mount -uw /mnt/system
  [ -f /mnt/system/etc/scripts/startncm.sh.usb-replug-stock ] || \
    cp /mnt/system/etc/scripts/startncm.sh \
       /mnt/system/etc/scripts/startncm.sh.usb-replug-stock
  [ -f /mnt/system/etc/scripts/stopncm.sh.usb-replug-stock ] || \
    cp /mnt/system/etc/scripts/stopncm.sh \
       /mnt/system/etc/scripts/stopncm.sh.usb-replug-stock
  [ -f /mnt/system/etc/usblauncher_otg.lua.usb-replug-stock ] || \
    cp /mnt/system/etc/usblauncher_otg.lua \
       /mnt/system/etc/usblauncher_otg.lua.usb-replug-stock
  sync
'
```

The backup is created only after a successful read-only preflight, so the
`usb-replug-stock` suffix has an unambiguous meaning.

### A3. Upload `.new`, read back and verify

```sh
"$SSH" exec 'cat > /mnt/system/etc/scripts/startncm.sh.usb-replug-new' \
  < "$PROJECT_DIR/deploy/experimental/usb_replug_recovery/startncm.sh"
"$SSH" exec 'cat > /mnt/system/etc/scripts/stopncm.sh.usb-replug-new' \
  < "$PROJECT_DIR/deploy/experimental/usb_replug_recovery/stopncm.sh"
"$SSH" exec 'cat > /mnt/system/etc/usblauncher_otg.lua.usb-replug-new' \
  < "$WORK_DIR/usblauncher_otg.lua"

"$SSH" exec 'cat /mnt/system/etc/scripts/startncm.sh.usb-replug-new' \
  > "$WORK_DIR/startncm.readback"
"$SSH" exec 'cat /mnt/system/etc/scripts/stopncm.sh.usb-replug-new' \
  > "$WORK_DIR/stopncm.readback"
"$SSH" exec 'cat /mnt/system/etc/usblauncher_otg.lua.usb-replug-new' \
  > "$WORK_DIR/usblauncher_otg.readback"

shasum -a 256 \
  "$WORK_DIR/startncm.readback" \
  "$WORK_DIR/stopncm.readback" \
  "$WORK_DIR/usblauncher_otg.readback"
```

Only once the candidate hashes match, activate all three files in a single
transaction:

```sh
"$SSH" exec '
  set -e
  sh -n /mnt/system/etc/scripts/startncm.sh.usb-replug-new
  sh -n /mnt/system/etc/scripts/stopncm.sh.usb-replug-new
  chmod 755 \
    /mnt/system/etc/scripts/startncm.sh.usb-replug-new \
    /mnt/system/etc/scripts/stopncm.sh.usb-replug-new
  chmod 644 /mnt/system/etc/usblauncher_otg.lua.usb-replug-new
  mv /mnt/system/etc/scripts/startncm.sh.usb-replug-new \
     /mnt/system/etc/scripts/startncm.sh
  mv /mnt/system/etc/scripts/stopncm.sh.usb-replug-new \
     /mnt/system/etc/scripts/stopncm.sh
  mv /mnt/system/etc/usblauncher_otg.lua.usb-replug-new \
     /mnt/system/etc/usblauncher_otg.lua
  sync
'
```

After this, the owner performs a full HU reboot. Before it, the persistent files are already
new, but the running `usblauncher` and the RAM copy of `/etc` may still be old;
do not treat any result before the reboot as valid.

### A4. Verification after the reboot

First verify that the runtime copies really were updated:

```sh
"$SSH" exec 'cat /etc/scripts/startncm.sh' \
  > "$WORK_DIR/startncm.runtime"
"$SSH" exec 'cat /etc/scripts/stopncm.sh' \
  > "$WORK_DIR/stopncm.runtime"
"$SSH" exec 'cat /etc/usblauncher_otg.lua' \
  > "$WORK_DIR/usblauncher_otg.runtime"
shasum -a 256 \
  "$WORK_DIR/startncm.runtime" \
  "$WORK_DIR/stopncm.runtime" \
  "$WORK_DIR/usblauncher_otg.runtime"
```

Then run at minimum:

1. one normal connect after a cold boot;
2. ten slow unplug/replug cycles with a pause of more than three seconds;
3. twenty fast replugs with a pause of roughly 0.1–0.5 seconds;
4. five replugs during CarPlay start-up, while the session is still coming up;
5. one CarLife cycle, if CarLife is used on this HU.

Before the series, add a clear marker without clearing older evidence:

```sh
"$SSH" exec \
  'echo "USB_REPLUG_TEST phase=NCM run=01" >> /tmp/carplay_hook.log'
```

After the series, collect the data:

```sh
"$SSH" exec '
  grep NCM_FENCE /tmp/carplay_hook.log
  ls -l /dev/io-usb/otg_dev
  ifconfig carplay0
  ifconfig -a | grep carplay
  pidin ar | grep -E "[i]o-usb-dcd|[d]io_manager|[u]sblauncher"
  cat /ramdisk/pps/device/usb_ctrl1
  ls /ramdisk/pps/device
' > "$WORK_DIR/ncm-phase-snapshot.txt"
```

Phase A success criteria:

- after every connect, CarPlay comes up without a reboot;
- exactly the expected `carplay0` exists, with no orphan `carplay1`;
- `carplay0` gets a working carrier / the `RUNNING` flag;
- `NCM_FENCE` shows distinct `busno:devno:iface` values for new enumerations;
- on overlap, `wait_lock` is visible, or the stale removal gets `skip`;
- no `lock_timeout`, no endless `mount_rc`, no perpetual `TIMEOUT_STARTUP`.

If QNX immediately reuses the same `devno`, generation identity cannot
distinguish a late removal of the old session from the new one. Such a result means
`INCONCLUSIVE/STOP`: do not consider the NCM fence proven, and do not mask it by
increasing timeouts.

### A5. Full rollback of the NCM set

Restore only all three files together:

```sh
"$SSH" exec '
  set -e
  mount -uw /mnt/system
  [ -f /mnt/system/etc/scripts/startncm.sh.usb-replug-stock ]
  [ -f /mnt/system/etc/scripts/stopncm.sh.usb-replug-stock ]
  [ -f /mnt/system/etc/usblauncher_otg.lua.usb-replug-stock ]
  cp /mnt/system/etc/scripts/startncm.sh.usb-replug-stock \
     /mnt/system/etc/scripts/startncm.sh.rollback-new
  cp /mnt/system/etc/scripts/stopncm.sh.usb-replug-stock \
     /mnt/system/etc/scripts/stopncm.sh.rollback-new
  cp /mnt/system/etc/usblauncher_otg.lua.usb-replug-stock \
     /mnt/system/etc/usblauncher_otg.lua.rollback-new
  chmod 755 \
    /mnt/system/etc/scripts/startncm.sh.rollback-new \
    /mnt/system/etc/scripts/stopncm.sh.rollback-new
  chmod 644 /mnt/system/etc/usblauncher_otg.lua.rollback-new
  mv /mnt/system/etc/scripts/startncm.sh.rollback-new \
     /mnt/system/etc/scripts/startncm.sh
  mv /mnt/system/etc/scripts/stopncm.sh.rollback-new \
     /mnt/system/etc/scripts/stopncm.sh
  mv /mnt/system/etc/usblauncher_otg.lua.rollback-new \
     /mnt/system/etc/usblauncher_otg.lua
  sync
'
```

After the rollback, a full reboot is again required, followed by a check of the three stock hashes on the
runtime copies in `/etc`.

## Phase B. Fixing SI's terminal cleanup

Run this phase with a fully stock NCM set. The patch does not add an external
watchdog and does not create an endless retry. It changes exactly one ARM instruction:

```text
VA 0x17FDB4, file offset 0x7FDB4
stock:   00 30 a0 e3    MOV R3, #0
patched: 2e 33 09 e0    AND R3, R9, LR, LSR #6
```

At this point `R9` is the error flag, and `LR == 0x40` only for CarPlay. Result:

| Cleanup | direction |
|---|---:|
| normal CarPlay disconnect | 0 |
| any CarLife cleanup | 0 |
| final CarPlay cleanup after an error | 1 |

The stock direction 1 performs activate-CarPlay, switch-port and
`start_stack::device,2`. It removes the stop-only ending, but live testing still has to
prove that this path completes successfully in precisely the hung hardware
state.

### B1. Create and verify the candidate

Use the SI read from the HU during the read-only preflight:

```sh
python3 -B "$PROJECT_DIR/deploy/experimental/usb_replug_recovery/patch_si_rearm.py" \
  apply \
  "$WORK_DIR/smartphone_integrator.hu.stock" \
  "$WORK_DIR/smartphone_integrator.rearm"

python3 -B "$PROJECT_DIR/deploy/experimental/usb_replug_recovery/patch_si_rearm.py" \
  check "$WORK_DIR/smartphone_integrator.rearm"
```

Expected: `state=patched`, size `802798` and candidate SHA-256
`606236a8...`.

Verify reversibility before installing:

```sh
python3 -B "$PROJECT_DIR/deploy/experimental/usb_replug_recovery/patch_si_rearm.py" \
  revert \
  "$WORK_DIR/smartphone_integrator.rearm" \
  "$WORK_DIR/smartphone_integrator.reverted"
cmp \
  "$WORK_DIR/smartphone_integrator.hu.stock" \
  "$WORK_DIR/smartphone_integrator.reverted"
```

### B2. Backup, upload and atomic replacement

```sh
"$SSH" exec '
  set -e
  mount -uw /mnt/app
  [ -f /mnt/app/eso/bin/apps/smartphone_integrator.usb-replug-stock ] || \
    cp /mnt/app/eso/bin/apps/smartphone_integrator \
       /mnt/app/eso/bin/apps/smartphone_integrator.usb-replug-stock
  sync
'

"$SSH" exec \
  'cat > /mnt/app/eso/bin/apps/smartphone_integrator.usb-replug-new' \
  < "$WORK_DIR/smartphone_integrator.rearm"

"$SSH" exec \
  'cat /mnt/app/eso/bin/apps/smartphone_integrator.usb-replug-new' \
  > "$WORK_DIR/smartphone_integrator.readback"
python3 -B "$PROJECT_DIR/deploy/experimental/usb_replug_recovery/patch_si_rearm.py" \
  check "$WORK_DIR/smartphone_integrator.readback"
```

Only after `state=patched`:

```sh
"$SSH" exec '
  set -e
  chmod 755 /mnt/app/eso/bin/apps/smartphone_integrator.usb-replug-new
  mv /mnt/app/eso/bin/apps/smartphone_integrator.usb-replug-new \
     /mnt/app/eso/bin/apps/smartphone_integrator
  sync
'
```

Do not kill the live SI by hand. The owner performs a full reboot, after which the
runtime binary is read back to the Mac once more and verified with the `check` command.

### B3. SI live criteria

Repeat the same matrix of normal and fast replugs. For this phase the case that
actually exhausts `maxSetupRetries` is especially important. If such a failure
did not occur, the SI test remains `INCONCLUSIVE`, even if twenty normal
connections succeeded.

A successful SI-patch result:

- after the retries are exhausted, the stack does not stay on `start_stack::none` forever;
- `io-usb-dcd` and `/dev/io-usb/otg_dev` come back via the stock SI path;
- the already-plugged-in phone gets a fresh chance to bring up CarPlay;
- no restart storm, and SI does not start killing `dio_manager` endlessly;
- normal unplug and CarLife still perform direction 0 semantics.

### B4. SI rollback

```sh
"$SSH" exec '
  set -e
  mount -uw /mnt/app
  [ -f /mnt/app/eso/bin/apps/smartphone_integrator.usb-replug-stock ]
  cp /mnt/app/eso/bin/apps/smartphone_integrator.usb-replug-stock \
     /mnt/app/eso/bin/apps/smartphone_integrator.rollback-new
  chmod 755 /mnt/app/eso/bin/apps/smartphone_integrator.rollback-new
  mv /mnt/app/eso/bin/apps/smartphone_integrator.rollback-new \
     /mnt/app/eso/bin/apps/smartphone_integrator
  sync
'
```

Then a full reboot and a readback check for `state=stock`.

## Final production combination

Both fixes may be installed together only after the independent phases:

| NCM-only | SI-only | Decision |
|---|---|---|
| eliminates the reproducible NCM overlap | SI test could not reach exhaustion | keep NCM; treat SI as unverified, do not install it "just in case" |
| race remains | exhaustion recovers via direction 1 | keep SI, continue investigating NCM identity |
| each separately closes its reproducible scenarios | both PASS | combined installation is acceptable, with a repeat of the full matrix |
| any variant causes a regression/storm | — | immediate rollback of the corresponding layer |

Even after two PASSes an honest limitation remains: if the OTG
`usblauncher` itself dies and the one-shot hub flag prevents the main launcher from starting it
again, these two patches do not prove recovery from that third scenario.
An automatic kill/relaunch wrapper has already shown that it can bring the node back
"behind the owner's back" without bringing CarPlay up. This branch first needs
a separate generation-aware lifecycle design and live proof; an `rc.local` reset loop
must not be passed off as a finished fix.

## Diagnosing a remaining dead session

Before rebooting, save:

```sh
"$SSH" exec '
  echo "USB nodes"
  ls -l /dev/io-usb/otg_dev /dev/io-usb/otg /dev/io-usb/io-usb
  echo "processes"
  pidin ar | grep -E "[i]o-usb-dcd|[i]o-otg|[u]sblauncher|[d]io_manager|[s]martphone_integrator"
  echo "NCM"
  ifconfig carplay0
  ifconfig -a | grep carplay
  echo "SI command"
  cat /ramdisk/pps/device/usb_ctrl1
  echo "PPS topology"
  ls /ramdisk/pps/device
  echo "quiet log"
  grep -E "NCM_FENCE|TIMEOUT_STARTUP|start_stack" /tmp/carplay_hook.log
' > "$WORK_DIR/dead-session-snapshot.txt"
```

Interpretation:

| State | Likely branch |
|---|---|
| `otg_dev` present, `carplay0` absent, log shows `mount_rc`/timeout | NCM start/mount |
| `carplay0` present but no `RUNNING`; PPS shows matched 2/running 1 | the phone did not enable both functions, iAP2 did not start |
| after retries no `otg_dev`, last `usb_ctrl1` is `start_stack::none` | SI terminal cleanup |
| the new start waited for the lock, then came up; the old stop got `skip` | NCM fence worked as designed |
| `usblauncher` missing while SI/hub are alive | a separate launcher-lifecycle failure; the current two PASSes do not prove it is fixed |

Do not use `sloginfo` as the only source: on this system its ring buffer
is flushed by the video storm in about two minutes. Save
`/tmp/carplay_hook.log` and the PPS/process snapshot first.

## Emergency recovery before the fixes are installed

The only proven full recovery of an already-captured dead session is a
full HU reboot. Manually invoking the device-stack start brought back `otg_dev`, but
did not bring CarPlay up by itself. Manually switching the media-connector port once
made the state worse. So collect the snapshot above before the reboot; the reboot itself
is performed by the owner.

Do not run in a loop:

- `start_otg_stack_device_mode.sh` from every `dio_manager` launch;
- kill/relaunch of `usblauncher` without a generation/latch;
- `reset port` or a GPIO power-cycle from `rc.local`;
- a parallel restart of `dio_manager` by an external watchdog;
- raising `maxSetupRetries` as a "fix" — it only lengthens the storm.

## What does not need to change

- `verifyPortReset` state 12 → 14: CarPlay is app ID 0 and takes a separate branch;
- Tailscale, memory and CPU: previously suspected, but refuted by measurements;
- the LTE edit in the main `usblauncher.lua`: it concerns the modem;
- altScreen/APAC/renderers: the failure happens before RTSP and the media streams;
- `rc.local`: keep it as a generic supervisor for the services already assigned to it,
  but do not make it a second owner of the USB state machine.

## Related evidence

- [`SI_RAPID_REPLUG_RACE_RE.md`](../connect-and-usb/SI_STACK_AND_REPLUG_RACE.md) — the exact
  SI state machine, ARM addresses and the MH2p comparison;
- [`USB_IPHONE_CONNECT_POSTMORTEM.md`](../connect-and-usb/USB_REPLUG_RECOVERY.md) — live
  symptoms, refuted hypotheses and log limitations;
- [`USB_STARTUP_RECOVERY_DIAGNOSIS.md`](../connect-and-usb/SI_STACK_AND_REPLUG_RACE.md)
  — early diagnosis and the correcting note;
- [`NCM_AND_FIREWALL_IDEAL_START.md`](../connect-and-usb/USB_AND_NCM_CONNECT.md)
  — the normal USB/NCM startup flow;
- [`deploy/experimental/usb_replug_recovery/README.md`](../../deploy/experimental/usb_replug_recovery/README.md)
  — implementation of both reversible candidates.
