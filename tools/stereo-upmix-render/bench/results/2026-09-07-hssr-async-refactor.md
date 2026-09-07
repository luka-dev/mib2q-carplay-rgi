# HSSR block/async-controller refactor — 2026-09-07

## Scope

The production renderer now has a block/hop entry point and explicit analysis,
control and mask/synthesis stages.  FFT, masks and overlap-add stay on the APAP
audio thread.  Only the recurrent GRU controller moves to a persistent worker
through two-entry SPSC job/result mailboxes.  Every job/result carries a
generation and sequence; reset invalidates stale work without waiting.

Offline tools retain the synchronous controller.  Their pre/post-refactor
V11 checksum for 512 measured blocks after 128 warm-up blocks is identical:

```text
checksum_fnv1a64_le=0x122aaab3bf2e0152
```

That checksum closes the block/worker refactor itself. The later same-day
Cabin-Aware LFE V2 replacement intentionally changes LFE/front samples; its
current checksum and regression are recorded in
[`2026-09-07-cabin-aware-lfe-v2.md`](2026-09-07-cabin-aware-lfe-v2.md).

## Host audio-thread latency

Apple Silicon host, 1024-frame calls, V11 model, 64 warm-up plus 256 measured
blocks.  Both runs used real-time pacing outside the measured region so cache
and worker scheduling are comparable.

```text
sync:  mean 405638 ns, p50 377125, p95 629250, p99 751250, max 917167
async: mean 333291 ns, p50 330583, p95 404958, p99 414417, max 424542
```

The async split reduced mean calling-thread latency by 17.8% and p99 by 44.8%
in this run.  All 639 feature jobs completed; `busy=0`, `stale=0`.  Async mean
was 1.56% of the 21.333 ms block budget.  Host timing is evidence for the split,
not a substitute for a live Krait/QNX wall-time trace.

## ARMv7/QNX instruction regression

GCC 8.5, QEMU `-icount`, 32 warm-up plus 128 measured blocks, real-time pacing:

```text
sync:  mean 5459845, p99 5471028 estimated retired instructions/block
async: mean 5470226, p99 5480321 estimated retired instructions/block
```

Total retired work rose 0.19%, the expected mailbox/generation overhead; the
benefit is moving GRU latency off the audio thread on a multicore target, not
reducing total instructions.  The async run completed all 319 jobs with zero
busy or stale events.  QEMU instruction counts are not vehicle CPU cycles.

## Verification

- `scripts/run_tests.sh`: pass.
- block entry versus legacy sample entry: float/S16 bit-exact.
- synchronous V11 render checksum: unchanged.
- worker mailbox and reset-generation test: pass.
- ASan/UBSan worker and upmixer tests: pass.
- ThreadSanitizer worker test: pass.
- QNX GCC 8.5 hook build: pass, including emutls/runtime/constructor guards.
- QNX GCC 8.5 standalone benchmark build/run: pass.

The remaining release gate is the stationary on-unit listen plus live worker
statistics.  A healthy session should end with `busy=0` (or isolated events),
`stale=0` except intentional resets, and `completed` close to `submitted`.
