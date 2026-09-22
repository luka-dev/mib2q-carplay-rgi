# RE: how iOS gates LodWidgets / LiveActivities (and Atmos/Spatial/Mixable/SiriVoice)

Decompiled carkitd 24A5418b (iOS 27.0). Goal: what must the CAR advertise to enable these.

## The chain (phone side)

1. **`-[CRPairingAirPlaySessionInfo initWithSession:]`** (`0x10002e06c`) reads everything from
   **`[session configuration]`** — a `CARSessionConfiguration` that the phone built from the
   accessory's AirPlay SETUP:
   - `configuration.lodWidgetsMode`        → `supportsLodWidgets  = (lodWidgetsMode  != 1)`
   - `configuration.liveActivitiesMode`    → `supportsLiveActivities = (liveActivitiesMode != 1)`
   - `configuration.dolbyAtmosAudioSupported` → `supportsDolbyAtmos`
   - `configuration.spatialAudioSupported`    → `supportsSpatialAudio`
   - `configuration.supportsSiriMixable`      → `supportsMixableAudio`
   - `configuration.supportsSiriZLL`          → `supportsEnhancedSiriVoice`
   - `configuration.videoPlaybackSupported`   → `supportsVideoPlayback`
   - RouteSharing via `CAFASCTree hasAccessory:CAFAccessoryTypeNavigation service:CAFServiceTypeRouteSharing`
2. **`-[CRVehicle updateWithAirPlaySessionInfo:]`** (`0x10002de14`) copies those onto the vehicle
   (`setSupportsLodWidgets:` etc.). (Apple bug: several branches mis-call `setSupportsEnhancedSiriVoice:`.)
3. **`-[CRCarPlayCapabilitiesManager reconcileCapabilities]`** (`0x10002992C`) reconciles the
   car-advertised values with `CARPrototypePref` (Apple-internal debug prefs) and per-vehicle user
   prefs + the `CarPlayWidgetDenyList` / `CarPlayLiveActivityDenyList`, then the phone offers them.

## What this means for us

**These are NOT a new protocol and NOT an AirPlay feature bit.** They live in the
**CARSessionConfiguration** carried in the CarPlay/AirPlay SETUP — the *same session-config layer*
as altScreen's `CARSessionRequestDisplayConfiguration`. So the injection point on our side is the
same place we already patch for altScreen (the dio setup / SetProperty path), IF we can set these
config fields.

**Inversion caveat:** `supports* = (mode != 1)`. So `lodWidgetsMode`/`liveActivitiesMode` are a
tri-state where **1 = explicitly off**; 0/absent/other reads as supported. Meaning the phone may
already treat an absent key as "supported" — the real gate is likely elsewhere (deny-lists,
per-vehicle prefs, or a minimum session version). Do NOT assume "just set the key = on".

## Open item (the actionable next step)

Find the WIRE key of `CARSessionConfiguration.lodWidgetsMode` / `.liveActivitiesMode` — i.e. how the
accessory's SETUP plist populates that configuration. That decoding is in **CarPlay.framework /
the AirPlay session setup**, not carkitd; load that binary and trace `CARSessionConfiguration`
property → setup-dict key. Only then do we know the exact bytes to add to our libairplay/dio setup.

Reality check: the stock HU libairplay (210.81) and even the leaked 280.33.8 SDK have NO
lodwidget/liveactivity strings, so whatever the key is, our stack does not emit it today — this is a
net-new addition to the setup config, gated by whether the phone accepts it from a stack this old.

---

# Wire-key hunt — where it stalls (2026-08-28)

Traced the phone side to `CARSessionConfiguration.lodWidgetsMode` / `.liveActivitiesMode`. The class
that DEFINES those properties and decodes them from the wire is **not in the extracted standalone
binaries** (carkitd / accessoryd / caraccessoryd / CoreAccessories.framework). `caraccessoryd` only
carries the method-signature type string `...@"CARSessionConfiguration"...` — the implementation
lives in a framework inside the **dyld_shared_cache**, which our IPSW extraction did not unpack.

## Reframe that changes the plan
`supports* = (mode != 1)`. `lodWidgetsMode`/`liveActivitiesMode` are tri-state; **1 = explicitly
off**, and **absent/0 reads as SUPPORTED**. So the phone does NOT need a key set to "on" — a car
that says nothing already looks like it supports them. That means the real gate is almost certainly
NOT "add a missing key", but one of:
- a **minimum CarPlay/AirPlay session version** the config carries (our 210.81 stack is far below
  iOS-17-era), so the phone never even evaluates these fields for an old session;
- the **CarPlayWidgetDenyList / CarPlayLiveActivityDenyList** (server-pushed) excluding unknown/old cars;
- the car must actually accept the **Dashboard/widget render** into the video stream, which is a
  whole UI negotiation, not a bool.

## To go further (two options)
1. **Extract the defining framework from the dyld_shared_cache** in the iOS 27 IPSW
   (`ipsw dyld extract <cache> <CARSessionConfiguration-owner>` after unpacking the cache), then
   trace `lodWidgetsMode` property → the setup-dict key + the session-version guard. Heavy (multi-GB
   cache) but it is the only path to the exact bytes.
2. **Stop here and treat it as low-yield.** Given the inversion (absent=supported) plus the
   210.81/280.33.8 stack having zero widget/session-version support, the likely truth is the feature
   is gated by session version, not a key we can add — i.e. not reachable without a much newer stack.
   The RE was still worth it: it proved the layer (CARSessionConfiguration, same as altScreen) and
   ruled out "just flip a bit".

---

# Wire key FOUND — dyld_shared_cache extracted (2026-08-28)

Extracted `dyld_shared_cache_arm64e` from the iOS 27 IPSW, located the class, pulled the dylib:
`ipsw dyld extract … /System/Library/PrivateFrameworks/CarKit.framework/CarKit`. `CARSessionConfiguration`
is defined in **CarKit** (0x1e8938ce0).

## The full chain, end to end
1. **AirPlay receiver session (Fig endpoint)** — `-[CARSession initWithFigEndpoint:sessionStatusOptions:
   nightModeProvider:]`. The car's advertised session properties arrive as **`sessionStatusOptions`**
   (a MediaToolbox/AirPlay Fig-endpoint status dict).
2. **`-[CARSessionConfiguration initWithSessionStatusOptions:propertySupplier:]`** (`0x1c2008f7c`) reads
   each property through a `propertySupplier(key)` keyed by string constants. CarKit's own key strings
   include **`LodWidgetsKey`**, **`LodWidgetsMode`**, `LodWidgets`, and the LiveActivities equivalents.
3. Getter `-[CARSessionConfiguration lodWidgetsMode]` = **ivar +184**; `liveActivitiesMode` similarly.
4. `-[CRPairingAirPlaySessionInfo initWithSession:]` → `supportsLodWidgets = (lodWidgetsMode != 1)`,
   `supportsLiveActivities = (liveActivitiesMode != 1)`.

## What this nails down
- The capability is carried in the **AirPlay session-status-options** — the same AirPlay/Fig session
  transport as the rest of CarPlay setup. **Not a new protocol, not a feature bit.** Confirmed.
- CarKit key name: `LodWidgetsKey` / `LiveActivities…`. The value is a tri-state MODE.

## The remaining (and final) unknown
Where the ACCESSORY populates `sessionStatusOptions.lodWidgetsMode` on the wire lives one dylib
further out — in **AirPlaySupport / MediaToolbox (the Fig endpoint)**, not CarKit. That is the byte
the HU would emit.

## Verdict (recommendation: stop)
The inversion still stands: `supports = (mode != 1)`, so **absent ⇒ supported**. Combined with:
- our stock libairplay 210.81 (and the 280.33.8 SDK) emitting **none** of these session-status
  fields, and
- the whole path being iOS-17-era AirPlay session status the old stack never negotiates,
the realistic conclusion is that the gate is the **AirPlay session version the old stack advertises**,
not a key we can bolt on. Chasing the exact Fig byte in AirPlaySupport would confirm this but is very
unlikely to yield a working enable on a 210.81 stack. RE goal achieved: mechanism, layer, class,
ivar, key name, and the inversion are all pinned; "just flip a bit" is ruled out.

---

# The dig, to the bottom — it is PROTOTYPE-gated (2026-08-28)

Pushed past CarKit into the value's true origin. `CARSessionConfiguration.lodWidgetsMode` (ivar +184)
is NOT written in `initWithSessionStatusOptions:` — it is written only by
`-[CARSessionConfiguration updateCarCapabilities]` (`0x1c200af60`):

```
+184 (lodWidgetsMode)  = disabledCheck ? 1 /*off*/ : [carCaps <lodWidgetsMode>]
+176 (liveActivities)  = disabledCheck ? 1 /*off*/ : [carCaps <liveActivitiesMode>]
```
where `carCaps` = an **`_OBJC_CLASS_$_CRCarPlayCapabilities`** instance (off_1E1766960 resolved via
the shared cache).

`CRCarPlayCapabilities.lodWidgetsMode` is set by `-[CRCarPlayCapabilitiesManager reconcileCapabilities]`
(carkitd), which blends three sources:
1. the accessory's advertised value,
2. per-vehicle stored user prefs, and
3. **`+[CARPrototypePref lodWidgets]`** / `+[CARPrototypePref liveActivities]` — an Apple
   **prototype/internal preference** (a `dispatch_once` singleton created with a `CRLocalizedStringForKey`
   title, i.e. a toggle that shows up only in an Apple-internal/prototype Settings menu), plus the
   server-pushed **deny-list** (`com.apple.carkit.liveactivity.denylist-changed`).

Persistence/coding keys: **`CapabilitiesLodWidgetsKey`**, **`CapabilitiesLiveActivitiesKey`**,
plus a `disabledFeature` bitmask.

## Definitive verdict
This is not merely "our stack is too old". As of iOS 27, CarPlay LOD widgets / Live Activities are
**still gated behind `CARPrototypePref` (Apple prototype/internal prefs) and a server deny-list**,
blended with the car's advertisement. There is no clean, car-advertised boolean that turns them on:
even a modern head unit is subject to Apple's prototype flag + deny-list. So **we cannot enable them
from the accessory side** — full stop. The RE is complete and conclusive; closing this line of work.

---

# CORRECTION — the prototype default is ON, not off (2026-08-28)

I was WRONG to call this "prototype-gated / impossible". Read the actual default:
`+[CARPrototypePref lodWidgets]` factory (block @0x1c204fef0) passes default =
`unk_1ED9EB928` = an **NSConstantIntegerNumber value 2**. Mode semantics (from
`supports = (mode != 1)`): **1 = OFF, 2 = default = ON**, -1 = use car's value, 0 = unset.

`reconcileCapabilities` on a production phone (user hasn't set the prototype toggle, so
`[pref value] == nil`): the override is skipped, then `if (!mode) mode = [pref defaultValue] = 2`.
So **lodWidgetsMode defaults to 2 → supportsLodWidgets = TRUE by default**. The prototype pref does
NOT gate the feature off; it defaults it ON. Same story expected for liveActivities.

## Revised picture — this is NOT impossible from the accessory side
Consistent with the project's own altScreen / spatialAudio approach (CARSessionConfiguration
properties advertised via the dio SetProperty patch, see AUDIO.md / CARPLAY_SPATIAL_DOLBY_PLAN.md).
The real remaining gates are, in order:
1. **The server deny-list** (`CarPlayWidgetDenyList` / `CarPlayLiveActivityDenyList`,
   `com.apple.carkit.liveactivity.denylist-changed`) — Apple can exclude specific/unknown cars.
2. **The `disabledFeature` path in `updateCarCapabilities`** — `+184 = disabledCheck ? 1 : carCaps`
   — some condition can still force mode=1 (off).
3. **The car must actually surface the Dashboard/widget UI** — Live Activities / LOD widgets are
   drawn by the phone into the CarPlay video; the car needs a modern-enough CarPlay UI/Dashboard for
   the phone to place them. This, plus the AirPlay session version, is the likely practical wall for
   our 210.81 stack — but it is NOT a hard prototype flag.

## Bottom line
Reachability is OPEN again: default-ON means the car does not have to fight a prototype gate. Whether
our specific stack can actually get widgets to render is now the real question (deny-list + Dashboard
surface + session version), same class of problem as any CARSessionConfiguration advert we already
do for altScreen — not a categorical "no".
