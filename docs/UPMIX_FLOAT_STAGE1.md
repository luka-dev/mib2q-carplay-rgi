# Next upmix: float/cabin-aware headroom foundation

Historical stage-one snapshot. Subsequent hook integration/training work is
tracked in [UPMIX_CABIN_V12.md](UPMIX_CABIN_V12.md).

2026-09-08. Implemented and host-tested, **not deployed or selected by the
production audio hook**. Same V11 model; no retraining yet. Current worktree
also contains earlier static research, which was preserved.

## Implemented

- `spa_stereo_upmix_process_f32`: unchanged source analysis and model; float
  front transfer, rear shelf and LFE rendering. Float units are S16 units,
  where 32768 is full scale. The HSSR front scratch is an exact delay of S16
  source samples, not an intermediate synthesis quantizer. Existing defensive
  HSSR/biquad bounds remain. Legacy front/rear/LFE rail limiting is not applied
  in the candidate: common protection follows programme gain instead.
- `spa_soundcheck_apply_f32`: same metadata, duplicate/media-kind handling and
  sample-counted ramps as the legacy API. No Sound Check gain enters V11
  analysis. `spa_soundcheck_limit_matrix_f32` monitors the actual PCM rail plus
  optional downstream linear buses; applies only one common gain to source
  channels. `spa_soundcheck_quantize_s16` is the final boundary conversion.
- Candidate CLI `--float-render` opts into that path, with the fixed recovered
  profile-0/bank-1 6x8 matrix as detector sidechain. -1 dBFS sample ceiling,
  instantaneous attack and existing 100-ms linear release policy. No lookahead,
  true-peak claim, acoustic compensation or inverse OEM EQ.
- Explicit offline Sound Check target/tuple flags and a reproducible A/B tool.
  Fixture tuples are gain-order stress cases, not claimed sender metadata for
  these WAVs. `--surround 0.25` is OUR synthesis gain, unrelated to MMI minimum.
- `digital_stages.py`: finite-audio selectors, split-half pair mixer, explicit
  steady gain rows, packed output EQ with supplied five-word coefficients,
  ordered signal IDs. Missing complex DSP remains an error/unmodelled boundary.
  No fictional eight-bus -> ten-bus identity or physical driver duplication.
- Cabin state annotations now distinguish assumed MMI OFF/All/minimum from
  unknown live processing gates and numerical Surround. Existing 19-driver /
  16-circuit topology and inferred fan-out weights are unchanged.
- Sound Check finite extreme metadata is bounded before float-to-int gain
  conversion, avoiding undefined conversion for huge finite values. Non-finite
  samples cannot propagate through final quantization.

## Why the detector must include the input matrix

The first float candidate used rail-only protection. On Crazy Train p1, its
known downstream input-matrix peak was 1.030049 FS at unity and **1.155696 FS**
with the +1 dB fixture, despite legal individual output channels. This is a
failure of that intermediate candidate, not proof that the old shipping path
clipped on this track. The final candidate's single linked detector watches
both domains before quantization, without applying the matrix twice to PCM.

## Verification

Same model in every render:
`ab01463e9359f013c4b40f311d630bd20af48d761ad361563cb5246f6bd686ae`.

Fourteen held-out local excerpts (seven songs x two excerpts, ten seconds
each), four variants = 56 renders; additionally 14 pre-change references.
Every legacy WAV is byte-identical to a renderer built from the original
HEAD CLI/upmix sources. No source audio or renders were uploaded.

| Variant | Maximum known input-matrix bus peak, FS | Over-range samples |
|---|---:|---:|
| Legacy, no Sound Check fixture | 0.965194629 | 0 |
| Float, unity programme gain | 0.891250386 | 0 |
| Float, -9 dB fixture | 0.450987635 | 0 |
| Float, +1 dB fixture | 0.891251948 | 0 |

The ~0.89125 result is approximately -1 dBFS; final S16 rounding causes tiny
differences from the detector ceiling. This proves bounded sample levels on
these fixtures, **not better perceived sound**, better source separation, or
complete DSP safety after unknown EQ/processing. Lower peaks are not a quality
score; compare loudness-matched listening and stem selectivity next.

Report/renders: `build/reference-audio/float-candidate-verified/report.json`.
The earlier `float-candidate-stage1` report retains the rail-only failure;
`float-candidate-matrix-guard` is the first corrected run.

Tests:

- Full `scripts/run_tests.sh` passes, including legacy DSP, Sound Check,
  AAC/APAC switching, lifecycle, and the available real APAC capture check
  (3980 authenticated frames, 3977 reference-identical APAC packets).
- New `float_render_test`: exact bypass, input/output overlap, validation,
  identical HSSR analysis state, arbitrary block partitions, known over-FS
  synthesis before attenuation, metadata/FLUSH ramps, +1-dB channel-ratio
  preservation, matrix detector, gain extremes and non-finite handling.
- New C test also passes ASan/UBSan/float-cast-overflow sanitizers.
- 26 amplifier static tests and 11 virtual-cabin/stage tests pass.
- `git diff --check` passes.

## Reproduce

Build with the command in `tools/stereo-upmix-render/README.md`, using a
separate binary path such as `build/stereo_upmix_render_float`. Then:

```sh
python3 tools/stereo-upmix-render/evaluate_float_candidate.py \
  --renderer build/stereo_upmix_render_float \
  --model build/hssr-selection-v11/hssr_model.bin \
  --manifest build/reference-audio/lfe-lp48-heldout-source/manifest.json \
  --output build/reference-audio/float-candidate-next-evaluation
```

The output directory must not already exist. Optionally pass
`--legacy-reference build/stereo_upmix_render_head_reference` after building
that pre-change reference. Firmware and vehicle settings are not needed.

## Next slice / promotion gates

1. Measure vocal/bass leakage, front-anchor behavior and limiter activity on
   teacher stems plus held-out mixes. Loudness-match listening comparisons.
   Optimize policy from those results, not from louder/more active rears alone.
2. Measure ARM/QNX cost and consider faster matrix peak detection only with
   numeric equivalence tests. The candidate has not been cross-built or
   benchmarked on QNX, and is not yet wired into `audio_hook.c`.
3. Add training-time level augmentation and evaluation in the verified matrix
   domain while retaining semantic supervision. Do not change the model until
   the evaluation gates exist. Never train an inverse of unknown cabin EQ.
4. Complete downstream graph coefficient generation/complex-stage modeling
   when supported by evidence. The isolated kernels are not a full emulator.
5. Confirm actual Focus and numeric Surround later; continue development with
   the explicitly annotated target assumptions in the meantime.
