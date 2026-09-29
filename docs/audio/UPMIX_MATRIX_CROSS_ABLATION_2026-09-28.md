# Frozen Matrix Cross-Term Ablation

_2026-09-28. Diagnostic only, fixed before running; the 4k matrix student already failed promotion._

## Question and method

The [spatial oracle](UPMIX_SPATIAL_CUE_UPPER_BOUND_2026-09-28.md) found a substantial advantage for cross-channel L/R terms, but the trained [causal matrix student](UPMIX_CAUSAL_MATRIX_PROBE_2026-09-28.md) failed every fixed source gate. Distinguish two explanations on its frozen step-4000 checkpoint: (a) cross-channel terms are effectively unused; (b) they contribute materially but the learned operator is insufficiently source-selective.

On the same 12 deterministic DEV20s excerpts, infer the full matrix **once from each stereo mixture**. Compare its output and source attribution to an ablation setting only the two cross coefficients to zero while preserving the inferred diagonal coefficients, normalization, trunk state and song. Report cross-term output energy as a fraction of full output energy, complex eligible error full-minus-ablated, and eligible/vocal/drum/bass source transmission through identical reanalysis. Also compare the fixed 43 vocal entries with the full and ablated operator. The first 12 DEV excerpts and all 20 transition songs are already inspected; this is not independent validation or a chance to revise the failed promotion rule.

Interpret cross terms as **materially used** if median cross-output energy / full-output energy is ≥ **.05**. If below .05 and source/entry changes are small, the model did not learn to exploit its new pathway at 4k. If above .05, compare source ratios and entry behavior: a better complex reconstruction with worse vocal/eligible selectivity indicates the trained cross terms serve the audio objective but not the rear routing objective. Neither diagnosis authorizes a C render of this rejected checkpoint. Zeroing the cross terms after training is an ablation, not a deployable new candidate or a matched retraining control.

Checkpoint SHA-256: `849C80EDBC6AF2B1D02D647A9EDC956B6787EE53558F8CBA36F2651A0A0F0604`. The test modifies only Track B host diagnostics; no head-unit or Track A file is involved.

## Results

Pending.
