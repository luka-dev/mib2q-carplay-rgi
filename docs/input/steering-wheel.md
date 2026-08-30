---
title: Steering-wheel roller - zoom & route-info toggle
tags: [input, steering-wheel, verified]
status: verified-source
sources:
  - code: java_patch/com/luka/carplay/core/SteeringWheelInputModule.java
  - code: java_patch/de/audi/tghu/navi/app/cluster/ScreenCombiBAPListener.java
  - code: java_patch/de/audi/app/terminalmode/dsi/carplay/CarplayDSILifecycleController.java
  - code: java_patch/com/luka/carplay/core/ScreenModule.java
---

# Steering-wheel roller - zoom & route-info toggle

The left MFW roller has two axes: **rotation** and **press**. On this branch the cluster shows the
stock native map, so rotation is left to stock; only the press is repurposed.

## Context

> MFW roller -> **rotation** = stock native-map zoom - **press** = cluster route-info toggle ->
> [[bap-fctids]] FctID 19 -> [[rgd-activation]].

## Rotation (zoom) -> stock

The roller sends rotation as Navigation-BAP `MapScale.steps`. Since the cluster renders the stock
native map (no CarPlay video plane to zoom), `ScreenCombiBAPListener` adds no override - it is a
`CombiBAPListener` subclass with no behaviour of its own, so the step falls through to stock
`setMapScale`, which zooms the native cluster map exactly as stock does.

## Press (OK) -> route-info toggle

The raw MFW roller press (DSI key 40, `KEY_MFW_ROLLER_LEFT`) and the centre-console DDS (key 16,
`KEY_DDS`) both collapse to the same `DDS_SELECT` in the stock keyboard stack. `SteeringWheelInputModule`
observes the raw `ATTR_KEY2` stream and marks only key 40, so `CarplayDSILifecycleController.updateKey`
can **suppress that one copy** of `DDS_SELECT` before it reaches iOS (via `consumeCollapsedSelect`) -
the centre knob still selects in the CarPlay Main UI.

Gated to the confirmed VC map tab, the press then calls `ScreenModule.onSteeringWheelOkPressed()` ->
`RouteGuidance` toggles the cluster route-info line between the **next turn-to street** (phase 0) and
the **trip summary** (ETA / arrival clock + remaining, phase 1). See [[bap-fctids]] (FctID 19).

```mermaid
flowchart LR
    k40["raw key 40 press"] --> mark["mark collapsed-select<br/>(SteeringWheelInputModule)"]
    mark --> sup["updateKey: suppress the<br/>MFW DDS_SELECT copy"]
    mark --> tog["on map tab -><br/>ScreenModule.onSteeringWheelOkPressed"]
    tog --> rg["RouteGuidance: desiredInfoPhase ^= 1"]
    k16["centre DDS key 16"] --> sel["DDS_SELECT -> CarPlay select<br/>(never marked, passes through)"]
```
