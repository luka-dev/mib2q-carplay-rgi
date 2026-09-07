# Stereo to 5.1: HSSR Universal

Status: implemented, host-tested, GCC 8.5 ARMv7/QNX-buildable, and selected by
the checked-in vehicle policy.  The working tree contains one upmix algorithm,
`hssr-universal`.  `off` is retained only as an exact safety bypass.  Superseded
experiments and their measurements remain recoverable from Git history.

## Contract

The buffered AAC decoder produces 48 kHz interleaved S16 stereo.  Universal
produces the fixed six-channel rail:

```text
L, R, C, LFE, Ls, Rs
```

- Digital C is always zero because J525 derives its centre from L/R/LFE.
- Fronts, rears and optional LFE share the HSSR 512-frame latency.
- Rear gain is bounded to `0.0..0.25`; deployment selects `0.25`.
- `assist` is Cabin-Aware LFE V2 only. There is no legacy implementation or
  runtime fallback.
- V2 combines per-side LR4 low-pass branches at 55 Hz (body) and 95 Hz
  (punch), capped at Q15 `8192` (`0.25`).
- Low-band correlation continuously selects a normalized `L+R` or `L-R` fold,
  so coherent anti-phase bass is preserved instead of disappearing.
- The V11 low-band bass/drum posteriors and a sample-domain transient envelope
  steer the body/punch blend and bounded dynamics. They do not delay or replace
  the sample-domain waveform path.
- Enabling LFE selects the paired front gain Q15 `30100` (about `0.919`).
- A per-sample feasible-interval limiter applies the exact recovered J525
  profile-0/bank-1 gains to `L/R/LFE/Ls/Rs` and keeps every known logical bus
  within `0.985` full scale. The static centre-row worst case remains below
  unity as a second configuration-time guard.
- Native decoded APAC 5.1 bypasses the stereo upmixer.
- Realtime processing allocates nothing and accepts blocks up to 1024 frames.

With `AUDIO_STEREO_UPMIX=off`, output is exactly
`L,R,0,0,0,0`, ignoring all gain/LFE settings.  Missing, corrupt or
dimension-mismatched model data also leaves upmix off; it never silently picks
another renderer.

## Universal routing

The causal 512/128 STFT feeds a quantized two-layer GRU semantic controller.
The DSP retains one persistent coherent object slot per stereo half-plane,
requires agreement between semantic, pan, ridge/transient and slot evidence,
projects through the measured source vector, and hard-steers only the rear
output vector.  A source-aware `2.5x` front transfer reduces front anchoring for
accepted rear objects.

When no strong object is accepted, a bounded mid/high fallback prevents dead
rear channels.  It is frequency-steered and uses a conjugate phase pair so Ls
and Rs remain distinct.  Vocal and bass posteriors veto this path rapidly;
upper-percussion transients have a separate presence curve.  This fallback is
a perceptual coverage bed, not a claim of studio-stem recovery.

## Runtime split

The production entry point is block-based.  It advances input in chunks ending
at the next 128-frame STFT hop and runs three explicit stages:

1. window/FFT/spatial analysis;
2. feature extraction and learned-control exchange;
3. mask construction, spectral routing, inverse FFT and overlap-add.

Stages 1 and 3 always remain on the APAP audio thread.  Only the two-layer GRU
in stage 2 runs on one persistent `SCHED_RR` priority-17 worker, below the
priority-18 receive/decode thread.  The audio thread uses no mutex, condition
variable, allocation, log call or wait: it publishes to a two-entry SPSC
feature queue and reads a two-entry result mailbox through 32-bit acquire/
release operations.  Both control steps in one 1024-frame callback can be
queued, so the GRU still consumes the trained 93.75 Hz feature sequence.

The low-band bass/drum controls used by LFE V2 travel through their own
512-frame sample-indexed delay ring. They therefore stay aligned with the
delayed L/R waveform and remain byte-stable across arbitrary callback sizes.

A result is first eligible one 512-frame control period after submission.  This
adds 10.667 ms to learned-control reaction, not to the 512-frame waveform/front/
rear latency.  If the worker misses one or two deadlines, DSP keeps the last
stable controls.  From the third consecutive miss it decays semantic/object
masks toward a quiet rear instead of blocking or emitting an unbounded result.
If worker creation fails, the renderer retains the synchronous GRU path.

Every upmixer reset advances a nonzero 32-bit generation.  That covers prefix
FLUSH, pause/rate teardown, decoder-cookie or codec changes, SETUP replacement
and session stop.  Jobs and results carry generation plus sequence; the worker
resets its private recurrent state on a generation change and rechecks the
generation after inference before publishing.  The audio thread rejects stale
results unconditionally.  Host/offline tools leave the provider unset and
therefore keep the original bit-exact synchronous render for reproducible
model evaluation.

## Runtime policy

```sh
export AUDIO_STEREO_UPMIX=on
export AUDIO_HSSR_MODEL=/mnt/app/root/hooks/hssr_model.bin
export AUDIO_STEREO_SURROUND=0.25
export AUDIO_STEREO_LFE=assist
```

The variables are parsed at process start, so changing them requires a
`dio_manager`/CarPlay child restart.  There is no build-time profile selector:
all builds contain Universal only.

## Verification

The unit tests cover exact OFF passthrough, fail-closed model handling, latency,
block/sample and chunk/reset determinism, overlapping input/output, bounded
C/LFE/rears, V2 anti-phase preservation, high-frequency rejection, all-six-bus
Cabin bounds, signal-derived fallback, worker mailbox/generation rejection, and
AAC/APAC cookie switches.  The public-authored corpus has
seven fixtures (five authored stereo/5.0 pairs, one E-AC3 JOC Atmos/LFE sample,
and one EBU channel test).

After removal of all old implementations and the HSSR profile switch, every
Universal render in that corpus remained byte-for-byte identical to the
pre-removal baseline.  The chosen model is:

```text
build/hssr-selection-v11/hssr_model.bin
SHA-256 ab01463e9359f013c4b40f311d630bd20af48d761ad361563cb5246f6bd686ae
```

See
[`../tools/stereo-upmix-render/bench/results/2026-09-07-hssr-selection-v11.md`](../tools/stereo-upmix-render/bench/results/2026-09-07-hssr-selection-v11.md)
for selectivity, corpus scores and timing.  Objective tests guard safety and
regression.  The block/worker split, deadline evidence and sync parity are in
[`../tools/stereo-upmix-render/bench/results/2026-09-07-hssr-async-refactor.md`](../tools/stereo-upmix-render/bench/results/2026-09-07-hssr-async-refactor.md).
Stationary vehicle A/B remains the final listening check.
