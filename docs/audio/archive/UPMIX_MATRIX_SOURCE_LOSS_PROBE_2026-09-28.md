# Full-Matrix Source-Loss Probe

_2026-09-28. Track B controlled training experiment; completed and rejected at the frozen source/entry gate._

## Hypothesis and fixed comparison

The [spatial oracle](UPMIX_SPATIAL_CUE_UPPER_BOUND_2026-09-28.md) passed its upper-bound gate, and the trained [matrix cross-term ablation](UPMIX_MATRIX_CROSS_ABLATION_2026-09-28.md) shows the student does use cross-channel information: 30.8% median cross-output energy and −1.541 dB eligible reconstruction error with cross terms. Yet its song-median vocal/eligible ratio is .538 against the .324 source gate and vocal-entry retention p10 .466 against .60. The ordinary eligible-audio loss rewards a better estimate but does not explicitly balance retention of the eligible stem against leakage of each protected stem. This test adds a balanced source-attribution term to the **full mixture-inferred 2×2 matrix**, not just its diagonal. The hypothesis is that existing spatial capacity can be steered toward the actual rear-routing constraint while preserving eligible energy on vocal overlap.

The control is the saved causal `matrix2` step-4000 checkpoint, SHA-256 `849C80EDBC6AF2B1D02D647A9EDC956B6787EE53558F8CBA36F2651A0A0F0604`. The new arm trains from scratch with **identical** architecture, seed 1, 176 true/774 pseudo training songs, half/half 8 s batch-12 crops, 4000 steps, 3e-4 LR and 2000-step warm-up, causal streaming level normalization and zero lookahead. The only change is a source loss: normalized complex error when the inferred matrix is applied to true eligible audio plus the average normalized transmitted energy of true vocal, drum and bass audio. The operator is always inferred from the **mixture** and held fixed for all four sources; this is supervised training only, not stem input at inference. A similar loss on the earlier one-real-mask architecture failed, so this mechanism is uncertain. Candidate neural MAC estimate remains **126.5 M/s** before the 2×2 operations, FFT/iFFT and renderer; device fit is unmeasured.

Before choosing the source-loss coefficient, [`diag_matrix_source_grad.py`](RETIRED_TOOLS.md#tool-a1e06c1c438d) evaluates four deterministic real/pseudo **training** batches at random initialization and chooses `0.20 × median(audio parameter-gradient norm / source parameter-gradient norm)`. The 20% target was frozen before any DEV output. The measured base/source gradient ratios were **.269856/.367457/1.20688/.367299**; the outlying third batch does not control the median. The fixed coefficient is **.0734755788**, giving the targeted 20% median parameter-gradient scale. No DEV/TEST song or source-selectivity result was used to set it. The source code is frozen at SHA-256 `2aa3bd15874e7399d0a1136754df04d685387d3591b1901bf66a88c1f48b4737`. PC calibration output: `D:\upmix\runs\matrix_source_loss4k\grad.txt`.

## Frozen decision rule

At step 4000, run the unchanged fixed DEV12 true-source attribution and DEV20 43-vocal-entry diagnostic against the original matrix control and the shared-mask anchor. Candidate eligible transmission must be ≥ **.432**, song-median vocal/eligible ≤ **.324**, drums/eligible ≤ **.0568**, bass/eligible ≤ **.0339**, and entry eligible post/pre median ≥ **.85**, p10 ≥ **.60**. Require no new degradation of candidate versus matched matrix control on eligible recall, vocal/eligible or entry p10 before attributing a benefit to the added loss. The thresholds are unchanged from the earlier probes.

Only if these pass, run the frozen enriched DEV20 direct/no-bed/body0 real-C screen at one global injection gain chosen from paired rear-RMS delta, verified within ±.1 dB; require guitar ≥.15, keys ≥.15, piano ≥.10, strings ≥.14, voice ≤.02, bass ≤.06, drums ≤.08 and no new any-stem swing song. Then consider longer training and full DEV40 plus separate TEST20 with the unchanged gain. Actual causal streaming/device int8 ≤150 M MAC/s, ≤85 ms extra latency, Track A-owned port, genuinely independent stems and the owner's deferred blind CarPlay verdict remain separate. If the 4k source/entry screen fails, do not run C or extend this arm.

## Results

The one-GPU training run completed 4000 steps in 905 s. The candidate checkpoint is `D:\upmix\runs\matrix_source_loss4k\focused\step4000.pt`, SHA-256 `35B042EBBD44B03697F01444E03861C4D4A185C13F46B23376DD121B1B67FA4A`. Scalar DEV SI-SDR/vocal-leak changed from **−.68 dB/.139** for the saved matrix control to **−.54 dB/.165** for the candidate. These scalar measures were not the promotion rule.

The frozen CPU source diagnostic on the same 12 already-inspected DEV songs gave the following song medians. Ratios are medians of per-song protected/eligible transmission, not ratios of the displayed transmission medians.

| Checkpoint | Eligible | Vocal | Drums | Bass | Vocal/eligible | Drums/eligible | Bass/eligible |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Shared-mask anchor | .4624 | .1563 | .0270 | .0154 | .4050 | .0710 | .0424 |
| Matrix control | .4043 | .1548 | .0231 | .0110 | .5381 | .0605 | .0406 |
| Full-matrix source-loss candidate | .4620 | .1811 | .0319 | .0176 | .5061 | .0662 | .0415 |
| Frozen candidate gate | ≥.432 | — | — | — | ≤.324 | ≤.0568 | ≤.0339 |

The candidate restored eligible transmission by **+.0577** against its matched matrix control, but also raised absolute vocal, drum and bass transmission. Vocal/eligible improved **.5381→.5061** versus control yet remained worse than the shared-mask anchor's .4050. Drum and bass ratios worsened against control. Only the eligible-transmission gate passed; all three protection gates failed.

The unchanged 20-song transition diagnostic detected 43 vocal entries. Eligible post/pre retention improved from control **median/p10 .776/.466** to **.841/.535**, still below the frozen **.85/.60** targets. Median vocal transmission in the first 0–2 s after entry rose **.185→.206**; the median peak over that interval rose **.243→.291**. This is more instrument continuity accompanied by more vocal leakage, rather than the missing selective persistence.

Diagnostic outputs are `D:\upmix\runs\matrix_source_loss4k\focused_vs_shared.{json,txt}`, `focused_vs_matrix.{json,txt}` and `transitions.txt`; the guarded runner is [`run_matrix_source_loss_diag.ps1`](RETIRED_TOOLS.md#tool-0dcfbd2c980f). The PC GPU lease was free and the rapid queue empty after training. No C renderer, full40, longer training, device port or listening test was run for this failed checkpoint, as specified before training.

This result rejects this **loss recipe at 4k**. It does not show that spatial matrix routing is intrinsically inadequate: the privileged spatial oracle improved both eligible transmission and rejection, and the trained cross terms materially affect output. The measured gap is in learned source ownership at vocal overlap. A next experiment must make source persistence and fast vocal protection distinguishable on mixed cells and vocal entries; increasing training duration or changing a scalar output threshold has no measured basis for promotion here.
