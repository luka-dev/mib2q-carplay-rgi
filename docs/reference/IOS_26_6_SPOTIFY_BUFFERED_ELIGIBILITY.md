# iOS 26.6: Spotify and buffered CarPlay audio

Date: 2026-09-19. Static findings verified in 23G71; actual iPhone session policy was not captured.

## Finding

MediaToolbox explicitly denies buffered eligibility for a FigAirPlay route whose audio session's `HostApplicationDisplayID` equals `com.spotify.client`. This is executable policy, not merely a diagnostic string. It explains why receiver type-103 capability alone cannot guarantee a Spotify type-103 attempt.

This finding does **not** prove every possible Spotify playback path is denied. The inspected policy controls registration of this route's client. Other buffered clients, sample-buffer renderers, and existing routing contexts have separate lifetimes. A receiver log alone cannot identify which of these phone-side states caused a particular stream transition.

No requirement to launch Apple Music once or enable Dolby Atmos once was found in the inspected capability, stream-construction, or client-policy functions. A persistent first-use unlock remains unproved.

## Inputs

- Requested archive: `/Users/luka/Desktop/AUDI_2/Refferences/iPhone18,2_26.6_23G71_Restore.ipsw`. Its `BuildManifest.plist` reports ProductVersion `26.6`, ProductBuildVersion `23G71`, SupportedProductTypes `[iPhone18,2]`.
- Existing cache: `/Users/luka/Desktop/AUDI_2/Refferences/ios266_extract/23G71__iPhone18,2/dyld_shared_cache_arm64e` and sibling subcaches. The existing extraction's directory matches the manifest; this pass did not re-extract and byte-compare the entire cache against the archive.
- AirPlaySender: `ios266_extract/dylibs/AirPlaySender`, SHA-256 `952d73f8e97d4697490f69bdf08067df6ccff29f5b44c60489eb117db015d832`.
- MediaToolbox extracted this pass with `ipsw dyld extract <cache> MediaToolbox --slide --output <dylibs>`, SHA-256 `05b3c01f264c8cef99a45f62534ad663aff3f0ea76b7d7d8c4f6d80cdfc5fdb1`.
- Retained decompilation: `output/ios266-buffered-eligibility/evidence.json`. Relevant functions were also annotated in their IDBs.

## Exact Spotify policy

`MediaToolbox!airplayRoute_updateAudioSessionPolicyForBuffered`, `0x1974a061c`, obtains these audio-session properties:

| Import slot | Decoded property |
| --- | --- |
| `0x1e5d7a8e8` | `AudioCategory` |
| `0x1e5d7ac30` | `InterruptionStyle` |
| `0x1e5d7ab98` | `HostApplicationDisplayID` |
| `0x1e5d7aed8` | `PrefersConcurrentAirPlayAudio` |
| `0x1e5d7af90` | `RemoteDeviceID` |

The original cache was used to follow each import slot through two pointer indirections to its CFString. Before app checks the function disallows certain interruption styles (16 or 32 without RemoteDeviceID), concurrent AirPlay preference, and alert/ringtone categories. It then compares HostApplicationDisplayID with `com.spotify.client` at `0x1974a0bb0` and sets the deny value to one. The matching diagnostic is:

```text
Disallowing buffered for Spotify on session %p for category %@
```

Disassembly confirms this branch works even with logging disabled: `0x1974a0bcc` jumps to `0x1974a0ca0` (`MOV W22,#1`), then `0x1974a0ccc` writes the result to route storage byte `+132`. When the value changes, `0x1974a0cd4` calls `airplayRoute_updateRoutingRegistry`. There is a similar explicit ESPN app check. There is no extra category exemption within the Spotify branch.

`airplayRoute_updateRoutingRegistryInternal`, `0x19749fdf0`, unregisters the prior route-client/context pair, then registers this client only when:

```text
routingContextUUID exists
AND playbackActive (+64)
AND !flag(+144)
AND !coremedia preference fpap_disableBuffered
AND !audioSessionDisallowsBuffered (+132)
AND !flag(+131)
```

The two unnamed flags were not assigned speculative meanings. Spotify's deny byte is therefore an actual registration gate. This is not a global purge of every client's registration.

## Accessory capability and source eligibility are separate

`AirPlaySender!airPlayDescription_isCarPlaySpatialAudioSupported`, `0x2278bb498`, requires:

1. `APSSettingsGetIntWithDefault("bufferedAPACSurround", 1)` to be nonzero.
2. Description storage byte `+65` to be nonzero (the project's earlier description audit identifies this as buffered support).
3. Type-103 / `media` extended audio formats containing at least one of indices `65, 66, 67, 68, 76, 77, 92`.

The format set comes directly from bounds `index-65 <= 27` and bit mask `0x800180f`. The setting key at `0x2838250e8` was decoded from the original cache. Helper `0x227704630` supplies default one; stub `0x22c825530` branches to `AirPlaySupport!APSSettingsGetIntWithDefault` at `0x22798dcb0`. Thus this specific gate defaults enabled and is not itself the Music Dolby Atmos preference.

`carEndpoint_setupStreams`, `0x2277ff304`, only constructs its buffered stream when endpoint storage byte `+58` is enabled. `carEndpoint_setupAudioStream`, `0x227805624`, then retrieves type-103/media formats and creates `APAudioEngineBufferedCreate` through an audio-engine vendor followed by `APEndpointStreamBufferedAudioCreate`. Constructing an available stream object is distinct from starting its network/audio transport.

`interruptibleStreamWrapper_resumeOrSuspendSubstreamAsNecessary`, `0x2277f12cc`, queries the buffered-client count for a routing context. A positive count blocks the wrapped substream. `interruptibleStreamWrapper_bufferedClientUpdateNotificationCallback`, `0x2278aedfc`, reevaluates it when the notification's context matches. This confirms active client/context state participates in arbitration; it does not identify the active client in the user's live session.

Separately, `MediaToolbox!audioRendererAirPlay_updateBufferedAirPlayOutput`, `0x1973dd1e8`, creates a buffered output when it has an endpoint stream and its `BufferedAirPlayAllowed` byte `+77` is true. This is another reason to scope the Spotify finding to the inspected route policy instead of declaring all playback APIs impossible.

## Vehicle evidence and interpretation

In `logs/captures/audio-current-20260919-224000/carplay_hook.log`:

- `00:53.663`: phone requests stream type 103.
- `00:53.995`: APAC decoder opens formatIndex 65, 48 kHz, 6 channels.
- `01:09.508`: type 103 is torn down.
- Through `07:58`, buffered write count stays at 32 and ringRead stays zero while ordinary stereo writes continue.

Earlier Spotify captures (`spotify-audio-20260919-221449`, `spotify-recheck-20260919-221724`) contain no type-103 start; `audio-recheck-20260919-221955` later contains an APAC type-103 start after the user's Music/Atmos experiment. Receiver capability and working transport are therefore demonstrated, but application attribution and the phone's private route state are not.

The supported interpretation is that switching playback/client state can change eligibility. A one-time Atmos unlock, a particular global Music preference, and persistence across reconnect have not been established. Format 65 is APAC 5.1; its presence does not by itself prove Atmos metadata remains present downstream or that HSSR stereo upmix runs.

For a complete live causal attribution, capture the phone's FigAirPlay_Route diagnostics (HostApplicationDisplayID, AudioSessionDisallowsBuffered, routingContextUUID and buffered client count) alongside HU SETUP/TEARDOWN while switching Spotify -> Music stereo -> Music Atmos -> Spotify. Music stereo should be tested separately to distinguish app/renderer eligibility from content format. Receiver-only traces cannot settle those phone-side fields.

## Implication for this project

Changing only the HU's advertised audio formats does not remove the verified phone-side Spotify client-registration gate. Reliable local upmix of the ordinary stereo path would need an implementation on that path. The current `spa_pcm_expand_fallback` path copies L/R and leaves the other output channels zero; the HSSR pipeline is currently attached to buffered stereo playback. No runtime/source behavior was changed by this audit.
