# Anchored Vocal-Entry Matrix Probe

_2026-09-28. Review 33, one-change causal training test; completed and rejected at the frozen source/entry gate._

## Fixed experiment

The [review-32 vocal-invariance loss](UPMIX_VOCAL_INVARIANCE_PROBE_2026-09-28.md) achieved .0867× counterfactual sensitivity by collapsing eligible transmission to .0713. Its output-difference objective has a nearly silent solution. This test anchors a **paired vocal-entry prediction directly to true eligible audio**, keeping the ordinary mixture's target and the matrix source-attribution loss active. A silent output now incurs eligible reconstruction error on both predictions.

The control is the saved review-29 matrix source-loss model at step 4000, SHA-256 `35B042EBBD44B03697F01444E03861C4D4A185C13F46B23376DD121B1B67FA4A`. The candidate trains from scratch with identical `matrix2` architecture, causal streaming normalization, zero lookahead, seed 1, 176 true/774 pseudo half/half training set, 8 s batch-12 crops, 4000 steps, LR 3e-4/warm-up 2000, and source-attribution weight .0734755788. The **only change** is one additional reconstruction term on a training-only paired mixture: remove the aligned vocal for the first 4 s of each crop, fade it in by a half cosine over the next 250 ms, and leave eligible/drum/bass samples unchanged. Run the same network on that pair and compare its eligible output to the same true eligible target in the interval **0.5 s before through 3 s after** the inserted onset, using the same complex/magnitude and multiresolution audio losses as the ordinary mixture. This targets an actual transition while keeping its preceding mixture history. At inference the model receives only ordinary stereo; true stems construct training examples and targets only.

[`diag_vocal_entry_target_grad.py`](../../tools/stereo-upmix-render/diag_vocal_entry_target_grad.py) evaluates four deterministic real/pseudo training batches at random initialization. Freeze the new coefficient to `0.30 × median((ordinary audio + .0734755788 × source) parameter-gradient norm / unweighted entry-target parameter-gradient norm)` before any DEV score. The 30% target matches review 32's added-loss scale but the objective has a true audio target; this is one change relative to the review-29 control. No DEV/TEST data choose the coefficient.

The measured base/entry gradient ratios were **1.18398/1.07453/1.01904/1.07507**. The fixed coefficient is **.322439834**. PC calibration output: `D:\upmix\runs\vocal_entry_target4k\grad.txt`. Source hashes: `sep_net.py` **`31D249884042AD640514D5A2925AA1AC2BF0E2C380CA9359B6BEEA8A896450B7`**, `diag_vocal_entry_target_grad.py` **`04BE623AD872FD1BE146F7DD3934DF26F29F1E51FD59D00F0ABB5D4C9D703FA9`**. No DEV song or scalar metric set this weight.

At step 4000, run the unchanged fixed DEV12 reanalyzed source-attribution and DEV20 43-entry diagnostics. The candidate must have eligible transmission ≥.432; song-median vocal/eligible ≤.324, drums/eligible ≤.0568, bass/eligible ≤.0339; entry post/pre eligible retention median ≥.85 and p10 ≥.60. Because review 32 passed the ratio by going nearly silent, also require **median absolute entry eligible transmission pre ≥.45 and post ≥.40** (review-29 control .576/.457). Only if all pass, run enriched DEV20 through the frozen direct/no-bed/body0 real C renderer with one global paired rear-RMS gain verified within ±.1 dB; require guitar ≥.15, keys ≥.15, piano ≥.10, strings ≥.14, voice ≤.02, bass ≤.06, drums ≤.08 and no new any-stem swing song. Then consider full DEV40, separate adequately covered real-stem validation, actual device int8/latency and Track A port, and finally the owner's deferred blind CarPlay verdict. A 4k failure stops this arm before C or longer training.

## Results

The one-GPU run completed 4000 steps in **1278 s** without a training error. Candidate checkpoint: `D:\upmix\runs\vocal_entry_target4k\focused\step4000.pt`, SHA-256 `0A364F10EE346FA537DF74321F3383AEEF647D80F88AD11C4F8D9FFE6D88F0BE`. Scalar DEV SI-SDR/vocal-leak was **−.52 dB/.167**, nearly the saved source-loss control's **−.54 dB/.165**. The anchored objective avoided review 32's nearly silent solution, but the scalar metric does not decide source routing.

The frozen DEV12 reanalyzed source diagnostic gave song medians:

| Checkpoint | Eligible transmission | Vocal | Drums | Bass | Vocal/eligible | Drums/eligible | Bass/eligible |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Saved source-loss matrix control | .4620 | .1811 | .0319 | .0176 | .5061 | .0662 | .0415 |
| Anchored vocal-entry candidate | **.4569** | .1825 | .0291 | .0164 | **.5422** | **.0636** | **.0413** |
| Frozen limits | ≥.432 | — | — | — | ≤.324 | ≤.0568 | ≤.0339 |

The candidate preserves useful eligible energy, passing the recall floor, but vocal/eligible worsens against control **.5061→.5422** and all three protected ratios remain beyond their predeclared limits. Its absolute drum and bass transmission decreases slightly without enough source selectivity to pass their ratios.

The unchanged DEV20 diagnostic found the same 43 vocal entries. Candidate eligible post/pre retention was **median/p10 .833/.526**, versus control **.841/.535** and limits **.85/.60**. Absolute median eligible transmission pre/post stayed **.583/.454**, passing the new anti-silence floors **.45/.40** and close to control **.576/.457**. The first 0–2 s vocal leak median changed **.206→.202**, while 4–8 s steady vocal leak increased **.197→.217** and settling-time p90 **1.4→3.5 s**. Thus the added onset-focused target neither fixed the retention tail nor protected a sustained vocal.

PC outputs: `D:\upmix\runs\vocal_entry_target4k\focused_vs_matrix.{json,txt}`, `focused_vs_shared.{json,txt}`, `transitions.txt` and `focused\train.log`. [`run_vocal_entry_target_diag.ps1`](../../tools/stereo-upmix-render/pc/run_vocal_entry_target_diag.ps1) guards the source/checkpoint hashes and empty GPU queue, then runs the fixed CPU screens. The candidate failed all protected-ratio gates and both entry-ratio gates, so the precommitted rule stops it before real C, full40, longer training, device port or car listening.

Together with reviews 29–32, this isolates the tested *loss and training-data interventions* at 4k: source attribution opens pure-vocal cells, an ideal hard veto protects them but loses eligible continuity, output invariance collapses toward silence, and an anchored synthetic onset preserves eligible level but not source selection or transition stability. These experiments do **not** prove a universal causal-compute limit. Another scalar penalty or longer continuation of these rejected checkpoints has no measured support. The next discriminating architecture test should explicitly separate a persistent eligible-audio state from a fast protected-source state, then pass the same source and real-entry gates before C promotion.
