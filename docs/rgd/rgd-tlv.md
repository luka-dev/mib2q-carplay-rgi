---
title: iAP2 route-guidance TLV map (0x5200-0x5204)
tags: [rgd, hook, ios-re, verified]
status: verified-decompile
sources:
  - code: hook/routeguidance/rgd_tlv.h
  - code: hook/routeguidance/rgd_tlv.c
  - firmware: accessoryd 23G71 +[ACCNavigationRouteGuidanceUpdateInfo keyForType:]
reconciles:
  - docs/reference/NAVSD_FCTID_MATRIX.md
  - docs/reference/IOS266_MANEUVER_DECOMPILE.md
  - docs/reference/IAP2_RGI_INJECTION_FIX.md
---

# iAP2 route-guidance TLV map (0x5200-0x5204)

The wire format iOS uses to push route guidance to the accessory, and exactly what our hook parses.
Ground truth = our parser (`rgd_tlv.h`) cross-checked against Apple's own field enum in `accessoryd`.

## Context

First stop in the route-guidance path - **you are here** turns wire bytes into parsed state:

> iOS RGD -> **rgd-tlv** - parse -> [[bus-protocol]] - EVT_RGD_UPDATE -> [[rgd-activation]] - decide ->
> [[maneuver-mapping]] - icons + [[bap-fctids]] - HUD

```mermaid
flowchart LR
    ios["iOS RGD<br/>0x5200-0x5204"] --> p["hook parse<br/>rgd_tlv.c"]:::here
    p --> bus["EVT_RGD_UPDATE<br/>bus-protocol"]
    bus --> act["rgd-activation"]
    bus --> map["maneuver-mapping"]
    classDef here fill:#fde68a,stroke:#b45309,color:#000;
```

## Message family

| Msg | Name | Carries |
|---|---|---|
| 0x5200 | StartRouteGuidanceUpdates | source name, `SourceSupportsRouteGuidance`, `SupportsExitInfo` |
| 0x5201 | RouteGuidanceUpdate | the route-level state (table below) |
| 0x5202 | RouteGuidanceManeuverUpdate | one maneuver's detail (type, angles, roads) |
| 0x5203 | StopRouteGuidanceUpdates | teardown |
| 0x5204 | RouteGuidanceLaneGuidanceInformation | per-lane arrows |

## 0x5201 RouteGuidanceUpdate - our IDs match Apple exactly

Every ID matches `+[ACCNavigationRouteGuidanceUpdateInfo keyForType:]` (accessoryd 23G71).
`0x01-0x14` we **parse**; `0x15-0x1A` Apple emits but we **do not parse** (see gaps).

| ID | Apple field (ACCNav_RGUpdate_*) | our field | parsed |
|---:|---|---|:--:|
| 0x01 | RouteGuidanceState | `routeState` | [x] |
| 0x02 | ManeuverState | `maneuverState` | [x] |
| 0x03 | CurrentRoadName | `currentRoad` | [x] |
| 0x04 | DestinationName | `destination` | [x] |
| 0x05 | EstimatedTimeOfArrival | `etaSeconds` | [x] |
| 0x06 | TimeRemainingToDestination | `timeRemaining` | [x] |
| 0x07 | DistanceRemaining | `distDestM` | [x] |
| 0x08-0x09 | DistanceRemaining DisplayString / Units | - | [x] (num used) |
| 0x0A | DistanceRemainingToNextManeuver | `distManeuverM` | [x] |
| 0x0B-0x0C | ...ToNextManeuver DisplayString / Units | - | [x] (num used) |
| 0x0D | RouteGuidanceManeuverCurrentList | `maneuverOrder[]` | [x] |
| 0x0E | RouteGuidanceManeuverCount | `maneuverCount` | [x] |
| 0x0F | **RouteGuidanceBeingShownInApp** | `visible_in_app` | [x] -> [[rgd-activation]] |
| 0x10 | LaneGuidanceCurrentIndex | `laneGuidanceIndex` | [x] |
| 0x11 | LaneGuidanceTotalCount | `laneGuidanceTotal` | [x] |
| 0x12 | LaneGuidanceShowing | `laneGuidanceShowing` | [x] |
| 0x13 | SourceName | (from 0x5200) | [x] |
| 0x14 | SourceSupportsRouteGuidance | `sourceSupportsRg` | [x] |
| 0x15 | DestinationTimeZoneOffsetMinutes | - | [ ] |
| 0x16 | StopType | - | [ ] |
| 0x17 | ChargingStationInfoList | - | [ ] |
| 0x18-0x1A | Arrival / Departure / FinalWaypoint BatteryLevel | - | [ ] |

## 0x5202 RouteGuidanceManeuverUpdate - per-maneuver sub-TLVs

| ID | Field | Notes |
|---:|---|---|
| 0x01 | Index | which maneuver slot |
| 0x02 | Description | InstructionText (parsed, not surfaced) |
| 0x03 | Type | EManeuverType 0-53 -> [[maneuver-mapping]] |
| 0x04 | AfterRoadName | turn-to street |
| 0x05-0x07 | DistanceBetween / String / Units | |
| 0x08 | DrivingSide | L/R, mirrors icons |
| 0x09 | JunctionType | roundabout / interchange gate |
| 0x0A | **JunctionElementAngle** | side-street angles |
| 0x0B | **JunctionElementExitAngle** | signed; drives ramp sharpness -> [[maneuver-mapping]] |
| 0x0C | LinkedLaneGuidance | ties maneuver <-> 0x5204 lane event |
| 0x0D | ExitInfo | motorway exit number/name |

## 0x5204 LaneGuidanceInformation

`0x01` LaneGuidanceIndex - `0x02` LaneInformations (per-lane angle vectors) - `0x03` Description.
Detail -> [[bap-fctids]] (FctID 24) and the renderer lane glyphs.

## Gaps - Apple sends, we drop

- **0x15 DestinationTimeZoneOffsetMinutes** - the ETA clock is computed in the vehicle's TZ, ignoring
  the destination TZ Apple provides here. Cross-TZ trips show the arrival clock in the wrong zone.
- **0x16 StopType**, **0x17 ChargingStationInfoList**, **0x18-0x1A BatteryLevel** - EV routing
  metadata, unused.
- **0x5202/0x02 Description (InstructionText)** - parsed but not surfaced on the cluster.
