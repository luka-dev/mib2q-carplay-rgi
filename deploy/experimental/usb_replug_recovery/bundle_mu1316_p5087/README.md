# MU1316 P5087 CarPlay USB replug recovery bundle

A self-contained set of four ready-made files for the Audi MHI2Q US MU1316:

1. `startncm.sh` — generation-aware NCM start;
2. `stopncm.sh` — teardown with USB identity and a shared lock;
3. `usblauncher_otg.lua` — passes `busno/devno/iface` to both scripts;
4. `smartphone_integrator` — one-instruction CarPlay error re-arm.

The bundle is intended only for
`MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398`. The installers compare
the actual HU files byte for byte against `stock/` and `payload/` and refuse to
modify an unknown version.

Status: offline/static verification and host tests passed; live A/B is not yet
closed. The scripts never call reboot — the HU owner does that.

## Folder contents

```text
bundle_mu1316_p5087/
├── payload/                  the four ready-made patched files
├── stock/                    the exact four stock files for rollback
├── docs/
│   ├── TECHNICAL.md          mechanism and limits of the fixes
│   ├── LIVE_TEST.md          independent A/B and PASS criteria
│   └── ROLLBACK.md           exact return to stock
├── MANIFEST.sha256           check of all eight binary/text payloads
├── verify_bundle.sh          fully local verification
├── verify_over_ssh.sh        read-only HU check after reboot
├── install_over_ssh.sh       stage/readback/atomic install
├── rollback_over_ssh.sh      stage/readback/atomic stock restore
└── capture_state_over_ssh.sh read-only snapshot of a dead session
```

## Local verification

```sh
cd /Users/luka/Desktop/AUDI_2/Patches/mhi2-carplay/deploy/experimental/usb_replug_recovery/bundle_mu1316_p5087
./verify_bundle.sh
```

Expected: eight `OK` lines and `bundle verification: PASS`.

By default
`/Users/luka/Desktop/AUDI_2/audi_ssh.sh` is used. For a different helper:

```sh
AUDI_SSH_HELPER=/absolute/path/to/audi_ssh.sh ./verify_over_ssh.sh stock
```

## Modes

| Mode | NCM scripts + Lua | SI | Purpose |
|---|---|---|---|
| `stock` | stock | stock | only `verify_over_ssh.sh` after a full rollback |
| `ncm` | payload | stock | first independent live phase |
| `si` | stock | payload | second independent live phase |
| `all` | payload | payload | final, only after two separate PASSes |

## Recommended order

Check the original HU without writing anything:

```sh
./verify_over_ssh.sh stock
```

First phase:

```sh
./install_over_ssh.sh ncm
# owner reboots the HU
./verify_over_ssh.sh ncm
```

Run the matrix from [`docs/LIVE_TEST.md`](docs/LIVE_TEST.md). Then return to
stock before the second independent test:

```sh
./rollback_over_ssh.sh ncm
# owner reboots the HU
./verify_over_ssh.sh stock
```

Second phase:

```sh
./install_over_ssh.sh si
# owner reboots the HU
./verify_over_ssh.sh si
```

Only if both phases gave a PASS separately:

```sh
./rollback_over_ssh.sh si
# owner reboots the HU and verifies stock
./install_over_ssh.sh all
# owner reboots the HU
./verify_over_ssh.sh all
```

`install_over_ssh.sh` performs:

- local verification of `MANIFEST.sha256`;
- read-only identification of each target as exact `stock` or exact `payload`;
- refusal of `ncm` when SI is non-stock and refusal of `si` when NCM is
  non-stock, so an independent A/B is not accidentally turned into a combined
  test;
- a check of already existing `.usb-replug-stock` backups;
- remount of only the required persistent partition;
- upload to `.usb-replug-new`;
- readback and a byte-for-byte `cmp` before activation;
- `sh -n` of the NCM scripts on QNX;
- atomic `mv` and `sync`;
- writing a `reboot_required` RAM marker, because of which the verifier will
  not accept even an SI-only replacement until a full reboot;
- stopping without a reboot and without killing live processes.

## Quick snapshot before reboot

If the session died, first:

```sh
./capture_state_over_ssh.sh ./dead-session-01.txt
```

The script changes nothing on the HU. It saves USB nodes, processes, NCM,
`usb_ctrl1`, PPS topology and the quiet `NCM_FENCE` lines to the Mac.

## Hard constraints

- Do not install only the two `.sh` files without the matching
  `usblauncher_otg.lua`.
- Do not apply SI to a different SHA, MH2p or a different MU version.
- Do not treat ordinary successful connects as proof for SI: the test must
  actually reach exhaustion of `maxSetupRetries`.
- Do not add a USB reset/kill loop to `rc.local`.
- The only proven emergency recovery of an already dead stock session is a
  full reboot after saving a snapshot.

Mechanism details: [`docs/TECHNICAL.md`](docs/TECHNICAL.md). Full rollback:
[`docs/ROLLBACK.md`](docs/ROLLBACK.md).
