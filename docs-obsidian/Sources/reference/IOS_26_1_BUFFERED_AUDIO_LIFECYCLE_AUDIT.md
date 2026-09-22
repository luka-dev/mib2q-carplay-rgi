# iOS 26.1 buffered CarPlay audio lifecycle audit

Date: 2026-09-06

Target phone image: `iPhone18,3_26.1_23B85_Restore`

Receiver under audit: the current MHI2Q hook's type-103 APAP path

Status: static/retained-trace audit complete; deterministic lifecycle repair
implemented and host/QNX verified on 2026-09-06. Not deployed by this pass.

## Bottom line

The exact iOS 26.1 restore was useful. Its `AirPlaySender`, `AirPlaySupport` and
`MediaToolbox` pseudo-source confirms the complete buffered-audio control
contract instead of requiring an inference from the later iOS 27 cache.

Most of the current path agrees with iOS and the Pioneer receiver: negotiation,
type-103 SETUP shape, APAP framing/authentication, cookie-driven AAC/APAC
switching, trim fields 4/5, state-only field 10, inclusive flush boundary,
QSA ownership transfer and teardown all have supporting code or vehicle
evidence.

Two lifecycle defects were proved in the pre-repair hook:

1. `spa103_render_start` sets `playing=1`, although the receiver-side session
   state was initialized to `rate=0`. A retained cold-connect trace shows eight
   complete 768-frame QSA writes (6,144 frames / 128 ms) before the first
   `SETRATE rate=1` request. The sender deliberately buffers first and starts
   presentation later by `SETRATE`/`SETRATEANCHORTIME`; submitting PCM before
   that command is a contract violation.
2. At `rate=0`, `spa103_render_frame` authenticates and parses each APAP record
   but discards every non-empty access unit. Pioneer instead gates rendering at
   its stored rate while leaving the packet queue intact; only
   `FLUSHBUFFERED` removes a range. A retained MHI2Q handoff with no intervening
   flush proves the practical loss: 30 received 1024-frame AUs, 30,720 frames
   or about 640 ms, were not decoded while rate was zero.

The same repair also closes the confirmed bounded-flush mismatch. Compressed
APAP records and decoded PCM frames now carry the same authenticated
sequence/RTP ownership, so a bounded `from...until` removes only its interval;
an until-only request remains an inclusive prefix reset. In all retained
bounded-flush examples iOS followed it 6--16 ms later with an until-only prefix
flush at the same upper boundary, so this correction must still not be claimed
as the sole cause of an old long gap.

The next surviving architectural difference is timestamped presentation. The
hook stores rate/anchor/first-audible data but drains a FIFO on the QSA clock;
Pioneer maps media timestamps through the synchronized AirTunes clock. That
difference can affect transition alignment, but no retained trace yet proves
that it caused the reported A -> B -> C AutoMix incident.

## Inputs and confidence

The exact restore pseudo-source root is:

```text
/Users/luka/Desktop/AUDI_2/Refferences/iPhone18-3_26.1_23B85_Restore/
  System/Library/PrivateFrameworks/CoreAccessories.framework/System/Library/
```

Relevant exact-build components:

- `PrivateFrameworks/AirPlaySender.framework/AirPlaySender/`
- `PrivateFrameworks/AirPlaySupport.framework/AirPlaySupport/`
- `Frameworks/MediaToolbox.framework/MediaToolbox/`

The product receiver comparison is Pioneer
`libSPF_CP_PlugInWrapper.so`, SHA-256
`d8ac9aa2247d43627228d6538496ee0757e8f6eaa50526babc44dd697b971790`.
Its IDA database contains the recovered function names and the lifecycle notes
from this pass.

Claims labelled **proved** below are supported by direct source/disassembly and,
where stated, retained vehicle traces. **Compatible** means the hook matches the
observed contract but not every private state field was recovered. **Open**
means the difference exists but has no causal vehicle measurement.

## Lifecycle matrix

| Stage | iOS 26.1 / Pioneer contract | Current hook | Verdict |
|---|---|---|---|
| Capability negotiation | `audioOutputFormatsExtended` is an array; multiple buffered formats are normal. Buffered APAC stereo index 64 is disabled unless `bufferedAPACStereo` override is enabled. | Advertises AAC stereo plus APAC spatial and `mainBufferedInfo`. | Compatible and vehicle-proved. |
| Type-103 SETUP | APAP transport, 1024 frames/AU, `shk`, optional dynamic stream ID; receiver returns data port and buffer size. | Validates the fixed-48k/APAP/key/media contract before replacing a live receiver, then opens listener, installs control dispatcher and returns port/buffer/stream ID. | **Hardened; QNX build and CFLite accessor ABI verified, vehicle acceptance pending.** |
| Initial presentation state | Sender fills the hose, waits for readiness/startup delay, then issues GETANCHOR/SETRATE/SETRATEANCHORTIME. Receiver must not present while rate is zero. | Receiver and renderer now share one lifecycle gate initialized to stopped; incoming authenticated AU records are queued, not presented. | **Repaired; host/QNX verified, vehicle acceptance pending.** |
| APAP record framing | 15-byte signed media-time header plus 24-bit sequence, followed by protected extension/audio. | Same framing, bounded record reads and authenticated header use. | Compatible and fixture-tested. |
| Magic cookie / codec epoch | Cookie may change inside one type-103 stream; AAC/APAC selection follows the new cookie/format state. | Stores cookies by ID/content generation, rebuilds decoder, and has cookie-driven AAC/APAC fallback. | Compatible and vehicle-proved in both directions. |
| TLV 4/5 trim | Signed BE64 value + signed BE32 timescale; decode full AU, then remove whole PCM frames from start/end. | Current working tree parses exact 12-byte fields and applies bounded floor conversion after decode and before reorder/upmix/ring. | **Compatible; host and sanitizer tested.** |
| TLV 10 TransitionID | Apple reconstructs an ordered empty sample buffer. Pioneer does not recognize field 10; its empty packet yields zero PCM without reset/flush. | Authenticates, counts and drops a state-only packet. | Audio-equivalent to Pioneer. Do not invent a reset. |
| TLV 11/12 SoundCheck | SETUP media-kind target plus MainLoudnessParameters select one bounded per-item gain. | Uses the recovered Pioneer calculation before trim/reorder/upmix; optional local fallback/limiter default off. | **Compatible calculation; host/capture/QNX verified, listening acceptance pending.** |
| `rate=1` | Enables presentation at an anchor. | Enables ring consumption. | Basic gate works after the command, but anchor is not used for scheduling. |
| `rate=0` | Stops presentation. Pioneer retains queued packets unless a separate flush removes them. | Stops QSA consumption, continues authenticating/parsing and stores complete APAP records in a bounded FIFO. | **Repaired; ordered host regression passes.** |
| Until-only flush | Drop queued media through the inclusive upper sequence/time boundary and reset presentation/converter state. | Prunes compressed and tagged-PCM queues through the modular upper boundary, resets decoder/upmix and reprimes. | Compatible and range-tested. |
| Bounded `from...until` flush | Remove only packets inside the named range; preserve packets outside it. | Applies the same wrapping range to compressed packets, packet-in-flight and tagged PCM; decoder continuity outside the interval is retained. | **Repaired and range-tested.** |
| Startup/reprime | Exact sender normally selects 400 ms startup delay; other modes choose 200/300/600/750 ms. | Cold prime is 400 ms; post-flush reprime is 160 ms; steady backpressure target is about 250 ms. | Cold value matches exact sender. Post-flush/steady policy is local. |
| Arrival/render latency | Receiver may return `arrivalToRenderLatencyMs`; sender converts it to host ticks. Zero/absence is accepted. | Returns zero while maintaining a non-zero local queue. | Allowed response shape; not a proved fault. Actual timestamp alignment remains open. |
| QSA owner handoff | Buffered stream can remain live while realtime type 100 is torn down/recreated. | Same physical handle can pass between stock and detached owners; feedback clock survives. | Vehicle-proved for continuity. The `rate=0` queue loss above affects one handoff shape. |
| Per-stream teardown | Tear down only the named type-103 stream; invalidate old socket/thread work. | Session comparison plus generation, socket close, join, decoder/ring/key/cookie cleanup. | Compatible; no stale media survives normal teardown. |
| Full teardown/reconnect | All stream state and secrets are released, then a new generation is negotiated. | Stops receiver, closes detached QSA, forgets feedback, clears key/cookies/anchor and increments generation on next setup. | Compatible in saved reconnects. Concurrency hardening remains. |

## 1. Exact sender start gate

`AirPlaySender_12.mm:5410..5473` recovers
`bufferedAudioEngine_getStartUpDelay`. The ordinary non-speaker/non-special
case returns 400 ms; its other explicit choices are 200, 300, 600 and 750 ms.

`AirPlaySender_12.mm:7036..7197` recovers
`bufferedAudioEngine_startPlaybackIfNecessary`. It returns without starting
until the ready flag/state allows it, then selects one of the receiver-anchor,
sender-anchor or all-hose rate-one operations. In other words, APAP data may be
queued before play, but the play edge is an explicit rate operation.

The pre-repair hook contradicted that contract by assigning:

```c
spa103_render.playing = 1;
```

The retained trace
`logs/captures/state-trace/20260905-131355/carplay_state_trace.log` shows:

```text
02:41:30.126  BUFFERED_RENDER_START
02:41:30.163  BUFFERED_DECODER_READY
02:41:30.231  QSA_OPEN, pending=19456
02:41:30.232  QSA_DETACHED_FIRST_WRITE plus seven complete writes
02:41:30.242  BUFFERED_CONTROL SETRATE rate=1
```

This does not prove all 128 ms became audible before the request; QSA may have
queued some of it in hardware. It does prove that the receiver submitted media
before iOS authorized presentation, which can move the effective anchor and
consume the wrong head of a transition.

The current renderer embeds `spa_apap_lifecycle_t`. Its initializer sets the
only presentation gate to zero; `spa103_render_start` no longer has a separate
optimistic `playing` value. Before the first successful rate-one command,
authenticated APAP audio can enter only the compressed FIFO. No decoder output
or buffered-QSA bootstrap is possible from that queue while stopped.

## 2. Rate-zero queue preservation

The pre-repair `spa103_render_frame` authenticated the packet, applied any active
flush boundary, parses its TLV extension, and then returns at
`hook/audio/audio_hook.c:2427` when `playing==0`. The compressed AU is neither
queued nor decoded.

Pioneer separates those operations:

- `Pioneer_BufferedAudioThread @ 0x123f00` continues receiving and enqueueing
  APAP packets.
- `AirPlayReceiverSessionSetRateAndAnchorTime @ 0x12c0d0` stores the rate at
  buffered render state `+0x78` and stores anchor/network mappings separately.
- `AirPlayAudioStreamReadAudio @ 0x1281b0` reads that rate at `0x1285bc` and
  again at `0x128774`; the zero-rate branch at `0x128784` bypasses the packet
  list/render path and supplies the paused result. Queue removal is performed
  by the separate flush routine.

The decisive retained trace is
`logs/captures/state-trace/20260905-live-aac-stutter-followup/carplay_state_trace.log`:

```text
04:06:44.142  APAP frame=21, framesIn=19456
04:06:44.869  SETRATEANCHORTIME rate=0
04:06:45.588  SETRATE rate=1             (no FLUSH in between)
04:06:45.741  APAP frame=98, framesIn=67584
```

The APAP count advanced by 77 records while `framesIn` advanced by only 48,128
frames, exactly 47 1024-frame AUs. `stateOnlyPackets` stayed zero. The missing
30 AUs equal 30,720 frames, about 640 ms at 48 kHz, and the direct early-return
path identifies why they were lost.

This scenario is a type-103/type-100 route handoff, not an ordinary seek. Most
other saved `rate=0` events are immediately followed by `FLUSHBUFFERED`, so
discarding their old media was harmless. The no-flush case means the current
policy cannot be justified as an optimization.

An exact repair needs a bounded timestamp/sequence-aware queue, not merely a
larger untagged PCM ring. The existing APAP thread can own it; no additional
io-audio thread is required. A later flush must be able to delete its specified
packet range before decode, and a rate-one resume must render retained packets
in order.

The current APAP thread authenticates and validates TLV structure while stopped,
then retains the complete encrypted record. On rate one the same existing
thread takes records from the head, authenticates immediately before decode and
commits each exactly once. A rate change racing decode returns ownership to the
front instead of dropping it. Queue growth stops at the SETUP-advertised
`audioBufferSize` plus at most one complete APAP record; TCP backpressure then
halts further reads. No render or `io-audio` thread was added.

## 3. Flush semantics

Exact iOS 26.1 `FlushWithinSampleRange.mm` carries both start and end sequence
plus media time to the hose protocol, advances the sender's message-ring read
index, recomputes buffered byte count, and marks the hose unprimed.

Pioneer implements both receiver shapes:

- `AirPlayReceiverSessionFlushBufferedAudio @ 0x11a710` resolves stream type
  103 and calls the stream flush under the session lock.
- `AirPlayAudioStreamFlushBufferedAudio @ 0x127e60` stores the bounded start/end
  state and walks the queued packet list with wrap-aware comparisons when a
  `from` boundary is present.
- For an until-only prefix flush it also clears play/prime state, resets the
  audio converter and prunes everything through the upper boundary.

The repaired `spa103_render_flush` accepts all four optional boundary classes.
Every decoded PCM frame is tagged with the authenticated APAP sequence and RTP
which produced it. The same wrap-aware predicate prunes the compressed FIFO,
the packet safely held in flight, and the PCM ring. A bounded range retains
outside records and converter state. An until-only prefix resets decoder/upmix
and sequence baseline. Both select the existing 160 ms post-flush reprime.

### QSA queue boundary: measured and repaired 2026-09-06

The APAP and decoded-PCM checks above did not cover periods already accepted by
`snd_pcm_plugin_write`. A live rapid-song-selection trace preserved in
`logs/captures/state-trace/20260906-song-change-215036/` made that boundary visible:

- each until-only prefix flush removed 11,264--12,032 decoded frames;
- after the 160 ms software-ring reprime, the first new detached write returned
  zero 272--427 ms after the flush and required `prepare+retry`;
- eight such recoveries followed eight rapid selections, while the reported
  QSA device underrun counter remained zero;
- there were no decode failures or APAP sequence gaps explaining the audible
  old-song tail.

Therefore the old compressed records and decoded ring were already correct,
but old PCM resident in QSA/deva could continue rendering until starvation
stopped the device. `spa103_render_flush` now propagates every prefix timeline
reset synchronously to the tracked expanded QSA handle: after releasing the
render mutex it takes the QSA I/O lock, calls the real
`snd_pcm_plugin_flush(PLAYBACK)` followed by the real
`snd_pcm_plugin_prepare(PLAYBACK)`, resets the feedback-clock window, and only
then acknowledges the control request. The new trace event is
`QSA_TIMELINE_FLUSH action=flush_prepare`. The existing first-zero recovery is
retained as a failure fallback.

This is intentionally not done for a lone bounded `from..until` request. Once
PCM has crossed into QSA it has no APAP sequence/RTP tags, so that queue cannot
selectively remove only the bounded interval. The vehicle sends a following
prefix reset for the inspected song/seek cases; that second operation performs
the full device flush. No render or `io-audio` thread was added.

Bounded flushes do occur in the vehicle. Retained examples include:

```text
fromSeq=13846086 untilSeq=13850671
fromSeq=6252860  untilSeq=6254000
fromSeq=5903972  untilSeq=5907730
```

Each inspected bounded request was followed 6--16 ms later by an until-only
request with the same upper boundary. Therefore a perfect first operation would
still be superseded by the second full-prefix flush in those sessions. This
prevents attributing the observed 397--1242 ms interruptions solely to the
bounded-flush mismatch: the same traces also contain long APAP no-data gaps.

Required regression: keep packets before, inside and after a wrapping bounded
range and verify that only the middle set is removed. Then execute the observed
bounded-plus-prefix pair and verify the final state matches Pioneer.

## 4. APAP payload, trims and transition marker

Exact `AirPlaySupport_01.mm:2743..2805` encodes/decodes the fixed 15-byte APAP
header: signed media-time value, timescale and 24-bit sequence.
`AirPlaySupport_01.mm:2808..2945` encodes extension items as variable-length
field ID, variable-length size, payload, ending in field zero.

Exact `AirPlaySender_12.mm` confirms:

- trim-at-start handling at `5124..5222`;
- trim-at-end handling at `5225..5350`;
- APAP field emission around `6448..6659`.

The current parser and renderer now match the Pioneer product behavior:

- `hook/audio/apap_crypto.c:427` performs bounded floor conversion;
- `hook/audio/apap_crypto.c:477..485` requires exact 12-byte signed media-time
  fields 4/5;
- `hook/audio/audio_hook.c:2703..2708` applies the decoded-frame window before
  reorder, upmix and ring insertion.

Field 10 remains intentionally non-operative for audio. Apple's generic
receiver makes an ordered empty CoreMedia marker; Pioneer skips unknown field
10 and decodes the empty packet to zero PCM. The hook's authenticated
state-only count/drop has the same audible behavior. It must not be converted
into an invented decoder reset or flush.

## 5. Anchor and presentation timing

The exact sender parses `arrivalToRenderLatencyMs` from the SETUP response in
`AirPlaySender_23.mm:4700..4749` and converts it to ticks. It accepts a zero or
missing value. The Pioneer donor has no recovered use proving a non-zero value,
so the hook's explicit zero is not by itself a protocol failure.

However, Pioneer actively uses the anchor:

`AirPlayReceiverSessionSetRateAndAnchorTime @ 0x12c0d0` converts the supplied
network time through `AirTunesClock_GetUpTicksNearSynchronizedNTPTime`, stores
rate, RTP/media anchor, network time and local tick mapping, and resets render
counters. `AirPlayAudioStreamReadAudio @ 0x1281b0` compares local ticks and
timestamps, reports late packets and controls concealment/skew.

The hook validates and stores `anchor`, `firstAudible` and `networkTime`, but
its QSA output is selected by FIFO depth plus the device's blocking write
cadence. Those stored times never choose which PCM frame is presented at a
given local tick.

This is **open**, not a proved defect behind the reported transition. The
measurement needed to decide it is one compact transition trace joining:

- authenticated APAP sequence and media time;
- rate, anchor, first-audible and synchronized network/local ticks;
- decoded queue head timestamp and QSA write timestamp;
- cookie/decoder epoch and Now Playing persistent ID.

If the QSA head disagrees with the anchor by a stable amount or crosses a track
epoch, timestamped scheduling becomes the next implementation task. Without
that comparison, changing advertised latency or adding arbitrary sleeps would
be guesswork.

## 6. SETUP, teardown and concurrency hardening

The setup ordering is sound: old receiver stop, new generation/state/key,
render initialization, listening socket, buffered-control dispatcher, receiver
thread, response stream publication, then setup commit. Failure closes owned
resources and calls the same stop path. Teardown closes listener/client sockets,
joins both receiver/control threads before detached-QSA close, clears decoder,
ring, key, cookies and feedback, and prevents a stale generation from clearing
a replacement session.

The repair also closes the two C-level races found by the audit:

- worker loops now read stop plus generation through
  `spa103_receiver_should_stop`, under `spa103_lock`;
- the APAP render path snapshots `cookieGeneration` under `spa103_lock` before
  comparing or publishing it under the render lock.

Neither mutex is held across blocking QSA or socket I/O. `FLUSH` may mark one
compressed packet in flight, but never frees it until the APAP thread returns
ownership, so teardown and control cannot race a decoder pointer.

The current working tree now rejects a missing/unsupported fixed-48k
`audioFormatIndex`, a missing or non-32-byte key, false/missing
`packetFormatAPAP` or `isMedia`, non-1024 `spf`, wrong audio type/mode, mismatched
legacy `audioFormat`, and a mismatched compression type. It performs this gate
before stopping a live receiver. `supportsDynamicStreamID=false` remains a valid
legacy shape and simply omits the returned stream ID. The target CFLite boolean
conversion is statically verified in `CFGetInt64 @ 0x57af0`; vehicle acceptance
of the stricter gate is still pending.

## Verification of the repaired working tree

```text
bash scripts/test_hook_audio.sh  PASS
ASan/UBSan lifecycle regression  PASS
LOG=0 AUDIO_STATE_TRACE=1 bash scripts/build_hook.sh
                                PASS (QNX 6.5 ARMv7 Docker toolchain)
```

The APAP test includes the real six-packet fixture at nine split sizes, TLV 4/5
vectors, 100,000 deterministic arithmetic properties and 50,000 malformed
extension fuzz cases. `apap_lifecycle_test` additionally covers stopped
ownership, ordered resume, rate-race requeue, inclusive prefix flush, bounded
flush, the observed bounded-plus-prefix pair, sequence wrap, RTP-only ranges,
packet-in-flight deletion and advertised-limit backpressure.

Artifacts produced by this audit's clean rebuild:

```text
build/libcarplay_hook.so
sha256 cccda67b5d6e491c6328c4a776c219eaf968e0ef0c24f3d6c17bcae3af73207c

build/libcarplay_apac.so
sha256 a6a7bc8c3f17dfe170765b04a461259a407269de47dde6fe0a2b3f6b2695044f
```

They were not deployed by this pass.

The hook hash above is the quiet lifecycle-trace build, not a deployed image.

## Ordered vehicle acceptance

1. Deploy the trace build only when an on-unit run is requested. On cold
   connect, require `BUFFERED_RENDER_START ... initialRate=0`, growing
   `compressed_packets`, and no buffered QSA write before
   `BUFFERED_RATE_GATE ... new_playing=1`.
2. Exercise a short rate-zero/rate-one handoff with no flush. Queue depth must
   rise while stopped, then return to zero in order; `sequence_gaps`,
   `queue_alloc_fail` and QSA short/zero writes must not grow.
3. Exercise pause/seek and AutoMix. For each flush, compare
   `BUFFERED_FLUSH_APPLIED` removal counts with the requested interval and make
   sure a bounded-plus-prefix pair leaves only post-upper data.
4. Only if a transition fault survives those checks, add the already-specified
   media-time/anchor-to-QSA measurement. Timestamp scheduling remains open and
   unproved; do not change `arrivalToRenderLatencyMs` or add sleeps first.

The deterministic defects are closed in code. Vehicle evidence is still
required before calling the audible AutoMix symptom fixed.
