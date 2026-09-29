# Protected Contribution to the Spatial Cue

_2026-09-29. Review 41. Privileged one-factor/factorial ablation and source gate frozen before DEV measurement._

The [review-40 causal student cue](UPMIX_STUDENT_SPATIAL_CUE_2026-09-29.md) let a slow full 2×2 operator pass eligible and bass medians, yet vocal/eligible **.5453** and drums/eligible **.0714** failed **.324/.0568**. The privileged true-stem target on the same windows passed. Determine whether protected audio leaking into the *fit target* is the specific obstacle, before designing a new source estimator.

Use the frozen review-37 fine-role checkpoint and the same first **12 inspected DEV songs**, **0–29.9 s** fit, **30.1–60 s** test, 1%-ridge full 2×2 fit, and exact source-attributed iSTFT/STFT metric. Infer the fine-role matrix **once from the stereo mixture** in each fit window; apply that unchanged matrix to the aligned true vocal, drums, bass and eligible spectra. Algebraically verify that the sum of the four attributed outputs reproduces the model's mixture output, with normalized squared residual **≤1e-8** on every song. Fit separate spatial operators to five preselected targets:

1. Raw model eligible output (must reproduce review 40).
2. Raw minus its attributed **vocal** output.
3. Raw minus its attributed **drum** output.
4. Raw minus both attributed **vocal and drum** outputs.
5. The attributed **eligible** output, equivalent to subtracting all protected contributions if closure passes.

All subtractions use unavailable true stems **only on the operator-fit window**. The unchanged second half is used only for evaluation. No result is a live stereo-only candidate. For every arm, require the same source gate: 12 valid eligible songs, ≥8 valid songs per protected role, median eligible **≥.432**, vocal/eligible **≤.324**, drums/eligible **≤.0568**, bass/eligible **≤.0339**. Report every arm and every song even if an early arm fails; no choice of subtraction coefficient or posthoc gain. The raw arm should reproduce review 40 within rounding. If vocal subtraction alone restores vocal but not drums, an explicit drum estimate is also needed; the converse applies for drum subtraction. If subtracting both passes, the missing live mechanism is source-specific protected-audio estimation. If even the all-protected subtraction fails, the model's eligible component or the static fit is inadequate. This is an architecture-selection diagnostic, not a C/full40/device/ear gate.

The isolated [`diag_cue_attribution_bound.py`](../../tools/stereo-upmix-render/diag_cue_attribution_bound.py) is SHA-256 `b0f4b9120f6009cd367736baf4aba670c64a674f15f1ea2fc5ec8331ca391b0f`; guarded CPU runner [`run_cue_attribution_bound.ps1`](../../tools/stereo-upmix-render/pc/run_cue_attribution_bound.ps1) checks queue/GPU lease, frozen source hashes and checkpoint. Local Python syntax passed. No training, package or head-unit change is authorized by this result alone.
