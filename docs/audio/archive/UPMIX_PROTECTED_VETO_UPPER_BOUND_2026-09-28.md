# Protected-Cell Veto Upper Bound

_2026-09-28. Review 31, privileged frozen-checkpoint diagnostic; completed and failed the joint gate._

## Fixed question

The [cell-attribution audit](UPMIX_MATRIX_CELL_ATTRIBUTION_2026-09-28.md) found that the full-matrix source-loss student's extra vocal output came predominantly from cells already ≥.8 vocal-owned by true-stem power. Test whether suppressing those easy protected cells on its **frozen** mixture-inferred eligible output is enough to meet the source-selectivity and vocal-entry gates, before implementing and training a causal vocal detector. This is an upper bound that uses unavailable true stems to choose the gate at evaluation time; it cannot play live.

The fixed gate is **0.25 amplitude** where any one protected source (vocal, drums or bass) owns ≥.8 of the sum of the four aligned true-source powers in a time-frequency cell, and **1.0 elsewhere**. It is applied after the same full 2×2 matrix operator to each source and mixture estimate, with no per-song tuning, temporal smoothing or lookahead. The .8 definition is the one in review 30; .25 is a deliberately strong 12 dB output-amplitude attenuation to test feasibility, not an optimized operating point. Use the unchanged first 12 inspected DEV 20 s excerpts for reanalyzed source transmission and the unchanged first 20 DEV songs' 43 detected entries for eligible post/pre retention. Report source ratios as medians of per-song ratios and entry retention as median/p10 across events.

The predeclared interpretation is: if eligible transmission is ≥.432, vocal/eligible ≤.324, drums/eligible ≤.0568, bass/eligible ≤.0339, **and** entry retention median ≥.85 and p10 ≥.60, a learned mixture-only fast veto is worth a short train. If source ratios pass but entry fails, the veto is insufficient by itself and the next training mechanism must also preserve eligible audio across vocal onset. If source ratios fail, this particular pure-protected gate is insufficient even with privileged labels. No C, device or listening claim follows from any outcome.

## Results

The guarded CPU run used the fixed source-loss checkpoint and produced `D:\upmix\runs\protected_veto_upper_bound\dev.{json,txt}`. The diagnostic source SHA-256 is `FB933061601ADDA6787A51E0816EDF34995145158D77F82BA0E1A08D0D4F155F`; [`run_protected_veto_upper_bound.ps1`](../../../tools/stereo-upmix-render/pc/run_protected_veto_upper_bound.ps1) verifies that, the unchanged transition/model code and checkpoint. The baseline exactly reproduced review 29's DEV12 source values and 43-entry retention.

| Same frozen matrix output | Eligible transmission | Vocal/eligible | Drums/eligible | Bass/eligible | 43-entry retention median/p10 | Entry vocal leak, first 2 s |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| No veto | .4620 | .5061 | .0662 | .0415 | .841/.535 | .206 |
| Privileged pure-protected veto | **.4255** | **.1642** | **.0397** | **.0276** | **.828/.493** | **.073** |
| Frozen limits | ≥.432 | ≤.324 | ≤.0568 | ≤.0339 | ≥.85/.60 | diagnostic |

The perfect-label veto passes all three protected-source ratio limits and sharply reduces entry vocal leak. It also loses enough eligible output to fall **.0065** below the recall floor and worsens both entry-retention statistics. Thus it fails the predeclared joint gate. This does not disprove every fast detector or gate shape: .25/≥.8 was one fixed operating point and no real mixture-only detector was trained. It does show that a fast protected-cell veto **alone**, even with unavailable true-stem labels, is not a reason to port or train this exact mechanism as the next product candidate. The remaining problem is to preserve eligible audio when vocal energy appears while suppressing voice itself.

A next bounded training test should enforce vocal-invariant eligible audio on paired counterfactual mixtures that share the same eligible/drum/bass stems but differ in vocal level. The input at inference remains ordinary stereo; true stems only construct training pairs. Compare to the saved matrix source-loss control at the same architecture, seed, data and 4k age, with a training-batch gradient-calibrated consistency coefficient and the unchanged source/entry/C promotion sequence. This is a proposed experiment, not a claimed fix.
