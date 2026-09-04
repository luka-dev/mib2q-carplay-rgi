# Buffered audio across iPhone lock / app background

Status: the former stop/teardown failure is vehicle-fixed.  The deployed
same-handle/no-flush handoff survived repeated transitions and was initially
reported audibly excellent on 2026-09-04.  A later complete CarPlay connection
restart means that fast handoff is not yet release-accepted, although the saved
evidence does not show a hook crash: QSA writes were complete, teardown was
orderly, the final reason was `kConnectionErr`, and no new `dio_manager` core
was produced. Repeated next/previous tests then found and fixed a separate
deterministic AAC buffering error. A later long detached run exposed a hidden
QSA device underrun during a system CPU spike; the priority fix and direct
recovery telemetry are deployed for the next vehicle run, as described below.

## Symptom

With an ordinary Apple Music track using buffered type 103, audio stops when
the iPhone is locked or Apple Music leaves the foreground.  This was first
reported while the vehicle still had the 461990-byte hook and
`AUDIO_STEREO_UPMIX=ambient-lite`.  The local HQ build was not on the unit, so
the symptom is independent of `ambient-hq`.

## Live trace: the exact failure sequence

The retained host copy is `/tmp/mhi2q_lock_after.log`; the corresponding
control plists were copied to `/tmp/mhi2q_feedback.OSXJrM/` during the session.
Those `/tmp` paths are diagnostic evidence, not repository artifacts.

The lock/background transition was:

1. `00:11:42.897`: a per-stream TearDown without type 103 arrived.  The
   altScreen hook correctly classified it as partial and preserved stream 111.
2. `00:11:43.135`: stock closed the `mpl5_dio_ent` type-100 QSA handle.
3. Type 103 remained live.  At `00:11:46.659`, decoder telemetry showed the
   PCM ring at its full 24000 frames, `in=5504000`, `out=5046528`, and
   `drop=433472`.  The renderer was producing audio but no sink was consuming
   it.
4. The last valid stock feedback before the close was request 319:

   ```text
   { streams: [ { type: 100, sr: 48003.5546875 } ] }
   ```

   Requests 320, 321 and 322 at `00:11:44.887`, `00:11:46.915` and
   `00:11:48.915` returned a null response because stock had no realtime stream
   left to enumerate.
5. `00:11:50.438`: iOS sent `SETRATEANCHORTIME rate=0`.
6. `00:11:50.447`: iOS sent `FLUSHBUFFERED`.
7. `00:11:50.460..462`: iOS sent a type-103 TearDown; the APAP receiver exited
   with `stop=1` and the renderer was destroyed.

This falsifies the earlier first-pass idea that phone lock merely stops the
type-100 callback clock.  Two dependencies fail together:

- decoded type-103 PCM is drained only by stock type-100 QSA writes;
- type-103 flow control receives a measured clock only while stock can build a
  type-100 `updateFeedback` response.

When iOS backgrounds the otherwise unnecessary realtime stream, buffered media
loses both its physical consumer and its clock feedback.  The eventual
rate-zero/flush/teardown is a consequence, not the first failure.

## Implemented ownership handoff

The fix keeps the existing fixed PCM6/48 kHz/S16 contract and adds no thread:

- If stock closes `mpl5_dio_ent` while type 103 is running, the hook returns a
  successful close to stock but temporarily retains the already configured
  QSA handle.
- The existing APAP receiver thread prepares that retained handle after
  stock's flush and drains the same PCM ring in the measured 768-frame/16 ms
  device fragments.  Its blocking QSA writes provide the render clock.  APAP's
  1024-frame codec blocks are therefore re-fragmented without changing samples
  or decoder state.
- A new stock type-100 open closes the detached handle before reclaiming the
  normal bridge.  Type-103 teardown, replacement and process shutdown also
  close it.  No second simultaneous handle is opened against `mpl5_dio_ent`.
- The last valid stock clock-only `updateFeedback` plist is retained per
  AirPlay session.  Only while the same type-103 session is active, a null stock
  response is replaced by a retained copy of that exact plist.  The measured
  floating-point `sr` is not rounded or reconstructed.  The explicit
  `AUDIO_FEEDBACK_BLANK=1` failure-reproduction mode still bypasses this logic.
- QSA state and real I/O now use separate mutexes.  Calls into `libasound` no
  longer hold the state mutex that the `snd_pcm_write_internal` interposer
  re-enters.  This closes the previously documented `EDEADLK`/premature-unlock
  exclusion hole and serializes detached writes against close/reopen.
- The invariant `snd_pcm_plugin_setup` readback is logged once per handle, not
  roughly 62 times per second.  The old flood exhausted diagnostic history
  before a lock/resume sequence could be inspected.

The APAC/AAC decoder selection, native 5.1 reorder, stereo upmix, SAD metadata,
CSD device and amplifier contract are unchanged.

## Verification already complete

```text
bash scripts/test_hook_audio.sh     PASS
bash scripts/build_hook.sh          PASS (QNX 6.5 ARMv7)
git diff --check                    PASS
```

Artifact from the first complete build of this fix (rebuild before deployment
if the worktree changes):

```text
build/libcarplay_hook.so
size   524536 bytes
sha256 6223410c500d9130fe3173c18be390e0573a443a0a0413fcaa3fb4802a12e6b5

build/libcarplay_apac.so
size   678911 bytes
sha256 a6a7bc8c3f17dfe170765b04a461259a407269de47dde6fe0a2b3f6b2695044f
```

## Vehicle result: accepted continuity, transition still audible

The exact artifacts documented above were installed on the MHI2Q together with
`AUDIO_STEREO_UPMIX=ambient-hq`.  Their remotely read-back SHA-256 hashes matched
the local files byte for byte.  The old hook, Java and wrapper logs were cleared
before the run.

An ordinary AAC/index-23 track remained audible after the phone was locked.  The
fresh trace measured the intended ownership transfer:

```text
00:26:31.022  partial per-stream TearDown; type 103 preserved
00:26:31.258  retained mpl5_dio_ent for active buffered type 103
00:26:31.275  detached writes=1 zero=0 last=9216 expected=9216
```

The first detached write followed retention by about 17 ms.  From then until
the realtime stream returned, cached measured feedback was supplied about every
two seconds.  The detached writer reached 2412 writes with `zero=0`; the sink
telemetry also kept `zeroWrites=0`, `shortWrites=0`, `underrunFrames=0`, and
`devUnderruns=0`.  No `rate=0`, `FLUSHBUFFERED`, receiver exit, or type-103
TearDown occurred.

The pre-existing `drop=80448` was accumulated during initial startup, before
the first QSA media handle became available, and remained exactly constant
through the lock interval.  After handoff the ring commonly held only 704
frames, about 14.7 ms at 48 kHz.  That small reserve and the ownership transfer
are plausible boundaries for the brief jump heard during lock, but the trace
does not prove which was audible.  It does prove that the jump was not a zero or
short QSA write and did not develop into a stopped type-103 stream.

On foreground/realtime return the hand-back was also clean:

```text
00:27:10.305  closed detached handle; writes=2412 zero=0 result=0
00:27:10.308  captured the new stock mpl5_dio_ent handle
00:27:10.326  first stock write; zeroWrites=0 shortWrites=0
```

Playback continued normally according to the vehicle test.  Therefore the
lifecycle fix is accepted for continuity.  Eliminating the small transition
jump is a separate polish item; do not reopen the old feedback/consumer-loss
diagnosis unless a future trace again shows rate zero, flush, teardown, or
growing drops.

## Whole-period transition polish (vehicle-verified negative for lock gap)

The longer saved trace in
`captures/mhi2q_ambient_hq_lock_20260904_0030/` isolated one local PCM
shortage.  After iOS deliberately sent `rate=0` plus `FLUSHBUFFERED` while
changing AAC-LC stereo to APAC-5.1, the stock-owned bridge once read only 512
of its 768-frame period and zero-padded the remaining 256 frames.  QSA itself
did not underrun (`devUnderruns=0`) and accepted the complete 9216-byte write;
`underrunFrames=256` described our synthesized zero tail, not a device fault.

The bridge now treats a QSA period as indivisible.  A buffered read returns
either all requested frames or zero.  With a primed ring remainder of 256 or
512 frames, it leaves that remainder queued and widens the phone's realtime
stereo fallback for the complete period.  It never emits a fragment made from
a decoded prefix plus a zero tail.  The detached owner already required a
complete 768-frame period and is unchanged.  `rate=0`, flush-boundary discard,
decoder reset, cookies and rate-one resume are also unchanged.

Telemetry separates the two cases:

```text
underrunFrames=0
shortRingFallbacks=<whole periods substituted>
avoidedSilenceFrames=<zero-tail frames avoided>
lastReady=<decoded frames retained at the last substitution>
```

`underrunFrames` remains as a defensive invariant and must stay zero.  The new
1024-producer/768-consumer regression covers 256-, 512- and 767-frame
remainders and proves they are not partly consumed.  Host audio tests,
ASan/UBSan, the full hook suite, APAP/APAC integration and the QNX ARMv7 build
pass.

Deployed artifact used for the lock/unlock trace:

```text
build/libcarplay_hook.so
size   524825 bytes
sha256 dd1413c16ef9ade1f283630059449c9bb8b840a5848ff3fe4848ec9ddb9442ae

build/libcarplay_apac.so
size   678911 bytes
sha256 a6a7bc8c3f17dfe170765b04a461259a407269de47dde6fe0a2b3f6b2695044f
```

It was read back byte-for-byte from the unit, and startup was changed to the
requested A/B policy `hsr-td`, rear gain `0.18`, LFE `assist`.  Old main and
`spatial_*` diagnostics were cleared after the prior capture.  The next CarPlay
child generation will be the first run of this exact combination.

The subsequent vehicle run kept `underrunFrames=0`,
`shortRingFallbacks=0`, `zeroWrites=0`, `shortWrites=0` and
`devUnderruns=0` across lock and unlock, yet the brief interruption remained.
The all-or-zero period policy is therefore working, but it is not the source of
this particular transition gap.

## Physical QSA handoff polish (deployed; connection restart under audit)

The clean trace is retained in
`captures/mhi2q_hsr_whole_period_lock_20260904/`.  It showed two deterministic
physical state changes despite perfect ring and write counters:

```text
lock:   00:44:27.150 retain old handle
        00:44:27.169 first detached write after real flush + prepare

unlock: 00:45:14.061 close detached old handle
        00:45:14.062 open a new handle
        00:45:14.070 apply params
        00:45:14.078 prepare/setup
        00:45:14.084 first stock write
```

This is the surviving discontinuity.  A static audit was made against the
exact accepted stock `libairplay.so`, SHA-256
`01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2`.
Its only `snd_pcm_plugin_flush` xrefs are:

```text
SingleThreadPolicy::close  0x47744 flush; 0x4774c close
ObjectLockPolicy::close    0x48a94 flush; 0x48a9c close
```

Both load the PCM handle, derive playback/capture channel as 0/1, call flush,
reload the same handle, and immediately call close.  The flush return is not
tested.  There are no recovery or pause flush callers in this binary.  Open is
similarly narrow: `0x47884` and `0x48be8` only call `snd_pcm_open_name`; params,
prepare and setup are separate calls.  Prepare calls are `0x46688`/`0x479b8`,
followed by setup at `0x466d8`/`0x47a08`.  These recovered call sites establish
the two-argument `(snd_pcm_t *, channel)` flush/prepare contract; the hook does
not dereference an unknown object.

The deployed follow-up therefore changes ownership without changing QSA
state:

- For the expanded `mpl5_dio_ent` playback handle only, and only while type
  103 is active, the close-adjacent flush is deferred.  If close does not
  retain the handle after all, the real flush is restored before the real
  close, preserving the stock path.
- A retained handle whose flush was suppressed stays running; the detached
  writer no longer calls prepare.  The old prepare path remains as a fallback
  if type 103 became active only after a real flush.
- When stock type 100 reopens, it receives the same live handle between two
  complete 768-frame writes.  If the requested format is still
  PCM/S16/48 kHz and the target width is unchanged, exactly one redundant
  params and one redundant prepare are acknowledged without touching QSA.
  Real setup still reads and validates 6ch/48 kHz/9216 bytes and translates it
  to stock's 2ch/3072-byte fragment.
- Any geometry mismatch abandons the fast handoff and takes the real QSA path.
  No second handle and no new thread are introduced.

New telemetry makes every leg explicit:

```text
deferred close-adjacent flush ...
retained ... handoff=continuous
detached writes=1 ... handoff=continuous
reclaimed live ... without close/open
reclaimed params kept live ...
reclaimed prepare kept live ...
handoffFlush=1 reclaim=1/1/1
```

The deployed/read-back artifacts are:

```text
build/libcarplay_hook.so
size   528060 bytes
sha256 875139d48eb473db2390676d02b5dc15fe74cc127c8a9b11337cecbb0f3b241e

build/libcarplay_apac.so
size   678911 bytes
sha256 a6a7bc8c3f17dfe170765b04a461259a407269de47dde6fe0a2b3f6b2695044f
```

Host audio tests, the complete integration suite, ASan/UBSan, QNX ARMv7 build,
ELF symbol audit, remote byte comparison and `git diff --check` pass.  Startup
remains `hsr-td`, surround `0.18`, LFE `assist`; the three diagnostic logs were
zeroed after deployment.

### First vehicle result

The complete snapshot is retained in
`captures/mhi2q_hsr_seamless_handoff_20260904/`.  Lock used the same
`handle=6e0be8` throughout: the close-adjacent flush was deferred at
`00:58:16.007`, the handle was retained with `handoff=continuous`, and the first
detached write was a full 9216 bytes.  Unlock at `00:58:24.884` reclaimed
`6e0be8` after 542 full detached writes, skipped compatible duplicate params and
prepare, and obtained the correct real setup.  No detached close, new open, or
new handle occurred.  A second lock repeated the continuous transfer, and the
detached writer later passed 1024 full writes with zero failures.  The user
reported playback as excellent during this live run.

One edge was visible in telemetry.  iOS had sent two explicit
`FLUSHBUFFERED` controls 209/139 ms before the first foreground open, leaving
QSA in status 4 (`SND_PCM_STATUS_UNDERRUN`) because there was intentionally no
buffered media to write.  The first stock write returned zero, stock recovered
with real prepare, and every later write was complete.  Repeated
`(9216 of 9216)` WARN lines were a diagnostic edge-trigger bug, not repeated
write failures; `zeroWrites` stayed 1 and `shortWrites` stayed 0.

The local follow-up now asks the real device status before suppressing the
reclaimed prepare.  Only `SND_PCM_STATUS_RUNNING` skips it; an intentional
flush/underrun prepares immediately instead of discovering the same state via
a zero write.  The WARN is also edge-triggered correctly.  This refinement
passes host audio/integration tests and the QNX build but was deliberately not
installed while the successful vehicle session remained live:

```text
build/libcarplay_hook.so
size   528573 bytes
sha256 c9410850a154e5ea1485125434116eb58f362bafb9eb188a360865a154696d52
```

### Later connection restart: not a demonstrated hook segfault

The subsequent incident snapshot is retained in
`captures/mhi2q_hsr_seamless_handoff_crash_20260904/`.  The original
`dio_manager` PID `72769671` disappeared and `smartphone_integrator` started
PID `85991498`.  Before exit, however, the old process continued to render with
`fill=512`, `drop=0`, and full detached 9216-byte writes.  At
`01:07:53.491` its APAP thread exited with `stop=1`, then closed the detached
QSA handle cleanly (`writes=3434 zero=0 result=0`), stopped the decoder and
alt-screen receiver, and processed full-session teardown.  The final teardown
reason at `01:07:54.390` was `-6753`, CoreUtils `kConnectionErr`.

There is no new `dio_manager*.core.gz` at the incident time and no signal,
fault, abort, QSA write failure, or hook error before the orderly stop.  The
measurement therefore proves a complete CarPlay connection/process restart,
not a segfault in the hook.  It does not prove what caused the connection to
be lost, so same-handle reclaim remains under audit rather than accepted or
convicted.

The replacement process explains the user's "hung, then the next song
started" observation.  Type 103 started at `01:08:02`, but the first type-100
QSA handle did not open until `01:08:10.761`.  During that gap no measured
feedback cache existed, iOS flushed, the PCM ring filled and startup drop
reached 178752 frames.  Once type 100 returned, PID `85991498` stayed alive and
rendering resumed.

### Repeated next/previous lag: wrong receiver buffer family for index 23

The same replacement process was observed through several manual track
changes.  It did not restart again; QSA continued to accept full fragments and
the PCM drop counter stayed constant.  The measured flushes were:

```text
AAC -> AAC   FLUSH 01:14:00.101, boundary 01:14:01.198, 497 packets, 1.097 s
AAC -> APAC  FLUSH 01:14:33.148, boundary 01:14:33.816, 477 packets, 0.668 s
APAC -> AAC  FLUSH 01:17:48.099, boundary 01:17:48.184,   1 packet,  0.085 s
```

Decoder dispatch itself was correct: the unchanged type-103 session rebuilt
AAC-LC -> APAC-5.1 -> AAC-LC from the changed cookies.  The lag followed the
amount of old buffered AAC data that had to cross the inclusive
`flushUntilSeq` boundary.

The captured SETUP request negotiated `audioFormatIndex=23`, yet the hook's
response returned `audioBufferSize=21168000`.  Static receiver work already
established that AAC-LC/AAC-ELD compression types 4/8 use 5,000,000 and APAC's
type 0 uses 21,168,000.  `spa103_audio_buffer_size()` still recognized only the
old AAC indices 40/42; it omitted the newly shipped stereo indices 22/23.  This
is a definite metadata-family bug, regardless of how much of each measured
pause the sender buffer size ultimately removes.

The follow-up adds 22/23 to the AAC family, keeps 40/42, and regression tests
all four against APAC 64/65/66.  It also contains the status-gated reclaim
prepare and edge-triggered write warning described above.  It was deployed and
read back byte-identically after the incident logs were preserved. The next
fresh index-23 session then returned the correct `audioBufferSize=5000000`, so
that live verification gate is closed:

```text
build/libcarplay_hook.so
size   528773 bytes
sha256 d0bdb72382791b647e097f86b8804388180e03b3e8083137802cbd8b45041769

build/libcarplay_apac.so
size   678911 bytes
sha256 a6a7bc8c3f17dfe170765b04a461259a407269de47dde6fe0a2b3f6b2695044f
```

`scripts/test_hook_audio.sh`, the QNX ARMv7 build, and `git diff --check` pass.
The deployed `875139d4...` build also emitted 1240 false synchronous warnings
after one real zero write because it tested the cumulative `zeroWrites`
counter instead of the current result; the local candidate removes that log
load without hiding any write failure.

### Hidden detached QSA underrun under scheduler pressure

The correct 5,000,000-byte session is retained in
`captures/mhi2q_aac_buffer_5m_20260904/underrun/`. From `01:24:35` onward the
decoded rail and wrapper appeared healthy: ring fill stayed at 448 frames,
startup `drop=5440` never increased, every detached write returned the full
9216 bytes, and `zero=0`. Nevertheless the system slog proves one physical
recovery at `01:26:53`:

```text
deva_trigger: ADO_PCM_TRIGGER_STOP
session_csd_stop_playback
deva_prepare
deva_trigger: ADO_PCM_TRIGGER_GO
```

This was `mpl5_dio_ent`, not a CarPlay control transition: there was no nearby
`FLUSHBUFFERED`, track boundary, hook teardown or receiver exit. The same
heartbeat sample reported 98% system CPU. The custom APAP receiver is also the
detached QSA writer; it still ran at QNX's default priority 10 while stock's
audio receiver uses priority 18. Eight 16-ms QSA fragments provide only 128 ms
of scheduler tolerance. Therefore full plugin-write return values did not
exclude a device underrun: libasound recovered it internally and hid it behind
the successful return.

The smallest follow-up creates no thread and changes no PCM/codec algorithm.
The existing APAP receiver/QSA writer now enters `SCHED_RR` at priority 18
(`AUDIO_BUFFERED_THREAD_PRIO`, falling back to stock's
`AUDIO_RECEIVER_THREAD_PRIO`). Detached telemetry polls real device status and
counts any prepare reached after detached rendering begins as
`detachedRecoveries`. Host audio tests, the QNX ARMv7 build and
`git diff --check` pass. The deployed/read-back candidate is:

```text
build/libcarplay_hook.so
size   531252 bytes
sha256 6aee7e0397a65f5efac3921b7efe1497cc1572836d417d86a0a7910f805af135

build/libcarplay_apac.so
size   678911 bytes
sha256 a6a7bc8c3f17dfe170765b04a461259a407269de47dde6fe0a2b3f6b2695044f
```

The staged runtime policy is `hsr-td-active`, surround 0.20, LFE `assist`, and
buffered priority 18. A new vehicle process must confirm the priority log and
hold `recoveries=0`; the one-event/98%-CPU correlation is strong but is not yet
proof that priority alone eliminates every underrun.

The first run of that exact candidate used PID `122302538` and confirmed every
runtime gate: `hsr-td-active`, `rearGainQ15=6554`, LFE assist, index-23 receiver
buffer 5,000,000, and `APAP receiver/QSA writer priority=18 policy=SCHED_RR`.
It reached 16,384 detached periods, exactly 262.144 seconds of PCM, with every
write 9216/9216, QSA status `RUNNING`, `underruns=0` and `recoveries=0`. This is
well past the previous recovery at 144 seconds.

The user heard two brief freezes during this run, but neither crossed the
measured audio path: consecutive PCM-stat windows remained exactly 196,608
input and output frames per 4.096 seconds, ring fill/drop were constant, and
there was no deva STOP, control transition, cookie switch or decoder rebuild.
Both reports overlapped full 1.3 MB `sloginfo` extraction over SSH while the
unit heartbeat showed 94--100% CPU. That measurement activity is a plausible
disturbance, not a proved cause. Full slog extraction is therefore deferred
until playback stops; live monitoring uses only the small hook counters.

## Reproduction checklist

1. Park the vehicle; deploy and restart `dio_manager`.
2. Start an ordinary AAC/index-23 track and wait until
   `bufferedWrites` is advancing.
3. Lock the phone for at least 20 seconds, then unlock it.
4. Repeat by leaving Apple Music in the background for at least 20 seconds.
5. Confirm continuous audible playback and these transitions:

   ```text
   audio sink: deferred close-adjacent flush ...
   audio sink: retained mpl5_dio_ent ... handoff=continuous
   audio sink: detached writes=1 ... handoff=continuous
   buffer103: updateFeedback published detached QSA clock ...
   audio sink: reclaimed live ... without close/open
   audio sink: reclaimed params kept live ...
   audio sink: reclaimed prepare kept live ...
   ```

6. Confirm no premature `SETRATEANCHORTIME ... rate=0`, no type-103 TearDown,
   no detached zero/short write, and bounded PCM-ring fill/drop counters.
7. Bring Apple Music/phone foreground back. Confirm the pointer in `reclaimed
   live` is the same pointer retained at lock, normal `bufferedWrites` resumes,
   and neither `closed detached` nor a new `captured ... handle` appears.

If the new fast handoff cannot be used, the log deliberately says
`prepare-required`, `geometry changed`, or returns to `closed detached`; that
fallback must be investigated from the same trace rather than hidden with
silence or a second uncoordinated QSA handle.

## AAC-to-APAC auto-next stall: the mixed stream needs the larger buffer

The first priority-18 session later produced a different, fully captured
failure.  The evidence is under
`captures/mhi2q_hsr_td_active_prio18_20260904/post_2min_auto_next_incident/`.
PID `122302538` did not crash or restart, QSA remained `RUNNING`, every device
write remained complete, and both the direct and deva underrun/recovery counts
stayed zero.  `updateFeedback` was also sane: 48000 initially and
47999.2695/47998.8984 around the transition.

iOS itself initiated the item change at `01:39:08.184` with
`FLUSHBUFFERED[2]`, then sent new cover art and a new 33-byte APAC cookie.  The
flush covered sequence 6086178 through 6089320: 3143 packets, or 67.0507 s at
1024/48000.  The hook crossed the inclusive boundary at `01:39:09.049` after
discarding 2691 packets (57.408 s) and brought up the six-channel APAC decoder
at `01:39:13.376`.  Decoded APAC then advanced only through
`01:39:16.580`; no further decoded packet arrived until `01:40:01.869`.
During that approximately 45.3-second sender pause the PCM ring drained to a
256-frame remainder and the whole-period policy correctly substituted the
phone's realtime stereo instead of emitting silence.  That is why the audible
result sounded like the next item had started while the buffered item had not
cleanly changed.  It was not an HQ CPU failure or a QSA underrun.

The SETUP for this session selected AAC-LC index 23 and returned the recently
changed single-format AAC default, `audioBufferSize=5000000`.  That value is
statically correct for an AAC-only receiver, but the deployed stream is not
AAC-only: `/info` advertises `[65,23]`, and iOS changes AAC/APAC per item inside
the same type-103 connection without another SETUP.  Its one immutable buffer
contract must therefore cover the largest advertised codec family.  The
strongest vehicle A/B is an earlier index-23 session with
`audioBufferSize=21168000`: it switched to APAC at `01:14:33.818` and then
decoded continuously for more than three minutes, until the deliberate next
change at `01:17:48.098`.

The replacement keeps the recovered per-index helper unchanged (AAC alone is
still 5,000,000; APAC alone is 21,168,000) but takes the maximum over the
selected index and the complete advertised offer.  Thus `[65,23]` returns
21,168,000 even when SETUP initially selects 23.  Host render, switch, APAP and
buffer-contract tests pass.  The hook was deployed and read back byte-exact:

```text
libcarplay_hook.so  532641 bytes
sha256 9a6a73bd3430d9648f74991c68e10221ccdbe3168cc0b0d8bc632239d28c33cb
libcarplay_apac.so  678911 bytes
sha256 a6a7bc8c3f17dfe170765b04a461259a407269de47dde6fe0a2b3f6b2695044f
```

The remaining live gate is a fresh SETUP showing 21,168,000 followed by a
natural or manual AAC-to-APAC transition without a long PCM-stat gap.  Until
that repetition passes, the mixed-buffer diagnosis is a strongly supported
candidate rather than a claimed final proof.

The first fresh run accepted that contract on the car.  Evidence is in
`captures/mhi2q_music_stage_mixed_buffer_20260904/good_live/`: SETUP returned
`audioBufferSize=21168000`, the renderer started with `music-stage`, surround
0.20, LFE assist and `SCHED_RR` priority 18, and iOS changed APAC to AAC inside
the same type-103 session at `02:00:41.891`.  PCM statistics then continued at
exact 196,608-frame intervals, stock and detached QSA writes remained complete,
`status=RUNNING`, and the zero/recovery/underrun/short-ring counters remained
zero through the captured window.  This closes the APAC-to-AAC direction; the
longer AAC-to-APAC direction remains the decisive repetition of the former
45-second stall.

## Late-song discontinuity: do not freeze a handoff sample-rate transient

The next `music-stage` run is captured under
`captures/mhi2q_music_stage_mixed_buffer_20260904/late_underrun_check/`.
It ran from `02:08:10` until orderly connection teardown at `02:21:32`.
The PCM/QSA path itself remained clean: every observed write was 9216 bytes,
device status stayed `RUNNING`, and `devUnderruns`, `recoveries`, `zeroWrites`,
`shortWrites`, and `shortRingFallbacks` all stayed zero.  A filtered
`sloginfo -m 13103 -t` capture and the full slog taken after playback stopped
contain no hidden `STOP -> prepare -> GO` in the retained `02:17:12`--`02:21:32`
window.  The only STOP is the expected final teardown.

The audible-shaped discontinuities were controls from iOS rather than short
device writes.  One `rate=0`/`FLUSHBUFFERED` interval left 44.396 seconds
between decoded PCM windows.  Later, `02:15:17`--`02:15:27` contains eight
FLUSH requests, new RTP anchors and cover-art changes from image 6 through 12.
Suppressing those controls would be wrong: a real seek or item change must
discard old compressed and decoded data.

The captured feedback plists exposed a separate late-song risk.  Before the
first detached interval, stock measured `sr=48005.39453125`; the hook replayed
that value while type 100 was absent.  After the rapid control/item storm,
stock briefly returned `48082.29296875` and then `48080.2265625`.  Type 100
closed at `02:15:30`, so the old implementation froze `48080.2265625` into
every response until it returned at `02:20:06` and stock again reported
`48000`.  That is +0.167% for about 276 seconds, an implied timeline error of
roughly 0.46 seconds.  It is a measured and time-compatible cause candidate,
not yet a proved cause of the user's late-song symptom.

The local, **not deployed**, follow-up removes that unbounded replay:

* the first 256 successful detached QSA writes are discarded as queue/handoff
  warmup;
* 1024 subsequent 768-frame writes measure the sink using `CLOCK_MONOTONIC`
  (a 16.384-second measurement window, about 20.48 seconds after handoff);
* until the first clean window, feedback publishes the fixed sink contract
  `48000`, not the last stock transient;
* a write gap over 100 ms, a short/failed write or a recovery invalidates the
  measurement and returns to nominal warmup;
* each detached response is a fresh verified stock-shape plist,
  `{streams:[{type:100,sr:<nominal-or-measured>}]}`;
* the previously hidden `write==0 -> prepare -> retry` path now increments and
  logs `detachedRecoveries`, even when the retry accepts all 9216 bytes.

No CFLite ABI was guessed.  The accepted MHI2Q `libairplay.so`
(`01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2`)
exports `CFDictionarySetDouble` at `0x56ad0`; its ARM body takes the double in
`r2/r3`, creates `kCFNumberFloat64Type` (13), and inserts it into the mutable
dictionary.  The new pure clock regression covers warmup, exact 48-kHz
measurement, long-gap reset, recovery reset, an implausible-rate window and an
invalid timestamp.  All host audio tests and the QNX ARMv7 build pass.
Candidate artifacts are:

```text
libcarplay_hook.so  sha256 8ef70b59a7b1ebe9c8cf6a78939349e2963c4d0c4215a95a66f821ac31229a6e
libcarplay_apac.so  sha256 a6a7bc8c3f17dfe170765b04a461259a407269de47dde6fe0a2b3f6b2695044f
```

The candidate was deployed on the user's explicit request after the static and
host/QNX checks.  SCP readback is byte-identical to the artifacts above, the
startup profile remains `music-stage` / surround `0.20` / LFE `assist` /
buffered priority 18, and new `dio_manager` PID `213557292` maps both
`libcarplay_hook.so` and `libcarplay_apac.so`.  Deployment proves installation,
not the clock hypothesis: it still needs a full foreground/background or
type-100 handoff before being described as vehicle-tested.
