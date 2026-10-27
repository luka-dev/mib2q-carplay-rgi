# Fine-Cell Role and Direct Matrix Route Probe

_2026-09-28/29. Review 37, gate frozen before training; completed and rejected._

The [review-36 frozen audit](UPMIX_ROLE_CUE_AUDIT_2026-09-28.md) found that review 35's band-level fast head ranks pure eligible/protected cells but tracks eligible fraction on **mixed vocal+eligible cells** poorly (song-median correlation .192). Its immediate decoder path changes output only 2.53% RMS. Test one architecture change that addresses both mechanisms, with a matched control and no Track A/head-unit change.

Retain the review-35 causal band trunk, full 2×2 complex matrix, 8 s eligible-evidence state, 16 s training crops, half truth/pseudo sampling, source-attr weight .0734755788, four-role cross-entropy .279254, seed 1, 4k steps and zero neural lookahead. Replace the band-constant role logits with a small role context projection (36→8 per band) plus a **shared per-bin** 14→4 head. It sees the band context and six current-bin stereo mixture features. Average the resulting eligible probabilities within each band to admit slow-state evidence. The base band decoder sees averaged role probabilities, while an explicit shared 4→8 role-to-matrix map adds a per-bin complex-matrix residual from the **current** role probability minus the neutral .25 vector. The residual weights start near zero; the original diagonal .3 prior remains. The target labels exist only in the training loss, never inference.

Train two otherwise identical arms from scratch, same initialization, data order, parameter count, nominal operations and schedule. The **fine** arm sees each bin's six local features; the **coarse control** repeats that band's mean six features into every bin before the same role head. Both retain the same 8 s state and direct role-to-matrix route. This differs from review 35's older coarse student, so it isolates the value of local detail *within the new architecture* rather than crediting a larger head or direct route. Keep estimated neural MAC/s below 150 M before FFT, softmax, EMA, memory traffic and int8 implementation; product fit remains a device gate.

The isolated [`sep_net_fine_role.py`](../../../tools/stereo-upmix-render/sep_net_fine_role.py) is SHA-256 `b2d9cc188a6abf112a6d4d09385ad2947e6fa939036872f060baecb4198badb6`; [`run_fine_role4k.ps1`](../../../tools/stereo-upmix-render/pc/run_fine_role4k.ps1) is `926d8d7d6a004d0acf2b22b2bd4123db1a87dc7843398b1098af8edd9a34edb7`. Local forward/backward, role-shape and causal-prefix checks passed for both modes. Both have **1,032,172 parameters** and **146.321 M estimated neural MAC/s** before the uncounted operations above. The historical base and review-35 model files remain byte-identical.

## Frozen decision sequence

At step 4000, use the fixed first 12 inspected DEV 20 s excerpts to compare fine versus matched coarse role quality and source attribution. The fine arm must improve song-median mixed vocal+eligible fraction correlation by **≥.10** without reducing pure eligible/protected AUC by >.05, and its direct role-route output contribution must reach **≥5% RMS** to count as a mechanism advance. These architecture metrics cannot override the source and transition gate. Require eligible transmission **≥.432**, song-median vocal/eligible **≤.324**, drums/eligible **≤.0568**, bass/eligible **≤.0339**; no source ratio may worsen by >.01 versus matched coarse. On the same first-20-DEV/43-vocal-entry event detector with **full cached-song causal inference**, require eligible post/pre median **≥.85**, p10 **≥.60**, absolute eligible pre and post **≥.25**, and p10 improvement **≥.04** versus coarse. Report worst songs and whether the original 22 s excerpt protocol agrees. Do not select checkpoints by scalar SI-SDR.

The frozen [`diag_fine_role.py`](../../../tools/stereo-upmix-render/diag_fine_role.py), SHA-256 `125ed865f1763f822614b69306400596e99b8a3b4389534a43e52c262a617a1e`, reuses the unchanged DEV12 source and full-song event methods. Its guarded runner [`run_fine_role_diag.ps1`](../../../tools/stereo-upmix-render/pc/run_fine_role_diag.ps1) is SHA-256 `fd96966d9e4ec0f94bbcd39274dc79e6cc1362e15c4b1a0ec0721c366229b050`. A local synthetic algebra check reproduced the output effect of zeroing the explicit role-to-matrix weights within **1.7e-9** for both arms.
The protocol-comparability helper [`diag_fine_role_legacy_entry.py`](../../../tools/stereo-upmix-render/diag_fine_role_legacy_entry.py), SHA-256 `1f4d9777bb76f728840844833c2dc41eac50d5251228747695bacbed2019d6be`, calls the original 22 s event metric on the same frozen checkpoints after the primary gate. It cannot change the promotion rule.

Only a joint pass authorizes the fixed-gain enriched DEV20 real-C screen at guitar/keys/piano/strings ≥.15/.15/.10/.14, voice/bass/drums ≤.02/.06/.08, no new swing and paired rear RMS within ±.1 dB, then full40 and reused TEST20. The TEST20 was already inspected and has weak family coverage; it cannot prove independent generalization. Streaming chunk carry, int8 ≤150 M MAC/s and ≤85 ms device measurements, Track A's port and the owner's blind CarPlay listening remain separate product gates. A failed 4k source/entry mechanism stops this pair before C or longer training. No threshold, role temperature or gain is chosen after seeing this DEV output.

## Matched training and fixed diagnostics

The guarded one-GPU runner trained both arms sequentially to exactly **4000 steps**. The coarse arm took **1277 s** and the fine arm **1260 s**, with identical **1,032,172 parameters** and **146.321 M estimated neural MAC/s** before FFT, softmax, EMA, memory traffic and int8 kernels. Scalar DEV SI-SDR/vocal-cell leak changed from coarse **−.48 dB/.140** to fine **−.39 dB/.135**; this was not the selection criterion. Checkpoints: `D:\upmix\runs\fine_role4k\coarse\step4000.pt` SHA-256 `e7244c4de1a3cb265babf0807845e7f7247aeed1d0f369f9d3ccd6c4b3d66431` and `D:\upmix\runs\fine_role4k\fine\step4000.pt` SHA-256 `826061fd92ae515fd0c55798db848c98831d7df3db15ca1b4e562de73b381f29`. The SSH session lagged after completion, so the final checkpoint/train-done files, absent GPU lease and absent Python training process were checked directly before any diagnostic started. Training was **not** rerun.

The fixed first-12-DEV, 20 s role-quality result is `D:\upmix\runs\fine_role4k\diag\quality.json`. The same source-attribution screen is `source.json`. The mixture alone determines each matrix; true stems are applied through it for measurement. Ratios are medians of per-song protected/eligible transmission.

| Measure | Coarse control | Fine current-bin cue | Frozen fine requirement |
| --- | ---: | ---: | ---: |
| Pure eligible versus protected AUC | .9409 | .9411 | ≥ control − .05 |
| Mixed vocal+eligible fraction correlation | .1978 | **.1941** | ≥ control + .10 |
| Mixed-cell coverage | .0825 | .0825 | descriptive |
| Direct role-route output RMS fraction | .0312 | **.0696** | ≥.05 |
| Predicted eligible probability on ≥.8 pure-eligible cells | .4260 | .4249 | descriptive |
| Eligible transmission | .4948 | .4920 | ≥.432 |
| Vocal/eligible | .4832 | **.4775** | ≤.324 |
| Drums/eligible | .0678 | **.0668** | ≤.0568 |
| Bass/eligible | .0368 | **.0376** | ≤.0339 |

The explicit route became substantial (6.96% output RMS), but the local-bin features **did not improve mixed-cell ownership** (median correlation −.0037 versus the matched control), and all three protected-source gates failed. AUC on pure cells remained high but says little about overlaps; the fine arm's worst song AUC was about **.564**. Song-paired eligible transmission changed **−.0063** median; paired vocal/eligible **−.0018**, drums/eligible **+.0011**, bass/eligible approximately zero. These are too small to explain a useful source-selectivity advance. The candidate maintained the precommitted ≤+.01 per-family non-worsening comparison but remained far from absolute protection limits.

The first-20-DEV full cached-song diagnostic used the same detector's **43 vocal entries, 40 exits and 54 sustained-vocal controls**. Its complete output and fixed gate are `D:\upmix\runs\fine_role4k\diag\transitions.json` and `summary.json`.

| Entry measure | Coarse | Fine | Frozen fine requirement |
| --- | ---: | ---: | ---: |
| Eligible post/pre median | .9279 | **.9036** | ≥.85 |
| Eligible post/pre p10 | .6055 | **.5814** | ≥.60 and ≥ control + .04 |
| Absolute eligible pre/post | .5423/.4775 | .5436/.4912 | ≥.25 each |
| Vocal leak 0–2 s | .2391 | .2406 | descriptive |

Fine worsened the p10 by **−.0241** instead of improving it by +.04, and the paired 43-event median change was **−.0190**; the worst paired change was **−.1785**. The weakest fine event retained only **.3653** of eligible output. The legacy 22 s excerpt protocol on the same checkpoints found entry median/p10 **.913/.600→.904/.581** with 43 events, agreeing on the fine-arm failure. Its different coarse p10 near the .60 boundary is a protocol effect; the precommitted full-song rule remains primary. No checkpoint was selected by these results.

**Frozen gate: NO.** Pure-cell ranking, explicit role-route strength, eligible level, absolute entry output and entry median passed. Mixed-cell correlation gain, all protected-source ratios, entry p10 and relative p10 improvement failed. No real-C, full40, TEST20, device or car promotion follows; a longer train or role-temperature sweep is not selected. This bounds this specific per-bin six-feature role pathway at 4k, not the extractability of sources from stereo. It shows that increasing the decoder's use of the role cue can coexist with nearly unchanged overlap understanding and worse entry stability. The next discriminating experiment should target **eligible audio reconstruction during vocal overlap** or a stronger causal representation of overlapping sources, with the same protected/entry gates and matched controls, rather than only making the current role route louder.
