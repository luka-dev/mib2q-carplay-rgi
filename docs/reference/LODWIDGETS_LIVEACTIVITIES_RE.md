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
