# Dynamic APAC/AAC switching in one buffered CarPlay session

Date: 2026-09-03; SAD live-update audit added 2026-09-04

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

That fixed PCM geometry must not be confused with the independent SAD speaker-allocation
sideband. With Spatial armed, the current hook creates the PCM stream once and announces the
six-active-channel SAD once. A later APAP index change rebuilds only the decoder; it does not
currently change the SAD header.

This is now the adopted shipping policy, not a missing live-update feature: keep one
`48 kHz / PCM6 / S16` QSA stream and one six-active-channel SAD for the complete buffered
session. Index 65 decodes into the six native positions; index 23 decodes stereo and widens it
to `L,R,0,0,0,0`. Do not stop/reconfigure/start CSD merely because an APAP packet changes codec.
The SoundAmp audit proved that `ASAD=0xc0` would not select its stereo matrix bank on sink `0x26`
anyway; both `0xc0` and `0xfc` remain in bank 1 with SHARC control `5.0`.

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

## SAD speaker allocation: exact values and update lifetime

This is a separate state machine from APAP codec selection. Stock MHI2Q
`libairplay.so` contains an explicit channel-count-to-ASAD table at VA/file offset `0x000a9ff0`:

| declared channels | ASAD | active-position shape |
|---:|---:|---|
| 1 | `0x20` | one non-L/R position in the stock table |
| 2 | `0xc0` | positions 1,2: L/R |
| 3 | `0xe0` | positions 1..3 |
| 4 | `0xcc` | L/R plus the stock surround pair |
| 5 | `0xec` | L/R/C plus the stock surround pair |
| 6 | `0xfc` | positions 1..6: L/R/C/LFE/Ls/Rs |
| 7 | `0xef` | stock seven-position allocation |
| 8 | `0xff` | all eight positions |

The unlocked `CQNXSoundIF::createMetadata` checks `channels-1 <= 7` and reads the table with a
four-byte stride at `0x00047568..0x0004757c`; the locked instantiation performs the same lookup at
`0x0004889c..0x000488b0`. The two values relevant here are therefore read directly, not inferred
from a popcount:

```text
2 channels -> ASAD 0xc0 = 11000000
6 channels -> ASAD 0xfc = 11111100
```

Both implementations send the 24-byte metadata object with devctl `0x8018fa01`:
`0x000475e8..0x000475fc` and `0x0004891c..0x00048934`. The current hook intercepts these functions
at stream creation and substitutes six channels/24 bits while Spatial is armed.

The devctl is **not a live LPASS update**. In stock `deva-ctrl-qc.so`, the SET branch of
`pcm_devctl` at `0x0003f8a8..0x0003f908` only copies the metadata fields into the persistent MPL
session and branches to the common return path. There is no CSD call in that branch. The actual
custom-header configuration is emitted later by `session_csd_start_playback`:

```text
0x0006a030  test persistent custom-header enable
0x0006a048  convert_sad_header_to_csd(session, payload)
0x0006a058  set_device_tdm_sad_header(csd_device, stream, payload)
```

`set_device_tdm_sad_header @ 0x00029d70` constructs `CSD_DEV_CMD_CONFIGURE` and reaches
`csd_ioctl` at `0x00029eac`. The stock image calls this in the playback-start sequence, before the
device is enabled; no stock path was found which exercises it as an in-flight track-format update.
Consequently, calling `createMetadata(2,...)` on an APAP `65 -> 23` boundary would only arm
`ASAD=0xc0` for the **next** CSD start. It would not alter the header already emitted by the running
stream. The reverse is equally true for `23 -> 65`.

### Does changing ASAD select the amplifier's stereo matrix?

The amplifier-side audit now answers this exactly. Its V850 polls the live INIC/MOST allocation,
stores a changed ASAD at `gp-0x3e21`, and schedules task-9 event 15. That event re-evaluates the
matrix bank and rewrites the relevant SHARC parameter groups. ASAD can remap a raw-bank-2 sink:
`0xc0` selects bank 0, `0xfc` selects bank 1, and an eight-position allocation can retain bank 2.

The active CarPlay sink `0x26`, however, has raw bank 1. It produces bank 1 for both values:

```text
Sink 0x26 + ASAD 0xc0 -> bank 1, SHARC control 0x10 = float 5.0
Sink 0x26 + ASAD 0xfc -> bank 1, SHARC control 0x10 = float 5.0
```

Bank 0 has a separate control value `2.0`, but the firmware provides no semantic name for that
control. Calling it Symphoria or an upmix enable would be inference. The visible bank-0 matrix is a
two-bus fold-down, not an L/R-to-surround upmixer. Profile-0 bank 1 maps stereo PCM
`L,R,0,0,0,0` to Bus L/Bus R at -2 dB and also to Bus C at -6 dB per side; it has no direct L/R
route to the surround buses. See
[`AMP_STEREO_5_1_MATRIX_DIFF.md`](AMP_STEREO_5_1_MATRIX_DIFF.md) for the code path and complete
coefficient comparison.

Therefore a live SAD toggle is neither free nor justified for the shipping path. The PCM contract
remains six slots with `ASAD=0xfc` for the duration of the mixed APAC/AAC buffered session. The
following two **separate-start** stereo trials remain available only as an allocation diagnostic:

1. index 23 with `AUDIO_SAD_CHANNELS=6`;
2. index 23 with `AUDIO_SAD_CHANNELS=2`.

Verify the start slog changes from `asad=0xfc` to `asad=0xc0`, then compare actual speaker use. This
does not enter the separate bank-0/control-`2.0` path; it isolates ASAD-aware calibration and any
unobserved downstream behavior while keeping the current sink. Do
not play index 65 in the second trial: `0xc0` intentionally marks C/LFE/Ls/Rs inactive. If the two
stereo trials sound the same, live SAD switching has no benefit. Even if `0xc0` changes presentation
through hidden allocation-aware processing, the shipping mixed-codec session deliberately does not
restart CSD at a format boundary. Merely calling the existing metadata devctl is insufficient, and
a restart would add an avoidable click/gap between tracks.

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
