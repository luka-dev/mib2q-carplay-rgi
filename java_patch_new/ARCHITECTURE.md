# java_patch_new — clean-room redesign of the MHI2Q CarPlay Java patch

Full rewrite (decided 2026-07-27), compiled against the accurate full decompile
`Tools/jxe2jar/out/MU1316-combined-final.jar` (not the old `lsd.jar`). Java **1.2**
target (MU1316 lsd runtime). Ports the proven RGD/coverart *algorithms*; redesigns the
*foundation* (entry, framework access, bus, module lifecycle) and adds the cluster **altScreen**.

## Integration seam (unchanged, proven)
A tiny hook call is inserted into ONE stock class and the patched stock classes override
lsd at inject time. Entry:
- `de/audi/app/terminalmode/combi/TerminalModeBapCombi$ActiveDeviceStateListener` → calls
  `CarPlayApp.onActivate(ctx)` / `onDeactivate()` on CarPlay connect/disconnect.
- `ctx` → `IFrameworkAccess` → HMI services (models, DSI providers, NavigationEnv/ClusterService).

## Module map (`com.luka.carplay.*`)
- **`bus`** — `CarplayBus`: **bidirectional** TCP link to the C hook (Java = server :19810).
  Receives hook events AND sends `CMD_*` (the old patch's `send()` was a stub — now a real writer).
- **`core`** — `CarPlayApp` (entry/lifecycle), `FrameworkRef` (holds IFrameworkAccess + service lookups),
  `Module` (start/stop lifecycle interface).
- **`rgd`** — route guidance (port ManeuverMapper/SideStreets/BAPBridge/RouteGuidance logic).
- **`coverart`** — cover-art bus event → BAP.
- **`input`** — touch/cursor (main screen) + the cluster roller listener.
- **`altscreen`** — NEW cluster integration (below).

## altScreen cluster design (RE-grounded)
Cluster tab lives on the **kombisync** layer as top-level context **117** (SMARTPHONE_CARPLAY /
0x75) — NOT a `ClusterViewMode` sub-mode. Two axes:
1. **License / availability** — `MenuState.smartPhone = AVAILABLE(2)` (byte5 b6-7 / `ZR_Sta_SmartPhone`)
   pushed via the kombisync provider on phone-connect; `NOT_EXISTING(3)` on disconnect. MU1316 **never
   sets this** (verified: only the RX deserializer writes it) → we add the sender. VC also gates on
   coding 4237/4238.
2. **Active switch** — watch `DSIKombiSync DisplayRequestResponse.mainContext`; `==117` ⇒ our tab is
   active → feed altScreen MOST video + route roller→`CMD_ALT_HID` + hide the KDK maneuver render;
   `!=117` ⇒ stop video, show KDK render, roller to native menu. Force via `setMMIDisplayStatus`.
- **Roller capture** — a `VirtualButtonListener` (template `TMVirtualButtonListener`): `increment/
  decrement`→`CMD_ALT_HID [wheel]`, `keyPressed(select)`→`[buttons]`; only while tab active.
- **KDK render hide** — gate on active tab; do NOT touch BAP (projection rides `CombiBAPListener`).

## Build
`build_java_new.sh` → compile `java_patch_new/**.java` against `MU1316-combined-final.jar` + OSGi →
`build/carplay_hook_new.jar`. Same deploy path as the old patch.

## Status
- [x] Folder + build script + this doc
- [x] `bus.CarplayBus` — bidirectional (reader + real writer)
- [ ] `core` entry/lifecycle/framework-ref
- [ ] rgd / coverart / input (port)
- [ ] altscreen (kombisync tab driver, mainContext observer, roller listener, KDK-hide)
- [ ] patched stock classes (from combined-final decompile)
