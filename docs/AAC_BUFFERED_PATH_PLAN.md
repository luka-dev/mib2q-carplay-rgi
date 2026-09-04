# MHI2Q buffered AAC-LC path: static contract and implementation plan

Date: 2026-09-03

Scope: MHI2Q MU1316, CarPlay buffered stream type 103, AAC-LC indices 23 and 22

Status: static analysis and plan only; no hook, firmware, or deployed binary changed

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
  starts reading object type at bit zero; a 39-byte `esds` wrapper must not be passed there intact.
* For index 23 the standard AAC-LC/stereo/48 kHz ASC is `11 90`; the donor-compatible padded form is
  `11 90 00 00`. This value was calculated from the ISO bit fields and matches the target parser,
  but the live cookie still has to be captured and parsed before production code may use it.

One policy limitation is now measured, not hypothetical: iOS chooses the buffered format **once for
the session**. Advertising `[65,23]` made it choose 23 for all content; it did not switch back to APAC
65 for an Atmos item. The decoder implementation must retain both APAC and AAC dispatches, but that
does not by itself create automatic per-track spatial/stereo switching. Until a separate
renegotiation mechanism is proved, the honest modes remain:

* type 100 / `AUDIO_SPATIAL=0`: universal stereo, current safe default;
* type 103 / `[65]`: discrete APAC 5.1 for spatial material, ordinary items skipped;
* type 103 / `[23]` (after the decoder exists): expected universal buffered stereo, including an
  expected stereo rendering of an Atmos item, but not discrete spatial playback. Selection is
  measured; audible decode is not yet measured.

This limitation supersedes the earlier claim that the format was selected per track.

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

Together these are strong static evidence that index 23 supplies **one raw AAC-LC access unit per
APAP media item**. This remains high confidence rather than a live-proven fact because the index-23
APAP capture from the vehicle has not been copied into the repository.

### Minimal capture that settles it

No drive is needed. In one short stationary index-23 session, preserve:

1. the type-103 SETUP request and selected `audioFormatIndex=23`;
2. the exact 32-byte `shk` locally (never commit it or print it in documentation);
3. `/tmp/spatial_apap_<sequence>.bin` for two to five seconds;
4. `/tmp/spatial_magiccookie_<sequence>_<id>.bin`, expected to be the measured 39 bytes;
5. the log lines pairing TLV 17's cookie ID with that file.

Use `tools/pioneer_apap_decrypt.py --output-dir` on the host. It already validates the inclusive
BE32 records, both legal AAD variants and Poly1305 before emitting plaintext. Classify each media
item after any extension TLVs:

* ADTS: 12-bit `0xfff` sync, normally `ff f1` or `ff f9`, with a self-contained 7/9-byte header and
  frame length;
* LOAS/LATM: 11-bit `0x2b7` sync, normally beginning `56 e?`;
* raw access unit: no stable transport sync and the APAP media-item boundary is the AU boundary.

The decisive positive test is not only the absence of a sync word: extract the ASC from the cookie,
submit each authenticated media item to Cinemo, and obtain 1024 interleaved stereo frames per AAC-LC
AU without stripping any bytes. If bytes have to be removed, record the exact rule before coding.

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

### What remains inferred

The measured length of 39 bytes is exactly the common size of a full `esds` box containing an
ES descriptor, decoder-config descriptor, a two-byte decoder-specific-info/ASC field and an
SL-config descriptor. That is a strong structural clue, **not a byte-level finding**: the actual
39 bytes are not in the repository, so their tags, bitrate fields and ASC have not been observed.

The future parser must consequently fail closed and must not hard-code byte offset 34 merely because
it is common. It should accept either:

* a directly validated raw ASC; or
* a size-checked `esds`/descriptor chain whose variable-length MPEG-4 descriptor lengths are parsed,
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

The MH2p adapter waits up to 100 ms for its callback; that value is set at `0xb348a`. It is evidence
for a bounded wait, not permission to block the APAP receiver indefinitely.

## 4. Thread and coexistence decision

Static analysis supports independent decoder **objects**, but it does not prove unlimited or
race-free coexistence with Cinemo's stock media pipeline:

* the SDK wrapper and AAC plugin allocate one new object per factory call;
* no per-process AAC decoder singleton was found;
* the plugin registry/class factory is global;
* `libNmeAudioAAC.so` links `libOmxCore.so`, so an underlying shared OMX resource or teardown race
  cannot be excluded statically.

Use exactly one AAC decoder per type-103 stream generation. The existing APAP receiver thread can
own all `Create`/`Initialize`/`Write`/`Flush`/`Release` calls; no new hook thread is required. Calls
on one Cinemo instance must be serialized.

The decoded callback may run inline or on a Cinemo/OMX thread. It may only validate the generation
and output format, copy into a bounded decoder-owned PCM FIFO, update counters, and signal a
condition variable. It must not call Cinemo again, write QSA, or take socket/QSA locks.

In particular, do not hold `spa103_render.lock`, the APAP socket lock, or the QSA sink lock across
`Initialize`, `Write`, callback wait, `Flush` or `Release`. The current APAC path can decode while
holding its render lock because its decoder is synchronous; copying that locking shape into the
callback-based Cinemo path risks immediate self-deadlock. Snapshot the session generation/state,
operate under an AAC-private mutex, then revalidate the generation before putting PCM in the shared
ring.

On timeout, malformed callback format, FIFO overflow or Cinemo error, count and drop/mute that AU.
Do not feed compressed bytes or unvalidated PCM into DIO5, and do not tear down `dio_manager` from a
decoder callback.

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

## 6. Coding plan

The first coding pass should follow this order:

1. Add an AAC bridge with the same lifecycle shape as `apac_bridge.h`: create/get-info/decode-S16/
   reset/destroy. Resolve all six Cinemo wrappers from `RTLD_DEFAULT`; do not import target-private
   addresses or initialize a second Cinemo configuration.
2. Parse and validate the selected cookie by codec family. For AAC, extract the actual ASC from raw
   ASC or a validated descriptor wrapper. Keep the ASC allocation alive for the entire decoder
   session. Reject APAC `dapa` and any geometry not matching the negotiated index.
3. Build the exact input/output `CinemoMediaType` structures above and initialize one decoder for
   the stream generation. Check every return value and actual callback format.
4. Add a decoder-kind dispatch at the render boundary: 65/66 continue through the untouched APAC
   bridge; 23 goes to AAC-LC; 22 exists only behind an explicit fallback gate. Unsupported indices
   remain fail-closed.
5. Feed each authenticated APAP media item to Cinemo as one AU using `(1, 0, 1)`. Collect zero or
   more callback chunks in the bounded private FIFO, enforce frame alignment, and emit complete
   stereo frames only.
6. Widen PCM2 to PCM6 as `L,R,0,0,0,0`, run the existing privacy-preserving level counters, and push
   into the established 48 kHz render ring. Do not change the APAC channel reorder/downmix paths.
7. On `FLUSHBUFFERED`, stop accepting callbacks for the old generation, call `Flush`, empty the AAC
   FIFO and shared PCM queued before the flush boundary, and resume with the same ASC. On cookie or
   format change, flush/release/recreate. Use `Drain` only for orderly EOS, not for a discontinuity.
8. Teardown prevents new callbacks, waits only for bounded in-flight work, releases the Cinemo
   interface exactly once, then frees ASC/FIFO/mutex state. It must be idempotent.
9. Add bounded counters: AU/bytes in, callbacks, PCM frames, `Write` errors, timeouts, bad callback
   formats, stale-generation callbacks and FIFO overruns. Do not retain or log playable PCM in a
   production build.

This code makes both decoders coexist in the receiver. It does not claim that iOS will choose both
within one session; that is a separate, currently negative vehicle measurement.

## 7. Ordered validation and risk

| Gate | Test | Risk to current vehicle state | Drive? |
|---:|---|---|---|
| 0 | Keep the current known-good bundle and `AUDIO_SPATIAL=0`; record hashes before any test. | none | no |
| 1 | Copy/decrypt the already observed index-23 cookie/APAP capture, or make the short stationary capture described above. Prove raw/ADTS/LATM and parse the exact 39 bytes. | none offline; low for one reversible session capture | no |
| 2 | Host tests with fake Cinemo functions: raw/`esds` cookie parsing, zero/multiple callbacks, malformed geometry, timeout, flush, stale generation and FIFO bounds. Run the existing APAC tests unchanged. | none | no |
| 3 | With CarPlay and stock local media stopped, run a standalone ARM/QNX probe in a separate process against the real MHI2Q libraries and captured AUs. Require 48 kHz/2ch/S16, 1024 frames/AU and a valid WAV/hash. Repeat create/decode/flush/release. | low, process-local and non-persistent | no |
| 4 | While stock local Cinemo playback is active, repeat a bounded separate-process AAC probe and watch `dio_manager`, Cinemo and OMX logs. This is the first test of shared runtime capacity. | medium; stop immediately on stock playback or OMX disturbance | no |
| 5 | Integrate the wrapper but leave `AUDIO_SPATIAL=0`; verify ordinary CarPlay and APAC unit/bench regressions before advertising AAC. | low | no |
| 6 | In a fresh stationary session advertise index 23 only. Play ordinary material; require clean auth/AU/decode/PCM/ring counters, audible L/R, zero-filled extra slots, and correct pause/seek/flush/cookie replacement. | medium, one LD_PRELOAD generation; rollback is the known bundle/env switch | no |
| 7 | In a separate session advertise `[65]` and replay the proven Atmos case. APAC hashes, channel order, feedback and six-channel counters must remain unchanged. | low-medium regression test | no |
| 8 | Only after both decoders pass separately, repeat `[65,23]` to document selection. Current evidence predicts 23 for the whole session; success here means universal stereo, not automatic spatial switching. | medium | no |

No decoder/framing/resampling gate requires a moving vehicle. A drive is optional only for final
subjective acoustic acceptance after stationary counters and speaker tests pass. Do not combine the
first Cinemo coexistence experiment with a drive.

## Confidence ledger

| Finding | Confidence | Basis |
|---|---|---|
| index 23 is AAC-LC/48000/stereo and is selected by the tested phone | high/measured | iOS enumeration plus vehicle SETUP/APAP scale |
| index 23 removes the need for 44.1-to-48 resampling | high | selected 48000 format and fixed 48 kHz HU endpoint |
| APAP itself copies codec bytes without adding ADTS/LATM | high/direct | iOS encode/decode media callbacks |
| payload is one raw AAC-LC AU per APAP media item | high/static, not yet live-proven | iOS byte copy + donor Write boundary + target `AUDIO_ES` raw path |
| 39-byte index-23 cookie differs from APAC cookie | high/measured | distinct live length and ID; APAC cookies are `dapa` atom chains |
| 39-byte cookie is a full `esds` wrapper | medium/inferred | exact characteristic length; bytes have not been retained |
| Cinemo `header_data` must be raw ASC | high/direct | target bit parser begins at header bit zero |
| index-23 ASC is `11 90` | high for standard AAC-LC geometry; capture still gates production | direct ISO field calculation and target parser |
| separate codec instances are structurally supported | high/direct | fresh SDK wrapper and fresh AAC object per factory call |
| concurrent use cannot disturb stock Cinemo/OMX | unknown until runtime | global registry and `libOmxCore` remain shared |
| `[65,23]` switches codec per track | refuted | vehicle selected 23 once for the session |

The only discovery item that should precede coding is the exact index-23 capture: framing and the
39-byte cookie bytes. Everything after that is implementation and bounded validation, not another
firmware-wide search.
