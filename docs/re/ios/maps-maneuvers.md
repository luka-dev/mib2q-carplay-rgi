---
title: Maps - accNav maneuver enum & signed exit angle
tags: [re, ios, maneuver, verified]
status: verified-decompile
sources:
  - firmware: Maps (iPhone18,2, 23G71) executable - /private/var/staged_system_apps/Maps.app/Maps, extracted from the 26.6 rootfs dmg
  - firmware: Maps - +[CarClusterUpdateManeuverInfo _enumProperties] / _accNavManeuverTypeForGEOManeuverType: / maneuverUpdateWith* / +[ACCNavigationInfoBuilder accNavigationManeuverUpdateInfoFrom:]
  - firmware: CarPlay.framework - +[CPManeuver _descriptionForTrafficSide:]
reconciles:
  - docs/reference/IOS266_MANEUVER_DECOMPILE.md
---

# Maps - accNav maneuver enum & signed exit angle

The Apple-side source of the maneuver values our hook consumes. Backs [[maneuver-mapping]].

## Context

> Maps accNav -> iAP2 0x5202 type/angle -> [[rgd-tlv]] -> [[maneuver-mapping]] -> BAP.

## U-turn family

`+[CarClusterUpdateManeuverInfo _enumProperties]` initializes the accNav name dictionary. Verified
values:

| accNav | name |
|---:|---|
| 4 | U_TURN |
| 18 | START_ROUTE_WITH_U_TURN |
| 19 | U_TURN_AT_ROUNDABOUT (stays a roundabout, not a U-turn) |
| 26 | U_TURN_WHEN_POSSIBLE |

The `maneuverType` map is keyed by `__NSConstantIntegerNumber` constants, with a `NotSet` sentinel at
`-1` (0xFFFF) in slot 0 and the real values starting at 0 (NO_TURN) - so the integers above are the
plain GEO direction-enum numbers, not shifted.

`_accNavManeuverTypeForGEOManeuverType:` maps GEO 1-88 to accNav via a 16-bit table. Notable: GEO 25->18,
35->26, **86/88->4** (collapsed). So types 4/18/26 need a **signed** direction; 86/88 forward their angle.

## Signed `JunctionElementExitAngle`

Both builders (`maneuverUpdateWithStep:` and `maneuverUpdateWithGuidanceEvent:`) select the role-2
element, and for GEO type 4 normalize the sign by traffic side, ending in an `FNEG`, then
`setJunctionElementExitAngle:`:

| drivingSide | `_descriptionForTrafficSide:` | ordinary U-turn |
|---:|---|---|
| 0 | right | negative -> left |
| 1 | left | positive -> right |

Exact predicate in both builders: `(drivingSide == 0 && angle > 0) || (drivingSide == 1 && angle < 0)`
-> `+[NSNumber numberWithDouble:-angle]` (the FNEG) -> `setJunctionElementExitAngle:`. Junction elements
with role 1 are skipped; role != 1,2 are pushed to the `junctionElementAngle` array.

- `+[ACCNavigationInfoBuilder accNavigationManeuverUpdateInfoFrom:]` calls `setInfo:data:` directly -
  **no later clamp / abs / re-sign**. It just enumerates `accNavFormat` and forwards each
  `(key.unsignedIntValue as uint16, value)` pair. The signed value ships intact.
- HU side: `rgd_tlv.c` reads TLV 0x0B as signed BE16 and publishes it as both `turn_angle` and
  `exit_angle`, so the sign reaches `ManeuverMapper` and the renderer intact.

## Sentinel

Apple Maps 26.6 **does not** synthesize angle 1000. The `0/+/-1000 = absent` sentinel is our own
convention ([[maneuver-mapping]]); reading the sign is Apple's primary rule, with `drivingSide` the
fallback when no role-2 angle is present.

## Decompile anchors (iOS 26.6, 23G71)

Maps executable, imagebase `0x100000000`:

- `+[CarClusterUpdate _accNavManeuverTypeForGEOManeuverType:]` @ `0x100e0b8c8` - `return (a3-1u) > 0x57 ? 0 : word_101388934[a3-1]`. Table @ `0x101388934` (88 x u16): idx24=18, idx34=26, idx85=4, idx87=4; low range is identity (GEO4->4 = U_TURN).
- `+[CarClusterUpdateManeuverInfo _enumProperties]` dict built in the dispatch_once block @ `0x1010b3424` (cached in `qword_101AF2688`); `maneuverType` sub-dict = 55 entries, NotSet=-1.
- `+[CarClusterUpdateManeuverInfo maneuverUpdateWithStep:component:]` - FNEG @ `0x1010b416c`, `setJunctionElementExitAngle:` @ `0x1010b4180`.
- `+[CarClusterUpdateManeuverInfo maneuverUpdateWithGuidanceEvent:routeStep:component:]` - FNEG @ `0x1010b4508`, `setJunctionElementExitAngle:` @ `0x1010b451c`.
- `+[ACCNavigationInfoBuilder accNavigationManeuverUpdateInfoFrom:]` @ `0x100da8814`; forwarder block `sub_100DA88C8` -> `setInfo:data:` only, no arithmetic on the angle.
- `+[CPManeuver _descriptionForTrafficSide:]` - CarPlay.framework (public), @ `0x23d91c7e8`: `0 -> "right"`, `1 -> "left"`, else nil.
