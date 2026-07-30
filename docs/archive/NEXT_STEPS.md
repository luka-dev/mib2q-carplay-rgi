# MHI2Q CarPlay — Next Steps / What We're Missing

_Snapshot 2026-07-27. Companion to `java_patch_new/ARCHITECTURE.md` and the RE writeups._

Two parallel tracks feed the cluster (LVDS2 → MOST → VC): the **Java patch** (BAP route
guidance + altScreen tab + context switching) and the **C renderers** (managed QNX Screen
windows the MOST encoder captures). The C **hook** bridges libairplay ⇄ Java over TCP :19810.

---

## 1. Where we are (DONE)

### Java (`java_patch_new/`, compiles: 15 files, target 1.2)
- **Foundation**: `bus.CarplayBus` (bidirectional, real writer + `Data`/`parseText`), `core`
  (`CarPlayApp` + `CarPlayApp.framework()` static accessor + `FrameworkRef` + `Module`).
- **Ported**: `coverart.CoverArt`; full `rgd` (ManeuverMapper, SideStreets, RendererMapper,
  GatedCombiService, RendererServer, RouteGuidance, BAPBridge) — foundation-swapped, compiles.
- **Patched stock**: `DisplayManagerMIB2High` (array `[79]→[83]`, 3 contiguous custom contexts
  dc[80/81/82], gated kombiType!=4); `ClusterService` (4 CarPlay accessors grafted; 2
  renderer-pipeline methods STUBBED).

### C renderers
- **`common/cluster_surface.{c,h}`** — shared managed-window primitive: raw `screen_create_window`
  + `screen_manage_window("How are you gentlemen?")` + buffers + MANAGER_STRING watchdog.
- **maneuver_render** (id **98**, RGBA/GL) — dmdt cut + libdisplayinit dropped, on cluster_surface.
  968→391 lines. `make hostcheck` PASS.
- **altscreen_render** (id **99**, NV12/OMX) — physical-bind + group-host dropped, on cluster_surface.
  `make hostcheck` PASS.

### RE facts locked (see `SMARTPHONE_CLUSTER_KNOWLEDGE.md`, ida sessions)
- MOST encoder capture = QNX Screen **stream consumer** keyed by `SCREEN_PROPERTY_ID_STRING`;
  the DM composites the **whole active context**, encoder grabs the composite.
- Managed-window handshake = `screen_manage_window(win,"How are you gentlemen?")`; DM stamps
  `MANAGER_STRING="All your base are belong to us!"`. `display_init` does NO GPU magic.
- Displayable id space: enum 15–60 (60=HUD_MAP_VIEW) + KDK 100/101/102 → **61–99 free**.

---

## 2. Critical path (ordered)

1. **Java: altScreen driver** — ✅ DONE. `AltScreenModule` + ClusterService stubs filled
   (switchCluster(80)/restoreCluster → HMI DisplayManager.switchContext).
2. **Java: wire rgd as a `Module`** — ✅ DONE. `RgdModule` in MODULES[]: retry-until-ready on
   CombiBAPServiceNavi → RouteGuidance.init(navi) → EVT_RGD_UPDATE. (CoverArt→BAP wiring still TODO.)
3. **Java: entry seam** — ✅ DONE. Patched `TerminalModeBapCombi` (VF stock): ACTIVATING+carplay →
   `CarPlayApp.onActivate(this$0.context)`, NOT_ATTACHED+carplay → `CarPlayApp.onDeactivate()`. Compiles.
4. **Java: altScreen tab availability/active** — `MenuState.smartPhone=AVAILABLE(2)` push +
   `DSIKombiSync mainContext==117` observer (design in ARCHITECTURE.md §altScreen, not implemented).
5. **Java: input** — cluster roller `VirtualButtonListener` → `CMD_ALT_HID`; cursor for main.
   NEEDS RE: VirtualButtonModel registration for the cluster roller + TMVirtualButtonListener ctor deps.
6. **On-unit validation of the raw+manage path** (see Risks §4) — canary via one renderer.

---

## 3. What we're MISSING / not yet accounted for

- **Renderer launch on-unit** — nothing found that spawns `maneuver_render` / `altscreen_render`.
  Decide: Java spawns them? the hook? an ifs entry? (grep found no launcher.)
- **ClusterService FUNCTIONAL patches** (compile-independent, needed on-unit): `setKOMODataRate`
  FPK guard, FctID18 `!showBargraph` fix (:305), `setValidRGTypeReceived`, `getKOMOHintsRaw`.
  Only the 6 accessor/stub methods are in; the behavioural patches from the old patch are NOT.
- **libairplay altScreen advert** — the 111-stream only exists if libairplay advertises
  `extendedFeatures "altScreen"` + cluster URLs (from-source `libairplay-mhi2q`,
  `altScreen/impl_plan_fromsource.md`). If not live, `altscreen_render` has nothing to decode.
- **altscreen_render geometry** — cluster panel = **1440×542** (`cluster_panel_resolution` memory).
  Confirm `ALTSCREEN_W/H` + the `/info` advert match, else OMX decode geometry is wrong.
- **maneuver_render geometry** — old overlay was 328×181 (id 20). Confirm what size id-98 should be
  now that we own the composite root (full cluster vs small card).
- **CoverArt → BAP actual push** — `CoverArt` fires a callback; the callback→BAP wiring
  (`AppConnectorTerminalMode$CoverArtProvider`) must be ported/registered.
- **VC coding** — smartphone-tab availability (coding 4237/4238), any InfoStates gating.
- **Build/deploy packaging** — how `carplay_hook_new.jar` + the 3 patched stock classes + the C
  binaries land on the unit (classpath-override jar, ifs). Not scripted here.

---

## 4. Risks / unvalidated assumptions

- **BIGGEST: raw-screen + manage → VC is UNVALIDATED on-unit.** RE says it must work (IPTE captures
  any managed window by ID_STRING; `display_init` had no magic), and the old libdisplayinit path
  proved the *chain*, but our hand-rolled `screen_manage_window` + EGL/OMX window has never run on
  hardware. **Both** renderers now share this. Prove with ONE canary before trusting both.
- **DM disown lifecycle** — with our own ids (98/99, no stock collision) the recreate watchdog
  should fire rarely, but the DM still owns our window and can disown on context-away. Watchdog kept.
- **Context indices 80–82 on A5** — array bumped to [83]; A5 is kombiType!=4 so within range. If a
  unit is kombiType==4 (158-context variant) our block is gated OFF (correct, but altScreen then N/A).
- **hostcheck ≠ on-unit** — hostchecks verify syntax/types vs stub EGL/GLES/screen/OMX only.

---

## 5. Open RE questions (nice-to-close)

- Confirm the encoder's IPTE screen **context/group identity** — that a `screen_manage_window`
  window surfaces as a CREATE event in *videoencoderservice*'s IPTE context (empirically implied by
  the working maneuver path; not traced end-to-end).
- `dc[82]={98,102,101,99}` z-order — confirm `displayables[0]` composites on **top** (so maneuver
  arrows draw over CarPlay video, not under). Assumed from stock dc[74] ordering (20 first = overlay).
- Whether the DM validates displayable ids against the enum (would reject 98/99). Assumed no
  (101/102 are non-enum-ish KDK and work; unknown ids show as "--" in dmdt and render fine).

---

## 6. Reference — ids, contexts, strings

| thing | value |
|---|---|
| maneuver displayable | **98** (`CR_DISPLAYABLE_ID`), RGBA8888 (fmt 8), USAGE OPENGL_ES2 (0x20), transparent |
| altscreen displayable | **99** (`SS_DISPLAYABLE_ID`), tiled-NV12, USAGE QcomVideo, opaque |
| manage group (request) | `"How are you gentlemen?"` |
| MANAGER_STRING (DM stamp) | `"All your base are belong to us!"` |
| cluster contexts | `dc[80]={98,102,101}` `dc[81]={98,102,101,33}` `dc[82]={98,102,101,99}` |
| KDK bg | 102 = LARGE_STAGE, 101 = SMALL_STAGE |
| stock cluster map | 33 (KOMBI_MAP_VIEW) |
| cluster display | LVDS2, display id 1, 1440×542 |
| bus | TCP 127.0.0.1:**19810** (hook=client, Java=server); EVT_RGD_UPDATE 0x0020, EVT_COVERART 0x0010, CMD_ALT_HID 0x0110 |
