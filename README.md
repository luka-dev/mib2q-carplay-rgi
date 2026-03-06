# MHI2(Q) CarPlay Hook + Patch

CarPlay patch set for Audi MHI2(Q) infotainment.
(Based on MHI2Q MU1316 firmware, but may need rebuild for different versions.)

## Features

### Route Guidance - HUD

All data sent via BAP protocol (LSG 50) to the VC, which drives the HUD.

- [x] Maneuver icons - iAP2 turn type mapped to BAP ManeuverDescriptor (FctID 23). Supports turns, roundabouts, highway exits, merges, U-turns, ferry, etc.
- [x] Multi-maneuver list - up to 3 upcoming maneuvers sent in a single descriptor
- [x] Side streets at intersection - computed from iAP2 junction type + turn angle
- [x] Junction view - roundabout, highway interchange exit numbers
- [x] Left-hand / right-hand driving - auto-detected from iAP2 `driving_side`, affects icon mirroring and side-street layout
- [x] Distance to next maneuver - numeric display when far (FctID 18)
- [x] Distance bargraph - fills up on approach, with call-for-action blink at maneuver point (FctID 18)
- [x] Auto distance units - reads current HU setting (metric/imperial), no manual switch needed
- [x] Lane guidance - lane arrows with active-lane highlighting (FctID 24)

### Route Guidance - VC Text Overlays

Sent via the same BAP path. VC renders these as text bars over the native map area.

- [x] Turn-to street / exit name (FctID 23, part of ManeuverDescriptor)
- [x] Current road name (FctID 19)
- [x] Distance to destination (FctID 21)
- [x] ETA / remaining travel time - timezone-adjusted to HU local time (FctID 22)
- [x] Destination name (FctID 46)

### Route Guidance - VC Maneuver Graphics

The VC can render maneuver graphics (KDK intersection maps, turn arrows) in the MOST video stream via PresentationController.

- [ ] Maneuver graphics in MOST video stream
- [ ] Prevent VC from hiding native map during active route guidance

### Media

- [x] Cover art forwarding to VC

### Planned

- [ ] CarPlay AltScreen on cluster display (stream type 111, MHI3 backport - still in research phase)

## Patch Components

| Component                    | Type                | Purpose                                                                | Output                 |
|------------------------------|---------------------|------------------------------------------------------------------------|------------------------|
| `c_hook/`                    | C (ARM32 QNX)       | iAP2 hooks, route guidance and cover art bridge, PPS publishing        | `libcarplay_hook.so`   |
| `java_patch/`                | Java patch JAR      | Route guidance rendering logic, BAP bridge, cover art forwarding hooks | `carplay_hook.jar`     |
| `dio_manager.json`           | System config patch | Enables iAP2 route guidance message exchange with iOS                  | patched JSON on device |
| `smartphone_integrator.json` | System config patch | Loads `libcarplay_hook.so` via `LD_PRELOAD` in dio_manager process     | patched JSON on device |

## Build c_hook
You will need QNX SDP 6.5. I did it by spinning up a QNX VM and using the cross-compilation toolchain over SSH.
Link to QNX VM image: https://archive.org/details/qnxsdp-65.7z
(do not forget to set up SSH on VM and set the IP in `compile_c.sh`)
```bash
./compile_c.sh
```

Expected output artifact:

```text
./libcarplay_hook.so
```

Notes:

- `compile_c.sh` uploads sources to QNX VM
- Cross-compiles with `/usr/qnx650/host/qnx6/x86/usr/bin/ntoarmv7-gcc`
- Downloads the built `.so` back

Build flags:

- `LOG=1|0` (default: `1`)
- `LOG_RGD_PACKET_RAW=1|0` (default: `0`, requires `LOG=1`)

Examples:

```bash
# default build (logging enabled)
./compile_c.sh

# disable all logging in c_hook
LOG=0 ./compile_c.sh

# enable full raw RGD packet logging
LOG=1 LOG_RGD_PACKET_RAW=1 ./compile_c.sh
```

Invalid combination:

```bash
LOG=0 LOG_RGD_PACKET_RAW=1 ./compile_c.sh
```

## Deployment Steps

### Step 1: Deploy `libcarplay_hook.so`

Copy hook to device:

```text
/mnt/app/root/hooks/libcarplay_hook.so
```

Runtime log (default, resets on each reboot):

```text
/tmp/carplay_hook.log
```

### Step 2: Patch `smartphone_integrator.json`

File on device:

```text
/mnt/system/etc/eso/production/smartphone_integrator.json
```

Add/update `LD_PRELOAD` under:

```text
$.children.carplay.envs
```

Add this entry in `envs` (or replace existing `LD_PRELOAD=...` entry):

```json
"LD_PRELOAD=/mnt/app/root/hooks/libcarplay_hook.so"
```

Example:

```json
"carplay": {
  "exec": "dio_manager",
  "envs": [
    "LD_LIBRARY_PATH=/mnt/app/root/lib-target:/eso/lib:/mnt/app/usr/lib:/mnt/app/armle/lib:/mnt/app/armle/lib/dll:/mnt/app/armle/usr/lib",
    "IPL_CONFIG_DIR_DIO_MANAGER=/etc/eso/production",
    "LD_PRELOAD=/mnt/app/root/hooks/libcarplay_hook.so"
  ]
}
```

### Step 3: Patch `dio_manager.json`

File on device:

```text
/mnt/system/etc/eso/production/dio_manager.json
```

Add these iAP2 message IDs in Identify registration:

`MessagesSentByAccessory` add (or verify already present):

- `0x5200` = `StartRouteGuidanceUpdates`
- `0x5203` = `StopRouteGuidanceUpdates`

`MessagesReceivedFromDevice` add:

- `0x5201` = `RouteGuidanceUpdate`
- `0x5202` = `RouteGuidanceManeuverUpdate`
- `0x5204` = `LaneGuidanceInformation`

Why this matters: without these registrations, iOS does not send route guidance payloads.

Example (before):

```json
"MessagesSentByAccessory": [
  "0x5000", "0x5002", "0xAE00", "0xAE02", "0xAE03",
  "0x4154", "0x4156", "0x4157", "0x4159",
  "0xFFFB", "0x4C00", "0x4C02", "0x4C03", "0x4C05"
],
"MessagesReceivedFromDevice": [
  "0x4E09", "0x4E0A", "0x4E0C", "0x5001", "0xAE01",
  "0x4155", "0x4158", "0xFFFA", "0xFFFC", "0x4C01", "0x4C04"
]
```

After (added `0x5200`, `0x5203`, `0x5201`, `0x5202`, `0x5204`):

```json
"MessagesSentByAccessory": [
  "0x5000", "0x5002", "0xAE00", "0xAE02", "0xAE03",
  "0x4154", "0x4156", "0x4157", "0x4159",
  "0xFFFB", "0x4C00", "0x4C02", "0x4C03", "0x4C05",
  "0x5200", "0x5203"
],
"MessagesReceivedFromDevice": [
  "0x4E09", "0x4E0A", "0x4E0C", "0x5001", "0xAE01",
  "0x4155", "0x4158", "0xFFFA", "0xFFFC", "0x4C01", "0x4C04",
  "0x5201", "0x5202", "0x5204"
]
```

### Step 4: Build `carplay_hook.jar`

Requires `lsd.jar`.

Why it is needed:

- `build_java.sh` compiles patch classes against original Head Unit classes.
- Those classes are inside the OEM `lsd.jxe`, so we need it converted back to `lsd.jar`.

How to get your own `lsd.jar`:

- Get your original `lsd.jxe` from your firmware dump `/mnt/app/eso/hmi/lsd/lsd.jxe`
- Use `jxe2jar` (`../jxe2jar`) to convert it.
- Tool: https://github.com/luka-dev/jxe2jar

Build JAR (jxe2jar includes bundled JVMs for Windows/Linux/macOS that target Java 1.2).
Check paths to `lsd.jar` in `build_java.sh` (default: `jxe2jar/out/lsd.jar`).

```bash
./build_java.sh
```

Output artifact:

```text
./carplay_hook.jar
```

Copy JAR to device:

```text
/mnt/app/eso/hmi/lsd/jars/carplay_hook.jar
```


## Quick Deployment Checklist

1. Build `libcarplay_hook.so` with `bash ./compile_c.sh`
2. Copy `libcarplay_hook.so` to `/mnt/app/root/hooks/`
3. Set `LD_PRELOAD` in `smartphone_integrator.json`
4. Patch `dio_manager.json` with the `0x5200/01/02/03/04` IDs above
5. Build `carplay_hook.jar` with `./build_java.sh`, copy it to `/mnt/app/eso/hmi/lsd/jars/`
6. Reboot infotainment process/system 

P.S. If you do instant reboot of Head Unit after file changes, it's possible that changes will not yet be saved to disk. Give it 30+ seconds before rebooting.

## Help Wanted

### Maneuver icon testing

The iAP2-to-BAP maneuver mapping covers all 54 CarPlay maneuver types, but has only been tested on a limited set of real-world routes. Edge cases like complex interchanges and multi-lane roundabouts need more road testing. If you see a wrong or missing icon on the HUD, a log from `/tmp/carplay_hook.log` of the exact moment + explanation of what's wrong would help. Note: the log resets on each device reboot, so grab it before restarting.

### Maneuver icons on VC display

BAP ManeuverDescriptor (FctID 23) successfully drives the **HUD** -- it shows turn arrows, roundabout icons, etc. However, the **VC's own display** does not render maneuver icons from BAP.

**Why**: The HUD and the VC use different data paths for maneuver icons:
- **HUD**: Reads BAP ManeuverDescriptor directly from MOST Class 46 messages. This is why our BAP sends work for HUD.
- **VC display**: When in `SV_NavFPK_Compass_MobileDevice` mode (smartphone navigation), the VC renders maneuver arrows from `AIO_Arrow0-3_Direction` dp items. These dp items are written by the **KOMO/DSI path** on the HU side (native `KOMOService` -> DSI -> VC), not by BAP. Since we don't push KOMO data, these dp items are never populated, so the VC shows text overlays (distance, street name) but no maneuver icons.

**Possible approaches**:
- Write `AIO_Arrow` dp items directly from the HU side (need to find how KOMO/DSI maps ManeuverDescriptor to AIO arrow direction codes)
- Use the MOST video stream approach (see next section) to render graphical maneuvers instead of AIO arrows
- Reverse-engineer the `gssipc-kbd` BAP-to-dp mapping table to see if there's an unmapped BAP FctID that could drive VC icons

See `docs/vc_fpk_state_machine.md` for full VC state machine details and AIO arrow enum values.

### VC maneuver graphics in MOST video stream (alternative to AIO arrows)

Instead of AIO arrow icons, the VC can display rich graphical maneuvers (KDK intersection maps, turn arrows drawn at junctions) via MOST video. The HU-side `PresentationController` (native C++ library in `PNavApp`) renders these graphics and `videoencoderservice` encodes them as H.264 MPEG-TS over MOST isochronous channel to the VC.

This approach would replace the VC's widget-based compass/arrow view with a rendered map area showing graphical turn-by-turn, similar to what the VC shows during native navigation.

**The rendering side works**: `RouteInfoElement[]` pushed via Java `KOMOService.setRouteInfo()` reaches `PresentationController` through the DSI layer. `KVS_FPK` and `KVS_Most` use the same rendering pipeline internally -- both create the same render targets and process `RouteInfoElement` data identically.

**The video encoding side does not work**: `videoencoderservice` (always running, started via `dsistartup.json`) never begins capturing frames because it waits for a `setUpdateRate()` call from the native display manager, which is triggered by `ChoiceModel(1,168)` hints:

| Hint | Meaning                | Set by                             | Used by                                         |
|------|------------------------|------------------------------------|-------------------------------------------------|
| 1    | MOST reduced bandwidth | `setKOMODataRate(1)`               | `CombiMapController.updateFrameRate()` -> 1fps  |
| 2    | MOST full bandwidth    | `setKOMODataRate(2)`               | `CombiMapController.updateFrameRate()` -> 10fps |
| 4    | KDK available          | `ClusterKDKHandler.refreshHints()` | KDK positioning only                            |
| 8    | Small stage            | `ClusterKDKHandler.refreshHints()` | KDK positioning only                            |
| 16   | KDK visible            | `ClusterKDKHandler.refreshHints()` | KDK positioning only                            |

The FPK code path only sets hints 4/8/16 (KDK handler). Hints 1/2 are guarded by `isClusterMapMOST()` in `setKOMODataRate()` and never fire for FPK. Even after patching that guard, the encoder didn't start because `CombiMapController.processModelUpdateEvent()` for FPK calls `handleKDK()` + `switchToTargetContext()` but **skips `updateFrameRate()`** -- only the MOST variant calls it. Without `updateFrameRate()`, no `setUpdateRate()` reaches the native display manager, so `videoencoderservice` never captures.

**Possible approaches**:
- Native C hook in `videoencoderservice` or `CombiMapController` to force `setUpdateRate(1, 10)` when FPK KOMO is active
- Direct DSI/ASI call to `IVideoEncoding.setUpdateRate` from Java (bypassing `CombiMapController`)
- Patch `CombiMapController` class to call `updateFrameRate()` for FPK variant

See `docs/hu_maneuver_rendering_pipeline.md` for the full rendering and encoding architecture.

### Prevent VC from hiding native map during CarPlay route guidance

When CarPlay route guidance is active, the VC sometimes switches away from the map view. Need to understand what triggers this and how to keep the map visible.

## Thanks and References

Thanks for the previous work and knowledge that helped to figure this out.

- https://github.com/ludwig-v/wireless-carplay-dongle-reverse-engineering
- https://github.com/EthanArbuckle/iPhone18-3_26.1_23B85_Restore
- https://github.com/adi961/mib2-android-auto-vc
- [@fifthBro](https://t.me/fifthBro)
