# altScreen — Behavior Implementation Plan (RE-grounded 2026-07-28)

Synthesis of 3 RE passes (availability / KDK-popup / stock-nav-cancel) vs MU1316 decompile +
the old proven patch.  Companion to BEHAVIOR_SPEC.md.  PROVEN (old patch) vs NEW/UNVERIFIED.

---

## A. Announce the tab (phone connect) — availability
`DSIKombiSync.setMenuState(MenuState)` (RT 1006; setMenuState is on the interface, the object we
hold in `KombiSyncController.addDSIService` already has it):
```java
MenuState ms = new MenuState();                    // 24 int fields = 0 (INIT)
ms.smartPhone = DSIKombiSync.MENUSTATE_AVAILABLE;  // 2   (NOT_EXISTING = 3)
kombiSync.setMenuState(ms);
```
✅ RESOLVED (4th RE pass, 2026-07-28 — agents A[HU] + B[VC AU491]):
1. **Channel = MenuState IS MMI_Status (same wire, two names).** VC (AU491) has NO MenuState/KombiSync
   availability consumer at all — tab availability is exclusively BAP `MMI_Status` (`gss_ipc` decodes the
   6-byte/24×2-bit frame → dp `MMI_Status_Stat_Smartphone` = **0x20002f9** on AU491, not 0x20002e6=TT).
   On the HU, `MenuState.smartPhone` (byte5 bits6-7) is what the **native BAP-Kombi composer** packs into
   that MMI_Status frame. So `setMenuState(smartPhone=AVAILABLE)` DOES raise the tab — through MMI_Status.
   **Enabling value = AVAILABLE(2).** (1=greyed, 3=hidden, 0=default.)
2. **Provider / who sets it.** HU-Java `DSIKombiSyncProvider` = dead proxy, never instantiated; NO business
   code calls setMenuState. The native composer feeds the smartphone bit from projection (`gal`/`dio_manager`)
   **+ coding license**: sysConst **4237 (isAppleDIO / CarPlay, car-coding byte51 bit0)** / **4238 (isGoogleGAL/
   AA, byte43 bit7)**. If coded → MMI_Status.SmartPhone=AVAILABLE(2) is emitted **at boot, natively, no jar**.
   → If the A5 is coded for CarPlay, the tab likely appears WITHOUT our code. Our client `setMenuState` may be
   a no-op (native recomputes) or may work (same wire) — the on-unit probe (+coding dump) decides.
3. 🔴 **The real A5 blocker is NOT the tab — it's video-plane routing.** Tab bit only makes the tab EXIST.
   To show our video in it: `LVDS_Available (0x20002d7)=1` (MOST H.264 present) + projection/phone-connected
   state + `KombiSync_MainContext=117`; else fds shows `SV_LVDS_Smartphone_NA_*` / NoSmartphoneConnected popup.
   That's exactly our altScreen pipeline (ctx 82={99,102,101} + setUpdateRate + DISPLAYSTATUS_LVDS_DM_ACTIVE).

## B. Contexts — ONE segmented MOST stream (map fullscreen + KDK popup), mirrors stock ctx74
Stock `ctx74 = {20,102,101,33}` is ONE stream the VC lays out: **33 map FULLSCREEN + 20 KDK POPUP**
box (top-right "2,9km/117km" in the reference photo). displayables[0] = the active/popup plane (20),
last entry = the fullscreen bg (33). Our mirror:
- `dc[80]={98,102,101}` = maneuver KDK popup only (CarPlay nav, no video). CORRECT.
- `dc[82]=`**`{98,102,101,99}`** = maneuver KDK POPUP (98, active [0]) + bg + CarPlay video 99 FULLSCREEN
  bg. **Both at once** (map/video full-screen AND maneuver popup) — NOT video-only. (Reverted the
  earlier wrong `{99,102,101}`; user corrected: the stream is segmented, not either/or.)
🔴 GEOMETRY: stock VC hardcodes id 20=popup / map-slot=fullscreen (CombiMapController.getKdkDisplayable→20).
Our custom ids 98/99 don't inherit that → we must set it ourselves: `DM.setPosition/setCropping`
(99 → fullscreen 0,0,W,H; 98+102+101 → popup rect). OPEN: does the VC honour our DM-composited
geometry (Model 2, "encoder captures whole context") or route only by hardcoded id (Model 1)? → on-unit.
Clean-fullscreen note (RE): the VC's SV_LVDS_Smartphone view carries NO nav-map overlays, so driving
video into the Smartphone LVDS context avoids the Nav_Map_Scale/Altitude/Streetname chrome entirely.

## C. Show KDK popup with OUR plane 98 (CarPlay nav) — proven driver, adapted 20→98
1. maneuver_render registers window ID="98" + first frame. ✅ (CR_DISPLAYABLE_ID=98)
2. dc[80]={98,102,101} declared. ✅
3. `forceClusterRouteInfoState(true)` (rgActive/rgiValid). *(proven, ported BAPBridge)*
4. **BOUNCE:** `switchContext(72,1,null)` → sleep ~180ms → `switchContext(80,1,null)` → `setActiveDisplayable(4,98)`.
   🔴 our `switchCluster` does a SINGLE switch — MISSING the bounce (single switch to already-active ctx short-circuits, no preContextSwitchHook, no encoder update).
5. `forceGfxAvailable(true)` (3-strategy reflection) — VC display-enable. *(proven, ported BAPBridge)*
6. `dm.setUpdateRate(1,10)` — start MOST encoder. *(proven, old ClusterService.activate)*
7. (opt) BAP `setSupplementaryMapVisibility(true)`.
⚠️ Geometry: CombiMapController crops/opacities id 20 only; direct switchContext(80) bypasses it →
98 renders full-window default → maneuver_render must paint the exact popup rect itself.
🔴 our `ClusterService.activateCustomRendererPipeline` = just `switchCluster(80)` (single) → replace
with real driver (bounce + setUpdateRate) + BAPBridge forceClusterRouteInfoState+forceGfxAvailable.

## D. Suppress popup (altScreen focused / no CarPlay nav) — mirror of C
- `switchContext(72,1,null)` (map) or `75` (empty) — OFF ctx 80.
- `setUpdateRate(1,0)`; BAP `setSupplementaryMapVisibility(false)`; `forceGfxAvailable(false)`; `forceClusterRouteInfoState(false)`.

## E. Cancel stock nav + block RG-restart — ALREADY PORTED (BAPBridge + GatedCombiService)
```java
// CANCEL (guard rm.getRoute()!=null):
csRef.setRouteGuidanceAborted();
Navigation.getInstance().getRouteManager().stopRouteGuidance();
// BLOCK (cancel THEN block):
gatedService.blockRouteGuidance = true;   // onStart  (drops only RG methods)
gatedService.blockRouteGuidance = false;  // onShutdown
```
🔴 BOUNDARY: stock nav → `CombiBAPListener.combiservice` (we wrap = GATED); our maneuvers →
`appConnectorNavi` (raw OSGi, bypasses CombiBAPListener = NEVER gated). Sound. On-unit: log `==`
identity of the two refs once.

## F. State machine — inputs all available
- altTabFocused = `KombiSyncController.isSmartphoneTabFocused()` ✅
- carplayNavActive = `RouteGuidance.State` ✅
- stockNavActive = `Navigation.getInstance().getRouteManager().getRoute() != null` ✅

| carplayNav | altFocused | stockNav | cluster |
|---|---|---|---|
| no | — | — | all stock, no widget |
| yes | no | — | KDK popup = our 98 (ctx 80, driver C) |
| yes | yes | — | altScreen video (ctx 82={99,102,101}) + suppress popup |
| no | yes | yes | video + stock KDK ({99,102,101,20}) |

---

## Immediate corrections to what we built
1. ✅ DONE — `AltScreenModule.switchCluster` now BOUNCEs (72→sleep 180ms→target) + `setUpdateRate(1,10)`; restore does `setUpdateRate(1,0)`→switch(74). Bounce ctx = 72 (74 if target==72).
2. ✅ DONE — real driver lives in `switchCluster` (bounce+setUpdateRate); `ClusterService.activateCustomRendererPipeline` calls it; BAPBridge wraps with `forceClusterRouteInfoState(true)`@1770 + `forceGfxAvailable(true)`@1776 (stop mirror @1895/1896). setUpdateRate arg order confirmed vs `CombiMapController.updateFrameRate` = (kombiTerminal=1, fps): hint&2→10, ctx75→0.
3. ✅ DONE — `dc[82]` = `{99,102,101}` (video=displayables[0] capture root; 98 popup suppressed on altScreen).
4. ✅ RESOLVED (4th RE pass) — MenuState.smartPhone IS the MMI_Status SmartPhone bit (byte5 6-7); value AVAILABLE(2); licensed by coding 4237/4238; native composer emits at boot if coded. Tab is NOT the blocker — VC video-plane routing (LVDS_Available=1) is. Probe now also dumps isAppleDIO/isGoogleGAL + sysConst[4237/4238] so we confirm the license on-unit. See section A above.
5. ✅ VERIFIED — BAPBridge has `forceClusterRouteInfoState`@495 + `forceGfxAvailable`@2056, called in start (1770/1776) and stop (1895/1896) lifecycle.

## Open RE / verify (not from HU source alone)
- A5 availability channel: DSIKombiSync MenuState vs BAP `MMI_Status_Stat_Smartphone` (0x20002e6) → **4th RE pass on the BAP smartphone-status signal.**
- id-98 direct switchContext(80) (bypassing hardcoded-20) — unproven on unit.
- popup without gfxAvailable — untested (old patch set both).
- maneuver_render popup rect geometry.
