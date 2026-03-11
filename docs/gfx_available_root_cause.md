# gfxAvailable Root Cause Analysis (MU1316)

## The Blocker

`ClusterViewMode.gfxAvailable` stays **false** throughout all KOMO pipeline tests. This prevents the VC from transitioning to LVDS map view (`SV_LVDS_NavMap_FPK`), because:
- `gfxAvailable=true` triggers EB GUIDE ViewModeSM -> MAP view activation -> BAP rgType=4 -> VC sets `INTERN_Active_NavFPK_Content=Map(1)`
- Combined with `LVDS_Available=1` (from MOST video sync) -> `SV_LVDS_NavMap_FPK`

## Callback Chain (How gfxAvailable Gets Set)

```
videoencoderservice (native C++)
  -> DSIKOMOGfxStreamSink Stub (DSI server)
    -> ATTR_GFXSTATE notification (DSI attribute change)
      -> KOMOCaller (Java) subscribes via setNotification on ATTR_GFXSTATE
        -> KOMOService.updateGfxState(i, j)
          -> if j == 1: ClusterViewMode.setGFXAvailable(i == 1)
```

### Source Code References

**KOMOService.java:177-182** (decompiled from MU1316 lsd.jar):
```java
public void updateGfxState(int i, int j) {
    if (j == 1) {
        clusterViewMode.setGFXAvailable(i == 1);
    }
}
```

**KOMOCaller.java:144-157** (decompiled):
- Subscribes to `ATTR_GFXSTATE` via `setNotification` on DSIKOMOGfxStreamSink proxy
- Proxy UUID (client): `e431fc99-c578-5fa8-b280-ce902809bd5d`
- Stub UUID (server): `7f54a915-2fc6-5ec3-81a7-a15e19315a24`

**AbstractNavigationActivator.java** (decompiled):
- `addDSIKOMOGfxStreamSink` wires the DSI proxy to KOMOService callback

## Root Cause: No Native Provider

**DSIKOMOGfxStreamSink has NO native provider on MU1316.**

Evidence:
- `libdsikomogfxstreamsinkproxy.so` exists in `/mnt/app/eso/lib/factories/` -- contains BOTH Stub (server) and Proxy (client) code
- BUT no native process loads or instantiates the Stub side
- `videoencoderservice` has **ZERO references** to DSIKOMOGfxStreamSink -- it does not implement or call this DSI service
- `libPresentationController.so` also has **ZERO references** to GfxStreamSink
- No other binary in the firmware references the server UUID `7f54a915-2fc6-5ec3-81a7-a15e19315a24`

**Result:** The KOMOCaller's DSI subscription to ATTR_GFXSTATE never receives any notification -> `updateGfxState` never called -> `gfxAvailable` stays false.

## Native Navigation Works -- How?

User confirmed that native route guidance on MU1316 DOES produce widget video on the VC cluster. This means `gfxAvailable` DOES become true during native RG. Possible explanations:

1. **Native RG triggers a different code path** that sets gfxAvailable without going through DSIKOMOGfxStreamSink
2. **Native RG activates a service that provides DSIKOMOGfxStreamSink** (e.g., PresentationController activates its own GfxStreamSink when rendering starts)
3. **Native RG sets gfxAvailable directly** via an internal method not visible in the decompiled code

This is an open question. The KOMOProbe Phase 5 tests force gfxAvailable to bypass this uncertainty.

## Other Missing DSI Services

| DSI Service | Provider on MU1316 | Impact |
|-------------|-------------------|--------|
| DSIKOMOView | libPresentationController.so (ServiceProviderDSIKomoViewImpl) | **Available** -- RouteInfoElement[] data arrives |
| DSIKOMONavInfo | NONE | Nav metadata (street, distance text) not delivered via DSI |
| DSIKOMOGfxStreamSink | NONE | **gfxAvailable never set** |

## Fix (IMPLEMENTED)

`BAPBridge.forceGfxAvailable(true)` in `startKOMO()` uses a 3-strategy approach:

```
Strategy 1: komoService.updateGfxState(1, 1)  -- direct callback mimicry
Strategy 2: clusterViewMode.setGFXAvailable(true)  -- method reflection
Strategy 3: field reflection on clusterViewMode.gfxAvailable  -- direct field set
```

Tries all three in order, logs which succeeded. Called in `startKOMO()` and reversed
in `stopKOMO()` with `forceGfxAvailable(false)`.

File: `java_patch/com/luka/carplay/routeguidance/BAPBridge.java`

## Relationship to VC State Machine

When `gfxAvailable=true`:
1. ClusterViewMode signals change -> CombiBAPListener detects -> BAP sends `rgType=4` (MAP)
2. VC EB GUIDE receives rgType=4 -> ViewModeSM transitions to MAP -> sets `INTERN_Active_NavFPK_Content=Map(1)`
3. videoencoderservice is running (from context switch + setUpdateRate) -> MOST video stream flows
4. VC detects MOST sync -> `LVDS_Available=1`
5. `LVDS_Available=1` + `content=Map(1)` -> EB GUIDE -> `SV_LVDS_NavMap_FPK` (video displayed)

Without gfxAvailable=true, step 1 never happens -> VC never enters MAP mode -> even if video is streaming, VC doesn't display it.
