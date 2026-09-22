# CarPlay call/Siri priority over buffered media

Update 2026-09-17: the owner confirms mode transitions, including music and
calls/Siri, are closed on the current vehicle setup. The implementation and
historical validation below are retained; the September 10 live-call note is
superseded by the [current project status](../../TODO.md).

2026-09-10. Implemented and host-tested; **not yet verified with a live HU call**.
No change to voice codecs, microphone PCM, upmix, LFE or loudness processing.

## Evidence and scope

The local iOS 26.1 AirPlaySender dump constructs `type=100` streams with
`audioType=telephony`, optional `vocoderInfo` and the `input` flag. Its
`carPlayAudioFormat_getFormatsKeyForAudioType` selects `audioInputFormats` for
main telephony/speechRecognition, independently of media output capabilities.
See `AirPlaySender_01.mm` around line 4070 and `AirPlaySender_13.mm` around 9479
under `Refferences/iPhone18-3_26.1_23B85_Restore/.../AirPlaySender/`.

Pioneer SPHERA 01030010 `libSPF_CP_PlugInWrapper.so`, `AudioStreamStart`
at `0x14d210`, starts main input when requested and main output separately
from buffered 103 and alternate 101. Type 102 is not a prerequisite for calls;
100 is not a synonym for music, and 101 is not a synonym for Siri.

MU1316 MHI2Q P5087 stock `eso/lib/libairplay.so`, verified in IDA:

| Function | Address | Relevant behavior |
| --- | --- | --- |
| `get_audio_path` | `0x49398` | Telephony 3 → `tel2_dio_phone`; speech 4 → `ann4_dio_voice`; media → `mpl5_dio_ent`. |
| `AudioStreamPrepare` | `0x4a9d0` | Reads 32-bit streamType at object byte offset 108 and audioType at 112. Main 100 declares audioType, waits for HMI ACK, then opens QSA. Input uses `mic2_dio`. |
| `AudioStreamStop` | `0x4a2f8` | Synchronously joins workers and calls teardown callbacks before returning; also closes QSA objects. |
| `_AudioStreamCopyProperty` | `0x49a90` | Requires prepared state and does not return audioType; unsuitable for pre-Prepare classification. |
| `AudioStreamStart` | `0x4a73c` | On failure calls `AudioStreamStop` for cleanup. |

The stock ELF has `R_ARM_JUMP_SLOT` relocations for both Prepare and Stop;
the hook exports both entry points via `carplay_hook.exports.map`.
Private fields are read only after the existing reviewed AirPlay 210.81
firmware fingerprint passes; offsets are part of that profile, not generic
assumptions about every AirPlay implementation.

MU1316 `dio_manager`: `s_audioTypeCallback` at `0x154e58` queues the global
main-audio type; `s_audioTornDownCallback` at `0x154dbc` queues -1. Neither is
scoped to a QSA handle. The Java `CarPlayDSIManager.updateMainAudioType` replaces
the active audio resource, so an out-of-order hook Media request can interfere
with telephony even though the underlying PCM devices are separate.

## Fixed race

Previously a retained media QSA handle scheduled a Media declaration 50 ms
after stock close. The timer did not know whether phone/Siri had started, and
ran before checking whether 103 was playing. A paused, still-live 103 could
therefore overwrite a newer Telephony declaration. Hook teardown also cleared
its ownership flag under a lock but called global tornDown after unlocking.

The new ownership policy in `hook/audio/media_route.h` is:

1. Before **every main-100 Prepare**, register the stock object and relinquish
   hook ownership without sending tornDown. All main purposes take precedence,
   not only the currently known phone/Siri values. Alternate 101 is unaffected.
2. Keep the object registered throughout stock activity and Stop, including its
   worker joins and teardown callback. An older media Stop cannot release a
   newer phone or Siri object. A failed new Prepare removes only its own new
   registration; a duplicate failed Prepare preserves the existing owner.
3. Cold bootstrap and the detached 103 writer cannot claim Media while any
   stock main object is registered. The detached pump also yields instead of
   consuming PCM during this interval.
4. After the last stock Stop, apply the existing 50-ms settle interval. Reclaim
   Media only with PLAYING, primed, full-period buffered PCM. A living paused
   receiver never forces playback or route acquisition. A stock media reopen
   takes ownership normally instead.
5. Check hook ownership and issue any global release under the same `routeLock`
   as the stock-begin transition. Never send a hook release after ownership
   has transferred to stock. Never hold this lock over real Prepare/Stop:
   their HMI wait, QSA re-entry and worker joins must run without it.

Tracking uses eight fixed slots, no new thread and no heap allocation. Capacity
overflow fails closed for hook route claims for the process lifetime, leaving
stock calls operational. The supported lifecycle assumes stock serializes
operations on one AudioStream object; distinct overlapping objects are tracked
independently.

## Transport, PCM I/O and feedback hardening

The follow-up implementation keeps the same DSP/upmix/LFE settings, with four
changes in `audio_hook.c` and small host-testable policy headers:

1. Unexpected APAP TCP EOF, framing rejection or receiver failure now sets
   `failed/stop` and renderer `TRANSPORT_FAILED`, clears queued PCM after any
   reserved read completes, invalidates the clock cache, and closes a detached
   QSA handle before publishing worker exit. An old realtime 100 timeline is
   **not** substituted automatically. Recovery requires a fresh media Prepare
   or a new successful 103 SETUP. A separate invalidation gate prevents even
   that fresh Prepare from reading the old ring while it is being retired.
   A normal requested 103 teardown can still fall back to the existing 100.
2. Expanded PCM scratch is reserved before configuring PCM6 (1 MiB maximum),
   never reallocated in `plugin_write`. Invalid alignment/channel geometry,
   oversized writes and missing scratch return an error; they never send the
   original stereo bytes to an expanded device. Partial writes commit only
   the accepted, whole-frame prefix of the buffered transaction.
3. Media QSA writes use nonblocking mode and descriptor readiness polling,
   at most one device period per native write, 10-ms polling slices and a
   250-ms transfer budget. Zero/EAGAIN is backpressure, not an automatic
   prepare; detached prepare+retry is allowed only for confirmed UNDERRUN
   and only while media remains eligible. That single retry has its own
   250-ms budget. Route changes cancel subsequent writes. Unrelated phone,
   Siri and microphone prepare/write/flush/close bypass media `ioLock`.
   The callback/ownership lock is separate (`ioLock → routeLock → state`).
   Native device status queries also run outside the shared state mutex.
   Receiver teardown shuts down connected sockets to wake readers, waits
   up to 2.5 s for actual worker exit and joins only exited workers. On timeout
   it retains the old generation, decoder, ring and thread ownership; it
   never uses `pthread_cancel`, detaches or reuses live state. A later SETUP
   can retry cleanup, but cannot replace a still-running generation.
4. Stock main objects now retain their audio purpose and a route epoch.
   Feedback is cacheable only for known media with no competing non-media
   main object. Cache publication, synthesized fallback and the diagnostic
   blank-stream mode all require an unchanged epoch across stock Control.
   Telephony/speech/unknown main purposes leave stock feedback untouched and
   invalidate the music cache. A complete call occurring inside a slow query
   therefore cannot leave a telephone clock in the music cache, even at 48 kHz.

The nonblocking contract follows QNX's
[plugin write documentation](https://www.qnx.com/developers/docs/6.5.0SP1/neutrino/audio/libs/snd_pcm_plugin_write.html),
[nonblocking mode API](https://www.qnx.com/developers/docs/6.4.0/neutrino/audio/libs/snd_pcm_nonblock_mode.html)
and [PCM readiness guidance](https://get.qnx.com/developers/docs/6.4.1/neutrino/audio/pcm.html).
The transfer budget bounds normal buffer-readiness waits, **not arbitrary
hangs inside the resource manager, native open/prepare/status/close or a broken
kernel**. Quarantining a worker avoids freeing memory it might still touch;
it does not promise playback recovery from a permanently hung driver. The
2.5-s budget is for the receiver-worker wait, not a hard real-time guarantee
for every setup, codec cleanup or stock AirPlay operation.

## Validation and next vehicle check

### Follow-up: transactional params and SETUP lifetime

The second safety pass (2026-09-10) adds three safeguards without changing DSP:

- Media params are a serialized `UNKNOWN → CONFIGURING → READY/FAILED`
  transaction under `ioLock`. Only READY permits writing. Both expanded params
  and a successful stock rollback require authoritative `plugin_setup`
  readback of format/rate/width (and the fixed period for expanded PCM).
  A double failure, allocation failure, failed nonblocking setup or mismatched
  readback leaves the device quarantined from writes. The last expanded flag
  is no longer treated as proof that an unsuccessful rollback produced stereo.
  Subsequent setup-readback failures also latch FAILED; only fresh successful
  params can reopen writing. A failed handle cannot be retained/reclaimed for
  detached playback, pumped or used as a detached feedback clock. Unrelated
  stock devices remain passthrough.
- A buffered SETUP ticket is recorded before stock Setup and revoked before
  stock TearDown. A late response cannot rearm a retired ticket. The session's
  private address-family byte is copied while holding the same lock, before
  any stop/join/model-load/decoder/socket work. After that, setup uses the
  session pointer only as an identity, never to read its mutable internals.
  Request and response CF dictionaries have explicit temporary references.
  After blocking work the ticket/generation/stop conditions are checked again;
  cleanup carries its original generation so it cannot stop a newer one.
  Thus a timed-out setup can safely unwind after stock teardown clears the
  session; no indefinite wait or assumed CFRetain contract for opaque session
  internals was introduced. Native stock's own synchronization is unchanged.
- The bounded writer now returns accepted bytes **and** a separate stop reason.
  Partial success preserves cancellation, timeout or device error through the
  QSA adapter and subsequent status/logging calls. The stock-facing count is
  still converted to source-channel bytes, and the ring commits only accepted
  frames. `QSA_IO_ABORT` includes both `result` and `error`, including positive
  partial results. Native short writes accompanied by an error are not blindly
  retried; zero/EAGAIN/EWOULDBLOCK remains bounded backpressure.

QNX documents readback after successful params in
[snd_pcm_plugin_params](https://www.qnx.com/developers/docs/6.5.0SP1/neutrino/audio/libs/snd_pcm_plugin_params.html)
and the configuration-query contract in
[snd_pcm_plugin_setup](https://www.qnx.com/developers/docs/6.5.0SP1/neutrino/audio/libs/snd_pcm_plugin_setup.html).

### Follow-up: generation isolation across reconnect

The third safety pass (2026-09-10) closes two additional, reproduced
in-flight replacement races. No DSP or advertised audio-format change:

- Replacement SETUP validates its request ticket **inside** the receiver-stop
  transaction, under the same lock that claims `joining`. A stale request
  returns cancellation before stopping anything. Even cleanup with no receiver
  reserves `joining` while closing global detached QSA state, preventing a new
  SETUP from publishing a device that the older cleanup could close.
- Buffered controls snapshot both SETUP sequence and receiver generation before
  parsing. Receiver updates require both to match. The renderer also owns a
  generation: rate changes and software FLUSH check it under `render.lock`,
  rather than trusting an earlier receiver check. FLUSH rechecks after each
  condition-variable wait; it cannot wake into a successor's PCM reservation.
  Stop invalidates the renderer generation and wakes stale waiters.
- Prefix FLUSH carries that generation into its later native QSA phase and
  rechecks after acquiring `ioLock`. New-generation writes cannot reach the
  device while that lock is held. An old software prune therefore cannot
  become a device flush of the new playback queue.

A parsed command superseded during processing is acknowledged and discarded;
`BUFFERED_CONTROL_STALE` identifies the method, SETUP sequence and generation.
This guards commands already in flight across replacement; it does not add
new HTTP-connection/session identity mapping. Actual stock RTSP connection
ordering remains part of HU validation.

### Native QSA re-entry constraint

The transactional lock must allow native QSA to query its own geometry.
In MU1316, `snd_pcm_plugin_write` calls an internal helper that re-enters
`snd_pcm_plugin_setup` through the PLT. That call must receive the real PCM6
configuration, not the stereo view exposed to stock AirPlay, and must not
try to acquire the same non-recursive `ioLock` again. The hook tracks the I/O
owner with `pthread_t` under the state lock (no TLS/emutls dependency).
Setup queries from that owner call native QSA directly; other callers remain
serialized and receive the stock-facing view. This applies even after the
tracked handle has been cleared during detached close.

Evidence from read-only ARM disassembly and ELF dynamic relocations:

- File: `Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/ifs2/ifs_coreservices3/lib/libasound.so.2`
  relative to the `AUDI_2` root; ELF32 ARM little-endian EABI5.
- SHA-256: `c1ead2649d32d25f2d40fa8c61706e7600e413c89599a4aebb1a8a5ff2209ae2`.
- `snd_pcm_plugin_write` starts at VA/file offset `0x1012c`; calls at
  `0x101f0`, `0x10254` and `0x10290` target helper `0xfe70`.
- Helper call `0xfeac → PLT 0x4190 → GOT 0x20d64` resolves to
  `snd_pcm_plugin_setup` (native implementation `0xfd98`). Flush also calls
  the helper at `0x10300`.
- Native setup calls `snd_pcm_channel_setup` at `0xfdc4` through PLT `0x440c`
  / GOT `0x20e38`; this lower-level function is not interposed. Native write
  at `0x10260–0x10268` also confirms that an error can set errno while returning
  an already accepted positive byte count.

The native-write mock deliberately re-enters the exported setup interposer
and asserts native channel geometry; a separate external setup call asserts
the stereo view. This is a regression test of the hook's re-entry behavior,
not execution of the ARM library on the host. No firmware bytes were changed.

### Test coverage

- `scripts/run_tests.sh`: PASS, including `media_route_test` and existing
  buffered lifecycle, PCM, soundcheck and real APAC capture regression tests.
- New tests cover music→call→resume, cancelled/failed Prepare, duplicate
  Prepare, phone/Siri overlap, cold 103 during a call, pause, insufficient PCM,
  teardown103 during a call, capacity overflow and 20,000 concurrent stock /
  buffered ownership iterations per thread.
- Follow-up tests: `pcm_io_test` covers bounds, aligned accepted prefixes,
  device failures, cancellation and timeout; `media_transport_test` covers
  EOF recovery latches and fresh-Prepare/invalidation races;
  `receiver_wait_test` exercises actual condition-variable timeout and later
  worker completion; `media_route_test` adds feedback epochs, unknown purpose
  and stock priority publication while media I/O is locked. These test the
  production helpers, not the real QNX driver or an end-to-end RTSP server.
- `scripts/test_audio_interposers.sh` now compiles the actual `audio_hook.c`
  and framework `airplay_seams.c` with mocked QSA/CF/AirPlay/loader boundaries.
  It exercises allocation rejection, writes during params, double failure with
  old PCM6 still configured, verified stereo rollback, live reclaim, bad
  readback, nonblock failure, partial-error errno preservation and actual
  buffered peek/write/commit accounting. 64 controlled concurrent SETUP /
  teardown cases cover cancellation before the hook response and timeout
  during renderer initialization, with stock freeing the session before setup
  resumes. It also checks later SETUP progress and old-generation cleanup.
  The generation regressions add 32 late SETUP cases (same and different
  session pointers), absent-receiver cleanup exclusion, all four buffered
  handlers across a parser-time replacement, current pause/resume, replacement
  after the receiver check, FLUSH waking into a different PCM generation and
  replacement between software pruning and native QSA flushing. Current
  generation FLUSH still reaches the mocked native flush/prepare exactly once.
  Run separately with `SANITIZE=address,undefined` or `SANITIZE=thread`.
  The test header under `tests/mocks` is not a QNX ABI model; it is never used
  by shipping builds. These tests do not exercise native ARM dispatcher code,
  real QSA timing, successful end-to-end RTSP negotiation or audible playback.
- GCC 8.5 QNX/ARM build: `LOG=0 AUDIO_STATE_TRACE=1`; exact-export and loader
  safety checks pass. These are host/state-machine and binary checks, not a
  substitute for a live phone/HU audio test.

On HU, test incoming answer and reject, outgoing call, remote/local hangup,
Siri, phone foreground/background and reconnect during a call. Check both
microphone and remote voice, then music resume (including deliberately paused
music). Use `AUDIO_ROUTE action=stock_begin phase=prepare audio_type=3/4` and
the matching `stock_end phase=stop_done` events. Between them there must be no
hook `action=request` or `action=release`. After hangup, expect either stock
main media ownership or a hook request after buffered PLAYING resumes.

The initial development checks were offline. A later native-hook deployment and
read-only capture on 2026-09-10 provide partial on-unit validation:

- Deployed hook SHA-256:
  `55acdb1d7ecd0f9cecf40e416adcb1a9fa0eb192bed157a17e8662b1349b8a32`.
- Local private capture: `logs/captures/state-trace/live-20260910-nav.wInRTV/review.md`
  (capture files remain ignored by Git).
- About 207 seconds of music, four AAC/APAC format switches and 14 successful
  FLUSH operations; 49 QSA health snapshots reported no device underruns or
  write/recovery failures. Both `QSA_IO_ABORT` error 47 events followed teardown
  and were cancellations, not 250-ms transfer timeouts (stop took 70/66 ms).
- Decode/auth/upmix/sequence/queue failures were zero in that capture.
- Audible call/microphone quality and a full music-to-call-to-resume/Siri cycle
  were not verified. The capture does not replace those acceptance tests.

On 2026-09-11 the host suite was rerun successfully, including the real APAC
reference test, and the production-interposer suite passed with
`SANITIZE=address,undefined`. No new vehicle deployment accompanied the commit.

Additional HU checks for this follow-up: disconnect the APAP TCP sender and
verify `BUFFERED_TRANSPORT_FAILED` (no continuing RUNNING claim); reconnect
with fresh SETUP and verify a new generation. Check call/Siri interruption
during media buffer stalls, `QSA_IO_ABORT`, no music-clock substitution during
voice feedback, and normal music resume after hangup. A simulated permanently
stuck writer should yield `BUFFERED_STOP_TIMEOUT quarantined=1`, with no new
generation until the previous worker truly exits. Native QSA polling cadence
and underrun recovery remain **vehicle-validation pending**.

## Stock write contract: never return a negative (dio_manager core, 2026-09-19)

`/mnt/ota/system/core/dio_manager.core.gz` from the first CarPlay generation after boot
(uptime 00:01:44): thread 54 = stock `CAudioOutputThread::thread` (libairplay 210.81
`+0x4bcb0`) for main audio type 100, SIGSEGV in libc `memset` with `r4 = 0xffffffd1`
(= -47 = QNX `ECANCELED`). Decompiled chain:

1. `CQNXSoundIF::write` (`+0x484c8` / `+0x471bc`) calls `snd_pcm_plugin_write` and, when
   the result is below the request, runs status → prepare → second write and returns
   `write2 + max(write1, 0)`; a negative errno from either write is returned **unchanged**.
2. `CAudioOutputThread::thread` treats that result as an unsigned byte count:
   `memcpy(buf, buf + result, v6 - result)` and the next read callback is asked for
   `v6 - (v6 - result) = result` bytes.
3. `AirPlayReceiverSessionReadAudio` → `RTPJitterBufferRead(jb, buf + v6 + 47, -47)` →
   `memset(dst, 0, 0xffffffd1)` runs until the next unmapped page.

libasound itself never returns a negative from `snd_pcm_plugin_write` except `-EINVAL`
(block-mode size mismatch); native failures surface as `0` with `errno`. Our bridge
returned `-ECANCELED` when `spa_qsa_cancelled()` was true with nothing written yet
(type-100 started 0.8 s after the type-103 detached handle was released), and `-EBUSY` /
`-EIO` / `-EINVAL` / `-ENOMEM` on the other early exits.

Fix: `spa_stock_write_result()` in `audio_hook.c`. Every bridge-path failure is a stock-
native short write (`0`, stock does status → prepare → rewrite); a deliberately cancelled
fragment reports itself fully consumed (`size`, errno `ECANCELED`) so the stock timeline
keeps pacing instead of spinning on its 30 ms select. `tests/audio_interposer_test.c`
pins both. Core-file recipe (QNX notes `QNT_CORE_STATUS`=8 / `GREG`=9 / `LINK_MAP`=11,
8-word link-map records) lives in the session scratch `qnxcore.py` and in memory
`stock_qsa_write_negative_return_crash`.
