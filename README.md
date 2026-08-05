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

Raw unit logs and generated class trees are intentionally kept outside Git. The cleanup archive is
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

To replace only the native hook and restart `dio_manager`:

```sh
./scripts/deploy_unit.sh
```

The deploy helper keeps one on-unit `libcarplay_hook.so.bak`; it does not accumulate timestamped backup
copies. Java deployment is deliberately not automated by that script because restarting the Java stack
on a live HU is unsafe.

## Current design documents

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — current process, Java and cluster architecture.
- [`docs/DEPLOYMENT.md`](docs/DEPLOYMENT.md) — renderer/runtime deployment.
- [`docs/reference/JAVA_PATCH_STOCK_AUDIT.md`](docs/reference/JAVA_PATCH_STOCK_AUDIT.md) — patched-versus-stock audit.
- [`docs/reference/ALTSCREEN_VC_NAV_STATUSBAR.md`](docs/reference/ALTSCREEN_VC_NAV_STATUSBAR.md) — VC status-bar findings.

Historical altScreen negotiation reports and from-source experiments are under `research/altscreen/`;
they are useful research records, not the definition of the shipping architecture.
