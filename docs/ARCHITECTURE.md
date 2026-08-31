# Current architecture

This document describes the shipping MHI2Q implementation. Historical plans and alternative
libairplay experiments live under `research/` and `docs/archive/`.

## Process topology

```text
smartphone_integrator
  └─ carplay_startup.sh
      ├─ maneuver_render       TCP 127.0.0.1:19800
      ├─ altscreen_render      data :19831, status :19832
      └─ dio_manager           stock libairplay 210.81 + LD_PRELOAD hook

Java patch
  └─ CarplayBus server         TCP 127.0.0.1:19810
```

`LD_PRELOAD` is applied only to `dio_manager`. The supervisor and renderers explicitly run without
it. When `dio_manager` exits, the owner-generation monitor stops the exact renderer generation unless
a replacement `dio_manager` has already taken ownership.

## Native hook

`hook/main.c` initializes the production framework bus, iAP2 helpers, route-guidance parser,
cover-art path and altScreen receiver. The stream-111 H.264 path is self-contained in the hook and
forwards Annex-B access units to `altscreen_render` on port 19831. Stock `libairplay.so` remains
installed and loaded.

## Java patch

There is exactly one source tree: `java_patch/`. `CarPlayApp` keeps the transport alive and starts an
ordered module chain:

1. `AltScreenModule` owns the cluster display context and steering-wheel map zoom.
2. `RgdModule` starts after the first module is ready, then publishes RGI/BAP maneuver state.

Patched stock classes provide lifecycle seams, cluster contexts, cover-art integration and input
interception. The right-hand pure OPS popup (`2100008`) has a narrow `SMALL_STAGE` exception: CarPlay
keeps `Resource.SCREEN`, while every ordinary HMI deactivation and fullscreen camera takeover stays
stock. They are compiled against `MU1316-combined-final.jar` as Java 1.4 bytecode.

## Cluster composition

On the supported A5-style cluster, the Java patch adds two contexts:

| Context | Displayables | Meaning |
| --- | --- | --- |
| 80 | `98, 101, 102, 99` | CarPlay map plus maneuver/backing layers |
| 81 | `99` | Plain CarPlay map |

Displayable 98 belongs to `maneuver_render`; displayable 99 belongs to `altscreen_render`. Context 81
is selected as soon as the CarPlay cluster is owned. The switch to context 80 happens only after RGI
presentation is confirmed; it returns to 81 when guidance/presentation ends. `AltScreenModule`
serializes all context writes.

The A5 context table also declares main-terminal context 90 as `{16,43}` for RetroArch: HMI overlays
and partial popups remain above its video displayable. G24 is excluded because stock already uses ID
90 for its generated `11 + 79` KDK context.

That context is declared here rather than in RetroArch's own repository
(`luka-dev/audi-mib2q-retroarch`, working tree `Tools/retroarch-qnx`) because there is nowhere else
it can go: `DisplayManagerMIB2High` is a patched stock class and only `carplay_hook.jar` carries it.
RetroArch's `java_patch/java_src/` patches different stock classes and builds a separate
`ra_mhi2q.jar`; both land in `/mnt/app/eso/hmi/lsd/jars/`, so they coexist rather than overwrite —
but a stock class may only be patched by one of them. **RetroArch therefore depends on a context
defined in this repository.** Anyone rebuilding this jar without context 90 breaks it, and anyone
adding a `DisplayManagerMIB2High` patch to the RetroArch tree creates a duplicate of a class that
already exists in another jar in the same directory.

## Local protocols

| Port | Direction | Payload |
| --- | --- | --- |
| 19800 | Java → `maneuver_render` | fixed maneuver/bargraph commands |
| 19810 | hook ↔ Java | sticky RGD events and `CMD_ALT_*` controls |
| 19831 | hook → `altscreen_render` | framed Annex-B H.264 |
| 19832 | `altscreen_render` → Java | video on/off status |

## Build outputs

The only supported artifacts are:

```text
build/carplay_hook.jar
build/libcarplay_hook.so
build/maneuver_render
build/altscreen_render
```

Build them with the scripts in `scripts/`; variant and probe builds no longer exist.
