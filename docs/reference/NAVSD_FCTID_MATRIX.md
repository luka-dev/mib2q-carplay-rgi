# MU1316 NavSD FctID matrix

## Scope and authority

This is the Navigation BAP LSG `0x32` exposed by the stock MU1316 Java stack.
It distinguishes three different facts:

1. **Registered** — `FunctionRegistrationNavi` creates a property, array, or
   method object for the FctID.
2. **Advertised** — the coded `FunctionListNaviEvo` variant exposes it to the
   instrument cluster. Registration alone does not make a function available.
3. **Writable through the public service** — stock `CombiBAPServiceNavi` and
   `AppConnectorNavi` provide a usable Java call. Some registered functions are
   framework-owned command/list machinery and have no direct publishing API.

Authoritative sources:

- stock ID/name catalogue: `FunctionIDsNavi.java`, FctID 1–56;
- stock type registration: `FunctionRegistrationNavi.java` (33 properties,
  6 arrays, 5 methods, plus framework functions);
- coded availability: `FunctionListNaviEvo.java`;
- public seam: `CombiBAPServiceNavi.java` and `AppConnectorNavi.java`;
- our ownership policy: `GatedCombiService.java` and `BAPBridge.java`.

Variant legend: `S` = Std, `H` = HighPrem, `M` = MMIKombi, `MH` =
MMIKombiHUD. `All` means all four. China additionally disables FctID 47.
The feature set observed on the MHI2Q virtual cockpit most closely matches
`H`, but the live FunctionList remains the final authority.

## Complete catalogue

| ID | Hex | Stock name | Kind | Advertised variants | Public HU-side seam | Payload / visible role | Current CarPlay policy |
|---:|---:|---|---|---|---|---|---|
| 1 | `0x01` | GetAll | protocol | All | framework-owned | Request a complete status snapshot | Stock/framework |
| 2 | `0x02` | BAP_Config | property | All | framework-owned | BAP version/configuration | Stock/framework |
| 3 | `0x03` | FunctionList | property | All | framework-owned | Runtime availability bitmap | Stock/framework; use as capability truth |
| 4 | `0x04` | HeartBeat | protocol | All | framework-owned | LSG liveness | Stock/framework |
| 5–14 | — | Reserved/unknown | — | None | none | No NavSD function in this firmware | Do not use |
| 15 | `0x0F` | FSG_OperationState | property | All | `showInitializingScreen()` / `hideInitializingScreen()` | Navigation READY/INITIALIZING state | Always delegated to stock; high-risk lifecycle control |
| 16 | `0x10` | CompassInfo | property | All | `updateCompassInfo(angle, symbolic)` | Compass heading and `N/NE/...` indicator | Delegated to stock |
| 17 | `0x11` | RG_Status | property | All | `updateRGStatus(status)` | Route-guidance active state; starts FctSync | CarPlay-owned for connected session; sent during RGI |
| 18 | `0x12` | DistanceToNextManeuver | property | All | `updateDistanceToNextManeuver(distance, unit, barOn, bar)` | Next-turn distance plus optional bargraph | CarPlay-owned/gated; actively sent |
| 19 | `0x13` | CurrentPositionInfo | property | All | `updateCurrentPositionInfo(text)` | VC lower-bar text | All CarPlay route text and click summaries; phase 0: bracketed exit/signpost, else next road, else current road; phase 1: compact duration in smallscreen, arrival + duration in fullscreen; stock allowed for `CP && !RGI` |
| 20 | `0x14` | TurnToInfo | property | All | `updateTurnToInfo(turnTo, signPost)` | Separate turn-to/signpost overlay | Both strings explicitly empty during CarPlay RGI, with stock writes gated; no CarPlay text or ETA sent here |
| 21 | `0x15` | DistanceToDestination | property | All | `updateDistanceToDestination(value, unit, stopover)` | Destination/trip distance | CarPlay-owned/gated during active RGI; cached for instant info-mode refresh |
| 22 | `0x16` | TimeToDestination | property | All | `updateTimeToDestination(type, format, time)` | ETA or remaining travel time | CarPlay-owned/gated during active RGI; always publishes absolute ETA (`timeInfoType=1`) |
| 23 | `0x17` | ManeuverDescriptor | property | `S/H/MH` | `updateManeuverDescriptor(slots[])` | Up to three maneuver descriptors for VC/HUD | CarPlay-owned/gated; the real current-only descriptor is shown at every distance; approach thresholds affect only bargraph/action timing |
| 24 | `0x18` | LaneGuidance | array | `S/H/MH` | `updateLaneGuidance(show, lanes[])` | Lane arrows and recommended-lane flags | CarPlay-owned/gated; actively sent |
| 25 | `0x19` | TMCinfo | property | `S/H` | `updateTMCInfoMessages(messages[])` | Urgent traffic messages | Delegated to stock |
| 26 | `0x1A` | MagnetFieldZone | property/SetGet | None | no public publisher | Compass calibration region | Registered but not advertised; do not use |
| 27 | `0x1B` | Calibration | property/SetGet | None | no public publisher | Compass calibration state | Registered but not advertised; do not use |
| 28 | `0x1C` | ASG_Capabilities | property/SetGet | None | no public publisher | Cluster/navigation capabilities | Registered but not advertised; framework-owned |
| 29 | `0x1D` | LastDest_List | array | `S/H` | `updateLastDestinationsList(entries[])` | Last-destination menu | Delegated to stock |
| 30 | `0x1E` | FavoriteDest_List | array | `S/H` | `updateFavoriteDestinationsList(entries[])` | Favorite-destination menu | Delegated to stock |
| 31 | `0x1F` | PreferredDest_List | property | None | no direct publisher | Selects preferred source list | Registered, not advertised |
| 32 | `0x20` | NavBook | array | None | no direct publisher | Navigation address book | Registered, not advertised |
| 33 | `0x21` | Address_List | array | `S/H` | `updateHomeAddress(destination)` | Address/home list content | Delegated to stock |
| 34 | `0x22` | RG_ActDeact | method | `S/H` | `routeGuidanceActDeactResult(result)` | Response to cluster start/stop-guidance command | Stock command path; not a drawing primitive |
| 35 | `0x23` | RepeatLastNavAnnouncement | method | None | `repeatLastNavAnnouncementResult(result)` | Response to repeat-announcement command | Registered but not advertised |
| 36 | `0x24` | VoiceGuidance | property/SetGet | `S/H` | `updateVoiceGuidanceState(state)` | Voice-guidance mode | Delegated to stock |
| 37 | `0x25` | FunctionSynchronisation | property | All | framework-owned | Atomic RG status synchronization | Used implicitly by FctID 17/23/18/49; never write directly |
| 38 | `0x26` | InfoStates | property | All | `updateInfoStates(state)` | No error, no GPS, initializing map, mobile-device navigation | Always delegated to stock; **high-risk global navigation state** |
| 39 | `0x27` | ActiveRGType | property/SetGet | `S/H` | `updateActiveRGType(type)` | Selects RGI/MOST/LVDS guidance presentation | CarPlay-owned/gated; currently sends type `0` (BAP RGI) |
| 40 | `0x28` | TrafficBlock_Indication | property | `S/H/MH` | `updateTrafficBlockIndication(symbol)` | Traffic-block/TMC icon | Delegated to stock |
| 41 | `0x29` | GetNextListPos | method | `S/H` | no public result seam | Internal list paging | Framework/list manager |
| 42 | `0x2A` | NB_Speller | method | None | no public result seam | Navigation-book spelling/input | Registered but not advertised |
| 43 | `0x2B` | MapColorAndType | property/SetGet | `H` | `updateMapColor`, `updateMapType`, `updateSupportedMapTypes` | Stock map colour/type/capabilities | Delegated to stock; does not control the CarPlay video map |
| 44 | `0x2C` | MapViewAndOrientation | property/SetGet | `H` | `updateMapView`, `updateSupportedMapViews`, `updateMapVisibility`, `updateMapOrientation` | Stock map view, orientation and visibility | Delegated to stock; high risk for native-map lifecycle |
| 45 | `0x2D` | MapScale | property/SetGet | `H` | `updateMapScale(auto, enabled, value, unit, intersection)` | Native scale/auto-zoom and steering-wheel SetGet ACK | Cached and replaced with hidden status while CarPlay owns cluster; restored afterward |
| 46 | `0x2E` | DestinationInfo | property | All | `updateDestinationInfo(info)` | Destination details/menu | CarPlay-owned/gated during active RGI |
| 47 | `0x2F` | Altitude | property | `H/M/MH`, except China | `updateAltitude(value, unit)` | Native lower-bar altitude | Cached/suppressed while CarPlay owns cluster; restored afterward |
| 48 | `0x30` | OnlineNavigationState | property | `H/M/MH` | `updateOnlineNavigationState(state, progress, system)` | Online-map/download status | Delegated to stock |
| 49 | `0x31` | Exitview | property | `S/H/MH` | `updateExitView(variant, id)` | Junction/exit-view data and RG FctSync member | CarPlay-owned/gated; actively toggled to complete sync |
| 50 | `0x32` | SemidynamicRouteGuidance | property | All | `updateSemidynamicRouteGuidance(info)` | Traffic delay and alternative-route distance/new ETA | Delegated to stock; not current-route live ETA |
| 51 | `0x33` | POI_Search | method | None | `poiSearchResult(result, count)` | Response to cluster POI search | Registered but not advertised |
| 52 | `0x34` | POI_List | array | None | `updatePOIListSize()` is a stock no-op stub | POI result list | Registered but not usable through current service |
| 53 | `0x35` | FSG_Setup | property | `S/H` | `updateFSGSetup(voiceMask, poiSupport)` | Voice-guidance and POI capabilities | Delegated to stock |
| 54 | `0x36` | Map_Presentation | property/SetGet | `H` | `updateMapPresentation(largeMap, leftMenu, rightMenu)` | Reports stock map size and side-menu states | Delegated to stock; the echo cannot close a VC-local drawer |
| 55 | `0x37` | ManeuverState | property | `S/H` | `updateManeuverState(state)` | Maneuver/action transition state used by HUD/VC | CarPlay-owned/gated; actively sent |
| 56 | `0x38` | ETC_Status | property | `M/MH` | `updateEtcStatus(status)` | Electronic-toll-card status | Not advertised by HighPrem; delegated to stock |

## Important enums and compound functions

### FctID 22 — TimeToDestination

`timeInfoType` is not another FctID; it changes the meaning of FctID 22:

| Value | Meaning | Value passed to `updateTimeToDestination` |
|---:|---|---|
| `0` | Remaining travel time | Duration in seconds; serializer emits hours/minutes |
| `1` | Absolute arrival time | Unix epoch seconds; serializer emits date/hour/minute |
| `2` | Arrival time local to another time zone | Explicit remote-local ETA |
| `15` | Unsupported/invalid | Validity bits cleared |

The bridge always uses type `1`, independent of view and presentation phase.
If iOS supplies only remaining time, the bridge computes `now + remaining` and
publishes an absolute ETA. Remaining time is exposed as text in FctID 19
CurrentPositionInfo, selected with the steering-wheel click. FctID 20 stays empty.

### FctID 38 — InfoStates

Relevant values are `0` no error, `3` no GPS, `5` initializing MOST map, and
`6` navigation in a mobile device active. This property affects the whole
navigation presentation and was involved in earlier `Initializing` / navigation
unavailable behaviour. It must not be repurposed merely to hide a widget.

### FctID 39 — ActiveRGType

Values: `0` BAP ManeuverDescriptor RGI, `1` MOST KDK, `2` compass, `3` MOST
map, `4` LVDS map, `5` LVDS KDK, `255` cluster default. Our custom maneuver
pipeline deliberately uses `0`; the separate Screen planes do not require
claiming an LVDS RG type.

### FctID 50 — SemidynamicRouteGuidance

This packages traffic impact, alternative-route availability, delay, new
distance, and a **new-route ETA**. It should not replace FctID 22 for the live
current route.

## Current ownership summary

| Ownership class | FctIDs | Rule |
|---|---|---|
| CarPlay exclusive while takeover/RGI owns them | `17,18,23,24,39,49,55` | Stock writes are dropped by `GatedCombiService`; `BAPBridge` publishes them |
| CarPlay exclusive only during active RGI | `19,20,21,22,46` | Stock route-text/trip output is allowed again for `CP && !RGI`; CarPlay owns it atomically while RGI is active |
| Suppressed native-map decorations | `45,47` | Cache stock value, publish hidden/invalid while takeover is active, restore on release |
| Explicitly left stock-owned | `15,16,25,29,30,33,36,38,40,43,44,48,50,53,54,56` | Avoid disturbing native navigation lifecycle and unrelated menus |
| Framework/command plumbing | `1–4,26–28,31,32,34,35,37,41,42,51,52` | Not general-purpose UI setters; method seams are command results rather than status controls |

## Practical candidates

- **Safe/useful now:** variants of FctID 22; richer FctID 21/46 destination
  data; FctID 25/40 only if CarPlay traffic data is available.
- **Single-owner interactive presentation:** FctID `19,20,21,22,46` while
  CarPlay RGI is active.
- **Potentially destructive:** FctID `15,38,43,44,45,54`; these control the
  stock navigation state/map rather than the CarPlay altScreen scene.
- **Not useful for visual customization:** method responses `34,35,41,42,51`
  and framework synchronization `37`.

## Representation selectors inside a FctID

These fields alter presentation without selecting a different FctID. They are
not all equally safe to drive during CarPlay ownership.

| FctID | Selector | Values / effect | CarPlay use |
|---:|---|---|---|
| 16 | `direction_Symbolic`, `direction_Angle` | 16 compass sectors or unsupported, plus angle | Stock-owned. `255` does not prove that the VC will hide the compass container. |
| 17 | `rg_Status` | `0` inactive, `1` active, `2` suspended, `255` unsupported | Lifecycle control, not cosmetic; do not use as a display toggle. |
| 18 | `bargraphOnOff`, distance validity | Independent fields on the wire; display policy is consumer-specific | CarPlay sends both, but the audited AU491 VC HMI hides distance while `BargraphOn != 0`. The HU `ClusterService` patch does not override that predicate. See [full chain and implementation options](DISTANCE_BARGRAPH_CHAIN.md). |
| 21 | `distanceToStopover` | `false` final destination, `true` next stopover | Safe if the CarPlay route exposes a real waypoint distinction. |
| 22 | `timeInfoType` | `0` remaining time, `1` arrival time, `2` arrival in another time zone, `15` invalid | Keep type `1`: this VC does not render a remaining-duration widget. Click summaries use FctID 19 instead. |
| 22 | `navigationTimeFormat` | `0` 24-hour, `1` 12-hour | Follow HU locale; this changes formatting, not ETA/RTT semantics. |
| 23 | `mainElement`, `direction`, `zLevelGuidance`, descriptor slot | Selects maneuver glyph, signed direction/angle, level up/down and current/next maneuver | Safe only when derived from the actual iOS maneuver. |
| 24 | `laneGuidanceOnOff`, per-lane `guidanceInfo` | Hide/show lanes; `0` not recommended, `1` recommended, `2` best | Safe when sourced from iOS lane guidance. Lane type/markings/description further alter glyphs. |
| 25 | `messageStatus`, `messageWaitingIndication` | Traffic-message popup request/confirmation and waiting count | Not a general overlay switch; leave stock-owned unless CarPlay traffic events are mapped fully. |
| 38 | `states` | Normal/error/no GPS/initializing/mobile-device navigation | Global navigation lifecycle. Never use merely to alter a widget. |
| 39 | `rgtype` | BAP RGI, MOST KDK, compass, MOST map, LVDS map, LVDS KDK, default | Selects the entire guidance presentation source; high-risk, not a cosmetic toggle. |
| 40 | `tmc_Symbol` | No icon or a specific traffic-event glyph | Safe only with a matching real traffic event. |
| 43 | `colour`, `activeMapType`, `mainMapSetup` | Day/night/auto and native destination/2D/3D/overview/range map | Controls the stock map, not stream 111; leave stock-owned. |
| 44 | `mapView`, `supplementaryMapView`, `mapOrientation`, visibility bits | Standard/online/traffic, compass/intersection/box, north/heading, map visibility | Controls the stock map and lifecycle; leave stock-owned. |
| 45 | `autoZoom`, `scale`, `unit` | Auto-zoom mode and native scale display; incoming SetGet carries wheel steps/settings | Keep the status hidden during CarPlay. Its consumed SetGet is a useful steering-wheel input seam. |
| 49 | `variant`, `exitview_Id` | EU/NAR/ROW/Asia junction-view resource selection | Only useful with a real matching exit-view resource. |
| 50 | alternative-route/traffic flags, distance type, nested `timeInfoType` | Controls alternative-route/traffic card and its proposed distance/ETA | Not the current-route ETA; leave stock-owned until alternative-route data exists. |
| 54 | `largeMapView`, `leftSideMenueOpen`, `rightSideMenueOpen` | Reports map size and side-menu state | This is ASG HMI state, not a command: the VC *reports* its drawer through it. Blocking the cluster side menus from the HU was tried and failed - see docs/reference/CLUSTER_SIDE_MENUS.md before trying again. |
| 55 | `state` | `0` unknown, `1` follow, `2` prepare, `3` distance, `4` call-for-action | Changes maneuver phase/animation; safe only when synchronized to the real maneuver. |

FctID 19 is different: it has no representation enum. It is one UTF-8 text
field (`positionInfo`, maximum 96 bytes). A fullscreen summary such as arrival
time + remaining time + remaining distance must therefore be composed as text.

## CarPlay route-info phases

The compact route-info representation is bound to the left steering-wheel
roller press.  It is intentionally not derived from Navi FctID 45: that BAP
function carries map-scale steps and auto-zoom, not OK.  A dedicated
`DSIKeyPanel` listener observes raw `KEY_MFW_ROLLER_LEFT` (key 40) before the
stock keyboard stack collapses it and the centre-console DDS (key 16) into the
same `INC_MENU_ENTER`/`Key.DDS_SELECT` event.  The action is fail-closed unless
`IMMICombiScreenChangeManager.getLastConfirmedDisplayStatus()` reports
`MAIN_CONTEXT_NAVIGATION_MAP` (20), i.e. the VC is really on its map tab.
The marked key-40 copy is removed at the CarPlay DSI controller on every tab,
so it never becomes an iOS Select; unmarked key 16 remains normal CarPlay
Select.

| View | Phase 0 | Phase 1 |
|---|---|---|
| Fullscreen | FctID 19: `‹exit/signpost›`, else next road, else current road; no synthetic arrow | FctID 19: `◌ 10:42 PM` + ` \| ` + `37 min` (or `1 h 05 min`) |
| Smallscreen | Same FctID 19 route text as fullscreen; no synthetic arrow | FctID 19: `◌ 37 min` (or `◌ 1 h 05 min`) |

In both views/phases, FctID 20 is `("", "")` and FctID 22 remains absolute ETA.
Arrival-clock text follows the HU's 12/24-hour setting and local time conversion.

Every fullscreen/smallscreen View transition resets to phase 0. The serialized
presentation worker republishes cached route state and travel estimates immediately,
without waiting for another iOS delta. A second click restores the normal route text.

Input priority is `mExitInfo -> mAfterRoad -> mName -> currentRoad`, after whitespace
normalization. Exit/signpost text is enclosed in `‹…›`; it retains its full input
(including any colon prefix and next-road name), because there is no competing text
row to deduplicate. `mAfterRoad` retains the existing last-colon-part normalization.
All text uses the 96-byte UTF-8 budget of FctID 19, reserving six bytes for the
U+2039/U+203A angle quotes (90 bytes remain for exit/signpost content);
neither multibyte characters nor surrogate pairs are split at the limit.

The user-selected time marker is U+25CC DOTTED CIRCLE (`◌`), replacing the literal
`ETA` prefix in both views. It is a standalone character, not U+20DD COMBINING
ENCLOSING CIRCLE and not an emoji sequence. VC 0379's Arabic and JP/KR font files
contain it, but the five Audi faces do not: actual VC font fallback/display remains
an on-unit acceptance test. Angle quotes have direct coverage in all five Audi
faces. See [VC font glyphs](VC_FONT_GLYPHS.md) for extracted-font evidence.
The user reports that the tested route-text output does not appear on HUD;
this layout therefore targets the VC, without changing HUD maneuvers/distances.
That observation is specific to the tested configuration, not proof that every
HUD text mode is unsupported.

An unknown ETA falls back to normal route text. If neither route text nor an estimate
exists, active RGI publishes a neutral `…`, including during initial startup. Do not
use `""` to hide the CurrentPositionInfo widget: on this VC it can expose the native
circle/`---` placeholder. Teardown still clears the fields and releases stock ownership.

### 2026-09-10 rollback of the separate-text experiment

Commit `3f1ef4a` (2026-09-09) moved text from FctID 19 into FctID 20 and deliberately
emptied FctID 19 on smallscreen clicks. This removed the nonempty fallback introduced
in `2308e51` (2026-08-08). The user reported the stock VC placeholder after a click
and requested the previous all-CurrentPositionInfo presentation, without our arrow
and with brackets for exit/signpost text. Keep the FctID 20 ownership gate so stock
cannot reintroduce a second layer; real maneuver descriptors, distance/bargraph logic,
and stock passthrough outside RGI are unchanged. This rollback does not send text ETA
to the HUD through SignPost.

Host verification: `bash scripts/test_route_info.sh` builds the shipping Java 1.4
JAR and runs the actual BAPBridge against a recording stock-service proxy. Covers
both views/phases, input priority, missing estimates/text, UTF-8/bracket boundaries,
repeated clicks, FctID 20 clearing, and FctID 22 remaining type 1. Vehicle display
validation is still required; HU was offline when this rollback was prepared.
