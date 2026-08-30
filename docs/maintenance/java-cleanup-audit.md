---
title: Java production cleanup audit - reconciled to current branch
tags: [maintenance, java, verified]
status: verified-source
sources:
  - code: java_patch/com/luka/carplay/rgd/BAPBridge.java
  - code: java_patch/com/luka/carplay/rgd/RouteGuidance.java
  - code: java_patch/com/luka/carplay/core/ScreenModule.java
  - code: java_patch/com/luka/carplay/core/SteeringWheelInputModule.java
  - code: java_patch/com/luka/carplay/rgd/RendererServer.java
  - code: java_patch/com/luka/carplay/core/FrameworkRef.java
reconciles:
  - docs/reference/JAVA_PRODUCTION_CLEANUP_AUDIT.md
---

# Java production cleanup audit - reconciled to current branch

Migrated from `docs/reference/JAVA_PRODUCTION_CLEANUP_AUDIT.md` (original scope: `java_patch/`
against stock MU1316) and **re-verified against the current branch**. Each item below carries its
**real current status**, not the audit's original status - several cleanups have since landed.

## Context

> [[architecture]] - Java patch layer -> cluster HUD + route-info - KOMO gfx gate -> [[komo-widget-video]]
> - route-info toggle publishes [[bap-fctids]] FctID 22

**Bottom line:** three of the four cleanup groups are complete. The **KOMO reflection ladder**
(`forceGfxAvailable`) is the one substantive item still open, plus two small dead accessors that
survived the dead-member sweep.

## [x] route-info phase - live, keep (was: closed)

The historical "phase can never leave 0, remove it" finding is **resolved and the path is live** -
do not remove it. It is now driven by the steering-wheel roller press:

`SteeringWheelInputModule` (`SteeringWheelInputModule.java:223`) ->
`ScreenModule.onSteeringWheelOkPressed()` (`ScreenModule.java:225-229`) ->
`RouteGuidance`'s `InfoModeListener.onInfoModeToggle()` (`RouteGuidance.java:68-74`) ->
`requestInfoModeToggle()` (`RouteGuidance.java:577-587`, `desiredInfoPhase ^= 1`) ->
`BAPBridge.infoPhase` and the trip-summary FctID 22 path.

`ScreenModule.InfoModeListener` is a real registered interface (`ScreenModule.java:210-229`), bound
by `RouteGuidance` in start (`RouteGuidance.java:299,321`) and cleared on stop
(`RouteGuidance.java:384`). `BAPBridge.infoPhase` / `buildTripSummary` / `lastDistanceToDestinationM`
are therefore reachable and must stay. See [[steering-wheel]].

## [x] fake ClusterService pipeline API - removed (was: to remove)

Confirmed gone. `activateCustomRendererPipeline()`, `deactivateCustomRendererPipeline()` and
`refreshInitializingScreenAfterCarPlay()` no longer exist anywhere in `java_patch/`, and the
`BAPBridge` branch that read the constant "readiness" string is gone too. Real readiness
(`RendererServer.isReady()/isFrameReady()` + frame-event gate) remains authoritative. The four live
`ClusterService` accessors (`getDSIResponseContainer()`, `triggerRefreshRGIValid()`, and the CombiBAP
getter/setter) are retained as intended.

## (!) KOMO reflection ladder (`forceGfxAvailable`) - STILL OPEN (key remaining item)

**Current code:** the three-strategy reflection ladder is intact at
`BAPBridge.java:2175-2271`, with `import java.lang.reflect.Field/Method` (`:29-30`) and the
`private KOMOService komoService` field (`:139`). What it does today:

1. **Pre-step** - reflectively locates and writes `KOMOService.dataRate` (walks the superclass chain
   for a `dataRate` field, `:2180-2196`).
2. **Strategy 1** - reflective `komoService.updateGfxState(gfx, 1)` (`:2198-2208`).
3. **Belt-and-suspenders** - reflective `csRef.setKOMODataRate(desiredRate)` (`:2210-2221`).
4. **Strategy 2/3** - pull `clusterViewMode` off `ClusterService` by reflection, then reflective
   `setGFXAvailable(boolean)` with a `gfxAvailable` field write as last resort (`:2223-2270`).

**Why the audit flagged it (assumptions the stock source contradicts):**

- `KOMOService` has **no `dataRate` field** - the pre-step field search at `:2180-2196` cannot
  succeed on stock.
- `ClusterService.setKOMODataRate()` is public but is a **no-op unless `Util.isClusterMapMOST()`**
  (SysConst 541==1; FPK cars fail the guard). Calling it reflectively does not bypass that gate.

**Proposed public-API replacement.** The required state is exposed directly, no reflection needed:

- `ClusterService.getClusterViewMode()` is public;
- `ClusterViewMode.setDataRate(int)` is public - **call this first**;
- `ClusterViewMode.setGFXAvailable(boolean)` is public - **call this second**;
- (`KOMOService.updateDataRate` / `updateGfxState` are public too and merely delegate to those same
  two methods, so either entry point works.)

Replace the ladder with direct calls on `csRef.getClusterViewMode()`, setting **data rate before gfx
availability**, gated on `Util.isClusterMapMOST()` (the honest gate - the MOST pacing hint only
matters where that guard is true). Then drop the `komoService` field, its acquisition/logging, and
the `java.lang.reflect.Field/Method` imports.

This is a **semantic change**, not a mechanical edit: validate map/popup start, renderer respawn, and
RGI stop on the unit before release. Do **not** touch the reflection in `CoverArtProviderMux` - that
reaches a stock-private provider registry with no public setter.

## (!) disabled diagnostics & dead members - PARTIALLY DONE

**Done (verified gone):**

- `traceBap()` / `traceDescriptor()` / `BAP_TRACE_ENABLED` and every trace call site - removed.
- All `Log.d()` call sites - **0 remaining** in `java_patch/`. Logger and C hook default to WARN
  (see `docs/reference/PRODUCTION_LOGGING.md`). `Log.i` deliberately kept.
- `CarPlayApp.fw()`, `CarplayBus.isConnected()` / `Data.bool()` / `Data.strList()`,
  `BAPBridge.isActionBlinkThreadRunning()`, `EXITVIEW_ROW/EXITVIEW_ASIA`,
  `AltScreenModule.isClusterActive()`, `ROUTE_STATE_ACCEPT_ALL_MANEUVER_IDX` - all removed.

**Still open (two dead accessors survived the sweep, no callers found):**

- `RendererServer.isConnected()` (`RendererServer.java:426`) - no call site.
- `FrameworkRef.context()` / `deviceManager()` / `hmiServiceApp()` (`FrameworkRef.java:34,37-38`) -
  no call sites; drop these and their now-unused imports.

(Note: `ScreenModule.isConnected()` is a **different, live** method - it pins the cluster while
CarPlay owns it, called from `CombiMapController` and `ClusterService`. Keep it.)

## Keep (unchanged - do not delete for size)

- The six stock replacement classes: their public/protected ABI is complete and stock code calls
  into them outside local reachability.
- `CarplayBus` (:19810) and `RendererServer` (:19800) - different peers/protocols.
- `RgdModule`, `FrameworkRef.ServiceHandle`, `Module` - ordered lifecycle + paired OSGi release.
- `CursorController`, mapper classes, lane fallback.
- The cover-art replacement/provider/mux chain (live; intentionally preserves the stock provider).
- Trip-summary ETA helpers (`lastEtaSeconds`, `lastTimeRemainingSeconds`,
  `lastTimeRemainingSampleSeconds`, `currentRemainingSeconds()`, `currentArrivalSeconds()`) - they
  publish the real FctID 22 absolute ETA.

## Remaining patch order

The route-info, fake-pipeline, and trace/debug items are done. What is left:

1. Replace the KOMO reflection ladder with the direct `ClusterViewMode` API, gated on
   `Util.isClusterMapMOST()`, and validate on-unit.
2. Remove the two dead accessors (`RendererServer.isConnected()`,
   `FrameworkRef.context/deviceManager/hmiServiceApp` + unused imports).
3. Rebuild, rerun the six-class ABI comparison, and test cold boot, RGI start/stop, View changes,
   renderer death/reconnect, stock navigation after disconnect, and cover art.
