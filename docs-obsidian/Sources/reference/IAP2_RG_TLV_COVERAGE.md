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
| 0x13 | **SourceName** | yes, ON THE BUS | **Java drops it** | which nav app. Hook parses AND writes `source_name` to the bus (rgd_hook.c:1030); Java never reads it. Cheapest win: Java field + surface only, no hook change. |
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

## Already parsed at the hook but dropped downstream (cheapest wins)

Two useful fields are fully parsed by the C hook and then lost before the cluster:

- **SourceName (0x13)** — parsed into `out->source_name`, written to the bus as key `source_name`
  (`rgd_hook.c:1030`). Java has no `sourceName` in `RouteGuidance.State` and never reads the key.
  Surfacing it is Java-only work.
- **Lane InstructionText (0x5204 / 0x03, `lane_guidance_description`)** — parsed and copied onto each
  per-lane `description` (`rgd_hook.c:908`). Not surfaced on the cluster. A ready phrase like
  "Use the 2 left lanes".

Lane ARROW geometry is NOT missed: per-lane positions/directions(angles)/status are parsed and reach
Java as `mLanePositions[][]` / `mLaneDirections[][]` / `mLaneStatus[][]`. ManeuverUpdate (0x5202) is
100% covered. So the core turn+lane path is complete; the gaps are metadata (source, EV, TZ, signs).

## Other ACCNavigation* classes (unverified)

`ACCNavigationAccessoryInfo`, `ACCNavigationProviderInfo`, `ACCNavigationUpdateInfo` appear in
accessoryd but were not decompiled here; they look like iOS-internal plumbing rather than new wire
TLV sets. Not confirmed either way.

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

---

# Maneuver coverage — do we handle every type? (2026-08-28)

Two layers checked.

## ManeuverUpdate TLV fields (0x5202 / 0x01–0x0D)
All parsed — confirmed 1:1 against accessoryd 23G71 `+[ACCNavigationManeuverUpdateInfo keyForType:]`
(Index, ManeuverDescription, ManeuverType, AfterManeuverRoadName, DistanceBetweenManeuver
{,DisplayString,DisplayUnits}, DrivingSide, JunctionType, JunctionElementAngle,
JunctionElementExitAngle, LinkedLaneGuidanceInfo, ExitInfo). Nothing missing.

## ManeuverType coverage (all 54 values 0–53)
`convertManeuverTypeForFunc` is a bounds-checked passthrough (valid 0..53, >=54 -> 0xFF), so the wire
values equal our `MT_*` enum. Every value maps in `ManeuverMapper.map()`:

| type | → main / dir |
|---|---|
| 0 NO_TURN, 5 FOLLOW_ROAD | FOLLOW_STREET |
| 1 LEFT_TURN, 2 RIGHT_TURN, 3 STRAIGHT | TURN + L/R/straight |
| 4 U_TURN, 18 START_ROUTE_WITH_U_TURN, 26 U_TURN_WHEN_POSSIBLE | UTURN |
| 6 ENTER_ROUNDABOUT | TURN + L/R (simplified, not a roundabout glyph) |
| 7 EXIT_ROUNDABOUT | EXIT_ROUNDABOUT_TRS (junctionType==1 only) |
| 8 OFF_RAMP | EXIT_L/R + angle-derived dir |
| 9 ON_RAMP | TURN + slight |
| 10,12,27 ARRIVE_* | ARRIVED |
| 11 START_ROUTE, 16 EXIT_FERRY, 51 CHANGE_HIGHWAY | TURN + angle |
| 13 KEEP_LEFT, 49 SLIGHT_LEFT | TURN + SLIGHT_LEFT |
| 14 KEEP_RIGHT, 50 SLIGHT_RIGHT | TURN + SLIGHT_RIGHT |
| 15 ENTER_FERRY, 17 CHANGE_FERRY | FOLLOW_STREET |
| 19 U_TURN_AT_ROUNDABOUT | ROUNDABOUT_TRS + UTURN |
| 20 LEFT_TURN_AT_END, 21 RIGHT_TURN_AT_END | TURN + end-of-road angle |
| 22 HWY_OFF_RAMP_LEFT, 23 HWY_OFF_RAMP_RIGHT | EXIT_L/R + angle-derived dir |
| 24,25 ARRIVE_DESTINATION_L/R | ARRIVED + L/R |
| 28–46 ROUNDABOUT_EXIT_1..19 | ROUNDABOUT_TRS + angle16 (junctionType==1 only) |
| 47 SHARP_LEFT, 48 SHARP_RIGHT | TURN + SHARP |
| 52 CHANGE_HIGHWAY_LEFT, 53 CHANGE_HIGHWAY_RIGHT | CHANGE_LANE + L/R |
| 255 NOT_SET | NO_SYMBOL |

**No type is unconditionally dropped.** The only conditional NO_INFO is the roundabout family
(7, 28–46) when `junctionType != 1`. That gate is sound: `convertJunctionTypeForFunc` proves iOS
emits junctionType ∈ {0,1} only (0=intersection, 1=roundabout), exactly our constants — so a
roundabout maneuver always arrives with junctionType==1 and is handled.

**Deliberate simplifications (not gaps):** ENTER_ROUNDABOUT → plain TURN (no dedicated approach
glyph); CHANGE_HIGHWAY_L/R → CHANGE_LANE; ferries → FOLLOW_STREET/TURN. All render *something*
sensible.

**List depth:** the full per-maneuver arrays (type/angles/name/lane) are parsed for up to
MANEUVER_CACHE_SIZE=32 maneuvers; BAP/HUD renders only the current one (pos 0) by design.

## Angle sentinels — three of them, and they are not interchangeable

**2026-09-11 correction:** see [verification against iOS 23B85/24A5418b](IOS_MANEUVER_ANGLE_SENTINELS.md).
The hook synthesizes **+1000**, not both signs. `-1000` remains a defensive mapper
case. iOS 27's serializer omits a missing value and otherwise preserves signed16;
`-1` can also be a real wire angle. Keep the legacy unset-slot safeguard, but do
not infer absence from the protocol number alone. Current icon choices supersede
the historical table above: [54-type selection](CARPLAY_HUD_ICON_SELECTION.md).

Fixed 2026-08-30 (ported from `Patches/mib2q-carplay-rgi-old`, which had it and this tree did not).

An angle reaching `ManeuverMapper` can be absent for three different reasons, each with its own
value, and code that only knows some of them misrenders:

| value | origin | meaning |
|---|---|---|
| `0` | wire | iOS sent an angle of zero, i.e. straight |
| `+1000` / defensive `-1000` | receiver pipeline | `rgd_tlv.c` supplies +1000 for an omitted field; mapper also guards -1000 |
| `-1` | Java | the `RouteGuidance.State` slot was never filled |

The `-1` case is easy to miss because it also represents a Java slot default. `RouteGuidance.State` resets
`mTurnAngle[i] = -1` (`RouteGuidance.java:224`) and parses `turn_angle` with `-1` as the
missing-field default (`:998`); note `mExitAngle[i]` resets to `1000` instead, so the two fields do
**not** share a default.

`ManeuverMapper` previously tested only `0` and `±1000`, so an unfilled slot took the signed path:

```java
directionFromTurnAngle(-1)  ->  turnAngle < 0  ->  DIR_LEFT
```

— a maneuver with no angle data rendered as a **left turn**. The same gap made
`hasDirectionalTurnAngle(-1)` report that angle data existed, and made `rampDirection()` compute
`dirFromEndOfRoadAngleLeft(-1)` instead of falling back to SLIGHT_LEFT/RIGHT. All three sites now
include `-1`; `tests/RendererMapperDirectionTest` covers the fallbacks (it is in-tree but wired into
no build script — run it by hand).

**Rule for new code:** treat `0`, `1000`, `-1000` and `-1` as "no directional angle" everywhere, and
prefer the shared `hasDirectionalTurnAngle()` predicate over open-coding the comparison.
