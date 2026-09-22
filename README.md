# MHI2Q CarPlay cluster integration

Production implementation for Audi MHI2Q (MU1316, QNX 6.5 ARMv7):

- stock `libairplay.so` 210.81 inside `dio_manager`;
- `LD_PRELOAD` hook for CarPlay negotiation, route guidance and altScreen stream 111;
- external QNX renderers for maneuver graphics and cluster video;
- one Java patch for cluster context, BAP/RGI, cover art and steering-wheel input.

The shipped path always keeps stock `libairplay.so`. Historical alternative implementations under
`docs/Sources/research/` are not production dependencies or authorities for stock 210 behavior.

## Repository layout

| Path | Purpose |
| --- | --- |
| `hook/` | Shipping native `libcarplay_hook.so` source |
| `java_patch/` | The only supported Java patch source |
| `maneuver_render/` | GLES maneuver overlay renderer |
| `altscreen_render/` | OMX/GLES altScreen video renderer |
| `common/` | Shared QNX Screen surface code |
| `deploy/smartphone_integrator/` | Runtime scripts and child-process configuration for the HU |
| `scripts/` | Host build and deployment entry points |
| `install_MoreIncredibleBash/`, `uninstall_MoreIncredibleBash/` | M.I.B. SD install/remove entry points |
| `toolchain/qnx65-abi/` | QNX Screen/OpenMAX ABI headers used only for cross-compilation |
| `docs/` | The documentation: an Obsidian vault — start at `docs/Home.md` (domain notes, `Landmines.md`) |
| `docs/Research/` | Long-form white papers (upmix, HSSR model, Virtual Cabin, Dolby teardown) |
| `docs/Sources/` | Primary evidence kept verbatim: papers, manuals, wiring images, data files, raw tables, decompiled stock Java |
| `logs/` | Ignored local evidence: vehicle captures (`captures/`), HU logs, iPhone sysdiagnose, state traces |
| `assets/` | Screenshots and visual reference material |
| `build/` | Canonical deployable artifacts |

Build output, raw unit logs and generated class trees are intentionally kept outside Git;
`build/` is reproduced by the three scripts below. The cleanup archive is
under `/Users/luka/Desktop/AUDI_2/LocalLogs/`.

## Build

Run from the repository root:

```sh
./scripts/build_java.sh
./scripts/build_hook.sh
./scripts/build_renderers.sh
```

The hook can also be built with the independently guarded QNX GCC 8.5 image:

```sh
QNX_TOOLCHAIN_VARIANT=8.5 ./scripts/build_hook.sh
```

The vehicle has one runtime stereo-upmix toggle:
`AUDIO_STEREO_UPMIX=on|off`.  ON always selects the sole HSSR Universal
renderer; OFF is exact `L,R,0,0,0,0`.  There is no build-time profile selector.

Canonical outputs:

```text
build/carplay_hook.jar
build/unprotected/libcarplay_hook.so
build/unprotected/libcarplay_apac.so
models/hssr-selection-v11/hssr_guard.bin
build/maneuver_render
build/altscreen_render
```

There are no Java variants. `java_patch/` builds directly to `build/carplay_hook.jar`.

The native builds use the `qnx65-armv7-toolchain` Docker image. Renderer import libraries are generated
during the build; the resulting ELF resolves the real Screen/EGL/GLES/OpenMAX libraries on the unit.

## Deployment

The runtime integration lives in [`deploy/smartphone_integrator/`](deploy/smartphone_integrator/README.md).
It starts the two renderers around the lifetime of `dio_manager`, does not restart Java, and performs
bounded cleanup when `dio_manager` exits.

SD installation/removal scripts live in
[`install_MoreIncredibleBash/`](install_MoreIncredibleBash/) (see [Deploy and Release](<docs/Build and Deploy/Deploy and Release.md>)) and
[`uninstall_MoreIncredibleBash/`](uninstall_MoreIncredibleBash/README_CARPLAY.md).
Compiled outputs and the copied runtime/firmware artifacts stay in `build/`.
One `mod/custom.sh` verifies the package and firmware, refuses an active CarPlay
session, remounts the partitions, stages runtime files and atomically replaces
the SI child configuration. It never stops processes or reboots the HU.

Prepare loose release artifacts without recompiling the existing build or creating a ZIP:

```sh
python3 scripts/package_mib.py
```

Copy the selected folder’s contents over the M.I.B. SD1 card root, and place
the release artifacts in `/build/` on the card. The removal script needs no
build files. See [the artifact list](<docs/Build and Deploy/Deploy and Release.md>).

Or upload the release allowlist and run the same installer over SSH:

```sh
./scripts/deploy_over_ssh.sh root@10.173.189.1
```

Disconnect CarPlay first. Reboot manually after installation. SSH uses your
keys/agent, or `SSHPASS` with `sshpass`; credentials are not embedded in the script.
For a quiet production native artifact, use `LOG=0 ./scripts/build_hook.sh`
before preparing the release artifacts.

`LOG=0` compiles out the logger thread and all `LOG_*` calls.  It also compiles
out the historical audio probe captures (APAP, RTCP, setup/control plists,
magic cookies, elementary streams and PCM taps); negotiation, decoding,
rendering and feedback-clock behavior are unchanged.

For the narrow startup/metadata discriminator, keep the ordinary quiet build
but enable only the compact state ledger:

```sh
LOG=0 AUDIO_STATE_TRACE=1 ./scripts/build_hook.sh
python3 scripts/package_mib.py
./scripts/deploy_over_ssh.sh
./scripts/carplay_state_trace.sh clear
# reproduce once, then:
./scripts/carplay_state_trace.sh pull
```

This writes `/tmp/carplay_state_trace.log` without payload dumps. Events are
grouped by both `dio_manager` PID and physical-Identify generation, include a
bounded HID Play/Next/touch delivery seam, and cover iAP2, AirPlay, decoder and
QSA lifecycle boundaries. The pull
helper stores and classifies it by process and physical iAP2 generation; see the Now
Playing state-machine audit below for the exact event and decision tables.

## Current design documents

- [`docs/Reference/Architecture.md`](docs/Reference/Architecture.md) — current process, Java and cluster architecture.
- [`docs/Cluster and RGI/HUD Renderer.md`](<docs/Cluster and RGI/HUD Renderer.md>) — consolidated RGI/HUD/renderer handoff: accepted visuals, lane events, sentinels, synchronization, delivery recovery, tests and current deployment status.
- [`docs/Build and Deploy/Deploy and Release.md`](<docs/Build and Deploy/Deploy and Release.md>) — renderer/runtime deployment.
- [`docs/Sources/USB_REPLUG_RECOVERY_RUNBOOK.md`](docs/Sources/USB_REPLUG_RECOVERY_RUNBOOK.md) — full MU1316 rapid-replug diagnosis, independent NCM/SI fixes, live A/B gates and exact rollback.
- [`docs/Build and Deploy/Production Audits.md`](<docs/Build and Deploy/Production Audits.md>) — patched-versus-stock audit (archived; superseded by JAVA_PRODUCTION_CLEANUP_AUDIT.md).
- [`docs/AltScreen and Video/AltScreen Cluster Video.md`](<docs/AltScreen and Video/AltScreen Cluster Video.md>) — VC status-bar findings.
- [`docs/AltScreen and Video/AltScreen Frame Rate.md`](<docs/AltScreen and Video/AltScreen Frame Rate.md>) — cluster swap-interval doctrine: why a nominally 30 fps chain judders, and the QNX/Screen platform traps behind it.
- [`docs/Audio/HSSR Model.md`](<docs/Audio/HSSR Model.md>) — provenance of the shipped HSSR control models, the datasets (MoisesDB, MUSDB18, AlbumDB, Amvizo, HiRez, private corpus) and the teacher separators used for labelling.
- [`docs/Audio/Buffered Audio Path.md`](<docs/Audio/Buffered Audio Path.md>) — CarPlay discrete 5.1 handoff; §10 has the type-103 five-second-teardown root cause.
- [`docs/Audio/Discrete 5.1 Routing.md`](<docs/Audio/Discrete 5.1 Routing.md>) — current audio evidence and closed hypotheses.
- [`docs/Audio/Cinemo AAC Decoder.md`](<docs/Audio/Cinemo AAC Decoder.md>) — measured index-23 framing/cookie, exact Cinemo ABI and the implemented ordinary-content AAC path.
- [`docs/Audio/Cinemo AAC Decoder.md`](<docs/Audio/Cinemo AAC Decoder.md>) — MHI2Q codec-object, class-registry, callback and OMX coexistence proof; FDK-AAC is rollback only.
- [`docs/Audio/Buffered Audio Path.md`](<docs/Audio/Buffered Audio Path.md>) — iOS in-session APAC/AAC selection, packet-boundary receiver state machine and decisive vehicle test.
- [`docs/Audio/Buffered Playback Lifecycle.md`](<docs/Audio/Buffered Playback Lifecycle.md>) — measured phone-lock/app-background ownership loss, same-live-handle handoff, detached-QSA scheduler priority, and the mixed `[65,23]` buffer contract needed for in-session AAC/APAC changes.
- [`docs/Audio/Buffered Playback Lifecycle.md`](<docs/Audio/Buffered Playback Lifecycle.md>) — exact iOS 26.1/Pioneer type-103 lifecycle contract and its implemented repair: stopped SETUP queue, lossless ordered `rate=0→1`, exact sequence/RTP FLUSH across compressed and PCM queues, concurrency hardening, tests and the remaining on-unit anchor gate.
- [`docs/Connect and USB/Now Playing Cold Start.md`](<docs/Connect and USB/Now Playing Cold Start.md>) — exact Pioneer/iOS 27 Now Playing, XPC and AirPlay routing-context state machines for blank-on-connect and stale-metadata failures, plus the passive discriminator required before any retry/rebind change.
- [`deploy/deva-ctrl-qc/BUILD_CHAIN.md`](deploy/deva-ctrl-qc/BUILD_CHAIN.md) — vehicle-confirmed deva fix and exact deployable build chain.
- [`docs/Audio/QNX Audio Topology.md`](<docs/Audio/QNX Audio Topology.md>) — historical root cause of the now-fixed MPL5 render-width defect.
- [`docs/Audio/Internal Amp Path.md`](<docs/Audio/Internal Amp Path.md>) — connection-162 MC selection and its `Sound_System` coding source.
- [`docs/Audio/Virtual Cabin and Amp RE.md`](<docs/Audio/Virtual Cabin and Amp RE.md>) — recovered A07 8×8 SHARC input mixer, selection logic, coefficient addresses and the remaining physical-output boundary.
- [`docs/Audio/SHARC DSP Blocks.md`](<docs/Audio/SHARC DSP Blocks.md>) — checked extraction of the embedded J525 ADSP-2148x SPI boot/application stream, hashes and loader-block validation.
- [`docs/Audio/Stereo to 5.1 Upmix.md`](<docs/Audio/Stereo to 5.1 Upmix.md>) — amplifier-aware HSSR Universal design, fixed PCM6/LFE bounds, model policy and verification gates.
- [`tools/stereo-upmix-render/`](tools/stereo-upmix-render/README.md) — host WAV renderer, authored/stereo-stress evaluation, teacher-stem routing checks and performance benchmarks for the exact shipping Universal DSP.
- [`docs/Audio/QNX Audio Topology.md`](<docs/Audio/QNX Audio Topology.md>) — connection-162 source allocation and actual MOST Connect BlockWidth.
- [`docs/Audio/QNX Audio Topology.md`](<docs/Audio/QNX Audio Topology.md>) — amplifier Connect validation and exact HU BlockWidth constant sites.

Historical altScreen negotiation reports and from-source experiments are under `docs/Sources/research/altscreen/`;
they are useful research records, not the definition of the shipping architecture.
