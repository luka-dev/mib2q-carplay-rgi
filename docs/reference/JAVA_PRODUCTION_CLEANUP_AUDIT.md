# Java production cleanup audit

Date: 2026-08-15  
Scope: current `java_patch/` against stock MU1316 `MU1316-combined-final.jar`

> Update: the route-info finding below was closed after this audit.
> `SteeringWheelInputModule` now binds the raw MFW left-roller press from
> `DSIKeyPanel.ATTR_KEY2` to `InfoModeListener` only while the VC's last
> confirmed main context is `MAIN_CONTEXT_NAVIGATION_MAP` (20), without
> conflating it with the centre-console DDS.  The phase code is therefore live
> and must not be removed.

## Verdict

The tree does not contain an entire disposable Java subsystem or source file. The 28 sources are
either live CarPlay services or full stock class replacements whose apparently unused methods may
be invoked by stock code. Removing a whole replacement class would restore the stock version and
silently remove the intended hook.

There are, however, four concrete cleanup groups:

1. ~~a route-info phase feature which has no input and can never leave phase 0~~ (closed);
2. no-op `ClusterService` pipeline methods which pretend to provide readiness;
3. an over-engineered KOMO reflection path despite public stock APIs;
4. disabled diagnostic calls which still allocate strings on hot BAP paths.

The current tree contains 29 Java sources / 12,617 lines. The built JAR contains 97 class files and
uses Java class version 48. All six replaced stock outer classes retain every stock
public/protected member descriptor.

## Closed: route-info phase now has an authoritative input

Historical finding: `AltScreenModule.InfoModeListener` was described as unbound
(`AltScreenModule.java:63-66`). The only implementation is registered by `RouteGuidance`
(`RouteGuidance.java:69-74,300,322`), but no code invokes `onInfoModeToggle()`. The complete source
search contains only the declaration and that implementation.

Consequently, `desiredInfoPhase` is initialized/reset to zero (`RouteGuidance.java:56-58,330,561`)
and its sole transition to phase 1 is the unreachable `requestInfoModeToggle()`
(`RouteGuidance.java:570-586`). This makes the following phase-1 path dead:

- `BAPBridge.infoPhase` and its phase branches (`BAPBridge.java:82-84,1120-1123,1282-1294,1322-1329`);
- `buildTripSummary`, `appendSummaryPart`, `formatArrivalForText`, and
  `formatRemainingForText` (`BAPBridge.java:1385-1425`);
- `lastDistanceToDestinationM`, which is written and reset but never read
  (`BAPBridge.java:90,638,801,1346`).

Do not apply the removal below: the raw key-40 listener now reaches this path. Preserve the
`ViewAreaModeListener`: its real job is still to replay cached FctID 19/22 data immediately after a
fullscreen/smallscreen switch (`RouteGuidance.java:62-67,554-568,625-629`). Simplify that replay to
one fixed phase-0 presentation.

Do not remove `lastEtaSeconds`, `lastTimeRemainingSeconds`,
`lastTimeRemainingSampleSeconds`, `currentRemainingSeconds()`, or `currentArrivalSeconds()`.
They are still used to publish the real FctID 22 absolute ETA (`BAPBridge.java:1336-1383`).

## Remove now: fake ClusterService pipeline API

The added methods do not control any renderer or stock pipeline:

- `activateCustomRendererPipeline()` returns a constant string
  (`ClusterService.java:1125-1131`);
- `deactivateCustomRendererPipeline()` is empty (`ClusterService.java:1133-1134`).

`BAPBridge` treats the constant string as a readiness decision
(`BAPBridge.java:2025-2033`) and calls the empty method on loss/stop
(`BAPBridge.java:1997-2004,2080-2089`). Actual readiness is already authoritative:
`RendererServer.isReady()/isFrameReady()` and the frame event gate are checked before that call
(`BAPBridge.java:2007-2023`). Remove both `ClusterService` additions and their callers; keep the
real renderer checks.

`refreshInitializingScreenAfterCarPlay()` is another unused added method
(`ClusterService.java:1118-1123`): it has no call site. Current policy intentionally leaves stock
initializing state alone (`AltScreenNavStatusGate.java:1-5`). Remove it and update the stale
documentation which still describes the old recovery seam.

The four other added `ClusterService` accessors are live and must remain:
`getDSIResponseContainer()` and `triggerRefreshRGIValid()` are used by `BAPBridge`
(`BAPBridge.java:551-570`), while the CombiBAP service getter/setter are used by
`AltScreenNavStatusGate` (`AltScreenNavStatusGate.java:42-65,129-149`).

## Simplify next: replace KOMO reflection with the public stock API

`BAPBridge.forceGfxAvailable()` currently spans `BAPBridge.java:2231-2347` and tries three
reflection strategies. Two assumptions are contradicted by authoritative MU1316 stock source:

- `KOMOService` has no `dataRate` field; the search at `BAPBridge.java:2255-2271` cannot succeed.
- `ClusterService.setKOMODataRate()` is public but is a no-op unless
  `Util.isClusterMapMOST()` is true (`stock ClusterService.java:687-691`). Calling it reflectively
  does not bypass that guard.

The required state is already exposed directly:

- `ClusterService.getClusterViewMode()` is public (`stock ClusterService.java:202-204`);
- `ClusterViewMode.setDataRate(int)` is public (`stock ClusterViewMode.java:119-125`);
- `ClusterViewMode.setGFXAvailable(boolean)` is public (`stock ClusterViewMode.java:87-98`);
- `KOMOService.updateDataRate/updateGfxState` are also public and only delegate to those same
  methods (`stock KOMOService.java:177-180,193-198`).

Replace the reflection ladder with direct calls to the `ClusterViewMode` returned by
`csRef.getClusterViewMode()`, setting data rate before gfx availability. Then remove the
`KOMOService` field, its acquisition/logging, and `java.lang.reflect.Field/Method` imports.
This is a semantic simplification, so validate map/popup start, renderer respawn, and RGI stop on
the unit before release.

Do not remove the reflection in `CoverArtProviderMux`: that code accesses a stock-private provider
registry for which no public setter exists (`CoverArtProviderMux.java:27-51`).

## Remove now: disabled diagnostics and small dead APIs

> Done for the two logging items: `traceBap`/`traceDescriptor` and every standalone `Log.d` call
> site are gone, and both the Java logger and the C hook now default to WARN. See
> `docs/reference/PRODUCTION_LOGGING.md`. `Log.i` was deliberately kept.

`BAP_TRACE_ENABLED` is a compile-time false constant (`BAPBridge.java:39`). `javac` compiles
`traceBap()` and `traceDescriptor()` to immediate returns, but it does not remove argument
construction at their 25 + 1 call sites. For example the built bytecode for
`sendDistanceToManeuverRaw` allocates a `StringBuffer` before invoking the empty method. Remove the
trace call sites and both trace methods from the production build.

The same problem exists for ordinary debug logging. `Log.level` starts at INFO
(`Log.java:14-16`), and `Log.setLevel()` has no call site (`Log.java:34`). Therefore all 41
`Log.d()` calls are currently output-dead, but their concatenated messages are constructed before
`Log.out()` rejects them (`Log.java:39,45-47`). The hottest examples are the per-update and
per-lane calls in `BAPBridge.java:908,1257,1559`. Either remove production `Log.d()` calls or put
them behind a compile-time diagnostic build flag at each call site. Merely keeping the level check
inside `Log.d()` does not remove allocation cost.

Other high-confidence dead members:

- `CarPlayApp.fw()` (`CarPlayApp.java:135`);
- `FrameworkRef.context()`, `deviceManager()`, and `hmiServiceApp()`
  (`FrameworkRef.java:34,37-38`) and their now-unused imports;
- `CarplayBus.isConnected()`, `Data.bool()`, and `Data.strList()`
  (`CarplayBus.java:122-124,259-266,283-295`);
- `RendererServer.isConnected()` (`RendererServer.java:428-432`);
- `BAPBridge.isActionBlinkThreadRunning()` (`BAPBridge.java:201-203`);
- `AltScreenModule.isClusterActive()` (`AltScreenModule.java:77`);
- unused `EXITVIEW_ROW/EXITVIEW_ASIA` constants (`BAPBridge.java:41-49`);
- unused route-state alias/comment `ROUTE_STATE_ACCEPT_ALL_MANEUVER_IDX`
  (`RouteGuidance.java:30-37`).

`CarplayBus.stop()` has no caller because the transport is intentionally process-lifetime
(`CarPlayApp.java:61-75`). It can be removed, but retaining a bounded shutdown API is harmless and
useful for diagnostics; it is not worth touching in the first cleanup pass.

## Keep

Do not collapse or delete the following just because the implementations are large:

- the six stock replacements: their public/protected ABI is complete and stock code calls into
  them outside local source reachability;
- `CarplayBus` (`:19810`) and `RendererServer` (`:19800`): they serve different peers and protocols;
- `AltscreenControlServer` (`:19832`): it supplies generation-bound VIDEO_ON/OFF and layout/style
  state to/from `altscreen_render` (`AltscreenControlServer.java:1-13,182-223`);
- style detection in `AltScreenModule`: maneuver geometry now comes from live stock Layout, but the
  altScreen loader still needs classic/sport style (`AltScreenModule.java:213-249`);
- `RgdModule`, `FrameworkRef.ServiceHandle`, and `Module`: they preserve ordered module lifecycle
  and paired OSGi release (`CarPlayApp.java:305-357`, `FrameworkRef.java:40-75`);
- `AltScreenCombiBAPListener`, `CursorController`, all mapper classes, and lane fallback logic;
- the cover-art replacement/provider/mux chain. It is live and intentionally preserves the stock
  provider rather than replacing it.

The raw stock skin byte carried on the `:19832` layout packet is diagnostic only
(`AltscreenControlServer.java:43-49`; `altscreen_render/sink.h:29-30`). It could be removed, but it
touches a working cross-process protocol for a one-byte saving. Keep it until that protocol next
changes for a functional reason.

## Recommended patch order

1. Keep and validate the now-live route-info phase through raw MFW key 40.
2. Remove fake `ClusterService` pipeline/initializing methods and callers.
3. Remove disabled BAP/debug trace allocations.
4. Replace KOMO reflection with direct stock API and validate it on-unit.
5. Rebuild, rerun the six-class ABI comparison, and test cold boot, RGI start/stop, View changes,
   renderer death/reconnect, stock navigation after disconnect, and cover art.

Expected reduction is roughly 250-350 source lines, mostly in hot `BAPBridge`/presentation code,
without deleting any functional subsystem.

`docs/archive/JAVA_PATCH_STOCK_AUDIT.md` is archived (historical): its headline counts (80 class
files / 11,081 lines) and several dead-code findings describe an older tree and are no longer
authoritative for production cleanup.
