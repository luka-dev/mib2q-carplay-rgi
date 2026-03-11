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

### Route Guidance - VC LVDS Video (KOMO Widget)

Maneuver graphics rendered by the HU's `libPresentationController.so` into the MOST LVDS video stream, displayed on the VC cluster map area. Requires 3 binary patches to the native library (see below).

- [x] Turn arrow / maneuver icon in cluster widget area
- [x] Java-controlled frame rate via `setKOMODataRate()` (0 = stop, 2 = render)
- [x] Turn-to street name in widget
- [x] Distance and ETA in widget follow-info area
- [x] Full start/stop lifecycle (KOMO view enable, video pipeline activation, gfxAvailable forcing)

### Route Guidance - VC AIO Arrows (NOT possible)

The VC's `SV_NavFPK_Compass_MobileDevice` view (which renders AIO arrow maneuver icons) requires InfoStates=6, but the VC's KSS AUTOSAR firmware **rejects value 6** at the MOST message validator (`sub_108F42C`: bit-dependency rule `(value & 5) != 4` blocks value 6) before it reaches EB GUIDE. The firmware cannot be persistently patched (secure AUTOSAR, RAM-only UDS patches lost on reboot). See `docs/kss_aio_arrow_analysis.md` for the full reverse engineering analysis.

BAP ManeuverDescriptor (FctID 23) drives the **HUD** maneuver icons — that path works and is not affected.

### Media

- [x] Cover art forwarding to VC

### Planned

- [ ] CarPlay AltScreen on cluster display (stream type 111, MHI3 backport - still in research phase)

## Patch Components

| Component                    | Type                | Purpose                                                                | Output                              |
|------------------------------|---------------------|------------------------------------------------------------------------|--------------------------------------|
| `c_hook/`                    | C (ARM32 QNX)       | iAP2 hooks, route guidance and cover art bridge, PPS publishing        | `libcarplay_hook.so`                 |
| `java_patch/`                | Java patch JAR      | Route guidance rendering logic, BAP bridge, cover art forwarding hooks | `carplay_hook.jar`                   |
| `tools/`                     | Python patch script | Binary patches for libPresentationController.so (KOMO widget video)    | `libPresentationController.so`       |
| `dio_manager.json`           | System config patch | Enables iAP2 route guidance message exchange with iOS                  | patched JSON on device               |
| `smartphone_integrator.json` | System config patch | Loads `libcarplay_hook.so` via `LD_PRELOAD` in dio_manager process     | patched JSON on device               |

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


### Step 5: Patch `libPresentationController.so` (for KOMO widget video)

This enables PresentationController to render maneuver graphics into the LVDS video
stream when driven by Java (CarPlay) instead of native navigation. Without this patch,
all Java DSI calls to PresentationController are silently dropped.

Generate patched binary from stock:

```bash
python3 tools/patch_libpresentationcontroller.py libPresentationController.so.stock libPresentationController.so
```

To verify bytes without patching:

```bash
python3 tools/patch_libpresentationcontroller.py --verify-only libPresentationController.so.stock
```

Copy to device:

```text
/mnt/app/navigation/libPresentationController.so
```

Back up original first:

```bash
cp /mnt/app/navigation/libPresentationController.so /mnt/app/navigation/libPresentationController.so.bak
```

Three patches applied (ARM32, file offset = VA):

| Patch | Address | Change | Purpose |
|-------|---------|--------|---------|
| 1 | `0x60BE48` | NOP `StopDSIs` (MOV R0,#0; BX LR) | Keep DSI interfaces alive after native guidance stops |
| 2 | `0x61C11C` | BNE → B (unconditional) | Force `StartDrawing` mode check to pass |
| 3 | `0x5C75A0`, `0x5C75E4`, `0x5C783C` | LDR offset 0x68 → 0x54 | Java-controlled frame rate via `setKOMODataRate()` |

See `docs/widget_video_architecture.md` for full technical details.

## Quick Deployment Checklist

1. Build `libcarplay_hook.so` with `bash ./compile_c.sh`
2. Copy `libcarplay_hook.so` to `/mnt/app/root/hooks/`
3. Set `LD_PRELOAD` in `smartphone_integrator.json`
4. Patch `dio_manager.json` with the `0x5200/01/02/03/04` IDs above
5. Build `carplay_hook.jar` with `./build_java.sh`, copy it to `/mnt/app/eso/hmi/lsd/jars/`
6. Patch `libPresentationController.so` with `tools/patch_libpresentationcontroller.py`, copy to `/mnt/app/navigation/`
7. Reboot infotainment process/system 

P.S. If you do instant reboot of Head Unit after file changes, it's possible that changes will not yet be saved to disk. Give it 30+ seconds before rebooting.

## Help Wanted

### Maneuver icon testing

The iAP2-to-BAP maneuver mapping covers all 54 CarPlay maneuver types, but has only been tested on a limited set of real-world routes. Edge cases like complex interchanges and multi-lane roundabouts need more road testing. If you see a wrong or missing icon on the HUD, a log from `/tmp/carplay_hook.log` of the exact moment + explanation of what's wrong would help. Note: the log resets on each device reboot, so grab it before restarting.

### VC AIO arrows - not possible (workaround: LVDS video)

The VC's MobileDevice view (`SV_NavFPK_Compass_MobileDevice`) renders AIO arrow maneuver icons and is the only VC view that shows graphical turn-by-turn from smartphone navigation. However, activating this view requires InfoStates=6, which is **rejected by the VC's KSS AUTOSAR firmware** (`sub_108F42C` at `0x108F42C`: bit-dependency rule `(value & 5) != 4` blocks value 6).

The entire HU-side pipeline works — KOMO data reaches PresentationController, MOST 0x2289 arrow bytes are sent, and AIO_Arrow dp items are populated on the VC. But the view that renders them can never be activated.

The VC firmware cannot be persistently patched (secure AUTOSAR environment, RAM-only UDS patches lost on every reboot). See `docs/kss_aio_arrow_analysis.md` for the full KSS reverse engineering analysis.

**Workaround**: Instead of AIO arrows, maneuver graphics are rendered by PresentationController into the LVDS video stream (KOMO widget path). This requires patching `libPresentationController.so` on the HU side (see Step 5 above). The VC displays this via `SV_LVDS_NavMap_FPK` (LVDS map view), which does NOT require InfoStates=6.

## Thanks and References

Thanks for the previous work and knowledge that helped to figure this out.

- https://github.com/ludwig-v/wireless-carplay-dongle-reverse-engineering
- https://github.com/EthanArbuckle/iPhone18-3_26.1_23B85_Restore
- https://github.com/adi961/mib2-android-auto-vc
- [@fifthBro](https://t.me/fifthBro)
