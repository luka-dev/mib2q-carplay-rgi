# Current architecture

This document describes the shipping MHI2Q implementation. Historical plans and alternative
libairplay experiments live under `research/` and `docs/archive/`.

## Process topology

```text
smartphone_integrator
  └─ carplay_startup.sh → exec dio_manager   stock libairplay 210.81 + LD_PRELOAD hook
                            └─ carplay_monitor.sh
                                ├─ maneuver_render   TCP 127.0.0.1:19800
                                └─ altscreen_render  data :19831, status :19832

Java patch
  └─ CarplayBus server         TCP 127.0.0.1:19810
```

`LD_PRELOAD` is applied only to `dio_manager`. The monitor and renderers explicitly run without it.
The startup shell immediately becomes `dio_manager`; the generation monitor starts one copy of each
persistent renderer, exits when its dio PID disappears or is superseded, and leaves healthy renderers
for the next generation to adopt.

## Native hook

`hook/main.c` is the module table and nothing else. The framework registers what stands there, in
that order, at the first real Cinemo boundary, and tears it down in reverse from its destructor;
there is no ELF constructor. The stream-111 H.264 path is self-contained in the hook and forwards
Annex-B access units to `altscreen_render` on port 19831. Stock `libairplay.so` remains installed
and loaded.

Every module declares what it needs in one `hook_module_def_t` next to its own code:

| field | seam |
| --- | --- |
| `on_init` / `on_shutdown` | framework lifecycle, reverse-order teardown |
| `on_identify` | patch the outgoing Identify |
| `on_message` + `msg_filter` | incoming iAP2 messages |
| `on_state`, `on_transport_send` | session edges, outgoing transport frames |
| `on_transport_recv` / `_reset` | raw `NmeTransport::Recv` bytes, Identify boundary |
| `airplay.*` | AirPlay SETUP request/response, `/info`, TearDown |

The three AirPlay entry points wanted by more than one module — `AirPlayReceiverSessionSetup`,
`AirPlayCopyServerInfo`, `AirPlayReceiverSessionTearDown` — are interposed once, by the framework
(`hook/framework/airplay_seams.c`), and fan out to subscribers in module priority order. An ELF
symbol has one definition per object, so before this the altScreen module owned all three and
called the audio module's type-103 negotiation on its behalf. Single-consumer interpositions
(`SetNightMode`, `AirPlayReceiverServerPlatformCopyProperty`, the QSA writers) stay in the module
that uses them.

| module | priority | seams used |
| --- | --- | --- |
| `routeguidance` | normal | Identify, 0x52xx messages, state, transport send |
| `coverart` | low | transport recv + reset |
| `altscreen` | normal | AirPlay setup response, `/info`, teardown |
| `audio` | high | AirPlay setup request/response (+final), `/info` (+final), teardown |

## Java patch

There is exactly one source tree: `java_patch/`. `CarPlayApp` keeps the transport alive and starts an
ordered module chain:

1. `AltScreenModule` owns the cluster display context and steering-wheel map zoom.
2. `RgdModule` starts after the first module is ready, then publishes RGI/BAP maneuver state.

Patched stock classes provide lifecycle seams, cluster contexts, cover-art integration and input
interception. The right-hand pure OPS popup (`2100008`) has a narrow `SMALL_STAGE` exception: CarPlay
keeps `Resource.SCREEN`, while every ordinary HMI deactivation and fullscreen camera takeover stays
stock. They are compiled against `MU1316-final.jar` as Java 1.4 bytecode.

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

Main-terminal context 90 for RetroArch is deliberately NOT declared here. It was, briefly, and was
removed: `DisplayManagerMIB2High` is a patched stock class carried only by `carplay_hook.jar`, so
declaring it here made RetroArch depend on a build of this repository with nothing on either side to
enforce it. RetroArch owns that context from its own repository instead
(`luka-dev/audi-mib2q-retroarch`, `docs/DISPLAY_CONTEXT_90.md`), by extending the table at runtime
the way its `RuntimeSmmInjector` already extends the state machine.

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
