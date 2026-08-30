---
title: Navigation BAP FctID matrix (LSG 0x32)
tags: [rgd, bap, cluster, verified]
status: verified-source
sources:
  - code: java_patch/com/luka/carplay/rgd/BAPBridge.java
  - code: java_patch/com/luka/carplay/rgd/GatedCombiService.java
  - code: java_patch/de/audi/tghu/navi/app/cluster/ClusterService.java
reconciles:
  - docs/reference/NAVSD_FCTID_MATRIX.md
---

# Navigation BAP FctID matrix (LSG 0x32)

Which Navigation-BAP functions the patch **drives** while CarPlay route guidance is active, and which
stay delegated to the stock navigator. Full stock catalogue: [[navsd-catalogue]].

## Context

> [[rgd-activation]] - decide active -> **bap-fctids** - publish FctIDs -> cluster HUD +
> [[compositing]] - maneuver overlay

## Ownership gate

`GatedCombiService` wraps the stock `CombiBAPServiceNavi`; `ScreenNavStatusGate` installs it and
`BAPBridge` toggles two flags so BAPBridge is the single writer for route-guidance FctIDs:

- `blockRouteGuidance` - drops stock writes to the maneuver FctIDs (17/18/23/24/39/49/55).
- `blockCurrentPositionInfo` - drops stock writes to the lower-bar text FctIDs (19/21/22/46).

```mermaid
flowchart LR
    stock["stock navigator"] --> gate
    bap["BAPBridge (CarPlay RGI)"] --> gate["GatedCombiService<br/>(single writer while gated)"]
    gate -->|"blocked FctID from stock -> dropped"| x["x"]
    gate -->|"BAPBridge writes + everything ungated"| real["real CombiBAPServiceNavi"] --> vc["cluster HUD"]
    note["ScreenNavStatusGate installs the wrapper;<br/>BAPBridge toggles blockRouteGuidance /<br/>blockCurrentPositionInfo"] -.-> gate
```

See [[rgd-activation]] for when these flip. Everything else always delegates to stock.

## CarPlay-owned / gated FctIDs

| FctID | Hex | Name | Role | Notes |
|---:|---:|---|---|---|
| 17 | 0x11 | RG_Status | route-guidance active; starts FctSync | sent during RGI |
| 18 | 0x12 | DistanceToNextManeuver | next-turn distance **+ bargraph** | [[bargraph-sync]] |
| 19 | 0x13 | CurrentPositionInfo | lower-bar road / info line | gated by `blockCurrentPositionInfo`; info-toggle -> ETA, see [[steering-wheel]] |
| 21 | 0x15 | DistanceToDestination | trip distance | gated during RGI |
| 22 | 0x16 | TimeToDestination | ETA / remaining | ETA clock uses vehicle TZ (dest-TZ TLV 0x15 unused, see [[rgd-tlv]]) |
| 23 | 0x17 | ManeuverDescriptor | up to 3 maneuver slots | [[maneuver-mapping]] |
| 24 | 0x18 | LaneGuidance | lane arrows | from 0x5204 |
| 39 | 0x27 | ActiveRGType | guidance presentation type | sends `0` (BAP RGI) |
| 46 | 0x2E | DestinationInfo | destination detail | gated during RGI |
| 49 | 0x31 | Exitview | junction/exit-view + FctSync member | toggled to force sync |
| 55 | 0x37 | ManeuverState | maneuver transition state | sent |

## FctSync (37) is implicit

`FunctionSynchronisation` (FctID 37) atomically syncs FctID 17/18/23/49 - never written directly.
`BAPBridge` toggles the cosmetic Exitview (49) variant to force a transmission when the stock
`sendStatusIfChanged` would otherwise dedup a bargraph tick.

## Scale (45) & altitude (47) pass through

FctID 45 MapScale and 47 Altitude are the native map's lower-bar readouts. The cluster shows the
stock native map, so they stay visible: `GatedCombiService.updateMapScale` / `updateAltitude` always
delegate to stock (never gated on this branch), and `setRouteGuidanceBlocked` does not hide them. The
steering-wheel roller drives stock native-map zoom - see [[steering-wheel]].
