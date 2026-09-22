# iOS AutoMix -> buffered CarPlay transition audit

Date: 2026-09-06

Target: iOS 27 build `24A5418b`, buffered AirPlay/CarPlay audio stream type 103

Status: static architecture established; the currently reported vehicle transition fault is **not yet dynamically attributed** to an APAP transition field.

## Bottom line

iOS performs AutoMix on the phone.  It does not send two undecorated songs and ask the head unit to invent the transition.  The recovered path is:

1. `_SonicKit_MusicKit` selects a transition strategy from song analysis (tempo, downbeats, structure, genre and timing accuracy), then produces separate incoming/outgoing automation schedules and, when allowed, a time-stretch schedule.
2. `MediaPlaybackCore` installs the smart transition on the two player items and observes begin, pivot and end events.
3. `MediaToolbox` gives the outgoing and incoming Buffered AirPlay render pipelines a common mix event, joins them through its offline mixer, applies their audio curves/speed ramp, and transcodes the result to one transport format.
4. `AirPlaySender` sends the resulting single type-103 stream.  It can also serialize exact trim durations and a marker-only `TransitionID` packet.

The head unit therefore does **not** need an Apple AutoMix implementation.  It receives the already mixed result.  Its remaining responsibility is to decode the packet sequence faithfully, including any APAP trim and marker semantics that iOS actually uses in that session.

One candidate was falsified by this audit: the properties deferred to a `TransitionID` in the recovered audio path are audio curves and speed-ramp data, not an APAC/AAC format index or magic cookie.  Transport-format changes remain identified by the inline APAP magic cookie, which the current hook already handles.

The pre-2026-09-06 hook was protocol-incomplete for trim fields 4/5. Both
Apple's receiver and the Pioneer donor preserve those durations; Pioneer
converts them to sample counts and removes the named frames after decode. The
current hook now implements that behavior as described and validated in
section 8.

Field-10 `TransitionID` is different.  Apple's generic receiver reconstructs it as an ordered, empty CoreMedia sample buffer, but the Pioneer receiver does not recognize field 10 at all.  It queues the otherwise valid empty packet, obtains zero decoded PCM and continues without a converter reset or buffer flush.  The hook's authenticated state-only drop is therefore audio-equivalent to Pioneer for this field.  Adding an invented reset for field 10 would reduce, not improve, Pioneer compatibility.

None of fields 4, 5 or 10 occurs in the retained vehicle captures, so the missing trim support is still not the proved cause of the observed A -> B -> C incident.

## Images inspected

| Image | Role | SHA-256 |
|---|---|---|
| `MediaPlaybackCore` | queue/player transition controller | `fd69b7acd8284e2fd1b19ab8184d30afb2793c5ca0c25c17e93adb1298e0b142` |
| `_SonicKit_MusicKit` | AutoMix planner, strategies and schedules | `53da355e42af91fa9b4b03c1b3bda2787925c619f5e6f2d9ceabb6480ed8bd89` |
| `MediaToolbox.i64` | FigPlayer and Buffered AirPlay render/mix pipeline | `378e00075f24511d92e4b68f0df2b12be8351bb2ada92bf5efe79bbc4df9f73f` |
| `AirPlaySender.i64` | APAP sender | `dab6d9d4b436382ab2530d6682db770f09c2d83cfabdcd5d809de6a5268b559a` |
| `AirPlaySupport.i64` | stock APAP receiver reconstruction | `25b0ea21d57a6ab14ec13158a2ad360d065170c5ca2d9856198ae85fb225fd43` |
| Pioneer `libSPF_CP_PlugInWrapper.so` | shipping buffered receiver comparison, AirPlay 480.7.4 | `d8ac9aa2247d43627228d6538496ee0757e8f6eaa50526babc44dd697b971790` |

The first two dylibs were extracted from the supplied dyld shared cache.  Addresses below are their slid virtual addresses in that cache/IDA database, not standalone file offsets.

## 1. What AutoMix computes

### Read directly

`_SonicKit_MusicKit` has distinct candidate-generation and styling strategies, including:

- `SmartCrossFadeCandidateGenerationStrategy`;
- `BeatMatchedFilteredCrossFadeCandidateGenerationStrategy`;
- beat-matched pop, hip-hop and dance variants;
- `FallbackCrossFadeCandidateGenerationStrategy`;
- dead-air removal and soft-skip strategies.

The reflected schedule model includes:

| Evidence | VA |
|---|---:|
| `playbackRateRamp` | `0x2b8795640` |
| `outgoingSongAutomations` | `0x2b8795d20` |
| `incomingSongAutomations` | `0x2b8795d40` |
| `timeStretchingSchedule` | `0x2b8795eb0` |
| `beatMatchedFilteredCrossFade` | `0x2b8796070` |
| `timeStretchedCrossFadeWithEffects` | `0x2b87961c0` |

Its analysis objects expose `transitionPivotPoint`, `advanceTimeForOverlappedPlayback`, `averagePlaybackRateAsDominantTrack`, downbeats, average tempo and tempo ratio.  Logs explicitly reject or downgrade candidates when tempos, genres, timing accuracy or structural boundaries are unsuitable.

The automation parameter catalogue is not only volume.  The binary names filter cutoff/resonance/type, repeater, reverb, delay, gate, flanger, phaser and tape-stop controls at `0x2b87a0500..0x2b87a09e0`, plus Audio Unit filter/delay/reverb parameters at `0x2b87a0a00..0x2b87a0b40`.

`MediaPlaybackCore` exposes the corresponding transition state machine:

| Function/event | VA |
|---|---:|
| `+[MPCPlaybackEngine deviceSupportsSmartTransitions]` | `0x1cdcf2a68` |
| `smartTransitionWillBegin...` | `0x1cde02cac` |
| `overlappingTransitionDidReachPivotPoint...` | `0x1cde03204` |
| `smartTransitionDidEnd...` | `0x1cde036f4` |

The begin callback records outgoing and incoming source/target times and an average transition rate.  Other recovered logs show that smart transitions are cancelled when setup is too late, when playback jumps in the incoming item, or when a jump occurs during an active crossfade (`0x1ce18cb60..0x1ce18cd90`).  Separate setters install speed ramps on incoming and outgoing items (`0x1ce062eb0`, `0x1ce063498`, `0x1ce064be8`, `0x1ce065df4`).

### Inference

The exact private scoring weights for every strategy were not reconstructed.  The types, property names and control flow do prove the shape: AutoMix chooses an overlap/pivot, may retime one or both items, and applies independent envelopes/effects to the two sides.  It is substantially more than a fixed crossfade, but not every transition uses every available effect.

## 2. How Buffered AirPlay renders it

### The property change at the boundary

`_playerfig_prepareWorkingItem2 @ 0x1968a2764` creates the property dictionary immediately after assigning an exit `TransitionID` to the preceding item.  The relevant instructions are:

```text
0x1968a444c  ADRL X8, _kFigRenderPipelineProperty_SpeedRampData
0x1968a4458  LDR  X2, [X23,#0x8b0]
0x1968a4460  BL   CFDictionarySetValue-like import

0x1968a4510  ADRL X8, _kFigAudioRenderPipelineProperty_AudioCurves
0x1968a451c  BL   CFDictionarySetValue-like import

0x1968a4520  ADRL X8, _kFigAudioRenderPipelineProperty_SecondaryAudioCurves
0x1968a452c  LDR  X2, [X23,#0x630]
0x1968a4534  BL   CFDictionarySetValue-like import

0x1968a4558  BL   _FigRenderPipelineSetPropertiesToUpdateAtTransition
```

The normal gapless-commit helper `_fpfs_SetRenderPipelinePropertiesToUpdateAtTransition @ 0x196ba56c4` independently builds the same kind of dictionary:

```text
0x196ba5748..0x196ba5758  SecondaryAudioCurves
0x196ba5764..0x196ba5794  timeline SpeedRampData -> render-pipeline SpeedRampData
```

No format index or magic-cookie property is inserted by either recovered audio call path.  This closes the earlier possibility that the hook was missing a deferred codec switch hidden in `PropertiesToUpdateAtTransition`.

### Two pipelines become one encoded stream

The Buffered AirPlay implementation has explicit mix-event coordination:

- `_fbapspManager_prepareMixingData @ 0x196cf51e8`;
- `_fbapspManager_startMixingData @ 0x196cf52a0`;
- `_fbapspManager_stopMixingData @ 0x196cf5358`;
- `_fbapop_startSubPipeManagersForMixEventID @ 0x19714aa14`;
- `FigBufferedAirPlayOfflineMixerFactory` acquisition/release code around `0x196dac004`;
- `_FigBufferedAirPlayAudioChainSubPipeTranscodeCreate @ 0x197381924`;
- `_bapspTranscode_assembleAudioChain @ 0x197383978`;
- `_bapspTranscode_setPropertiesToUpdateAndStartAtTransition @ 0x197383f5c`;
- `_bapspTranscode_stopMixer @ 0x197384190`.

The transcode constructor logs both content and transport ASBDs, including sample rate, channel count, packet/frame size, bit depth and layout tag.  Other code explicitly handles different multichannel layouts and mixed sample rates before the shared offline mixer is started.

### Conclusion from this layer

The outro and intro are mixed/transcoded on the phone into the one selected Buffered AirPlay transport.  `AudioCurves`, `SecondaryAudioCurves` and `SpeedRampData` are local render/mixer controls.  The arbitrary property dictionary is not an APAP extension payload.  Only the marker and media-buffer semantics described below cross the APAP boundary.

Confidence: high.

## 3. What crosses APAP

### Sender

`_bufferedAudioEngine_enqueueMessagesFromBufferQueue @ 0x2510611a8` checks the sample-buffer attachment `kCMSampleBufferAttachmentKey_TransitionID`, reads its 64-bit value and enqueues an empty message (`0x251061c74..0x251061c94`).  Thus a transition marker is deliberately separate from an audio access unit.

`_bufferedAudioEngine_encodeExtensionAPAP @ 0x2510637b0` serializes:

| Field | Meaning | Wire payload | Instructions |
|---:|---|---|---|
| 4 | trim duration at start | signed BE64 value + signed BE32 timescale | `0x251063964..0x2510639c4` |
| 5 | trim duration at end | signed BE64 value + signed BE32 timescale | `0x2510639e8..0x251063a48` |
| 10 | transition ID | big-endian uint64 representation | `0x251063c00..0x251063c28` |

The sender also adjusts its remote media timestamp by the trim durations before/after enqueue.  These fields are timing semantics, not optional decoder decoration.

### Apple's receiver

`_hoseSBAR_decodeAPAPExtensionItem @ 0x22666bf34` has explicit switch cases:

- field 4 at `0x22666c0ec..0x22666c150`: copy exactly 12 bytes, byte-swap, make/validate `CMTime`;
- field 5 at `0x22666c274..0x22666c2d8`: the same for end trim;
- field 10 at `0x22666c1f4..0x22666c218`: decode a big unsigned value and set its present flag.

`_hoseSBAR_enqueueAudioBBuffToInternalBuffersInternal @ 0x226661b48` then reconstructs CoreMedia semantics:

- a field-10 packet with media data is rejected;
- a field-10 packet without media data becomes an empty `CMSampleBuffer` carrying `kCMSampleBufferAttachmentKey_TransitionID`, then enters both internal queues;
- normal audio buffers receive `kCMSampleBufferAttachmentKey_TrimDurationAtStart` and/or `...TrimDurationAtEnd` dictionaries when the decoded fields are valid.

This is strong evidence that stock consumers are expected to see the boundary marker and exact trim attachments in queue order.

## 4. Difference in the pre-2026-09-06 MHI2Q hook

The implementation audited before the field-4/5 change did the following:

- `spa_apap_extension_t` retains only the cookie ID and inline cookie;
- `spa_apap_parse_extension` parses fields 17/18 and skips every other field;
- `spa103_render_frame` increments `stateOnlyPackets` and immediately returns when `audioLength == 0`;
- every decoded AAC/APAC access unit is rendered in full, with no start/end trim.

That implementation lost fields 4, 5 and 10 even though Apple's sender and
receiver both implement them. The current working tree closes fields 4/5 as
recorded in section 8; field 10 remains an intentional Pioneer-compatible
audio no-op.

It does **not** invalidate the accepted cookie-driven APAC/AAC switch.  APAP fields 2/8 remain samples-per-frame in the captured sessions, not format indices; field 17/18 cookie identity remains the correct decoder selector.

## 5. Pioneer receiver comparison

The Pioneer SPHERA receiver gives a useful product-specific answer which is narrower than the generic Apple receiver contract.

### Fields 4 and 5 are applied to decoded PCM

`APAPDecodeExtension @ 0x1099f0` recognizes both 12-byte media-time fields:

- field 4 is byte-swapped and stored by `0x109b4c..0x109b60`;
- field 5 is byte-swapped and stored by `0x109cb8..0x109ccc`;
- `AirPlayMediaTimeMake @ 0x109880` stores the signed 64-bit value followed by the 32-bit timescale.

The buffered receive thread `sub_123F00 @ 0x123f00` decodes the extension at `0x124a48`.  Before enqueue it converts each non-zero duration to a sample count using the stream sample rate:

```text
trimSamples = floor(durationValue * streamSampleRate / durationTimescale)
```

If the duration timescale already equals the stream sample rate it copies the value directly.  The relevant instructions are:

- field 4 / start trim: `0x124ab0`, then `0x124c34..0x124c6c`; result at packet `+0x44`;
- field 5 / end trim: `0x124ab8..0x124b00`; result at packet `+0x48`;
- zero timescale is rejected at `0x124c50` or `0x124ae4`.

The render-side state/update routine `sub_126160 @ 0x126160` parses the extension again at `0x1261e0`, decodes the AAC/APAC access unit through `sub_123940` at `0x126358`, applies SoundCheck and the APAC 5.1 channel reorder, then trims at `0x12642c..0x126488`:

```text
startBytes     = trimAtStartSamples * decodedBytesPerFrame
totalTrimBytes = (trimAtStartSamples + trimAtEndSamples) * decodedBytesPerFrame

if totalTrimBytes < decodedBytes:
    decodedPointer += startBytes
    decodedBytes   -= totalTrimBytes
    decodedStartFrame += trimAtStartSamples
else:
    decodedBytes = 0
```

The whole-packet case is selected at `0x126450..0x126458`; the stock log strings name the two packet fields explicitly as `trimAtStartSamples` and `trimAtEndSamples`.  Pioneer rounds each packet independently by integer division.  It does not carry a fractional remainder between access units.

This makes fields 4/5 worthwhile compatibility hardening.  In this hook the equivalent safe placement is immediately after `decodeS16` succeeds and before APAC reorder, stereo upmix and ring insertion, using the decoder PCM sample rate and clamping the sum to the number of decoded frames.  This prevents codec priming/padding or an explicitly trimmed transition edge from entering either the upmixer history or the QSA ring.

### Field 10 is deliberately a no-op in Pioneer

The Pioneer parser dispatches fields 2, 4, 5, 7, 8, 11, 12 and 17.  It has no field-10 case.  An unknown field takes the common skip path at `0x109bbc..0x109bc4`; therefore `TransitionID` is neither stored nor compared and cannot reset the converter.

The packet itself is still enqueued through the ordinary packet list at `0x124694..0x1246c4`.  On render, `_AudioDecoderDecodeFrame` (`sub_123940 @ 0x123940`) checks the protected payload length at `0x12394c..0x12397c`.  A packet containing no audio, only the 24-byte encryption overhead, returns success with zero output bytes at `0x123980..0x1239b0`; its diagnostic is `Detected no audio samples but encryption overhead data in the last packet.`

The current hook authenticates the record, applies flush-boundary rejection, records its sequence number, parses the extension, increments `stateOnlyPackets`, and returns before decode when `audioLength == 0`.  It therefore produces the same audio-side result as Pioneer.  It does not reproduce Apple's CoreMedia marker object, but no Pioneer consumer of that object exists in this recovered path.

Product-specific conclusion:

- implement and test fields 4/5;
- optionally recognize/count field 10 for diagnostics;
- do not map field 10 to decoder rebuild, ring clear, QSA restart or `FLUSHBUFFERED`;
- do not present fields 4/5 as the fix for the saved transition incident unless a transition-centred capture actually contains them.

Confidence: high for the Pioneer behavior above; direct parser, receive, decode and trim paths agree with their embedded log strings.

## 6. What the retained vehicle captures do and do not prove

Three authenticated type-103 captures were scanned:

| Capture | Complete authenticated frames | Fields 4/5/10 |
|---|---:|---|
| `logs/captures/spatial_live_20260901_s6/spatial_apap_5.bin` | 5698 | absent |
| `logs/captures/mhi2q_hsr_td_active_prio18_20260904/post_2min_auto_next_incident/spatial_apap_5.bin` | 3393 | absent |
| `logs/captures/mhi2q_ambient_hq_lock_20260904_0030/spatial/spatial_apap_4.bin` | 2727 | absent |

The second file's complete frames had field counts
`{2:3393, 8:3393, 11:3393, 12:3393, 17:3393, 18:10, 19:24, 20:3386, 21:24}`.

This negative result matters:

- it prevents claiming that ignored trims/markers caused that saved incident;
- the captures are bounded rolling files and may no longer contain the natural AutoMix boundary;
- iOS may also render some AutoMix boundaries completely before APAP and therefore need no 4/5/10 fields for that particular pair.

The user's observation that restarting the affected song from its beginning repairs playback is consistent with resetting decoder/queue timing, but it does not identify which side created the bad state.

## 7. Next measurement that can settle causality

The later exact-iOS-26.1 whole-lifecycle comparison is recorded in
`docs-obsidian/Sources/reference/IOS_26_1_BUFFERED_AUDIO_LIFECYCLE_AUDIT.md`. It proves two
separate rate-state defects in the receiver (presentation before initial rate
one, and AU loss during rate zero without a flush). Those findings supersede
the earlier assumption that only a transition-centred field-4/5/10 capture was
needed before the next code change.

Fields 4/5 are now implemented as recorded in section 8.  A bounded trace of
an exact natural AutoMix boundary is still required to determine whether those
fields participated in the reported incident. It should record, per
authenticated packet:

- fixed-header media time, timescale and 24-bit sequence;
- all TLV IDs;
- decoded field-4/5 `CMTime` values and field-10 transition ID;
- cookie ID, inline-cookie change and selected decoder;
- audio length, decoded frame count and PCM-ring depth;
- rate/anchor, `FLUSHBUFFERED`, QSA owner, fallback and underrun counters;
- old/new now-playing persistent IDs in the existing state trace.

The capture must begin before the outgoing song reaches its AutoMix overlap and continue through the incoming song's stable section.  A last-2-MiB rolling APAP file alone is insufficient if it can overwrite the boundary.

Decision after that capture:

1. **Fields 4/5 occur:** verify that the implemented Pioneer-equivalent trim removes the named decoded edge frames and correlate it with the audible boundary. Pioneer truncates each packet independently; no fractional carry is needed for product parity.
2. **Field 10 occurs:** count and correlate it with cookie/track epochs.  Do not invent a ring flush merely because the marker exists: Apple's code queues a marker object, while Pioneer ignores the field and emits zero PCM for its empty packet.  Neither implementation defines it as `FLUSHBUFFERED`.
3. **None occur at the exact bad boundary:** this compatibility gap is real but not the incident.  The next surviving target is the already mixed output's timing/buffering around the two-subpipe overlap and any iOS transition cancellation/jump, not decoder selection.

## Confidence ledger

| Claim | Confidence | Basis |
|---|---|---|
| AutoMix planning, timing and effects happen on iOS | high | SonicKit schedule/types, MediaPlaybackCore transition state/callbacks |
| Buffered AirPlay mixes intro/outro and emits one transport stream | high | explicit mix-event, offline-mixer and transcode paths |
| Deferred audio properties are curves/speed-ramp, not codec/cookie | high for the recovered audio paths | direct dictionary construction in `playerfig` and `fpfs` |
| APAP 4/5/10 are trim-start, trim-end and TransitionID | high | matching Apple sender encode and Apple receiver reconstruction |
| Pre-2026-09-06 hook ignored all three; current hook applies 4/5 and still ignores 10 | high | direct current source inspection and section 8 host tests |
| Pioneer applies 4/5 after decode | high | direct decode/enqueue/render path and named trim log strings |
| Pioneer ignores field 10 and emits zero PCM for its empty packet | high | no parser case; common unknown-field skip; explicit no-audio decoder path |
| Ignoring them caused the reported A -> B -> C vehicle incident | unproved | none appears in the retained authenticated captures |
| Restart-to-beginning repairs a decoder/queue epoch mismatch | plausible inference only | symptom; no transition-centred packet trace yet |

## 8. TLV 4/5 implementation status (2026-09-06)

Fields 4/5 are now implemented in the hook, without changing field 10 or the
existing `FLUSHBUFFERED` state machine:

- `spa_apap_parse_extension` requires the Pioneer/Apple 12-byte CMTime shape
  and decodes signed BE64 `value` plus signed BE32 `timescale`;
- `spa_apap_trim_window` converts positive valid durations with
  `floor(value * decodedSampleRate / timescale)`, using quotient/remainder
  arithmetic so a hostile value cannot overflow;
- both edges are clamped to the decoded access unit; a whole-packet trim
  produces zero rendered frames without being counted as a decoder failure;
- `spa103_render_frame` applies the returned source window after `decodeS16`
  and before APAC reorder, stereo upmix, PCM statistics and ring insertion.
  Trimmed samples therefore cannot contaminate the upmixer history.

Host coverage lives in `tests/apac_type103_test.c`. It checks the exact TLV
layout and audio split, signed decoding, 44.1/48 kHz rescaling and floor
behavior, start/end and whole-AU clamping, absent/invalid values, and rejection
of a trim field whose payload is not exactly 12 bytes. It also compares 100,000
deterministic combinations against an independent bounded reference formula
and drives 50,000 malformed extension buffers through the parser. The complete
`scripts/test_hook_audio.sh` suite passes, including the real six-packet APAC
fixture at all nine stream split sizes. The APAP/APAC test also passes under
AddressSanitizer and UndefinedBehaviorSanitizer. That pass found and removed a
pre-existing unaligned float read in the fixture test; no sanitizer finding was
reported in the production parser or trim arithmetic.

The Pioneer comparison was independently repeated against the original
`d8ac9aa2...1790` AArch64 image with radare2: `0x109b4c..0x109b60` and
`0x109cb8..0x109ccc` load/reverse the BE64+BE32 field-4/5 payloads;
`0x124ae8..0x124b00` and `0x124c54..0x124c6c` perform the rate multiplication
and integer division; `0x12642c..0x126488` applies the start/end byte window to
decoded PCM. The Docker QNX 6.5 ARM build produces both `libcarplay_hook.so`
and `libcarplay_apac.so`; the final ARM hook exports the compiled
`spa_apap_trim_window` and `spa_apap_parse_extension` functions and adds no new
shared-library dependency.

This closes the static compatibility gap. It still does **not** prove that
missing trims caused the saved A -> B -> C incident: the retained authenticated
captures contain no fields 4/5. A transition-centred live trace remains the
measurement needed to make that attribution.
