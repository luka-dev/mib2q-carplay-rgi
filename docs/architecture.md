---
title: Architecture - process topology & threading
tags: [architecture, overview, verified]
status: verified-source
sources:
  - code: hook/, java_patch/, java_resources/, maneuver_render/, deploy/smartphone_integrator/
  - code: scripts/, tests/
reconciles:
  - README.md
  - docs/ARCHITECTURE.md
  - docs/DEPLOYMENT.md
---

# Architecture - process topology & threading

The one-page overview; each subsystem has its own note. Start at [INDEX](INDEX.md).

## 📋 What the patch does

Production implementation for Audi MHI2Q (MU1316, QNX 6.5 ARMv7) - stock `libairplay.so` 210.81 kept
throughout:

- **Route guidance** - full HUD/BAP maneuver state ([rgd-activation](rgd/rgd-activation.md) - [bap-fctids](rgd/bap-fctids.md)) plus a custom
  3D maneuver overlay drawn over the native cluster map ([maneuver-renderer](cluster/maneuver-renderer.md) - [compositing](cluster/compositing.md)):
  arrow-fill distance progress ([bargraph-sync](rgd/bargraph-sync.md)), lane strip ([lane-guidance](rgd/lane-guidance.md)) and scrolling VC
  route text ([vc-route-text](rgd/vc-route-text.md)).
- **Cover art** - album art forwarded to the VC now-playing widget ([cover-art](hook/cover-art.md)).
- **Touchpad input** - MMI touchpad drag bridged to DPAD navigation ([touchpad-dpad](input/touchpad-dpad.md)); steering-wheel
  roller press toggles cluster route-info ([steering-wheel](input/steering-wheel.md)).

The maneuver overlay (`maneuver_render`, displayable 98, transparent when idle) composites over the
head unit's own native map (33): stock ctx 74 at rest, custom ctx 80 `{98,101,102,33}` only while
guidance is active (held after route end until the VC has faded its KDK out). Base CarPlay stays
byte-identical to stock.

## ⚙️ Components

| Component | Type | Output | Topic |
|---|---|---|---|
| `hook/` | C (ARM32 QNX), `LD_PRELOAD` into `dio_manager`; exports exactly 5 interposers | `libcarplay_hook.so` | [iap2-interception](hook/iap2-interception.md) [cover-art](hook/cover-art.md) [integration-seam](hook/integration-seam.md) |
| `java_patch/` + `java_resources/` | Java 1.4 class overrides loaded by the HMI (`lsd`) + VC glyph table | `carplay_hook.jar` | [rgd-activation](rgd/rgd-activation.md) [display-contexts](cluster/display-contexts.md) [touchpad-dpad](input/touchpad-dpad.md) |
| `maneuver_render/` | C EGL/GLES2 + C++11 scene engine (ARM QNX / macOS) | `maneuver_render` | [maneuver-renderer](cluster/maneuver-renderer.md) [compositing](cluster/compositing.md) |

## 🗂️ Process topology

```text
smartphone_integrator            (boot-resident; spawns on phone connect)
  +- carplay_startup.sh           exec's into dio_manager (same PID)
      = dio_manager               stock libairplay 210.81 + LD_PRELOAD hook
      +- carplay_monitor.sh       per-generation renderer monitor (no hook)
          +- maneuver_render      TCP 127.0.0.1:19800  (Java -> renderer; outlives dio)

Java patch (lsd.jxe, alive from boot)
  +- CarplayBus server            TCP 127.0.0.1:19810  (hook <-> Java)
```

`LD_PRELOAD` is scoped to `dio_manager` only. See [supervisor-lifecycle](deploy/supervisor-lifecycle.md) for ownership and
[bus-protocol](hook/bus-protocol.md) for the hook<->Java link.

## 🚀 Boot / init

`dio_manager` is **not** spawned at boot - `smartphone_integrator` launches it on phone connect, so
the hook constructor fires only then. The HMI (`lsd.jxe`) starts at boot, so the Java bus server is
already `accept()`ing when the hook connects. Connect sequence: [connect](deploy/connect.md).

## 🧭 Data flow (steady state)

```mermaid
flowchart LR
    accTitle: Steady-state data flow
    accDescr: The hook forwards iPhone iAP2 route guidance and cover art over CarplayBus to the Java patch, which drives maneuver_render and the Virtual Cockpit via BAP; the VC reports visibility and stage back, and the touchpad feeds Java.
    ip["iPhone iAP2"] --> hook["hook (dio_manager)"]
    hook -->|EVT_RGD_UPDATE| bus["CarplayBus :19810"]
    hook -->|EVT_COVERART| bus
    bus --> java["Java patch"]
    java -->|"MANEUVER / PROGRESS / LANES / VISIBLE_AREA :19800"| rend["maneuver_render -> displayable 98"]
    java -->|BAP FctIDs| vc["Virtual Cockpit"]
    vc -->|"FctID 44 visibility / 54 stage"| java
    rend -->|HU encode -> MOST| vc
    pad["MMI touchpad"] --> java
```

## 🔄 Threading (highlights)

- **hook**: iAP2 thread (recv/read hooks) - cover-art worker - bus connector/writer - 1 Hz timer.
- **Java**: HMI EDT - `carplay-bus` (bus server IO, + `carplay-bus-writer`/`carplay-bus-reader`) -
  `carplay-cluster-switch` (single DM writer, [display-contexts](cluster/display-contexts.md)) - `carplay-rgi-presentation`
  (RouteGuidance worker: retry, viewport, route-text scroll ticks) - `BAPActionBlink` ([bargraph-sync](rgd/bargraph-sync.md)) -
  `RendererServer` accept/read + writer.
- **renderer**: EGL draw loop - TCP client to Java - progress watchdog thread
  ([maneuver-renderer](cluster/maneuver-renderer.md)); no dmdt on this branch.

## 🔧 Build & deploy (quickref)

```sh
./scripts/build_java.sh        # -> build/carplay_hook.jar    (eclipse-temurin:8 Docker)
./scripts/build_hook.sh        # -> build/libcarplay_hook.so   (qnx65-armv7-toolchain Docker)
./scripts/build_renderers.sh   # -> build/maneuver_render      (qnx65-armv7-toolchain Docker)
```

All three build in Docker - no host toolchain. The `qnx65-armv7-toolchain` image is built once from
[luka-dev/qnx65-armv7-toolchain](https://github.com/luka-dev/qnx65-armv7-toolchain) (`./host-scripts/qnx-run.sh build`). No Java variants; `java_patch/` builds directly to the
jar, and `java_resources/` (the `vc-text.bin` glyph/Unicode table) is copied in before `jar cf`. The
renderer's `scene/` is C++11 compiled with the image's `g++` into `build/libmaneuver_scene.a` (no C++
runtime allowed). The native builds synthesize import stubs; the resulting ELF binds the unit's real
Screen/EGL/GLES libs at runtime. `build_hook.sh` also enforces the 5-symbol export allowlist.

## 🧪 Tests (host only, no HU)

```sh
./scripts/run_tests.sh            # C: RGD TLV parser, bus transport, signal guard, state trace, protocol constants
./scripts/test_route_info.sh      # Java BAPBridge/RouteGuidance vs stock service interface (text, maneuvers, lanes, viewport)
./scripts/test_java_transports.sh # CarplayBus / RendererServer sockets, TouchpadController
./scripts/test_maneuver_native.sh # renderer lane decoder, scene engine, maneuver parity (ASan/UBSan, macOS)
./scripts/audit_maneuvers.sh      # offline maneuver -> BAP wire export
./scripts/audit_java_stock.sh     # Java linkage against the stock jar
python3 tests/test_rgd_native_contract.py   # after build_java.sh: real C parser + slot writer -> Java -> stock BAP sender
```

The Java suites run in Docker (no host JDK) against the MU1316 stock jar and OSGi libraries under
`../../Tools/jxe2jar`; the README lists the variables that point them elsewhere.

Deploy by copying the runtime files to `/mnt/app/root/hooks/`, pointing `smartphone_integrator.json`
at `carplay_child.json`, and dropping `carplay_hook.jar` into `/mnt/app/eso/hmi/lsd/jars/`.
Separately, the RGD message IDs (0x5200/0x5203 sent, 0x5201/0x5202/0x5204 received) must be registered
in `dio_manager.json` (`MessagesSentByAccessory` / `MessagesReceivedFromDevice`). Then reboot - there
is no one-shot flasher. Runtime integration + install paths: [supervisor-lifecycle](deploy/supervisor-lifecycle.md).

## 📚 Reverse-engineering references

iOS: [accessoryd-rgd](re/ios/accessoryd-rgd.md) - [carkitd-bonjour](re/ios/carkitd-bonjour.md) - [maps-maneuvers](re/ios/maps-maneuvers.md). Firmware:
[display-manager](re/firmware/display-manager.md) - [komo-widget-video](re/firmware/komo-widget-video.md) - [dsi-carkombi](re/firmware/dsi-carkombi.md).
