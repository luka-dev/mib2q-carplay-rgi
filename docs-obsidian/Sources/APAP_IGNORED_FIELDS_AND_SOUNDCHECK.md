# APAP metadata, SoundCheck and type-103 SETUP closure

Date: 2026-09-06. Target: MHI2Q `dio_manager` hook.

This closes the audit of fields sent by iOS on the working buffered type-103
path but previously ignored by the hook. It is deliberately limited to the
authenticated APAP records, the recovered iOS encoder and the Pioneer receiver
implementation. It does not change the already vehicle-confirmed APAC/AAC
decoders, PCM6 route, SAD, feedback clock or HSSR algorithm.

## Correct APAP field map

The earlier statement that fields 2 and 8 both contained `spf=1024` was a
summary error. Re-reading the authenticated bytes gives:

| field | AAC/23 capture | APAC/65 capture | meaning | hook action |
|---:|---:|---:|---|---|
| 2 | `23`, every media packet | `65`, every media packet | transport `APAudioFormatIndex` | parse and count; decoder hint only |
| 4 | absent | absent | trim-at-start CMTime | apply after decode, before DSP/ring |
| 5 | absent | absent | trim-at-end CMTime | apply after decode, before DSP/ring |
| 7 | absent | absent | legacy SoundCheck, 5 x BE32 records | validate/preserve/count; no guessed gain |
| 8 | `1024`, every media packet | `1024`, every media packet | samples per frame | validate/count mismatch |
| 10 | absent | absent | `TransitionID` | recognize/count only |
| 11 | `[-6.5,-1,0.96875]` | `[-18,-1.8125,-1.78125]` or `[-19.25,-2.25,-2.25]` | MainLoudnessParameters | Pioneer-compatible gain |
| 12 | `1` | `1` | SoundCheck media kind | selects SETUP target |
| 17 | cookie ID | cookie ID | decoder-cookie selector | existing cookie lifecycle |
| 18 | 39-byte AAC cookie | 33-byte APAC cookie | inline decoder configuration | existing cookie lifecycle |
| 19 | once | 43 packets | `SbufTracer` | length/count telemetry only |
| 20 | `73` | `35` | source/content format index | change telemetry only |
| 21 | once | 43 packets | extended `SbufTracer` | length/count telemetry only |

The iOS AirPlaySender encoder `_bufferedAudioEngine_encodeExtensionAPAP @
0x2510637b0` independently emits its format member as ID 2, samples-per-frame
member as ID 8, transition member as ID 10, and source-format member as ID 20.
AirPlaySupport maps 19/21 to its two SbufTracer conversion helpers. Therefore
2/8 are not inferred from values alone.

The transport index cannot by itself replace cookie validation. A supported ID
2 selects which decoder is attempted first only when a cookie boundary already
requires a rebuild. An unsupported ID 2 is counted and ignored, leaving a
working decoder alive. If the announced decoder rejects the cookie, the other
decoder is tried; only the cookie-accepting API and its returned PCM geometry
are retained.

## SoundCheck contract

The type-103 SETUPs from the phone contain:

```text
soundCheckMediaKind = { "1": -16 }
```

Pioneer's `AirPlayReceiverApplySoundCheck @ 0x12fbf0`, through its helper at
`0x12fa70`, applies a MainLoudnessParameters/type-2 record as follows:

```text
desiredDb = signed_int8_setup_target - parameter[0]
if desiredDb > 0:
    headroomDb = -1.0 - parameter[2]
    desiredDb = min(desiredDb, headroomDb) when headroomDb >= 0
roundedDb = floor(desiredDb + 0.5)
roundedDb = min(roundedDb, +20)
gain = 10 ** (roundedDb / 20)
```

An invalid loudness value, or an invalid peak value on a boost path, is rejected
to unity. This produces `-9 dB` for the retained AAC tuple and `+1 dB` for both
retained APAC tuples. Gain is applied uniformly to decoded interleaved PCM
after channel reorder or stereo upmix, so L/R and all synthesized
centre/rear/LFE content stay at one item gain. A changed item ramps between
valid gains over 20 ms; the first valid tuple is applied immediately. FLUSH and
cookie discontinuities clear metadata/fallback history but preserve the
currently audible gain until the next authenticated tuple, avoiding a unity
jump.

The post-upmix placement is required for the custom HSSR backend. HSSR consumes
absolute log-energy and log-magnitude features, so applying the AAC `-9 dB`
first is not equivalent to scaling its final PCM6 result. A 20-second
`Bangarang` A/B changed rear/front RMS from `0.088238` at raw analysis to
`0.114080` at `-9 dB` pre-analysis; after compensating the common gain, the
rear waveforms correlated only `0.918473` (Ls) and `0.900969` (Rs). The optional
metadata-free RMS estimator still observes the raw decoded programme, but it
does not modify that buffer. The selected/ramped gain is applied to all final
channels immediately before the optional limiter and ring write.

TLV 7 is structurally implemented (length must be a multiple of 20 and records
are counted), but its gain is not synthesized. No retained vehicle packet uses
it, and the five-word semantic mapping is not sufficiently recovered to make a
safe production calculation. This is intentional fail-safe behavior, not an
omission hidden as support.

## SETUP validation

Before replacing a live receiver, the hook now requires the parts its parser
and fixed output rail actually implement:

- index 23, 64, 65 or 66; index 22/44.1 kHz is rejected because this rail has no
  resampler;
- an exact 32-byte `shk`;
- `packetFormatAPAP=true`, `spf=1024`, `isMedia=true`, `audioType=media`;
- `audioMode`, when present, must be `default`;
- legacy `audioFormat` must be `0x800000` for AAC/23 and zero for APAC;
- `ct`, when present, must be 4 for AAC/23 and 0 for APAC.

The existing receiver is not stopped until the new descriptor passes these
checks. This makes malformed/unsupported setup fail explicitly instead of
opening a silent stream.

The boolean reads do not rely on a guessed CFLite conversion. In the target
MHI2Q library `CFDictionaryGetInt64 @ 0x57e28` delegates to `CFGetInt64 @
0x57af0`; that function compares directly against the exported CFL boolean true
and false singleton slots and returns 1/0 before its number/string paths.

## Optional local processing

Three independent controls exist:

```sh
AUDIO_SOUNDCHECK=1                 # default: on, exact metadata path
AUDIO_SOUNDCHECK_FALLBACK=off      # off | rms
AUDIO_SOUNDCHECK_LIMITER=off       # off | peak
```

`rms` is a local, slow two-second-warmup estimator targeting -18 dBFS and capped
to -12/+6 dB. `peak` is a final sample-peak guard at -1 dBFS with immediate
attack and 100 ms release. Both default off in the deployed wrapper. The latter
is not called a true-peak limiter: it has no oversampled inter-sample detector
and no lookahead. Neither optional mode alters a metadata-normalized stream
unless explicitly enabled.

## Lifecycle and diagnostics

- TLV 4/5 remain exact whole-frame trims. They are performed after the complete
  AAC/APAC access unit decodes and before any DSP history or ring insertion.
- TLV 10 is informational. Pioneer 1.030010 also skips it and does not reset the
  decoder, queue or QSA device because it changed.
- State-only packets authenticate, update/count their metadata and emit no PCM.
- TLV 19/21 contents are not logged; only bounded counts and lengths are kept.
- Stop telemetry now reports format/SPF/transition/source/tracer/unknown counts,
  SoundCheck updates/rejections, legacy record count, fallback updates and
  limiter activity.

## Verification

Host tests cover:

- exact Pioneer gain values for the retained AAC and APAC tuples;
- duplicate suppression, 20 ms item ramp, discontinuity behavior and unity
  bit-exact bypass;
- opt-in fallback and final peak guard bounds;
- all newly named TLVs, malformed known-field rejection and parser fuzzing;
- the real APAC capture: field 2 is always 65, field 8 is always 1024, field 12
  is 1, field 20 is 35, and thousands of AUs still match the accepted decrypted
  reference byte-for-byte.

The QNX build keeps the existing no-emutls/no-constructor/no-libstdc++ gates.
Vehicle validation is still required before enabling either optional local
fallback or limiter; the production SoundCheck metadata path itself mirrors the
recovered Pioneer calculation and is enabled in the wrapper.
