# MHI2Q CarPlay cluster integration

Production implementation for Audi MHI2Q (MU1316, QNX 6.5 ARMv7):

- stock `libairplay.so` 210.81 inside `dio_manager`;
- `LD_PRELOAD` hook for CarPlay negotiation, route guidance and altScreen stream 111;
- external QNX renderers for maneuver graphics and cluster video;
- one Java patch for cluster context, BAP/RGI, cover art and steering-wheel input.

The shipped path always keeps stock `libairplay.so`. Historical alternative implementations under
`research/` are not production dependencies or authorities for stock 210 behavior.

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
| `toolchain/qnx65-abi/` | QNX Screen/OpenMAX ABI headers used only for cross-compilation |
| `docs/` | Current architecture, deployment and audit documents |
| `docs/reference/` | Reverse-engineering reference notes |
| `docs/archive/` | Superseded plans retained for history |
| `research/` | Historical experiments, disassembly notes and old reports |
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
build/libcarplay_hook.so
build/libcarplay_apac.so
build/hssr-selection-v11/hssr_model.bin
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

Whole install from the M.I.B. SD card — runtime files, jar, child config and PF ports:
see [`deploy/card/README.md`](deploy/card/README.md).

To replace only the native hook and restart `dio_manager` over ssh:

```sh
./scripts/deploy_over_ssh.sh
```

The deploy helper stages both native audio artifacts as `.new`, atomically
renames them over the live paths, syncs, and restarts `dio_manager`; it does
not accumulate stale backup copies.  Staging is required because QNX refuses a
direct scp overwrite while a shared object is mapped.  Java deployment is
deliberately not automated by that script because restarting the Java stack on
a live HU is unsafe.

For a quiet pre-production native artifact, build and deploy with:

```sh
LOG=0 ./scripts/build_hook.sh
./scripts/deploy_over_ssh.sh
```

`LOG=0` compiles out the logger thread and all `LOG_*` calls.  It also compiles
out the historical audio probe captures (APAP, RTCP, setup/control plists,
magic cookies, elementary streams and PCM taps); negotiation, decoding,
rendering and feedback-clock behavior are unchanged.

For the narrow startup/metadata discriminator, keep the ordinary quiet build
but enable only the compact state ledger:

```sh
LOG=0 AUDIO_STATE_TRACE=1 ./scripts/build_hook.sh
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

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — current process, Java and cluster architecture.
- [`docs/RGI_HUD_RENDERER_KNOWLEDGE.md`](docs/RGI_HUD_RENDERER_KNOWLEDGE.md) — consolidated RGI/HUD/renderer handoff: accepted visuals, lane events, sentinels, synchronization, delivery recovery, tests and current deployment status.
- [`docs/DEPLOYMENT.md`](docs/DEPLOYMENT.md) — renderer/runtime deployment.
- [`docs/USB_REPLUG_RECOVERY_RUNBOOK.md`](docs/USB_REPLUG_RECOVERY_RUNBOOK.md) — full MU1316 rapid-replug diagnosis, independent NCM/SI fixes, live A/B gates and exact rollback.
- [`docs/archive/JAVA_PATCH_STOCK_AUDIT.md`](docs/archive/JAVA_PATCH_STOCK_AUDIT.md) — patched-versus-stock audit (archived; superseded by JAVA_PRODUCTION_CLEANUP_AUDIT.md).
- [`docs/reference/ALTSCREEN_VC_NAV_STATUSBAR.md`](docs/reference/ALTSCREEN_VC_NAV_STATUSBAR.md) — VC status-bar findings.
- [`docs/reference/CLUSTER_PRESENT_CADENCE.md`](docs/reference/CLUSTER_PRESENT_CADENCE.md) — cluster swap-interval doctrine: why a nominally 30 fps chain judders, and the QNX/Screen platform traps behind it.
- [`docs/HANDOFF_SPATIAL_AUDIO.md`](docs/HANDOFF_SPATIAL_AUDIO.md) — CarPlay discrete 5.1 handoff; §10 has the type-103 five-second-teardown root cause.
- [`docs/AUDIO_5_1_STATE.md`](docs/AUDIO_5_1_STATE.md) — current audio evidence and closed hypotheses.
- [`docs/AAC_BUFFERED_PATH_PLAN.md`](docs/AAC_BUFFERED_PATH_PLAN.md) — measured index-23 framing/cookie, exact Cinemo ABI and the implemented ordinary-content AAC path.
- [`docs/CINEMO_AAC_THREAD_SAFETY_AUDIT.md`](docs/CINEMO_AAC_THREAD_SAFETY_AUDIT.md) — MHI2Q codec-object, class-registry, callback and OMX coexistence proof; FDK-AAC is rollback only.
- [`docs/DYNAMIC_BUFFERED_CODEC_SWITCH.md`](docs/DYNAMIC_BUFFERED_CODEC_SWITCH.md) — iOS in-session APAC/AAC selection, packet-boundary receiver state machine and decisive vehicle test.
- [`docs/BUFFERED_BACKGROUND_LIFECYCLE.md`](docs/BUFFERED_BACKGROUND_LIFECYCLE.md) — measured phone-lock/app-background ownership loss, same-live-handle handoff, detached-QSA scheduler priority, and the mixed `[65,23]` buffer contract needed for in-session AAC/APAC changes.
- [`docs/reference/IOS_26_1_BUFFERED_AUDIO_LIFECYCLE_AUDIT.md`](docs/reference/IOS_26_1_BUFFERED_AUDIO_LIFECYCLE_AUDIT.md) — exact iOS 26.1/Pioneer type-103 lifecycle contract and its implemented repair: stopped SETUP queue, lossless ordered `rate=0→1`, exact sequence/RTP FLUSH across compressed and PCM queues, concurrency hardening, tests and the remaining on-unit anchor gate.
- [`docs/reference/NOW_PLAYING_STARTUP_STATE_MACHINE_AUDIT.md`](docs/reference/NOW_PLAYING_STARTUP_STATE_MACHINE_AUDIT.md) — exact Pioneer/iOS 27 Now Playing, XPC and AirPlay routing-context state machines for blank-on-connect and stale-metadata failures, plus the passive discriminator required before any retry/rebind change.
- [`deploy/deva-ctrl-qc/BUILD_CHAIN.md`](deploy/deva-ctrl-qc/BUILD_CHAIN.md) — vehicle-confirmed deva fix and exact deployable build chain.
- [`docs/ROUTER_INPUT_WIDTH_AUDIT.md`](docs/ROUTER_INPUT_WIDTH_AUDIT.md) — historical root cause of the now-fixed MPL5 render-width defect.
- [`docs/MC_SELECTION_AUDIT.md`](docs/MC_SELECTION_AUDIT.md) — connection-162 MC selection and its `Sound_System` coding source.
- [`docs/SHARC_INPUT_OUTPUT_MATRIX.md`](docs/SHARC_INPUT_OUTPUT_MATRIX.md) — recovered A07 8×8 SHARC input mixer, selection logic, coefficient addresses and the remaining physical-output boundary.
- [`docs/SHARC_APPLICATION_EXTRACTION.md`](docs/SHARC_APPLICATION_EXTRACTION.md) — checked extraction of the embedded J525 ADSP-2148x SPI boot/application stream, hashes and loader-block validation.
- [`docs/STEREO_TO_5_1_UPMIX_DESIGN.md`](docs/STEREO_TO_5_1_UPMIX_DESIGN.md) — amplifier-aware HSSR Universal design, fixed PCM6/LFE bounds, model policy and verification gates.
- [`tools/stereo-upmix-render/`](tools/stereo-upmix-render/README.md) — host WAV renderer, authored/stereo-stress evaluation, teacher-stem routing checks and performance benchmarks for the exact shipping Universal DSP.
- [`docs/MOST_SOURCE_BLOCKWIDTH_AUDIT.md`](docs/MOST_SOURCE_BLOCKWIDTH_AUDIT.md) — connection-162 source allocation and actual MOST Connect BlockWidth.
- [`docs/CONNECT_BLOCKWIDTH_VALIDATION_AUDIT.md`](docs/CONNECT_BLOCKWIDTH_VALIDATION_AUDIT.md) — amplifier Connect validation and exact HU BlockWidth constant sites.

Historical altScreen negotiation reports and from-source experiments are under `research/altscreen/`;
they are useful research records, not the definition of the shipping architecture.
