# iAP2 Route-Guidance TLV coverage — what iOS sends vs what we consume

Ground truth: **accessoryd 23G71** (iOS 26.6) `+[ACCNavigation* keyForType:]` enums, decompiled.
Ours: `hook/routeguidance/rgd_tlv.h` (parser) + `RouteGuidance.State` fields (what reaches the cluster).

## RouteGuidanceUpdate (0x5201) — Apple types 0x01–0x1A

| type | Apple field | we parse | reaches cluster | note |
|---|---|---|---|---|
| 0x01 | RouteGuidanceState | yes | yes (routeState) | |
| 0x02 | ManeuverState | yes | yes (maneuverState) | |
| 0x03 | CurrentRoadName | yes | yes (currentRoad) | |
| 0x04 | DestinationName | yes | yes (destination) | |
| 0x05 | EstimatedTimeOfArrival | yes | yes (etaSeconds) | |
| 0x06 | TimeRemainingToDestination | yes | yes (timeRemainingSeconds) | |
| 0x07 | DistanceRemaining | yes | yes (distDestM) | |
| 0x08 | DistanceRemainingDisplayString | — | no | iOS pre-formatted text; we format ourselves |
| 0x09 | DistanceRemainingDisplayUnits | — | no | same |
| 0x0A | DistanceRemainingToNextManeuver | yes | yes (distManeuverM) | |
| 0x0B | DistToNextManeuverDisplayString | — | no | we format ourselves |
| 0x0C | DistToNextManeuverDisplayUnits | — | no | same |
| 0x0D | RouteGuidanceManeuverCurrentList | yes | yes (maneuverOrder) | |
| 0x0E | RouteGuidanceManeuverCount | yes | yes (maneuverCount) | |
| 0x0F | RouteGuidanceBeingShownInApp | yes | yes (visibleInApp) | NOT route-active; see issue #8 |
| 0x10 | LaneGuidanceCurrentIndex | yes | yes (laneGuidanceIndex) | |
| 0x11 | LaneGuidanceTotalCount | yes | yes (laneGuidanceTotal) | |
| 0x12 | LaneGuidanceShowing | yes | yes (laneGuidanceShowing) | |
| 0x13 | **SourceName** | define only | **no** | which nav app (Apple Maps/Waze/Google). Parsed name exists, no State field. |
| 0x14 | SourceSupportsRouteGuidance | yes | yes (sourceSupportsRg) | |
| 0x15 | **DestinationTimeZoneOffsetMinutes** | parsed, dropped | no | destination TZ; we render the arrival clock in the VEHICLE's current TZ instead (see below) |
| 0x16 | **StopType** | parsed, dropped | no | multi-stop: waypoint vs final destination |
| 0x17 | **ChargingStationInfoList** | parsed, dropped | no | EV: charging stops along the route |
| 0x18 | **ArrivalBatteryLevel** | parsed, dropped | no | EV: battery % at arrival |
| 0x19 | **DepartureBatteryLevel** | parsed, dropped | no | EV |
| 0x1A | **FinalWaypointBatteryLevel** | parsed, dropped | no | EV |

CORRECTION: 0x15–0x1A ARE handled in `rgd_tlv.c:405` — the switch recognises them and
deliberately drops them (they are not silently unknown). `rgd_tlv.h` only stops adding *named*
macros at 0x14; the C parser knows 0x15–0x1A and skips them.

## ManeuverUpdate (0x5202) — Apple types 0x01–0x0D

Fully covered. Our `MAN_TLV_*` = Apple `ACCNav_MIUpdate_*` one-for-one (Index, ManeuverDescription,
ManeuverType, AfterManeuverRoadName, DistanceBetweenManeuver{,DisplayString,DisplayUnits},
DrivingSide, JunctionType, JunctionElementAngle, JunctionElementExitAngle, LinkedLaneGuidanceInfo,
ExitInfo). Nothing missing here.

## Whole message families we do NOT handle at all

**RoadObjectDetection** (`ACCNavigationRoadObjectDetectionInfo`, separate iAP2 message):
- 0x00 ID, 0x01 Timestamp, 0x02 **EgoSpeed**, 0x03 **EgoYawRate**, 0x04 RoadSign, 0x05 RoadLane, 0x06 RoadObject
- RoadSign sub-info: ID, State, **Type**, **Value**, LateralOffset, ForwardOffset
  → `Value`+`Type` = detected **speed-limit sign** (and other signs).
- The `Ego*` fields imply the accessory feeds its own speed/yaw and the phone returns detected
  objects — a negotiated request/response, only live if the HU advertises the detection component.
  We advertise no such component, so iOS sends none of this today.

## Opportunities, ranked by cluster value vs effort

1. **SourceName (0x13)** — cheapest: the hook already parses it, only a `State.sourceName` field +
   forward is missing. Lets the cluster show which nav app is guiding.
2. **EV fields (0x17 ChargingStationInfoList, 0x18 ArrivalBatteryLevel, 0x1A FinalWaypointBattery)** —
   high value on e-tron / EV Audis, medium effort: extend the 0x5201 parser past 0x14 + BAP surface.
   Only worth it if the cluster has (or we can drive) a battery/charging widget.
3. **StopType (0x16) / DestinationTimeZoneOffsetMinutes (0x15)** — minor polish. NOTE on 0x15: we
   already render ETA as an absolute arrival wall-clock (`BAPBridge.publishTimeToDestination`,
   `updateTimeToDestination(1,...)`), converting the Apple ETA epoch with
   `convertUtcToLocalMs` = the HU UTCOffset DSI = **the vehicle's CURRENT timezone** (DST-aware).
   We do NOT know the destination's timezone. Same-TZ trips: identical. Cross-TZ trips: our clock
   is in the car's current zone, iPhone shows the destination zone — they diverge until the car
   crosses the boundary and the HU offset updates. 0x15 would close that cross-TZ gap; it is an
   edge case, not redundant with what we have.
4. **RoadObjectDetection / speed-limit signs** — highest wow (speed limit on the cluster) but highest
   effort: needs us to advertise a detection component AND feed EgoSpeed/EgoYawRate from vehicle
   sensors; unverified that iOS streams it over wired CarPlay in practice. Research-grade, not a quick win.

The display-string variants (0x08/0x09/0x0B/0x0C) are deliberately ignored — we format distances
ourselves for the cluster's unit/locale, so consuming Apple's pre-baked strings would be a regression.
