# VC FPK Navigation State Machine (AU491)

## Architecture: 3 Layers

```
MHI2 HU (Java)
    | BAP over MOST (LSG 50)
    | LVDS video over MOST (isochronous channel)
    v
gssipc-kbd  (BAP->dp gateway, ARM32 QNX binary)
    | dp item writes via bap_send_indication_u8/u16/u32/bytes
    | mapping table at dword_19A2E8: (28*lsg + 7*fctId_group + record) -> dp index
    v
fds  (EB GUIDE HMI runtime, separate binary)
    -> evaluates dp bindings -> state machine -> renders cluster views
```

- `gssipc-kbd` only handles BAP reception and dp item writes
- State machine is in `fds` binary (EB GUIDE compiled model, not in KZB files)
- KZB files (AU491_CarWarnings.kzb etc.) are only for car warnings, not navigation
- dp_monitor.cfg defines schema (items, states, views, events, enums)
- Transition rules are in EB GUIDE model compiled into `fds`

## FPK Navigation State Hierarchy

```
NavFpk (0xa6)
+-- SV_NavFPK_NA (0xb7)               <- Nav not available
+-- NAV_fpk_Operable (0xa7)
    +-- SV_NavFPK_HinweisTexte (0xb6)  <- Info/hint texts
    +-- NAV_FPK_Operable_Normal (0xa8)
        |
        +-- NavMap_FPK (0xa9)           <- LVDS MAP VIDEO
        |   +-- OptionMenu (0xaa)       <- Map settings submenu
        |   +-- SV_LVDS_NavMap_FPK_BackgroundDelayHack (0xb0)
        |   +-- SV_LVDS_NavMap_FPK (0xb1)  * MOST video stream
        |
        +-- SV_NavFPK_Favourites (0xb2)      <- Favourites list
        +-- SV_NavFPK_LastDestinations (0xb3) <- Last destinations
        +-- SV_NavFPK_Compass (0xb4)          * Compass (native navi)
        +-- SV_NavFPK_Compass_MobileDevice (0xb5) * MobileDevice (smartphone)
```

## Key dp Items Driving Transitions

| dp Item                         | ID        | Type         | What it controls                                          |
|---------------------------------|-----------|--------------|-----------------------------------------------------------|
| `INTERN_Active_NavFPK_Content`  | 0x4000037 | Integer      | Content selector: None=0, Map=1, LastDest=2, Fav=3        |
| `BAP_NavSD_InfoStates_States`   | 0x400000f | Integer      | MobileDevice trigger (value 6 = NavigationInMobileDevice) |
| `BAP_NavSD_RG_Status_RG_Status` | 0x2000070 | Integer      | Route guidance active/inactive                            |
| `LVDS_Available`                | 0x20002d7 | Integer      | LVDS video stream available                               |
| `NavFPK_RG_toggled`             | 0x400004f | Integer      | RG toggle tracking                                        |
| `AIO_Arrow0_Direction`          | 0x1000000 | Integer      | KOMO/DSI turn arrow 0                                     |
| `AIO_Arrow1_Direction`          | 0x1000001 | Integer      | KOMO/DSI turn arrow 1                                     |
| `AIO_Arrow2_Direction`          | 0x1000002 | Integer      | KOMO/DSI turn arrow 2                                     |
| `AIO_Arrow3_Direction`          | 0x1000003 | Integer      | KOMO/DSI turn arrow 3                                     |
| `AIO_Arrows_IconPaths`          | 0x6000000 | StringVector | Arrow icon image paths                                    |

### AIO Arrow Direction Enum
```
NoArrow=0, Straight=1, StraightLeft=2, Left=3, SharpLeft=4,
UturnLeft=5, UturnRight=6, SharpRight=7, Right=8, StraightRight=9
```

## Critical Finding: NO ActiveRGType dp Item

The VC has NO dp item for rgType (BAP FctID 39). It's either:
- Mapped to sentinel 0x1f ("not mapped") in the BAP->dp table
- Or consumed internally without dp write

**The VC state machine uses InfoStates (not rgType) to switch display modes.**

rgType=2 matters on HU Java side (CombiBAPListener internal state) but VC doesn't use it.

## When Each View Is Shown

### SV_LVDS_NavMap_FPK (0xb1) - LVDS Map Stream over MOST

**Transition requirements:**
- `INTERN_Active_NavFPK_Content` = 1 (Map)
- `LVDS_Available` = 1
- Enters `SV_LVDS_NavMap_FPK_BackgroundDelayHack` (0xb0) first, waits for LVDS sync via
  `CODING_Timer_tdLVDSLock`, then transitions to `SV_LVDS_NavMap_FPK` (0xb1)
- Triggered by: `APP_Nav_LeftMenu_Map_pressed` event or HU BAP MapPresentation

**Video stream -- hardware pipeline (not dp items):**
```
HU: PresentationController renders map/KDK -> GPU framebuffer
  -> videoencoderservice captures via IPTE (Inline Processing and Transfer Engine)
  -> Qualcomm H.264 HW encoder -> MPEG-TS packetizer
  -> MLB (Media Logical Block) ISO TX -> MOST isochronous channel
  -> VC MOST receiver -> HW video decoder -> display layer
```

The LVDS video is a hardware video pipeline. `LVDS_Available` (0x20002d7) is set by the
VC's graphics subsystem when it detects valid video sync from the MOST decoder. The video
layer is rendered BEHIND EB GUIDE widget overlays.

**Video/KDK positioning dp items:**

| dp Item                               | ID           | Purpose                           |
|---------------------------------------|--------------|-----------------------------------|
| `SV_LVDS_FPK_NavMap_Image_x/y`        | 0x400007c/7d | Video frame position              |
| `SV_LVDS_FPK_NavMap_Image_imageFiles` | 0x60000c1    | Fallback/background images        |
| `SV_LVDS_FPK_NavMap_Overlay_color`    | 0x400007e    | Tint overlay color                |
| `SV_LVDS_FPK_NavMap_Slider_x/y`       | 0x400007f/80 | Slider widget position            |
| `SV_LVDS_FPK_NavMap_overlay_txtPaths` | 0x60000c2    | Overlay text paths                |
| `LVDS_KDK_visible`                    | 0x20002df    | KDK intersection overlay visible  |
| `LVDS_KDK_opacity`                    | 0x20002dc    | KDK overlay opacity (fade in/out) |
| `LVDS_KDK_position_x/y`               | 0x20002dd/de | KDK overlay position              |
| `LVDS_KDK_follow`                     | 0x20002db    | KDK follow mode                   |
| `LVDS_KDK_DM_opacity`                 | 0x20002d8    | Display manager KDK opacity       |
| `LVDS_KDK_DM_position_x/y`            | 0x20002d9/da | Display manager KDK position      |
| `CODING_Timer_tdLVDSLock`             | 0x20000e6    | LVDS lock delay timer             |
| `MMI_Current_LVDS_Skin`               | 0x20002e6    | LVDS skin variant                 |

**BAP text overlays rendered ON TOP of video:**

| dp Item                                                | ID        | BAP FctID | Content            |
|--------------------------------------------------------|-----------|-----------|--------------------|
| `BAP_NavSD_DistanceToNextManeuver_Distance`            | 0x2000067 | 18        | Distance text      |
| `BAP_NavSD_DistanceToNextManeuver_BargraphOn`          | 0x2000066 | 18        | Bargraph active    |
| `BAP_NavSD_DistanceToNextManeuver_Unit`                | 0x2000068 | 18        | Distance unit      |
| `BAP_NavSD_DistanceToNextManeuver_ValidityInformation` | 0x2000069 | 18        | Validity           |
| `BAP_NavSD_TurnToInfo_TurnToInfo_text`                 | 0x1000018 | 23        | Turn-to street     |
| `BAP_NavSD_TurnToInfo_TurnToInfo_available`            | 0x1000017 | 23        | Turn-to visible    |
| `BAP_NavSD_TurnToInfo_SignPost_text`                   | 0x1000016 | 23        | Signpost text      |
| `BAP_NavSD_TurnToInfo_SignPost_available`              | 0x1000015 | 23        | Signpost visible   |
| `BAP_NavSD_CurrentPositionInfo_PositionInfo_text`      | 0x1000013 | 19        | Current road       |
| `BAP_NavSD_CurrentPositionInfo_PositionInfo_available` | 0x1000012 | 19        | Road visible       |
| `BAP_NavSD_DistanceToDestination_Distance`             | 0x4000001 | 21        | Dist to dest       |
| `BAP_NavSD_DistanceToDestination_Unit`                 | 0x4000002 | 21        | Unit               |
| `BAP_NavSD_RG_Status_RG_Status`                        | 0x2000070 | 17        | RG active          |
| `NavFPK_DTD_ArrivalTime`                               | 0x400003b | 22        | ETA                |
| `NavFPK_DTD_Distance`                                  | 0x400003d | 21        | Formatted distance |
| `BAP_NavSD_Seperator_text`                             | 0x1000014 | -         | Separator          |

**Map overlay widgets (on-screen map UI elements):**

| dp Item                                         | ID        | Content             |
|-------------------------------------------------|-----------|---------------------|
| `Nav_Map_Scale_String`                          | 0x4000056 | Scale bar text      |
| `Nav_Map_Scale_Unit`                            | 0x4000057 | Scale unit          |
| `Nav_Map_Scale_visible`                         | 0x4000058 | Scale bar visible   |
| `Nav_Map_Altitude_String`                       | 0x4000053 | Altitude text       |
| `Nav_Map_Altitude_Unit`                         | 0x4000054 | Altitude unit       |
| `Nav_Map_Streetname`                            | 0x4000059 | Street name on map  |
| `Nav_Map_TurnToInfo_available`                  | 0x400005a | Turn-to bar visible |
| `Nav_Map_signPost`                              | 0x400005b | Signpost text       |
| `Nav_Map_signPost_available`                    | 0x400005c | Signpost visible    |
| `BAP_NavSD_MapColorAndType_Colour`              | 0x400001c | Day/night/auto      |
| `BAP_NavSD_MapScale_AutoZoom`                   | 0x400001d | Auto zoom state     |
| `BAP_NavSD_MapViewAndOrientation_ActiveMapView` | 0x400001e | 2D/3D/overview      |

**LVDS events:**
- `APP_LVDS_Capturing_Enable` (0xb7) / `APP_LVDS_Capturing_Disable` (0xb6)
- `INTERN_LVDS_Nav_Update` (0x162)
- `ENGINEERING_LVDS_DebugLabel_Text/Visible` -- debug overlay

### SV_NavFPK_Compass (0xb4) - Native Compass with RG Overlays
- `INTERN_Active_NavFPK_Content` = 0 (None)
- `BAP_NavSD_InfoStates_States` != 6
- Renders compass rose with heading angle (`NavFPK_Compass_Angle`)
- When RG is active (InfoStates=3), renders maneuver data from KSS signal groups 58/61

**Two data paths feed this view:**

1. **BAP -> gssipc-kbd -> named dp items** (text overlays):
   - `BAP_NavSD_DistanceToNextManeuver_*` (distance text, bargraph, unit, validity)
   - `BAP_NavSD_TurnToInfo_*` (turn-to street text, signpost text)
   - `BAP_NavSD_CurrentPositionInfo_*` (current road text)
   - `BAP_NavSD_DistanceToDestination_*` (distance to dest)
   - `BAP_NavSD_RG_Status_*` (RG active/inactive)
   - `NavFPK_DTD_*` (arrival time, distance)

2. **MOST Class 46 -> KSS EclWrapper -> signal groups 58/61 -> KssIpc -> EB GUIDE** (maneuver rendering):
   - 0x500 -> SG 58 (+10,+16) -- ManeuverDescriptor part 1
   - 0x501 -> SG 61 (+24,+30) -- ManeuverDescriptor part 2
   - 0x504 -> SG 58 (+42/+22) -- Distance data (variant-dependent)
   - 0x505 -> SG 61 (+0,+6) -- TurnTo street
   - 0x506 -> SG 61 (+12,+18) -- Current street
   - 0x509 -> SG 58/61 -- Route info
   - 0x50C -> SG 58 (+57) -- Lane guidance (5 lanes, byteswap)
   - 0x510 -> SG 58 (+70,+76) -- Distance to destination
   - 0x522 -> SG 61 (+44) -- RG status byte

   **No ManeuverDescriptor/ExitView/LaneGuidance/SideStreet dp items exist** in dp_monitor.cfg.
   EB GUIDE reads signal group 58/61 bytes directly for maneuver icon rendering.

   These MOST messages are the same ones generated by our BAP ManeuverDescriptor sends
   (FctID 23 -> MOST 0x500/0x501). The data path already works for this view.

### SV_NavFPK_Compass_MobileDevice (0xb5) - Smartphone Navigation (CarPlay Target)
- `BAP_NavSD_InfoStates_States` = 6 (NavigationInMobileDevice)
- Triggered by GAL state: `setGALState(true)` -> `updateInfoStates()` -> sends 6
- Renders:
  - AIO arrows from `AIO_Arrow0-3_Direction` (written by KOMO/DSI)
  - Arrow icons from `AIO_Arrows_IconPaths`
  - BAP text overlays (distance, turn-to, destination info)
  - No LVDS video - pure widget rendering

### SV_NavFPK_Favourites (0xb2) / SV_NavFPK_LastDestinations (0xb3)
- `INTERN_Active_NavFPK_Content` = 3 (Favourites) or 2 (LastDest)
- Triggered by user pressing in left menu or HU sending destination list BAP data

### SV_NavFPK_NA (0xb7) - Nav Not Available
- Navigation system not ready or no BAP connection

## Navigation View Summary

All 6 leaf views in the NavFpk state hierarchy:

| View                             | ID   | Maneuver source                        | Trigger                       | Status                      |
|----------------------------------|------|----------------------------------------|-------------------------------|-----------------------------|
| `SV_LVDS_NavMap_FPK`             | 0xb1 | KDK video via MOST isochronous         | LVDS_Available=1, content=Map | Needs video encoder         |
| `SV_NavFPK_Compass`              | 0xb4 | Signal groups 58/61 (MOST 0x500-0x522) | InfoStates!=6, content=None   | **Our BAP sends feed this** |
| `SV_NavFPK_Compass_MobileDevice` | 0xb5 | AIO_Arrow dp items (MOST 0x2289)       | InfoStates=6                  | **Blocked by KSS**          |
| `SV_NavFPK_Favourites`           | 0xb2 | None                                   | content=Fav                   | N/A                         |
| `SV_NavFPK_LastDestinations`     | 0xb3 | None                                   | content=LastDest              | N/A                         |
| `SV_NavFPK_HinweisTexte`         | 0xb6 | None                                   | Hint text mode                | N/A                         |

The compass view (0xb4) is the most promising path -- it uses the same MOST Class 46
messages that our BAP ManeuverDescriptor sends generate. InfoStates=3 (native RG active)
passes the KSS validator and should enable RG overlays in this view.

## CarPlay Route Guidance Flow

```
BAPBridge.onStart()
    -> forceClusterRouteInfoState(true) -> container.rgActive=true, rgiValid=true
    -> setGALState(true)
        -> CombiBAPListener.naviIsRunningOnSmartphone = true
            -> updateInfoStates() -> BAP FctID 17 sends value 6
                -> gssipc-kbd writes BAP_NavSD_InfoStates_States = 6
                    -> EB GUIDE -> SV_NavFPK_Compass_MobileDevice

BAP sends maneuver/distance/text -> BAP dp items -> EB GUIDE renders HUD icons + text overlays
```

Note: `SV_NavFPK_Compass_MobileDevice` also has AIO_Arrow dp items (driven by native KOMO/DSI in stock navigation). In the CarPlay path these are not used -- all maneuver data goes through BAP ManeuverDescriptor to HUD, and BAP text overlays to VC.

### KSS Firmware Blocks InfoStates=6

Even though the HU sends InfoStates=6 via MOST 0x515, the KSS AUTOSAR firmware
**rejects value 6** before it reaches EB GUIDE. The validator `sub_108F42C` at
`0x108F42C` enforces bit-dependency rules:

```c
return a1 <= 0xF && (a1 & 5) != 4 && (a1 & 0xA) != 8;
//                   ^^^^^^^^^^^^^^
//                   6 & 5 = 4 -> FAIL
```

Value 6 (`0b0110`) has bit 2 set without bit 0, violating the rule. The
`BAP_NavSD_InfoStates_States` dp item is never written to 6, so the VC never
enters `SV_NavFPK_Compass_MobileDevice`.

The VC/FPK firmware cannot be persistently patched (secure AUTOSAR environment,
RAM-only UDS patches lost on reboot). This makes the MobileDevice/AIO arrow
approach not viable.

See `docs/kss_aio_arrow_analysis.md` for the full reverse engineering analysis.

## Firmware Paths
- gssipc-kbd: `extract_stage3/KI_FPK_AU491/.../bin/app/gssipc-kbd`
- fds (EB GUIDE): `extract_stage3/KI_FPK_AU491/.../bin/app/fds`
- dp_monitor.cfg: `extract_app/KI_FPK_AU491/.../dp/dp_monitor.cfg`
- KZB models: `extract_app/KI_FPK_AU491/.../models/*.kzb` (CarWarnings only)

## Non-FPK LVDS States (for reference)
Non-FPK clusters use different state paths:
- `LVDS_NavMap` (0x9e) -> `SV_LVDS_NavMap` (0x9f) - standard LVDS nav map
- `LVDS_NavDest` (0x9a) -> `SV_LVDS_NavDestinations` (0x9d) - LVDS destinations
- `LVDS_Nav_Generic` (0xa2) -> `SV_LVDS_Nav` (0xa3) - generic LVDS nav
Each has NA_Init and NA_Off sub-states for availability management.
