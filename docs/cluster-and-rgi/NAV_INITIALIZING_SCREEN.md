# Navigation initializing screen

The current build leaves stock NAVSD initialization unchanged. It does not replace
`InitializationManagerNavi`, clamp the outgoing operation state, or defer stock
`showInitializingScreen()` / `hideInitializingScreen()` calls. The CarPlay route
guidance gate only controls route data; it does not make native Navigation ready.

## What the cluster waits for

The Virtual Cockpit's navigation-not-available screen can cover the CarPlay map
while stock Navigation starts. NAVSD has two independent startup requirements:

- Java Navigation must register its app-service listener. Until then the NAVSD
  BAP module stays off and cannot complete its initialization sequence.
- Native Navigation must report readiness before the operation state becomes
  normal. A CarPlay renderer presenting a frame does not establish that readiness.

The text is selected by NAVSD FSG_OperationState (FctID 15), rather than the
separate InfoStates hint field (FctID 38). See [stock Navigation readiness](STOCK_NAV_READINESS.md)
and the [NAVSD field matrix](NAVSD_FCTID_MATRIX.md).

## Current implementation

[AltScreenModule](../../java_patch/com/luka/carplay/core/AltScreenModule.java)
uses the renderer's presentation state for CarPlay display ownership.
[AltScreenNavStatusGate](../../java_patch/com/luka/carplay/core/AltScreenNavStatusGate.java)
installs the route-data wrapper, and
[GatedCombiService](../../java_patch/com/luka/carplay/rgd/GatedCombiService.java)
passes stock initialization requests through.

The `ClusterService` CarPlay RGI flag keeps route-guidance validity separate
from the stock route state. It is not an initialization override.

## Historical investigation

Earlier show/hide deferral and operation-state clamps did not clear the screen
in the owner's cold-boot test. They were removed from the published build and
are not restored during consolidation. The full binding analysis and attempted
patches remain in the
[October 19 investigation](https://github.com/luka-dev/mib2q-carplay-rgi/blob/a63d54741839c7d30d32dd4e149f693856bcb0db/docs/cluster-and-rgi/NAV_INITIALIZING_SCREEN.md).

Host tests cover renderer startup, context switching and route-data ownership.
They do not prove that a future change shortens native Navigation startup on a
physical head unit.
