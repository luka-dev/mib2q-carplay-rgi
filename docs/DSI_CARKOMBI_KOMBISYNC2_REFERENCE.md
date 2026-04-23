# DSI Reference: `DSICarKombi` + `DSIKombiSync2`

Source: decompiled `MU1316-lsd.jar` (see `Refferences/MU1316-jxe-jar-decompile/decompile_src/`).

Two independent DSI interfaces expose Virtual Cockpit (Kombi) state to Java clients running inside `lsd.jxe`:

- `org.dsi.ifc.carkombi.DSICarKombi` — v2.11.30 — per-display user preset, layout, content selection, SIA/BC/HUD settings.
- `org.dsi.ifc.kombisync2.DSIKombiSync` — v2.11.0 — real-time MMI⇄Kombi display-sync state (active tab, focus, popups, LVDS/HMI flags).

They answer different questions. Use them together for full cluster state.

Related (referenced only): `org.dsi.ifc.cardrivingcharacteristics.DSICarDrivingCharacteristics` for drive-select profile (Charisma) — see end of doc.

---

## 1. `DSICarKombi` — preset + layout of the Virtual Cockpit

Full name: `org.dsi.ifc.carkombi.DSICarKombi` (version `2.11.30`).

### 1.1 Key attributes for "what's shown on VC"

| ATTR | ID | Type | Meaning |
|---|---|---|---|
| `ATTR_DCACTIVEDISPLAYPRESET` | 99 | int | Which preset type is active |
| `ATTR_DCDISPLAYVIEWCONFIGURATION` | 100 | `DCDisplayViewConfiguration` | Layout mode (normal / large / view1..10) |
| `ATTR_DCDISPLAY1MAINSELECTION` | 89 | `DCMainItems` | Content of the LEFT display zone |
| `ATTR_DCDISPLAY2MAINSELECTION` | 90 | `DCMainItems` | Content of the CENTER display zone |
| `ATTR_DCDISPLAY3MAINSELECTION` | 91 | `DCMainItems` | Content of the RIGHT display zone |
| `ATTR_DCDISPLAY1SETUP` / `2SETUP` / `3SETUP` | 86 / 87 / 88 | — | Setup (which subfields visible) per zone |
| `ATTR_DCADDITIONALINSTRUMENTSETUP` | 93 | `DCAdditionalInstrument` | Additional instrument 1 setup |
| `ATTR_DCADDITIONALINSTRUMENT2SETUP` | 95 | `DCAdditionalInstrument2` | Additional instrument 2 setup |
| `ATTR_DCDISPLAYDEPENDENCYSETUP` | 98 | `DCDisplayDependency` | Link preset ↔ drive profile |
| `ATTR_DCLEDCONFIGURATION` | 102 | bool | LED config |
| `ATTR_DCVIEWOPTIONS` | 82 | `DCViewOptions` | Which VC controls are accessible |

### 1.2 Preset types (`ATTR_DCACTIVEDISPLAYPRESET`)

```
DCDISPLAYPRESETSTYPE_UNKNOWN        = 0
DCDISPLAYPRESETSTYPE_INDIVIDUAL     = 1   // custom user-defined
DCDISPLAYPRESETSTYPE_CLASSIC        = 2   // traditional dials view
DCDISPLAYPRESETSTYPE_DRIVINGPROFILE = 3   // follows Charisma drive profile
```

### 1.3 View configuration (`DCDisplayViewConfiguration.activeDisplayView`)

```
DCDISPLAYVIEWCONFIGURATION_UNKNOWN       = 0
DCDISPLAYVIEWCONFIGURATION_NORMAL        = 1   // two dials + center map
DCDISPLAYVIEWCONFIGURATION_LARGE         = 2   // big map, tiny dials (VIEW button)
DCDISPLAYVIEWCONFIGURATION_DISPLAY_VIEW1 = 3
...
DCDISPLAYVIEWCONFIGURATION_DISPLAY_VIEW10= 12
```

### 1.4 Display zone content (`DCELEMENTCONTENT_*`)

Values that can be placed in each zone (used inside `DCMainItems` and `DCAdditionalInstrument*`):

```
 0 BLANKLINE
 1 BOOSTPRESSURE              31 AVERAGECONSUMPTION
 2 OILPRESSURE                32 DISTANCE
 3 OILTEMPERATURE             33 DRIVINGTIME
 4 COOLANTTEMPERATURE         34 CURRENTCONSUMPTION
 5 FUELRANGE                  35 ZEROEMISSION
 6 DESTINATIONARRIVALTIME     36 DRIVINGPROFILE
 7 INTERMEDIATEARRIVALTIME    37 SECONDARYSPEED
 8 DESTINATIONTRIPTIME        38 DIGITALSPEED
 9 INTERMEDIATETRIPTIME       39 ENERGYFLOW
10 COMPASS                    40 ACC
11 GPSHEIGHT                  41 ROUTEGUIDANCE
12 TIME                       42 TRAFFICSIGNDETECTION
13 DATE                       43 SHIFTUPINDICATION
14 HYBRIDBATTERY              44 PERFORMANCE
15 STATIONTRACK               45 PREDICTIVEEFFICIENCYASSISTANT
16 PHONEINFO                  46 WILDCARD
17 LATERALACCELERATION        47 STEERINGANGLE
18 ACCELERATION               48 SLOPE
19 DECELERATION               49 CONSUMPTION_DATA
20 ELECTRICRANGE              50 COMBUSTOR_CONSUMPTION
21 BATTERYSTATEOFCHARGE       51 ELECTRICAL_CONSUMPTION
22 CHARGINGTIMELEFT           52 AVERAGESPEED
23 BATTERYTEMPERATURE         53 POWERMETER
24 BATTERYLEVELRANGE          54 TACHOMETER
25 COOLANT                    55 POWERMETER_AND_TACHOMETER
26 BOOSTLEVELVALUE            56 HYBRID
27 BATTERYCOOLANT             57 ENGINE_DATA
28 BATTERYBOOST               58 SHORTTERM_DATA
29 BOOSTCOOLANT               59 LONGTERM_DATA
30 VEHICLEVOLTAGE             60 G_METER
                              61 TYRE_PRESSURE_MONITOR
```

### 1.5 HUD presets (`ATTR_HUDPRESETS`)

```
HUDPRESETS_NONE        = 0
HUDPRESETS_ASSISTANCE  = 1
HUDPRESETS_NAVIGATION  = 2
HUDPRESETS_SPORTCHRONO = 3
HUDPRESETS_OFFROAD     = 4
HUDPRESETS_PHEV        = 5
HUDPRESETS_INDIVIDUAL  = 6
```

`HUDCOLLECTIVETOPICS_VISUALISATION_VIRTUALCOCKPIT = 2` explicitly indicates VC-only visualisation path.

### 1.6 Setter / RT methods (Java → DSI → Kombi)

```java
void setDCActiveDisplayPreset(int i);                         // RT=1038
void setDCDisplayViewConfiguration(DCDisplayViewConfiguration);// RT=1039
void setDCDisplay1MainSelection(DCMainItems);                 // RT=1026
void setDCDisplay2MainSelection(DCMainItems);                 // RT=1027
void setDCDisplay3MainSelection(DCMainItems);                 // RT=1028
void setDCAdditionalInstrumentSetup(DCAdditionalInstrument);  // RT=1032
void setDCAdditionalInstrument2Setup(DCAdditionalInstrument2);// RT=1034
void setDCDisplayDependencySetup(DCDisplayDependency);        // RT=1035
```

### 1.7 Listener callbacks (Kombi → Java)

Implement `DSICarKombiListener`:

```java
void updateDCActiveDisplayPreset(int preset, int i);
void updateDCDisplayViewConfiguration(DCDisplayViewConfiguration cfg, int i);
void updateDCDisplay1MainSelection(DCMainItems items, int i);
void updateDCDisplay2MainSelection(DCMainItems items, int i);
void updateDCDisplay3MainSelection(DCMainItems items, int i);
// ... plus ~150 others for BC/SIA/HUD
```

---

## 2. `DSIKombiSync2` — MMI ⇄ Kombi sync state

Full name: `org.dsi.ifc.kombisync2.DSIKombiSync` (package has `2` suffix; version `2.11.0`).

### 2.1 Attributes

| ATTR | ID | Type | Meaning |
|---|---|---|---|
| `ATTR_KOMBICOMMUNICATIONSTATE` | 1 | bool+int | Kombi comms up/down + reason |
| `ATTR_KOMBIMESSAGESTATEDISPLAYIDENTIFICATION` | 2 | int | Ack state of last DisplayIdent |
| `ATTR_KOMBIMESSAGESTATEDISPLAYREQUESTRESPONSE` | 3 | int | Req/resp msg state |
| `ATTR_KOMBIMESSAGESTATEDISPLAYSTATUS` | 4 | int | Ack state of DisplayStatus |
| `ATTR_KOMBIMESSAGESTATEPOPUPACTIONREQUEST` | 5 | int | PopupAction msg state |
| `ATTR_KOMBIMESSAGESTATEPOPUPREGISTERRESPONSE` | 6 | int | PopupRegister response state |
| `ATTR_KOMBIMESSAGESTATEPOPUPSTATUS` | 7 | int | PopupStatus msg state |

**Key structures** (received via `responseKombi*`):

- `responseKombiDisplayStatus(DisplayStatus)` ← main cluster display state
- `responseKombiPopupStatus(PopupStatus)` ← active popup state

### 2.2 `DisplayStatus` — THE main cluster state object

```java
public class DisplayStatus {
    int internalState;              // ZR/KBIINTERNALSTATE_*
    int mainContext;                // POPUPCONTEXT_* — which MMI tab is mirrored
    int screenFormat;               // SCREENFORMAT_*
    int focus;                      // FOCUS_KOMBI=1, FOCUS_MMI=8
    MenuContext menuContext;        // side/status line menu states
    int style;                      // STYLE_1..5 visual style
    DisplayStatusFlags statusFlags; // bitfield — see 2.5
}
```

### 2.3 Main context values (`mainContext`)

These are reused `POPUPCONTEXT_*` constants:

```
POPUPCONTEXT_NO_CONTEXT = 0
POPUPCONTEXT_NAV        = 1   // Navigation tab
POPUPCONTEXT_MEDIA      = 2   // Media tab
POPUPCONTEXT_PHONE      = 3   // Phone tab
POPUPCONTEXT_NOT_USED   = 4
POPUPCONTEXT_SETTINGS   = 5
POPUPCONTEXT_APPS       = 6   // Apps (CarPlay/AndroidAuto lives here)
POPUPCONTEXT_CONNECT    = 7
POPUPCONTEXT_CAR        = 8   // Car / Vehicle tab
```

### 2.4 Internal state (`internalState` — different enum for Kombi vs ZR)

**Kombi (KBI):**
```
KBIINTERNALSTATE_INIT           = 0
KBIINTERNALSTATE_STARTUP_BLACK  = 1
KBIINTERNALSTATE_STARTUP_LOGO   = 2
KBIINTERNALSTATE_IN_SYNC        = 3   // fully in sync with MMI
KBIINTERNALSTATE_SINGLE         = 4   // standalone (no MMI)
KBIINTERNALSTATE_SHUTDOWN       = 5
KBIINTERNALSTATE_OFF            = 6
KBIINTERNALSTATE_SYNC_LOST      = 10
KBIINTERNALSTATE_SWDL           = 11
```

**ZR (central module) — for popup-side state:**
```
ZRINTERNALSTATE_INIT            = 0
ZRINTERNALSTATE_STARTUP_BLACK   = 1
ZRINTERNALSTATE_NORMAL_MODE     = 3
ZRINTERNALSTATE_MMI_STANDALONE  = 4
ZRINTERNALSTATE_SHUTDOWN        = 5
ZRINTERNALSTATE_OFF             = 6
ZRINTERNALSTATE_RECOVERY        = 10
ZRINTERNALSTATE_SWDL            = 11
ZRINTERNALSTATE_OVERRUN         = 12
```

### 2.5 `DisplayStatusFlags` bitfield

Convenience accessors on the class; raw bits:

| Bit | Mask | Flag | Meaning |
|---|---|---|---|
| 15 | `0x8000` | ALL_INVALID | Whole struct invalid |
| 14 | `0x4000` | USER_STAGE | |
| 13 | `0x2000` | LVDS_DM_ACTIVE | `isLVDSDMActive()` — DM is driving LVDS video |
| 12 | `0x1000` | LVDS_HMI_ACTIVE | `isLVDSHMIActive()` / `isLVDSLock()` |
| 11 | `0x0800` | LVDS_PROTOCOL_DM_OK | |
| 10 | `0x0400` | LVDS_PROTOCOL_HMI_OK | |
|  9 | `0x0200` | SYNC_PROTOCOL_OK | |
|  8 | `0x0100` | SLOW_DOWN | Kombi requested slow-down |
|  7 | `0x0080` | LEFT_FLAP | Left side-menu flap open |
|  6 | `0x0040` | RIGHT_FLAP | Right side-menu flap open |
|  5 | `0x0020` | KDK_VISIBLE | **Kombi Display Karte (map on VC) visible** |
|  4 | `0x0010` | NAV_BARGRAPH | Nav bargraph visible |
|  3 | `0x0008` | 2ND_STATUSLINE | Second status line visible |
|  2 | `0x0004` | REDUCED_SCREEN | Reduced screen mode |
|  1 | `0x0002` | EARLY_RVC | Early rear-view camera |

### 2.6 Screen format / focus / style

```
SCREENFORMAT_INIT           = 0
SCREENFORMAT_SMALL          = 1   // small map zone (standard dials view)
SCREENFORMAT_LARGE          = 2   // large map zone (infotainment view)
SCREENFORMAT_SMALL_USER     = 3   // user-locked small
SCREENFORMAT_LARGE_USER     = 4   // user-locked large
SCREENFORMAT_SMALL_LOCK     = 5
SCREENFORMAT_LARGE_LOCK     = 6

FOCUS_INIT  = 0
FOCUS_KOMBI = 1   // input focus on cluster (steering-wheel wheel active)
FOCUS_MMI   = 8   // input focus on main MMI (rotary active)

STYLE_INIT = 0
STYLE_1..5 = 1..5
```

### 2.7 `MenuContext` — side menu states

```java
public class MenuContext {
    int sdsTabState;           // SDS tab (speech dialog)
    int leftMenuState;         // left side drawer
    int rightMenuState;        // right side drawer
    int secondStatusLineState; // 2nd status line
}
```

Menu-context constants:
```
MENUCONTEXT_RIGHTSIDEMENU     = 1
MENUCONTEXT_LEFTSIDEMENU      = 2
MENUCONTEXT_SECONDSTATUSLINE  = 3
MENUCONTEXT_SDS_TAB           = 4

MENUCONTEXTSTATE_INIT    = 0
MENUCONTEXTSTATE_CLOSED  = 1
MENUCONTEXTSTATE_OPENED  = 2
```

### 2.8 `PopupStatus` — active popup on the cluster

```java
public class PopupStatus {
    int popupID;
    int popupID2;
    int screenFormat;
    int focus;
    MenuContext menuContext;
    int popupFlapLeft;       // POPUPFLAPLEFT_*
    int popupFlapRight;      // POPUPFLAPRIGHT_*
    PopupStatusFlags popupStatusFlags;
}
```

Popup types:
```
POPUPTYPE_INIT            = 0
POPUPTYPE_FULL_SCREEN     = 1
POPUPTYPE_PARTIAL_SCREEN  = 2
POPUPTYPE_RVC             = 3   // rear-view camera
POPUPTYPE_DRIVE_SELECT    = 4   // drive-select popup (when Charisma profile changes)
POPUPTYPE_ACTIVE_CALL     = 5
POPUPTYPE_POWERBL         = 6
POPUPTYPE_FLAP_LEFT       = 8
POPUPTYPE_FLAP_RIGHT      = 9
```

Popup state machine:
```
POPUPSTATE_RESET      = 0
POPUPSTATE_SET        = 1
POPUPSTATE_QUIT       = 2
POPUPSTATE_IDLE       = 3
POPUPSTATE_RESETRETR  = 8   // retry variants
POPUPSTATE_SETRETR    = 9
POPUPSTATE_QUITRETR   = 10
```

Popup flaps (left = info/car data, right = KDK):
```
POPUPFLAPLEFT_NO_FLAP  = 0
POPUPFLAPLEFT_BC       = 1   // on-board computer
POPUPFLAPLEFT_CAR      = 2
POPUPFLAPLEFT_OPS      = 3   // parking sensors
POPUPFLAPLEFT_D_LIST   = 8
POPUPFLAPLEFT_PICTURE  = 9

POPUPFLAPRIGHT_NO_FLAP = 0
POPUPFLAPRIGHT_KDK     = 10  // map
```

`PopupStatusFlags` additions:
```
POPUPSTATUS_RVC_POSSIBLE              = 128
POPUPSTATUS_SDS_POSSIBLE              = 64
POPUPSTATUS_WITHOUT_FOLDER            = 4
POPUPSTATUS_BACKGROUND_QUITT_ALLOWED  = 2
```

### 2.9 Setter methods (MMI → Kombi)

```java
// DSIKombiSync = MMI-side proxy (we SET, kombi RESPONDS)
void setMMIDisplayRequestResponse(DisplayRequestResponse);
void setMMIDisplayStatus(DisplayStatus);
void setMenuState(MenuState);
void setMMIPopupRegisterRequest(PopupRegisterRequestResponse);
void setMMIPopupActionResponse(PopupActionRequestResponse);
void setMMIPopupStatus(PopupStatus);
void setMMIDisplayIdentification(DisplayIdentification);
void setHMIIsReady(boolean);
```

### 2.10 Listener callbacks (`DSIKombiSyncListener`)

```java
void updateKombiCommunicationState(boolean up, int reason);
void updateKombiMessageStateDisplayStatus(int state, int handle);
void updateKombiMessageStatePopupStatus(int state, int handle);
// + 5 other msg-state callbacks

void responseKombiDisplayStatus(DisplayStatus status);           // ← main listener
void responseKombiDisplayIdentification(DisplayIdentification);
void responseKombiDisplayRequestResponse(DisplayRequestResponse);
void responseKombiPopupRegisterResponse(PopupRegisterRequestResponse);
void responseKombiPopupActionRequest(PopupActionRequestResponse);
void responseKombiPopupStatus(PopupStatus status);               // ← popup listener
```

---

## 3. Cross-reference: drive-select profile (Charisma)

Separate DSI interface but usually consumed together with Kombi state.

- Interface: `org.dsi.ifc.cardrivingcharacteristics.DSICarDrivingCharacteristics` (v2.11.21).
- Attribute: `ATTR_CHARISMAACTIVEPROFILE = 13` (int).
- Callback: `updateCharismaActiveProfile(int profile, int i)`.

Values:
```
CHARISMAPROFILES_NOPROFILE_INIT   = 255
CHARISMAPROFILES_COMFORT          = 1
CHARISMAPROFILES_AUTO_NORMAL      = 2
CHARISMAPROFILES_DYNAMIC          = 3    // "Sport" on non-RS
CHARISMAPROFILES_OFFROAD_ALLROAD  = 4
CHARISMAPROFILES_EFFICIENCY       = 5
CHARISMAPROFILES_SPORT_RACE       = 6    // Race / RS-only
CHARISMAPROFILES_INDIVIDUAL       = 7
CHARISMAPROFILES_RANGE            = 8
CHARISMAPROFILES_LIFT             = 9
CHARISMAPROFILES_OFFROADLEVEL2..4 = 10..12
```

Related attrs:
- `ATTR_CHARISMAACTIVEOPERATIONMODE = 28` → `CharismaOperationMode` (EV/Hybrid/Sustaining/Charging/SoC/HybridSport).
- `ATTR_CHARISMAVIEWOPTIONS = 12` → which profiles are enabled in the list.

---

## 4. Practical combinations

What you can learn by subscribing to all three:

| Question | Source |
|---|---|
| Is sport mode active? | `DSICarDrivingCharacteristics.ATTR_CHARISMAACTIVEPROFILE` → 3 (Dynamic) or 6 (Race) |
| What tab is the driver looking at on VC? | `DSIKombiSync2.DisplayStatus.mainContext` (Nav/Media/Phone/Car/...) |
| Is VC in "classic" dials mode or "big map" mode? | `DSICarKombi.DCDisplayViewConfiguration.activeDisplayView` → 1=NORMAL / 2=LARGE |
| Is VC preset following drive-mode? | `DSICarKombi.ATTR_DCACTIVEDISPLAYPRESET` → 3=DRIVINGPROFILE |
| What's shown in each VC zone? | `DSICarKombi.ATTR_DCDISPLAY{1,2,3}MAINSELECTION` + `DCELEMENTCONTENT_*` |
| Is the map currently rendered on VC? | `DisplayStatusFlags.isKDKVisible()` (bit 5) |
| Is LVDS video stream to VC active? | `DisplayStatusFlags.isLVDSDMActive()` (bit 13) |
| Which popup is currently on VC (RVC / drive-select popup / call)? | `DSIKombiSync2.PopupStatus.popupID2` + `POPUPTYPE_*` |
| Is focus on cluster or MMI? | `DisplayStatus.focus` → 1=KOMBI / 8=MMI |
| VC in standalone mode (MMI off)? | `DisplayStatus.internalState == KBIINTERNALSTATE_SINGLE` (4) |
| VC sync lost? | `DisplayStatus.internalState == KBIINTERNALSTATE_SYNC_LOST` (10) |

## 5. Usage pattern in a Java patch

Same DSI-client acquisition pattern used by `BAPBridge.java` in this project. Subscribe in listener:

```java
DSICarKombiListener kombiListener = new DSICarKombiListener() {
    public void updateDCActiveDisplayPreset(int preset, int i) {
        // preset ∈ {UNKNOWN=0, INDIVIDUAL=1, CLASSIC=2, DRIVINGPROFILE=3}
        publishState("vc_preset", preset);
    }
    public void updateDCDisplayViewConfiguration(DCDisplayViewConfiguration cfg, int i) {
        // cfg.activeDisplayView ∈ {NORMAL=1, LARGE=2, VIEW1..10=3..12}
        publishState("vc_view", cfg.activeDisplayView);
    }
    // ... other callbacks stubbed
};

DSIKombiSyncListener syncListener = new DSIKombiSyncListener() {
    public void responseKombiDisplayStatus(DisplayStatus s) {
        publishState("vc_tab", s.mainContext);        // 1=Nav, 2=Media, ...
        publishState("vc_focus", s.focus);            // 1=Kombi, 8=MMI
        publishState("vc_format", s.screenFormat);
        publishState("kdk_visible", s.statusFlags.isKDKVisible());
        publishState("lvds_active", s.statusFlags.isLVDSDMActive());
    }
    public void responseKombiPopupStatus(PopupStatus p) {
        publishState("vc_popup", p.popupID);
    }
    // ... stub rest
};
```

Bridge VC state into PPS (`/pps/iap2/vc_state`) so native hooks can react (e.g. change CarPlay cursor behaviour / hide overlay when VC is in a specific popup / adapt rendering to LARGE vs NORMAL view config).
