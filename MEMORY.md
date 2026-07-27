# MHI2 CarPlay Route Guidance - Key Patterns

## Hook ↔ Java bus (TCP 127.0.0.1:19810)
Full-duplex TCP bus replaces every PPS channel between `libcarplay_hook.so` (server, inside dio_manager) and `carplay_hook.jar` (client, inside lsd).

- Wire format: 16-byte BE header `{magic 'CPHB', seq, type, flags, rsvd, len}` + payload.
- Text payloads (default) reuse QNX PPS `key:type:value\n` format for minimal consumer diff. Binary payloads flagged with `FLAG_BINARY`.
- Types split `0x0000-0xFF` (EVT_*, Hook→Java) and `0x0100+` (CMD_*, Java→Hook).
- `FLAG_STICKY` types are cached server-side; on `CMD_SYNC_REQ` the hook replays them wrapped in `EVT_SYNC_BEGIN`/`EVT_SYNC_END`. This restores PPS's "new subscriber gets current state" semantics.
- Send policies per type: `RELIABLE` / `LOSSY` (overwrite same-type pending) / `DROP_OLD`.
- Files: `c_hook/framework/bus.{c,h}`, `c_hook/framework/bus_protocol.h`, `java_patch/com/luka/carplay/framework/CarplayBus.java`.
- Replaced paths: `/ramdisk/pps/iap2/coverart_notify` → `EVT_COVERART`; `/ramdisk/pps/iap2/routeguidance` → `EVT_RGD_UPDATE`. PPS writer + PPS.java removed.
- PPS delta-delivery burst bug no longer applies — TCP delivers every frame in order, no coalescing.

## Transient maneuver_count=0
iOS briefly sends maneuver_count=0 at route start before re-populating. Don't clear per-slot Java data (mType, mTurnAngle, etc.) on count=0 - only clear on `route_state=0` or `source_supports_rg=0`.

## Key Files
- `c_hook/framework/bus.{c,h}` - hook-side TCP bus (server on 19810)
- `c_hook/framework/bus_protocol.h` - shared wire-format spec (C + Java)
- `java_patch/.../framework/CarplayBus.java` - Java client, per-type listener registry
- `c_hook/routeguidance/rgd_hook.c` - C hook main, slot mapping, publishes EVT_RGD_UPDATE
- `c_hook/routeguidance/rgd_tlv.h` - TLV IDs, data structures (rgd_maneuver_t, rgd_update_t)
- `c_hook/coverart/coverart_hook.c` - publishes EVT_COVERART on JPEG decode
- `c_hook/cursor/cursor_hook.c` - CMD_CURSOR_POS/HIDE receiver, NV12 overlay (WIP)
- `java_patch/.../BAPBridge.java` - BAP bridge via Navigation→ClusterService→CombiBAPListener chain
- `java_patch/.../RouteGuidance.java` - Bus listener, state container, dirty tracking
- `java_patch/.../cursor/CursorClient.java` - cursor Java API (show/hide)
- `java_patch/.../ManeuverMapper.java` - iAP2 type → BAP mainCategory/direction mapping
- `java_patch/.../SideStreets.java` - Side streets byte computation
- `java_patch/.../KOMOCaller.java` - Patched stock KOMOCaller: getKomoViewStyle()=1 for FPK (see fpk_presentation.md)

## BAPBridge Architecture (refactored Mar 2026)
BAPBridge → AppConnectorNavi (CombiBAPServiceNavi) directly. ACTIVE_RGTYPE=0 (RGI for HUD).
- **rgType=0** = RGI mode. HUD reads ManeuverDescriptor from BAP. VC FPK ignores rgType entirely.
- KOMO/GAL removed - no InfoStates override, no DSIKOMOView, no MobileDevice mode.
- LVDS map stays on VC naturally (driven by `INTERN_Active_NavFPK_Content`=Map, not by InfoStates).
- BAP text overlays (distance, turn-to, street) work independently of video stream.
- **FPK guard blocks KOMO**: `Util.isClusterMapFPK()` = `getSysConst(0)==2`. `refreshViewMode()` returns early for FPK/MMI → viewMode stuck COMPASS, rgSelect=2, rgType=4. **Fix**: `forceClusterRouteInfoState(true)` in BAPBridge sets container.rgActive, rgiValid=true, favoredViewMode=KDK(2), KDK handler state via reflection.
- exitViewNum always 0 (1 overwrites icon to highway exit)

## VC FPK State Machine - Full Architecture (see `vc_fpk_state_machine.md`)
- gssipc-kbd = BAP→dp gateway. fds = EB GUIDE HMI runtime (state machine).
- VC has NO ActiveRGType dp item - uses InfoStates to switch modes.
- InfoStates=6 → `SV_NavFPK_Compass_MobileDevice` (renders AIO arrows from KOMO)
- `INTERN_Active_NavFPK_Content`: None=0→Compass, Map=1→LVDS video, LastDest=2, Fav=3
- LVDS map: `LVDS_Available`=1 + content=Map → `SV_LVDS_NavMap_FPK` (MOST video stream)

## Java 1.2 Compatibility
All Java patches must be Java 1.2 compatible (no generics, no autoboxing, no enhanced for loops). Compile with `-source 8 -target 8`.

## BAP Bargraph
Bargraph denominator = `mDistance[firstIdx]` (distance_between from 0x5202). If -1 or 0, bargraphOn=false. displayMode param of `updateDistanceToNextManeuver` controls whether cluster shows bargraph.

## FctID 18: Distance vs Bargraph - Mutually Exclusive (Fixable)
BAP spec defines distance number and bargraph as **independent records** in the same FctID 18 message - both should display simultaneously. However, `ClusterService.java:305` in the cluster firmware enforces mutual exclusivity: `if (showDistance && distance > 0 && !showBargraph)` → distance visible, else hidden. Model 64 = distance metrics, Model 65 = bargraph choice. **FIX**: Patch `ClusterService.class` to remove `!showBargraph` from the condition → `if (showDistance && distance > 0)` → always show distance number. Then both distance text and bargraph bar render together. File: `de/audi/tghu/navi/app/cluster/ClusterService.java:305`.

## Slot Mapping
C hook remaps iOS monotonic maneuver indices to 8 fixed slots [0..7] with LRU eviction. `maneuver_list` in PPS contains slot indices, not iOS indices.
- **LRU eviction bug (fixed)**: `rgd_slot_for_iap_index()` didn't refresh seq on existing-slot lookup, and had no eviction protection for active ManeuverList slots. Incoming 0x5202 data for future maneuvers evicted active slots → `maneuver_list` never updated in PPS → icon + turn-to text stuck. Fix: LRU touch on lookup + `rgd_is_active_index()` eviction guard.

## CarPlay VC Video Pipeline (libairplay.so internals)
When HU hides CarPlay, it sends RTSP TEARDOWN (stream type 110). Chain: `_requestProcessTearDown` → `AirPlayReceiverSessionTearDown(session, dict, err, &flag)` → `_ScreenTearDown` → `SendCommand('q')` to pipe → `ProcessFrames` exits loop → OMX decoder drains → no more frames.
- `dict != NULL` = RTSP teardown (specific streams). `dict == NULL` = full disconnect/finalize.
- `ProcessFrames` is the main H.264 receiving loop (select on AirPlay socket + control pipe).
- `SetVisibility` is never called (only in a vtable, never code-referenced).
- Hook `AirPlayReceiverSessionTearDown`: block when `dict != NULL` + `keep_alive=1`.

## FSG Sync & ExitView (FctID 49)
- CombiBAPListener.applyManeuverDescriptor() internally sends {23,18,49}
- exitViewNum=1 **overwrites maneuver icon to highway exit** regardless of actual maneuver - never use exitViewNum=1
- **FSG sync(1) stall root cause (fixed Mar 2026)**: AppConnectorNavi uses sendStatusIfChanged internally. When exitView variant+num is unchanged between sends, FctID 49 is skipped → sync(1) for {23,18,49} never closes → ManeuverDescriptor updates silently dropped. FOLLOW_STREET worked because it doesn't trigger sync(1). **Fix**: Toggle exitView variant (EU↔NAR) on every send via `sendExitView()`. Since exitViewNum=0 (no exit view), variant is cosmetically irrelevant.
- rgType priming and RGStatus re-assertion not needed - AppConnectorNavi handles FSG sync internally when updateManeuverDescriptor() is called (triggers startSync(1) for non-FOLLOW_STREET descriptors)
- `forceClusterRouteInfoState(true)` must be called on start even in BAP-only mode (without KOMO) - sets container.rgActive, rgiValid, favoredViewMode=KDK so VC accepts RGI data

## CarPlay Cluster Map (AltScreen) - Backport from MHI3
- **Feature bit 26** (`0x4000000`) = AltScreen - already set on BOTH MHI2 and MHI3
- AltScreen = stream type **111** (MainScreen = 110)
- `AirPlayAltScreenDictCreate`: keys uuid, type(111), maxFPS, w/hPixels, w/hPhysical, altScreenURLs(optional)
- `HasFeatureAltScreen` reads `session->platformData[0x18][0x127]` (byte bool)
- MHI2 `CopyDisplaysInfo` returns flat CFDict (1 screen); MHI3 returns CFArray
- MHI2 `Screen_Setup` only extracts `latencyMs`, no stream type routing
- MHI3 video: `CESOGfxVideo` class; MHI2 video: direct OMX (`COMXVideoDecoder`)
- Hook: `c_hook/cluster/carplay_cluster_hook.c` - Phase 1 injects altscreen dict into CopyDisplaysInfo
- Key binaries: MHI2 `libairplay.so` (ARM32 QNX), MHI3 (AArch64 Linux)

## BAP FunctionSync (FctID 37)
FunctionSync property = FctID 37 on LSG 50. Sync type 0 = {17,39,23,18,49} (open=`{0:0:97:0:1:0:64:0}`). Sync type 1 = {23,18,49} (open=`{0:0:33:0:0:0:64:0}`). Close = `{0:0:0:0:0:0:0:0}`. Defined in `FunctionSynchronizationNavi`. State machine: IDLE→STARTING→RUNNING→COMPLETED.

## MHI2Q LSD Audit Fixes (Feb 2026)
- **Play position sentinel**: Was `-65536`, fixed to `65535` (`0xFFFF` = BAP "unknown time"). File: `TerminalModeBapCombi$EventListener.java:126-127`.
- **Ferry mainElement=44**: MHI3 tbt_renderer uses 44, but MHI2Q BAP spec only goes to 38 and IconUtil has no case 44. Changed to `FOLLOW_STREET` (11). Enter/change/exit ferry all now map to FOLLOW_STREET.

## FPK Presentation Sidebar - Full Architecture (see `fpk_presentation.md`)

## HU Maneuver Rendering Pipeline (see `hu_maneuver_rendering_pipeline.md`)
- `libPresentationController.so` (in PNavApp) is the ONLY thing rendering maneuvers into MOST video stream
- Creates KVS_FPK render targets (210x153 / 328x181) via SDKRenderTargetFactory
- `videoencoderservice` captures via IPTE → QC H.264 HW encode → MPEG-TS → MLB ISO TX → MOST → VC
- CarPlay data does NOT flow through this pipeline - only native nav does
- BAP text overlays (BAPBridge) work independently on VC regardless of video content

## libPresentationController.so Widget Patches (Option A) — IMPLEMENTED
- **Root cause**: `ManeuvreViewManager_Stop` calls `StopDSIs` which unregisters DSI services AND clears `ManeuvreViewControllerRequestInterface` (offset 104) and selectManoeuvreView interface (offset 116). After Stop, all Java DSI calls are silently dropped.
- **Patch script**: `patch_libpresentationcontroller.py`
- **Stock binary**: `libPresentationController.so.stock` / **Patched**: `libPresentationController.so`
- **3 patches** (all ARM32, file offset = VA, base=0):
  1. `0x60BE48`: NOP `StopDSIs` (MOV R0,#0; BX LR) — keeps DSI interfaces alive
  2. `0x61C11C`: BNE→B in `StartDrawing` — forces mode check pass
  3. `0x5C75A0`, `0x5C75E4`, `0x5C783C`: LDR offset 0x68→0x54 in `StartNewFrame` — Java-controlled fps via `setKOMODataRate()`. Rate 0 stops rendering, rate 2 resumes.
- **Frame rate architecture**: SetFrameRateMode writes fps to offset 0x54. StartNewFrame was reading 0x68 (double-buffered via 0x54→0x64→0x68 chain that breaks without native guidance). Patch 3 redirects reads to 0x54 directly.
- **Dependency**: Patch 1 MUST be applied first (patches 2 crashes without it, offset 104 dereferenced)
- **Prerequisite**: ManeuvreViewManager_Start must have run at boot (sets offset 104). Evidence: widget shows initial state.
- **No Java changes needed** — BAPBridge.java already has full KOMO lifecycle (startKOMO/updateKOMO/stopKOMO)
