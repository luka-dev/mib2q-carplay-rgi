# Causal Confidence Mapping of the Four-Role Student

_2026-09-28. Frozen Track B offline screen, before running either mapped variant. No head-unit or listening claim._

## Question and fixed variants

The focused 4k four-role checkpoint underpredicts eligible ownership on true eligible-dominant cells, yet ranks pure eligible against pure protected cells fairly well. The exploratory hard threshold in the [role-focus probe](UPMIX_CAUSAL_ROLE_FOCUS_2026-09-28.md) nearly met the source-only screen but was selected after inspecting DEV and could cause binary-mask artifacts. This screen tests whether one fixed **smooth, causal** mapping of the same frozen role logits can retain more eligible audio without increasing protected leakage or vocal-entry ducking.

Use checkpoint `D:\upmix\runs\causal_role_focus4k\focused\step4000.pt`, SHA-256 `0434C9E584427B8101148C68F11E31CDD97AB8A2152A741FE61B313EEB90E277`. At each time-frequency cell, define the eligible margin `m = logit(E) - logsumexp(logit(V), logit(D), logit(B))`. Both variants output `sigmoid((m - log(.55/.45))/.10)` as a real mask shared by left and right. `soft_static` applies this immediately. `soft_ema80` first applies a causal, per-frequency exponential average to `m`, with 80 ms time constant and explicit state at chunk boundaries. These are frozen before the mapped DEV outputs. The probability centre .55 came from the earlier inspected DEV hard-threshold sweep; this is **not independent validation**. The .10 logit softness and 80 ms memory were chosen before this screen. Neither mapping has lookahead, but the host Python code and whole-file model diagnostic do not establish streaming-device behavior or an int8 compute budget.

## Frozen decision rule

First run the existing fixed DEV12 source-attribution diagnostic against the shared-mask 4k anchor. Each variant must transmit eligible energy ≥ **.432**; its per-song median vocal/eligible, drums/eligible and bass/eligible transmission ratios must be ≤ **.324**, **.0568** and **.0339** respectively. Then run the fixed DEV20 transition diagnostic: vocal-entry eligible post/pre median ≥ **.85** and p10 ≥ **.60**. These are the review-21 source and entry gates, rounded from 80% of the fixed anchor ratios. A source or entry failure stops that variant before C rendering. If both pass, prefer `soft_ema80` for the C screen when it does not worsen these measures; report both results.

Only a passing variant can enter the frozen enriched DEV20 real C renderer screen. Use the same direct/no-bed/body0 renderer and source cache as reviews 19–21, one fixed gain chosen to match paired-median rear RMS within ±.1 dB. Then require guitar ≥.15, keys ≥.15, piano ≥.10, strings ≥.14, voice ≤.02, bass ≤.06, drums ≤.08 and no new any-stem swing songs against the fixed control. If it passes, full DEV40 and separate TEST20 follow with the same fixed gain. No DEV threshold will be revised after seeing the outputs. Source and transition screens alone cannot prove sound quality, output smoothness, device fit, or the owner's deferred in-car verdict.

## Results

Both CPU diagnostics completed on the frozen focused checkpoint. The mask was exactly chunk-invariant (maximum absolute difference **0** on a local two-batch, 13-frame test) and bounded within [0,1]. This verifies the mapping's state continuation in that test, not whole-model streaming equivalence.

| Fixed DEV12 source diagnostic | Shared-mask anchor | `soft_static` | `soft_ema80` |
| --- | ---: | ---: | ---: |
| Eligible energy transmission | .4624 | **.4670** | .4618 |
| Vocal / eligible, per-song median | .4050 | **.3231** | .4091 |
| Drums / eligible | .0710 | **.0402** | .0674 |
| Bass / eligible | .0424 | **.0172** | **.0173** |
| Source gates passed | — | **4/4** | **2/4** |

The static variant's vocal ratio is only **.0009 below** the rounded .324 gate and only .0009 below the exact 80%-of-anchor value (.3240); this is a marginal result on already inspected DEV excerpts. The causal 80 ms logit average mixes the scores around source changes and fails both the vocal and drum ratios. The fixed 43 vocal entries on 20 DEV songs decide the static variant:

| Vocal-entry eligible post/pre retention | Required | `soft_static` | `soft_ema80` |
| --- | ---: | ---: | ---: |
| Median | ≥.85 | **.826** | **.832** |
| 10th percentile | ≥.60 | **.473** | **.462** |

Both variants miss both entry gates. The static mask retains more eligible energy in aggregate but still drops it on vocal entry, and the smoothed variant leaks more protected material without fixing the entry dip. **Reject both before the real-C screen under the frozen rule.** No gain, host C render, full40/TEST20, longer training, device port or car change was made. The result isolates a failure of these two mappings; it does **not** prove the score or causal architecture fundamentally incapable. It does show that re-calibrating one shared mask and adding symmetric 80 ms inertia do not jointly solve attribution and entry stability.

Next architectural question: whether a source-conditioned persistent eligible representation can keep an instrument during vocal overlap **while** a separate fast vocal protection path blocks the newly arriving voice. Test that separation explicitly at vocal entries and on mixed cells before investing in longer training. A single shared score plus symmetric time smoothing has no observed operating point here. The owner's blind car test remains deferred by request; there is no ready deployable candidate from Track B.

PC artifacts: `D:\upmix\runs\causal_confidence_map\{soft_static,soft_ema80}.json`, matching `.source.txt` and `.transitions.txt`; runner [`run_causal_confidence_diag.ps1`](../../../tools/stereo-upmix-render/pc/run_causal_confidence_diag.ps1), mapping [`role_confidence.py`](../../../tools/stereo-upmix-render/role_confidence.py).
