# MHI2 CarPlay Patch

https://archive.org/details/qnxsdp-65.7z



CarPlay feature patches for Audi MHI2 (MHI2Q) infotainment - route guidance on HUD/Virtual Cockpit, cover art on cluster, and video keep-alive.

## Build Artifacts

| File | Description |
|------|-------------|
| `libcarplay_hook.so` | C hook library (ARM32 QNX). Route guidance PPS bridge, cover art file watcher, iAP2 protocol hooks. |
| `carplay_hook.jar` | Java patch JAR. BAP bridge, KOMO/DSI maneuver icons, cover art push, TerminalMode hooks. |
| `komoviewstyle.conf` | Patched PresentationController config. Changes FPK style 3 from KVS_FPK to KVS_Most so the VC sidebar renders from DSI RouteInfoElement data instead of expecting native LVDS. |
| `maneuver_render/build/carplay_vmaneuver_render_egl` | EGL render binary for VC video (ARM32 QNX). |

## Device Paths

### C hook library

```
Device:  /mnt/app/root/hooks/libcarplay_hook.so
```

Loaded via `LD_PRELOAD` in smartphone_integration process. Set in:
```
/mnt/system/etc/eso/production/smartphone_integrator.json
```

Add to the process environment:
```json
"LD_PRELOAD": "/mnt/app/root/hooks/libcarplay_hook.so"
```

Log output: `/tmp/carplay_hook.log`

### Java patch JAR

```
Device:  /mnt/app/eso/hmi/lsd/jars/carplay_hook.jar
```

Must be prepended to the navigation process CLASSPATH in:
```
/mnt/system/etc/eso/production/navigation.json
```

Add to CLASSPATH (before existing entries):
```
/mnt/app/eso/hmi/lsd/jars/carplay_hook.jar:
```

### komoviewstyle.conf (FPK Presentation sidebar fix)

```
Device:  /mnt/app/navigation/resources/app/au/nar/pcconfig/komoviewstyle.conf
```

Replace the stock file. Change: line 3 `KVS_FPK` → `KVS_Most` (keeps 328x181 resolution).
Requires reboot - PresentationController reads this once at startup.

**Stock:**
```
3; KVS_FPK;328x181
```
**Patched:**
```
3; KVS_Most;328x181
```

### dio_manager.json (iAP2 message registration)

```
Device:  /mnt/system/etc/eso/production/dio_manager.json
```

Adds route guidance message IDs to the iAP2 Identify message so iOS enables sending them:

- `MessagesSentByAccessory`: add `0x5200` (StartRouteGuidanceUpdates), `0x5203` (StopRouteGuidanceUpdates)
- `MessagesReceivedFromDevice`: add `0x5201` (RouteGuidanceUpdate), `0x5202` (RouteGuidanceManeuverUpdate), `0x5204` (LaneGuidanceInformation)

Without `0x5204`, iOS never sends lane guidance data (checked via `iap2_identification_isIdentifiedForOutgoingMessageID` in the 0x5200 handler).

### VC render binary (if using cluster video)

```
Device:  /mnt/app/root/hooks/carplay_vmaneuver_render_egl
```

## PPS Interfaces (on device)

| Path | Direction | Purpose |
|------|-----------|---------|
| `/ramdisk/pps/iap2/routeguidance` | C→Java | Route guidance state (maneuver slots, distances, route state) |
| `/ramdisk/pps/iap2/coverart_notify` | C→Java | Cover art file change notifications |
| `/var/app/icab/tmp/37/` | C (files) | Cover art image files from iAP2 |

## Source Layout

```
c_hook/
  main.c                          Entry point (constructor-based module registration)
  framework/
    hook_framework.{c,h}          dlsym hook infrastructure
    logging.{c,h}                 File + stderr logging
    pps_writer.{c,h}              QNX PPS write helpers
    iap2_protocol.{c,h}           iAP2 TLV parsing
    common.h                      Shared defines
  routeguidance/
    rgd_hook.{c,h}                iAP2 → PPS bridge, slot mapping, TLV decode
    rgd_tlv.{c,h}                 TLV IDs, data structures
  coverart/
    coverart_hook.{c,h}           Cover art file watcher → PPS notify
    stb_image.h                   Image decode (stb single-header)

java_patch/
  com/luka/carplay/
    CarPlayHook.java              Main entry - onActivate/onDeactivate lifecycle
    DeviceStateHandler.java       CarPlay connect/disconnect state
    framework/
      Log.java                    Logging wrapper
      PPS.java                    QNX PPS subscriber (delta mode)
    routeguidance/
      RouteGuidance.java          PPS listener, state container, dirty tracking
      BAPBridge.java              BAP (HUD) + KOMO/DSI (VC sidebar) maneuver output
      ManeuverMapper.java         iAP2 type → BAP mainCategory/direction mapping
      SideStreets.java            Side streets byte computation
      GatedCombiService.java      FctID 19 gate (blocks stock position updates during CarPlay RG)
    coverart/
      CoverArt.java               PPS → VC cover art push
  de/audi/app/
    combi/bap/app/audio/
      AppConnectorTerminalMode.java   Patched: cover art via responseCoverArt (sourceType=21)
    terminalmode/combi/
      TerminalModeBapCombi$ActiveDeviceStateListener.java   Hook: CarPlay activate trigger
      TerminalModeBapCombi$EventListener.java               Hook: media event forwarding

maneuver_render/
  carplay_vmaneuver_render_egl.c         EGL render for VC video stream
```

## Build

### Java JAR
```bash
./build_java.sh
```
Requires `mu1316_lsd.jar` in project root. Compiles with Java 1.2 target.

### C hook library
```bash
./compile_c.sh
```
Cross-compiles on QNX VM (SSH). Requires QNX 6.5.0 SDP toolchain.

## Architecture

### Route Guidance Data Flow

```
iPhone (CarPlay)
  → iAP2 TLV (0x5201 route update, 0x5202 maneuver data, 0x5204 distance)
  → libcarplay_hook.so (C hook in smartphone_integration process)
    → slot mapping (8 fixed slots, LRU eviction)
    → PPS write /ramdisk/pps/iap2/routeguidance
  → carplay_hook.jar (Java in navigation process)
    → RouteGuidance.java (PPS subscriber, state + dirty tracking)
    → BAPBridge.java
      → BAP path: CombiBAPListener.applyManeuverDescriptor() → HUD (FctIDs 23,18,49)
      → KOMO path: KOMOService.setRouteInfo() + direct DSIKOMOView → VC sidebar
```

### FPK Presentation Sidebar (komoviewstyle.conf patch)

The VC sidebar is rendered by `libPresentationController.so` in the navigation process.
At boot it reads `komoviewstyle.conf` to select a rendering variant:

- **KVS_FPK** (stock for style 3) - renders from native map engine data only. DSI setRouteInfo() data is silently ignored. setKomoViewStyle() is permanently ignored after startup.
- **KVS_Most** (patched) - renders from DSI RouteInfoElement data. This is what MOST clusters use for turn arrow display.

The conf patch makes style 3 use KVS_Most at 328x181 (FPK resolution), so the VC sidebar renders our KOMO data.

### Cover Art Flow

```
iPhone (CarPlay)
  → iAP2 NowPlaying artwork → /var/app/icab/tmp/37/
  → libcarplay_hook.so (file watcher)
    → PPS write /ramdisk/pps/iap2/coverart_notify
  → carplay_hook.jar
    → CoverArt.java (PPS subscriber)
    → AppConnectorTerminalMode.responseCoverArt(sourceType=21) → VC
```
