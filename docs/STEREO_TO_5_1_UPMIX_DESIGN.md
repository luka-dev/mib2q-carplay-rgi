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
- LFE assist is a cascaded Butterworth LR4 low-pass at 80 Hz, capped at Q15
  `5833` (about `0.178`).
- Enabling LFE selects the paired front gain Q15 `30802` (about `0.94`).
- The known J525 centre row remains below unity:
  `0.501187*L + 0.501187*R + 0.316228*LFE`.
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
chunk/reset determinism, overlapping input/output, bounded C/LFE/rears, signal-
derived fallback, and AAC/APAC cookie switches.  The public-authored corpus has
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
regression; stationary vehicle A/B remains the final listening check.
