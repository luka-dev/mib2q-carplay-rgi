# Frozen Matrix Cross-Term Ablation

_2026-09-28. Diagnostic only, fixed before running; the 4k matrix student already failed promotion._

## Question and method

The [spatial oracle](UPMIX_SPATIAL_CUE_UPPER_BOUND_2026-09-28.md) found a substantial advantage for cross-channel L/R terms, but the trained [causal matrix student](UPMIX_CAUSAL_MATRIX_PROBE_2026-09-28.md) failed every fixed source gate. Distinguish two explanations on its frozen step-4000 checkpoint: (a) cross-channel terms are effectively unused; (b) they contribute materially but the learned operator is insufficiently source-selective.

On the same 12 deterministic DEV20s excerpts, infer the full matrix **once from each stereo mixture**. Compare its output and source attribution to an ablation setting only the two cross coefficients to zero while preserving the inferred diagonal coefficients, normalization, trunk state and song. Report cross-term output energy as a fraction of full output energy, complex eligible error full-minus-ablated, and eligible/vocal/drum/bass source transmission through identical reanalysis. Also compare the fixed 43 vocal entries with the full and ablated operator. The first 12 DEV excerpts and all 20 transition songs are already inspected; this is not independent validation or a chance to revise the failed promotion rule.

Interpret cross terms as **materially used** if median cross-output energy / full-output energy is ≥ **.05**. If below .05 and source/entry changes are small, the model did not learn to exploit its new pathway at 4k. If above .05, compare source ratios and entry behavior: a better complex reconstruction with worse vocal/eligible selectivity indicates the trained cross terms serve the audio objective but not the rear routing objective. Neither diagnosis authorizes a C render of this rejected checkpoint. Zeroing the cross terms after training is an ablation, not a deployable new candidate or a matched retraining control.

Checkpoint SHA-256: `849C80EDBC6AF2B1D02D647A9EDC956B6787EE53558F8CBA36F2651A0A0F0604`. The test modifies only Track B host diagnostics; no head-unit or Track A file is involved.

## Results

The CPU run completed on the frozen 4k matrix checkpoint. On fixed DEV12 excerpts, the median cross-term output energy was **.308 of full output energy**, above the predeclared .05 materiality line. Full-minus-zero-cross normalized complex eligible error was **−1.541 dB** median and lower in **11/12** songs. The cross-channel pathway is learned and materially improves the audio estimate; this is not an unused-head failure.

| Frozen DEV12 source diagnostic | Full matrix | Same checkpoint, cross terms zero |
| --- | ---: | ---: |
| Eligible transmission | **.4043** | .3914 |
| Vocal / eligible, per-song median | **.5381** | .6372 |
| Drums / eligible | **.0605** | .2706 |
| Bass / eligible | **.0406** | .2189 |
| Existing source gates passed | **0/4** | **0/4** |

The full matrix improves all four listed source measures relative to zeroing its cross terms, especially drum/bass rejection. Nevertheless it remains above the original .324 vocal/eligible limit and below .432 eligible transmission; post hoc ablation cannot rescue either checkpoint. On the fixed 43 vocal entries, full versus zero-cross eligible post/pre retention median/p10 was **.776/.466 versus .782/.516**. The learned cross terms slightly lower the median and lower p10 by **.050**, while reducing the vocal leak during the 0–2 s entry interval (median **.185 versus .253**). Thus cross-channel cancellation is active and useful for source protection/audio reconstruction, but at some vocal entries it also removes instrument energy.

This points to a **training objective / temporal ownership** bottleneck for this output architecture, not to absent stereo information or a disabled cross branch. The earlier source-attribution loss on a one-mask student failed, so it cannot be assumed to work here. One controlled next test is to apply a balanced true-source loss to the *full matrix operator* inferred from each training mixture, with its weight calibrated on parameter gradients, then use the same source/entry/C promotion rule. It must preserve eligible energy during overlap as well as reduce protected transmission. The present failed student remains rejected; no C render, longer continuation, device or listening claim was added.

PC artifacts: `D:\upmix\runs\matrix_cross_ablation\energy.json`, `{full,zero_cross}.selectivity.json`, `{full,zero_cross}.transitions.txt` and matching text logs. The runner [`run_matrix_cross_ablation.ps1`](RETIRED_TOOLS.md#tool-31cdb2f8eba3) checks source/checkpoint hashes and forbids a competing GPU tenant. No head-unit or Track A file changed.
