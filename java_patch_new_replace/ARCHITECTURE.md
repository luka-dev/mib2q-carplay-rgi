# java_patch_new — clean-room redesign of the MHI2Q CarPlay Java patch

Full rewrite (decided 2026-07-27), compiled against the accurate full decompile
`Tools/jxe2jar/out/MU1316-combined-final.jar` (not the old `lsd.jar`). Java **1.2**
target (MU1316 lsd runtime). Ports the proven RGD/coverart *algorithms*; redesigns the
*foundation* (entry, framework access, bus, module lifecycle) and adds the cluster **altScreen**.

## Integration seam (unchanged, proven)
A tiny hook call is inserted into ONE stock class and the patched stock classes override
lsd at inject time. Entry:
- `de/audi/app/terminalmode/combi/TerminalModeBapCombi$ActiveDeviceStateListener` → calls
  `CarPlayApp.onActivate(ctx)` / `onDeactivate()` on CarPlay connect/disconnect.
- `ctx` → `IFrameworkAccess` → HMI services (models, DSI providers, NavigationEnv/ClusterService).

## Module map (`com.luka.carplay.*`)
- **`bus`** — `CarplayBus`: **bidirectional** TCP link to the C hook (Java = server :19810).
  Receives hook events AND sends `CMD_*` (the old patch's `send()` was a stub — now a real writer).
- **`core`** — `CarPlayApp` (entry/lifecycle), `FrameworkRef` (holds IFrameworkAccess + service lookups),
  `Module` (start/stop lifecycle interface).
- **`rgd`** — route guidance (port ManeuverMapper/SideStreets/BAPBridge/RouteGuidance logic).
- **`coverart`** — cover-art bus event → BAP.
- **`input`** — touch/cursor (main screen) + the cluster roller listener.
- **`altscreen`** — NEW cluster integration (below).

## altScreen cluster design (RE-grounded)
Cluster tab lives on the **kombisync** layer as top-level context **117** (SMARTPHONE_CARPLAY /
0x75) — NOT a `ClusterViewMode` sub-mode. Two axes:
1. **License / availability** — `MenuState.smartPhone = AVAILABLE(2)` (byte5 b6-7 / `ZR_Sta_SmartPhone`)
   pushed via the kombisync provider on phone-connect; `NOT_EXISTING(3)` on disconnect. MU1316 **never
   sets this** (verified: only the RX deserializer writes it) → we add the sender. VC also gates on
   coding 4237/4238.
2. **Active switch** — watch `DSIKombiSync DisplayRequestResponse.mainContext`; `==117` ⇒ our tab is
   active → feed altScreen MOST video + route roller→`CMD_ALT_HID` + hide the KDK maneuver render;
   `!=117` ⇒ stop video, show KDK render, roller to native menu. Force via `setMMIDisplayStatus`.
- **Roller capture** — a `VirtualButtonListener` (template `TMVirtualButtonListener`): `increment/
  decrement`→`CMD_ALT_HID [wheel]`, `keyPressed(select)`→`[buttons]`; only while tab active.
- **KDK render hide** — gate on active tab; do NOT touch BAP (projection rides `CombiBAPListener`).

## Build
`build_java_new.sh` → compile `java_patch_new/**.java` against `MU1316-combined-final.jar` + OSGi →
`build/carplay_hook_new.jar`. Same deploy path as the old patch.

## Status
- [x] Folder + build script + this doc
- [x] `bus.CarplayBus` — bidirectional (reader + real writer)
- [x] `core` — `Module` + `FrameworkRef` (IContext→framework/services/hmiServiceApp) + `CarPlayApp`
      (onActivate/onDeactivate + retry-until-ready). Compiles; framework seam verified vs combined-final.
- [x] `bus.CarplayBus.Data` + `parseText` ported (text-payload parser; unblocks rgd/coverart)
- [x] `coverart.CoverArt` ported (bus import swap only; callback→BAP unchanged). Compiles.
- [x] rgd ported (all 7: ManeuverMapper/SideStreets/RendererMapper/GatedCombiService/RendererServer/
      RouteGuidance/BAPBridge). Foundation swap: `CarPlayHook.getFrameworkAccess()`→`CarPlayApp.framework()`
      (new static accessor over `fwRef`), `framework.CarplayBus`→`bus.CarplayBus`. Compiles.
- [x] entry seam — patched stock `TerminalModeBapCombi`: ActiveDeviceStateListener.updateActiveDeviceState
      calls `CarPlayApp.onActivate(ctx)` (ACTIVATING+carplay) / `onDeactivate()` (NOT_ATTACHED+carplay).
      Fixed decompiler `class$` artifact → `.class` literal. Compiles (16 files).
- [x] patched stock `DisplayManagerMIB2High` — array [79]→[83], gated kombiType!=4, 3 contiguous
      custom contexts dc[80]={98,102,101}, dc[81]={98,102,101,33}, dc[82]={98,102,101,99} (see below).
      `@Override` stripped for -source 1.2.
- [x] patched stock `ClusterService` — 4 CarPlay accessors grafted onto combined-final stock
      (getDSIResponseContainer/triggerRefreshRGIValid/get+setCombiBAPListenerCombiService); the 2
      renderer-pipeline methods are STUBS (old switchContext(74)+steal-20 hack dropped — replaced by ctx-80/82).
- [x] renderers on shared **`common/cluster_surface.{c,h}`** — raw `screen_create_window` +
      `screen_manage_window("How are you gentlemen?")` + MANAGER_STRING watchdog.  maneuver_render (id 98,
      RGBA/GL) dropped libdisplayinit+dmdt (968→391 lines); altscreen_render (id 99, NV12/OMX) dropped
      physical-bind+group-host.  Both `make hostcheck` PASS.  ON-UNIT UNVALIDATED (raw+manage path).
- [x] wire rgd as a `Module` — `RgdModule` adapter in MODULES[]: waits for CombiBAPServiceNavi
      (retry-until-ready via startPass/startRetry) → RouteGuidance.init(navi) → subscribe EVT_RGD_UPDATE.
      onActivate starts it, onDeactivate stops it. Compiles (17 files).
- [ ] ClusterService FUNCTIONAL patches (not needed to compile; needed on-unit): setKOMODataRate FPK guard,
      FctID18 `!showBargraph` fix (:305), setValidRGTypeReceived, getKOMOHintsRaw
- [x] input (cursor + roller) — both done (seam injections into the DSI controller)
- [x] altscreen driver — `AltScreenModule` (in MODULES[]): acquires HMI DisplayManager
      (framework→HMIService→getDisplayManager); ClusterService renderer stubs now call
      `AltScreenModule.switchCluster(80)` / `restoreCluster()` → `switchContext(80,1,null)`.
- [x] coverart — merged into the patched `TerminalModeBapCombi.EventListener` (owns the stock
      now-playing BAP path): implements CoverArt.Callback, subscribes EVT_COVERART, merges the
      artwork `setPicture(id,path)` into the now-playing `CombiBAPCurrentStationInfo`. Compiles.
- [x] cursor — whole-class patch of stock `CarplayDSILifecycleController` (VF decompile): inner
      `TerminalModeDSIKeyEventsController.updateTouchEvents` routes touchpad single-finger → `CursorController`
      DPAD ticks + `installCursorTouchSink` → `dsiCarplaySafe.postButtonEvent`.  Cleaned 25 decompiler
      artifacts (class$→`.class`, 7 AbstractDispatcherRunnable anon ctor/capture).  Build now **1.4/1.4**
      (jclfoun11 = Foundation 1.1 = J2SE 1.4).  Compiles (20 files, major 48).
- [x] input roller — NOT a VirtualButtonModel registration after all: the MMI knob already routes
      through the DSI controller (`TMVirtualButtonListener.increment/decrement → keyEventListener.updateRotary`;
      knob press → `updateKey` DDS_SELECT).  So injected into the SAME patched
      `CarplayDSILifecycleController$TerminalModeDSIKeyEventsController`: `updateRotary` → CMD_ALT_HID
      [i8 wheel] and `updateKey`(DDS_SELECT) → CMD_ALT_HID [buttons bit0], gated on
      `AltScreenModule.isClusterActive()` (set when the cluster is switched to dc[80/81/82]).  Else stock.
      Compiles.

### altScreen cluster contexts — LOCKED (2026-07-27, RE-verified vs combined-final)
`DisplayManagerMIB2High.defineContexts()` builds `DisplayContext[79]` (kombiType!=4); stock cluster family
is `dc[73]={20,102,101}`, `dc[74]={20,102,101,33}` (KDK overlay 20 + KDK-bg imgs 102/101 + stock kombi map
33).  Our renderers do NOT steal 20/33 — each opens its OWN managed window under a free displayable id
(picked from 61-99, above the semantic enum max 60=HUD_MAP_VIEW, below KDK 100-102 so still in DM range):
**98 = maneuver graphics overlay, 99 = CarPlay cluster video.**  We enlarge the array to `[83]` and add
3 contiguous custom contexts (A5-class only, kombiType!=4):
- `dc[80] = {98,102,101}`      — maneuver graphics + KDK bg
- `dc[81] = {98,102,101,33}`   — maneuver graphics over stock kombi map (hybrid)
- `dc[82] = {98,102,101,99}`   — maneuver graphics (top) over CarPlay video (99, bottom)
Our id is always `displayables[0]` (the MOST encoder captures the DM-composited context; it takes the WHOLE
context, so 102/101/33 composite as layers).  On tab-active Java calls `switchContext(1, <ctx>)`; stock still
owns 20/33 (no double-owner → no render-kill).
