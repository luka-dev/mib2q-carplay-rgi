# iOS 27 extended audio-format indices and CarPlay buffered-stream selection

**Status:** static audit complete, 2026-09-03; vehicle/implementation correction
added 2026-09-04. The original audit itself modified no target binary, hook, or
deployed configuration. Its `[64,65]` proposal is retained as history and is
superseded by the accepted cookie-driven `[65,23]` implementation in §7.

This audit corrects one statement in the older spatial-audio handoff: APAC has a
stereo format. It is extended format index **64**. The spatial formats begin at
index 65.

The correction does not by itself prove that adding 64 will make this particular
phone send stereo APAC. iOS guards APAC stereo with the setting
`bufferedAPACStereo`, queried with a default value of false. The setting's runtime
override on the test phone is not recoverable from the static shared cache.

## 1. Result

The buffered CarPlay entry at the time of this audit was:

```text
type: 103
audioType: media
audioOutputFormats: 0x400000
audioOutputFormatsExtended: [65]
```

It describes two independent things:

* `audioOutputFormats = 0x400000` is legacy **index/bit 22**,
  `AAC-LC/44100/stereo`. It is also the single AAC-family compatibility value
  required by iOS when it constructs the type-103 CarPlay engine.
* `audioOutputFormatsExtended = [65]` is the actual list used by the modern
  buffered format selector. Index 65 is only `APAC/48000/5.1`.

When `SupportedAudioFormatsExtended` exists, the buffered endpoint constructs its
supported-format list from `bufferStream` in that property. It consults the legacy
`SupportedAudioFormats` mask only if the extended property/list is absent. Thus the
legacy bit 22 does **not** silently add AAC stereo to the current extended list.

The smallest APAC-only experiment is therefore:

```text
audioOutputFormats: 0x400000
audioOutputFormatsExtended: [64, 65]
```

This is a valid, normal multi-index shape and keeps the existing six-channel APAC
path. It lets stereo use APAC index 64 **only if** `bufferedAPACStereo` is enabled
at runtime.

The format-list shape that does not depend on that hidden APAC-stereo flag is:

```text
audioOutputFormats: 0x400000
audioOutputFormatsExtended: [22, 65]
```

Here stereo is buffered AAC-LC at 44.1 kHz and 5.1 is APAC at 48 kHz. This matches
the shape of Apple's receiver-side list builder: ordinary AAC/ALAC stereo formats
and spatial APAC formats coexist in one list. It was **not safe for the
then-current HU implementation**, because its type-103 data path dispatched only
to the APAC decoder and had no AAC type-103 decoder. Adding 22 could therefore
make iOS send a codec that path could not decode.

For the then-current APAC-only implementation, `[64,65]` was the only narrowly scoped
negotiation test worth making. It was not yet a playable repository configuration:
`spa103_render_start` at that time accepted only indices 65 and 66 and classified 64
as `unsupported-format`. Keep the working realtime type-100 entry intact. If the
phone does not select 64, the static result predicts that its
`bufferedAPACStereo` override is off; metadata alone then cannot guarantee both
stereo and spatial playback. The two honest alternatives are to keep spatial as a
separate mode, or implement codec dispatch for buffered AAC and advertise
`[22,65]`.

## 2. Directly read from iOS

The source is the iOS 27 build `24A5418b`, iPhone model cache `iPhone18,1`:

```text
Refferences/ios27_dsc/24A5418b__iPhone18,1/
```

Relevant extracted images:

```text
AirPlaySupport  SHA-256 edf99982e576d46309471638f565a36e7eb326fb78bdb2bd75eba119a992c2a3
AirPlaySender   SHA-256 fc18fcf63503e2382488b37a9e77e1e21a432fe0c4d853482be421d1190fa61e
```

`AirPlaySender` was extracted from the cache with `ipsw dyld extract`, with slide,
stubs, and Objective-C metadata enabled. All addresses below are loaded virtual
addresses in those extracted arm64e images.

### Enumeration functions in AirPlaySupport

| Function | VA | What was read |
|---|---:|---|
| `_APAudioFormatIndexToASBD` | `0x22669646c` | 93-case range for indices 2 through 94: codec FourCC, rate, bits, packet size, and channel count |
| `_APAudioFormatIndexToChannelLayoutTag` | `0x226696ed4` | channel-layout tag for each valid index |
| `_APSAudioFormatDescriptionCopyDebugString` | `0x2266d7438` | Apple's own normalized `codec/rate/layout` label |
| `_APAudioFormatIndexToAPAudioFormat` | `0x226698434` | `index < 64 ? (1ULL << index) : 0` |
| `_APSAudioFormatDescriptionGetLegacyFormatMask` | `0x2266d78b4` | same legacy-mask rule, read from a format description |

The first switch subtracts 2 and bounds the result at `0x5c`; indices 0 and 1
are therefore invalid, as are the explicit empty cases 36, 37, and 59.

### APAC entries

Every row below is a direct result of the three enumeration functions above.
Confidence is **high** for every index. There are no inferred APAC indices.

| Index | Codec/rate/layout | Channels | Core Audio layout tag | Confidence |
|---:|---|---:|---:|---|
| **64** | `APAC/48000/2` | 2 | `0x00650002` (`Stereo`: L R) | high, direct |
| **65** | `APAC/48000/5.1` | 6 | `0x007c0006` (`MPEG_5_1_D`: C L R Ls Rs LFE) | high, direct |
| **66** | `APAC/48000/5.1.2` | 8 | `0x00c20008` (`Atmos_5_1_2`) | high, direct |
| **67** | `APAC/48000/7.1` | 8 | `0x00800008` (`MPEG_7_1_C`) | high, direct |
| **68** | `APAC/48000/7.1.4` | 12 | `0x00c0000c` (`Atmos_7_1_4`) | high, direct |
| **76** | `APAC/48000/5.1.4` | 10 | `0x00c3000a` (`Atmos_5_1_4`) | high, direct |
| **77** | `APAC/48000/7.1.2` | 10 | `0x00c4000a` (`Atmos_7_1_2`) | high, direct |
| **92** | `APAC/48000/9.1.6` | 16 | `0x00c10010` (`Atmos_9_1_6`) | high, direct |

Index 64 is the only APAC format below 5.1. There is no mono APAC entry in this
enumeration.

The layout tag for index 65 literally names the compressed stream's order as
`C L R Ls Rs LFE`. This does not contradict the already-proven HU PCM order. The
APAC decoder/cookie normalization produces the HU's canonical interleaved order
`L R C LFE Ls Rs` before QSA.

### The complete index enumeration

This table is the complete accepted range of the iOS function, not a list inferred
from codecs found elsewhere. Descriptions are normalized from Apple's debug-string
formatter; `32f` means 32-bit float PCM. Confidence is **high/direct for all valid
rows** and **high/direct for the five invalid/reserved values**.

| Index | Codec / sample rate / sample format / layout |
|---:|---|
| 0 | invalid/reserved |
| 1 | invalid/reserved |
| 2 | `PCM/8000/16/mono` |
| 3 | `PCM/8000/16/stereo` |
| 4 | `PCM/16000/16/mono` |
| 5 | `PCM/16000/16/stereo` |
| 6 | `PCM/24000/16/mono` |
| 7 | `PCM/24000/16/stereo` |
| 8 | `PCM/32000/16/mono` |
| 9 | `PCM/32000/16/stereo` |
| 10 | `PCM/44100/16/mono` |
| 11 | `PCM/44100/16/stereo` |
| 12 | `PCM/44100/24/mono` |
| 13 | `PCM/44100/24/stereo` |
| 14 | `PCM/48000/16/mono` |
| 15 | `PCM/48000/16/stereo` |
| 16 | `PCM/48000/24/mono` |
| 17 | `PCM/48000/24/stereo` |
| 18 | `ALAC/44100/16/stereo` |
| 19 | `ALAC/44100/24/stereo` |
| 20 | `ALAC/48000/16/stereo` |
| 21 | `ALAC/48000/24/stereo` |
| 22 | `AAC-LC/44100/stereo` |
| 23 | `AAC-LC/48000/stereo` |
| 24 | `AAC-ELD/44100/stereo` |
| 25 | `AAC-ELD/48000/stereo` |
| 26 | `AAC-ELD/16000/mono` |
| 27 | `AAC-ELD/24000/mono` |
| 28 | `OPUS/16000/mono` |
| 29 | `OPUS/24000/mono` |
| 30 | `OPUS/48000/mono` |
| 31 | `AAC-ELD/44100/mono` |
| 32 | `AAC-ELD/48000/mono` |
| 33 | `QC3/48000/5.1.2` |
| 34 | `QC3/48000/7.1.4` |
| 35 | `QC3/48000/9.1.6` |
| 36 | invalid/reserved |
| 37 | invalid/reserved |
| 38 | `PCM/48000/16/5.1.2` |
| 39 | `AAC-LC/48000/5.1` |
| 40 | `AAC-LC/48000/5.1.2` |
| 41 | `AAC-ELD/48000/5.1` |
| 42 | `AAC-ELD/48000/5.1.2` |
| 43 | `AAC-ELD/32000/mono` |
| 44 | `PCM/48000/16/5.1` |
| 45 | `PCM/48000/32f/mono` |
| 46 | `PCM/48000/32f/stereo` |
| 47 | `PCM/48000/32f/5.1` |
| 48 | `PCM/48000/32f/5.1.2` |
| 49 | `DDPLUS/48000/stereo` |
| 50 | `DDPLUS/48000/5.1` |
| 51 | `DDPLUS/48000/5.1.2` |
| 52 | `DDPLUS/48000/7.1.4` |
| 53 | `DDPLUS/48000/9.1.6` |
| 54 | `QAAC/48000/stereo` |
| 55 | `QAAC/48000/5.1` |
| 56 | `QAAC/48000/5.1.2` |
| 57 | `QAACHE/48000/stereo` |
| 58 | `QAACHE/48000/5.1` |
| 59 | invalid/reserved |
| 60 | `QAACHE/48000/5.1.2` |
| 61 | `QLAC/48000/24/stereo` |
| 62 | `QC3/48000/stereo` |
| 63 | `QC3/48000/5.1` |
| 64 | `APAC/48000/stereo` |
| 65 | `APAC/48000/5.1` |
| 66 | `APAC/48000/5.1.2` |
| 67 | `APAC/48000/7.1` |
| 68 | `APAC/48000/7.1.4` |
| 69 | `PCM/48000/32f/7.1.4` |
| 70 | `QAAC/44100/stereo` |
| 71 | `QAACHE/44100/stereo` |
| 72 | `QAACHEV2/44100/stereo` |
| 73 | `QLAC/44100/24/stereo` |
| 74 | `MP3/44100/stereo` |
| 75 | `MP3/48000/stereo` |
| 76 | `APAC/48000/5.1.4` |
| 77 | `APAC/48000/7.1.2` |
| 78 | `PCM/48000/16/7.1` |
| 79 | `PCM/48000/32f/7.1` |
| 80 | `PCM/48000/16/5.1.4` |
| 81 | `PCM/48000/32f/5.1.4` |
| 82 | `PCM/48000/16/7.1.2` |
| 83 | `PCM/48000/32f/7.1.2` |
| 84 | `PCM/48000/16/7.1.4` |
| 85 | `ALAC/44100/20/stereo` |
| 86 | `ALAC/48000/20/stereo` |
| 87 | `QAC3/48000/5.1` |
| 88 | `QEC3/48000/7.1` |
| 89 | `PAAC/44100/stereo` |
| 90 | `AAC-LC/48000/7.1` |
| 91 | `EAC3/48000/5.1` |
| 92 | `APAC/48000/9.1.6` |
| 93 | `PCM/48000/16/9.1.6` |
| 94 | `PCM/48000/32f/9.1.6` |

## 3. Legacy mask versus the extended list

The relationship is exact and simple:

```c
APAudioFormatIndexToAPAudioFormat(index) {
    return index < 64 ? (1ULL << index) : 0;
}
```

Therefore:

| Extended index | Legacy bit/mask | Meaning |
|---:|---:|---|
| 22 | bit 22 / `0x400000` | AAC-LC, 44.1 kHz, stereo |
| 23 | bit 23 / `0x800000` | AAC-LC, 48 kHz, stereo |
| 64 and every APAC index | **none / 0** | extended-only; no 64-bit legacy representation |

No legacy bit pairs one-to-one with APAC 64, 65, 66, 67, 68, 76, 77, or 92.
For a type-103 CarPlay entry the legacy mask is a separate compatibility/bootstrap
field. `carPlayAudioFormat_repackageFormatsForStream` requires the type-103 media
legacy family to have FourCC `aac ` (`0x61616320`). That is why the known-good
single bit 22 can accompany any of the APAC indices. It does not declare the APAC
channel count.

`APCarPlayAudioFormatsCopyFormatsForAudioType` proves that indices below 64 may
also appear in `audioOutputFormatsExtended`: it copies the whole array and, when
`carplayForce44kForBuffered` is enabled, explicitly removes numeric index 23 from
that array while clearing legacy bit 23. An extended list such as `[22,65]` is
therefore a native shape, not a protocol abuse.

## 4. Multiple entries are normal

This is direct, high-confidence evidence rather than an inference from the field's
plural name.

`_APSAudioFormatDescriptionListAddSupportedFormatsForAPACToList` at
`0x226654af0` appends these values to one list, in this order:

```text
64 (only when bufferedAPACStereo is enabled),
65, 66, 67, 76, 77, 68, 92
```

`_APSAudioFormatDescriptionListCreateForBufferedReceiverWithAudioCapabilities`
at `0x226656144` builds one receiver list containing stereo AAC/ALAC indices and
spatial APAC indices. Its base sequence starts with 22 and 23, optionally adds
20/86/21, then adds spatial formats 39/40 and APAC 65/66; capability-dependent
branches add 67/76/77/68/92. That receiver builder does **not** add APAC stereo
64.

On the CarPlay side:

* `_APCarPlayAudioFormatsCopyFormatsForAudioType` at `0x25107b070` mutable-copies
  the entire `audioOutputFormatsExtended` array.
* `_airPlayDescription_copyCarPlayAudioFormatsExtended` at `0x25109c5dc` copies
  the whole type-103/media array to the `bufferStream` property.
* `_airPlayDescription_isCarPlaySpatialAudioSupported` at `0x2511541c8` loops
  over the array until it sees any of 65, 66, 67, 68, 76, 77, or 92. Index 64 is
  deliberately not a spatial-gate value.

Consequently `[64,65]` remains spatial-capable because 65 satisfies the gate,
while 64 supplies an APAC stereo candidate when its separate sender flag allows
it.

## 5. How type 100 and type 103 are selected

### What is directly established

`_APCarPlayAudioFormatsCreate` at `0x25107b82c` packages type 100 and type 103
separately. Type 103 accepts exactly the one audio type in
`kMainBufferedAudioTypes`: `media`.

The inspected cache does not contain a literal `/info` document from a shipping
Apple receiver. What it proves is the parser/constructor shape: type 100/media
and type 103/media are separate records, while the type-103 record may contain a
multi-value extended array. The statement that an Apple accessory emits those
two exact records is therefore an inference from the constructors, not a captured
Apple `/info` payload.

`___carEndpoint_activateInternal_block_invoke_2.134` at `0x2510873d8` always
creates the ordinary main CarPlay audio stream first. If the negotiated buffered
state byte is set, it then creates the additional buffered stream. Therefore a
type-103 advertisement does not delete or overwrite type 100.

`_carEndpoint_setupAudioStream` at `0x25108c080` creates the type-103 object with
`APEndpointStreamBufferedAudioCreate`. The realtime entries use
`APEndpointStreamCarPlayAudioCreate`; they are distinct endpoints.

`_audioStream_createSupportedAudioFormatList` at `0x2510224ac` behaves as follows:

1. If `SupportedAudioFormatsExtended.bufferStream` exists, create the supported
   list from that array and return it.
2. Only if it is absent, create the list from the legacy
   `SupportedAudioFormats.bufferStream` mask.

For each source content description,
`_audioStream_copyTransportFormatDescriptionForSourceContentFormatDescription`
at `0x251029dd0` filters that active stream's supported list and calls the format
selector. The selector in AirPlaySupport at `0x226654c88` chooses by source
channel count. For stereo it tries APAC 64 only when both the APAC feature and
`bufferedAPACStereo` are enabled, then tries ALAC 20/86/21 and AAC-LC 23/22.
If none is present, it returns an error. This code never switches to another
endpoint.

The selection of a transport description is therefore re-evaluated for source
content, but only within the already-active stream. It is not a per-item
type-103-to-type-100 fallback mechanism.

### What is inferred from the static boundary and the vehicle behavior

The higher media policy that initially elects the buffered endpoint is outside
these format helpers. The strongest static boundary is:

* type 100 and 103 both exist;
* any spatial APAC index makes
  `airPlayDescription_isCarPlaySpatialAudioSupported` true;
* once the buffered stream asks for a stereo transport and its list has only 65,
  the selector returns failure rather than changing endpoints.

This matches the vehicle observation that `[65]` makes Music hunt through the
catalogue for spatial material rather than falling back track-by-track. It does
not prove which Music-layer object makes the first endpoint choice, nor whether
all iOS builds apply identical catalogue policy.

Changing the buffered entry's `audioType` is not a valid fix. The iOS table for
type 103 contains only `media`; using another value makes it unrecognized rather
than reserving type 100 for stereo.

## 6. Confidence and historical safe action

| Finding | Confidence | Basis |
|---|---|---|
| APAC stereo is index 64 | high | direct ASBD/debug-string/layout enumeration |
| APAC spatial indices are 65/66/67/68/76/77/92 | high | direct enumeration plus the spatial-gate mask |
| APAC indices have no legacy bit | high | two direct `index < 64` implementations |
| Multi-index extended arrays are normal | high | Apple list builders and array iteration |
| `[64,65]` is a syntactically and semantically valid sender offer | high | direct list-builder order and selector cases |
| This phone will actually choose 64 | medium/conditional | guarded by `bufferedAPACStereo`; static default is false and runtime override is unknown |
| The current HU will render index 64 | unmeasured on vehicle | current cookie/geometry classifier would send decoded APAC stereo to the common upmixer, but iOS has not selected 64 |
| `[22,65]` supplies buffered stereo independent of the APAC-stereo flag | high | direct stereo selection order and receiver-list builder |
| The current HU can render index 22 on type 103 | refuted without a resampler | the AAC bridge can identify/decode 44.1 kHz, but the fixed 48 kHz render validation rejects it |
| The sender will fall back from an active type 103 to type 100 per item | low/refuted at the inspected layer | format-selection error is returned without an endpoint switch |

The following was the safe vehicle order proposed before index 23 and the AAC
bridge were measured. It is retained only as experiment history:

1. Preserve the complete known-good type-100 entry and all current feedback/SAD
   fixes.
2. For a negotiation-only trace, change only the extended array from `[65]` to
   `[64,65]`; do not change the legacy `0x400000` mask. The then-current render code would
   reject 64 cleanly, so this step cannot be an audible acceptance test.
3. Confirm the selected index and codec before enabling rendering. If iOS selects
   64, add and bench an explicit index-64 APAC stereo path; do not assume the
   existing six-channel output geometry is correct for it.
4. If it does not select 64, revert or keep spatial as an explicit mode. Do not add
   22 until buffered AAC type-103 dispatch is implemented and fail-closed.

This audit authorizes no deployed change. It establishes the exact metadata
choices and their limits so the next vehicle action is a single-field test rather
than another format-index guess.

### Superseded working-tree warning found during this audit

At audit time the working tree contained an uncommitted change that permitted index
22 as `SPATIAL_103_EXTRA_INDEX`. That change only alters advertisement. The same
tree's `spa103_render_start` rejects every format except APAC 65/66, so index 22
will become `unsupported-format`; it does not add AAC decode. Do not deploy that
advertisement as a stereo fix without a real type-103 AAC dispatch and tests.

## 7. Vehicle correction: index 23 and cookie-driven switching — 2026-09-04

Later vehicle measurements and the completed receiver supersede the safe-action
proposal above:

* index 64 was not selected, consistent with `bufferedAPACStereo` being false;
* index 23 was selected as AAC-LC/48000/stereo, with APAP time scale `0xBB80`
  and a separate 39-byte magic cookie;
* an early `[65,23]` trace appeared to select 23 for the entire session only
  because the receiver misread APAP field 2 as a format index;
* independent AAC/23 and APAC/65 sessions both carry `1024` in fields 2 and 8:
  this is SETUP `spf`, and APAP carries no per-packet format index;
* the changed inline magic cookie selects AAC or APAC, and decoded
  channels/rate/layout selects stereo upmix or the measured APAC 5.1 reorder;
* the vehicle has now switched APAC -> AAC and AAC -> APAC inside one type-103
  session with continuous playback and no new underrun.

Therefore index 23, not 22, is the AAC production format and requires no
resampler. SETUP's index is only the initial decoder hint; it may stay stale
across a track change. The vehicle result is authoritative for this phone/build.

Decoder branches must not be format-index keyed. The receiver tries the current
codec and then its alternate against each changed cookie, retains only the one
that accepts it, and classifies the resulting PCM. The complete implementation
record is [`AAC_BUFFERED_PATH_PLAN.md`](AAC_BUFFERED_PATH_PLAN.md); the exact
state machine and vehicle evidence are in
[`DYNAMIC_BUFFERED_CODEC_SWITCH.md`](DYNAMIC_BUFFERED_CODEC_SWITCH.md).
