# HSSR Universal: RoFormer teacher selection (2026-09-06)

## Outcome

`hssr-roformer-v9` is the selected model for the sole HSSR Universal renderer.  It keeps the
runtime HSSR-160 graph unchanged (451,464 parameters, 469,760-byte model) and
adds isolated-vocal counterexamples only during offline training.

The private MP3 sources are retained under `tests/fixtures/private-audio/` in
the private repository. Twenty deterministic 30-second excerpts were prepared
from ten local tracks; decoded WAVs remain ignored build artifacts.

## Candidate comparison

All candidates use the same Universal renderer, rear gain 0.25 and
LFE assist.  `Mix rear contribution` weights each isolated teacher stem's rear
fraction by the stem level relative to its mixture.

| model | private rear % | independent rock/rap/dubstep rear % | authored rear-envelope corr | authored rear SI-SDR dB | RoFormer instrumental mix rear % | RoFormer vocal mix rear % | decision |
|---|---:|---:|---:|---:|---:|---:|---|
| V4 baseline | 2.98 | 1.94 | 0.640 | -8.28 | n/a | n/a | superseded |
| V8, mixture teacher | 3.63 | 2.41 | 0.674 | -8.04 | 2.151 | 0.753 | reject: vocal leakage |
| **V9, full vocal negatives** | **3.53** | **2.14** | **0.637** | **-8.54** | **1.915** | **0.020** | **selected** |
| V10, 25% vocal negatives | 3.24 | 2.25 | 0.667 | -8.20 | 1.906 | 0.326 | reject: weaker selectivity |

V9's teacher-stem instrumental-to-vocal selectivity is about 96:1 by this
energy-weighted measure.  V8 is about 2.9:1 and V10 about 5.8:1.  The Demucs
bass contribution for V9 is 0.0025% of mixture power, so low-end protection
remains intact.

The independent authored-reference score does not improve over every metric:
V9 trades 0.003 rear-envelope correlation and 0.26 dB rear SI-SDR against V4
for substantially stronger measured vocal safety.  That is the intentional
selection priority; a vehicle listening test remains required.

## Model identity

- Path: `build/hssr-roformer-v9/hssr_model.bin`
- SHA-256: `a72b9b6ad21cf01b22dc8386f10e59c4320acf0b9b6c9f5a8e5d549c4813b2c2`
- Payload CRC32: `80fa70b3`
- Parameters: 451,464
- Bytes: 469,760
- Selected training epoch: 3 of 6

## Runtime regression

The weights do not change the network dimensions or the inference path.  A
512-call host run after 128 warmups, at 1,024 frames per call, measured:

| mean | P95 | P99 | max | realtime-budget mean/max |
|---:|---:|---:|---:|---:|
| 0.255 ms | 0.287 ms | 0.305 ms | 0.352 ms | 1.19% / 1.65% |

This is a same-host regression check, not a Krait timing claim.  The full hook
also cross-builds successfully for QNX ARMv7 with the GCC 8.5 toolchain and no
`libstdc++` dependency.

## Universal-only prune verification

After V9 was selected, every superseded upmix implementation, backend enum,
profile parser, build selector and internal HSSR `active_profile` branch was
removed. `off` remains only as the exact safety bypass. The production
`stereo_upmix.c` shrank by about 1,300 lines and the caller-owned state now uses
43,384 bytes for runtime DSP (47,520 bytes total on the host ABI; 47,512 on the
QNX ARM ABI).

Before pruning, all seven fixtures in
`build/reference-audio/public-authored-corpus` were rendered with V9, rear
`0.25` and LFE assist. The final Universal-only binary rendered the same inputs,
and `cmp` reported byte-for-byte equality for all seven output WAV files. This
proves the cleanup did not alter the selected signal path.

A post-prune 256-call host run after 16 warmups measured 0.240 ms mean, 0.259 ms
P95, 0.290 ms P99 and 0.296 ms max per 1,024-frame block (1.13% mean and 1.39%
max of the realtime block budget). A calibrated GCC 8.5 ARM/QNX QEMU run over
64 blocks measured 5,509,343 estimated retired instructions mean and 5,515,671
max. QEMU calibration agreement was 0.999498; this remains an instruction
regression metric, not a Krait cycle measurement.

The final GCC 8.5 artifacts from this verification are:

```text
libcarplay_hook.so f9251f9cc50192acfafbf18ac7db0212228e39419d574b1a1eb24d56f37f3fc0
libcarplay_apac.so a6a7bc8c3f17dfe170765b04a461259a407269de47dde6fe0a2b3f6b2695044f
hssr_model.bin     a72b9b6ad21cf01b22dc8386f10e59c4320acf0b9b6c9f5a8e5d549c4813b2c2
```

## Documentary Virtual Cabin regression

The seven final Universal renders were also passed through the exact recovered
J525 profile-0/bank-1 input matrix and the documentary equal-power fan-out to
all sixteen known 9VS amplifier circuits. Across the six music items, rear
circuit energy excluding the subwoofer ranges from 10.65% to 50.88%, with a
25.42% arithmetic mean and -5.10 dB mean rear/front ratio. The worst pre-clip
peak is -7.07 dBFS and there are no clipped samples. The EBU technical routing
fixture is excluded from that aggregate.

This validates the known electrical topology, not perceived driver-seat SPL:
OEM per-output gain/EQ/delay and measured cabin responses remain unavailable.
Full per-track results and limitations are in
[`../../../../docs/Audio/Virtual Cabin and Amp RE.md`](<../../../../docs/Audio/Virtual Cabin and Amp RE.md>).

## Expanded private stereo stress regression

The final renderer was exercised on 43 private mastered stereo tracks using two
deterministic 30-second excerpts per track: 86 renders and 43 minutes of input.
Mean rear energy is 2.86%, the median is 2.28%, P10/P90 are 0.78%/5.27%, and
the full range is 0.20% to 20.88%. Ten excerpts are below 1%, ten are above 5%,
and two are above 10%. Rear L/R waveform correlation spans 0.305 to 0.664.

All renders have zero clipped samples. The maximum digital sample is 0.9400 and
the worst visible SoundAmp-matrix peak is -0.33 dBFS. The lowest front/fold-back
SNR pair is 2.85/5.73 dB on the 20.88% outlier, so aggressive isolated sections
still trade substantially more front energy for rear placement.

Stable low-activity cases include `Bellingham (Slowed)` (0.20%/0.42%),
`I Ain't Done` (0.32%/0.44%), `Save Some Bitches For The Rest Of Us`
(0.62%/0.61%) and `Stand and Deliver` (0.75%/0.78%). The most section-dependent
cases are `XOXO` (2.40%/20.88%), `Maximum Black` (12.05%/1.62%) and `Stop!`
(7.47%/2.40%). These are listening and future hard-training candidates, not
proof of either clean separation or leakage because the masters have no stems.

The complete ignored report is
`build/reference-audio/hssr-universal-v9-43-track-stress/report.md`.

## Reports

- `build/reference-audio/hssr-roformer-v9-private-local-eval/report.md`
- `build/reference-audio/hssr-roformer-v9-apple-genre-eval/report.md`
- `build/reference-audio/hssr-roformer-v9-public-authored-eval/report.md`
- `build/reference-audio/hssr-roformer-v9-teacher-stem-routing/report.md`
- `build/reference-audio/hssr-roformer-v9-roformer-stem-routing/report.md`

## Interpretation

This is semantic TF routing, not recovery of original Atmos objects.  The
universal fallback deliberately keeps a small nonzero rear bed on difficult or
nearly mono masters, while the learned path selects stable instrumental and
upper-percussion components.  Very low rear percentages on dense masters are
not evidence of a clean isolated stem; subjective separation must be checked in
the car with the fixed SoundAmp routing.
