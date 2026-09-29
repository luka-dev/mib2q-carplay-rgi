# Full-Matrix Source-Loss Probe

_2026-09-28. Track B controlled training experiment; gradient weight and DEV results pending._

## Hypothesis and fixed comparison

The [spatial oracle](UPMIX_SPATIAL_CUE_UPPER_BOUND_2026-09-28.md) passed its upper-bound gate, and the trained [matrix cross-term ablation](UPMIX_MATRIX_CROSS_ABLATION_2026-09-28.md) shows the student does use cross-channel information: 30.8% median cross-output energy and −1.541 dB eligible reconstruction error with cross terms. Yet its song-median vocal/eligible ratio is .538 against the .324 source gate and vocal-entry retention p10 .466 against .60. The ordinary eligible-audio loss rewards a better estimate but does not explicitly balance retention of the eligible stem against leakage of each protected stem. This test adds a balanced source-attribution term to the **full mixture-inferred 2×2 matrix**, not just its diagonal. The hypothesis is that existing spatial capacity can be steered toward the actual rear-routing constraint while preserving eligible energy on vocal overlap.

The control is the saved causal `matrix2` step-4000 checkpoint, SHA-256 `849C80EDBC6AF2B1D02D647A9EDC956B6787EE53558F8CBA36F2651A0A0F0604`. The new arm trains from scratch with **identical** architecture, seed 1, 176 true/774 pseudo training songs, half/half 8 s batch-12 crops, 4000 steps, 3e-4 LR and 2000-step warm-up, causal streaming level normalization and zero lookahead. The only change is a source loss: normalized complex error when the inferred matrix is applied to true eligible audio plus the average normalized transmitted energy of true vocal, drum and bass audio. The operator is always inferred from the **mixture** and held fixed for all four sources; this is supervised training only, not stem input at inference. A similar loss on the earlier one-real-mask architecture failed, so this mechanism is uncertain. Candidate neural MAC estimate remains **126.5 M/s** before the 2×2 operations, FFT/iFFT and renderer; device fit is unmeasured.

Before choosing the source-loss coefficient, [`diag_matrix_source_grad.py`](../../tools/stereo-upmix-render/diag_matrix_source_grad.py) will evaluate four deterministic real/pseudo **training** batches at random initialization and choose `0.20 × median(audio parameter-gradient norm / source parameter-gradient norm)`. The 20% target is frozen here before any DEV output. No DEV/TEST song or source-selectivity result is used to set the coefficient. The exact coefficient, source hash, training runner and checkpoint will be recorded below after calibration.

## Frozen decision rule

At step 4000, run the unchanged fixed DEV12 true-source attribution and DEV20 43-vocal-entry diagnostic against the original matrix control and the shared-mask anchor. Candidate eligible transmission must be ≥ **.432**, song-median vocal/eligible ≤ **.324**, drums/eligible ≤ **.0568**, bass/eligible ≤ **.0339**, and entry eligible post/pre median ≥ **.85**, p10 ≥ **.60**. Require no new degradation of candidate versus matched matrix control on eligible recall, vocal/eligible or entry p10 before attributing a benefit to the added loss. The thresholds are unchanged from the earlier probes.

Only if these pass, run the frozen enriched DEV20 direct/no-bed/body0 real-C screen at one global injection gain chosen from paired rear-RMS delta, verified within ±.1 dB; require guitar ≥.15, keys ≥.15, piano ≥.10, strings ≥.14, voice ≤.02, bass ≤.06, drums ≤.08 and no new any-stem swing song. Then consider longer training and full DEV40 plus separate TEST20 with the unchanged gain. Actual causal streaming/device int8 ≤150 M MAC/s, ≤85 ms extra latency, Track A-owned port, genuinely independent stems and the owner's deferred blind CarPlay verdict remain separate. If the 4k source/entry screen fails, do not run C or extend this arm.

## Results

Pending.
