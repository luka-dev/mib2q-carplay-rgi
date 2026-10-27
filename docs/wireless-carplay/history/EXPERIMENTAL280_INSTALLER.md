# Wireless-only installation candidate

_MHI2Q wireless CarPlay — historical investigation, integrated 2026-09-29._

---

[Wireless overview](../README.md) · [Current stock210 lifecycle](../STOCK210_RECEIVER_LIFECYCLE.md)

Historical investigation; earlier limitations and commands describe that experiment. The current implementation and validation boundary are in the section overview.

Implementation and local diagnostic paths in this record refer to the [original source directory](../../../wireless/integration/). Run `wireless/...` commands from the repository root; moving this note does not move binaries or logs.

## 📊 Experiment overview

```mermaid
flowchart LR
    accTitle: Historical replacement installer stages
    accDescr: These stages belong only to the experimental280 replacement; the current stock210 review archive has no installer.
    preflight["Validate pinned inputs"]
    backup["Back up replacement targets"]
    stage["Stage experimental280 files"]
    replace["Replace reviewed target files"]
    rollback["Restore owned backups"]
    preflight --> backup --> stage --> replace
    replace -->|Failure or rollback| rollback
```

## 📋 Implementation and evidence

**Historical experimental280 installer. Do not use it for the stock210 hook
candidate.** The current [stock210 review package](../STOCK210_RECEIVER_LIFECYCLE.md)
preserves factory libairplay and has no installer. The procedure below replaces
libairplay and belongs only to the separate older experiment.

`install_wireless_candidate.sh` is included in the offline review archive. It
has three modes: `preflight`, `install`, and `rollback`. Without
`MHI2Q_TARGET_ROOT`, it refuses all three modes unless
`MHI2Q_ENABLE_HU_INSTALL=1` is explicitly set. No mode starts or stops a
service or reboots the unit; preflight runs the daemon briefly with
`--check-config`. Offline tests use a temporary `MHI2Q_TARGET_ROOT` tree.

The script needs `MHI2Q_STAGE` pointing to an extracted bundle. Before any
write, it checks every fixed bundle artifact with the firmware's `openssl
dgst -sha256`, compares each live replacement against the bundled exact
baseline with `cmp`, requires all new file paths to be vacant, and runs the
QNX daemon's `--check-config` mode on a separate per-unit
`integration/wireless_carplay.env`. The daemon check only parses identity,
network, MFi bus address, and custom-IE slot; it does not open hardware or
publish the Wi-Fi IE. This QNX CLI path and the factory OpenSSL digest format
were exercised in QEMU (`daemon/out/qemu_daemon_bridge.log`).
`install` also checks the real lock directory before creating backups: the
bundled `dio_owner_exec --check-locks` creates private temporary files beside
both `/mnt/persist` locks and verifies that a second process is excluded by
`fcntl` locks. It removes the probe files. A failed check leaves all replaced
files untouched. `preflight` stays read-only and therefore does not run this
capability probe.

The eight replaced paths are stock `btstack` and `libairplay.so`, the four
already-widened wired PF profiles, the current wired
`carplay_startup.sh`, and the current local `rc.local`. The archive carries
each exact baseline. A different firmware or later modification fails
preflight. Six new files live only in `/mnt/app/root/hooks`: the Rust daemon,
owner helper, native supervisor, receiver bridge, supervisor launcher and
per-unit environment. The script never invokes the existing wired-CarPlay
uninstaller or alters its Java, renderers, SI JSON, NCM scripts, or cleanup
hooks.

Installation first copies all eight baselines to sibling `.wireless-prev`
backups, places the six new files, then atomically replaces the eight files.
`rc.local` is replaced last. An error after backup creation triggers an
ownership-checked rollback. Explicit rollback refuses to overwrite a file
that changed after installation; it restores the eight original files from
their backups and removes only unchanged wireless-owned files. A later edit to
an owned file is preserved and reported so it cannot block restoration of
stock Bluetooth. The user-supplied
environment is mode `0600`. No process transition is hidden in file staging;
the new boot path has not been tested on the head unit.
On the live filesystem only, `install` and `rollback` remount `/mnt/app` and
`/mnt/system` writable after their read-only checks and before changing files.
The sandbox test uses its temporary tree and never invokes `mount`.

`test_install_wireless_candidate.py` unpacks the archive into a temporary
tree, substitutes a host config-check stub for the QNX executable in that
tree, and verifies preflight, missing-lock-directory rejection, installation, a forced mid-install failure and
automatic rollback, duplicate-install rejection, refusal to overwrite a
post-install edit, clean rollback, and rejection of a corrupted payload.
`qemu_install_wireless.py` also runs the whole shell installer in QNX 6.5
QEMU on a disposable writable QNX4 RAM disk. It decodes the exact bundle into
that disk, starts the actual QNX daemon in `--check-config` mode, and verifies
preflight, missing-lock-directory rejection, a forced mid-install rollback, installation, and explicit rollback
against the file bytes (`out/qemu_install_wireless.log`). The fixture uses
synthetic unit configuration and does not start the wireless services.
Neither test proves writable QNX6 `/mnt/persist` locks, active PF rules, or
radio/MFi behavior on the unit. The separate factory-`pfctl` QNX/QEMU test
parses the exact default candidate PF profile and compares the other three
against their wired baselines under single-interface VM aliases; see
`firewall/README.md` in the repository.

This candidate is not ready for on-unit execution. The bundled `libairplay.so`
is now the from-source 280.33.8 hardware candidate with MU1316 `ScreenCreate`
ABI adaptation, wireless pairing routes and identity, real QSA and QNX MFi
I²C code. The software-MFi/null-audio sibling completed pair-setup,
pair-verify, and encrypted `/info` against a same-SDK synthetic client in
QEMU. The hardware binary passed the pinned `dio_manager` export check and a
full-firmware symbol-resolution audit; it has not run on the head.

The hardware receiver's pairing keychain is directed to
`/mnt/persist/mhi2q-carplay/keychains` and creates its file with mode `0600`.
The QEMU sibling passed a file-mode check during pairing. Writable QNX6 mount
behavior and persistence across boots have not been tested on the unit.

The production bridge gates START on the candidate's port 7000, feature bits
37 and 38, and Bonjour pairing identity. The per-unit environment must
advertise `MHI2Q_CP_AIRPLAY_PORT=7000`.

Required runtime evidence still includes the on-unit result of the writable
QNX6 owner-lock probe, exact per-unit identity and AP configuration, active PF ruleset
inspection, a real MFi transaction, and a phone/radio session. An independent QEMU client
has exercised the receiver's pairing, encrypted control, screen and PCM
media paths; persistent keychain behavior on the unit is unknown. See the bundled
`reference/identity_and_pairing.md`. The current archive is a review artifact
and never contacts the head unit.
