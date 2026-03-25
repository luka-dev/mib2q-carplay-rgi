# MHI2 CarPlay Route Guidance

CarPlay patch set for Audi MHI2 infotainment.
(Based on MHI2Q MU1316 firmware, but may need rebuild for different versions.)

**Disclaimer:** Use at your own risk. These patches modify firmware binaries and system configurations on your infotainment unit. Always back up all original files before making any changes. The authors are not responsible for any damage, bricked devices, or warranty issues resulting from use of these patches.

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

### Route Guidance - VC LVDS Video (Custom Renderer)

Custom 3D renderer (`c_render/`) draws maneuver icons into the cluster's LVDS video pipeline. Renders into displayable 20 via EGL/GLES2, captured by the video encoder and sent over MOST to the VC.

- [x] Procedural 3D maneuver icons (turns, roundabouts, U-turns, merges, lane changes, arrival)
- [x] Side streets at junctions from iAP2 junction angles
- [x] Animated route path with curvature-aware speed
- [x] Smooth push transitions between consecutive maneuvers (crossfade + path chaining)
- [x] Camera follow/settle during transitions
- [x] Arrow-to-bulb tip morphing for arrival
- [x] Animated destination flag sprite
- [x] Distance bargraph overlay with blink mode
- [x] Perspective/orthographic view with animated blend
- [x] FXAA + 2x SSAA anti-aliasing
- [x] Painter's algorithm road rendering (white outline + grey fill + blue active route)
- [x] TCP command protocol (port 19800) - Java bridge sends maneuver updates, renderer handles all animation autonomously

### Route Guidance - VC AIO Arrows (NOT possible)

The VC's `SV_NavFPK_Compass_MobileDevice` view (which renders AIO arrow maneuver icons) requires InfoStates=6, but the VC's KSS AUTOSAR firmware **rejects value 6** at the MOST message validator (`sub_108F42C`: bit-dependency rule `(value & 5) != 4` blocks value 6) before it reaches EB GUIDE. The firmware cannot be persistently patched (secure AUTOSAR, RAM-only UDS patches lost on reboot). See `docs/kss_aio_arrow_analysis.md` for the full reverse engineering analysis.

BAP ManeuverDescriptor (FctID 23) drives the **HUD** maneuver icons - that path works and is not affected.

### Media

Stock MHI2 CarPlay (TerminalMode) forwards track title, artist, and album to the VC, but the stock `AppConnectorTerminalMode` never pushes cover art to the BAP picture manager. The VC always shows a blank/default album icon.

**Cover art pipeline:**

The C hook intercepts iAP2 transport packets (`read()`/`recv()` hooks), reassembles JPEG cover art from chunked transfers, decodes and resizes to 256x256 PNG using stb_image, and writes to `/var/app/icab/tmp/37/coverart.png`. A PPS notification (`/ramdisk/pps/iap2/coverart_notify`) signals Java. The Java `CoverArt` module watches PPS, and `TerminalModeBapCombi$EventListener` pushes the image through `AppConnectorTerminalMode` to the BAP picture manager with proper `ResourceLocator` and `responseCoverArt()` calls, mirroring the native `AppConnectorMedia` pattern. Late-arriving cover art (after track info was already sent) triggers a re-send with a modified picture ID to force the VC to refresh.

- [x] Cover art forwarding to VC

### Planned

- [ ] CarPlay AltScreen on cluster display (stream type 111, MHI3 backport - still in research phase)

## Patch Components

| Component                    | Type                  | Purpose                                                                | Output                |
|------------------------------|-----------------------|------------------------------------------------------------------------|-----------------------|
| `c_hook/`                    | C (ARM32 QNX)         | iAP2 hooks, route guidance and cover art bridge, PPS publishing        | `libcarplay_hook.so`  |
| `c_render/`                  | C (ARM32 QNX / macOS) | Custom 3D maneuver renderer (EGL/GLES2), TCP command-driven            | `maneuver_render`     |
| `java_patch/`                | Java patch JAR        | Route guidance rendering logic, BAP bridge, cover art forwarding hooks | `carplay_hook.jar`    |
| `dio_manager.json`           | System config patch   | Enables iAP2 route guidance message exchange with iOS                  | Manual edit on device |
| `smartphone_integrator.json` | System config patch   | Loads `libcarplay_hook.so` via `LD_PRELOAD` in dio_manager process     | Manual edit on device |

## Prerequisites

- **QNX SDP 6.5** - needed for cross-compiling C hook and renderer to ARM32 QNX. Easiest way is a QNX VM with the toolchain accessible over SSH.
  QNX VM image: https://archive.org/details/qnxsdp-65.7z (or any other QNX SDP 6.5 VM image available online).
  Set up SSH on the VM and configure the IP in the build scripts.

- **GLFW 3** (macOS only) - needed for local renderer development/testing. Install via `brew install glfw`.

- **lsd.jar** - original HU classes, needed to compile the Java patch against. Extract from your firmware dump:
  1. Get `/mnt/app/eso/hmi/lsd/lsd.jxe` from your firmware
  2. Convert with [jxe2jar](https://github.com/luka-dev/jxe2jar) (`../jxe2jar`)
  3. Output lands in `jxe2jar/out/lsd.jar` (default path used by `build_java.sh`)

## Build

### `libcarplay_hook.so` (C hook)

Cross-compiles on the QNX VM via SSH. Set `QNX_VM` IP in `compile_hook.sh`.

```bash
./compile_hook.sh
```

Output: `./libcarplay_hook.so`

Build flags:

| Flag                 | Default | Description                                 |
|----------------------|---------|---------------------------------------------|
| `LOG`                | `1`     | Enable/disable all logging                  |
| `LOG_RGD_PACKET_RAW` | `0`     | Full raw RGD packet dump (requires `LOG=1`) |

```bash
# default (logging on)
./compile_hook.sh

# silent build
LOG=0 ./compile_hook.sh

# verbose packet logging
LOG=1 LOG_RGD_PACKET_RAW=1 ./compile_hook.sh
```

### `maneuver_render` (3D renderer)

**For QNX (deployment):** Cross-compiles on the QNX VM via SSH. Set `QNX_VM` IP in `compile_render_qnx.sh`.

```bash
./compile_render_qnx.sh
```

Output: `./maneuver_render`

To build with the debug grid overlay:

```bash
./compile_render_qnx.sh grid
```

**For macOS (local development):** Builds natively with GLFW + OpenGL 2.1. Requires `brew install glfw`.

```bash
make -C c_render
```

Output: `c_render/c_render` (renderer) + `c_render/test_harness` (test client)

The macOS build also compiles a test harness - see [Testing the Renderer](#testing-the-renderer) below.

### `carplay_hook.jar` (Java patch)

Requires `lsd.jar` (see [Prerequisites](#prerequisites)). Check paths in `build_java.sh`.

```bash
./build_java.sh
```

Output: `./carplay_hook.jar`

All classes are compiled with `-source 1.2 -target 1.2` for MU1316 JVM compatibility. The bundled JDK from jxe2jar is used automatically.

## Testing the Renderer

The macOS build includes a **test harness** (`c_render/test_harness`) that sends TCP commands to the renderer, letting you cycle through all maneuver types and verify animations without a real device.

Run both in parallel:

```bash
cd c_render
./c_render & ./test_harness
```

The renderer window opens and the harness connects to `127.0.0.1:19800`.

**Harness controls:**

| Key          | Action                                                                               |
|--------------|--------------------------------------------------------------------------------------|
| Left / Right | Cycle through maneuver presets (turns, roundabouts, U-turns, merges, arrivals, etc.) |
| R            | Send a random maneuver with random angle, junction streets, and bargraph             |
| P            | Toggle perspective / orthographic view                                               |
| D            | Toggle debug grid overlay                                                            |
| Space        | Save screenshot (PPM)                                                                |
| S            | Toggle sidescreen / popup viewport mode                                              |
| Q / Esc      | Quit                                                                                 |

The harness auto-cycles through a built-in preset list that covers all icon types with varying directions, exit angles, junction configurations, and bargraph levels. Each arrow key press sends a `CMD_MANEUVER` packet triggering a push transition.

## Deployment

### Step 1: Create hooks directory

The hooks directory does not exist by default. Create it on the device:

```bash
mkdir -p /mnt/app/root/hooks
chmod 755 /mnt/app/root/hooks
```

### Step 2: Deploy `libcarplay_hook.so`

```bash
cp libcarplay_hook.so /mnt/app/root/hooks/
chmod 755 /mnt/app/root/hooks/libcarplay_hook.so
```

Runtime log (resets on each reboot): `/tmp/carplay_hook.log`

### Step 3: Patch `smartphone_integrator.json`

File: `/mnt/system/etc/eso/production/smartphone_integrator.json`

Add `LD_PRELOAD` under `$.children.carplay.envs`:

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

### Step 4: Patch `dio_manager.json`

File: `/mnt/system/etc/eso/production/dio_manager.json`

Add these iAP2 message IDs in the Identify registration:

`MessagesSentByAccessory` - add:
- `0x5200` = `StartRouteGuidanceUpdates`
- `0x5203` = `StopRouteGuidanceUpdates`

`MessagesReceivedFromDevice` - add:
- `0x5201` = `RouteGuidanceUpdate`
- `0x5202` = `RouteGuidanceManeuverUpdate`
- `0x5204` = `LaneGuidanceInformation`

Without these registrations, iOS does not send route guidance payloads.

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

After:

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

### Step 5: Deploy `carplay_hook.jar`

```bash
cp carplay_hook.jar /mnt/app/eso/hmi/lsd/jars/
```

### Step 6: Deploy `maneuver_render` and flag atlas

```bash
cp maneuver_render /mnt/app/root/hooks/
cp c_render/flag_atlas.rgba /mnt/app/root/hooks/
chmod 755 /mnt/app/root/hooks/maneuver_render
```

The flag atlas (`flag_atlas.rgba`) must be placed next to the `maneuver_render` binary. The renderer looks for it relative to its own path.

Launched automatically by the Java bridge (`BAPBridge`) when CarPlay route guidance starts. Log: `/tmp/maneuver_render.log`.

### Step 7: Reboot

Reboot the infotainment system. Give it 30+ seconds after file changes before rebooting - otherwise changes may not be flushed to disk.

## Quick Deployment Checklist

1. `./compile_hook.sh` - build `libcarplay_hook.so`
2. `./compile_render_qnx.sh` - build `maneuver_render`
3. `./build_java.sh` - build `carplay_hook.jar`
4. `mkdir -p /mnt/app/root/hooks` on device
5. Copy `libcarplay_hook.so` to `/mnt/app/root/hooks/`, `chmod 755`
6. Copy `maneuver_render` to `/mnt/app/root/hooks/`, `chmod 755`
7. Copy `flag_atlas.rgba` to `/mnt/app/root/hooks/`
8. Copy `carplay_hook.jar` to `/mnt/app/eso/hmi/lsd/jars/`
9. Set `LD_PRELOAD` in `smartphone_integrator.json`
10. Add `0x5200/01/02/03/04` IDs in `dio_manager.json`
11. Reboot (wait 30s after file changes)

## Help Wanted

### Maneuver icon testing

The iAP2-to-BAP maneuver mapping covers all 54 CarPlay maneuver types, but has only been tested on a limited set of real-world routes. Edge cases like complex interchanges and multi-lane roundabouts need more road testing. If you see a wrong or missing icon on the HUD, a log from `/tmp/carplay_hook.log` of the exact moment + explanation of what's wrong would help. Note: the log resets on each device reboot, so grab it before restarting.

## Thanks and References

Thanks for the previous work and knowledge that helped to figure this out.

- https://github.com/ludwig-v/wireless-carplay-dongle-reverse-engineering
- https://github.com/EthanArbuckle/iPhone18-3_26.1_23B85_Restore
- https://github.com/adi961/mib2-android-auto-vc
- [@fifthBro](https://t.me/fifthBro)
