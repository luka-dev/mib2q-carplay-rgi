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
