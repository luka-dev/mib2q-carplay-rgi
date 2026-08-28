# CarPlay accessory feature survey — what we could still add

Source: capability flags in carkitd/accessoryd 23G71 (iOS 26.6). "We" = this project's additions on
top of Audi's stock CarPlay.

## Baseline (already there, NOT our work)
Stock Audi CarPlay (stock dio_manager + libairplay) already provides: **main-screen video, audio
out, touch/HID input on the main screen, Siri, phone/telephony, media playback**. Our project only
ADDS two things on top: **RGI** (RouteGuidanceDisplayComponent, cluster turn-by-turn) and
**AltScreen** (cluster second display, stream 111).

So "what else does CP support that we haven't enabled" = features beyond stock CP + RGI/AltScreen.

## Tier 1 — nav-aligned, natural extensions of what we're building
- **Vehicle motion data → phone** (`ForwardAccel`, `LateralAccel`, wheel/ground speed) +
  **`SupportsLocationDeadReckoning`**: the car streams its own motion to the phone so nav keeps
  working in tunnels / parking decks / GPS-denied, and the ETA/position stay live. Car→phone
  direction. No new hardware — the signals already exist on the vehicle bus; work is advertising the
  sensor component and piping the values. **Biggest lever, squarely in our lane.**
- **RoadObjectDetection / speed-limit signs** (`ACCNavigationRoadObjectDetectionInfo`, RoadSign
  Type+Value — see IAP2_RG_TLV_COVERAGE.md): speed limit + sign detection on the cluster. Depends on
  feeding ego-speed/yaw above, so it pairs with Tier-1 vehicle data.

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
- DND while driving (`SupportsDNDWhileDriving`), Live Activities / LOD widgets
  (`supportsLiveActivities`, `supportsLodWidgets`).

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
