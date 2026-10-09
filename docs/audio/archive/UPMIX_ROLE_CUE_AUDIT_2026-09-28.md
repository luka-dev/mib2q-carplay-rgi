# Frozen Dual-State Role-Cue Audit

_2026-09-28. Review 36, question and interpretation frozen before the diagnostic._

The [review-35 student](UPMIX_DUAL_STATE_PROBE_2026-09-28.md) learned a nonzero, causal 8 s spatial state and improved vocal-entry median slightly, yet passed nearly the same protected audio as its 2 s reset control. Distinguish two narrower explanations without another GPU train: **the fast role head does not identify eligible versus protected material**, or **it identifies material but the matrix decoder barely uses its current-frame cue**. Neither test alone can prove an architecture limit; the oracle still uses true first-half stems.

Freeze the review-35 persistent step-4000 checkpoint SHA-256 `eb43dc0e2a058957176672fa16bcd2afec54ab6c30ef2fda4c5ee1560787e3ef`. On the same fixed first 12 inspected DEV 20 s excerpts as source attribution, infer the network only from the stereo mixture. Compare its four band-level role probabilities with aligned true-stem power fractions. Report song-median predicted eligible probability on ≥.8 pure eligible and ≥.8 pure vocal/drum/bass audible cells; eligible-versus-any-protected AUC, mixed eligible/vocal-cell correlation and coverage. Use the role-calibration audible floor (`total stem power > 1e-5 × within-excerpt mean`), preserving missing categories.

The frozen implementation is [`diag_dual_state_role_cue.py`](../../../tools/stereo-upmix-render/diag_dual_state_role_cue.py), SHA-256 `08884b27b8d4ef980cf4869c59eefcd750aba1a77fd62d293c00bc105abae656`, run by [`run_dual_state_role_cue.ps1`](../../../tools/stereo-upmix-render/pc/run_dual_state_role_cue.ps1), SHA-256 `8e6034bd5e6969aa689e382809c2dca865103ef1357c9b004b32be352aadfcff`. The runner guards the GPU lease, queue, checkpoint hash and source hashes; the diagnostic needs no GPU or PC package changes.

On a local synthetic mixture, the algebraic neutral operator reproduced an explicit decoder-weight/bias replacement to within **1.4e-9** maximum complex-output difference, confirming that the intervention removes exactly the direct role input while leaving the slow-state path intact.

For decoder use, hold **every inferred feature and slow state fixed** after mixture inference. Algebraically replace only the decoder's direct four current-frame role-probability inputs by the neutral vector (.25,.25,.25,.25), subtracting their exact linear contribution from the frozen 2×2 matrix. Compare output RMS and true-source transmissions for original versus neutral operator on the same mixture/stems. This is a diagnostic intervention, not a separately trained candidate, full causal role ablation, oracle upper bound or renderer test. It intentionally keeps the slow state, which also depends on the role head, unchanged.

Interpretation frozen before results: median pure-cell AUC < **.75** or mixed eligible/vocal correlation < **.30** means this coarse fast cue is weak for ownership at overlap. If both are at least those values but neutralizing the direct role input changes output RMS by < **5%** on the song median, decoder use is weak. If the cue and direct use both exceed these screens while source leakage remains high, inspect the matrix fusion/objective rather than simply changing cue resolution. No outcome promotes the rejected checkpoint to C/full40/device/car. Do not tune thresholds or a role temperature on inspected DEV from this audit.

## Frozen-checkpoint result

The guarded CPU run completed all 12 fixed DEV excerpts. Output: `D:\upmix\runs\dual_state4k\role_cue\result.json` and `run.txt`. Every source result uses the operator inferred from the **mixture**. The role target is privileged only for measurement, never fed into inference.

| Quantity, song median | Result | Predeclared screen |
| --- | ---: | ---: |
| Pure eligible vs any pure protected AUC | **.9407** (12 songs; range .5758–.9868) | ≥.75 |
| Predicted eligible fraction on ≥.8 pure eligible cells | **.4162** | target fraction ≥.8 |
| Predicted eligible fraction on ≥.8 pure vocal/drum/bass cells | **.2095/.1508/.1546** | low is better |
| Mixed eligible+vocal cell correlation | **.1915** (12; range −.0570–.3271) | ≥.30 |
| Mixed eligible+vocal cell coverage | **.0825** of cells | descriptive |
| Output RMS change from neutralizing direct role input | **.0253** (range .0134–.0434) | ≥.05 for strong direct use |

The head ranks pure eligible/protected cells well on most songs, but its absolute eligible confidence remains modest and its estimate of **mixed-cell eligible fraction** poorly follows the true fraction. One song's pure-cell AUC is only **.576**, so even the ranking result is not uniform. The predeclared hierarchical interpretation is **weak fast cue at overlap** because mixed correlation misses .30. Independently, direct decoder use is small: removing only the immediate role input changes output RMS by **2.53%**, below the 5% screen on every tested song. It does not erase the role-dependent slow state, so this is a lower bound on total role-head influence, not a full role-head ablation.

On the same frozen checkpoint, neutralizing the direct role input changed song-median eligible transmission **.4673→.4629**, vocal **.1578→.1560**, and protected/eligible ratios vocal **.4455→.4452**, drums **.0684→.0693**, bass **.0372→.0377**. It does not move the candidate close to the hard source gates. The current and neutral operators are both failed diagnostic points, not product candidates.

**Architecture implication:** adding slow memory to a coarse band-level role estimate cannot recover mixed-cell source ownership when that estimate is weak and the decoder uses its immediate cue lightly. The next bounded design should test **fine-grained, current-frame protected/eligible evidence inside each band** and a decoder path that is forced to use it, with a matched coarse-cue control, identical losses/data/age and the same source/entry gates. A simple role-temperature change, longer EMA or extra epochs on review 35 is not selected by these results. This remains an inspected-DEV diagnostic; no device or listening claim follows.
