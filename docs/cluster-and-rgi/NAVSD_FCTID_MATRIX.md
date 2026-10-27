# NavSD FctID Matrix

*Every function of the Navigation BAP LSG (`0x32`, NavSD) in the stock MU1316 stack: which exist, which the cluster is told about, which have a usable Java setter, and who owns each one while CarPlay drives the cluster.*

---

## 📋 Three different facts per FctID

1. **Registered:** `FunctionRegistrationNavi` creates a property, array or method object (33 properties, 6 arrays, 5 methods, plus framework functions).
2. **Advertised:** the coded `FunctionListNaviEvo` variant exposes it to the cluster. Registration alone does not make it available.
3. **Writable:** stock `CombiBAPServiceNavi` / `AppConnectorNavi` offer a Java call. Some registered functions are framework command or list machinery with no publisher.

Sources of truth: `FunctionIDsNavi.java` (names, FctID 1-56), `FunctionRegistrationNavi.java`, `FunctionListNaviEvo.java`, `CombiBAPServiceNavi.java`, `AppConnectorNavi.java`; our policy is in `GatedCombiService.java` and `BAPBridge.java`. `[decompile]`, `[code]`

Variant legend: `S` Std, `H` HighPrem, `M` MMIKombi, `MH` MMIKombiHUD; `All` = all four. China also disables FctID 47. The MHI2Q virtual cockpit matches `H` most closely, but the live FunctionList (FctID 3) is the final authority.

## 📊 The catalogue

| ID | Hex | Stock name | Kind | Advertised | HU seam | Role | CarPlay policy |
|---:|---|---|---|---|---|---|---|
| 1 | 0x01 | GetAll | protocol | All | framework | full status snapshot | stock |
| 2 | 0x02 | BAP_Config | property | All | framework | BAP version / config | stock |
| 3 | 0x03 | FunctionList | property | All | framework | availability bitmap | stock; capability truth |
| 4 | 0x04 | HeartBeat | protocol | All | framework | LSG liveness | stock |
| 5-14 | - | reserved | - | none | none | - | do not use |
| 15 | 0x0F | FSG_OperationState | property | All | `show/hideInitializingScreen()` | READY / INITIALIZING | stock, not gated: INITIALIZING (3) puts the VC NA "Initialising navigation system" screen over the CarPlay map until stock Navigation is ready; every workaround was removed 2026-10-19 ([Nav Initializing Screen](NAV_INITIALIZING_SCREEN.md)) |
| 16 | 0x10 | CompassInfo | property | All | `updateCompassInfo(angle, symbolic)` | heading, N/NE/… | stock |
| 17 | 0x11 | RG_Status | property | All | `updateRGStatus(status)` | RG active; opens FctSync | CarPlay-owned for the session |
| 18 | 0x12 | DistanceToNextManeuver | property | All | `updateDistanceToNextManeuver(dist, unit, barOn, bar)` | distance + bargraph | CarPlay-owned |
| 19 | 0x13 | CurrentPositionInfo | property | All | `updateCurrentPositionInfo(text)` | VC lower-bar text | CarPlay during RGI; stock for `CP && !RGI` |
| 20 | 0x14 | TurnToInfo | property | All | `updateTurnToInfo(turnTo, signPost)` | turn-to / signpost overlay | both strings empty during CarPlay RGI; stock writes gated |
| 21 | 0x15 | DistanceToDestination | property | All | `updateDistanceToDestination(value, unit, stopover)` | trip distance | CarPlay during RGI; cached for instant refresh |
| 22 | 0x16 | TimeToDestination | property | All | `updateTimeToDestination(type, format, time)` | ETA / remaining | CarPlay during RGI; always `timeInfoType = 1` |
| 23 | 0x17 | ManeuverDescriptor | property | S/H/MH | `updateManeuverDescriptor(slots[])` | up to 3 descriptors | CarPlay-owned; current maneuver at any distance |
| 24 | 0x18 | LaneGuidance | array | S/H/MH | `updateLaneGuidance(show, lanes[])` | lane arrows | CarPlay-owned |
| 25 | 0x19 | TMCinfo | property | S/H | `updateTMCInfoMessages(msgs[])` | urgent traffic | stock |
| 26 | 0x1A | MagnetFieldZone | property/SetGet | none | none | compass calibration region | not advertised |
| 27 | 0x1B | Calibration | property/SetGet | none | none | compass calibration | not advertised |
| 28 | 0x1C | ASG_Capabilities | property/SetGet | none | none | cluster/nav capabilities | framework |
| 29 | 0x1D | LastDest_List | array | S/H | `updateLastDestinationsList(entries[])` | last destinations | stock |
| 30 | 0x1E | FavoriteDest_List | array | S/H | `updateFavoriteDestinationsList(entries[])` | favourites | stock |
| 31 | 0x1F | PreferredDest_List | property | none | none | preferred list source | not advertised |
| 32 | 0x20 | NavBook | array | none | none | address book | not advertised |
| 33 | 0x21 | Address_List | array | S/H | `updateHomeAddress(dest)` | home address list | stock |
| 34 | 0x22 | RG_ActDeact | method | S/H | `routeGuidanceActDeactResult(result)` | answer to cluster start/stop guidance | stock command path |
| 35 | 0x23 | RepeatLastNavAnnouncement | method | none | `repeatLastNavAnnouncementResult` | repeat announcement | not advertised |
| 36 | 0x24 | VoiceGuidance | property/SetGet | S/H | `updateVoiceGuidanceState(state)` | voice mode | stock |
| 37 | 0x25 | FunctionSynchronisation | property | All | framework | atomic RG sync | implicit via 17/23/18/49; never write |
| 38 | 0x26 | InfoStates | property | All | `updateInfoStates(state)` | no error / no GPS / initializing / mobile device | stock, passed through. 6 (navigation in mobile device, sent while a terminal-mode nav app runs) replaces the map with the VC's mobile-device compass screen; 5 is a diagnostic splash ([Nav Initializing Screen](NAV_INITIALIZING_SCREEN.md)) |
| 39 | 0x27 | ActiveRGType | property/SetGet | S/H | `updateActiveRGType(type)` | guidance presentation source | CarPlay-owned; sends `0` (BAP RGI) |
| 40 | 0x28 | TrafficBlock_Indication | property | S/H/MH | `updateTrafficBlockIndication(symbol)` | TMC icon | stock |
| 41 | 0x29 | GetNextListPos | method | S/H | none | list paging | framework |
| 42 | 0x2A | NB_Speller | method | none | none | nav book speller | not advertised |
| 43 | 0x2B | MapColorAndType | property/SetGet | H | `updateMapColor/Type`, `updateSupportedMapTypes` | stock map colour / type | stock; does not touch the CarPlay map |
| 44 | 0x2C | MapViewAndOrientation | property/SetGet | H | `updateMapView`, `updateSupportedMapViews`, `updateMapVisibility`, `updateMapOrientation` | stock map view / visibility | stock; its incoming visibility drives KDK opacity |
| 45 | 0x2D | MapScale | property/SetGet | H | `updateMapScale(auto, enabled, value, unit, intersection)` | scale / auto-zoom; wheel SetGet | cached, hidden while CarPlay owns the cluster, restored after |
| 46 | 0x2E | DestinationInfo | property | All | `updateDestinationInfo(info)` | destination details | CarPlay during RGI |
| 47 | 0x2F | Altitude | property | H/M/MH (not China) | `updateAltitude(value, unit)` | lower-bar altitude | cached, suppressed, restored after |
| 48 | 0x30 | OnlineNavigationState | property | H/M/MH | `updateOnlineNavigationState(...)` | online map status | stock |
| 49 | 0x31 | Exitview | property | S/H/MH | `updateExitView(variant, id)` | junction view; FctSync member | CarPlay-owned; variant toggled every send |
| 50 | 0x32 | SemidynamicRouteGuidance | property | All | `updateSemidynamicRouteGuidance(info)` | traffic delay, alternative route and its ETA | stock; not the live-route ETA |
| 51 | 0x33 | POI_Search | method | none | `poiSearchResult(result, count)` | cluster POI search | not advertised |
| 52 | 0x34 | POI_List | array | none | `updatePOIListSize()` is a no-op stub | POI results | unusable |
| 53 | 0x35 | FSG_Setup | property | S/H | `updateFSGSetup(voiceMask, poiSupport)` | voice / POI capabilities | stock |
| 54 | 0x36 | Map_Presentation | property/SetGet | H | `updateMapPresentation(large, left, right)` | map size and side-menu state | stock; its incoming `largeMapView` selects the KDK stage |
| 55 | 0x37 | ManeuverState | property | S/H | `updateManeuverState(state)` | maneuver phase | CarPlay-owned |
| 56 | 0x38 | ETC_Status | property | M/MH | `updateEtcStatus(status)` | toll-card status | not in HighPrem; stock |

## ⚙️ Ownership during CarPlay

`[code]` (`java_patch/com/luka/carplay/rgd/GatedCombiService.java:1-10`, `:28-99`, `:118-205`; `java_patch/com/luka/carplay/core/AltScreenNavStatusGate.java:152`)

| Class | FctIDs | Rule |
|---|---|---|
| Session altScreen availability | 15 | While plane 99 owns the cluster, stock `showInitializingScreen` is deferred and `hideInitializingScreen` reasserted; on release the latest request is replayed and `ClusterService` recomputes. This covers `appIsReady` only and did not stop the NA screen (owner, 2026-09-23); an INITIALIZING→NORMAL clamp for the same lifetime was tried and removed 2026-09-24 ([Nav Initializing Screen](NAV_INITIALIZING_SCREEN.md)) |
| CarPlay exclusive during takeover | 17, 18, 23, 24, 39, 49, 55 | `GatedCombiService` drops stock writes; `BAPBridge` publishes |
| CarPlay exclusive only during active RGI | 19, 20, 21, 22, 46 | stock route text and trip output allowed again for `CP && !RGI` |
| Suppressed native-map decorations | 45, 47 | stock value cached; hidden status sent (`updateMapScale(15, false, 0xFFFF, 0xFF, false)`, `updateAltitude(0x7FFF, 2)`); restored on release |
| Left stock-owned | 16, 25, 29, 30, 33, 36, 38, 40, 43, 44, 48, 50, 53, 54, 56 | avoid disturbing native navigation and menus |
| Framework / command plumbing | 1-4, 26-28, 31, 32, 34, 35, 37, 41, 42, 51, 52 | not UI setters |

The FctID 45 SetGet from the cluster carries the steering-wheel roller steps. While CarPlay owns the cluster, `AltScreenCombiBAPListener.setMapScale` turns them into `CMD_ALT_ZOOM` for the phone and ACKs with the unchanged native scale; `setMapScaleSetting` (auto-zoom) is swallowed. `[code]` (`java_patch/de/audi/tghu/navi/app/cluster/AltScreenCombiBAPListener.java:62-79`)

The native [input scheduler](../altscreen-and-video/ALTSCREEN_INPUT_COMMANDS.md)
paces and bounds the semantic zoom after this BAP reply; command scheduling does
not replace the required stock status transaction.

Practical reading: FctID 22 variants and richer 21/46 data are safe; 25/40 only with real CarPlay traffic data. **Potentially destructive: 15, 38, 43, 44, 45, 54**, because they drive the stock navigation state or map, not the CarPlay scene. Not useful for visuals: method results 34, 35, 41, 42, 51 and sync 37.

## 📊 Selectors inside a FctID

| FctID | Selector | Values / effect | CarPlay use |
|---:|---|---|---|
| 16 | `direction_Symbolic`, `direction_Angle` | 16 sectors or unsupported, plus angle | stock; `255` does not prove the VC hides the compass |
| 17 | `rg_Status` | 0 inactive, 1 active, 2 suspended, 255 unsupported | lifecycle, not a display toggle |
| 18 | `bargraphOnOff`, distance validity | independent on the wire | the AU491 VC hides distance while `BargraphOn != 0`; see [RGI and BAP Bridge](RGI_AND_BAP_BRIDGE.md) |
| 21 | `distanceToStopover` | false = final destination, true = next stopover | safe with a real waypoint distinction |
| 22 | `timeInfoType` | 0 remaining, 1 arrival, 2 arrival in another zone, 15 invalid | keep 1; the VC has no remaining-time widget ([VC FPK State Machine](VC_FPK_STATE_MACHINE.md)) |
| 22 | `navigationTimeFormat` | 0 24 h, 1 12 h | follows HU locale |
| 23 | `mainElement`, `direction`, `zLevelGuidance`, slot | glyph, signed direction, level, current/next | only from the real iOS maneuver |
| 24 | `laneGuidanceOnOff`, per-lane `guidanceInfo` | 0 not recommended, 1 recommended, 2 best | from iOS lane guidance; type, markings and description alter glyphs |
| 25 | `messageStatus`, `messageWaitingIndication` | popup request / confirmation, waiting count | leave stock |
| 38 | `states` | 0 no error, 1 no data medium, 2 database corrupted, 3 no GPS, 4 database update, 5 initializing MOST map, 6 navigation in mobile device | global lifecycle; not the source of the "Initialising navigation system" text (that is FctID 15 OP 3); 6 selects the mobile-device screen, 1/2/4/5 a hint popup ([Nav Initializing Screen](NAV_INITIALIZING_SCREEN.md)); never use to hide a widget |
| 39 | `rgtype` | 0 BAP ManeuverDescriptor RGI, 1 MOST KDK, 2 compass, 3 MOST map, 4 LVDS map, 5 LVDS KDK, 255 default | selects the whole guidance source; we send 0, the planes need no LVDS type |
| 40 | `tmc_Symbol` | none or a traffic glyph | only with a real event |
| 43 | `colour`, `activeMapType`, `mainMapSetup` | day / night / auto; destination / 2D / 3D / overview / range | stock map only |
| 44 | `mapView`, `supplementaryMapView`, `mapOrientation`, visibility bits | standard / online / traffic; compass / intersection / box; north / heading | stock map and lifecycle |
| 45 | `autoZoom`, `scale`, `unit` | auto-zoom and scale; SetGet carries wheel steps | status hidden during CarPlay; SetGet is our wheel input |
| 49 | `variant`, `exitview_Id` | EU / NAR / ROW / Asia resource | only with a matching resource; we toggle EU/NAR with id 0 |
| 50 | alternative-route flags, distance type, nested `timeInfoType` | alternative route card | not the current-route ETA |
| 54 | `largeMapView`, `leftSideMenueOpen`, `rightSideMenueOpen` | map size and drawer state | the VC reports its own state; not a command ([VC FPK State Machine](VC_FPK_STATE_MACHINE.md)) |
| 55 | `state` | 0 unknown, 1 follow, 2 prepare, 3 distance, 4 call-for-action | only in sync with the real maneuver |

FctID 19 has no selector. It is one UTF-8 field `positionInfo` of at most 96 bytes, so any summary must be composed as text (see [RGI and BAP Bridge](RGI_AND_BAP_BRIDGE.md)).

## 🔗 Related

- [RGI and BAP Bridge](RGI_AND_BAP_BRIDGE.md): how `BAPBridge` sends 17/18/19/22/23/39/49 and the FSG sync.
- [KDK Geometry and Animation](KDK_GEOMETRY_AND_ANIMATION.md): how incoming FctID 44/54 place and reveal the KDK.
- [VC FPK State Machine](VC_FPK_STATE_MACHINE.md): the VC dp items these FctIDs land in.

## 📚 Sources

Merged originals were removed from the tree after this note absorbed them; they remain in git history (commit 23972aa).

- `NAVSD_FCTID_MATRIX`: the catalogue, ownership classes, selectors and the route-info phase design.
