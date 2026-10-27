# HSSR Universal: selection V11 (2026-09-07)

## Outcome

`hssr-selection-v11` is the selected model for the sole HSSR Universal
renderer. It replaces V9 without changing the runtime graph, model dimensions,
DSP path, rear cap or LFE policy. The selection target is deliberately
asymmetric: move usable instrumental mid/high objects rearward while rejecting
vocals, shared vocal/instrument regions and sub-bass.

Seven additional local songs were reserved before evaluation and were never
used for training, validation or epoch selection. Their 14 deterministic
30-second excerpts form the blind generalization set. Private source MP3s are
tracked under `tests/fixtures/private-audio/`; decoded excerpts and offline
teacher stems remain ignored build artifacts.

## Teacher-stem selection

BS-RoFormer supplied evaluation-only vocal and instrumental stems. `Mix rear`
weights each stem's rear-energy fraction by its level in the original mix.

| corpus/model | instrumental rear | vocal rear | instrumental/vocal | instrumental mix rear | vocal mix rear | mix ratio |
|---|---:|---:|---:|---:|---:|---:|
| 43-song / V9 | 2.500% | 0.272% | 9.20x | 1.294% | 0.0424% | 30.52x |
| 43-song / V11 selective loss | 3.814% | 0.222% | 17.21x | 1.850% | 0.0325% | 56.91x |
| **43-song / V11 selected** | **3.770%** | **0.199%** | **18.92x** | **1.828%** | **0.0296%** | **61.80x** |
| blind 7-song / V9 | 0.992% | 0.326% | 3.04x | 0.539% | 0.0310% | 17.37x |
| blind 7-song / V11 selective loss | 2.153% | 0.262% | 8.21x | 1.176% | 0.0256% | 46.02x |
| **blind 7-song / V11 selected** | **2.096%** | **0.205%** | **10.23x** | **1.144%** | **0.0194%** | **59.07x** |

Against V9 on the blind set, V11 raises isolated-instrument rear transfer by
111%, lowers isolated-vocal rear transfer by 37%, and raises the level-weighted
selection ratio from 17.37x to 59.07x. On `Crazy Train`, the two instrumental
excerpts rise from 1.72%/1.66% to approximately 5%/7.5%, while the corresponding
vocal-stem transfers stay around 0.37%/0.01%.

The ordinary balanced loss won the final A/B against the more aggressively
asymmetric loss: it retains virtually the same instrumental activity but has
lower vocal leakage on both the 43-song and blind corpora.

## Stereo activity and safety

These mastered-stereo stress sets have no authored rear reference, so their
rear levels are descriptive. The teacher-stem table above is the selection
accuracy gate.

| corpus/model | mean rear energy | rear/front | L/R corr | balance P90 | front/fold SNR | worst matrix peak |
|---|---:|---:|---:|---:|---:|---:|
| 43-song / V9 | 2.86% | -16.46 dB | 0.469 | 8.04 dB | 9.85/12.74 dB | -0.33 dBFS |
| 43-song / V11 | 3.87% | -14.80 dB | 0.466 | 8.61 dB | 8.13/11.02 dB | -0.33 dBFS |
| blind 7-song / V9 | 1.26% | -20.55 dB | 0.470 | 6.60 dB | 13.31/16.19 dB | -0.29 dBFS |
| blind 7-song / V11 | 2.25% | -18.60 dB | 0.473 | 7.44 dB | 11.47/14.36 dB | -0.26 dBFS |

All 100 V11 renders have zero clipped samples. The stronger object transfer
intentionally reduces front/fold SNR; the hard rear cap and visible SoundAmp
matrix retain headroom.

On the independent 12-preview rock/rap/dubstep set, mean rear energy changes
from 2.14% to 2.25%, rear L/R correlation from 0.463 to 0.457, and the worst
matrix peak remains -0.23 dBFS.

## Authored-surround trade-off

The six public authored-music fold-down pairs are a spatial regression gate,
not a source-separation ground truth. V11 changes rear-envelope correlation
from 0.637 to 0.596 and rear SI-SDR from -8.54 to -9.63 dB. Rear L/R correlation
error improves from 0.219 to 0.210; front and fold-back SNR remain effectively
unchanged at 5.97/8.85 dB. This is an explicit trade: V11 prioritizes blind
instrument/vocal selectivity and audible material-dependent placement over
matching the ambience distribution of six classical authored mixes.

The documentary J525/9VS matrix produces 13.05% to 35.24% rear-circuit energy
on those six music items, 22.76% mean, with zero clipping. This verifies that
the vehicle input matrix passes V11's Ls/Rs; it does not predict driver-seat
SPL without OEM output calibration and measured cabin responses.

## Model identity and training

- Path: `models/hssr-selection-v11/hssr_guard.bin`
- SHA-256: `ab01463e9359f013c4b40f311d630bd20af48d761ad361563cb5246f6bd686ae`
- Payload CRC32: `48a84364`
- Parameters: 451,464
- Bytes: 469,760
- Selected epoch: 5 of 6
- Extra supervision caches: 384 train, 91 validation in total
- V11 separator-supervision subset: 340 train, 90 validation from 43 songs
- Direct supervision: original, vocal-negative, instrumental-positive,
  vocal-heavy and instrumental-heavy variants with source-disjoint splits

The offline checkpoint is MUSDB-derived and retains its existing licensing
warning. That warning is unchanged from the previous production model.

## Runtime and build gates

The V11 weights preserve the HSSR-160 graph and inference path. A 512-call host
run after 128 warmups, using 1,024 frames per call, measured 0.255 ms mean,
0.269 ms P95, 0.283 ms P99 and 0.370 ms max. Mean realtime-budget use is 1.20%.

The GCC 8.5 ARMv7/QNX full hook build passes with no emutls, exceptions, RTTI,
STL or `libstdc++` dependency. The calibrated QEMU run reports 5,501,874 mean
and 5,513,191 maximum estimated retired instructions per 1,024-frame block.
This is a deterministic instruction regression metric, not a Krait timing
claim.

Verified artifact identities from this run:

```text
libcarplay_hook.so 9d78c3bf3c7c9ea3ba8f70eddc87dcf6c4d2d7d5325adda9f8f6cdf6fcfd08d7 421066 bytes
libcarplay_apac.so a6a7bc8c3f17dfe170765b04a461259a407269de47dde6fe0a2b3f6b2695044f 678911 bytes
hssr_model.bin     ab01463e9359f013c4b40f311d630bd20af48d761ad361563cb5246f6bd686ae 469760 bytes
```

## Limits

A stereo master is non-invertible. V11 is a causal semantic selection
controller over the existing fine-bin DSP, not an offline waveform separator;
when vocal and instrument occupy the same time-frequency region it deliberately
refuses ambiguous transfer. Objective checks can reject dead rears, leakage,
clipping and regressions, but stationary in-car A/B remains the final
perceptual gate.
