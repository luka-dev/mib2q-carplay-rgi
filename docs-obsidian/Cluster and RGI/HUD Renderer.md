# HUD Renderer

*What actually draws the turn: the windshield HUD firmware, our procedural 3D arrow in the cluster, the KOMO/LVDS video-map pipeline, and the VC FPK state machine that switches between them.*

---

## 📋 Three things called "the renderer"

The words matter — they are three different processes with three different jobs. **[car]**

| Name | What it is | How it is fed |
|---|---|---|
| **HUD** | The windshield head-up display; its own firmware draws the icon and bargraph | We send it **BAP** via the stock `AppConnectorNavi`; no pixels from us |
| **`maneuver_render`** | Our procedural 3D arrow on displayable 98 in the cluster (EGL) | Java commands over TCP `:19800`; it builds geometry/materials/animation itself |
| **`altscreen_render`** | The CarPlay video plane on the cluster (displayable 99) | The type-111 stream; see [[AltScreen Cluster Video]] |

`maneuver_render` does **not** push pixels to the HUD, and `altscreen_render` is neither a BAP sender nor a maneuver engine. The maneuver displayable is **98**, not the old stock 20. Its lifecycle belongs to startup/framework — Java never spawns a renderer per maneuver. **[car]**

## 🔧 The procedural arrow

The user's target is *close* HUD/renderer icon shape and action, not a pixel copy. The renderer keeps the exact valid turn angle (including sharp >100° and roundabout full rings), draws independent lane data as a bottom strip, and animates fill/blink toward a shared target. Blink phase has one Java source (600 ms, high/low) sent to both outputs; the renderer only smooths toward the received phase, it has no phase generator of its own. **[car]**

Sentinels that must survive "simplification": `+1000` = unknown angle (our C parser creates it for an indexed maneuver with no exit-angle TLV), `-1000` = a defensive case (do not read the sign as direction), `0` = real straight. `mTurnAnglePresent` distinguishes a real −1° from a legacy default. **[decompile]/[car]** Full mapping in `docs/RGI_HUD_RENDERER_KNOWLEDGE.md`.

## ⚙️ KOMO / LVDS map pipeline

The cluster map (not the arrow) is a **video** the HU renders and streams to the VC. Java assembles two `RouteInfoElement`s `[followInfoRIE, nextManeuverRIE]` and hands them to KOMO; native renders, captures, H.264-encodes, and sends over MOST; the VC decodes to its LVDS panel. **[decompile]**

```mermaid
flowchart TD
  accTitle: KOMO LVDS map pipeline
  accDescr: Java route info through the KOMO DSI services and native presentation controller, H.264 over MOST, to the VC LVDS map.
  ci["ClusterService.updateKOMOFollowInfo"]
  komo["KOMOService setRouteInfo"]
  pc["libPresentationController renders KDK map"]
  enc["videoencoderservice H.264 capture"]
  most["MOST isochronous channel"]
  vc["VC decode to LVDS panel"]
  ci --> komo --> pc --> enc --> most --> vc
  class ci,komo primary
  class pc,enc neutral
  class vc success
  classDef primary fill:#dbeafe,stroke:#2563eb,color:#1e3a5f
  classDef success fill:#dcfce7,stroke:#16a34a,color:#14532d
  classDef neutral fill:#f3f4f6,stroke:#6b7280,color:#1f2937
```

The control path that starts the encoder: `setKOMODataRate(2)` → hint 2 on `ChoiceModel(1,168)` → `CombiMapController` → for FPK (`sysConst 541 == 2`) `handleKDK()` → `switchContext` + `DisplayManager.setUpdateRate(1, 10)` → encoder runs at 10 fps. Three DSI services carry it: `dsi.komoview` (route info), `dsi.komonavinfo` (text), `dsi.komogfxstreamsink` (fade + `updateGfxState`/`updateDataRate` callbacks). **[decompile]**

<details>
<summary>KOMO start/stop sequences</summary>

- **Start:** `enableKomoView(true)` + `notifyVisibility(true)` + `setKDKVisibility(true)` + `updateRgActive(true)` + `setKOMODataRate(2)`.
- **Stop:** `setKOMODataRate(0)` → `setKDKVisibility(false)` → `updateRgActive(false)` → `updateNextManeuver(empty)` → `enableKomoView(false)`.

`updateGfxState(1,1)` → `setGFXAvailable(true)`; `updateDataRate(2,1)` confirms full bandwidth. **[decompile]**

</details>

---

## ⚙️ VC FPK state machine

The VC is a compiled EB GUIDE model (in the `fds` binary); BAP arrives over MOST (LSG 50) via `gssipc-kbd`, which writes dp items. The cluster nav sub-tree has two display targets that matter to CarPlay. **[decompile]**

```mermaid
stateDiagram-v2
  accTitle: VC FPK nav display selection
  accDescr: How INTERN_Active_NavFPK_Content, LVDS_Available, and InfoStates select the LVDS map, the AIO-arrow view, or the compass.
  [*] --> compass
  compass: SV_NavFPK_Compass (native)
  lvds_map: SV_LVDS_NavMap_FPK (MOST video)
  aio: SV_NavFPK_Compass_MobileDevice (AIO arrows)
  compass --> lvds_map: content Map and LVDS_Available 1
  compass --> aio: InfoStates 6
  lvds_map --> compass: content None
  aio --> compass: InfoStates not 6
  class lvds_map success
  class aio primary
  class compass neutral
  classDef primary fill:#dbeafe,stroke:#2563eb,color:#1e3a5f
  classDef success fill:#dcfce7,stroke:#16a34a,color:#14532d
  classDef neutral fill:#f3f4f6,stroke:#6b7280,color:#1f2937
```

**Production uses the LVDS map path**, driven purely by `INTERN_Active_NavFPK_Content = Map(1)` (set when `gfxAvailable = true`) + `LVDS_Available = 1` (set when video is physically flowing). This needs **no** `InfoStates = 6` and **no** KSS patch. The VC has no dp item for rgType at all; it switches on content/InfoStates, not rgType. **[car]/[decompile]**

<details>
<summary>InfoStates=6 rejection and the KSS patch (AIO-arrow variant only)</summary>

The alternative AIO-arrow view (`SV_NavFPK_Compass_MobileDevice`) needs `BAP_NavSD_InfoStates_States = 6`. But `KSSApplication.bin` `sub_108F42C` validates InfoStates from MOST 0x515 with `a1 <= 0xF && (a1 & 5) != 4 && (a1 & 0xA) != 8` — which **rejects 6** because `(6 & 5) == 4`, so the dp item is never set and the arrows never render. **[decompile]**

Fix (only if using the AIO-arrow path): patch `sub_108F42C` at `0x108F42C` (file offset `0x8F42C`), replace `0F 28 07 D8` with `01 20 70 47` (`MOVS R0,#1; BX LR` = always true). UDS: `3D 14 01 08 F4 2C 04 01 20 70 47`. Script `archive/infostates6_patch.py`. The production LVDS-map path does not need this. **[decompile]**

</details>

---

## ⚙️ LVDS video link

`LVDS` is the physical wire from the VC's decoder to its LCD panel; the MOST H.264 stream is the transport that feeds it. The VC flips to `SV_LVDS_NavMap_FPK` when `INTERN_Active_NavFPK_Content = 1` and `LVDS_Available = 1` — the `INTERN_Active_NavFPK_Content` bit is set automatically by the VC's own EB GUIDE state machine once `gfxAvailable = true` triggers the MAP view. **[decompile]**

Renderer-side, `setActiveDisplayable` fires **only on a context change** — force `switchContext(0)→72→setActiveDisplayable(4,33)`. `gfxAvailable = false` blocks it, cleared via `BAPBridge.forceGfxAvailable(true)`. Ctx 72's first displayable is 33 (map), ctx 74's first is 20. Related ARM32 patches keep the DSIs alive, force `StartDrawing`, and cap 10 fps (`libPresentationController`). **[car]**

## 🔗 Related

- [[RGI and BAP Bridge]] — the BAP + KOMO outputs that feed this.
- [[Cluster Contexts]] — where displayable 98/99 land on the panel.
- [[Third-party Nav Apps]] — why a starved `ManeuverMapper` gets no maneuverType.

Provenance: `docs/RGI_HUD_RENDERER_KNOWLEDGE.md`, `MEMORY.md` (VC FPK state machine, LVDS Video Pipeline), memory `komo_lvds_architecture.md`, `vc_fpk_state_machine.md`, `kss_infostates6_rejection.md`, `bapbridge_architecture.md`; source `maneuver_render/`, `KOMOService.java`, `libPresentationController.so`.
