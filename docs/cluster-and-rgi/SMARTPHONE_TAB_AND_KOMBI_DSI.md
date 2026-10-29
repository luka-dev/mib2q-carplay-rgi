# Smartphone Tab and Kombi DSI

*The HU-side interfaces that expose Virtual Cockpit state (`DSICarKombi`, `DSIKombiSync2`), the HU display architecture that feeds the cluster, and the unbuilt alternative: putting CarPlay in the VC's own Smartphone tab instead of the nav map slot.*

---

## 📋 Where this fits

Production does not use the Smartphone tab. CarPlay reaches the cluster through our own DisplayManager contexts 80/81 on the nav map slot (see [Cluster Contexts](CLUSTER_CONTEXTS.md)). This note keeps the reference material and the gap analysis for the other route. Facts here are `[decompile]` of `MU1316-lsd.jar` and the VC firmware, plus a live `dmdt` dump `[car]`, unless tagged otherwise.

Firmware map: A5 HU = MU1316 (Java `Tools/jxe2jar/out/MU1316-combined-vf/`, native `Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/`), A5 VC = AU491, TT VC = AU334, R8 VC = AU724 (`Firmwares/VC/…/extracted/`), TT HU = MU1339 (`Tools/jxe2jar/out/MU1339-lsd-full-final-vf/`). The DSI reference comes from the decompiled `MU1316-lsd.jar` (`Refferences/MU1316-jxe-jar-decompile/decompile_src/`).

## 🔧 HU display architecture

The context list is declared by one component. `DisplayManagerMIB2High.defineContexts()` builds `dc[]`, and `DisplayManager.declareContexts(dc[])` sends it to the native DM (`de/audi/tghu/fwhmi/DisplayManager.java:130-131` in the stock decompile). A displayable is a QNX Screen window registered with the native DM by its owner process (`libdm_modMain.so`: `CGLRenderer::registerDisplayable(CDisplayable)`, source `CSurfaceSource(_screen_window)`, stored in an RB-tree `id → SharedPtr<CSurfaceSource>`).

| Displayable | Owner |
|---|---|
| 16 HMI (Software) | LSD/HMI via `libeal.so` |
| 17 REAR_VIEW_CAM (Overlay) | video overlay |
| 19 MAPVIEWER, 20 ROUTE_GUIDANCE, 33 KOMBI_MAP_VIEW (Software) | `app/navigation/libPresentationController.so` (`RdvMapViewer`, `MI_KOMBI_MAP`) |
| 101 / 102 (Image, KDK backing) | `DisplayManagerMIB2High.configureDM()` → `createImageDisplayable` from `987.png` |
| CarPlay / projection | `gal` + `libairplay.so` (they composite into the central HMI) |

**Rule: a context can declare displayable N, but N exists only if some owner creates its window.** Declared-but-ownerless is dead. On the A5, stock `dc[78] = {16, 59}` is dead because nothing creates 59. That is why our contexts reuse existing ids and add our own windows 98/99. `[car]`, `[code]` (`java_patch/de/audi/tghu/fwhmi/DisplayManagerMIB2High.java:166`)

**HU → VC video is MOST H.264, not a raw LVDS picture.** `[decompile]`

- HU: `videoencoderservice` = `CASIVideoEncodingSrv` (`setActiveDisplayable(displayID, displayable)`, `setUpdateRate`, output `_most.ts`, `CDisplayLink::changeConnection`).
- Chain: `switchContext` → `CContextManager::preContextSwitchHook` → `CASIMostEncoder::setActiveDisplayable(4, context.displayables[0])` → OMX H.264 → MPEG-TS → MOST → VC `libnvtvmr.so` (Tegra `TVMRVideoDecoderCreate/Render`, `H264_DEC_DECODE_ENTRY`).
- There is one MOST video channel. The VC composites the decoded video into whatever tab is active. VC names like `LVDS_Smartphone` / `LVDS_NavMap` are legacy layer names.

## 🔍 The VC Smartphone tab exists and waits for the HU

The A5, TT and R8 VC firmwares share the same context enum (103 `eHMIDisplayContextID*`, 0x0-0xef), including `eHMIApplicationContextIDSmartphone` / `eHMIDisplayContextIDSmartphone` = `0x75`. VC components: `fds` (EB GUIDE runtime; the view list is compiled in), `app_fs/dp/dp_monitor.cfg` (schema), `gss_ipc` (BAP → dp over SPI), `mostcoreapp` (MOST NetworkService, not video), `libnvtvmr.so` (H.264 decode), `kdgui_2dmgr_client` / `libeal` (Kanzi). `[decompile]`

| Item | ID |
|---|---|
| `state LVDS_Smartphone` / `SV_LVDS_Smartphone` | 0x89 / 0x8a |
| `view SV_LVDS_Smartphone` (+ `_NA_Init`, `_NA_Off` = no phone) | 0x54 |
| `state MainWizard` / `view SV_MainWizard` | 0x72 / 0x49 |
| `SV_MainWizard_Smartphone_labelText` | 0x3000025 |
| `INTERN_ContentActivate_Smartphone` (activation event) | 0x146 |
| `MMI_Status_Stat_Smartphone` (Integer, set by the HU over BAP) | 0x20002e6 |
| `INTERN_Popup_NoSmartphoneConnected` | 0x181 |

The tab is gated only by the runtime status `MMI_Status_Stat_Smartphone`. There is no availability coding item. `[decompile]` The VC FPK analysis lists `MMI_Current_LVDS_Skin` at the same id 0x20002e6; which name is right for AU491 is `[open]`.

`CODING_Timer_tSammelReiter_SmartPhoneActive` is a timer (Long, ms), not an enable flag: address `0x20000c8` on A5, `0x20000d4` on TT. It is one of three collective-tab hold timers (`tSammelReiter`, `tSammelReiter_MMI_Off`, `tSammelReiter_SmartPhoneActive`) and sets how long the tab bar stays expanded while a phone projects. Related: `CODING_Timer_tdLVDSLock`, `CODING_Tabbar_tabAnimationDuration`. `[decompile]`

## ❌ Why it does not work on the A5

The VC is ready. The gap is entirely on the MU1316 HU: `[decompile]`

1. **No projection displayable.** `gal` / `libairplay` composite CarPlay into HMI 16, so 59 is never created and there is nothing to send.
2. **No terminal-mode → cluster integration.** MU1316 terminal mode registers only `evohigh` screens and has no `mmikombi` directory. TT (MU1339) has `evohighmmikombi`; the diff is `terminalmode/hmi/Activator.java`: `evohigh`/`"EvoHigh"` vs `evohighmmikombi`/`"EvoHighMMIKombi"` via `System.getProperty(PARAMETER_VARIANT, …)`.
3. **No smartphone status to the VC.** Nothing sets `MMI_Status_Stat_Smartphone`; `CoMaCarplayHandler` sends nothing to the Kombi.
4. **No switch to context 0x75.** Nothing drives `IMMICombiScreenChangeManager` there. MU1316 has the `de/audi/atip/mmicombi/` framework (`IMMICombiScreen`, `IMMICombiScreenChangeManager`, `MMICombiDisplayExchangePacket`, `MMICombiDisplayStatus`) but it is not wired, and `IMMICombiScreenChangeManager` has no implementation in MU1316. Our `SteeringWheelInputModule` therefore treats it as optional. `[code]` (`java_patch/com/luka/carplay/core/SteeringWheelInputModule.java:76-91`, `:190-204`)

So anything sent with `setActiveDisplayable(4, X)` lands in the Navi tab.

The full chain it would take, all HU-side and reversible as LSD classpath-override jars (like `aa_split_center`: remove the jar to get stock back; VC coding is UDS-reversible; no flashing needed):

1. Create a separate projection displayable (a renderer opening a QNX Screen window with a new id, for example 59), modelled on how `libPresentationController` opens 33.
2. Switch the cluster to a context whose `displayables[0]` is that id.
3. Report smartphone active (`MMI_Status_Stat_Smartphone` over BAP).
4. Switch the VC to `Smartphone 0x75` via `IMMICombiScreenChangeManager` (the MU1339 mmikombi path).
5. Set `DSIKombiSync` `DISPLAYSTATUS_LVDS_DM_ACTIVE`.
6. VC: nothing to code; optionally tune the SammelReiter timer.

Open items `[open]`: whether `preContextSwitchHook` takes `displayables[0]` without validation; which BAP LSG/FctID sets `MMI_Status_Stat_Smartphone`; the exact `IMMICombiScreenChangeManager` call that activates context N; whether `gal`/`libairplay` ever open a separate window; any hidden smartphone bit inside a packed VC coding byte.

## 📊 `DSICarKombi`: preset and layout

`org.dsi.ifc.carkombi.DSICarKombi`, v2.11.30. `[decompile]`

| ATTR | ID | Type | Meaning |
|---|---:|---|---|
| `ATTR_DCACTIVEDISPLAYPRESET` | 99 | int | active preset type |
| `ATTR_DCDISPLAYVIEWCONFIGURATION` | 100 | `DCDisplayViewConfiguration` | layout mode |
| `ATTR_DCDISPLAY1/2/3MAINSELECTION` | 89 / 90 / 91 | `DCMainItems` | left / centre / right zone content |
| `ATTR_DCDISPLAY1/2/3SETUP` | 86 / 87 / 88 | - | visible subfields per zone |
| `ATTR_DCADDITIONALINSTRUMENTSETUP` | 93 | `DCAdditionalInstrument` | additional instrument 1 |
| `ATTR_DCADDITIONALINSTRUMENT2SETUP` | 95 | `DCAdditionalInstrument2` | additional instrument 2 |
| `ATTR_DCDISPLAYDEPENDENCYSETUP` | 98 | `DCDisplayDependency` | preset ↔ drive profile link |
| `ATTR_DCLEDCONFIGURATION` | 102 | bool | LED config |
| `ATTR_DCVIEWOPTIONS` | 82 | `DCViewOptions` | accessible VC controls |

- Preset: `UNKNOWN 0, INDIVIDUAL 1, CLASSIC 2, DRIVINGPROFILE 3`.
- View configuration: `UNKNOWN 0, NORMAL 1` (two dials + centre map), `LARGE 2` (big map, View button), `DISPLAY_VIEW1..10 = 3..12`.
- HUD presets: `NONE 0, ASSISTANCE 1, NAVIGATION 2, SPORTCHRONO 3, OFFROAD 4, PHEV 5, INDIVIDUAL 6`. `HUDCOLLECTIVETOPICS_VISUALISATION_VIRTUALCOCKPIT = 2`.
- Setters (RT ids): `setDCActiveDisplayPreset` 1038, `setDCDisplayViewConfiguration` 1039, `setDCDisplay1/2/3MainSelection` 1026/1027/1028, `setDCAdditionalInstrumentSetup` 1032, `setDCAdditionalInstrument2Setup` 1034, `setDCDisplayDependencySetup` 1035.
- Listener `DSICarKombiListener`: `updateDCActiveDisplayPreset`, `updateDCDisplayViewConfiguration`, `updateDCDisplay1/2/3MainSelection`, plus about 150 BC/SIA/HUD callbacks.
- `DCMainItems` has no smartphone or CarPlay value, so the Smartphone tab is not driven by the preset.

<details>
<summary>Zone content values (`DCELEMENTCONTENT_*`)</summary>

| Value | Name | Value | Name |
|---:|---|---:|---|
| 0 | BLANKLINE | 31 | AVERAGECONSUMPTION |
| 1 | BOOSTPRESSURE | 32 | DISTANCE |
| 2 | OILPRESSURE | 33 | DRIVINGTIME |
| 3 | OILTEMPERATURE | 34 | CURRENTCONSUMPTION |
| 4 | COOLANTTEMPERATURE | 35 | ZEROEMISSION |
| 5 | FUELRANGE | 36 | DRIVINGPROFILE |
| 6 | DESTINATIONARRIVALTIME | 37 | SECONDARYSPEED |
| 7 | INTERMEDIATEARRIVALTIME | 38 | DIGITALSPEED |
| 8 | DESTINATIONTRIPTIME | 39 | ENERGYFLOW |
| 9 | INTERMEDIATETRIPTIME | 40 | ACC |
| 10 | COMPASS | 41 | ROUTEGUIDANCE |
| 11 | GPSHEIGHT | 42 | TRAFFICSIGNDETECTION |
| 12 | TIME | 43 | SHIFTUPINDICATION |
| 13 | DATE | 44 | PERFORMANCE |
| 14 | HYBRIDBATTERY | 45 | PREDICTIVEEFFICIENCYASSISTANT |
| 15 | STATIONTRACK | 46 | WILDCARD |
| 16 | PHONEINFO | 47 | STEERINGANGLE |
| 17 | LATERALACCELERATION | 48 | SLOPE |
| 18 | ACCELERATION | 49 | CONSUMPTION_DATA |
| 19 | DECELERATION | 50 | COMBUSTOR_CONSUMPTION |
| 20 | ELECTRICRANGE | 51 | ELECTRICAL_CONSUMPTION |
| 21 | BATTERYSTATEOFCHARGE | 52 | AVERAGESPEED |
| 22 | CHARGINGTIMELEFT | 53 | POWERMETER |
| 23 | BATTERYTEMPERATURE | 54 | TACHOMETER |
| 24 | BATTERYLEVELRANGE | 55 | POWERMETER_AND_TACHOMETER |
| 25 | COOLANT | 56 | HYBRID |
| 26 | BOOSTLEVELVALUE | 57 | ENGINE_DATA |
| 27 | BATTERYCOOLANT | 58 | SHORTTERM_DATA |
| 28 | BATTERYBOOST | 59 | LONGTERM_DATA |
| 29 | BOOSTCOOLANT | 60 | G_METER |
| 30 | VEHICLEVOLTAGE | 61 | TYRE_PRESSURE_MONITOR |

</details>

---

## 📊 `DSIKombiSync2`: MMI ⇄ Kombi sync state

`org.dsi.ifc.kombisync2.DSIKombiSync`, v2.11.0. It carries real-time display sync (active tab, focus, popups, LVDS/HMI flags). `[decompile]`

Attributes: `ATTR_KOMBICOMMUNICATIONSTATE` 1 (bool + reason), `...DISPLAYIDENTIFICATION` 2, `...DISPLAYREQUESTRESPONSE` 3, `...DISPLAYSTATUS` 4, `...POPUPACTIONREQUEST` 5, `...POPUPREGISTERRESPONSE` 6, `...POPUPSTATUS` 7 (all `ATTR_KOMBIMESSAGESTATE*` except the first).

`DisplayStatus` (from `responseKombiDisplayStatus`) is the main object: `internalState`, `mainContext`, `screenFormat`, `focus`, `menuContext`, `style` (1..5), `statusFlags`.

| Field | Values |
|---|---|
| `mainContext` (`POPUPCONTEXT_*`) | `NO_CONTEXT 0, NAV 1, MEDIA 2, PHONE 3, NOT_USED 4, SETTINGS 5, APPS 6` (CarPlay/AA), `CONNECT 7, CAR 8` |
| `internalState`, Kombi | `INIT 0, STARTUP_BLACK 1, STARTUP_LOGO 2, IN_SYNC 3, SINGLE 4` (no MMI), `SHUTDOWN 5, OFF 6, SYNC_LOST 10, SWDL 11` |
| `internalState`, ZR | `INIT 0, STARTUP_BLACK 1, NORMAL_MODE 3, MMI_STANDALONE 4, SHUTDOWN 5, OFF 6, RECOVERY 10, SWDL 11, OVERRUN 12` |
| `screenFormat` | `INIT 0, SMALL 1, LARGE 2, SMALL_USER 3, LARGE_USER 4, SMALL_LOCK 5, LARGE_LOCK 6` |
| `focus` | `INIT 0, KOMBI 1` (wheel), `MMI 8` (rotary) |

`DisplayStatusFlags` bits:

| Bit | Mask | Flag | Meaning |
|---:|---|---|---|
| 15 | 0x8000 | ALL_INVALID | whole struct invalid |
| 14 | 0x4000 | USER_STAGE | |
| 13 | 0x2000 | LVDS_DM_ACTIVE | DM drives the video (`isLVDSDMActive()`) |
| 12 | 0x1000 | LVDS_HMI_ACTIVE | `isLVDSHMIActive()` / `isLVDSLock()` |
| 11 | 0x0800 | LVDS_PROTOCOL_DM_OK | |
| 10 | 0x0400 | LVDS_PROTOCOL_HMI_OK | |
| 9 | 0x0200 | SYNC_PROTOCOL_OK | |
| 8 | 0x0100 | SLOW_DOWN | Kombi requested slow-down |
| 7 | 0x0080 | LEFT_FLAP | left side menu open |
| 6 | 0x0040 | RIGHT_FLAP | right side menu open |
| 5 | 0x0020 | KDK_VISIBLE | KDK visible on VC |
| 4 | 0x0010 | NAV_BARGRAPH | bargraph visible |
| 3 | 0x0008 | 2ND_STATUSLINE | second status line |
| 2 | 0x0004 | REDUCED_SCREEN | reduced screen |
| 1 | 0x0002 | EARLY_RVC | early rear-view camera |

`MenuContext` has `sdsTabState`, `leftMenuState`, `rightMenuState`, `secondStatusLineState`; contexts `RIGHTSIDEMENU 1, LEFTSIDEMENU 2, SECONDSTATUSLINE 3, SDS_TAB 4`; states `INIT 0, CLOSED 1, OPENED 2`.

<details>
<summary>`PopupStatus`, setters and listener</summary>

`PopupStatus`: `popupID`, `popupID2`, `screenFormat`, `focus`, `menuContext`, `popupFlapLeft`, `popupFlapRight`, `popupStatusFlags`.

- Popup types: `INIT 0, FULL_SCREEN 1, PARTIAL_SCREEN 2, RVC 3, DRIVE_SELECT 4, ACTIVE_CALL 5, POWERBL 6, FLAP_LEFT 8, FLAP_RIGHT 9`.
- Popup states: `RESET 0, SET 1, QUIT 2, IDLE 3, RESETRETR 8, SETRETR 9, QUITRETR 10`.
- Left flap: `NO_FLAP 0, BC 1, CAR 2, OPS 3, D_LIST 8, PICTURE 9`. Right flap: `NO_FLAP 0, KDK 10`.
- Flags: `RVC_POSSIBLE 128, SDS_POSSIBLE 64, WITHOUT_FOLDER 4, BACKGROUND_QUITT_ALLOWED 2`.

Setters (MMI → Kombi): `setMMIDisplayRequestResponse`, `setMMIDisplayStatus`, `setMenuState`, `setMMIPopupRegisterRequest`, `setMMIPopupActionResponse`, `setMMIPopupStatus`, `setMMIDisplayIdentification`, `setHMIIsReady(boolean)`.

Listener `DSIKombiSyncListener`: `updateKombiCommunicationState(up, reason)`, `updateKombiMessageStateDisplayStatus`, `updateKombiMessageStatePopupStatus` (+5 other message-state callbacks), `responseKombiDisplayStatus`, `responseKombiDisplayIdentification`, `responseKombiDisplayRequestResponse`, `responseKombiPopupRegisterResponse`, `responseKombiPopupActionRequest`, `responseKombiPopupStatus`.

</details>

---

## 📊 Drive-select profile and quick answers

`org.dsi.ifc.cardrivingcharacteristics.DSICarDrivingCharacteristics` v2.11.21: `ATTR_CHARISMAACTIVEPROFILE = 13`, callback `updateCharismaActiveProfile(profile, i)`. Profiles: `NOPROFILE_INIT 255, COMFORT 1, AUTO_NORMAL 2, DYNAMIC 3` (Sport on non-RS), `OFFROAD_ALLROAD 4, EFFICIENCY 5, SPORT_RACE 6` (RS only), `INDIVIDUAL 7, RANGE 8, LIFT 9, OFFROADLEVEL2..4 = 10..12`. Related: `ATTR_CHARISMAACTIVEOPERATIONMODE = 28`, `ATTR_CHARISMAVIEWOPTIONS = 12`. `[decompile]`

| Question | Source |
|---|---|
| Sport mode active? | `ATTR_CHARISMAACTIVEPROFILE` = 3 or 6 |
| Which VC tab is showing? | `DisplayStatus.mainContext` |
| Classic dials or big map? | `DCDisplayViewConfiguration.activeDisplayView` 1 / 2 |
| Preset follows drive mode? | `ATTR_DCACTIVEDISPLAYPRESET` = 3 |
| Map visible on VC? | `DisplayStatusFlags.isKDKVisible()` (bit 5) |
| Video to VC active? | `isLVDSDMActive()` (bit 13) |
| Which popup is on VC? | `PopupStatus.popupID2` + `POPUPTYPE_*` |
| Focus on cluster or MMI? | `DisplayStatus.focus` 1 / 8 |
| VC standalone or sync lost? | `internalState` 4 / 10 |

None of these listeners is wired in `java_patch/` today. The reference once suggested bridging this state into PPS (`/pps/iap2/vc_state`); the project bus is TCP now, so any such bridge would go over [Hook to Java Bus](../reference/HOOK_TO_JAVA_BUS.md). `[code]`

## 🔗 Related

- [Cluster Contexts](CLUSTER_CONTEXTS.md): the production path on the nav map slot.
- [VC FPK State Machine](VC_FPK_STATE_MACHINE.md): the VC nav views.
- [Phone Tab Gating](archive/PHONE_TAB_GATING.md): `mainContext` PHONE 3 and the telephone tab.

## 📚 Sources

Merged originals were removed from the tree after this note absorbed them; they remain in git history (commit 23972aa).

- `SMARTPHONE_CLUSTER_KNOWLEDGE`: HU display architecture, live `dmdt` dump, MOST H.264 transport, VC Smartphone tab and gap analysis.
- `DSI_CARKOMBI_KOMBISYNC2_REFERENCE`: `DSICarKombi`, `DSIKombiSync2` and drive-select reference.
