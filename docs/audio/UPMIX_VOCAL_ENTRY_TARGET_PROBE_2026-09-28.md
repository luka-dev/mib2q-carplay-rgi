# Anchored Vocal-Entry Matrix Probe

_2026-09-28. Review 33, one-change causal training test; gradient weight frozen, DEV results pending._

## Fixed experiment

The [review-32 vocal-invariance loss](UPMIX_VOCAL_INVARIANCE_PROBE_2026-09-28.md) achieved .0867× counterfactual sensitivity by collapsing eligible transmission to .0713. Its output-difference objective has a nearly silent solution. This test anchors a **paired vocal-entry prediction directly to true eligible audio**, keeping the ordinary mixture's target and the matrix source-attribution loss active. A silent output now incurs eligible reconstruction error on both predictions.

The control is the saved review-29 matrix source-loss model at step 4000, SHA-256 `35B042EBBD44B03697F01444E03861C4D4A185C13F46B23376DD121B1B67FA4A`. The candidate trains from scratch with identical `matrix2` architecture, causal streaming normalization, zero lookahead, seed 1, 176 true/774 pseudo half/half training set, 8 s batch-12 crops, 4000 steps, LR 3e-4/warm-up 2000, and source-attribution weight .0734755788. The **only change** is one additional reconstruction term on a training-only paired mixture: remove the aligned vocal for the first 4 s of each crop, fade it in by a half cosine over the next 250 ms, and leave eligible/drum/bass samples unchanged. Run the same network on that pair and compare its eligible output to the same true eligible target in the interval **0.5 s before through 3 s after** the inserted onset, using the same complex/magnitude and multiresolution audio losses as the ordinary mixture. This targets an actual transition while keeping its preceding mixture history. At inference the model receives only ordinary stereo; true stems construct training examples and targets only.

[`diag_vocal_entry_target_grad.py`](../../tools/stereo-upmix-render/diag_vocal_entry_target_grad.py) evaluates four deterministic real/pseudo training batches at random initialization. Freeze the new coefficient to `0.30 × median((ordinary audio + .0734755788 × source) parameter-gradient norm / unweighted entry-target parameter-gradient norm)` before any DEV score. The 30% target matches review 32's added-loss scale but the objective has a true audio target; this is one change relative to the review-29 control. No DEV/TEST data choose the coefficient.

The measured base/entry gradient ratios were **1.18398/1.07453/1.01904/1.07507**. The fixed coefficient is **.322439834**. PC calibration output: `D:\upmix\runs\vocal_entry_target4k\grad.txt`. Source hashes: `sep_net.py` **`31D249884042AD640514D5A2925AA1AC2BF0E2C380CA9359B6BEEA8A896450B7`**, `diag_vocal_entry_target_grad.py` **`04BE623AD872FD1BE146F7DD3934DF26F29F1E51FD59D00F0ABB5D4C9D703FA9`**. No DEV song or scalar metric set this weight.

At step 4000, run the unchanged fixed DEV12 reanalyzed source-attribution and DEV20 43-entry diagnostics. The candidate must have eligible transmission ≥.432; song-median vocal/eligible ≤.324, drums/eligible ≤.0568, bass/eligible ≤.0339; entry post/pre eligible retention median ≥.85 and p10 ≥.60. Because review 32 passed the ratio by going nearly silent, also require **median absolute entry eligible transmission pre ≥.45 and post ≥.40** (review-29 control .576/.457). Only if all pass, run enriched DEV20 through the frozen direct/no-bed/body0 real C renderer with one global paired rear-RMS gain verified within ±.1 dB; require guitar ≥.15, keys ≥.15, piano ≥.10, strings ≥.14, voice ≤.02, bass ≤.06, drums ≤.08 and no new any-stem swing song. Then consider full DEV40, separate adequately covered real-stem validation, actual device int8/latency and Track A port, and finally the owner's deferred blind CarPlay verdict. A 4k failure stops this arm before C or longer training.

## Results

Pending.
