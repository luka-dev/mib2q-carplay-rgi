# Spatial Operator Source and Vocal-Entry Bound

_2026-09-29. Review 39. Protocol and joint gate frozen before running on DEV._

The [review-38 bounded real IRM](UPMIX_OVERLAP_ORACLE_BOUND_2026-09-29.md) passed vocal, drum and vocal-entry gates with true eligible audio but missed the bass/eligible source limit (**.0589 > .0339**). The earlier [review-26 spatial oracle](UPMIX_SPATIAL_CUE_UPPER_BOUND_2026-09-28.md) showed that a song-specific full L/R matrix improved bass rejection over a diagonal matrix on 30.1–60 s, but used a different spectral energy measure and did not test vocal entries. Before another causal neural architecture, put the privileged spatial operator through the source-attributed **iSTFT/STFT** and entry measures used to screen students. This remains an inspected-DEV architecture diagnostic, not an independent or live candidate.

## Frozen protocol

- Use the first **12 inspected DEV songs**. Fit the existing frequency-wise, 1%-ridge, complex diagonal and full 2×2 matrices from true eligible stem and stereo mixture on **0–29.9 s**, then freeze them. Evaluate source-attributed output on **30.1–60 s**, using the same iSTFT/STFT reanalysis and protected/eligible ratios as review 38. Report the bounded real IRM on exactly this second-half test window as a same-audio reference. Fit matrices only on the earlier window; the IRM uses privileged truth from its test window.
- For the first **20 inspected DEV full cached songs**, take the unchanged `diag_transitions.events` vocal entries. Include an entry only when a disjoint **30 s true-stem fit** can end immediately before the 22 s event segment (10 s warm, 4 s before, 8 s after) and the whole segment is within the song. Freeze each diagonal/full operator during its segment. Apply each operator unchanged to the *mixture-selected* true eligible and vocal spectra; reanalyse and use the existing 0.5-s source-attributed entry metric. Report the same-test-segment IRM as a reference. Prior event metadata predicts 30 eligible entries from 16 songs; require at least 25 valid entries rather than assuming exact coverage.
- Report all song/event rows, absolute eligible transmission, protected/eligible ratios, entry post/pre median and p10, absolute pre/post transmission and 0–2 s vocal leakage. Report the positive gain needed to make the full matrix's eligible transmission equal the IRM on each source window; it is a comparison only and does not alter the raw matrix gate. Source ratios and entry post/pre are invariant to that gain. Do not tune ridge, fit duration, gain or thresholds after viewing the result.

The isolated [`diag_spatial_overlap_bound.py`](RETIRED_TOOLS.md#tool-6a011f8988af) is SHA-256 `14308acf2270c559d804c41b74bd097208d89e6904c12ff70b2284a994090fdd`; guarded CPU runner [`run_spatial_overlap_bound.ps1`](RETIRED_TOOLS.md#tool-885cf3dfea44) checks source hashes, PC queue/GPU lease and active jobs. Python syntax passed locally. The source code reuses the fixed review-26 fitter and review-38 masks/metric helpers. No GPU or package change is needed.

**Frozen joint gate for the raw full matrix:** 12 valid eligible source songs and ≥8 valid protected songs per role; ≥25 valid entries; source medians eligible **≥.432**, vocal/eligible **≤.324**, drums/eligible **≤.0568**, bass/eligible **≤.0339**; median paired full-minus-diagonal bass/eligible **<0**; entry eligible post/pre median **≥.85**, p10 **≥.60**; absolute entry eligible pre/post medians both **≥.25**. The first four limits are the same inspected-DEV source screen as review 38. A pass would show the privileged spatial representation can meet this narrow source/entry screen; it would **not** show a causal learner, C/full40, independent-set, device or ear fit. A fail on bass would weaken the spatial explanation of the review-38 shortfall. A fail on entry despite source success would point to a changing song-specific operator at vocal onset. In either case, inspect individual songs and do not continue the rejected review-37 checkpoint. Track A owns any eventual head-unit port.

## Completed result

The guarded CPU run completed without using the GPU. PC result: `D:\upmix\runs\spatial_overlap_bound\result.json`, SHA-256 `8a674aadfaa0fa83f73e4183819c29a8648aa877384843286b12741c0dbb9141`; `run.txt` and partial source/event rows are beside it. The fixed source screen covered all 12 songs (11 valid vocal ratios); the vocal-entry screen covered **30 entries from 16 songs**.

| Same 30.1–60 s source measure, song median | Diagonal fitted | Full 2×2 fitted | IRM on test truth | Full gate |
| --- | ---: | ---: | ---: | --- |
| Eligible transmission | .3109 | **.4491** | .7983 | ≥.432 pass |
| Vocal/eligible | .5153 | **.2995** | .1321 | ≤.324 pass |
| Drums/eligible | .0552 | **.0246** | .0209 | ≤.0568 pass |
| Bass/eligible | .0383 | **.00944** | .0633 | ≤.0339 pass |

The paired full-minus-diagonal bass/eligible median was **−.0240**, satisfying the additional spatial-effect gate. The positive scalar needed to match full eligible transmission to the IRM had median **1.337×**; the scalar is not a live calibration and did not enter the raw gate.

| 30 source-attributed vocal entries | Diagonal fitted | Full 2×2 fitted | IRM on event truth | Full gate |
| --- | ---: | ---: | ---: | --- |
| Eligible post/pre median | 1.011 | **1.041** | .921 | ≥.85 pass |
| Eligible post/pre p10 | .776 | **.783** | .788 | ≥.60 pass |
| Absolute eligible pre/post medians | .309/.321 | **.407/.406** | .865/.792 | both ≥.25 pass |
| First-2-s vocal leakage median | .121 | **.123** | .103 | diagnostic |

**Predeclared joint gate: pass.** The 2×2 operator substantially reduces the specific bass leak that defeated the IRM while preserving the screen's median eligible level and vocal-entry continuity. This strengthens the spatial-representation hypothesis, not a claim that the trained review-27/35/37 students can infer the needed operator. All fitted operators used song-specific true stems unavailable from CarPlay, and the source and entry subsets are inspected DEV, with different windows from review 38.

Individual failures matter. Only **7/12** full-matrix source windows clear the eligible threshold, **7/11** clear vocal/eligible, **7/12** clear drums/eligible and **9/12** clear bass/eligible. One song's first 30 s has almost no eligible evidence: its held-out eligible transmission is **0.00000623** and the hypothetical IRM-matching gain is **361×**. Another song has drums/eligible **.489** and bass/eligible **.096**. The worst covered entry's post/pre is **.703**, above the p10 limit but with absolute eligible transmission falling **.265→.186**. The median gate therefore does not certify each track or safe gain; the head-unit and ear gates remain open.

The next discriminating test is whether a **stereo-only, causal cue already available near the budget** can estimate a useful slow spatial operator. A frozen 30 s fit to the review-37 fine-role eligible estimate, compared with the privileged fit on the identical source/entry windows, can test this without another training run. Predeclare that comparison and check its checkpoint/runtime before running it. Sep9 is unsuitable for this causal-cue test because it has lookahead and whole-window normalization. If the fine-role cue is too contaminated, a learned operator distillation or direct eligible-audio estimator is required; do not infer that more review-37 training will close the gap. Any prospective student still needs the real-C full40/independent harness, measured int8 MAC and latency on the unit, and blind live CarPlay listening.
