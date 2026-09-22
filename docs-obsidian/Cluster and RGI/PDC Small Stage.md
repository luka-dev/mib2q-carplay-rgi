# PDC Small Stage

*Why CarPlay used to vanish when the right-hand parking (PDC/OPS) popup appeared, the stock HMI chain that causes it, and the narrow Java guard that keeps CarPlay visible beside the popup. Not an audio topic — it lives here because it came out of the same investigation batch; it belongs with the HMI/Java notes.*

---

## 📋 Status

| Date | Build | Result |
|---|---|---|
| 2026-09-02 | second guard (`smallStageType 1 → 6` + AP 1002) | CarPlay stayed under the right OPS popup **[car]** |
| 2026-09-14 | same | re-broke: parking message **108** also comes from plain OPS |
| 2026-09-15 | `6d057db-pdc-pla1`, `915c479-pdc-events` | 108 handled, passive-PLA classification, AP ordering — host-tested |
| 2026-09-17 | `b0653c6` (includes `2e536a6-ops-aps-drawer-r2`) | "Check surroundings!" drawer gone; stock status line (clock/LTE, black bar over CarPlay's bottom buttons) remained **[car]** |
| 2026-09-17 | `b0653c6-ops-statusline-r3` | hides status-line popup 62 during pure OPS — in code, not yet car-verified **[open]** |

The installed-runtime build ID was not captured; installation and restart are known only from the owner's report. The alarm/teardown defect is separate and not fixed by any of this.

## 🔍 Root cause — a policy clash, not a drawing bug

```mermaid
flowchart TB
    accTitle: Stock chain that hides CarPlay on PDC
    accDescr: OPS activation shows partial popup 2100008 and sends parking message 108; native arbitration drops the view size to small; TerminalMode treats small stage as HMI deactivation and hands the screen to the head unit, while message 108 separately switches the Main Wizard and takes the screen resource.
    ops_active["OPS setActive true"]
    popup_2100008["partial popup 2100008, right 304 px"]
    msg_108["MsgDistributor 108 parking active"]
    view_small["native MMICombi: viewSize SMALL"]
    kombi_stage["updateKombiStage false"]
    hmi_deact["HMIDeactivated: SCREEN to MAINUNIT"]
    tracker_108["HighPriorityResourceTracker: Main Wizard, restrict SCREEN"]
    carplay_gone["CarPlay projection hidden"]
    ops_active --> popup_2100008 --> view_small --> kombi_stage --> hmi_deact --> carplay_gone
    ops_active --> msg_108 --> tracker_108 --> carplay_gone
    classDef primary fill:#dbeafe,stroke:#2563eb,stroke-width:2px,color:#1e3a5f
    classDef warning fill:#fef9c3,stroke:#ca8a04,stroke-width:2px,color:#713f12
    classDef danger fill:#fee2e2,stroke:#dc2626,stroke-width:2px,color:#7f1d1d
    class ops_active,popup_2100008,msg_108 primary
    class view_small,kombi_stage,tracker_108 warning
    class hmi_deact,carplay_gone danger
```

**The popup itself is harmless.** Pure OPS shows `ParkingPopupIdentifier(1, 2100008)` via `hmiService.showPartialPopup(0, 2100008)`: bounds `(720,0,304,480)`, HMI layer 3, slot 4, type 7 (unbound partial), ZPM priority 135, ZPM slot 1 `PARKING`. Volume (52, layer 1, ZPM 5) and massage (2100016/2100017, layer 3, ZPM 5) use the same `PartialPopupController` and never hide CarPlay. `setSmallStageType(3)` on the PDC popup is a no-op (the partial-popup setter is empty in this build).

**Path 1 — small stage.** Parking policy (below Java; the `IMMICombiViewSizeSync` implementation is not in the decompiled tree) sets `viewSize = 1` (SMALL). `MMICombiStageListener.viewSizeChanged(1)` → `updateKombiStage(false)` → `ExternalEventsListener.hmiDeactivated()` → `HMIDeactivated` sets `Resource.SCREEN` owner `MAINUNIT`. The CarPlay screen `TERMINALMODEIPODOUT` (ID `3200000`) is built with `setSmallStageType(1)` = `SMALL_STAGE_OMISSION`: in small stage it is dropped, not resized. Stock apps reflow for small stage; TerminalMode deliberately does not.

**Path 2 — message 108.** `AbstractParkingSystemOPSComponent.setActive(true)` → `notifyParkingSystemActive` → `updateInterappClients(true)` → `MsgDistributor.sendMessage(108)` (107/108 = parking system off/on, not camera) — delivered **synchronously** and *before* the popup request. Stock `HighPriorityResourceTracker.processMsg(108)` flips `SWITCH_TO_MAIN_WIZARD_CHOICE`, sets `SMARTPHONE_SCREEN_BLOCKED_CHOICE = 3`, `rvcActive = true` → `acquireHighPriorityResources(SCREEN)` → `TMState.restrictAccessTo(SCREEN)` for `MAINUNIT`, `MURVCActive = true`. The earlier "pure OPS = small stage only, VPS = 108" split was wrong.

**Red herrings.** Action-proxy methods 1004/1005 (`parkingActivatedTerminalMode`) have no listeners and are never called. Display context `78 = {16 HMI, 59 CarPlay}` already composites HMI over CarPlay, so adding a context does not help; displayable `17` (camera) matters only for the deferred partial-360 idea. Full-screen VPS/RVC is a genuine takeover (`activateComponentRVC(17)`) and must stay stock.

## ⚙️ The guard in code

All in `java_patch/` **[code]**:

- `com/luka/carplay/pdc/PdcSmallStageGuard.java` — state for popup `2100008` on terminal 0. Before `showPartialPopup` it switches the live CarPlay screen widget `3200000` from `SMALL_STAGE_OMISSION (1)` to `SMALL_STAGE_NO_CHANGE (6)` (`:34-46`, `:340-367`) and restores it on hidden/removed/unregister/deinit or when an unconfirmed show request expires after **3 s** (`SHOW_REQUEST_TIMEOUT_MS`, `:46`).
- **Classification (2026-09-15).** Allowed only for DSI popups `POPUP_OPS (1)`, `OPSAUTOACTIVATION (4)`, `OPSFLANKGUARD (7)`, `OPSOFFROAD (17)` with exactly one OPS (system ID 2) on partial `2100008`, plus at most one **passive** PLA whose `isSystemActive()` is false (`:114-138`). A plain side OPS legitimately arrives as `[OPS=2, PLA=16]` — stock `AbstractParkingSystemPLAComponent.getSupportedDSIPopupIDs()` includes 1/4/7/17 — so the old `systems.size() == 1` test broke it. Camera/VPS, ARA, `OPSFLANKGUARDPLA (15)`, trailer-assist mode 12, unknown or incomplete combos keep the stock takeover. An old test fixture used popup `3`, which is actually `VPSOPS`.
- `de/audi/app/earlyfunc/evo/parking/ParkingSystemControllerComponentEvo.java` — stock replacement overriding `deactivateCurrentlyVisibleParkingSystems(newContent, nextSystems)`, which the base `activateParkingSystem` calls **before** activating anything, so the guard is armed before 108; `notifyParkingSystemActive(nonOPS, true)` revokes it.
- `de/audi/app/terminalmode/interapp/HighPriorityResourceTracker.java` — full MU1316 replacement (Java 1.4 anonymous classes). For an allowed standalone OPS, 108 neither switches the Main Wizard nor takes SCREEN (`:182-197`, log `OPS 108: keep CarPlay SCREEN; no Main Wizard takeover`); a late 107 from an old component does not erase the new intent; eCall/clamp-S restrictions stay stock.
- `de/audi/app/terminalmode/ExternalEventsListener.java` — `updateKombiStage(false)` and AP 1002 (`AP_METHOD_HMI_DEACTIVATED`) skip `HMIDeactivated` only for CarPlay active + guarded popup + the *same* protected screen instance + no pending screen change; the CarPlay key handler is deactivated under the popup and `hmiActivated()` restores it at BIG stage. The old one-shot AP 1002 exemption was used up by the first OPS event; repeats are now allowed while the conditions hold. The 70 ms `DebounceOperation` could deliver a stale AP after OPS closed — a confirmed parking transition now survives that window and is revoked by camera/PLA/timeout/unregistration/teardown; `deinit` cancels the subscription.
- `de/audi/app/earlyfunc/core/parking/ParkingPartialPopupHandler.java` — arms the guard before the show (closes the race where SMALL arrives before `partialPopupVisible`); `partialPopupHidden` now cancels only a matching `visiblePPID` on the main terminal (any hidden, including the registration snapshot, used to cancel).
- **"Check surroundings!" drawer.** Labels 3261/3928 → `SystemScreenBag6.aPSAUDIO()` / `aPSAUDIOREDUCED()`, screens 182/190, drawer content **6001/6002** chosen by `AbstractParkingSystemControllerComponent.updateAudioDrawerContext` → `AudioDrawerContext.SOURCE_APS` → model **538**. `de/audi/audio/context/AudioDrawerContextImpl.java` + `com/luka/carplay/pdc/OpsAudioDrawerPolicy.java` publish `INACTIVE` for column 0 only while protected, remember the last stock value and republish it on release (front MU only; choice 3915, sounds and the other 22 columns untouched). The earlier fix aimed at eight PLA/ARA popup IDs (`21000xx`) was the wrong target and was deleted.
- **Status line 62 (r3).** `SystemScreenBag2` popup 62 (slot 3, layer 1, `StatusbarRendererHigh`, prefab `mib2_statusBar`) is separate from the APS drawer. `de/esolutions/hmi/widgets/audi/evo/high/PartialPopupManagerEvoHigh.java:199-215` hides 62 before showing `2100008` and returns HIDDEN for a repeat `showPopup(62)` or a queue replay through `doShowPopup`, only for the protected screen instance; stock TerminalMode's own normal state is `StatusBarStubController.renderStyle=2` (no bar).

**Do not** globally pin context 78 or opacity 100 (breaks HOME and full camera), ignore all 108s, or rely on `getCurrentDisplayContent()` alone (stock updates it after `activateParkingSystem`). RetroArch context `90 = {16, 43}` belongs to the RetroArch patch (removed from this JAR in `7590e88`); on G24 kombis ID 90 is a stock `11 + 79` context.

## ✅ What to check on the car

With `/mnt/app/carplay_verbose` created **before** Java starts (the Java `Log` reads the marker only at init): `ParkingPartialPopupHandler#showPopup popupID=2100008` → `OPS guard: screen 3200000 smallStageType 1 -> 6` → `…SMALL_STAGE pure OPS: keep CarPlay SCREEN owner` (and `partial OPS: suppress HMI_DEACTIVATED` if AP 1002 fires) → `parking intent popup=… systems=2,16(idle) standaloneOPS=true` → `OPS 108: keep CarPlay SCREEN…`; on close `BIG_STAGE` → `HMIActivated` → `OPS guard: restored … 1`. HOME, full camera and active PLA must still show `HMIDeactivated.execute`. Next trace for any regression: build ID, parking intent/108, SMALL/BIG, AP/HMI commands, current/connected/pending screen, terminal-0 context and opacity of 59.

Deferred (owner decision 2026-09-14): CarPlay beside a partial 360° camera (`{16, 17, 59}`, camera at `(720,0,304,480)`); no new context ID chosen, and `dc[91]` is taken on G24.

---

## 📚 Sources

- [[Sources/CARPLAY_PDC_SMALL_STAGE_ROOT_CAUSE|CARPLAY_PDC_SMALL_STAGE_ROOT_CAUSE]] — (Russian) the full investigation: stock chain, three implementations, 108, PLA classification, APS drawer, status line r3.

## 🔗 See also

[[Cluster Contexts]] · [[Reconnect and Lifecycle]] · [[Landmines]] · [[Home]]
