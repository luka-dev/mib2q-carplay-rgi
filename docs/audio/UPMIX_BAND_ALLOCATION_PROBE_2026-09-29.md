# Upmix band-allocation probe — 2026-09-29

## Question and scope

Can a different frequency partition improve the separator's ability to fit the same four known TRAIN windows, without increasing nominal neural compute? This is one Track B architecture intervention. It does not train on DEV, change Track A, establish source protection through the C renderer, or certify head-unit cost or listening quality.

The earlier input-invariance and direct state-coordinate teaching probes failed their frozen acoustic gates. A privileged fixed-decoder state relaxation can reconstruct these windows far better than the trained encoder. That relaxation is not causal inference or a realizable recurrence. The reported per-band decoder null-space fractions do not prove that low-band states are useless for global context or recurrence.

History review found larger hidden widths (sep5), cross-band recurrence (sep7) and skip/lookahead decoder variants (sep9/sep9c), but no matching fixed-budget 48-band redistribution test in the plan, night report and training history. Those older interventions are not this partition change.

## Frozen intervention

| Property | Original control | Candidate |
| --- | --- | --- |
| Bin widths | `4×11, 8×11, 16×13, 32×8, 85×4, 89` | `8×11, 16×11, 24×13, 32×8, 38×4, 41` |
| Band/bin count | 48 / 1025 | 48 / 1025 |
| Largest band | 89 bins | 41 bins |
| Features / hidden widths | Absolute complex six-feature input; 48 / 32 / 32 / 32 | Same |
| Normalization / mask | `stream` / `complex2` | Same |
| Skip / lookahead | None | None |
| Expected parameters / neural MAC/s | 586,916 / 125,139,000 | Same, checked by construction |

Higher-frequency groups receive narrower encoders/decoders; low/mid groups become wider. This may lose low-frequency detail or source protection. It is not a uniform resolution improvement. Parameter and estimated MAC totals stay equal because the band count and summed widths stay equal. Matrix shapes, per-band fan-in initialization scales and frequency neighborhoods change; these are inherent consequences of the partition intervention, not identical encoder/decoder tensors. Use the same seed recipe. Actual device runtime, int8 cost and whole-chain latency remain unmeasured.

## Matched fitting protocol and gates

Reuse the exact four 8-s TRAIN crops, split checksum and selected int16 bytes from `D:\upmix\runs\separator_fit_probe_v1`. Same resident batch, seed 1, constant AdamW `lr=3e-4`, decay `1e-4`, clipping 5 and original magnitude/complex plus multiresolution audio objective. No augmentation, pseudo labels, new data, early stopping, coefficient search or geometry grid. Candidate runs exactly 10,000 steps from fresh initialization. The frozen original 10k result is the control; no unchanged full control retraining is needed.

Before the full run, execute fresh 20-step original/candidate smokes. Require original initial loss parity within `1e-6` of the historical control, plus equal parameter/nominal MAC totals. Construction restores the shared module's band factory in `finally`; the shared `sep_net.py` stays unchanged. Checkpoints explicitly store geometry name and widths and require the dedicated loader. The generic original-shaped loader must reject the candidate weights.

Joint finite-fitting success at 10k:

1. Actual objective ≤ **90%** of the original control's `.0555858351`.
2. Median waveform SI-SDR ≥ original **7.796560 + 1 dB**.
3. At least **3/4** windows nonworse in SI-SDR; no window worse by over **1 dB**.

Run an independent CPU replay of the checkpoint, crop hashes, topology/budget and frozen gates. CPU/GPU tolerances are `1e-4` objective and `.01 dB` per-window SI-SDR. A passing finite fit justifies a separate real-data comparison; it is not product promotion. A failure ends this recipe without a longer run or post-result gate adjustment.

## Reproduction and execution safety

- [Geometry factory and loader](../../tools/stereo-upmix-render/separator_band_geometry.py).
- [Fixed TRAIN trainer](../../tools/stereo-upmix-render/separator_band_probe.py).
- [CPU auditor](../../tools/stereo-upmix-render/audit_separator_band_result.py).
- [Guarded PC runner](../../tools/stereo-upmix-render/pc/run_separator_band_probe.ps1).

Main output: `D:\upmix\runs\separator_redistributed_bands_fit_v1`. Smokes use separate original/redistributed output directories. Preserve all completed and failed artifacts. The runner refuses an existing output, pending rapid queue, Python work or rapid daemon, checks source hashes, and acquires one exclusive GPU lease. Only its own token is released. No packages, Track A files, unit deployment/restart or DSD Test audio are involved.

Status at protocol freeze: PC idle, no GPU lease, rapid queue empty, no Python/daemon. DSD Dev cache remains deferred; Test audio is not decoded or scored. Execution/results will be appended below.
