# HU Maneuver Rendering Pipeline (MOST Video to VC)

## Architecture: HU -> MOST -> VC Video Stream

```
+--------------------------- HU (MHI2) ----------------------------+
|                                                                   |
|  PNavApp (navigation app)                                         |
|    +-- libPresentationController.so (9.8MB, ARM32 QNX C++)        |
|         +-- ServiceProviderDSIKomoViewImpl (KOMO view provider)   |
|         |    +-- RouteInfoElement[] -> turn icons, distances       |
|         +-- SDKRenderTargetFactory                                |
|         |    +-- CreateRenderKDKTarget      (intersection maps)   |
|         |    +-- CreateRenderExitViewTarget  (highway exits)      |
|         |    +-- CreateRenderDefaultTarget   (map)                |
|         +-- Renders to shared render target (RTP_SHARED_RENDER)   |
|              at KVS_FPK size: 210x153 or 328x181                  |
|                        |                                          |
|                        v  IPTE (inter-process texture exchange)    |
|                                                                   |
|  videoencoderservice                                              |
|    +-- QCVideoEncoderSrc   <- ipteCaptureDisplayable()            |
|    +-- QCVideoEncoderH264  <- OMX.qcom.video.encoder.avc (HW)    |
|    +-- QCVideoEncoderTS    <- MPEG-TS muxer                       |
|    +-- QCVideoEncoderSink  -> CMLBISODriver (MLB ISO TX)          |
|                        |                                          |
+------------------------+------------------------------------------+
                         |  MOST isochronous channel
                         v
+---------- VC (AU491 FPK) ----------+
|  gssipc-kbd -> dp items            |
|  fds (EB GUIDE) -> SV_LVDS_NavMap_FPK |
|    renders MOST video + BAP text   |
|    overlays (distance, street)     |
+------------------------------------+
```

## The Key Component: libPresentationController.so

This is the **only thing that renders maneuvers** into the video stream. Native C++ library inside the navigation app (`PNavApp`). Source path from debug strings:
```
/data/workspace/MIB-High_NB_8_CLU10_17123MIB2MAIN_QC_target_QNX_arm/build/QNX_armRelease/
  PresentationController/Code/PresentationController/map/Control/SDK/MapRendering/
```

### What It Does

1. **Reads `komoviewstyle.conf`** to determine cluster mode:
   - `KVS_FPK` -> 210x153 or 328x181 pixel render targets (AU491 FPK)
   - `KVS_Most` -> 800x252 (Top-Kombi/MOST cluster)
   - `KVS_RGI` -> 263x366 (legacy MIB1)
   - DSI values: 1=MOST, 2=FPK(210x153), 3=FPK(328x181), 255=RGI
   - "MoKo-Story for FPK display, one displayable for all single views, controlled by navigation, no HMI control possible"

2. **Creates dedicated render targets** for cluster (separate from main display):
   - `CreateRenderKDKTarget` - intersection map (junction drawings from KDK*.kdx files at `resources/app/au/nar/intersectionmap/`)
   - `CreateRenderExitViewTarget` - highway exit ramp views (from `resources/app/au/nar/exitview/`)
   - `CreateRenderDefaultTarget` - map view with route line
   - All use `SDKRenderTarget` with `RTP_SHARED_RENDER_TARGET` type
   - Up to 5 render targets: `CREATE_MAP_RENDERER_RENDER_TARGET_ID0..4`

3. **Populates `RouteInfoElement[]`** via `ServiceProviderDSIKomoViewImpl`:
   - Turn type + angle from native route calculation
   - Distance to maneuver
   - Street names
   - Turn arrows at junctions
   - `BapManeuverDescriptor` list -> converted to visual elements
   - Rendered into MoKo-Story view

4. **DSI Interfaces**:
   - `DSIKOMOView` - KOMO view provider (route info, view style, enable/disable)
   - `DSINavigation` - Navigation data provider (route, maneuvers, lane guidance)
   - Both implemented in `ServiceProviderDSIKomoViewImpl` / `ServiceProviderDSINavigationImpl`

5. **Renders via `libRenderSystem.so`** (2.0MB) to shared render target
   - Dual factories: RS2D (2D for cluster) and RS3D (3D for main)
   - Map rendering uses `libLuaMap.so` + `libLuaMapOverlay.so`
   - Output routing via `libLuaGraphicsOutput.so` (wraps QNX libscreen)

## Video Encoding Chain

`videoencoderservice` (separate ARM32 QNX binary) captures the render target:

```
IPTE capture
  ipteCaptureDisplayable(displayableId) -> raw framebuffer
    -> QCVideoEncoderH264 (Qualcomm OMX.qcom.video.encoder.avc HW encoder)
      -> QCVideoEncoderTS (MPEG-TS transport stream wrapper)
        -> QCVideoEncoderSink -> CMLBISODriver
          -> write to /dev/mlb/ isochronous TX device
            -> MOST isochronous channel -> VC
```

### videoencoderservice Key Classes
- `CASIVideoEncodingSrv` - Main service, receives `setActiveDisplayable(displayID, displayable)` and `setUpdateRate(displayID, rate)` from Java/HMI layer
- `CDisplayTimer` - Timer-driven frame capture loop
- `CDisplayLink` - Manages MOST video connection (request/release via `asi.videomanagement`)
- `CVideoEncoderFactory` - Creates encoder instances per displayable
- `QCVideoEncoderSrc` - Source: IPTE framebuffer capture
- `QCVideoEncoderH264` - H.264 encoder (Qualcomm OMX hardware)
- `QCVideoEncoderTS` - MPEG-TS muxer
- `QCVideoEncoderSink` - Sink: writes to MLB ISO driver

### videoovermost (receiver side - for reference)
- Receives MOST isochronous video (TV tuner, AVDC) on HU side
- NOT involved in sending to cluster - that's videoencoderservice
- Reads from `/dev/mlb/isoRX1` (TV) or `/dev/mlb/isoRX2` (AVDC)

## Display Configuration

From `graphics_eifs_MMXF.conf`:
- **Display 1** = Main HU screen (1024x480 @ 60Hz, pipeline 1)
- **Display 4** = Second display (1024x480 @ 60Hz, pipeline 2, mirroring=fill)
- Navigation renders to a **displayable** (not a physical display) - shared render target captured by videoencoderservice via IPTE

## Cluster Mode Selection

Persistence flags determine cluster type:
- `/mnt/app/eso/hmi/enableKombiMapFPK` -> LVDS/FPK mode
- `/mnt/app/eso/hmi/enableKombiMapMOST` -> MOST mode
- Neither -> RGI/MMI mode (no cluster map)

Scripts: `clusterCodingLVDS.sh` / `clusterCodingMOST.sh` / `clusterCodingOFF.sh`
Uses: `dumb_persistence_writer -P -f 0 3221356656 {03=LVDS, 02=MOST}`

## SDIS Coding

`videoovermost.sh` checks SDIS coding before enabling NVVS streaming:
```sh
dumb_persistence_reader -O 412 -L 1 28442848 100
# If 0 -> NVVS_STREAMING_ENABLE=1 (cluster map enabled)
# If non-0 -> NVVS_STREAMING_ENABLE=0 (disabled)
```

## Firmware Paths (MU1316)

| Component | Path |
|-----------|------|
| Navigation app | `advanced/MU1316-appimg/navigation/` |
| PresentationController | `advanced/MU1316-appimg/navigation/libPresentationController.so` |
| RenderSystem | `advanced/MU1316-appimg/navigation/libRenderSystem.so` |
| LuaMap/Overlay | `advanced/MU1316-appimg/navigation/libLuaMap.so`, `libLuaMapOverlay.so` |
| GraphicsOutput | `advanced/MU1316-appimg/navigation/libLuaGraphicsOutput.so` |
| KOMO view style | `advanced/MU1316-appimg/navigation/resources/app/au/nar/pcconfig/komoviewstyle.conf` |
| KDK intersection maps | `advanced/MU1316-appimg/navigation/resources/app/au/nar/intersectionmap/KDK*.kdx` |
| Exit view assets | `advanced/MU1316-appimg/navigation/resources/app/au/nar/exitview/` |
| Map preferences | `advanced/MU1316-appimg/navigation/mapprefs.xml` |
| Video encoder service | `advanced/MU1316-appimg/eso/bin/apps/videoencoderservice` |
| Video over MOST | `advanced/MU1316-appimg/eso/bin/apps/videoovermost` |
| Video config | `advanced/MU1316-system/etc/eso/production/videoovermost.json` |
| Display config | `advanced/MU1316-system/etc/system/config/display/graphics_eifs_MMXF.conf` |
| Cluster coding scripts | `advanced/MU1316-appimg/eso/hmi/engdefs/scripts/clusterCoding*.sh` |
| EB GUIDE (gemib) | `advanced/MU1316-appimg/gemib/` |
| MOST channel config | videoovermost.json: channel_id 65558 (CL_HDTV=4), isoRX1 |

## Implications for CarPlay

Maneuvers in the LVDS video stream come **entirely from native PresentationController**. CarPlay data does NOT flow through this pipeline.

| Scenario | What VC Shows |
|----------|---------------|
| Native nav active | Rich KDK/exit/map with maneuver graphics in video + BAP text overlays |
| CarPlay nav active (current) | BAP text overlays only (distance, turn-to, street via BAPBridge) + HUD maneuver icons. No graphical maneuvers in video stream |
| CarPlay + AltScreen (future) | CarPlay renders its own cluster map via stream type 111 |

### KOMO/PresentationController Approach (attempted, abandoned Mar 2026)

We attempted to inject RouteInfoElements via `KOMOService.setRouteInfo()` -> DSI -> PresentationController to get maneuver graphics in the MOST video stream. The rendering pipeline itself works (KVS_FPK and KVS_Most are functionally identical), but the **video encoder never starts** because:

- FPK path only sets ChoiceModel(1,168) hints 4/8/16 (KDK handler positioning)
- Hints 1/2 (MOST bandwidth) are needed to trigger `videoencoderservice` via `CombiMapController.updateFrameRate()`
- `setKOMODataRate()` was guarded by `isClusterMapMOST()` -- never fires for FPK
- Even after patching the guard, `gfxAvailable` remained false and `kdkVisible` dropped intermittently

All KOMO code was stripped from the Java patches. See `vc_fpk_state_machine.md` for the VC side of this.

BAP overlays (distance, turn-to, street name) work independently of the video stream -- they're rendered by VC's EB GUIDE widgets on top of whatever video is showing.
