# CarPlay Video — RE of the capability, the wire key, and the in-motion gate

Sources: `carkitd` + **CarKit** and **CoreMedia** extracted from the iOS 27 (24A5418b)
dyld_shared_cache; **MediaToolbox**; our stock libairplay 210.81 and the 280.33.8 SDK.
Status: capability chain PROVEN; transport and the in-motion writer NOT proven.

## 1. The capability — proven end to end

`CARSessionConfiguration.videoPlaybackSupported` is **ivar +15**, written in
`-[CARSessionConfiguration initWithSessionStatusOptions:propertySupplier:]`:

```c
if ( os_feature_enabled("CarPlay", "VideoBrowse") )   // Apple-internal force
    *(BYTE*)(cfg+15) = 1;
else {                                                // ← from the CAR
    v = propertySupplier("EndpointInfo")["videoPlaybackInfo"];
    *(BYTE*)(cfg+15) = (v != NULL);                   // CMP X0,#0 / CSET NE
}
```

Then: `-[CRPairingAirPlaySessionInfo initWithSession:]` → `supportsVideoPlayback` →
`-[CRVehicle updateWithAirPlaySessionInfo:]`.

**Key names resolved by dereferencing the constants** (not guessed):

| CarKit constant | CFString value |
|---|---|
| `CARkAPEndpointProperty_EndpointInfo` | `"EndpointInfo"` |
| `CARkAPEndpointInfoResponseKey_VideoInfo` | **`"videoPlaybackInfo"`** |
| `CARkAPEndpointInfoResponseKey_MainBufferedInfo` | `"mainBufferedInfo"` |

`APEndpointInfo**Response**Key` settles the channel: **`EndpointInfo` is the AirPlay `/info`
response** — the dictionary our hook already builds in `AirPlayCopyServerInfo`.

**Presence-only.** The string `videoPlaybackInfo` occurs exactly ONCE in CarKit and the test is
`CMP #0 / CSET NE`. Contents are not parsed there, and unlike `VehicleStateProtocolInfo` (which has
`PluginConfigs`/`PluginCount`/`ProtocolVersion`) `VideoInfo` has no sub-key constants at all.

### The full set of /info keys iOS reads
```
AudioFormats   FileTransfer   LogTransfer
MainBufferedInfo   VehicleStateProtocolInfo   VideoInfo
```

## 2. The in-motion gate

Two DIFFERENT properties, easy to conflate:

- **endpoint side** (defined in CoreMedia, read by CarKit):
  `kFigEndpointProperty_AirPlayVideoPlaybackAudioOnlyMode`, values
  `..._None` / `..._Elected` / `..._Forced`.
- **player side** (MediaToolbox): `kFigPlayerProperty_UseAirPlayVideoAudioOnlyMode`.

`-[CARSession videoPlaybackAvailable]` (`0x1c20071b8`) reduces to:

```
videoPlaybackAvailable = ( endpoint[AirPlayVideoPlaybackAudioOnlyMode] != "Forced" )
```
(the compare is against the CFString `"Forced"`, then `EOR W2,W0,#1`). Around it:
`cachedVideoPlaybackAvailable` and `CARSessionVideoPlaybackAvailabilityChangedNotification` — so the
value changes at runtime and is broadcast. Apple's model is therefore *degrade to audio-only*, not a
hard block.

**Who writes it: UNKNOWN.** CoreMedia only *defines* the constants (zero xrefs inside it);
MediaToolbox does not import them (`nm` clean). The likely writer is `airplayd`, which cannot be
pulled from the cache (`MH_EXECUTE`; `ipsw dyld extract` handles dylibs only), and a cache-wide
string hunt is useless because the shared linkedit de-duplicates symbol strings.

## 3. Transport — unresolved, two models

I flip-flopped on this; recording both with their evidence rather than picking one.

**(a) Buffered AirPlay Video (currently the stronger reading)**
- `kAirPlayStreamType_Playback 120 // AirPlay Video commands` — commands, not pixels.
- MediaToolbox player properties: `EnableBufferedAirPlayLocalPlayback`,
  `BufferedAirPlayLocalPlaybackVolume`, `UsesAirPlayVideoWhileAirPlayScreenIsActive`
  → video runs *alongside* the mirror, and the receiver plays media **locally**.
- `/info` carries `videoPlaybackInfo` **and** `mainBufferedInfo`.
- **Corroborated from the audio side:** `AUDIO.md §21` independently found that iOS 27 publishes a
  **buffered media stream** (`bufferStream`, audio type 103 / `media`) separate from the classic
  realtime stream. A buffered tier demonstrably exists in iOS 27 CarPlay.

**(b) Renders into the existing screen stream 110**
- Only real support: `-[CARSession _sessionUpdatesQueue_handleIsPlayingVideoFromApp:]` — which reads
  just as well as "a buffered playback is active".
- `_CARkMainBufferedAudio_DCX` sits next to `mainBufferedInfo`, i.e. that key leans audio — this was
  my reason for briefly preferring (b), and it does not actually disprove a buffered video path.

If (a) is right, `videoPlaybackInfo` is not a permission but an **obligation**: the car declares it
can fetch and decode media itself.

## 4. Our stack

`videoPlayback` / `AudioOnlyMode` / `VideoBrowse` / buffered-playback: **zero occurrences** in the
stock 210.81 libairplay and **zero** in the 280.33.8 SDK. `kAirPlayStreamType_Playback` exists in the
SDK header as a bare `#define` with no implementation anywhere.

## 5. Why there is no code in the hook

A probe was written and reverted (`6207dfe`, reverted): a `videoPlaybackInfo` key injected into
`/info` from the `AirPlayCopyServerInfo` interposer, env-gated `CARPLAY_VIDEO_PLAYBACK`, dark by
default. It is ~10 lines and the injection point is confirmed correct — but it would only raise the
capability bit (surfacing `CRVideoSettingsApp` / onboarding on the phone) while the transport and the
in-motion writer remain unknown, and under model (a) the car would be claiming an ability it does not
have. Re-add it when the transport question is settled.

## 6. Next steps, cheapest first
1. Settle (a) vs (b): find what actually carries CarPlay video. The buffered-stream work in
   `AUDIO.md §17/§21` is the same layer — reuse it.
2. Identify the writer of the endpoint `AudioOnlyMode`. Needs `airplayd`, i.e. `ipsw dyld split`
   (the whole ~7 GB cache) or another route to `MH_EXECUTE` images.
3. Only then decide whether the `/info` probe is worth running on the unit.
