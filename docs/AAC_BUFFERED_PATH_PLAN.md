# MHI2Q buffered AAC-LC path: static contract and implementation record

Date: 2026-09-03

Scope: MHI2Q MU1316, CarPlay buffered stream type 103, AAC-LC indices 23 and 22

Status: live framing/cookie capture and static ABI/thread audit complete;
cookie-driven APAC/AAC dispatch implemented and confirmed in both directions on
the vehicle. The stereo ambience/LFE profile has a separate stationary A/B gate.

## Result

The production AAC route is **format index 23: AAC-LC, 48000 Hz, stereo**. It was selected on the
vehicle, its APAP fixed header carried a time scale of `0x0000bb80` (48000), and its magic cookie was
a separate 39-byte value with a distinct ID. It therefore needs an AAC decoder but **no sample-rate
converter**. Index 22 (44100 Hz stereo) remains a fallback only.

The static boundary is sufficiently recovered to write the decoder adapter without more ABI
guessing:

* APAP copies the sample buffer's compressed media bytes verbatim. It adds transport framing,
  encryption and optional extension TLVs, but does not add ADTS or LOAS/LATM framing.
* The MHI2Q Cinemo AAC plugin's `AUDIO_ES` path passes those bytes directly to its AAC elementary
  stream decoder. It does not search for an ADTS or LATM sync word on this path.
* The working ARMv7 MH2p adapter submits one compressed access unit per Cinemo `Write` call.
* `CinemoCreateAudioCodec` and `NmeCreateCodecAAC` allocate a new wrapper and a new AAC codec object
  for each call. There is no codec-object singleton.
* The target plugin expects a raw MPEG-4 `AudioSpecificConfig` at `CinemoMediaType.header_data`. It
  starts reading object type at bit zero; the 39-byte descriptor chain must not be passed intact.
* The captured 39-byte MPEG-4 descriptor chain contains a two-byte ASC `11 90`, proving
  AAC-LC/stereo/48 kHz. Its descriptor lengths use the long `80 80 80 xx` representation, so the
  parser must walk tags and lengths rather than assume a fixed ASC offset.
* A captured 12.821333-second run contains 601 raw AUs, 376..1322 bytes each. Adding only ADTS
  headers lets a standard decoder produce exactly `601 * 1024` frames of 48 kHz stereo PCM.
* The final Cinemo concurrency audit found independent decoder/output objects, a mutex-protected
  shared plugin registry, no OMX calls and a fully synchronous callback chain. One decoder may be
  owned by the existing APAP receiver thread; no extra thread or callback wait is needed.

The earlier `[65,23]` run appeared not to return to 65 because fields 2/8 were
misread as a format index. Independent AAC/23 and APAC/65 sessions both carried
`1024` there: they are `spf`, not an `APAudioFormatIndex`. SETUP's index remains
only an initial hint. A changed inline cookie is tried against the current codec
and then the alternate; the successful codec plus decoded geometry/layout is
authoritative.

The decoder now retains both APIs, and the vehicle has played APAC -> AAC and
AAC -> APAC inside one session without dropout or new underrun. The complete
mechanism, exact iOS addresses and receiver state machine are in
[`DYNAMIC_BUFFERED_CODEC_SWITCH.md`](DYNAMIC_BUFFERED_CODEC_SWITCH.md).

## Exact target and provenance

The implementation target is MHI2Q MU1316, not MH2p. MH2p is used only as an ARMv7/QNX call-site
oracle.

| Artifact | Role | SHA-256 |
|---|---|---|
| MU1316 `armle/usr/lib/libNmeSDK.so` | public Cinemo C wrappers | `a34a968e10b5193d9bd8ee80abd89d29e987d1b573a72f44d85865fd11ace73e` |
| MU1316 `armle/usr/lib/cinemo/libNmeAudioAAC.so` | target AAC decoder plugin | `89536dcdfddeab67e98ce8945fc7e4346fa6d71da0de170f8c36d98475a6cd46` |
| MU1316 `eso/bin/apps/dio_manager` | hook host process | `e9862680d76ca2e6bb2eed067658f2ca8fa9b71b5446322384b76c9735c0e794` |
| MU1316 `armle/usr/lib/libNmeBaseClasses.so` | internal `NmeResample` implementation | `d69b568cc93759d44c7ac9dfd94d069ee52d7fc28829e9feb1086627d244505b` |
| MU1316 `armle/usr/lib/cinemo/libNmeAudioRenderer.so` | stock caller of `NmeResample` | `b0cdc4752c55c3b1e2067327c10a4c07b57f3c98493b6ee94c3598bf9d53432e` |
| MU1316 `armle/usr/lib/libspeex.so` | stable C resampler fallback | `36526f8b9c155e5d310cd57f75ae288f4fee88f9dd3de47283916d65aac83eb6` |
| MH2p P2712 `app/eso/lib/libairplay.so` | working ARMv7 AAC adapter oracle | `f94f1b661be0ff9c1131dbaa2c74c7fe924cd7548e7eb794b5b8b44f15ddc5dc` |
| iOS 27 build 24A5418b `AirPlaySupport` | APAP and format/cookie producer | `edf99982e576d46309471638f565a36e7eb326fb78bdb2bd75eba119a992c2a3` |

The target AAC plugin identifies itself as `NmeAudioAAC 1.79.33`, built 2017-12-19 for QNX 6.5.
`dio_manager` already has `libNmeSDK.so` in `DT_NEEDED`. Its production configuration points
`iap2.cinemoLib` to `/armle/usr/lib/cinemo`, and `cinemo_classes.xml` registers `NmeCodecAAC` and
`NmeCodecAACReference` from `NmeAudioAAC`. The hook must use this existing process configuration;
it must not create a second global Cinemo configuration or change the plugin directory.

## 1. APAP framing and the AAC payload

### What is directly read from iOS

In `AirPlaySupport`:

| Function | VA | Finding |
|---|---:|---|
| `APSAPAPBBufEncode` | `0x2266ccfc8` | gives the sample's `CMBlockBuffer` to `APSAPAPEncode` |
| `_APSAPAPBBufEncodeMediaDataCallback` | `0x2266cd28c` | walks every contiguous block-buffer region and writes exactly those bytes |
| `_APSAPAPBBufDecodeMediaDataCallback` | `0x2266cd6a0` | accepts one media-data item and exposes it as a block buffer without reframing |

At `0x2266cd2e0` the encoder obtains each `CMBlockBuffer` segment; at `0x2266cd2f0` it sends the
exact pointer and length to the APAP writer. There is no code between them which constructs an ADTS,
LOAS or LATM header. This proves that **APAP itself does not determine the AAC framing**: it carries
whatever the upstream compressed sample buffer already contains.

The outer format already implemented by the hook is:

```text
BE32 inclusive record length
signed BE64 mediaTimeValue
signed BE32 mediaTimeScale
BE24 sequence number
ciphertext || Poly1305 tag[16] || nonce[8]
```

The first 15 bytes after the length are the APAP fixed header. Thus `0xAC44` and `0xBB80` observed
at fixed-header bytes 8..11 are the media-time scales 44100 and 48000; they say nothing about ADTS
versus raw AAC.

### What is directly read from MHI2Q and the ARMv7 donor

In target `libNmeAudioAAC.so`:

* `NmeCodecAAC_Write` at `0x4dc8` checks the input media type.
* For media type 5 (`AUDIO_ES`) or 7 it branches directly at `0x4df4` to
  `NmeCodecAAC_DecodeElementaryStream` at `0x4540`.
* The decoder consumes the caller's byte pointer and length at `0x4698`. The ADTS/LATM sync-search
  branches are used only by other input media types.

The MH2p adapter's `CAudioDecoder::decode` at `0xb2874` forwards the one compressed packet obtained
from `AudioConverterFillComplexBuffer` directly to Cinemo `Write` at `0xb28a0`. It does not prepend
a transport header.

Together these were strong static evidence that index 23 supplies **one raw AAC-LC access unit per
APAP media item**. The vehicle capture in `captures/aac_route_20260903` now proves it directly.

### Measured vehicle capture

The hook authenticated and opened index-23 media items before any decoder was introduced. It
retained each AU as a four-byte little-endian length followed by the exact compressed payload:

| Measurement | Result |
|---|---:|
| selected format | index 23 |
| access units | 601 |
| AU payload range | 376..1322 bytes |
| compressed payload bytes | 423276 |
| AAC duration | 12.821333 seconds |
| first observed AU | `21 4e aa fb ...` |
| decoded output | 48000 Hz, stereo, S16, exactly `601 * 1024` frames |

There is no `0xfff` ADTS or `0x2b7` LOAS sync. `tools/aac_es_to_adts.py` adds an ADTS header from the
captured ASC without removing or changing any AU byte; the resulting stream decodes normally. This
settles both boundaries: one APAP media item is one raw AAC-LC AU, and that complete AU is the input
to Cinemo `Write`.

The retained `shk` is not documented. The derived AU corpus, ADTS wrapper, decoded WAV and cookie
are sufficient for offline implementation and regression testing.

## 2. Magic cookie contract

### Read directly from iOS

`APAudioFormatGetMagicCookieForAACLC` at `0x22669e680` is only a default-cookie helper for 48 kHz
surround AAC-LC:

* it returns without a cookie for any rate other than 48000 at `0x22669e6c4`;
* at 48000 it fills a cookie only for six or eight channels at `0x22669e6e4..0x22669e71c`;
* stereo index 22 or 23 therefore does not receive a generated default from this helper.

The surrounding hose code has a separate cached-format path at `0x22666443c`; that path retains a
cookie supplied by the incoming `CMAudioFormatDescription`. `AirPlaySender` obtains it with
`CMAudioFormatDescriptionGetMagicCookie`, copies the exact bytes to CFData, hashes those bytes into
the cookie ID, and sends the value through `SETMAGICCOOKIE`. This explains why the live index-23
session had its own 39-byte cookie even though the stereo default helper is empty.

### Read directly from MHI2Q

Target `libNmeAudioAAC.so` applies header type 5 in `AACDecoder_ApplyAudioSpecificConfig` at
`0x5d08`. It rejects a null header, a null output, or fewer than two bytes. Its parser then:

1. initializes an MSB-first bit reader at byte zero (`AAC_BitReaderInit`, `0x228ec`);
2. reads the first five bits as `audioObjectType` (`0xfba8`);
3. reads four bits of sampling-frequency index (`0xfbc8`), or an explicit 24-bit rate for index 15;
4. reads four bits of channel configuration (`0xfbe8`) and rejects values above seven.

There is no scan for an `esds` atom or MPEG-4 descriptor tag. Therefore the decoder's
`header_data` must point to the extracted ASC, not to the complete Apple cookie.

For the two relevant formats, independently calculated ASC prefixes are:

| index | object/rate/channels | ASC | donor-padded form |
|---:|---|---|---|
| 23 | AAC-LC / 48000 / 2 | `11 90` | `11 90 00 00` |
| 22 | AAC-LC / 44100 / 2 | `12 10` | `12 10 00 00` |

### Measured cookie structure

The retained cookie is a 39-byte MPEG-4 descriptor chain, not an APAC `dapa` atom:

```text
03 80 80 80 22 00 00 00
04 80 80 80 14 40 14 00 18 00 00 01 f4 00 00 01 f4 00
05 80 80 80 02 11 90
06 80 80 80 01 02
```

Tag `0x03` contains the ES descriptor, tag `0x04` its decoder configuration, tag `0x05` the
two-byte decoder-specific information/ASC `11 90`, and tag `0x06` the SL configuration. Lengths are
encoded in legal long form (`80 80 80 xx`). A fixed byte offset happens to work for this one cookie
but is not a valid parser.

The implementation must fail closed and accept either:

* a directly validated raw ASC; or
* a size-checked MPEG-4 descriptor chain whose variable-length lengths are parsed,
  containing exactly one decoder-specific-info tag `0x05` under the decoder-config descriptor.

The extracted ASC must decode to object type 2, the negotiated rate, two channels and a 1024-sample
AAC-LC frame. APAC `dapa`, missing/duplicate tag 5, unsupported object type, wrong rate/channel count
or trailing malformed descriptors are fatal setup errors. Cookie IDs remain byte-identity keys;
an AAC cookie must never be interpreted by the APAC decoder and vice versa.

## 3. Exact Cinemo call contract on MHI2Q

### Public wrappers

All calls should be resolved by symbol from the already loaded `libNmeSDK.so`; no target-private
vtable address belongs in production code.

| Export | VA | Exact call shape |
|---|---:|---|
| `CinemoCreateAudioCodec` | `0x4ea44` | `int32_t (void **out_codec)` |
| `ICinemoAudioCodec_Initialize` | `0x9eedc` | `int32_t (codec, context, callback, input, output)` |
| `ICinemoAudioCodec_Write` | `0x9ef00` | `int32_t (codec, AU, bytes, arg3, int64_t pts70mhz, arg6)` |
| `ICinemoAudioCodec_Drain` | `0x9ef2c` | `int32_t (codec)` |
| `ICinemoAudioCodec_Flush` | `0x9ef40` | `int32_t (codec)` |
| `ICinemoAudioCodec_Release` | `0x9eeb4` | releases the interface reference |

`CinemoCreateAudioCodec` allocates a fresh 112-byte wrapper, queries it for
`ICinemoAudioCodec::iid`, releases the temporary reference and returns a `CinemoError`; zero is
success and direct allocation failure is 12.

`Initialize` stores the callback at wrapper `+0x50` and its context at `+0x54`, creates a decoder
selected by `input.media_subtype`, then creates/verifies the output PCM path. `NmeCreateCodecAAC` at
target `0x4a20` allocates a fresh 7536-byte AAC object for each class-factory request.

### `CinemoMediaType` values for index 23

Both structures are zero-initialized and exactly `0x80` bytes. The 32-bit target pointer lives at
`+0x78`.

| Offset | AAC input | PCM output |
|---:|---:|---:|
| `+0x00` media type | `5` (`AUDIO_ES`) | `2` (`PCM`) |
| `+0x04` subtype | `0x101a` (AAC-LC) | `0x1023` (PCM) |
| `+0x08` sample format | `0` | `2` (S16) |
| `+0x0c` sample rate | `0` | `48000` |
| `+0x10` channels | `0` | `2` |
| `+0x74` header type | `5` (ASC) | `0` |
| `+0x78` header data | pointer to extracted ASC | `0` |
| `+0x7c` header size | validated ASC byte count | `0` |

The input rate and channel fields are zero in the working donor because ASC carries them. The output
request is explicit 48 kHz, two-channel, interleaved S16.

Initialize as:

```c
CinemoCreateAudioCodec(&codec);
ICinemoAudioCodec_Initialize(codec, state, decoded_callback, &input, &output);
```

The working donor calls each AU as:

```c
ICinemoAudioCodec_Write(codec, access_unit, access_unit_bytes, 1, 0, 1);
```

The two one-valued arguments have no recovered semantic names, so they should remain neutral names
such as `mh2p_arg3` and `mh2p_arg6`. Earlier notes that proposed `(0, 0, 0)` were wrong; Thumb
instructions at donor `0xb2888`, `0xb2892..0xb289a` and the indirect call at `0xb28a0` prove
`(1, pts=0, 1)`. APAP timing is already handled by the receiver/ring clock, so the first adapter
should keep donor PTS zero rather than invent a conversion to 70.56 MHz.

The callback ABI is:

```c
int32_t decoded_callback(void *context, const void *pcm, uint32_t pcm_bytes,
                         const CinemoMediaType *format,
                         int64_t pts70mhz, uint32_t flags);
```

For this route it must report PCM media type 2, subtype `0x1023`, S16 format 2, 48000 Hz and two
channels. AAC-LC normally produces 1024 frames per AU. Target decode code can consume partial input
and can issue zero or multiple callbacks, so the adapter must accumulate callback data in a bounded
FIFO and not assume a single synchronous callback solely from that nominal frame size.

The final MHI2Q call-chain audit proves that this callback is synchronous for AAC plus `NmeEncodePCM`.
It can occur zero or multiple times during one `Write`, but cannot arrive later on a Cinemo worker
thread because neither plugin creates one. The MH2p adapter's 100 ms condition-variable wait at
`0xb348a` is unnecessary here and must not be copied.

## 4. Thread and coexistence decision

The static coexistence risk is closed in
[`CINEMO_AAC_THREAD_SAFETY_AUDIT.md`](CINEMO_AAC_THREAD_SAFETY_AUDIT.md):

* `dio_manager` imports Cinemo Config/Metapool/iAP helpers but not `CinemoCreateAudioCodec`; no
  stock public audio-codec object exists in this process;
* wrapper, AAC decoder and PCM output objects are newly allocated per factory request;
* the global class/plugin registry serializes initialize/load/register/lookup and reference-counts
  loaded modules;
* despite its `DT_NEEDED`, the AAC plugin has no `OMX_*` import or relocation and performs no
  dynamic name lookup; this software path never obtains an OMX component;
* AAC decode, PCM pass-through and the application callback form one direct call chain ending at
  `libNmeSDK.so:0x4e710` before `Write` returns.

Use exactly one AAC decoder per type-103 stream generation. The existing APAP receiver thread can
own all `Create`/`Initialize`/`Write`/`Flush`/`Release` calls; no new hook thread is required. Calls
on one Cinemo instance must be serialized.

The decoded callback runs inline. It may validate the generation/output format, copy into a bounded
decoder-owned PCM FIFO and update counters. It must not call Cinemo recursively, write QSA, or take
socket/QSA/shared-render locks.

In particular, do not hold `spa103_render.lock`, the APAP socket lock, or the QSA sink lock across
`Initialize`, `Write`, `Flush` or `Release`. Snapshot the session generation/state, execute Cinemo
under AAC-private ownership, then move complete PCM frames into the shared ring after `Write`
returns. This avoids reentrant lock inversion while keeping the direct callback cheap.

On malformed callback format, FIFO overflow or Cinemo error, count and drop/mute that AU. Do not
feed compressed bytes or unvalidated PCM into DIO5, and do not tear down `dio_manager` from a decoder
callback. Do not invoke Cinemo from the preload constructor or an ELF destructor; initialization and
controlled release belong to the active stream lifetime.

## 5. Resampling decision

There is no resampler in the production index-23 path. Decode directly to 48000 Hz stereo, then use
the existing widening rule before the six-channel 48 kHz ring:

```text
AAC PCM:  L R
DIO PCM:  L R 0 0 0 0
```

Do not fan stereo into center/surround channels. `spa_pcm_expand_fallback` already preserves L/R and
zero-fills the remaining slots; the amplifier owns the downstream distribution.

If index 23 is later found unusable and index 22 must be retained, 44.1-to-48 conversion belongs
inside the AAC bridge **after decode and before the shared render ring**. That keeps every queue,
clock, DIO fragment and underrun counter downstream in the endpoint's native 48 kHz frame domain.
The device/QSA sink cannot be used as a transparent 44.1 kHz converter because the established HU
media endpoints accept 48 kHz only.

Two implementations already exist on the unit:

1. `libNmeBaseClasses.so` exports the internal C++ `NmeResample` class (`Create` at `0xbe278`,
   `Write` at `0xbfdfc`, `Drain` at `0xbf188`, `Flush` at `0xbde18`). Stock
   `libNmeAudioRenderer.so` constructs it from `NmeAudioRenderer_RecreateResampler` at `0x5960`.
   It proves Cinemo already resamples decoded audio, but its virtual callback and options structures
   are private C++ ABI and are a poor hook boundary.
2. `libspeex.so` exports the stable C SpeexDSP API, including `speex_resampler_init_frac` at
   `0x232c0`, `speex_resampler_process_interleaved_int` at `0x21eb8`, reset and destroy. It is the
   preferred fallback adapter if index 22 is ever needed.

For that fallback use interleaved S16 stereo, the exact rational ratio `160/147`, Speex desktop
quality 5, and retain fractional phase across AUs. A 1024-frame input AU yields a varying 1114/1115
output frames, not a fixed packet size. Reset both the PCM FIFO and resampler memory on
`FLUSHBUFFERED`. Quality 8 may be compared offline, but it should not replace quality 5 unless a
stationary deadline/CPU test shows comfortable headroom. None of this code belongs in the index-23
production path.

## 6. Implemented receiver sequence

The landed implementation follows this order:

1. The AAC bridge has the same lifecycle shape as `apac_bridge.h`: create/get-info/decode-S16/
   reset/destroy. Resolve all six Cinemo wrappers from `RTLD_DEFAULT`; do not import target-private
   addresses or initialize a second Cinemo configuration.
2. Parse and validate each changed cookie with the current decoder and then its alternate. For AAC, extract the actual ASC from raw
   ASC or a validated descriptor wrapper. Keep the ASC allocation alive for the entire decoder
   session. The wrong codec rejects the cookie; decoded rate/channels/layout, not the stale SETUP index, validate the result.
3. Build the exact input/output `CinemoMediaType` structures above and initialize one decoder for
   the stream generation. Check every return value and actual callback format.
4. At the render boundary, classify the successful codec and decoded PCM: 48 kHz stereo enters the
   stereo renderer; APAC 5.1 with tag `0x007c0006` enters the measured reorder. The unmeasured
   PCM8/5.1.2 path remains reachable only through the pre-existing SETUP=66 experiment.
5. Feed each authenticated APAP media item to Cinemo as one AU using `(1, 0, 1)`. Collect zero or
   more callback chunks in the bounded private FIFO, enforce frame alignment, and emit complete
   stereo frames only.
6. Render PCM2 into the established PCM6 ring. `AUDIO_STEREO_UPMIX=off` is exact
   `L,R,0,0,0,0`; the checked-in `ambient-lite` canary emits trimmed L/R, zero C, bounded LR4/80 Hz
   LFE assist and decorrelated Ls/Rs. Native APAC 5.1 never enters this module.
7. On `FLUSHBUFFERED`, stop accepting callbacks for the old generation, call `Flush`, empty the AAC
   FIFO and shared PCM queued before the flush boundary, and resume with the same ASC. On cookie or
   format change, flush/release/recreate. Use `Drain` only for orderly EOS, not for a discontinuity.
8. Teardown runs on the owner thread, prevents another `Write`, releases the Cinemo interface
   exactly once, then frees ASC/FIFO state. Do not rely on an ELF destructor, whose order relative
   to Cinemo's global class-factory teardown is undefined. Normal teardown must be idempotent.
9. Add bounded counters: AU/bytes in, callbacks, PCM frames, `Write` errors and maximum/slow `Write`
   duration, bad callback formats, stale-generation callbacks and FIFO overruns. Do not retain or
   log playable PCM in a production build.

Both decoders now coexist in the receiver. The vehicle has switched ordinary AAC-LC and spatial
APAC in both directions inside one session with continuous playback and no new underrun. Only the
new stereo ambience/LFE acoustic profile still needs its separate stationary A/B.

## 7. Historical rollout ladder and completed gate

This table preserves the risk order used to land the decoder. Gates 0–7 describe
the staged rollout state at the time; gate 8 is the accepted vehicle closure.
The only current open validation is the separate stereo ambience/LFE acoustic
A/B in the upmix design document.

| Gate | Test | Risk to current vehicle state | Drive? |
|---:|---|---|---|
| 0 | Keep the then-known-good bundle and `AUDIO_SPATIAL=0`; record hashes before any test. | none | no |
| 1 | **Complete:** retained cookie and 601-AU corpus prove the descriptor chain, ASC `11 90`, one raw AU per APAP item and standard decode to exactly `601 * 1024` frames. | none | no |
| 2 | Host tests with fake Cinemo functions: raw/descriptor-chain cookie parsing, zero/multiple callbacks, malformed geometry, slow/failed synchronous `Write`, flush, stale generation and FIFO bounds. Run the existing APAC tests unchanged. | none | no |
| 3 | Integrate the wrapper behind a disabled gate and keep `AUDIO_SPATIAL=0`; verify ordinary CarPlay and existing APAC host/bench regressions. A separate-process Cinemo probe is not the relevant coexistence test because it has a different registry. | low | no |
| 4 | With stock local playback stopped, run one bounded in-process create/decode/flush/release canary on the existing APAP thread. Require 48 kHz/2ch/S16, sane callback/frame counts and no process fault. | low-medium; reversible hook generation | no |
| 5 | Before and after that decoder lifetime, replay stock local audio. This checks the only remaining runtime concern despite the absence of shared codec/OMX state. | low-medium | no |
| 6 | In a fresh stationary session advertise index 23 only. Play ordinary material; require clean auth/AU/decode/PCM/ring counters, audible L/R, zero-filled extra slots, and correct pause/seek/flush/cookie replacement. | medium, one LD_PRELOAD generation; rollback is the known bundle/env switch | no |
| 7 | In a separate session advertise `[65]` and replay the proven Atmos case. APAC hashes, channel order, feedback and six-channel counters must remain unchanged. | low-medium regression test | no |
| 8 | **Complete:** advertise `[65,23]` and switch ordinary AAC-LC and spatial APAC in both directions. Require changed cookies, matching decoder rebuilds, continuous playback and clean counters; fields 2/8 must remain ignored `spf=1024`. | medium | no |

No decoder/framing/resampling gate requires a moving vehicle. A drive is optional only for final
subjective acoustic acceptance after stationary counters and speaker tests pass. Do not combine the
first Cinemo coexistence experiment with a drive.

## Confidence ledger

| Finding | Confidence | Basis |
|---|---|---|
| index 23 is AAC-LC/48000/stereo and is selected by the tested phone | high/measured | iOS enumeration plus vehicle SETUP/APAP scale |
| index 23 removes the need for 44.1-to-48 resampling | high | selected 48000 format and fixed 48 kHz HU endpoint |
| APAP itself copies codec bytes without adding ADTS/LATM | high/direct | iOS encode/decode media callbacks |
| payload is one raw AAC-LC AU per APAP media item | high/measured | 601 authenticated AUs; ADTS wrapping without stripping decodes the full corpus |
| 39-byte index-23 cookie differs from APAC cookie | high/measured | distinct live length and ID; APAC cookies are `dapa` atom chains |
| 39-byte cookie is an MPEG-4 ES/DecoderConfig/DecSpecificInfo/SL descriptor chain | high/measured | retained bytes and parsed nested tags/long-form lengths |
| Cinemo `header_data` must be raw ASC | high/direct | target bit parser begins at header bit zero |
| index-23 ASC is `11 90` | high/measured | retained tag-5 payload plus successful standard decode at 48000 Hz stereo |
| separate codec instances are structurally supported | high/direct | fresh SDK wrapper and fresh AAC object per factory call |
| one receiver-thread instance is structurally safe beside stock Cinemo state | high/direct | no stock codec factory user; locked registry; fresh objects; synchronous callback |
| AAC route shares an OMX component with stock playback | refuted | no `OMX_*` imports/relocations or dynamic symbol lookup |
| real-time CPU margin/vendor runtime behavior | unmeasured | bounded stationary in-process canary |
| iOS can switch the buffered transport within one session | high/direct | active-engine transport-change branch plus timed SBAR receiver path |
| cookie-driven AAC/APAC switching works in both directions | high/measured | accepted vehicle run, no dropout/new underrun |
| decoded PCM action is independent of stale SETUP index | high/host-tested | SETUP 65/23 switch regression, native reorder and fresh stereo DSP state |

No static discovery item remains. The framing, cookie, ABI, ownership/thread contract and dynamic
sender mechanism are closed. Everything after this point is bounded validation, not another
firmware-wide search. The dynamic state machine is in
[`DYNAMIC_BUFFERED_CODEC_SWITCH.md`](DYNAMIC_BUFFERED_CODEC_SWITCH.md); the detailed concurrency
evidence and FDK rollback boundary are in
[`CINEMO_AAC_THREAD_SAFETY_AUDIT.md`](CINEMO_AAC_THREAD_SAFETY_AUDIT.md).
