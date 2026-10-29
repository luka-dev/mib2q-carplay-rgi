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

- [Geometry factory and loader](RETIRED_TOOLS.md#tool-7b57389698f6).
- [Fixed TRAIN trainer](RETIRED_TOOLS.md#tool-bb2d30e55239).
- [CPU auditor](RETIRED_TOOLS.md#tool-4b3ded479434).
- [Guarded PC runner](RETIRED_TOOLS.md#tool-f86dd26ff24a).

Main output: `D:\upmix\runs\separator_redistributed_bands_fit_v1`. Smokes use separate original/redistributed output directories. Preserve all completed and failed artifacts. The runner refuses an existing output, pending rapid queue, Python work or rapid daemon, checks source hashes, and acquires one exclusive GPU lease. Only its own token is released. No packages, Track A files, unit deployment/restart or DSD Test audio are involved.

Status at protocol freeze: PC idle, no GPU lease, rapid queue empty, no Python/daemon. DSD Dev cache remains deferred; Test audio is not decoded or scored. Execution/results will be appended below.

## Completed 10k result and decision

Protocol/source commit: `62f5f8b`. Both original/candidate 20-step GPU smokes completed. The original initial objective **.2072141767** reproduced the frozen control within `1e-6`; both constructions have exactly **586,916 parameters / 125,139,000 estimated neural MAC/s**. The sole candidate then completed 10k steps in **443.85 s**.

| Step | Original objective | Candidate objective | Original median SI-SDR, dB | Candidate median SI-SDR, dB |
| --- | ---: | ---: | ---: | ---: |
| 0 | 0.20721418 | 0.20717120 | -6.35599 | -6.34072 |
| 1,000 | 0.06768120 | 0.06726923 | 5.91871 | 5.88055 |
| 4,000 | 0.05920464 | 0.05892782 | 7.07272 | 7.13853 |
| 5,000 | 0.05816909 | 0.05787896 | 7.24318 | 7.32286 |
| 7,500 | 0.05651488 | 0.05628608 | 7.52950 | 7.69147 |
| 10,000 | 0.05558584 | 0.05516277 | 7.79656 | 7.95127 |

Final objective improved **0.761%**, median SI-SDR **+.1547 dB**. The loss and median gates both failed; the per-window gate passed (three nonworse, none worse by over 1 dB). **Joint diagnostic promotion: NO.** Two of the three nonworse changes are very small; this single seed/four-window observation is not evidence of a reliable generalization improvement.

| Frozen TRAIN window | Original SI-SDR, dB | Candidate SI-SDR, dB | Delta, dB | Original waveform NSE | Candidate waveform NSE |
| --- | ---: | ---: | ---: | ---: | ---: |
| `f4b735de-14b1-4091-a9ba-c8b30c0740a7` at 25.545 s | 9.60000 | 9.63513 | +0.03513 | 0.099064 | 0.098228 |
| `1fc37390-1769-452d-9bea-19025be4c467` at 18.409 s | 12.31531 | 11.97744 | -0.33787 | 0.058049 | 0.060932 |
| `ad6c3742-e517-42eb-9dec-e74ed05387cc` at 19.355 s | 3.64257 | 3.64565 | +0.00307 | 0.286827 | 0.289655 |
| `fd6e4b4a-f33a-4f3c-aa6e-7c65fd5dc0bc` at 19.182 s | 5.99312 | 6.26742 | +0.27430 | 0.198336 | 0.188943 |

The difficult third window remains at **3.65 dB**, and the second loses **.338 dB**. This particular frequency reallocation does not materially close the known fitting gap. It does not reject every partition, establish a hard capacity limit, or identify high-frequency compression as the sole cause. Training still improves slowly at the chosen horizon. No geometry search, longer extension, DEV/entry/C screen or device port followed.

### Independent CPU verification

All source/control/crop/split/horizon checks passed, and the auditor confirmed TRAIN membership and equal nominal parameter/MAC counts. Dedicated-loader CPU replay gave objective **.0551621392** versus GPU **.0551627651**, with maximum per-window SI-SDR difference **.00107 dB**, inside the frozen tolerances. Loading candidate weights into the original-shaped network raised the expected shape mismatch; geometry cannot be silently interpreted as the default network. These checks establish the experiment's semantics, not head-unit int8 runtime or full-chain streaming latency.

Frozen-mask source contributions remain saved as descriptive sensitivity measurements in `progress.json` and `complete.json`; they are not literal audible contamination or product protection results. No protection/entry metric is inferred from the four-window reconstruction scores.

### Decision and next work

Keep the original partition as the anchor. Input replacement, raw decoder-state supervision and this redistribution have not supplied a useful architectural/training shortcut under their declared diagnostics. Stop extending these three failed recipes. The prior effectiveness audit still identifies an untested question: the proper causal training schedule, including the planned later SI-SDR phase, has not been evaluated by repeated 4k screens.

The next proposed efficacy comparison remains the predeclared full-horizon **40k causal original baseline versus audited DSD Dev addition**, with identical schedules and 4k/10k/20k/40k snapshots. Before execution, convert/check only DSD Dev and finish its dedicated paired runner; keep Test untouched. This is a separate data/training question, not promotion of the new geometry. No such long run is queued by this result, and all source/entry/real-C/device/owner-listening gates remain open.

### Artifacts and terminal state

PC output: `D:\upmix\runs\separator_redistributed_bands_fit_v1`. Original/candidate smokes are preserved separately as `separator_band_smoke_original_v1` and `separator_band_smoke_redistributed_v1`.

| Artifact | SHA-256 |
| --- | --- |
| `complete.json` | `3fb1865dce326b5e6fffd88e0fa920eb293306c844f99c1d08f80c3b7762be2d` |
| `result_audit.json` | `4fa6422dd045e3a214d90c0b39f459bd8d2ece72524785bf289c1a86dd5cd950` |
| `manifest.json` | `71e8abfa18d542aed44e4c8999e0a94524875a99e0a82c7ffa54b79944bfacac` |
| `progress.json` | `3317d588ce28d3dc6ddcc29751482bee74b42660680df3b77dc2623093e3d96a` |
| Candidate `fit.pt` | `a629675b13af27e97cf961e1128ebd520f00958a35d4b35c1b10b0c252eef387` |

Source hashes: trainer `74e818d85d8f08350dd13b946df63098cc2c18b28323ca691f8ec4cf35880805`; geometry/loader `6743b18527f29ce61c8f61018057d03aa5917e5734e05293b993c4cced3f40da`; CPU auditor `5c83da4e72edcd0b53913d0bb01f4a7fe6d1153f9d835c78a9dab7a24c328b68`. Shared `sep_net.py` remains `11e0c4fe89c82d3be3814e5d2a421c5fd73c40d4e3fa505904c57ae1d7dba905`.

Post-audit read-back: **no GPU lease, zero queued rapid jobs, no Python/daemon**. DSD Dev cache is absent; Test remains undecoded/unscored. No Track A/head-unit/package operation occurred. Earlier launch state above is historical.
