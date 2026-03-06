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
- `INTERN_Active_NavFPK_Content` = 1 (Map)
- `LVDS_Available` = 1
- HU renders nav map internally, sends video over MOST isochronous channel
- VC enters BackgroundDelayHack first (wait for LVDS sync) -> then renders video overlay
- BAP text data (distance, street name) overlaid as widgets
- Triggered by: user pressing map in left menu (`APP_Nav_LeftMenu_Map_pressed`) or HU BAP MapPresentation

### SV_NavFPK_Compass (0xb4) - Native Compass
- `INTERN_Active_NavFPK_Content` = 0 (None)
- `BAP_NavSD_InfoStates_States` != 6
- Renders compass rose with heading angle (`NavFPK_Compass_Angle`)

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
