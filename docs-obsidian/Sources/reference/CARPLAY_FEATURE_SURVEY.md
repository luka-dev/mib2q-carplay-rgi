# CarPlay accessory feature survey — what we could still add

Source: capability flags in carkitd/accessoryd 23G71 (iOS 26.6). "We" = this project's additions on
top of Audi's stock CarPlay.

## Baseline (already there, NOT our work)
Stock Audi CarPlay (stock dio_manager + libairplay) already provides FAR more than the base screen.
From the ACTIVE message allowlist in `dio_manager.json` (the `##` lines are a legend; the arrays
below them are live): main-screen video, audio out, touch/HID on the main screen, Siri,
phone/telephony (CallState 0x4154/56, Communications 0x4157/59), Power updates (0xAE00-03),
**NowPlaying + metadata** (0x5000/0x5001; CIAP2ServiceCinemo logs title/albumTitle/artist/genre/
composer/elapsed/shuffle/repeat/appName), **Media Library browsing** (0x4C00-05), and
**LocationInformation + dead reckoning** (Sent 0xFFFB, Received 0xFFFA/0xFFFC; `nmea` block
`deadReckoning:true`, bitmask 0x0003 = GPGGA+GPRMC position+speed/course — the "China No GNSS"
CNMEAProvider path). Our project only
ADDS two things on top: **RGI** (RouteGuidanceDisplayComponent, cluster turn-by-turn) and
**AltScreen** (cluster second display, stream 111).

So "what else does CP support that we haven't enabled" = features beyond stock CP + RGI/AltScreen.

## CORRECTION (2026-08-28, verified in dio_manager.json)
Two items first listed here as "to add" are ALREADY in stock and active:
- **LocationInformation + dead reckoning** — stock dio does it by default (0xFFFA/0xFFFB/0xFFFC
  active, `deadReckoning:true`). NOT something to add; if we want it richer we tune the existing
  `nmea` config/bitmask, not build it.
- **NowPlaying + metadata** — stock-active (0x5000/0x5001). Only the album-ART image render has
  historically been finicky here (DSICarplay coverArtUrl), the metadata pipe is stock.

## Tier 1 — genuinely not in stock, nav-aligned
- **RoadObjectDetection / speed-limit signs** (`ACCNavigationRoadObjectDetectionInfo`, RoadSign
  Type+Value — see IAP2_RG_TLV_COVERAGE.md): not in the stock message allowlist. Needs an
  advertised detection component + ego-speed/yaw. Note stock already streams speed via GPRMC
  (LocationInformation), so part of the ego-motion prerequisite is met.
- **Raw vehicle IMU to phone** (`ForwardAccel`/`LateralAccel` as a distinct accessory feed, beyond
  the NMEA position/speed already sent) — only if RoadObjectDetection needs finer ego-motion than
  GPRMC speed.

## Tier 2 — media / interaction
- **Now Playing metadata + album art** (`AccessoryNowPlaying`, `nowPlayingAlbumArt`,
  `NowPlayingUpdate`): track title/artist + cover art on the cluster/HUD. Same lane as the cover-art
  work already attempted here.
- **Dashboard** (`Dashboard`, `dashboardRoundedCorners`): the CarPlay Dashboard layout (map + widget
  tiles). A distinct screen mode, could ride the same video path.
- **Mixable audio** (`supportsMixableAudio`, `supportsSiriMixable`): nav/Siri prompts mix over media
  instead of hard-ducking. Audio-quality feature; we currently do PCM passthrough only.
- **Siri hardware button + enhanced voice** (`supportsSiriZLLButton`, `supportsEnhancedSiriVoice`):
  steering-wheel Siri button routing.

## Tier 3 — niche / advanced
- Destination & route sharing (`SupportsDestinationSharing`, `supportsRouteSharing`) — phone→car
  destination handoff.
- Electronic Toll Collection (`supportsElectronicTollCollection`).
- Dolby Atmos / Spatial audio (`supportsDolbyAtmos`, `supportsSpatialAudio`) — vs our stereo PCM.
- DND while driving (`SupportsDNDWhileDriving`) — genuinely phone-side, gated by a car flag; the
  only cheap one here IF we find where to advertise it.
- **Live Activities / LOD widgets — NOT feasible (corrected 2026-08-28).** Neither the stock
  libairplay 210.81 NOR the leaked 280.33.8 SDK has any lodWidget/liveActivity feature bit or key
  (verified: the AirPlay feature enum is Audio/AudioAAC_LC/AudioPCM/Car/CarPlayControl/Screen/Rotate/
  Pairing/UnifiedBonjour — no widget/dashboard bit). These are an iOS 17+ capability negotiated by a
  newer CarPlay-capabilities mechanism our stack version predates entirely. Enabling would require RE
  of that negotiation from carkitd plus a much newer stack — not a flag flip. My earlier "low effort"
  note was wrong.

## Out of scope for this box
- **Wireless CarPlay** (`supportsWirelessCarPlay`) — needs the BT+WiFi pairing stack and radios; we
  are wired USB only.
- **Car Keys** (`supportsCarKeys`) — digital key, needs UWB/NFC.
- **High-fidelity touch on the cluster** (`supportsHighFidelityTouch`) — the cluster has no touch
  panel (build_cluster_dict advertises no primaryInputDevice); main-screen touch is already stock.

## Honesty note
These flags are what the PHONE supports; whether stock Audi already advertises a given one on the
car side was not exhaustively checked against the live stock /info. The list is the menu of what CP
*can* do; each Tier-1/2 item needs its own confirmation of the exact accessory advertisement + message
before committing.
