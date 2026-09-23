# MHI2Q CarPlay cluster integration

Production implementation for Audi MHI2Q (MU1316, QNX 6.5 ARMv7):

- stock `libairplay.so` 210.81 inside `dio_manager`;
- `LD_PRELOAD` hook for CarPlay negotiation, route guidance, altScreen stream 111 and the
  buffered/upmixed 5.1 audio path;
- external QNX renderers for maneuver graphics and cluster video;
- one Java patch for cluster context, BAP/RGI, cover art and steering-wheel input.

The shipped path always keeps stock `libairplay.so`. From-source libairplay/dio experiments are
research records, not production dependencies or authorities for stock 210 behavior.

**Disclaimer:** use at your own risk. These patches modify firmware binaries and system
configuration on your infotainment unit. The installer keeps a `.carplay-stock` backup of every
file it replaces, but back up anything you change by hand as well. The authors are not
responsible for any damage, bricked units or warranty issues resulting from their use.

## Gallery

<p align="center">
  <img src="assets/gallery/IMG_0082_39-45.gif" width="90%" />
</p>
<p align="center">
  <img src="assets/gallery/IMG_0623.jpeg" width="45%" />
  <img src="assets/gallery/IMG_6299.jpeg" width="45%" />
</p>
<p align="center">
  <img src="assets/gallery/IMG_6302.jpeg" width="45%" />
  <img src="assets/gallery/IMG_0599.jpeg" width="45%" />
</p>

## Documentation

All documentation is one linked Markdown tree in [`docs/`](docs/README.md). Start at
[`docs/README.md`](docs/README.md), then the per-domain maps:

| Domain | Entry point |
| --- | --- |
| Audio (upmix, HSSR model, J525 amp, codecs, 5.1 routing) | [`docs/audio/README.md`](<docs/audio/README.md>) |
| Cluster and route guidance (contexts, BAP/RGI, HUD renderer) | [`docs/cluster-and-rgi/README.md`](<docs/cluster-and-rgi/README.md>) |
| AltScreen and video (stream 111, OMX pipeline, loader) | [`docs/altscreen-and-video/README.md`](<docs/altscreen-and-video/README.md>) |
| Connect and USB (NCM, replug recovery, PF, session lifecycle) | [`docs/connect-and-usb/README.md`](<docs/connect-and-usb/README.md>) |
| Build and deploy (toolchain, release map, VIN protection) | [`docs/build-and-deploy/README.md`](<docs/build-and-deploy/README.md>) |
| Architecture overview | [`docs/reference/ARCHITECTURE.md`](docs/reference/ARCHITECTURE.md) |

Read [`docs/LANDMINES.md`](docs/LANDMINES.md) before touching a subsystem. Long-form white papers
are in `docs/research/`; primary evidence (papers, manuals, data files, decompiled stock Java) is in
`docs/sources/`.

## Repository layout

| Path | Purpose |
| --- | --- |
| `hook/` | Shipping native `libcarplay_hook.so` source |
| `java_patch/` | The only Java patch source |
| `maneuver_render/` | GLES maneuver overlay renderer (cluster plane 98) |
| `altscreen_render/` | OMX/GLES altScreen video renderer and PRISM loader (cluster plane 99) |
| `apac_decoder/` | APAC decoder port behind the optional `libcarplay_apac.so` |
| `common/` | Shared QNX Screen surface code |
| `models/` | HSSR upmix models (q6d main model, V11 guard) |
| `deploy/` | On-unit runtime scripts, SI child config, patched firmware and PF/deva recipes |
| `scripts/` | Host build and test entry points |
| `install_MoreIncredibleBash/`, `uninstall_MoreIncredibleBash/` | M.I.B. SD install/remove scripts |
| `protector/` | VIN-lock protector used by the protected build |
| `tools/` | Host tools (upmix renderer and benches, QSA emulator, amp model, …) |
| `assets/` | RE working sets (`soundamp/`, `vc-fonts/`) and visual references |
| `toolchain/qnx65-abi/` | QNX Screen/OpenMAX ABI headers used only for cross-compilation |
| `docs/` | The documentation (GitHub-rendered; also opens as an Obsidian vault) |
| `build/` | Build outputs (untracked, empty in git) |
| `logs/` | Ignored local evidence: vehicle captures, HU logs, iPhone sysdiagnose, state traces |

Build output, raw unit logs and generated class trees are intentionally kept outside Git.
The cleanup archive is under `/Users/luka/Desktop/AUDI_2/LocalLogs/`.

## Build

Run from the repository root:

```sh
./scripts/build_java.sh
./scripts/build_hook.sh
./scripts/build_renderers.sh
```

Native builds run in the `qnx65-armv7-toolchain` Docker image; `:latest` (the default) is GCC 8.5,
`QNX_TOOLCHAIN_VARIANT=4.9` selects GCC 4.9.4. Renderer import libraries are generated during the
build; the resulting ELFs resolve the real Screen/EGL/GLES/OpenMAX libraries on the unit.

| Variant | Command | Output |
| --- | --- | --- |
| Quiet production | `LOG=0 ./scripts/build_hook.sh` | logging, audio probe captures and pace diagnostics compiled out |
| No audio module | `HOOK_AUDIO=0 ./scripts/build_hook.sh` | hook without the type-103 audio path, no codec |
| State trace | `LOG=0 AUDIO_STATE_TRACE=1 ./scripts/build_hook.sh` | compact `/tmp/carplay_state_trace.log` ledger (see `scripts/carplay_state_trace.sh`) |
| VIN-locked | `./scripts/build_hook_protected.sh <VIN>` | stamped `.so` files in `build/protected_<VIN>/` |

Build outputs (binaries only; builds never package a release):

```text
build/carplay_hook.jar
build/unprotected/libcarplay_hook.so
build/unprotected/libcarplay_apac.so
build/maneuver_render
build/altscreen_render
```

Models are versioned in `models/`: `hssr-q6d-moises-a/hssr_model.bin` (main) and
`hssr-selection-v11/hssr_guard.bin` (vocal guard, loaded from beside the main model).

Tests: `./scripts/run_tests.sh`.

## Deployment

**Compatibility.** The patch is not limited to US, EU or CN units, nor to one MU train: it is
meant for any MHI2Q MU firmware (developed on MU1316). What matters is:

- a fully digital instrument cluster (Audi virtual cockpit); cars with an analog cluster are not
  supported;
- preferably, the latest firmware available for the unit, flashed before installing the patch.

With both in place it should almost certainly work, as long as nothing went wrong during the
install itself.

A release is assembled by hand. Stage the build outputs, the models and the `deploy/` runtime files
into `install_MoreIncredibleBash/mod/carplay/root/`, each at its on-unit path (for example
`mod/carplay/root/mnt/app/root/hooks/libcarplay_hook.so`), and put `carplay_child.json` in
`mod/carplay/`. Copy the contents of `install_MoreIncredibleBash/` onto the M.I.B. SD card, so the
card holds `mod/custom.sh` and `mod/carplay/`, then run `mod/custom.sh` from M.I.B. The
source-to-unit map, file modes and hashes are in
[Deploy and Release](<docs/build-and-deploy/DEPLOY_AND_RELEASE.md>).

- `custom.sh` copies every file under `SD:/mod/carplay/root/` to the same path under `/` on the
  unit, replaces the SI `carplay` child by path, widens the CarPlay rule in all four PF profiles
  and registers the iAP2 route-guidance message IDs in `dio_manager.json` (without them no route
  guidance arrives). It keeps a `.carplay-stock` backup of everything it replaces or edits, and
  never stops processes or reboots the HU. For a manual install of the message IDs, see the
  [procedure](<docs/build-and-deploy/DEPLOY_AND_RELEASE.md#-route-guidance-message-ids-in-dio_managerjson>).
- Uninstall: copy `uninstall_MoreIncredibleBash/` over the card and run it; it restores every
  `.carplay-stock` backup and deletes the owned files.
- The runtime integration (renderer supervision, environment, spatial/upmix settings) lives in
  [`deploy/smartphone_integrator/`](deploy/smartphone_integrator/README.md); the shipped audio
  calibration is set in `deploy/smartphone_integrator/carplay_startup.sh` and documented in
  [Stereo to 5.1 Upmix](<docs/audio/STEREO_TO_5_1_UPMIX.md>).

Disconnect CarPlay first and reboot the HU manually after installing. Deploying the Java jar always
means a full unit restart (it is on j9's boot classpath).

## References

Thanks for the prior work and knowledge that helped figure this out:

- https://github.com/ludwig-v/wireless-carplay-dongle-reverse-engineering
- https://github.com/EthanArbuckle/iPhone18-3_26.1_23B85_Restore
- https://github.com/adi961/mib2-android-auto-vc
- [@fifthBro](https://t.me/fifthBro)
