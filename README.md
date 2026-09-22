# MHI2Q CarPlay cluster integration

Production implementation for Audi MHI2Q (MU1316, QNX 6.5 ARMv7):

- stock `libairplay.so` 210.81 inside `dio_manager`;
- `LD_PRELOAD` hook for CarPlay negotiation, route guidance, altScreen stream 111 and the
  buffered/upmixed 5.1 audio path;
- external QNX renderers for maneuver graphics and cluster video;
- one Java patch for cluster context, BAP/RGI, cover art and steering-wheel input.

The shipped path always keeps stock `libairplay.so`. From-source libairplay/dio experiments are
research records, not production dependencies or authorities for stock 210 behavior.

## Documentation

All documentation is one Obsidian vault in [`docs/`](docs/Home.md). Start at
[`docs/Home.md`](docs/Home.md), then the per-domain maps:

| Domain | Entry point |
| --- | --- |
| Audio (upmix, HSSR model, J525 amp, codecs, 5.1 routing) | [`docs/Audio/Audio MOC.md`](<docs/Audio/Audio MOC.md>) |
| Cluster and route guidance (contexts, BAP/RGI, HUD renderer) | [`docs/Cluster and RGI/Cluster and RGI MOC.md`](<docs/Cluster and RGI/Cluster and RGI MOC.md>) |
| AltScreen and video (stream 111, OMX pipeline, loader) | [`docs/AltScreen and Video/AltScreen and Video MOC.md`](<docs/AltScreen and Video/AltScreen and Video MOC.md>) |
| Connect and USB (NCM, replug recovery, PF, session lifecycle) | [`docs/Connect and USB/Connect and USB MOC.md`](<docs/Connect and USB/Connect and USB MOC.md>) |
| Build and deploy (toolchain, release map, VIN protection) | [`docs/Build and Deploy/Build and Deploy MOC.md`](<docs/Build and Deploy/Build and Deploy MOC.md>) |
| Architecture overview | [`docs/Reference/Architecture.md`](docs/Reference/Architecture.md) |

Read [`docs/Landmines.md`](docs/Landmines.md) before touching a subsystem. Long-form white papers
are in `docs/Research/`; primary evidence (papers, manuals, data files, decompiled stock Java) is in
`docs/Sources/`.

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
| `docs/` | The documentation vault |
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

A release is assembled by hand: copy the build outputs, the models and the `deploy/` runtime files
into `install_MoreIncredibleBash/mod/carplay/root/` at their on-unit paths, then run
`mod/custom.sh` from the M.I.B. SD card. The source-to-unit map, file modes and hashes are in
[Deploy and Release](<docs/Build and Deploy/Deploy and Release.md>).

- `custom.sh` copies the `root/` tree onto the unit, replaces the SI `carplay` child by path and
  widens the CarPlay rule in all four PF profiles, keeping a `.carplay-stock` backup of everything
  it replaces. It never stops processes or reboots the HU.
- Uninstall: copy `uninstall_MoreIncredibleBash/` over the card and run it; it restores every
  `.carplay-stock` backup and deletes the owned files.
- The runtime integration (renderer supervision, environment, spatial/upmix settings) lives in
  [`deploy/smartphone_integrator/`](deploy/smartphone_integrator/README.md); the shipped audio
  calibration is set in `deploy/smartphone_integrator/carplay_startup.sh` and documented in
  [Stereo to 5.1 Upmix](<docs/Audio/Stereo to 5.1 Upmix.md>).

Disconnect CarPlay first and reboot the HU manually after installing. Deploying the Java jar always
means a full unit restart (it is on j9's boot classpath).
