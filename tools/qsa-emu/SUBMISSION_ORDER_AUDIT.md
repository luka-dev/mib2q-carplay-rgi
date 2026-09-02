# QSA/deva CSD submission-order audit

This document is the complete record of the CODEX_PROMPT_3 measurement. It
preserves the static recovery, harness design, failed approaches, exact results,
artifacts, and remaining boundary so none of this work has to be rediscovered.

## Scope and conclusion

The accepted upstream facts were:

* QSA widens six-channel S16 to S32 by shifting every sample left 16 bits.
* Channel order is unchanged.
* All 62 written fragments in deva's DMA ring are byte-exact.
* The six-channel CSD descriptor is live and coherent: 48 kHz, six channels,
  signed, interleaved, 24 bits/sample in four-byte containers, seven 18432-byte
  buffers.

The remaining question was whether deva consumed the correct ring contents in
the wrong order or multiplicity. A cursor advancing by the client fragment size
`0x2400` instead of the widened container size `0x4800` would submit half a
fragment per tick and provide a concrete factor-of-two mechanism.

The measured answer is negative. In the frozen 62-fragment window:

* all 61 consecutive cursor deltas are `0x4800`;
* written generations 1 through 62 are each submitted exactly once;
* no generation is repeated, dropped, reordered, or unknown;
* 1,142,784 ring bytes are written and 1,142,784 ring bytes are submitted;
* the reconstructed submission-order PCM is bit-identical to the reference.

The ring and submission path is therefore a faithful single pass through
`csd_write`. The vehicle fault boundary moves downstream of `csd_write`. This
measurement does not identify which downstream DSP/hardware operation is wrong.

## Binary identities

Patched `deva-ctrl-qc.so` used by the harness:

* ELF32 little-endian ARM EABI5 PIC, image base 0;
* `.text` VA/file offset `0x84f8`;
* GNU build ID `9168acff553fc86d6a3c991613d620fd`;
* SHA-256
  `8b55b8d8f02b97d6f30191b54ef8a847e4706067f21cbf068d4b02a563ea84c9`.

The staged `io-audio` is ELF32 little-endian ARM, non-PIE, image base
`0x100000`, GNU build ID `f4fdcfac42030d7fa81271715fcb06b5`. Its preserved
firmware SHA-256 is
`b38fafad979fa2e320e7111d122cd5ec1f4d7737744b29e2cbf767d1b6b875ef`.
This measurement did not modify `io-audio`, `tap_preload.c`, or the existing
deva binary patch.

## Static recovery

### Ring-to-CSD submission

`session_csd_stream_playback` begins at deva `0x69084`.

* `0x6924c..0x6926c` copies one complete fragment from
  `dma_base + session_cursor` into a reserved CSD pool buffer.
* The copy length comes from the live CSD buffer geometry and is `0x4800`
  (18432) for the six-channel stream.
* The function builds a 36-byte descriptor at `stack + 0x20`.
* Descriptor word 0 is the submitted pool-buffer address.
* Descriptor word 1 is the audio payload length.
* `0x69318` calls `csd_write(handle, descriptor, 0x24)`.
* deva compares the return value with descriptor word 1, not with the 36-byte
  descriptor length. The DSP stub must therefore return 18432 for this stream.

The `memcpy` source address at `0x6924c` is the exact pre-advance ring cursor.
Associating that typed copy with the immediately following `csd_write` recovers
the ring-relative offset without guessing the physical CSD address mapping.

### Timer and cursor

`session_deva_timer_thread` reaches the playback path around `0x6e85c`.

* It calls the local data-dump routine at `0x6e85c`.
* `0x6e864..0x6e874` loads the live cursor, adds `dma + 0x94`, stores the new
  cursor, and wraps against the ring size at `dma + 0x10c`.
* `0x6e890` calls imported `dma_interrupt` after that cursor update.

For the measured stream, `dma + 0x94` is `0x4800` and the malloc-backed ring is
`10 * 0x4800 = 0x2d000` bytes.

The local `deva_timer_data_dumps` call cannot be LD_PRELOAD-interposed because
the call is bound directly within deva. Its absence from an interposer log does
not mean the timer failed to advance.

### Actual CSD-backed sink

Opening `mpl5_dio_ent` alone does not execute `csd_write` in this minimal guest.
That endpoint has the MIB/void timer side of the graph and normally depends on
the vehicle audio manager to keep the downstream path alive.

The CSD-backed path is the preferred output selected by
`default_device=six_channel_speakerp`. `deva_playback_6ch_aquire` at `0x4946c`
reaches `deva_csd_start_playback` at `0x497a8`. `qsa_probe` must open the
preferred playback device in submission-measurement mode.

### CSD callback protocol

Recovered commands and events used by the stub:

| value | meaning | payload |
|---:|---|---|
| `0x11401` | register callback | `{function, context}` |
| `0x11404` | stop | no payload |
| `0x11601` | buffer done | submitted `{address, length}` pair |
| `0x11602` | buffer ready | not needed for playback measurement |
| `0x11603` | EOS | not needed to produce the pre-teardown artifact |

The six-channel CSD stream is handle 257 and has seven 18432-byte PMEM buffers.
It issues two zero-filled startup-prime `csd_write` calls before the first
ring-backed copy. Those primes are tracked separately from ring consumption.

## Measurement implementation

### Written-generation tracking

The existing `resmgr_msgread` interposer observes each accepted QSA write after
the kernel copies it into the malloc-backed DMA ring. Each complete accepted
fragment receives a monotonically increasing generation number, stored for its
ring slot. A zero plugin return remains a failed/EIO write and does not advance
the input file or generation.

`qsa_probe` keeps `/proc/boot/music6.s16` open and reads the next file fragment
only after the current 9216-byte S16 fragment has been fully accepted. It does
not refill one fixed buffer from the beginning of the file.

### Pull/submission association

The stub's `memcpy` interposer recognizes the typed 18432-byte copy whose source
lies in the six-channel ring. It records:

* ring number and source pointer;
* ring-relative cursor offset;
* ring slot and current written generation;
* CSD pool destination and copy length.

The following `csd_write` consumes that pending record, appends the pool data's
high 16 bits to the submission-order S16 render, updates cursor-delta and
generation multiplicity counters, and returns descriptor word 1.

### Buffer recycling at the existing timer boundary

Calling BUF_DONE synchronously from inside the next `csd_write` re-enters deva
at an unsafe point and faults on the third ring-backed submission. Suppressing
callbacks avoids that fault but exhausts the seven CSD pools after two startup
primes plus five ring submissions.

No new io-audio thread is used. Instead, submitted descriptors are queued and
completed on the same existing deva timer thread after its cursor update.

deva imports `dma_interrupt`, but the definition in the main `io-audio`
executable takes ELF lookup precedence over the preload library, so defining
another `dma_interrupt` in the stub does not intercept the call. The first
imported operation inside io-audio's real `dma_interrupt` is
`pthread_mutex_lock` at runtime VA `0x10f7a8`; its relocation/GOT slot is
`0x124f78`.

At callback registration time, after lazy loading is initialized, the harness:

1. resolves the real libc `pthread_mutex_lock` with `dlsym(RTLD_NEXT, ...)`;
2. saves that typed target;
3. redirects only io-audio's resolved GOT slot `0x124f78` to a stub wrapper;
4. requires `pthread_self()` to match the thread that just called `csd_write`;
5. records the post-cursor timer boundary and drains queued BUF_DONE callbacks;
6. calls the saved libc mutex function.

The wrapper never dereferences the opaque `ado_pcm_subchn_t *` or any unrecovered
parameter. The public recovered prototype is
`void dma_interrupt(ado_pcm_subchn_t *subchn)`.

### Startup prefill and exact window

With `SND_PCM_START_DATA`, the traced timer can consume slot 1 before the probe
writes generation 2 because serial logging is much slower than the real system.
That produces a harness-only generation-zero startup underrun. Measurement mode
therefore uses `SND_PCM_START_FULL`, allowing QSA to prefill the ring before the
timer begins.

The result is frozen and emitted immediately after the 62nd real post-cursor
timer boundary. Waiting until STOP lets the timer pull a stale slot after the
client's final fragment. The frozen marker is the experiment boundary; anything
after `DSP_SUBMIT_COMPLETE` is teardown activity.

`qsa_probe` polls `/tmp/csd_submit_count`, then remains silent while io-audio
emits base64. It waits for `/tmp/csd_submit_emitted` before printing or flushing.
This matters because the QEMU serial console can interleave processes byte by
byte; a probe status line printed during export prefixes individual `DSP64`
lines one character at a time and corrupts carving.

## Failed approaches and traps

| approach | observation | implication |
|---|---|---|
| Drive only `mpl5_dio_ent` | no ring-backed `csd_write` | use preferred `six_channel_speakerp` sink |
| Return 36 from `csd_write` | deva compares against 18432 and rejects it | return descriptor word 1 |
| Complete previous buffer inside next `csd_write` | guest faults on submission 3 | callback point is re-entrant and unsafe |
| Disable callbacks | stable, but only five ring submissions after two primes | seven CSD pools must be recycled |
| LD_PRELOAD a `dma_interrupt` symbol | wrapper is never called | executable symbol has precedence |
| Globally interpose `pthread_mutex_lock` with lazy `dlsym` | io-audio faults during loader startup | never install a process-wide mutex wrapper |
| Save the constructor-time GOT value | value is a lazy binder trampoline; indirect call faults | resolve libc and patch after callback registration |
| Use `SND_PCM_START_DATA` | slot 1 may be consumed with generation 0 | use `SND_PCM_START_FULL` for traced measurement |
| Emit only at STOP | a 63rd stale slot can be pulled first | freeze at the 62nd timer boundary |
| Print probe status during base64 | status characters prefix `DSP64` lines | wait for the emission sentinel |
| Count calls instead of full write returns | probe claims 62 while only one crossed resmgr | require the full fragment return |
| Advance the file after a zero write | validator/input indices diverge | retry the same fragment after prepare |

The post-marker stale pull seen in diagnostic runs was slot 2 carrying old
generation 53. It follows the correct generation-62 submission and is caused by
the live timer racing final flush/STOP. It is deliberately outside the frozen
62-fragment artifact and must not be reported as an in-window repeat.

### Diagnostic run ledger

These logs are retained under `build/qsa-emu/` as provenance. Only
`qsa-submit-final2.log` is the accepted result; the others intentionally capture
partial or rejected harness states.

| log | purpose/result |
|---|---|
| `qsa-submit-serial.log` | drove `mpl5_dio_ent`; proved that endpoint alone never reaches `csd_write` |
| `qsa-submit-preferred.log` | reached the preferred CSD sink; synchronous callback re-entry faulted during the third ring submission |
| `qsa-submit-nocallback.log` | stable diagnostic with callbacks disabled; two primes plus five data buffers exhausted all seven pools |
| `qsa-submit-dmaintr.log` | demonstrated that a preload `dma_interrupt` definition cannot override the main executable's symbol |
| `qsa-submit-mutex.log` | rejected global mutex interposer; loader/startup recursion crashed io-audio |
| `qsa-submit-got.log` | constructor-time GOT redirection was overwritten by later lazy resolution |
| `qsa-submit-finalcandidate.log` | preserving the unresolved binder trampoline as the pass-through target faulted |
| `qsa-submit-got2.log` | first stable recycled-pool run; START_DATA caused generation 2 to be missed and STOP allowed a stale 63rd pull |
| `qsa-submit-final.log` | START_FULL produced the correct 62-generation window, but concurrent probe output corrupted serial base64 |
| `qsa-submit-final-clean.log` | emission wait was still too short; the probe status line interleaved midway through base64 |
| `qsa-submit-final2.log` | accepted clean window and clean export; all counters and PCM pass |
| `qsa-submit-definitive.log` | rejected attempt to emit only from immediate STOP; STOP still raced a 63rd stale pull into the statistics |

## Definitive measurement

The checked serial window reports:

```text
DSP_VERIFY ring=0 source=resmgr voices=6 fragments=62 bytes=1142784 mismatches=0 CONTINUOUS_PASS
TIMER_CURSOR_SUMMARY ring=0 advances=62 delta_0x4800=61 delta_0x2400=0 delta_other=0
CSD_CURSOR_SUMMARY submissions=62 delta_0x4800=61 delta_0x2400=0 delta_other=0
CSD_REGION_SUMMARY written_generations=62 distinct_submitted=62 submitted_once=62 repeated_generations=0 duplicate_submissions=0 missing_generations=0 unknown=0
CSD_BYTE_SUMMARY submitted=1142784 written=1142784 difference=0
CSD_TOTAL_WRITE_SUMMARY csd_write_calls=64 ring_calls=62 preroll_calls=2 ring_bytes=1142784 preroll_bytes=36864 total_payload_bytes=1179648
CSD_SUBMIT_RENDER_SUMMARY fragments=62 frames=47616 samples=285696 pcm_bytes=571392 reference_mismatches=0 overflow_calls=0
=== DSP_SUBMIT_COMPLETE ===
```

The three requested numerical answers are therefore:

1. Cursor delta: `0x4800` on 61/61 transitions; `0x2400` on 0; other on 0.
2. Multiplicity: 62 distinct written generations, 62 submitted once, 0 repeats.
3. Ring bytes: 1,142,784 submitted and 1,142,784 written; difference 0.

The two startup primes add 36,864 payload bytes before ring consumption. They
are real CSD startup submissions but are not written ring generations, so they
are excluded from the ring equality question and comparison WAV. The separate
total of all CSD payload calls through the frozen boundary is 1,179,648 bytes.

## Rendered artifact and hashes

Known-good reference:

`captures/broken_5_1_20260901_drive/session6_5_1_declared_order.wav`

Checked serial log:

`build/qsa-emu/qsa-submit-final2.log`

* SHA-256:
  `6d051876b2eec395a4f04781333fd29f5a1798d1dc92ee677589122d3d4f7d92`

Submission-order WAV:

`build/qsa-emu/submission-order-final2/dsp_submission_order.wav`

* geometry: 6 channels, 48,000 Hz, S16LE;
* frames: 47,616;
* duration: 0.992 seconds;
* PCM bytes: 571,392;
* peak: 12,526;
* RMS: 1598.896;
* mismatches against the first 47,616 reference frames: 0;
* PCM SHA-256:
  `f0f37e6c1f2e5d32d0c938053b9f2f70fbac25868e61f6e1fe2342ae0c90c312`;
* WAV-file SHA-256:
  `4daa9fc9915ce9929fa1874d2a4c5bf531d996be4c733cb3e12d5feae4909112`.

The WAV contains the ring-backed audio in actual `csd_write` order. It is
bit-identical to the known-good input segment and therefore does not resemble
the car's repeated-fragment symptom.

## Reproduction

Build:

```sh
./tools/qsa-emu/build_ifs.sh
```

Run:

```sh
qemu-system-arm -M virt -m 512 -cpu cortex-a15 -icount shift=auto \
  -device loader,file=build/qsa-emu/qsa.bin,addr=0x40200000,force-raw=on,cpu-num=0 \
  -display none -serial stdio -monitor none \
  > build/qsa-emu/qsa-submit.log 2>&1
```

The known teardown fault can occur after `DSP_SUBMIT_COMPLETE`; stop QEMU after
the complete marker if necessary.

Carve and compare:

```sh
python3 tools/qsa-emu/carve_renders.py \
  build/qsa-emu/qsa-submit.log \
  build/qsa-emu/submission-order
```

Expected carver result:

```text
frames=47616 duration=0.992000s samples=285696
SUBMIT_HIGH16 bit_identical=True mismatches=0 peak=12526 rms=1598.896 pcm_sha256=f0f37e6c1f2e5d32d0c938053b9f2f70fbac25868e61f6e1fe2342ae0c90c312
```

## Files carrying the measurement

* `csd_stub.c`: ring pulls, generations, descriptor capture, callback queue,
  exact timer boundary, counters, and serial export.
* `qsa_probe.c`: full-file sequential playback, accepted-write accounting,
  preferred CSD sink, START_FULL prefill, and silent export coordination.
* `qsa.build`: enables `QSA_SUBMIT_MEASURE` for io-audio and the probe.
* `carve_renders.py`: extracts submission-order base64, writes WAV, and compares
  PCM against the reference.
* `README.md`: combined transport, DSP-interpretation, descriptor, and
  submission-order conclusions.
* `HANDOFF.md`: current operational summary and traps.

## Remaining boundary

No byte, geometry, cursor-step, ordering, or multiplicity fault remains between
the APAC decoder output and the data handed to `csd_write`. Future vehicle work
should begin downstream: CSD IPC/DSP interpretation, DSP-side scheduling, DSP
processing, or hardware output. The existing low-24-versus-high-16 render is a
qualitative DSP-format hypothesis, not a demonstrated description of what the
vehicle DSP does.
