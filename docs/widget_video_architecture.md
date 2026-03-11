# KOMO Widget Video Architecture — MU1316 MHI2 FPK

## Overview

The FPK (Full-screen Presentation Kombi) cluster has two video-related display areas:
1. **Main map** (1440xYYY LVDS) — full navigation map
2. **Widget** (maneuver arrow/icon) — small overlay rendered by PresentationController

Both are part of the same MOST video stream pipeline:
```
PresentationController renders → framebuffer (displayable)
  → videoencoderservice IPTE captures → QC OMX H.264
  → MPEG-TS → MLB ISO → MOST → VC LVDS
```

## KomoViewStyle — Widget Render Sizes

From `komoviewstyle.conf` (PresentationController config):

| Style ID | Name | Resolution | Description |
|----------|------|------------|-------------|
| 0 | KVS_Invalid | - | Invalid/unset |
| 1 | KVS_RGI | 285x276 | Old MIB1-style widget |
| 2 | KVS_RGI2 | 363x260 | Current widget (MU1316) |
| 3 | KVS_FPK | 800x480 | Full FPK display |
| 4 | KVS_Most | - | MOST video mode |
| 5 | KVS_Debug_MoKoInMainDisplay | - | Debug mode |

`setKomoViewStyle` is called once during startup. After startup → "received SetKomoViewStyle after startup => will be ignored!"

## DSIKOMOView — The Widget's DSI Interface

**Provider**: `libPresentationController.so` (native C++, `ServiceProviderDSIKomoViewImpl`)
**Consumer**: Java `KOMOService` in lsd.jxe

### DSI Methods (Java → Native)

| Method | Purpose |
|--------|---------|
| `setRouteInfo(RouteInfoElement[])` | Push maneuver data for rendering |
| `enableKomoView(bool)` | Enable/disable the widget view |
| `notifyVisibility(bool)` | Visibility notification |
| `setKomoViewStyle(int)` | Set render target size (style 0-5) |
| `selectManoeuvreView(int)` | Select maneuver view type |

### DSI Callbacks (Native → Java)

| Callback | Purpose |
|----------|---------|
| `updateKomoViewEnabled(bool, validFlag)` | Widget enabled state |

## Internal Processing Chain (libPresentationController.so)

### Entry: setRouteInfo()

```
Java KOMOService.setRouteInfo(RouteInfoElement[])
  → DSI IPC → libdsikomoviewproxy.so
  → ServiceProviderDSIKomoViewImpl.setRouteInfo()     [0x6236E0]
    validates riElements (null → "riElements not valid!")
    dispatches setRouteInfoTask to workloop
```

### Gate: ManeuvreViewControllerRequestInterface

**ALL DSI commands** pass through `ManeuvreViewControllerRequestInterface` at `this+104`:

```c
// At 0x624424 in setRouteInfoTask:
(*(this+104)->vtable[28])(this+104, routeInfoElements);
```

If `this+104` is **NULL** → logs "no ManeuvreViewControllerRequestInterface available!" → **data silently dropped**.

This interface is set by `ManeuvreViewStartupController` during native guidance initialization. **Without native navigation guidance active, it is NEVER set.**

### Processing: GuidanceViewController

When ManeuvreViewControllerRequestInterface IS valid:

```
setRouteInfoTask
  → ManeuvreViewControllerRequestInterface.setRouteInfo(riElements)
  → GuidanceViewController:
    CheckCurrentRouteInfoElement()  [0x6192C0]
      → determines element type (GVRIE_Maneuvre, GVRIE_Poi, etc.)
    CheckForceUpdateOfRouteInfoBoxData()  [0x618C90]
      → if maneuver: forces update of RouteInfoBox + road icon
    SetRouteInfoBoxData()  [0x63A5A0]
      → writes exit number, street direction, icon ID
    GetManeuvreArrowIconId()  [0x63D8DC]
      → maps maneuver type → arrow icon ID
```

### Rendering: SDKRenderTarget + Frame Timer

```
GuidanceView data update → sets redraw flag
  → MVCSDKMapDrawingManager state machine
  → StartNewFrame()  [0x5C7598]
    - Checks frame rate at this+104 (offset 26*4)
    - If rate == 0 → "Frame rate is 0! Wait until new frame rate mode"
    - If frame time not reached → timer waits
    - Else → renders immediately to SDK render target
  → SDKRenderTarget renders map + GuidanceView overlay
  → framebuffer ready for IPTE capture
```

### Frame Rate Control

```
SetFrameRateMode()  [0x5C7478]
  → sets frame rate for the render target
  → if rate was 0 and new rate > 0 → "restart rendering"
  → calls ChangeFrameRate() to adjust timer

Frame rates configured via:
  PC_MAP_CONTROL_BUSINESSLOGIC_FRAMERATE
  framerateForeground / framerateBackground
  FRAMERATEMODE_NORMAL_FRAMERATE / FRAMERATEMODE_BOOST_FRAMERATE
```

## GuidanceView State Machine

Controls when the widget starts/stops drawing:

```
Events:
  PC_MV_GV_IDLE             — idle state
  PC_MV_GV_START_DRAWING    — begin rendering
  PC_MV_GV_STOP_DRAWING     — stop rendering
  PC_MV_GV_UPDATE_VIEW      — view update
  PC_MV_GV_UPDATE_MANOUVRE_VIEW — maneuver update
  PC_MV_GV_TOGGLE_COMPONENT — toggle component visibility
  PC_MV_GV_AUTOTRANSITION   — auto transition
  PC_MV_GV_SHUTDOWN         — shutdown

States:
  StartDrawing [0x61C0D8]
    → checks activeMode via vtable call
    → if wrong mode → "Wrong activeMode - start drawing aborted"
    → if OK → sets this+12 = 1, fires event 501

  StopDrawing [0x61BF90]
    → entered when guidance stops
```

The state machine source: `PresentationController/map/ManeuvreView/src/Impl/PCMapManeuvreView/GuidanceView/StateMachine/GuidanceViewStateMachine_sm.cpp`

## Why Widget Stalls During CarPlay Navigation

### Root Cause Chain

1. **No native guidance active** → `ManeuvreViewStartupController` never initializes
2. → `ManeuvreViewControllerRequestInterface` at `this+104` stays **NULL**
3. → ALL `setRouteInfo()` calls from Java are **silently dropped**
4. → GuidanceView never receives data → never triggers redraw
5. → Widget shows initial/default state → **stalls**

Even if data DID reach GuidanceView:
- `GuidanceViewStateMachine` is in IDLE (never received `START_DRAWING`)
- Internal frame rate is 0 (never received `SetFrameRateMode`)
- No new frames would be produced

### Why enableKomoView(true) Doesn't Help

`enableKomoView(true)` → `UpdateEnableManeuvreStory` at [0x621C50]:
- Requires `IDSIKOMOViewListener` at `this+8`
- Calls through vtable to update enabled state
- But this only sets a flag — does NOT initialize the `ManeuvreViewControllerRequestInterface`

### Why notifyVisibility(true) Doesn't Help

Same pattern — sets visibility flag but doesn't trigger startup of the rendering pipeline.

## VC View States (FPK)

The VC's EB GUIDE state machine has these FPK view states:

| View State | Trigger | Renders |
|------------|---------|---------|
| Compass (default) | Always available | Compass rose |
| `SV_NavFPK_Compass_MobileDevice` | InfoStates=6 | AIO arrows from KOMO |
| `SV_LVDS_NavMap_FPK` | LVDS_Available + content=Map | LVDS video stream |

**There is NO dedicated BAP RGI view on FPK.** Maneuver rendering requires either:
- InfoStates=6 → **blocked by KSS** (`sub_108F42C` rejects value 6)
- LVDS video with rendered content → **requires PresentationController to render**

## Three DSI Interfaces for KOMO

| DSI Interface | Native Provider | Status on MU1316 |
|---------------|----------------|-------------------|
| DSIKOMOView | libPresentationController.so | **Available** — but GuidanceView doesn't render |
| DSIKOMONavInfo | None | **Missing** — no native provider |
| DSIKOMOGfxStreamSink | None | **Missing** — no native provider |

### gfxAvailable Gate (Java side)

`gfxAvailable` is set by `DSIKOMOGfxStreamSink.updateGfxState(1, 1)`. Since no provider exists, it's never set naturally.

Java `ClusterViewMode` uses `gfxAvailable` to gate view modes:

| ViewMode | Needs gfxAvailable? | BAP rgType |
|----------|---------------------|------------|
| MOSTMapViewMode | **YES** | 3 (MOST) |
| KDKViewMode | **YES** | - |
| LVDSMapViewMode | **NO** (needs kombiMapReady) | 4 (LVDS) |
| RGIViewMode | **NO** (needs descriptorValid + rgiStringValid) | 0 (RGI) |
| CompassViewMode | **NO** (always available) | - |

## Native BAP Output (from PresentationController)

PresentationController generates BAP data during native guidance:

```
listener_updateBapManeuverDescriptor  → BAP FctID 23
listener_updateBapManeuverState       → BAP FctID 24
listener_updateBapManeuverInformation → BAP FctID 23 (info variant)
listener_updateDistanceToNextManeuver → BAP FctID 18
listener_updateBapTurnToInfo          → BAP FctID 25
listener_updateRgLaneGuidance         → BAP FctID 22
```

These go via DSI → Java `DSINavigation` listener → `CombiBAPListener` → BAP → MOST → VC.

Our BAPBridge bypasses this by calling `CombiBAPServiceNavi` directly.

## Fix: Option A — Patch libPresentationController.so (IMPLEMENTED)

Persistent binary patch on HU filesystem. Three patches remove native-side blockers
so Java (CarPlay) can drive the widget renderer.

**Patch script**: `tools/patch_libpresentationcontroller.py`
**Binary**: `/apps/PresentationController/lib/libPresentationController.so`

### Patch 1: NOP StopDSIs (0x60BE48)

`ManeuvreViewManager_StopDSIs` unregisters DSIKOMOView + DSIMapViewerManeuverView
services AND clears offsets 104 and 116. Replace with `MOV R0,#0; BX LR` to keep
DSI interfaces alive after native guidance stops.

```
0x60BE48  F0 40 2D E9 B0 30 9F E5  →  00 00 A0 E3 1E FF 2F E1
```

### Patch 2: Force StartDrawing (0x61C11C)

`GuidanceView_StartDrawing` checks activeMode via ManeuvreViewControllerRequestInterface.
During CarPlay the mode is wrong. Change BNE (conditional) to B (unconditional).

```
0x61C11C  0A 00 00 1A  →  0A 00 00 EA
```

### Patch 3: Redirect frame rate reads (0x5C75A0, 0x5C75E4, 0x5C783C)

`StartNewFrame` reads fps from "active" field (offset 0x68) but `SetFrameRateMode`
(called from Java `setKOMODataRate()`) writes to "stored" field (offset 0x54). The
propagation chain 0x54→0x64→0x68 requires native guidance context. Redirect all 3
LDR instructions to read 0x54 directly so Java controls fps.

```
0x5C75A0  68 60 90 E5  →  54 60 90 E5   (LDR R6,[R0,#0x68] → [R0,#0x54])
0x5C75E4  68 10 95 E5  →  54 10 95 E5   (LDR R1,[R5,#0x68] → [R5,#0x54])
0x5C783C  68 10 95 E5  →  54 10 95 E5   (LDR R1,[R5,#0x68] → [R5,#0x54])
```

Java controls: `setKOMODataRate(2)` → renders, `setKOMODataRate(0)` → stops.

### Frame Rate Architecture (MVCSDKMapDrawingManager)

Object layout (constructor at 0x5C8070):
```
+0x00  osal::Timer base
+0x50  lastFrameTimestamp
+0x54  stored fps         ← SetFrameRateMode WRITES here
+0x58  configured rate (mode 2 value)
+0x5C  ChangeFrameRate sub-object
+0x60  dirty flag (byte)
+0x64  pending fps        ← ChangeFrameRate writes here
+0x68  active fps         ← StartNewFrame WAS reading here (now patched to 0x54)
```

Normal propagation: SetFrameRateMode(0x54) → ChangeFrameRate(0x64) → DSIHasChanged(0x68).
Without native guidance the 0x54→0x64→0x68 chain breaks. Patch 3 bypasses it.

### Dependency Chain

```
Patch 1 (StopDSIs NOP)  ←  REQUIRED FIRST
  ├── Patch 2 (StartDrawing) depends on Patch 1 (dereferences offset 104)
  └── Patch 3 (fps redirect) independent but useless without 1+2
```

### Prerequisite

ManeuvreViewManager_Start must have run at boot (sets offset 104). Evidence: widget
shows initial state, DSIKOMOView service IS registered.

### VC Prerequisite

The VC must be in a state that displays the video stream:
- `SV_LVDS_NavMap_FPK` requires `LVDS_Available=1` + `INTERN_Active_NavFPK_Content=Map(1)`
- Triggered by: `gfxAvailable=true` (forced via reflection in BAPBridge.forceGfxAvailable)
  → BAP rgType=4 → VC MAP mode → content=Map(1), combined with MOST video → LVDS_Available=1

## Key Addresses in libPresentationController.so

| Address | Function | Purpose |
|---------|----------|---------|
| 0x60BE48 | ManeuvreViewManager_StopDSIs | **PATCH 1** — NOP (keep DSI alive) |
| 0x61C11C | GuidanceView_StartDrawing (branch) | **PATCH 2** — BNE→B (force mode check) |
| 0x5C75A0 | StartNewFrame (fps read 1) | **PATCH 3a** — LDR offset 0x68→0x54 |
| 0x5C75E4 | StartNewFrame (fps read 2) | **PATCH 3b** — LDR offset 0x68→0x54 |
| 0x5C783C | StartNewFrame (fps read 3) | **PATCH 3c** — LDR offset 0x68→0x54 |
| 0x60CAAC | ManeuvreViewManager_initStartupAndSetInterfaces | Sets offset 104+116, starts DSIs |
| 0x60CCF8 | ManeuvreViewManager_Stop | Calls StopDSIs + stops render control |
| 0x60BF1C | ManeuvreViewManager_StartDSIs | Registers DSI service providers |
| 0x5C7478 | MVCSDKMapDrawingManager_SetFrameRateMode | Writes fps to offset 0x54 |
| 0x5C706C | GetFrameRateForMode | Mode→fps: 0→0, 1→15, 2→config, 4→5, 5→10 |
| 0x5C79E8 | ChangeFrameRate | Writes pending fps to 0x64 (bypassed) |
| 0x5C7AF0 | DSIHasChanged | Copies 0x64→0x68 (bypassed) |
| 0x5C8070 | MVCSDKMapDrawingManager_ctor | Initializes all fields to 1 |
| 0x6236E0 | ServiceProviderDSIKomoViewImpl_setRouteInfo_entry | DSI entry, validates riElements |
| 0x6243D8 | ServiceProviderDSIKomoViewImpl_setRouteInfoTask | Forwarding gate (checks offset 104) |
| 0x61C0D8 | GuidanceView_StartDrawing | Checks activeMode via offset 104 |
| 0x61BF90 | GuidanceView_StopDrawing | Stops widget rendering |
| 0x6192C0 | CheckCurrentRouteInfoElement | Element type determination |
| 0x618C90 | CheckForceUpdateOfRouteInfoBoxData | Force maneuver box update |
| 0x63A5A0 | SetRouteInfoBoxData | Write icon/street/exit data |
| 0x63D8DC | GetManeuvreArrowIconId | Map maneuver → arrow icon |
| 0x6231A0 | KomoViewProvider_ctor | Subclass ctor, sets offset 104=0 |
| 0x771288 | ServiceProviderDSIKomoViewImpl_ctor | Base class ctor |
| 0x772044 | handleStopDsiUnGuarded | Unregisters DSI service |
