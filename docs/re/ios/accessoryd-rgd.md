---
title: accessoryd - ACCNav route-guidance update enum
tags: [re, ios, rgd, verified]
status: validated
sources:
  - firmware: accessoryd (iPhone18,2, iOS 26.6 / 23G71), CoreAccessories.framework
  - firmware: +[ACCNavigationRouteGuidanceUpdateInfo keyForType:]
---

# accessoryd - ACCNav route-guidance update enum

The phone-side composer that serializes route guidance into the iAP2 `RouteGuidanceUpdate` (0x5201).
This is the authoritative source for the TLV meanings our hook parses ([[rgd-tlv]]).

## The class

`ACCNavigationRouteGuidanceUpdateInfo` (in `accessoryd`) holds an `_infoDict` keyed by an integer
type, serialized to iAP2. `+[... keyForType:]` maps each type to a named key `ACCNav_RGUpdate_*`. The
value is supplied by the CarPlay nav app (Maps or 3rd-party) via `platform_navigation_*`.

## type -> field (verified from `keyForType:`)

| type | key |
|---:|---|
| 0x01 | RouteGuidanceState |
| 0x02 | ManeuverState |
| 0x03 | CurrentRoadName |
| 0x04 | DestinationName |
| 0x05 | EstimatedTimeOfArrival |
| 0x06 | TimeRemainingToDestination |
| 0x07 | DistanceRemaining |
| 0x08-0x09 | DistanceRemaining DisplayString / Units |
| 0x0A | DistanceRemainingToNextManeuver |
| 0x0B-0x0C | ...ToNextManeuver DisplayString / Units |
| 0x0D | RouteGuidanceManeuverCurrentList |
| 0x0E | RouteGuidanceManeuverCount |
| **0x0F** | **RouteGuidanceBeingShownInApp** |
| 0x10 | LaneGuidanceCurrentIndex |
| 0x11 | LaneGuidanceTotalCount |
| 0x12 | LaneGuidanceShowing |
| 0x13 | SourceName |
| 0x14 | SourceSupportsRouteGuidance |
| 0x15 | DestinationTimeZoneOffsetMinutes |
| 0x16 | StopType |
| 0x17 | ChargingStationInfoList |
| 0x18-0x1A | Arrival / Departure / FinalWaypoint BatteryLevel |

## The one that matters most

**0x0F = `RouteGuidanceBeingShownInApp`** - whether the nav app's guidance UI is *on screen*, a
separate field from RouteGuidanceState (0x01) and ManeuverCount (0x0E). It is **not** a route-active
flag; iOS sends it `0` for third-party maps / backgrounded nav even mid-route. This is the firmware
basis for the [[rgd-activation]] fix.

`+[ACCNavigationServer accessoryNavigationStartRouteGuidance:...]` handles start/stop; the value flows
from the platform navigation plugin (app visibility helpers: `isAppVisibleInCurrentMode:`,
`isApplicationInForeground`).
