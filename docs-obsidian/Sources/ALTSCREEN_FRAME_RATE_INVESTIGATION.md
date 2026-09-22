# altScreen frame-rate investigation — 2026-09-15

Status: **static investigation only**, per the user's latest instruction.
No HU connections, installs or runtime changes during this follow-up. The
previously prepared diagnostic hook remains undeployed. The exact reason for
this phone's variable frame rate is not yet confirmed.

Follow-up: [stock MU1316 path and concrete OMX configuration mismatch](ALTSCREEN_STOCK_30FPS_PATH.md)
verifies the native 20-byte DisplayPictureBuffer parameter, asynchronous
post/reuse ownership, and the independent 33 ms VC capture timer. These are
additional candidates, not a confirmed explanation of the recorded input FPS.

**Key correction from binary verification:** finding a 10/20/30/60 FPS ladder
does not establish that normal wireless CarPlay selects 20 FPS. The default
minimum bandwidth estimate is **1,000,000 bps** in the supplied iOS 26.6 and
27.0 binaries. In the verified 27.0 ladder this selects at least 30 FPS, before
the receiver's maximum clamps it. With our `maxFPS=30` and default preferences,
that policy recommends 30. Lower delivered FPS still needs to be explained by
scene production, encoding/queue drops or delivery/backpressure, rather than
assuming an intentional 20 FPS limit.

## What the saved HU evidence establishes

- `hook/altscreen/altscreen_hook.c:build_cluster_dict` advertises `maxFPS=30`.
- `AltScreenModule.CLUSTER_FPS=30` configures the HU-to-cluster update rate.
  That is downstream of the phone and cannot make iOS generate extra frames.
- The presenter uses swap interval 2; it presents incoming decoded pictures.
  It does not insert a 50 ms timer or convert the source to a fixed 20 fps.
- In `lag-first-snapshot.txt`, input-picture receipt timestamps for successive
  groups of 600 decoded frames show **20.15, 27.35, 27.23, 27.35, 20.83 fps**.
  These are local hook-to-renderer receipt timestamps carried through OMX, not
  the phone's PTS. They independently confirm that 20 fps is not a fixed cap.
- After the age-filter hotfix: renderer socket arrivals averaged 52 ms and
  GPU submissions 50.62 ms; later 61 ms versus 62.11 ms. Steady decode-worker
  intake age was 0–5 ms and swap cost approximately 0.3 ms. This is only
  `eglSwapBuffers` time, not the complete GPU presentation/callback cost;
  see the synchronous presentation audit below.

Thus the lower frame count is already present before decoding in those windows.
This does **not** by itself separate iPhone/network delivery from the hook:
receive/decrypt/forward share one receiver thread, so blocked forwarding would
also slow reads from the phone socket. The independent renderer reader/queue
and low worker age argue against persistent downstream congestion but are not
a replacement for the first-hop measurement.

## Synchronous presentation audit and what to do next

### The GPU timing blind spot

`screen_egl_present_frame` currently performs this sequence synchronously in
the decoder's `_OnFillDone` callback:

```text
entry timestamp
  -> stop/join waiting loader (first-frame handoff)
  -> gl_lock -> eglMakeCurrent
  -> eglWaitNative -> refresh external EGLImage binding
  -> draw -> glFinish
  -> timed eglSwapBuffers
  -> release EGL context -> unlock -> return
  -> OMX_FillThisBuffer (source buffer finally recycled)
```

The old `arrival` statistic is decoded-callback entry spacing, not phone FPS.
It may include delay caused by the preceding callback's synchronous work.
The old `swap` statistic excludes all operations before and after swap,
including `glFinish`. Consequently **0.3 ms swap does not rule out GPU/context
waiting**. Even full presenter timing would need to be distinguished from the
complete `_OnFillDone` interval, which also includes buffer recycling.

The real presenter host test now injects a deterministic fake-clock delay of
50 ms in `glFinish` and 0.3 ms in swap, after the real loader handoff. Four
sequential calls each take 50.3 ms; the recorded swap maximum remains 0.3 ms.
This reproduces a roughly 19.88 FPS presentation ceiling with cheap swap.
**This is a fault-injection example, not an observed QNX GPU delay.** The test
also preserves the existing no-loader-redraw and source-lifetime checks.

Two smaller statistics issues also need correction in a future measurement
patch: arrival ignores equal timestamps, and swap sums are divided by the
arrival-interval count rather than their own sample count. Decoder/socket
arrival counters should reset at generation boundaries and exclude config
records when reporting video cadence.

Do not remove `glFinish` to make this path look faster: it is the current
ownership barrier before OMX can overwrite the sampled source. Likewise, the
per-frame external-image rebind fixed verified stale UV/green frames on this
driver. Neither operation has been proved redundant. A separate presenter
thread alone also cannot make a genuinely 50 ms GPU operation deliver 30 FPS;
it could only remove callback coupling or overlap work, subject to ownership.

### Hook wait budget is per fragment

`av_send_all_locked` starts a new `AV_SEND_STALL_LIMIT_MS=1000` deadline on
each invocation. `av_send_locked` calls it separately for the four-byte length
and the payload; reconnect config replay adds another such pair. These calls
run while holding `g_lock`, in the phone receiver thread. Thus a successful
but repeatedly backpressured frame can span almost two seconds, or almost four
with config replay, plus connection/scheduling overhead. The deadline is not
an aggregate one-second frame budget. This is a pathological upper-bound
scenario, not evidence that it occurred on the car.

The accepted socket is nonblocking; `SO_SNDTIMEO` is not the effective retry
clock. `poll` with a monotonic deadline performs that job. Splitting forwarding
into another thread must also stop receiver-side owner/config checks from
waiting behind a network write under the same global lock.

### Prioritized changes, conditional on measurements

| Priority | Concrete next change | Why / acceptance check |
| --- | --- | --- |
| 1 | Add a default-off renderer timing probe: full present time, loader handoff, lock, context acquire/release, native wait/image refresh, draw/finish, swap, OMX recycle; separate startup from steady video | Paired with the existing first-hop hook probe, identifies which stage consumes the 33.33 ms frame budget. Aggregate counts/means/maxima and over-budget counts; no per-frame disk logging. Each metric needs its own sample count. This renderer probe is **not yet implemented**. |
| 2 | Give each forwarded AU/config transaction one monotonic send deadline shared by all fragments | Makes maximum blocking predictable. Keep the initial recovery allowance separate from steady playback; do not blindly set a 33 ms socket timeout. Verify short writes, EINTR/EAGAIN, aggregate expiry and config+IDR recovery. No deadline policy change in this audit. |
| 3 | If hook local-work time is high, separate socket draining from bounded forwarding with explicit generation/config/IDR ownership | Preserve AES-CTR advancement for every consumed encrypted AU; avoid waiting for a send-held lock in the receiver. Queue overflow must trigger explicit continuity recovery, not arbitrary compressed P-frame drops. The renderer already has an independent reader/worker; another queue needs evidence. |
| 4 | If presenter/OMX time is high, locate the costly phase before changing rendering ownership | Optimize a verified costly phase first. Any async present/fence design must hold native buffers until sampling finishes and handle teardown/recovery. Keep the simple loader handoff and avoid reviving the removed always-running compositor. |
| 5 | After stable 30 unique decoded pictures/s, check actual HU-to-VC capture cadence | Java's 30 FPS request and swap interval 2 are configuration, not proof of delivered unique frames. Add bounded presentation smoothing only if input is already 30 FPS but bursty and measured added latency is acceptable. |

The decode queue's **1024 items / 24 MiB** caps memory, not video latency.
Startup needs buffering while OMX becomes ready; steady playback should not
accumulate seconds of obsolete pictures. A future latency bound must separate
those states and account for compressed reference dependencies. Do not restore
the removed 200 ms output-age filter: healthy Qualcomm output already retains
about four input pictures and was incorrectly discarded by that filter.

Current changes from this audit are documentation, corrected explanatory
comments and the host timing reproduction. No runtime buffering, threading,
frame-rate or synchronization policy has been changed. No HU test or install.

The old socket statistics also ignored zero-length interarrival intervals.
Bursts in the same millisecond could inflate the reported mean interval. The
new first-hop accumulator includes zero intervals, uses microseconds, and resets
per receiver session. The 600-frame receipt-span check above does not have that
bias.

## Initial iOS 26.1 decompile findings

Reference root:
`Refferences/iPhone18-3_26.1_23B85_Restore/System/Library/PrivateFrameworks/CoreAccessories.framework/System/Library/PrivateFrameworks/`.

- `AirPlaySender.framework/AirPlaySender/AirPlaySender_10.mm:6278` maps
  `MaxFPS` to the receiver's `maxFPS` field.
- `AirPlaySender_19.mm:1303`, `screenstream_initDisplayInfo`, clamps the maximum
  encoder FPS to that advertised value. It is a ceiling, not a minimum.
- `AirPlaySender.mm:3150`, `screenstream_copyNetworkInfo`, contains the inlined
  `screenstream_updateFPS` logic and its diagnostic messages. Its adaptive
  branch selects 10/20/30/60 FPS from the bandwidth estimate (thresholds
  0.5/1/4 Mbps), then clamps to the maximum. Changes move through intermediate
  targets and a delay; initialization sets the adjustment delay to five seconds
  (`AirPlaySender_18.mm:9470`). These are the sender's internal estimates, not
  a measured throughput or requirement for our current connection. **See the
  binary correction below: the default bandwidth floor prevents the 10/20
  branches from being selected by this ladder.**
- A separate branch explicitly labelled `wired car connection` uses the maximum
  FPS instead of that adaptive ladder. Whether the failing session used that
  branch is unknown; USB versus Wi-Fi was asked of the user.
- `DashBoard.framework/DashBoard/DBThermalMonitor.mm:13` supplies a frame-rate
  limit for thermal levels; `DBInstrumentClusterLayoutViewController.mm:126`
  consumes it for the cluster scene. The numeric table was not decoded here.
  This proves the mechanism exists, not that this particular phone overheated.

Several calls in this decompile have lost their arguments, and one bandwidth
expression incorrectly appears as multiplication by zero. Do not reconstruct
property names or numeric defaults from those missing arguments.

## Binary verification: iOS 26.6 / 23G71 and 27.0 / 24A5418b

Used the existing local 26.6 Mach-O files and 27.0 dyld cache; no IPSW download
or phone access. ARM64e, little endian; addresses below are unslid VAs.
Binary hashes and sizes: [sources.json](../output/ios-fps/sources.json).
Decoded constants, raw bytes and file offsets:
[constants.json](../output/ios-fps/constants.json).

### 1. What the bandwidth ladder actually sees

27.0 `screenstream_copyNetworkInfo` at `0x250f4e9c0` obtains the transport's
`NetworkInfo` and reads **`AverageTxCapacity`**, then computes:

```text
estimated bandwidth = clamp(AverageTxCapacity * 0.75, bwMin, bwMax)
defaults: bwMin = 1,000,000 bps; bwMax = 40,000,000 bps
missing capacity: use bwMin

< 0.5 Mbps -> 10 FPS
< 1.0 Mbps -> 20 FPS
< 4.0 Mbps -> 30 FPS
otherwise -> 60 FPS
then clamp to display maxFPS
```

The scale/minimum constants are identical in 26.6 at `0x22796bed0` and 27.0
at `0x25120a570`. The maximum is initialized as `0x418312d000000000` (double
40,000,000). Generic initial maximum/current/target FPS are 60 and fixed FPS
is zero; the display's advertised maximum subsequently constrains the stream.

27.0 reads optional **phone-side** `com.apple.airplay` preferences
`encoderBWFixed`, `encoderFPSFixed`, `maxFPS`, `bwMin` and `bwMax`. Nondefault
preferences can change the conclusion. These are not receiver `/info` keys
that our hook can simply advertise to force the phone.

The five-second adjustment hysteresis is confirmed in instructions at
`0x2510e028c`–`0x2510e0294`. The resulting FPS is returned in the virtual-display
`networkInfo` dictionary under **`recommendedFPS`** at `0x250f4ec24`–`0x250f4ec38`.
It is a recommendation/limit in the sender pipeline, not proof that a new
encoded picture is produced every 33.3 ms.

Evidence: [network policy](../output/ios-fps/ios27-screenstream-network.asm),
[initialization and preferences](../output/ios-fps/ios27-screenstream-create.asm),
[26.6 default initialization](../output/ios-fps/ios266-defaults.asm).

### 2. USB and stream 111

The previously unnamed getter is resolved: `APEndpointStreamScreenCreate`
reads `IsConnectedOverUSB` (`0x2510dfe40`–`0x2510dfe60`) and `IsCarPlay`
(`0x2510dfe88`–`0x2510dfea8`) from the endpoint description. If both are true,
the network policy chooses the maximum FPS directly. A connected charging
cable alone is not proof of that property: the session must actually be over USB.

The same creation function selects 110 versus 111 by comparing the display UUID
with the primary display UUID (`0x2510dfc00`–`0x2510dfc88`). The FPS policy does
not branch on that 110/111 field. `CarPlayAlternate` in activation options is
the **`DisplayLabel`**, not an FPS preset. The property resolves through
`0x1e0bfd590`; selection occurs at `0x251177358`–`0x2511773b0`.

This excludes a separate fixed 20 FPS policy in the inspected stream setup;
it does not prove all downstream display/encoder code treats both screens
identically. Evidence:
[activation options](../output/ios-fps/ios27-screenstream-display.asm).

### 3. Thermal limits are a different layer

27.0 `-[DBThermalMonitor suggestedFrameRateLimit]`, `0x25e43fb44`, returns:

| Internal thermal level | Default suggested FPS limit |
| --- | --- |
| Other levels, including normal level 0 | nil / no thermal limit |
| 1 | 15 |
| 2 | 5 |
| 3 | 5 |

The table at `0x2696b0fb0` references constant NSNumber objects at
`0x2767f5d18` (15) and `0x2767f5d30` (5). The nonzero internal preference
`FrameRateLimit` in `com.apple.carplay.internal` can override the constrained
levels. These internal levels are not Celsius temperatures.

In the 26.1 decompile, `DBSceneHostViewController.mm:1383`–`1398` applies
the monitor's limit to compatible scene settings; the instrument-cluster
layout controller also consumes it (`DBInstrumentClusterLayoutViewController.mm:126`,
`DashBoard_02.mm:956`). We have not established which scene/controller was
active during the user's lag, or that any thermal limit fired. The decoded
27.0 table does **not** explain a default 20 FPS cap.

Evidence: [thermal method](../output/ios-fps/ios27-thermal-fps.asm).

### 4. Frames can disappear before the TCP receiver

27.0 production `APVirtualDisplaySink` implements `vdsink_ShouldDropFrame`
at `0x250f52e80`: it compares the encoded sink queue duration with its configured
limit and returns true when the queue is full. It increments an overflow-drop
counter. This is the production sink, not the separate similarly named test sink
at `0x250f52d40`. Its queue-duration option is supplied by stream activation
(`MaxFrameQueueDurationMs`, `0x2511773b4`). This is backlog control, not a fixed
50 ms frame clock.

The sender distinguishes `submitSurfaceFPS`, sent-frame rate and
`sinkOverflowDropFPS` in its own statistics (`0x250f4f274`–`0x250f4f2d8`).
Therefore a recommended 30 FPS and fewer encoded/delivered pictures are not
contradictory. Neither this static drop path nor the thermal path proves which
condition actually occurred on the user's phone.

Evidence: [production drop decision](../output/ios-fps/ios27-vdsink-drop1.asm),
[sender processing/statistics](../output/ios-fps/ios27-screenstream-dequeue.asm).

### 5. Queue ownership and the 67 ms default (follow-up)

This is the phone's outgoing sample-buffer queue, not the renderer's decode
queue and not an out-of-memory crash. The same queue is passed from stream
storage `+0x68` into `APVirtualDisplaySinkCreate` (`0x2510e1108`–`0x2510e1134`),
retained at sink storage `+0x10` (`0x25104f030`), and filled by production
`vdsink_PushFrame` (`CMBufferQueueEnqueue`, `0x250f525dc`). The outgoing
`screenstream_dequeueAndProcessSampleBuffer` takes buffers from stream `+0x68`
(`0x250f4f054`–`0x250f4f058`). Its empty/nonempty queue trigger notifies the
transport via `screenstream_signalDataAvailable` (26.1 creation at
`AirPlaySender_18.mm:9973`; 27.0 signal function `0x250f52c70`).

Production `vdsink_Start` initializes its limit to **CMTimeMake(67, 1000)**
(`0x25104f3a0`–`0x25104f3bc`), then accepts an override from
`SinkMaxFrameQueueDuration`. Stream activation supplies 67 ms when using the
normal screen latency, or 5000 ms when the selected presentation latency
differs (`0x251176d94`–`0x251176dc4`). Do not assume the media-presentation
branch or an override was active on the user's phone.

`ShouldDropFrame` compares the queue's **accumulated media duration** against
this limit, not the age of the oldest picture on a wall clock and not a byte
count. At nominal 30 FPS, two 33.33 ms samples are just below 67 ms; three
are above it. It returns a drop recommendation for an incoming frame and
increments the overflow counter; this function does not flush the queue.

Consequently, a short interruption in draining this particular queue can
trigger drops even if average network bandwidth is ample. Transport stalls,
TCP retransmission/flow control or scheduling/processing stalls in the sender
are plausible reasons for slower draining, not observed causes in this audit.
A slow scene or encoder can also lower delivered FPS without filling this
outgoing queue; these must not be conflated.

Our possible downstream contribution is indirect: the hook synchronously
reads/decrypts/forwards each access unit (`altscreen_hook.c:1458`); forwarding
can wait for localhost writability (`altscreen_av.c:184`). A sustained stall
there could stop phone-socket reads, eventually fill the intervening TCP
buffers and propagate backpressure to the sender. Buffering can absorb short
stalls, so one renderer slowdown does not immediately prove phone queue overflow.
The renderer already has an independent socket reader and decode worker;
saved low worker ages argue against persistent decoder congestion in those
sampled windows. The overflow hypothesis remains unconfirmed on the HU.

Evidence: [sink creation](../output/ios-fps/ios27-sink-create.asm),
[enqueue](../output/ios-fps/ios27-sink-push.asm),
[sink start / default and override](../output/ios-fps/ios27-sink-start.asm),
[transport notification](../output/ios-fps/ios27-signal-data.asm).

## What remains unproven

- Current phone iOS build, actual USB/Wi-Fi transport, private preferences and
  thermal/scene state during the lag.
- Actual source-surface FPS versus encoder output and sender queue drops.
- Whether phone delivery stalls independently, or our synchronous
  receive/decrypt/forward thread applies backpressure.

Static evidence does not justify changing `maxFPS`, bypassing queue/thermal
limits, or claiming that the phone intentionally sends exactly 20 FPS. No
production frame-rate or buffering policy was changed in this follow-up.

## Scoped probe

Build with `LOG=0 AUDIO_STATE_TRACE=0 ALTSCREEN_PACE_TRACE=1
LOG_RGD_PACKET_RAW=0 bash scripts/build_hook.sh`.

Candidate: `build/diagnostics/altscreen-pace-20260915/libcarplay_hook.so`.
Normal production builds leave `ALTSCREEN_PACE_TRACE=0` (default). This probe
uses the existing bounded asynchronous logger, without enabling ordinary hook
logs, audio state tracing or media captures. The dedicated output is
`/tmp/carplay_alt_pace.log`. No additional process or shell monitor is added.

Each receiver starts a fresh accumulator and emits one summary per 300 video
access units, plus any remaining summary on exit. Fields:

| Field | Meaning |
| --- | --- |
| `n`, `fwd` | Received video access units and successfully forwarded units |
| `arrival_us` | Mean/max interval between completed reads from the phone |
| `samples`, `zero` | Arrival intervals counted, including same-time bursts |
| `read_us` | Mean/max duration reading header and body; includes network waits |
| `work_us` | Mean/max local decrypt, commands and forward duration |
| `pts_us`, `pts_n`, `bad` | Mean/max plausible sender NTP deltas, valid/invalid counts |

Only NTP deltas are recorded, never absolute timestamps or video bytes. The
header field comes from `Carplay/Sources/AirPlayCommon.h:2391` (params at offset
8), and `AirPlayReceiverSessionScreen.c:776` interprets params[0] as synchronized
NTP time. Missing, repeated, backward or over-10-second discontinuities are
excluded from PTS averages and counted as invalid. Sender PTS is presentation
timing, not a direct measurement of encode completion or TCP send time.

Interpret the simultaneous metrics, not one number:

- Arrival and valid PTS near 50,000 us, work small: stream carries roughly
  20-fps timing upstream of our renderer. Investigate sender policy/scene/network.
- PTS near 33,333 us, arrival near 50,000 us, work large: local receiver/forward
  path is falling behind a faster-timed source.
- PTS near 33,333 us, work small, long reads/bursts: transport or sender delivery
  stalls; this alone cannot distinguish Wi-Fi from iPhone scheduling.
- Hook arrival near 33,333 us but renderer arrival/output slower: examine the
  hook-to-renderer link, queue and decoder. Compare generation and time windows.

Deferred live check, only after the user resumes HU testing: retain the fixed
alt1 renderer, install only the diagnostic hook, reconnect CarPlay, collect
matching probe/renderer logs during an animated map for 30–60 seconds. Compare
USB versus Wi-Fi if available. The current instruction remains static only.

Validation: QNX ARM build and exact interpose-export checks; host hook suite;
ASan/UBSan for the accumulator and both scoped logger modes. Tests cover 20-fps
source timing, 30-fps timing with local stalls, burst-zero averaging, reconnect
window isolation, invalid PTS, and ordinary logs remaining disabled. The private
APAC recording is not in the isolated probe build; that optional capture test
skips. No decoder/presenter/frame-rate policy changes are part of this probe.
