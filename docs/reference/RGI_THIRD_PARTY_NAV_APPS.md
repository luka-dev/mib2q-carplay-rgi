# Why Waze and AMap send no route guidance to the cluster

Analysed from the decrypted IPAs (`com.waze.iphone` 5.23.0.1, `com.autonavi.amap` 16.25.0) and
iOS 26.6 (23G71) `CarPlay.framework` / `CarPlaySupport`. Every claim below is a symbol or a
property list read out of a binary; nothing here was tested on the vehicle.

## Where RGI actually comes from on the phone

iAP2 `RouteGuidanceUpdate` is **not** produced by the app and not by Maps alone — it is produced by
`CarPlay.framework` from two private model objects, `CPRouteGuidance` and `CPManeuver`, through a
generic serializer:

```
CPMapTemplate startNavigationSessionForTrip:      app calls this
  -> CPNavigationSession                          app fills maneuvers here
  -> CPNavigationManager setRouteGuidance:/modifyRouteGuidance:
  -> CPRouteGuidance  (CPAccNavUpdate)            +accNavParameters
  -> +[CPAccNavUpdate accNavInfoWithUpdate:component:customValueBlock:]
  -> ACCNavigationRouteGuidanceUpdateInfo / ACCNavigationManeuverUpdateInfo
  -> accessoryd -> iAP2 RouteGuidanceUpdate
```

Symbols (iOS 26.6, `CarPlay` image base `0x23d8f1000`):

| what | address |
| --- | --- |
| `-[ACCNavigationManeuverUpdateInfo(CPAccNavInfo) initWithManeuver:component:]` | `0x23d9119e0` |
| its custom-value block (keys 2 and 4) | `0x23d911ab8` |
| `+[CPAccNavUpdate accNavInfoWithUpdate:component:customValueBlock:]` | `0x23d8f68a8` |
| `+[CPManeuver(CPAccNavUpdate) accNavParameters]` once-block | `0x23d91d540` |
| `+[CPRouteGuidance(CPAccNavUpdate) accNavParameters]` once-block | `0x23d916be0` |

## Exactly what fits through the wire

The once-blocks build a fixed `CPAccNavParam(property, keys)` table. That table **is** the accessory
payload — anything not in it never leaves the phone.

`CPManeuver`, 13 parameters:

```
componentID  index  instructionVariants  maneuverType  roadFollowingManeuverVariants
initialDistance  initialDistanceDisplay  trafficSide  junctionType
junctionElementAngles  junctionExitAngle  linkedLaneGuidanceIndex  highwayExitLabel
```

`CPRouteGuidance`, 22 parameters:

```
componentID  guidanceState  maneuverState  currentRoadNameVariants  destinationNameVariants
estimatedTimeOfArrival  timeRemaining  distanceRemaining  distanceRemainingDisplay
distanceRemainingToNextManeuver  distanceRemainingToNextManeuverDisplay
currentManeuverIndexes  totalManeuverCount  beingShownInApp  currentLaneGuidanceIndex
totalLaneGuidanceCount  laneGuidanceShowing  sourceName  sourceSupportsRouteGuidance
destinationTimeZoneOffsetMinutes  stopType  electricVehicleDestination
```

Two consequences worth stating plainly:

* **No image property is in either table.** `symbolImage`, `symbolSet`, `junctionImage`,
  `dashboardSymbolImage`, `cardBackgroundColor` are CarPlay-screen-only. The cluster is given a
  *semantic* maneuver — a type, a junction shape, angles, a distance, a string — and draws its own
  icon. That is why our `ManeuverMapper` exists.
* `initialDistance` is a derived property: `-[CPManeuver(CPAccNavUpdate) initialDistance]` returns
  `initialTravelEstimates.distanceRemaining`. Setting travel estimates is enough for distance.

## The on/off gate: Waze fails it, AMap passes it

`CPRouteGuidance.sourceSupportsRouteGuidance` (iAP2 TLV 0x14) is written in exactly one place:
`-[CPSTemplateInstance setSendsNavigationMetadata:]` (`CarPlaySupport` `0x24a18b3c8`, block
`0x24a18b4c4`) copies the BOOL straight through. That BOOL originates in
`-[CPMapTemplate startNavigationSessionForTrip:]` (`0x23d92cfb4`):

```objc
if ([mapDelegate respondsToSelector:@selector(mapTemplateShouldProvideNavigationMetadata:)]) {
    BOOL b = [mapDelegate mapTemplateShouldProvideNavigationMetadata:self];
    [session setSendsNavigationMetadata:b];
    if (b) [session pauseTripForReason:2 description:nil];
}
```

and is carried to the host as
`hostStartNavigationSessionForTrip:sendsNavigationMetadata:supportsRouteSharing:reply:`
(block `0x23d92d354`), where `-[CPSMapTemplateViewController hostStart…]` (`0x24a233380`) calls
`[navigatorObserver setSendsNavigationMetadata:]` **unconditionally** with whatever arrived.

`CPNavigationSession` does not default the flag on: `-[CPNavigationSession initWithTrip:mapTemplate:]`
(`0x23d925ea8`) leaves the ivar zeroed. **If the delegate does not implement the selector, the whole
`if` is skipped and the flag stays NO.**

* **AMap implements it three times** — `-[WCPMapPresenter mapTemplateShouldProvideNavigationMetadata:]`
  (`0x105036104`, forwards to the current map state), `-[WCPMapState …]` (`0x105037fe4`, `mov w0, #0`)
  and `-[WCPMapStateBaseNavi …]` (`0x10503ebc8`, tail-call). So AMap turns it on while navigating and
  off otherwise. It passes the gate.
* **Waze does not implement it at all.** The selector occurs exactly twice in the Waze binary, both
  times inside an emitted `CPMapTemplateDelegate` protocol descriptor (`objc` dump lines 671 and 719);
  neither of its two delegates — `WZCarPlayMapController` nor `WZMapTemplateManager` — carries it in
  its method list, and IDA finds no function of that name anywhere in the image. The phone therefore
  tells the car, explicitly, `SourceSupportsRouteGuidance = 0`.

That is the whole Waze story: it is not sending a weak maneuver, it is declaring no route guidance.
Our own `RouteGuidance.java` clears on `source_supports_rg == 0`, so the cluster correctly stays
stock.

## Waze in detail

Source path baked into the assertions: `third_party/waze/client/waze/src/iphone/Source/**CarPlayLegacy**/
WZCarPlayCoordinator/WZCarPlayCoordinator+NavigationSession.mm`. Waze's own name for this code is
"CarPlayLegacy".

`-[WZCarPlayCoordinator buildManeuverForDirectionsItemAtIndex:inItems:isFirst:]` (`0x10005ca10`),
complete — this is every property Waze ever sets on a `CPManeuver`:

```objc
CPManeuver *m = [CPManeuver new];
m.maneuverInfo.displayAndThenArrowFirst = item.displayAndThenArrowFirst;  // WZCPManeuverInfo, userInfo
m.maneuverInfo.isLanesManeuver          = NO;
m.instructionVariants                   = [builder instructionVariantsFrom:item];
if (flag(978)) m.attributedInstructionVariants = [builder attributedInstructionVariantsFrom:item];
if ([self canShowLanesForDirectionsItemAtIndex:i inItems:items]) {
    m.junctionImage          = <lane strip rendered to a bitmap>;
    m.dashboardJunctionImage = <same>;
}
[self addTurnSymbolToManeuver:m forDirectionsItemAtIndex:i inItems:items];   // -> symbolImage
m.initialTravelEstimates = [[CPTravelEstimates alloc] initWithDistanceRemaining:d timeRemaining:t];
```

Of those, only `instructionVariants` and `initialTravelEstimates` are in the accessory table.
`userInfo`, `junctionImage`, `dashboardJunctionImage` and `symbolImage` are all phone-local.

`-[WZCarPlayCoordinator addTurnSymbolToManeuver:forDirectionsItemAtIndex:inItems:]` (`0x10005c590`)
shows how much semantics is thrown away. It reads `item.imageName`, `item.isRoundabout` and
`item.roundaboutNumber`, and for a roundabout it **draws the exit number into a `UILabel` over a
`UIImageView` and snapshots the result to a `UIImage`**. The exit number — which `CPManeuverType`
models directly as `RoundaboutExit1…RoundaboutExit19`, and `junctionExitAngle` models as an angle —
becomes pixels.

Waze's own turn taxonomy is already complete: 22 CarPlay icon assets, including left/right/exit/
u-turn/HOV/end/stop, four roundabout entry variants, and `_UK` twins for left-hand traffic — which is
exactly `CPTrafficSide`. Every input `CPManeuverType`, `CPJunctionType` and `CPTrafficSide` need is
present in the item; it is discarded at the CarPlay boundary.

Session level, `-[WZCarPlayCoordinator startNavigationSessionImplementation]` (`0x10005bc30`):
the trip is built with the deprecated `initWithOrigin:destination:routeChoices:` and an **empty**
route-choices array, and `CPTrip.destinationNameVariants` (iOS 17.4) is never set — only
`MKMapItem.name` — so the accessory's `destinationNameVariants` would be empty even if the gate were
open. Waze never calls `setCurrentRoadNameVariants:`, `setManeuverState:`, or builds a
`CPLaneGuidance` (its `WZCPManeuverInfo.isLanesManeuver` proves it knows which maneuvers have lanes).

Not an SDK-age problem: Waze already implements the iOS 26.4 delegate methods
`mapTemplate:didRequestToInsertWaypoint:intoSegment:completion:` and
`mapTemplate:mapTemplateWaypoint:accepted:forSegment:`, both of which take `CPRouteSegment`. It also
ships a full `CPTemplateApplicationInstrumentClusterScene` implementation
(`CarPlayInstrumentCluster*`, `Waze.CarPlayClusterSceneDelegate`) — but that is Apple's *video*
cluster, a second CarPlay display, and has nothing to do with a BAP cluster fed over iAP2.

## What would fix it

Nothing on our side, and the two apps need different fixes.

**Waze** needs two changes, in order. First implement
`mapTemplateShouldProvideNavigationMetadata:` on `WZMapTemplateManager` returning YES while
navigating — without it nothing else matters, because the phone reports
`SourceSupportsRouteGuidance = 0`. Then set `CPManeuver.maneuverType` in
`buildManeuverForDirectionsItemAtIndex:inItems:isFirst:`; the mapping is mechanical from the
`imageName` / `isRoundabout` / `roundaboutNumber` it already has.

**AMap** only needs the second half — it already opts in; its maneuvers just carry no type.

Both would also want `junctionType` + `junctionExitAngle` for roundabouts, `trafficSide`,
`linkedLaneGuidance` instead of a rendered lane strip, and
`CPNavigationSession.maneuverState` / `currentRoadNameVariants`.

## Waze altScreen: steering-wheel zoom is dropped inside Waze

Waze 5.24.0.1, decrypted IPA, arm64 (IDA). This is the *video* cluster (our altScreen /
type-111 stream), separate from the BAP route guidance above.

Waze does ship the cluster scene: `Info.plist` maps
`CPTemplateApplicationInstrumentClusterSceneSessionRoleApplication` to
`Waze.CarPlayClusterSceneDelegate` and sets `CPSupportsInstrumentClusterNavigationScene = true`.
`…didConnectInstrumentClusterController:` (`0x100355b48` → `0x10035592c`) publishes
`.connected(scene, controller)`; `CarPlayInstrumentClusterDisplayManager` consumes it
(`0x1009aec50`): reads `contentStyle`, takes `instrumentClusterWindow`, `setDelegate:self`. The
manager is only created in `registerClusterScene` (`0x1009afe10`) when config item **1006**
("Instrument cluster enabled", compiled default `"1"`, server-overridable) is true — otherwise it
logs `cluster - not registering delegate, CARPLAY_INSTRUMENT_CLUSTER_ENABLED is false`.

The delegate implements `instrumentClusterControllerDidZoomIn:` (`0x1009ade98`) and
`…DidZoomOut:` (`0x1009adf68`). Each (`0x1009addfc` / `0x1009adea4`) sends a stat
(`clusterZoomIn` / `clusterZoomOut`, `cluster_zoom_in_stat.proto`) and then
`controlEventSubject.send(.zoom(.in|.out))`. Enum layout: `.zoom(.in)` = `{0,0}`,
`.zoom(.out)` = `{1,0}`, `.configurationChanged(cfg)` has bit 63 of word 0 set.

`events` (`0x1009ae3a0`) = `configurationStateSubject.map{.configurationChanged}
.merge(with: controlEventSubject)`, protocol witness slot +24. Its **only** subscriber is
`CarPlayInstrumentClusterRootCoordinator`: the windowEvents sink (`0x1009b8f88`) calls
`startCoordinator` (`0x1009b58b8`), which does
`events.compactMap { word0 < 0 ? word1 : nil }` (`0x1009b5e9c`) — i.e. keeps only the
`UIUserInterfaceStyle` out of `.configurationChanged` and maps `.zoom` to `nil`. Nothing else
in the module (all 13 `sink` calls in `0x1009ad000–0x1009bc000` enumerated: config, state,
window, skin, map state, presentation, rendering state, safe area, style) touches it.

`CarPlayInstrumentClusterMapPresentable` (`0x1009b25b8`) builds one `WazeMapCamera` with
`mapBoundsStrategy = .trackUserLocation(cameraZoomBehavior: .static(0.000125))` — a hard-coded
1:8000 scale (float `0x3903126f`; the enum also has `.dynamic` = Waze speed auto-zoom and
`.manual`, neither used here) plus north lock / centre offset, and only re-issues `WazeMapState`
on skin change (`0x1009b2bd8`). Compass and speed-limit settings from the HU are dropped by the
same `compactMap`. The camera's `zoomSubject` is fed solely by `-[WazeMapCamera zoomInAnimated:]` /
`zoomOutWithAnimated:` (main-screen map controls), never from the cluster module.

So our `changeMapZoomLevel {uuid: cluster, zoomDirection}` (`zoom_on_cmd`,
`hook/altscreen/altscreen_hook.c`) reaches Waze, is counted as a stat, and is discarded. Apple
Maps and Google Maps act on the same callback, which is why they zoom. Not fixable from the
head unit. If the Waze cluster map does not appear at all, that is the server side of flag 1006.

### The other lever: `updateViewArea` (iOS 27 AirPlaySender / CarKit RE)

The phone accepts exactly these HU→phone commands in `carEndpoint_processEventCreatingResponse`
(AirPlaySender, iOS 27 24A5418b): `forceKeyFrame, hidSendReport, changeModes, iAPSendMessage,
requestSiri, requestUI, updateViewArea, setLimitedUI, setNightMode, setVideoPlaybackAllowed,
videoPlayerBackButtonEvent, updateVehicleInformation`; anything else (incl. `changeMapZoomLevel`)
is "Unhandled command … forwarding to clients" → CarKit/DashBoard → the app's delegate.

`updateViewArea` (`carEndpoint_updateViewArea` 0x2510891fc) is the only command that changes
what an app draws **without the app's cooperation**: params `uuid` (display), `viewAreaIndex`,
`adjacentViewAreas`, `animationDurationMillis`. `carEndpoint_validateViewAreaParams` looks the
`uuid` up in the whole `/info` `displays[]` array (no main-display restriction) and only checks
`viewAreaIndex < viewAreas.count`. Gate: the setup-response feature list must contain
`"viewAreas"` (flag byte endpoint+0x45, same family as `altScreen` = +0x3f), and the display
entry must carry `viewAreas` (array of `{originXPixels, originYPixels, widthPixels, heightPixels,
safeArea?, viewAreaTransitionControl?, viewAreaSupportsFocusTransfer?, viewAreaStatusBarEdge?}`),
`initialViewArea` and `adjacentViewAreas`. On success the phone posts
`kFigEndpointNotification_ViewAreaChanged` → CarKit `-[CARSession
_sessionUpdatesQueue_handleViewAreaChangeWithPayload:]` → per-screen `CARScreenInfo.currentViewArea`
(`CARScreenInfo` is built per display with `screenType`, so the altScreen screen carries its own
`viewAreas`) → SpringBoard re-frames the scenes on that display.

For Waze that means: the cluster window shrinks/grows, the map stays at 1:8000 per point, so the
visible extent changes; the HU crops the active view area out of the (unchanged) 111 stream and
scales it to the panel = zoom steps. Scale trap: `CRScreenScaleHeuristics
getPrimaryDisplayScale:viewAreas:` derives the point scale from pixel/physical size, so a display
declared with more pixels needs proportionally more physical mm or iOS goes @2x and the extent
does not change. `zoomFactor` in the display description is the static version of the same lever.

**State in our hook (`hook/altscreen/altscreen_hook.c`):** `viewAreas` + `updateViewArea` are
already implemented, but only for zone switching (fullscreen / sportscreen / classicscreen). All
three declared view areas keep the full 1440×542 visible frame and differ only in the nested
`safeArea` (fullscreen (270,90) 900×290; sportscreen (90,135) 360×245; classicscreen (540,135)
360×245 — bottom edge y=380, above the VC's own map bar); display physical size 292×110 mm. The comment at line ~258 records that a view area
whose visible frame was reduced to the safe region got the 111 stream rejected at activation, so
zoom-by-view-area needs that rejection understood first (candidates: CarKit
`CRDisplayScaleInfo.minAcceptableViewAreaSize*` / `isViewAreaPixelSizeAcceptable`, or the initial
view area having to equal the display).

## Cluster scene URLs (`showUI`) — iOS 27 24A5418b, 2026-09-19

The HU never sends URLs during activation; the phone does.  AirPlaySender calls
`_APCarPlay_CRFetchInstrumentClusterURLs` → CarKit `CARInstrumentClusterURLController` →
CarPlayServices `-[CRSAppHistoryService serviceFetchInstrumentClusterURLs:]` and puts the
result in the follow-up `/info` request under `altScreenURLs`.  `_appLibraryUpdated:` builds it:

- Maps: `maps:/car/instrumentcluster`, `maps:/car/instrumentcluster/map`,
  `maps:/car/instrumentcluster/instructioncard`; Now Playing: `nowplaying://`.
- Third-party apps contribute `carPlayInstrumentClusterURLSchemes` (LaunchServices record).
  Waze 5.24.0.1 and AMap 16.25.0 declare none in Info.plist.

Selecting the scene is the HU's job: Maps sends `suggestUI{urls}` (stock 210 answers -6714,
we ignore it), the HU answers with `showUI{uuid,url}` at any time during the session.  CarKit
handles it in `-[CARSession _sessionUpdatesQueue_handleShowUIWithParameters:]`; `suggestUI`
is sanitised by intersecting the app list, the session list and the suggested list
(`_CARHandleSuggestUI`).  Query parameters on the Maps map URL, parsed by Maps.app itself:
`maneuverLayout=rightAligned` (Maps turn card; omit it to hide the card), `showETA=yes|no`;
CarPlaySupport's `CPSInstrumentClusterCardViewController` also logs a `mapOnly` flag.

Hook policy (`alt_cluster_url`, switched at runtime by `CMD_ALT_RGI 0x0116` from
`AltScreenModule.republish`): RGI presentation off →
`maps:/car/instrumentcluster?showETA=yes&maneuverLayout=rightAligned&showSpeedLimit=no`; on →
`maps:/car/instrumentcluster/map?showETA=no&maneuverLayout=no&showSpeedLimit=no`. The URL is re-sent
at first IDR, after each accepted `updateViewArea` and on each RGI edge.
`maneuverLayout=no` maps to justification 0 (unset).

### What the phone does with the URL (DashBoard + CarPlaySupport, iOS 26.6 23G71, 2026-09-19)

`-[DBInstrumentClusterURLHandler applySettingsForClusterURL:toSceneSettings:]` (DashBoard,
SpringBoard's CarPlay UI; iOS 26.6 23G71 and iOS 27 24A5418b verified) parses the `showUI` URL for **every** app and writes
`CRSUIInstrumentClusterSceneSettings`. Key names are compared case-sensitively, values are
lowercased first. Unknown keys are ignored; an unsupported path is rejected as a whole.

| URL | setting |
|---|---|
| `maps:/car/instrumentcluster` | itemType 3 (map + cards, "anyContent") |
| `maps:/car/instrumentcluster/map` | itemType 1 (map only) |
| `maps:/car/instrumentcluster/instructioncard` | itemType 2 (card only) |
| `?showETA=` `yes`\|`1` / `no`\|`0` / `user` | showsETA = CPInstrumentClusterSetting enabled 1 / disabled 2 / userPreference 3; any other value → 2; absent → 2 |
| `?showSpeedLimit=`, `?showCompass=` (same values) | showsSpeedLimit / showsCompass — **iOS 27 only** (24A5418b); 26.6 ignores them. Absent → disabled |
| `?maneuverLayout=topAligned` / `leftAligned` / `rightAligned` | layoutJustification 1 / 2 / 3 (value case-insensitive; anything else → 0) |
| `?mapsPresentation=anycontent|map|instructioncard` | itemType 3 / 1 / 2 (Maps hosted content) |

Third-party apps register `[maps:/car/instrumentcluster, maps:/car/instrumentcluster/map]`
through `-[CPSNavigator setAppSupportsInstrumentCluster:]` (suggestUI), so the `maps:`
namespace is shared, not Maps-specific. CarPlay.framework creates the app's cluster window
only for itemType 1 or 3 (`updateSceneForNewItemTypeSetting`).

The ETA and turn cards for third-party apps are drawn in the app process by
`CPSInstrumentClusterCardViewController` (CarPlaySupport) in one platter:

- layout 1 (centre): platter centred, width clamp(safeW, 165, 300), turn card on top
  (0.41 × safe height), ETA at the bottom; 2 / 3: 213 px column at the left / right edge of
  the safe area, full height, turn card top, ETA bottom. ETA needs safe height > 202 px.
- Geometry when no layout is requested (`initWithSafeAreaFrame:…physicalPixelWidth:`):
  scale = 0.153 mm / (physicalWidth / pixelWidth) = 0.7545 for 292 mm × 1440 px; wide
  branch if safeW ≥ 660 × scale ≈ 498 px → 2 or 3 by safe-area mid vs display mid (equal →
  2 on LHD); else centre when 0.3 × safeH > 115 × scale ≈ 87 px, otherwise layout 0
  "Undefined layout (0), will not show cluster content" (platter hidden, ETA included).
  Our zones must therefore keep safeH ≥ 290 (now 300); fullscreen 900 px is always wide.
- `mapOnly` (turn cards disabled) is logged by `-[… setShowETA:]`; the itemType 1 → mapOnly
  plumbing is in the app process but was not located in the extracted 26.6 dylibs
  (selector refs live in the shared cache). Waze on `/map` showed ETA + road name and no
  turn card, consistent with mapOnly; the root URL is the fix under test.
