---
title: RGD activation & the visible_in_app authority
tags: [rgd, activation, ios-re, verified]
status: verified-decompile
sources:
  - code: java_patch/com/luka/carplay/rgd/RouteGuidance.java
  - code: hook/routeguidance/rgd_hook.c
  - code: hook/routeguidance/rgd_tlv.h
  - firmware: accessoryd 23G71 +[ACCNavigationRouteGuidanceUpdateInfo keyForType:]
reconciles:
  - docs/reference/NAVSD_FCTID_MATRIX.md
  - docs/reference/IOS266_MANEUVER_DECOMPILE.md
  - docs/archive/MIB3.md
---

# RGD activation & the `visible_in_app` authority

How the patch decides that CarPlay route guidance is **active** (cluster shows the maneuver overlay)
versus **inactive** (return to the stock cluster).

## Context

Where this sits in the route-guidance path - **you are here** decides *active/inactive*; everything
upstream just delivers state, everything downstream renders it:

> [[rgd-tlv]] - parse TLVs -> [[bus-protocol]] - EVT_RGD_UPDATE -> **rgd-activation** - decide active ->
> [[bap-fctids]] - HUD + [[compositing]] - cluster overlay

```mermaid
flowchart LR
    ios["iOS RGD<br/>0x5200-0x5204"] --> tlv["hook: parse TLVs<br/>rgd-tlv"]
    tlv --> bus["bus: EVT_RGD_UPDATE<br/>bus-protocol"]
    bus --> act["decide wantActive<br/>+ 2-stage promote"]:::here
    act --> bap["BAP FctIDs (HUD)<br/>bap-fctids"]
    act --> comp["cluster ctx 74<->80<br/>compositing"]
    classDef here fill:#fde68a,stroke:#b45309,color:#000;
```

## The decision

`RouteGuidance` recomputes `wantActive` on every activation-relevant delta from three iAP2 inputs:

| Input | iAP2 source | Meaning |
|---|---|---|
| `routeState` | RouteGuidanceState (TLV 0x01) | 0=NO_ROUTE ... 1=ROUTE_SET ... 5=REROUTING |
| `maneuverCount` / maneuver list | ManeuverCount (0x0E) / CurrentList (0x0D) | how many maneuvers are queued |
| `visible_in_app` | **RouteGuidanceBeingShownInApp** (0x0F) | is the nav app's guidance UI on screen |

```mermaid
flowchart TD
    A[activation delta] --> B{"visible_in_app known? (0/1)"}
    B -- "yes" --> C{"visible_in_app == 1?"}
    C -- "yes" --> W[wantActive = true]
    C -- "no" --> R{"route still looks active?<br/>routeState>=ROUTE_SET OR maneuvers>0"}
    R -- "yes" --> W
    R -- "no" --> X[wantActive = false]
    B -- "no (-1)" --> R2{"route looks active?"}
    R2 -- "yes" --> W
    R2 -- "no" --> X
    W --> G{"sourceSupportsRg==0<br/>or routeState==NO_ROUTE?"}
    X --> G
    G -- "yes" --> X2[force wantActive = false]
    G -- "no" --> K[keep wantActive]
```

## `visible_in_app` is a visibility flag, not a route-active flag

`visible_in_app` is iAP2 RouteGuidanceUpdate **TLV 0x0F** = Apple's
`ACCNav_RGUpdate_RouteGuidanceBeingShownInApp` (accessoryd 23G71,
`+[ACCNavigationRouteGuidanceUpdateInfo keyForType:]`, case 0xF). It reports whether the nav app's
guidance UI is currently **on screen** - a separate field from `RouteGuidanceState` (0x01),
`ManeuverCount` (0x0E) and `SourceSupportsRouteGuidance` (0x14). iOS sends it `0` for third-party maps
and whenever the nav app is not the foreground CarPlay app, **even mid-route**.

Therefore `visible_in_app==0` must **not** deactivate while the route still looks active; genuine
end-of-route is caught by the `routeState==NO_ROUTE_SET` hard override. See [[rgd-tlv]] for the full
TLV map and [[accessoryd-rgd]] for the enum evidence.

## Two-stage activation (why the cluster doesn't flicker)

`wantActive` rising does **not** immediately switch the cluster to the maneuver context:

```mermaid
stateDiagram-v2
    direction LR
    [*] --> Idle: stock ctx 74
    Idle --> BAPOwned: wantActive -> bap.onStart()
    BAPOwned --> Presenting: renderer FRAME_READY
    Presenting --> BAPOwned: presentation lost (retry)
    BAPOwned --> Idle: wantActive false
    Presenting --> Idle: wantActive false / route end
    note right of BAPOwned
        BAP published, cluster still ctx 74
        (setNavActive false)
    end note
    note right of Presenting
        setNavActive(true) -> ctx 80
        (maneuver over stock map)
    end note
```

- **Stage 1** - own BAP and publish the start sync, but keep stock ctx 74 (`setNavActive(false)`).
- **Stage 2** - only after `maneuver_render` confirms a rendered frame does `setNavActive(true)`
  promote the cluster to ctx 80 -> see [[compositing]] / [[display-contexts]].

## Transient `route_state=0` is debounced in the C hook

`hook/routeguidance/rgd_hook.c` holds a deferred flush for `route_state=0` so a momentary reset /
reroute never reaches Java as a deactivation - any deactivation Java sees is genuine
(`source_supports_rg=0`, `visible_in_app=0` with no route, or a real route end).

## Open / to-verify

- (!) `routeState==5` (REROUTING) "accept all maneuvers" path is documented for MHI3 but not
  implemented here - decide if needed. See [[maps-maneuvers]].
