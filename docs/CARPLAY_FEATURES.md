# CarPlay features — what we have, what's reachable, what isn't

Consolidated map of CarPlay accessory features vs this project (Audi MHI2Q, wired USB).
"We" = our additions on top of Audi's **stock** CarPlay. Detailed RE lives under `docs/reference/`.

## Baseline — already provided by stock Audi CarPlay (NOT our work)
From the active message allowlist in `dio_manager.json` (the `##` lines are a legend; the arrays are live):
main-screen **video**, **audio out**, **touch/HID** on the main screen, **Siri**, **phone/telephony**
(CallState, Communications), **Power** updates, **NowPlaying + metadata** (title/album/artist/…,
msg 0x5000/0x5001), **Media Library** browsing, and **LocationInformation + dead reckoning**
(0xFFFA/B/C, `nmea` `deadReckoning:true`, "China No GNSS" CNMEAProvider).

## Our additions
- **RGI** — cluster/HUD route guidance (RouteGuidanceDisplayComponent + 0x5200–0x5204). Live.
- **AltScreen** — cluster second display (stream 111), advertised via the dio SetProperty patch as
  `"altScreen"` in `/info extendedFeatures`. Ref: `reference/IOS26_VS_IOS27_BONJOUR.md`,
  memory `altscreen_ios26_gate_verified`.

## The advertisement mechanism we control
`CARSessionConfiguration` properties + `/info extendedFeatures` are injected via the **dio
SetProperty patch** (same path as altScreen). This is the lever for adding phone-visible capabilities.

## Feature-by-feature verdict

| Feature | Where it lives | Verdict for us |
|---|---|---|
| **RGI (route guidance)** | iAP2 0x520x | ✅ done |
| **AltScreen (cluster display)** | `/info extendedFeatures` | ✅ done |
| **NowPlaying + metadata** | iAP2 0x5000/0x5001 | ✅ stock already |
| **Location + dead reckoning** | iAP2 0xFFFA/B/C | ✅ stock already (verify enabled) |
| **Speed-limit signs / RoadObjectDetection** | `ACCNavigationRoadObjectDetectionInfo` (RoadSign Type+Value) | ⚠️ net-new; needs a detection component + ego-speed/yaw. Stock already streams speed via GPRMC. Nav-aligned, real work. `reference/IAP2_RG_TLV_COVERAGE.md` |
| **SourceName (which nav app)** | RGU TLV 0x13 | ⚠️ cheapest: hook already parses & buses `source_name`; Java just drops it. Java-only surface. |
| **Lane InstructionText** | RGU lane 0x5204/0x03 | ⚠️ parsed at hook, not surfaced on cluster |
| **EV fields (charging/battery/TZ/stop)** | RGU 0x15–0x1A | ⚠️ parsed-then-dropped; extend parser + BAP surface if cluster has widgets |
| **Dolby Atmos / Spatial audio** | `CARSessionConfiguration.spatial/dolbyAtmos*` | ❌ transport wall: libairplay `AudioSessionGetSupportedOutputFormats` advertises only ≤2-ch (AAC/ALAC/PCM /2). Codec won't help — channels are the wall; needs multichannel end-to-end + amp path. `reference/HU_AUDIO_CEILING.md`, `reference/CARPLAY_SPATIAL_DOLBY_PLAN.md` |
| **DND while driving** | car advertises a flag | ⚠️ phone-side Focus; cheap if we find where to advertise it. Not cluster/nav. |
| **LOD widgets / Live Activities** | `CARSessionConfiguration.lodWidgetsMode/liveActivitiesMode` ← CRCarPlayCapabilities | ⚠️ **REACHABILITY OPEN.** NOT a hard prototype gate (default mode=2=ON). Real gates: server deny-list, `disabledFeature` bit, and the car surfacing a modern Dashboard/widget UI + AirPlay session version. Full RE: `reference/LODWIDGETS_LIVEACTIVITIES_RE.md` |
| **Wireless CarPlay** | BT+WiFi pairing | ❌ needs radios/stack; wired-only box |
| **Car Keys** | UWB/NFC | ❌ hardware |
| **High-fidelity touch on cluster** | `primaryInputDevice` | ❌ cluster has no touch panel |

## RE assets from this session (iOS 26.6 / 27.0)
- carkitd / accessoryd / caraccessoryd decompiled → `Refferences/ios_carplay_diff/`.
- iOS 27 dyld_shared_cache extracted + **CarKit** pulled → `Refferences/ios27_dsc/24A5418b__iPhone18,1/`.
  Recipe: `ipsw extract --dyld --dyld-arch arm64e`; `ipsw dyld objc --class` to find owner image;
  `ipsw dyld extract <cache> <path>`.

## Where the RE stands / next steps
- **Speed-limit signs** = the one clearly nav-aligned net-new feature; part of its prerequisite
  (vehicle speed) is already streamed by stock.
- **LOD widgets/Live Activities** = reopened; next is to check (a) whether stock CarPlay exposes a
  Dashboard surface at all, and (b) the `disabledCheck` in `-[CARSessionConfiguration updateCarCapabilities]`.
- **SourceName** = the cheapest concrete win (Java-only).
