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

## Completed result

The guarded CPU run completed. PC result: `D:\upmix\runs\cue_attribution_bound\result.json`, SHA-256 `9473e617c02cd9bb1b41557cbe1c6457e6deeb774f4568ce370f5cb0414b727c`; `run.txt` and partial rows are beside it. The attribution algebra closed on every song: median normalized squared residual **2.47e-14**, maximum **1.16e-13**, far below the frozen **1e-8** limit. The raw arm exactly reproduced review 40.

| Fit target, inspected DEV12 median | Eligible | Vocal/E | Drums/E | Bass/E | Frozen joint source gate |
| --- | ---: | ---: | ---: | ---: | --- |
| Raw cue | .4806 | .5453 | .0714 | .03314 | Fail vocal, drums |
| Minus attributed vocal | .3210 | .2896 | .0649 | .0365 | Fail eligible, drums, bass |
| Minus attributed drums | .4432 | .4889 | .0476 | .0320 | Fail vocal |
| Minus attributed vocal + drums | .3116 | .2438 | .0457 | .0316 | Fail eligible |
| Attributed eligible only, all protected removed | .3028 | .2386 | .0221 | .00529 | Fail eligible |

**None passes the frozen joint source gate.** Removing vocal contribution repairs the vocal ratio but drops eligible transmission below .432; removing drums alone repairs drum ratio but leaves vocal leakage. Removing both repairs all protected ratios, yet eligible transmission falls to **.3116**; removing all protected gives **.3028**. Thus the raw cue's apparent eligible level partly depends on protected signal in its fit target. The model needs to represent eligible audio more faithfully *while* protecting it; an added veto alone is unlikely to meet the same unscaled gate. This does not prove the class of spatial operators fails, because the true-stem fit in review 39 passed.

A single **posthoc** positive gain of about **1.18×** would move the vocal+drum-subtracted *median* eligible transmission to .432, and **1.19×** would do so for the all-protected arm; protected/eligible ratios are invariant to that scalar. These gains were calculated after inspecting DEV and were **not** part of the frozen gate or a measured stable C-render operating point. Even at 1.19×, the all-protected arm would clear the eligible floor in only **6/12** songs; one remains near zero (**4.28e-6** before gain). The result therefore supports a new anchored eligible-audio or operator-target design with explicit source protection and per-song continuity, not a retrospective gain promotion. No entry, real-C, device, independent or listening verdict was obtained.
