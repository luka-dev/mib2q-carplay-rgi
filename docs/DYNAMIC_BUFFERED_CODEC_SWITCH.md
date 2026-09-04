# Dynamic APAC/AAC switching in one buffered CarPlay session

Date: 2026-09-03

Target: MHI2Q MU1316, iOS 27 build 24A5418b, buffered stream type 103

Status: sender mechanism proved statically; receiver dispatch implemented and host-tested; the
APAC/AAC/APAC transition still needs one stationary vehicle run.

## Result

An Atmos item followed by an ordinary stereo item does **not** require a new CarPlay session or a
second QSA endpoint. iOS contains an in-session transport-format-change path for its buffered audio
engine. The source format is examined again when it changes, a compatible transport format is
chosen from the receiver's advertised list, and the changed format plus its magic cookie is carried
in APAP.

The intended advertisement is:

```text
audioOutputFormatsExtended: [65, 23]
```

In the current supervisor this is enabled for a stationary trial with
`AUDIO_SPATIAL=1`, `AUDIO_SPATIAL_ATMOS=0` and `SPATIAL_103_EXTRA_INDEX=23`. The checked-in startup
default intentionally leaves the extra index empty until the cross-codec vehicle transition passes.

Index 65 is APAC/48 kHz/5.1 and index 23 is AAC-LC/48 kHz/stereo. The legacy mask remains the
compatibility declaration; the extended array supplies the actual modern format identities.

The receiver must treat SETUP's `audioFormatIndex` as the initial format only. Its authoritative
runtime input is the index in each APAP extension. On a change it destroys the old decoder with the
old decoder's own API, selects the new API, and creates the new decoder from the cookie travelling
with that packet. The QSA/DIO side remains fixed at 48 kHz, six-channel S16: AAC stereo is widened to
`L,R,0,0,0,0`; APAC 5.1 uses the measured decoder-to-DIO permutation.

## What was read directly from iOS

### Selection is based on the current source channel count

In iOS 27 `AirPlaySender`,
`_audioStream_copyTransportFormatDescriptionForSourceContentFormatDescription` at
`0x251029dd0` first filters the endpoint's supported formats and then asks AirPlaySupport to select
a transport description for the source description.

The selector in `AirPlaySupport`,
`_APSAudioFormatDescriptionListCopyTransportFormatDescriptionForSourceFormatDescription` at
`0x226654c88`, reads `mChannelsPerFrame` from ASBD offset `+0x1c` at `0x226654d6c`. For a six-channel
source it branches to `0x226654e48`, creates extended index 65 at
`0x226654e60..0x226654e6c`, and checks that exact format against the receiver's supported list at
`0x226654e78..0x226654e84`.

For a stereo source it falls through the stereo candidate chain. APAC stereo index 64 is tried only
when `bufferedAPACStereo` is enabled; with its default false, the chain reaches the normal
ALAC/AAC candidates, including AAC-LC 48 kHz index 23. This is content-format selection, not array
ordering: putting 23 after 65 does not make it override the six-channel branch.

### The active buffered engine can change transport format

`_bufferedAudioEngine_enqueueMessagesFromBufferQueue` at `0x2510611a8` compares the incoming source
format with its cached source description and obtains a new transport description when it changes.
It compares the chosen transport with the cached transport around `0x251061d3c..0x251061d48`, stores
the replacement at `engine+0x4d0` around `0x2510621e0..0x2510621ec`, and logs the live transition at:

```text
0x251062444  ADRP X1, ...  ; bufferedAudioEngine_enqueueMessagesFromBufferQueue
0x251062450  ADRP X3, ...
0x251062454  ADD  X3, X3, #0xe92
                            "Transport audio format changed: %{asbd} = %@"
0x251062460  BL   <log>
```

AirPlaySupport's receiver side has the matching machinery:

```text
kAPSEndpointStreamAudioHoseSBARNotification_AudioFormatChanged
hoseSBAR_scheduleFormatChangeHandlingForAudioSessionOnTimer
hoseSBAR_handleFormatChangeForAudioSession
"TimedManagerTimerTriggerNotification: AudioFormat changed to %s"
```

That path schedules the audio-session change at the stream time. It is evidence for a format change
inside an existing SBAR/APAP hose, not an RTSP teardown/re-SETUP cycle.

### APAP makes the packet self-describing

The vehicle's opened index-23 packets measured this extension prefix:

```text
02 01 17                         field 2: format index 23
11 08 <eight bytes>              field 17: magic-cookie ID
12 27 <39 cookie bytes>          field 18: complete magic cookie
00                               end of extension; compressed AU follows
```

Thus a boundary packet brings all information needed to construct its decoder. The receiver does
not need to guess from track metadata or wait for `SETMAGICCOOKIE` ordering. Field 8 remains accepted
as an older/alternate encoding, but field 2 with a one-byte value is the measured form.

## Receiver state machine

All operations stay on the existing APAP receiver thread; no audio thread is added.

```text
APAP packet
  -> authenticate/decrypt
  -> parse formatIndex + cookieId + inline cookie
  -> same index/cookie: decode normally
  -> changed index/cookie:
       destroy old handle through old codec API
       select API: 65 -> APAC, 23 -> Cinemo AAC-LC
       create new handle from this packet's cookie
       decode this packet
  -> normalize to six-channel/48-kHz/S16
  -> append to the existing PCM ring in packet order
```

The ring is deliberately not cleared merely because the codec changes. It may still contain the
tail of the previous item; preserving it preserves submission order and avoids manufacturing a gap.
A real seek/skip is already represented by `FLUSHBUFFERED`, which resets the decoder and ring at its
explicit boundary.

Unsupported indices fail closed: their packets remain consumed so the CarPlay session survives,
but no PCM is emitted. A later supported index can rebuild and resume. Index 22 remains unsupported
for production rendering until a 44.1-to-48-kHz resampler is attached; index 23 avoids that path.

## Implementation status and corrected ownership bug

The packet-driven dispatch is in `hook/audio/audio_hook.c`; the measured TLVs are parsed in
`hook/audio/apap_crypto.c`; Cinemo AAC-LC is adapted in `hook/audio/aac_bridge.c`.

The first switch implementation had one cross-codec lifetime error: it assigned the new API before
destroying the old opaque handle. An AAC-to-APAC boundary could therefore call APAC `destroy` on a
Cinemo AAC object, or the reverse. The order is now:

1. save `previous = codec`;
2. call `previous->destroy(decoder)`;
3. clear the old handle;
4. publish `formatIndex` and the next API;
5. create from the new inline cookie.

The host APAP integration test now uses the vehicle's actual field-2/field-17/field-18 shape and
checks that the inline cookie survives parsing byte-for-byte. The QNX build still performs the
emutls, constructor, C++ runtime and optional-APAC dependency checks.

## Reconciling the earlier `[65,23]` vehicle observation

The earlier run logged index 23 for the initial buffered session and did not observe a return to 65
for the item described as Atmos. That observation remains real, but it does not prove the format is
session-constant: the code above proves the active engine has a change path and chooses index 65
when the source ASBD presented to it has six channels.

What the old run did not record is the source ASBD at that item and field 2 on every boundary packet.
There are therefore two live possibilities:

* the item reached this sender path as stereo, in which case index 23 was the correct selection and
  no change was requested;
* the source became six-channel and iOS emitted 65, in which case the older receiver failed to
  observe or act on the one-byte field-2 index.

Do not infer either one from the Music badge. `DolbyAtmos` badging and the source ASBD are separate
objects in this code.

## One decisive stationary test

Advertise `[65,23]` and play a verified native-Atmos item, an ordinary stereo item, then the Atmos
item again. The necessary log sequence is:

```text
stream switched format index 23 -> 65; rebuilding the decoder
decoder up ... 48000 Hz 6 ch
stream switched format index 65 -> 23; rebuilding the decoder
decoder up ... 48000 Hz 2 ch -- widened to the sink width
stream switched format index 23 -> 65; rebuilding the decoder
```

Also require continuous `framesIn/framesOut`, zero authentication/TLV failures and no new underrun.
If packet field 2 stays 23 throughout, the HU cannot force automatic Atmos selection: iOS is
presenting that playback as stereo, and the remaining problem is sender-side content eligibility,
not receiver codec switching. If the indices alternate, this test closes the architecture without
a CarPlay reconnect.

## Confidence ledger

| Claim | Confidence | Basis |
|---|---|---|
| iOS has an in-session buffered transport-format change path | high | active-engine and timed SBAR code/strings |
| a six-channel source selects index 65 when it is advertised | high | direct ASBD channel load, branch, literal 65 and membership test |
| stereo can select index 23 | high | static candidate chain plus vehicle-selected index 23 |
| APAP carries the new index and complete cookie | high | authenticated vehicle packet capture |
| the HU can rebuild APAC/AAC from a boundary packet | high statically | implemented dispatch, recovered decoder ABIs and host tests |
| Apple Music will expose every Atmos-labelled item as six-channel here | unknown | requires the one stationary boundary log above |
