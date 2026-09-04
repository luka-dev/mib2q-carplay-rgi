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

Canonical outputs:

```text
build/carplay_hook.jar
build/libcarplay_hook.so
build/libcarplay_apac.so
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

The deploy helper stages both native audio artifacts through `.new` files and does not accumulate
stale backup copies. Java deployment is deliberately not automated by that script because restarting
the Java stack on a live HU is unsafe.

## Current design documents

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — current process, Java and cluster architecture.
- [`docs/DEPLOYMENT.md`](docs/DEPLOYMENT.md) — renderer/runtime deployment.
- [`docs/archive/JAVA_PATCH_STOCK_AUDIT.md`](docs/archive/JAVA_PATCH_STOCK_AUDIT.md) — patched-versus-stock audit (archived; superseded by JAVA_PRODUCTION_CLEANUP_AUDIT.md).
- [`docs/reference/ALTSCREEN_VC_NAV_STATUSBAR.md`](docs/reference/ALTSCREEN_VC_NAV_STATUSBAR.md) — VC status-bar findings.
- [`docs/reference/CLUSTER_PRESENT_CADENCE.md`](docs/reference/CLUSTER_PRESENT_CADENCE.md) — cluster swap-interval doctrine: why a nominally 30 fps chain judders, and the QNX/Screen platform traps behind it.
- [`docs/HANDOFF_SPATIAL_AUDIO.md`](docs/HANDOFF_SPATIAL_AUDIO.md) — CarPlay discrete 5.1 handoff; §10 has the type-103 five-second-teardown root cause.
- [`docs/AUDIO_5_1_STATE.md`](docs/AUDIO_5_1_STATE.md) — current audio evidence and closed hypotheses.
- [`docs/AAC_BUFFERED_PATH_PLAN.md`](docs/AAC_BUFFERED_PATH_PLAN.md) — measured index-23 framing/cookie, exact Cinemo ABI and the ordinary-content AAC implementation plan.
- [`docs/CINEMO_AAC_THREAD_SAFETY_AUDIT.md`](docs/CINEMO_AAC_THREAD_SAFETY_AUDIT.md) — MHI2Q codec-object, class-registry, callback and OMX coexistence proof; FDK-AAC is rollback only.
- [`docs/DYNAMIC_BUFFERED_CODEC_SWITCH.md`](docs/DYNAMIC_BUFFERED_CODEC_SWITCH.md) — iOS in-session APAC/AAC selection, packet-boundary receiver state machine and decisive vehicle test.
- [`deploy/deva-ctrl-qc/BUILD_CHAIN.md`](deploy/deva-ctrl-qc/BUILD_CHAIN.md) — vehicle-confirmed deva fix and exact deployable build chain.
- [`docs/ROUTER_INPUT_WIDTH_AUDIT.md`](docs/ROUTER_INPUT_WIDTH_AUDIT.md) — historical root cause of the now-fixed MPL5 render-width defect.
- [`docs/MC_SELECTION_AUDIT.md`](docs/MC_SELECTION_AUDIT.md) — connection-162 MC selection and its `Sound_System` coding source.
- [`docs/SHARC_INPUT_OUTPUT_MATRIX.md`](docs/SHARC_INPUT_OUTPUT_MATRIX.md) — recovered A07 8×8 SHARC input mixer, selection logic, coefficient addresses and the remaining physical-output boundary.
- [`docs/MOST_SOURCE_BLOCKWIDTH_AUDIT.md`](docs/MOST_SOURCE_BLOCKWIDTH_AUDIT.md) — connection-162 source allocation and actual MOST Connect BlockWidth.
- [`docs/CONNECT_BLOCKWIDTH_VALIDATION_AUDIT.md`](docs/CONNECT_BLOCKWIDTH_VALIDATION_AUDIT.md) — amplifier Connect validation and exact HU BlockWidth constant sites.

Historical altScreen negotiation reports and from-source experiments are under `research/altscreen/`;
they are useful research records, not the definition of the shipping architecture.
