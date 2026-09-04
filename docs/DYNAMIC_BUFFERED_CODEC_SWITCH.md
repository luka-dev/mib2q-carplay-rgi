# Dynamic APAC/AAC switching in one buffered CarPlay session

Date: 2026-09-03; SAD live-update audit added 2026-09-04

Target: MHI2Q MU1316, iOS 27 build 24A5418b, buffered stream type 103

Status: sender mechanism proved statically; cookie-driven receiver dispatch is
implemented, host-tested and confirmed on the vehicle in both directions. The
new optional stereo ambience/LFE profile still needs its own stationary A/B.

## Result

An Atmos item followed by an ordinary stereo item does **not** require a new CarPlay session or a
second QSA endpoint. iOS contains an in-session transport-format-change path for its buffered audio
engine. The source format is examined again when it changes and a compatible
transport is chosen from the receiver's advertised list. APAP does not carry an
`APAudioFormatIndex`; it carries a changed inline magic cookie, and that cookie
selects the decoder at the receiver.

The intended advertisement is:

```text
audioOutputFormatsExtended: [65, 23]
```

The current supervisor enables this accepted configuration with
`AUDIO_SPATIAL=1`, `AUDIO_SPATIAL_ATMOS=0` and
`SPATIAL_103_EXTRA_INDEX=23`.

Index 65 is APAC/48 kHz/5.1 and index 23 is AAC-LC/48 kHz/stereo. The legacy mask remains the
compatibility declaration; the extended array supplies the actual modern format identities.

The receiver treats SETUP's `audioFormatIndex` only as the initial decoder hint;
it can remain unchanged for the life of the session. On a new cookie, the hinted
decoder is tried first. If its validated `create` rejects the cookie, the other
decoder is tried and retained. The successful codec plus decoded channel count,
sample rate and layout tag—not the stale SETUP index—select PCM processing. The
QSA/DIO side remains fixed at 48 kHz, six-channel S16: supported stereo enters
the configured stereo renderer, while APAC 5.1 uses the measured decoder-to-DIO
permutation.

That fixed PCM geometry must not be confused with the independent SAD speaker-allocation
sideband. With Spatial armed, the current hook creates the PCM stream once and announces the
six-active-channel SAD once. A later inline-cookie/codec change rebuilds only the decoder; it
does not currently change the SAD header.

This is now the adopted shipping policy, not a missing live-update feature: keep one
`48 kHz / PCM6 / S16` QSA stream and one six-active-channel SAD for the complete buffered
session. APAC decodes into the six native positions; AAC-LC decodes stereo and
is normalized into the same PCM6 rail. With the renderer off this is exactly
`L,R,0,0,0,0`; the checked-in canary instead adds bounded LFE/Ls/Rs. Do not
stop/reconfigure/start CSD merely because an APAP cookie changes codec.
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

### APAP makes the packet self-configuring, but carries no format index

Two authenticated vehicle sessions settled the ambiguous fields: one negotiated
AAC-LC stereo/index 23 and one APAC 5.1/index 65, yet fields 2 and 8 both carried
`1024`. That is the SETUP `spf` (samples per frame), not an
`APAudioFormatIndex`. Reading either as an index overwrote the negotiated value
and muted the stream.

Field 17 carries the magic-cookie ID and field 18 carries the complete inline
cookie. Thus a boundary packet still brings everything required to construct
and identify its decoder without depending on `SETMAGICCOOKIE` ordering: AAC's
MPEG-4 descriptor and APAC's GlobalConfig are mutually rejected by the wrong
decoder and accepted by the right one. Fields 2/8 are parsed and skipped.

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
two-bus fold-down, not an L/R-to-surround upmixer. Profile-0 bank 1 maps the stereo
compatibility/off baseline `L,R,0,0,0,0` to Bus L/Bus R at -2 dB and also to Bus C at -6 dB per side; it has no direct L/R
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
  -> skip fields 2/8 (samples-per-frame), parse cookieId + inline cookie
  -> same cookie: decode normally
  -> changed cookie:
       destroy old handle through old codec API
       try current API, then the alternate API if create rejects the cookie
       classify successful decoder + channels/rate/layout
       decode this packet
  -> stereo: configured PCM2 -> PCM6 renderer
  -> APAC 6ch/tag 0x007c0006: measured native-order permutation
  -> APAC 8ch/tag 0x00c20008: legacy policy only when SETUP itself was 66
  -> append to the existing PCM ring in packet order
```

The ring is deliberately not cleared merely because the codec changes. It may still contain the
tail of the previous item; preserving it preserves submission order and avoids manufacturing a gap.
A real seek/skip is already represented by `FLUSHBUFFERED`, which resets the decoder and ring at its
explicit boundary.

Unsupported cookies/layouts fail closed: their packets remain consumed so the
CarPlay session survives, but no PCM is emitted. A later supported cookie can
rebuild and resume. Index 22 remains unsuitable for the fixed rail until a
44.1-to-48-kHz resampler is attached; index 23 avoids that path.

## Implementation status and corrected ownership bug

The packet-driven dispatch is in `hook/audio/audio_hook.c`; the measured TLVs are parsed in
`hook/audio/apap_crypto.c`; Cinemo AAC-LC is adapted in `hook/audio/aac_bridge.c`.

The first switch implementation had one cross-codec lifetime error: it assigned the new API before
destroying the old opaque handle. An AAC-to-APAC boundary could therefore call APAC `destroy` on a
Cinemo AAC object, or the reverse. The order is now:

1. save `previous = codec`;
2. call `previous->destroy(decoder)`;
3. clear the old handle;
4. try `create` with the current API, then the alternate API;
5. retain only the API whose decoder accepted the inline cookie.

The host APAP integration test uses the vehicle's field-2/field-17/field-18
shape, verifies that field 2 does not populate a format index, and checks the
inline cookie byte-for-byte. `buffered_pcm_switch_test` then holds simulated
SETUP at both 65 and 23 while exercising AAC -> APAC and APAC -> AAC: stereo
must select the upmixer with nonzero LFE/rears, APAC 5.1 must select the measured
reorder, and every cookie reset must equal a fresh DSP state. The QNX build still
performs the emutls, constructor, C++ runtime and optional-APAC dependency checks.

The APAC 5.1.2 decoder PCM order is still unmeasured. Cookie classification may
recognize its declared tag for diagnostics, but general 65/23 switching cannot
select the 8-to-6 fold. Only the pre-existing experimental SETUP=66 path may do
so; the checked-in `[65,23]` policy cannot enter it.

## Vehicle closure and the earlier misleading observation

The earlier run treated field 2 as index 23 and therefore appeared never to
return to 65. That interpretation is superseded: the value was samples per
frame, and the SETUP index is not a track-by-track trace.

After the cookie-driven correction, the vehicle played ordinary AAC-LC stereo
and spatial APAC in both directions inside one type-103 session, without a
dropout or new underrun. This closes codec switching. The next stationary test
is narrower: listen to the new `ambient-lite` LFE-off/assist profiles and verify
an Atmos -> stereo -> Atmos sequence has no DSP-tail click while native 5.1
speaker placement remains unchanged.

## Confidence ledger

| Claim | Confidence | Basis |
|---|---|---|
| iOS has an in-session buffered transport-format change path | high | active-engine and timed SBAR code/strings |
| a six-channel source selects index 65 when it is advertised | high | direct ASBD channel load, branch, literal 65 and membership test |
| stereo can select index 23 | high | static candidate chain plus vehicle-selected index 23 |
| APAP carries no per-packet format index; fields 2/8 are `spf=1024` | high | independent authenticated AAC/23 and APAC/65 vehicle sessions |
| inline cookie identifies and configures the active decoder | high | mutually validating cookie formats, implementation and host tests |
| the HU switches APAC/AAC in both directions without reconnect/dropout | high | accepted vehicle run plus clean counters |
| decoded PCM dispatch is independent of stale SETUP index | high statically | classifier/action regression for SETUP 65 and 23, both orders |
| Apple Music exposes every Atmos-labelled item as six-channel here | unknown | Music badge and source ASBD remain distinct objects |
