# CarPlay cluster (RGI + altScreen) — how to do it right, end-to-end

**Date:** 2026-08-01
**Scope:** MHI2Q (MU1316), Audi Virtual Cockpit (LVDS2 / terminal 1). How the CarPlay
cluster display (RGI maneuver popup + altScreen video) should be architected so it is
stable, survives the steering-wheel **View** button, and never fights the stock cluster
view-mode state machine.
Companion doc (hook/altScreen protocol side): `hook/altscreen/IOS26_ALTSCREEN_REWORK_PLAN.md`.

---

## 0. Core principle

**Integrate with the stock cluster view-mode state machine; do NOT raw-override the
display context.** Every instability we hit (black cluster, View-button reverting to the
stock map, context flicker) came from `AltScreenModule.switchContext(80)` writing the
cluster context directly, in parallel with the stock system that the View button /
navigation also drives. Two writers of the cluster context = conflict.

---

## 1. The stock cluster view-mode chain (verified in the MU1316 decompile)

```
View button (steering wheel)
  → ClusterInputListener.evaluateFavoredViewMode / setFavoredViewMode(mode)
       modes: COMPASS=0, RGI=1, KDK=2, MAP=3          (ClusterViewMode.java)
  → ClusterViewMode.refreshDisplayContext()   (only for KDK/MAP; COMPASS/RGI are BAP/FPK, no ctx switch)
       KDK(2) → switchDisplayContextKombi(9)
       MAP(3) → switchDisplayContextKombi(8)
     (wrapped in KOMO doFadeOut → refresh → refreshRgMode → doFadeIn = smooth transition)
  → ClusterService.switchDisplayContextKombi(int)     ← WE ALREADY PATCH ClusterService
  → MapInterface.switchDisplayContextKombi(8/9)
  → abstractmap.getActiveContext().switchDisplayContextKombi() → DisplayManager.switchContext(stock)
```

`refreshDisplayContext` exists to keep the **logical** view mode (what the user wants) in
sync with the **physical** display context (what the VC composites). MAP/KDK are the
"heavy" MOST/LVDS modes that need a real context; COMPASS/RGI ride BAP/FPK.

**Why the View button always reverts to the stock map:** pressing View re-runs this chain →
`switchDisplayContextKombi(8)` → stock map context, overwriting our raw `switchContext(80)`.

---

## 2. Decision: CarPlay cluster = the MAP view mode, context = ctx 80 = {98,102,101,99}

**DECIDED:** when CarPlay is active, `ctx 80 = {98, 102, 101, 99}` **always** — the
**stock map (33) is never shown**. The cluster is fully ours during CarPlay.

```
ctx 80 = { 98 popup, 102/101 KDK bg, 99 CarPlay plane }
   98 = maneuver_render  (EGL/GPU) — RGI popup; transparent when not navigating; CMD_CLEAR on stop
   99 = altscreen_render (OMX)      — the CarPlay cluster plane (background)
```
- `dc[80] = {98,102,101,99}` in `DisplayManagerMIB2High.defineContexts()` (already set).
- **altscreen_render is always-on** (it owns window 99). If it dies, the background vanishes.
- No `33` anywhere in the CarPlay path.

### Plane 99 content evolves; the context does not
| phase | plane 99 (altscreen_render draws) | look |
|---|---|---|
| now (no altScreen video) | **OPEN DECISION A/B ↓** | popup over that background |
| endgame (Phase 2) | real CarPlay cluster video (from the hook URL-receiver) | popup over CarPlay video |

**OPEN DECISION (needs your pick) — interim plane-99 background while altScreen video isn't wired:**
- **A. black** — simplest; RGI = popup over black.
- **B. neutral placeholder** (dark bg / "CarPlay" / last-frame) — *recommended*: doesn't read as "broken".

---

## 3. Integration points (concrete)

### 3a. Route the MAP view mode to ctx 80 (the key change)
Patch `ClusterService.switchDisplayContextKombi(int i)` (already our patched class):
```java
public void switchDisplayContextKombi(int i) {
    // When CarPlay owns the cluster, the MAP view mode (8) must land on OUR ctx 80,
    // not the stock kombi map — else the View button reverts to the Audi map.
    if (i == 8 && com.luka.carplay.core.CarPlayApp.isActive()) {
        com.luka.carplay.core.AltScreenModule.enterClusterCtx();   // → DM.switchContext(80,1)
        return;
    }
    this.mapInterface.switchDisplayContextKombi(i);                 // stock KDK/gauges/etc.
}
```

### 3b. Enter/leave via the stock transition, not a raw switch
`AltScreenModule` stops doing `switchContext(80)` on connect. Instead:
- **Enter** (RGI start / navigating): `ClusterInputListener.setFavoredViewMode(MAP=3)` → the
  stock chain runs the fade + `switchDisplayContextKombi(8)` → [3a hook] → ctx 80. Free fade,
  and `refreshRgMode` sets RG-select for BAP at the same time.
- **Leave** (RGI stop / disconnect): restore the favored view mode → stock map/gauges return
  on their own (our 3a hook stops redirecting once `CarPlayApp.isActive()` is false).

### 3c. Renderers stay always-on + content-driven
Java never spawns/kills/mode-switches renderer content. maneuver_render draws transparent
when idle; altscreen_render draws the interim bg (A/B) then video. Windows 98/99 always exist,
so entering ctx 80 can never composite a missing plane (which was the black-cluster+hang bug).

---

## 4. RGI has two layers (graceful degradation)

- **BAP RGI (`appConnectorNavi`, CombiBAPServiceNavi)** — drives the NATIVE cluster RGI
  display (maneuver icon, distance, bargraph, lane, street). Works over BAP, independent of
  the display context. **This is the reliable base.**
- **maneuver_render (plane 98 on ctx 80)** — the custom GPU popup, the enhancement.

If the ctx-80 / GPU path ever misbehaves, BAP RGI still shows the maneuver. Don't couple them.

---

## 5. altScreen video = Phase 2, orthogonal (see the hook plan)

The hook must implement the iOS-26.1 **URL/scene** receiver (SetProperty `altScreenURLs`
store+ACK → open a URL → HTTP+H.264 → altscreen_render plane 99). That is the ONLY thing
that turns plane 99 from the interim bg into real CarPlay video. It does not touch any of
the view-mode integration above. Details + references: `hook/altscreen/IOS26_ALTSCREEN_REWORK_PLAN.md`.

---

## 6. Input routing

| input | goes to | note |
|---|---|---|
| **View** button (wheel) | stock view mode | do NOT intercept — it naturally toggles CarPlay-cluster (MAP) ↔ KDK ↔ gauges |
| **central MMI encoder** | CarPlay Main UI (next/prev) | `updateRotary → postRotaryEvent` (fixed 2026-08-01) |
| **steering-wheel rotary** | cluster altScreen HID (zoom/scroll) | only while on the CarPlay cluster (`AltScreenModule.isClusterActive`); the misplaced redirect on the central-encoder seam was the bug |

---

## 7. What this architecture fixes

| symptom | root cause | fix here |
|---|---|---|
| black cluster / 30s hang | raw switchContext(80) with a missing/`99`-empty plane | ctx entered via stock chain + always-on renderers own 98/99 |
| View → stock map | raw switch fights the view-mode chain | §3a hook: MAP(8) → ctx 80 while CarPlay active |
| context flicker / instability | two writers of the cluster context | single writer = the stock view-mode chain |

---

## 8. Touch list (files)
- `java_patch_new_replace/de/audi/tghu/navi/app/cluster/ClusterService.java` — add §3a redirect in `switchDisplayContextKombi`; RGI start/stop → `setFavoredViewMode`.
- `java_patch_new_replace/com/luka/carplay/core/AltScreenModule.java` — replace raw `switchContext(80)`-on-connect with `enterClusterCtx()` called only from §3a; keep the single-writer worker.
- `java_patch_new_replace/de/audi/tghu/fwhmi/DisplayManagerMIB2High.java` — `dc[80] = {98,102,101,99}` (done).
- `altscreen_render/` — interim plane-99 background (A/B) until Phase-2 video.
- Mirror into `java_patch_new_tab` after the replace variant is proven on-unit.

---

## 9. Open decisions
1. **Interim plane-99 background: A (black) or B (placeholder)?** (recommend B)
2. Enter trigger: on **RGI start** (navigating) vs on **CarPlay connect**? (recommend RGI-start — matches stock "cluster shows map when navigating"; revisit for Phase-2 video which may want connect-time).

---

## 10. Codex review #2 (2026-08-01) — corrections + REQUIRED on-unit probe

A skeptical second review against the decompile. Split into "our patch already handles"
vs "genuinely open". Codex reviewed the STOCK `Tools/jxe2jar/out/...` files, not our
`java_patch_new_replace` — some flags are already fixed in our tree.

### Already handled in `java_patch_new_replace/.../DisplayManagerMIB2High.java`
- **ctx-80 allocation** is kombiType-aware: `dc[158]` if `getKombiType()==4`, else `dc[81]`
  (so ctx 80 is in-bounds on non-type-4). Lines 36-40.
- **`dc[80]` set ONLY when `getKombiType()!=4`** (151-155); type-4 auto-generates 79..157 from
  base contexts — we deliberately don't use ctx 80 there. Documented "A5-class only".
- **`addKDKToContext` 80→73 fallthrough is guarded**: `if (visibleKDKs[j]==20 && i<80)` skips the
  KDK remap for ctx 80 (line 274, "audit #8"). Codex flagged this against the STOCK file; ours fixes it.

### GENUINELY OPEN (must resolve before implementing the view-mode integration)
- **R1 — kombiType.** Everything above assumes our unit is **`getKombiType() != 4`**. If it IS 4,
  ctx 80 = the stock G24-generated context (ours void). **VERIFY on-unit.**
- **R2 (the elephant) — `setFavoredViewMode(MAP)` may never reach `switchDisplayContextKombi(8)`
  on our config.** Guards that drop MAP: `validateViewMode` → KDK-only replaces all modes with
  KDK(2); RGI-only/no-map → MAP invalid → RGI(1). **FPK/MMI map → `refreshViewMode()` is skipped
  entirely** (`isClusterMapFPK()||isClusterMMI()`), so NO `…Kombi(8)` at all. **MOST map →
  `isMapReady()` requires `mapReady && gfxAvailable`** — and `gfxAvailable=false` is our known
  blocker. ⇒ §3b "enter via setFavoredViewMode" is fragile; likely needs forceGfxAvailable AND a
  non-FPK map, or we fall back to **direct `switchContext(80,1)`** (which our DM already composites
  correctly) + a narrow View-revert guard.
- **R3 — narrow the redirect.** `switchDisplayContextKombi(8)` has THREE callers: the desired
  `ClusterViewMode.refreshDisplayContext()` (logical MAP), plus `ClusterService.initializeModels()`
  (init) and `ClusterService.updateKombiMapReady(true)` (stock-map ready — fires 8 *before*
  updating mapReady). A global `i==8 && CarPlayActive` redirect hijacks ctx 80 at init/map-ready.
  Tie the redirect to the **logical-MAP call site** (hook `refreshDisplayContext`, or gate on the
  selected view mode), not the ClusterService boundary. Note KDK(9) from the view path would also
  clobber ctx 80 — guard both at the view-mode seam.
- **R4 — if using setFavoredViewMode:** use **flag=false** (don't persist to storage), explicitly
  **save/restore the prior favored mode**, make **`enterClusterCtx()` idempotent + nonblocking**
  (`refreshDisplayContext` calls it up to twice per transition), and beware `showKombiMap(true)`
  spinning up the stock Audi-map pipeline even if ctx 33 isn't shown.

### ⇒ REQUIRED gating probe — ✅ IMPLEMENTED (2026-08-01, no reflection)
`com/luka/carplay/probe/ClusterProbe.java` + gated hooks in the (replace-variant) patched
classes — direct calls, no reflection:
- **`ClusterViewMode.refreshViewMode`** (start): logs `kombiType, FPK, MMI, MOST, cur, fav, gfx,
  mapReady, rgiValid` — the R2 decision inputs, captured even when the FPK/MMI guard skips the body.
- **`ClusterViewMode.refreshDisplayContext`**: logs `viewMode` (whether it will emit Kombi(8/9)).
- **`ClusterService.switchDisplayContextKombi(i)`**: logs `i` + caller stack — R3, which caller fired.
- **`DisplayManagerMIB2High.switchContext`**: logs `req, final, terminal, visibleKDK, displayables[]`
  — the actual composited context (R3 final).
- **`DisplayManagerMIB2High.defineContexts`**: logs `kombiType` — R1.
- **One-shot** (`ClusterService` ctor → `ClusterProbe.armOneShot`): ~20 s after start, forces
  `clusterInputListener.setFavoredViewMode(3, false)` once so the above fire on a real MAP attempt.

**Currently ALWAYS-ON** (`ClusterProbe.on()` returns `true` — no marker needed; the one-shot forces
a MAP attempt ~20 s after every cluster start). Output → `/tmp/carplay_java.log` (grep `ClusterProbe`).
To disable later: flip `on()` back to a gate or delete the probe (all call sites are `if(on())`).
Builds clean (23 files, target 1.4); JCL gate passes for all firmware-touching files. Reading the log
answers R1+R2+R3 → decides direct-entry vs view-mode-entry and confirms whether the request stays
ctx 80 = {98,102,101,99}.
