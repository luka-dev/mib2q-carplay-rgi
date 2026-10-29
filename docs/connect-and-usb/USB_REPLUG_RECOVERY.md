# USB Replug Recovery

The installer supplies three coordinated components for wired reconnect: an in-place SI re-arm patch, generation-aware NCM start/stop scripts and SI cleanup integration. SI remains the owner of the USB/CarPlay lifecycle.

## ⚙️ Installed components

| Component | Current behavior | Source |
| --- | --- | --- |
| SI re-arm | Patches the recognized retry-cleanup instruction in the unit's own binary so the device stack is re-armed | [binpatch.py](../../deploy/binpatch/binpatch.py) |
| NCM generation fence | Serializes start/stop per interface and prevents an old teardown from destroying a newer `carplay0` generation | [NCM start](../../deploy/experimental/usb_replug_recovery/startncm.sh), [NCM stop](../../deploy/experimental/usb_replug_recovery/stopncm.sh) |
| OTG cleanup | Routes SI cleanup through a generation-aware wrapper | [carplay_ncm_cleanup.sh](../../deploy/smartphone_integrator/carplay_ncm_cleanup.sh) |
| Installation gates | Checks supported inputs before applying each component | `patch_ncm` and `patch_otg_cleanup` in [custom.sh](../../install_MoreIncredibleBash/mod/custom.sh) |

The `experimental` source-directory name does not mean the NCM scripts are excluded from the installer. They are staged by [prepare_install.sh](../../scripts/prepare_install.sh). Component applicability depends on the installer's fingerprints.

## 🔒 Ownership and filesystem rules

The startup wrapper checks files, publishes its owner, starts the renderer monitor and executes DIO. It does not reset USB ports or create a competing USB recovery loop. The renderer monitor does not control SI, DIO or the OTG stack.

<a id="-shipped-2026-09-25-fence-on-the-car"></a>
<a id="ncm-generation-fence"></a>

The NCM scripts use a per-interface directory lock under **`/ramdisk`** and a generation file under `/tmp`. On this QNX system, `/tmp` is `/dev/shmem`: files work there, but directory creation does not. Lock release uses `rm -rf`; the unit does not provide `rmdir`. Generation identity includes the USB address/interface and process identity, so a delayed teardown can reject stale ownership.

The OTG launcher attaches to the native stack; it does not create one. Host/device stacks are mutually exclusive. Preserve native SI/launcher ownership when changing recovery logic. The stock retry and cleanup state machine is documented in [SI Stack and Replug Race](SI_STACK_AND_REPLUG_RACE.md).

## 🔎 Diagnose a failed reconnect

Capture state before a restart:

```sh
ls -l /dev/io-usb/otg_dev /dev/io-usb/otg /dev/io-usb/io-usb
pidin ar | grep -E "[i]o-usb-dcd|[i]o-otg|[u]sblauncher|[d]io_manager|[s]martphone_integrator"
ifconfig carplay0
cat /ramdisk/pps/device/usb_ctrl1
grep -E "NCM_FENCE|TIMEOUT_STARTUP|start_stack" /tmp/carplay_hook.log
```

| State | Inspect |
| --- | --- |
| `otg_dev` exists, `carplay0` does not | NCM start, mount result and lock ownership |
| `carplay0` lacks `RUNNING` | Carrier, USB function enablement and the device stack |
| `otg_dev` absent after retries | SI retry state and final stack command |
| Teardown logs `skip` for an old generation | Expected stale-cleanup rejection |
| SI is alive but `usblauncher` is absent | Native launcher lifecycle |

Save `/tmp/carplay_hook.log`, `/tmp/carplay_wrapper.log` and relevant slog output promptly. A missing `io-usb-dcd` process alone does not establish the initiating cause; it may be the result of SI cleanup.

## 🧪 Validation

Relevant checks are [test_install_ncm.sh](../../scripts/test_install_ncm.sh), [test_usb_replug_recovery.sh](../../scripts/test_usb_replug_recovery.sh), [test_usb_replug_bundle.sh](../../scripts/test_usb_replug_bundle.sh) and [test_binpatch.sh](../../scripts/test_binpatch.sh). Vehicle checks cover cold connection, rapid unplug/replug, cleanup overlapping a new connection and return from calls/media.

Earlier traces, A/B bundles and rejected recovery attempts are retained in the [archived investigation](../archive/snapshots-2026-10-29/connect-and-usb/USB_REPLUG_RECOVERY.md).
