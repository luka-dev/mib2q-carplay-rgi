---
name: mhi2_mu1439_java_port_lowrisk
description: Java side of the MU1439 (MHI2) port is low-risk — RGI+coverart stock HMI classes are identical MHI2Q↔MHI2
metadata: 
  node_type: memory
  type: project
  originSessionId: a880bce1-4faa-4ed2-b8d7-2c43da3c7ca4
---

Evidence the Java/HMI side of the MU1439 port (RGI + cover art) is LOW RISK (gathered while the LZOZ lsd.jxe extraction ran in background — see [[mhi2_mu1439_lzoz_format]]):

The patch's class-replacement targets all live under `java_stock/` for BOTH baselines, and `MU1326 = MHI2_ER_AU37x_P5089` is **MHI2 (non-Q), the closest relative of MU1439 (MHI2_US_AUG22 G22)**. Full diff of all 3 stock classes mu1316 (MHI2Q) vs mu1326 (MHI2): **IDENTICAL** —
- `de/audi/tghu/navi/app/cluster/ClusterService.java` (1054 lines) — RGI target
- `de/audi/app/combi/bap/app/audio/AppConnectorTerminalMode.java` (62 lines) — cover-art target
- `de/audi/app/terminalmode/combi/TerminalModeBapCombi.java` (57 lines) — both
⇒ the eso HMI Java framework for RGI + cover-art is shared across MHI2Q and MHI2; MU1439 ≈ MU1326, so these classes almost certainly match on MU1439 too.

**Cover-art port dependency (from patch `AppConnectorTerminalMode` + stock):** extends `AbstractAppConnectorAudio` implements `CombiBAPServiceTerminalMode`; hooks `combiModule.getPictureManager().registerPictureProvider(0, CoverArtProvider)`; overrides `updateCurrentStation` → `pushCoverArt` → builds `ResourceLocator(stationInfo.getPictureID(), url)` + `pictureProvider.addToMapping(getListRef(), resource)` + `responseCoverArt(listRef, SOURCE_TYPE_VC, resource, true)`. All stock BAP picture-manager APIs, shared across generations.

**Patch Java layout:** RGI = `com/luka/carplay/routeguidance/*` (BAPBridge, RouteGuidance, ManeuverMapper, SideStreets, GatedCombiService) + replacements of ClusterService & TerminalModeBapCombi$EventListener/$ActiveDeviceStateListener. Cover-art = `com/luka/carplay/coverart/CoverArt` + replacement of AppConnectorTerminalMode(+$CoverArtProvider). build_java.sh compiles against `../jxe2jar/out/lsd.jar`.

## UPDATE (2026-05-26): MU1439-lsd.jar now exists (`Tools/jxe2jar/out/MU1439-lsd.jar`, 123MB) — exact diffs done (same cfr decompiler for all 3 jars, so diffs are real). Earlier "identical" was the java_stock STUBS; the FULL classes do differ:
- **`AppConnectorTerminalMode` (cover-art): 0 diff MU1316↔MU1439 — byte-identical. Cover-art Java ports 1:1.** ✅
- **`ClusterService` (RGI, 18 lines):** MU1439 `Util.formatDistance` and `Util.getInvalidDistance` gained an extra String arg (new signatures) — the patched ClusterService.class must be REBUILT against MU1439's Util. Removed: `satMapProviderChanged` field + setProviderChangeFlag/hasSatMapProviderChanged; `setOnlineNavigationState(n==1,...)` simplified; one `updateDestinationInfo(null,0,0)` call dropped.
- **`AppConnectorNavi` (RGI BAP, 31 lines) — but patch does NOT replace this class (only calls it), so these are INTERNAL stock changes, compile-safe:** (a) sideStreets is `byte[]` in BOTH MU1316 & MU1439 (CombiBAPNaviManeuverDescriptor identical) — the convertToRawString→direct setContent change is internal stock serialization; patch's SideStreets.java byte[] output stays compatible (NO change needed). (b) FSG-sync gate in `updateManeuverDescriptor` changed: MU1316 `!isSyncActive() && !isSyncPending()` → MU1439 `!isSyncActive() || getCurrentSyncType()!=0` — affects the exitView-toggle sync(1) workaround [[mhi2 FSG sync]]. (c) new stub methods updateManeuverDescriptorAndExitView/responseLast+FavoriteDestinationsList (patch doesn't replace AppConnectorNavi, so compile-safe, but CombiBAPServiceNavi interface may have grown). updateRGStatus now always startSync(0).
- **`TerminalModeBapCombi` (8 lines):** only synthetic accessor swap access$700↔access$800 (lc↔context) — compiler-ordering; the $EventListener class-replacement must match MU1439's synthetic numbering.
Decompiled refs at /tmp/{MU1316k,MU1326k,mu1439k}/java. Net: cover-art trivial; RGI needs ClusterService rebuild vs new Util sigs + SideStreets type fix + FSG-sync recheck.

**(superseded) Conclusion:** for MU1439 the Java port needs essentially only a RECOMPILE against MU1439's lsd.jar (to match its exact class versions/constant pool), not redesign — provided MU1439's stock classes match MU1326's (verify once MU1439-lsd.jar is extracted). Real porting effort concentrates on the C-hook side: RGI [[mhi2_mu1439_rgi_path]] (mm-ipod iap2.cfg + link_handle_iap2pkt) and cover art [[mhi2_mu1439_coverart_path]] (iap2_nowplay_getartwork), which differ structurally (Apple iAP2 stack vs Cinemo).
