# Matrix Cell-Attribution Audit

_2026-09-28. Review 30, frozen-checkpoint diagnostic; completed._

## Question and fixed design

The [full-matrix source-loss candidate](UPMIX_MATRIX_SOURCE_LOSS_PROBE_2026-09-28.md) raised DEV12 eligible transmission from .4043 to .4620, but also raised vocal transmission from .1548 to .1811 and failed every protected/eligible ratio gate. Its 43-entry eligible retention improved .776/.466 to .841/.535 median/p10, with more immediate vocal leakage. Before another train, locate where the added eligible output and vocal leakage occur. If most added vocal energy lies in mixed eligible/protected cells, the next mechanism needs overlap-specific ownership; if it lies mostly in pure protected cells, basic fast protection is the first fault. Neither result alone proves a model architecture is impossible.

[`diag_matrix_cell_attribution.py`](../../../tools/stereo-upmix-render/diag_matrix_cell_attribution.py) compares the frozen matrix control and source-loss candidate on the same first 12 inspected DEV 20 s excerpts as the source diagnostic. Both operators are inferred from the full mixture, applied unchanged to each aligned true stem, and measured in raw STFT energy. Input true-stem power fixes each audible cell's class: ≥.8 eligible fraction, ≥.8 one protected-source fraction, or mixed; audibility uses the already-used `total > 1e-5 × mean(total)` rule. The classes are identical for both models. This raw-cell audit deliberately omits iSTFT/STFT reanalysis so the output stays attributable to the original cells; its transmission values must not be substituted for the renderer-facing gate. Per-song deltas divide the candidate-minus-control output energy in each class by that song's total input energy of the same source. Positive-gain share describes which class supplies a source's positive output increment; it is not a listener score.

Use CPU inference only and guard the two checkpoint hashes and diagnostic source hash on the PC. Report median input-energy coverage, transmission and normalized output deltas per class for eligible and all protected sources, with song coverage. The predeclared interpretation is: **≥8/12** songs with >50% of positive *vocal* output gain in mixed cells supports an overlap-specific mechanism; **≥8/12** with >50% in pure-protected cells supports fast protected onset/recognition as the first target; otherwise the result is dispersed or uncertain. Inspect whether eligible gain arises in the same class before choosing the mechanism. This is diagnostic routing only: the existing DEV12 source and DEV20 entry gates stay fixed for the next trained candidate, followed by matched-level real C only if both pass.

After the first pass reported a pure-protected majority, split that fixed class into pure vocal, drum and bass subgroups without changing the original partition or its ≥8/12 decision. This follow-up identifies which protected source accounts for the increase; it is descriptive and adds no promotion threshold.

## Results

The guarded CPU run completed on the PC with no GPU lease or queued job. Output: `D:\upmix\runs\matrix_cell_attribution\dev12.{json,txt}`. The final diagnostic source SHA-256 is `66CC0148D95F2B29FDE8FEB94BF87BCC545C4C72926F17B4F32856CAA0D43E92`; the runner checks it and both checkpoint hashes. The first partition-only pass and the follow-up with the pure-protected subgroup split agreed on the original partition. All figures below are medians of song-level values, not a pooled-cell estimate.

| Source and fixed input cell class | Fraction of input source energy | Control transmission | Source-loss transmission | Candidate minus control output / total input source energy |
| --- | ---: | ---: | ---: | ---: |
| Eligible in pure-eligible cells | .6291 | .4328 | .4892 | +.0373 |
| Eligible in mixed cells | .3075 | .3667 | .4160 | +.0156 |
| Vocal in pure-protected cells | .7790 | .1363 | .1622 | +.0205 |
| Vocal in mixed cells | .2075 | .2144 | .2612 | +.0108 |
| Vocal in pure-eligible cells | .0154 | .3632 | .4368 | +.0008 |

For vocal output increases, the mixed class supplied >50% of positive gain in only **1/11** covered songs, whereas pure-protected cells supplied >50% in **8/12** songs. The one uncovered song had no positive vocal output gain. The predeclared **pure-protected** interpretation fired, not the mixed-cell interpretation. The follow-up split showed pure-vocal cells account for median **.0204** of the .0205 normalized vocal output increase in pure-protected cells; in the ten songs with positive pure-protected vocal increase, the pure-vocal subgroup supplied median **99.45%** of that increase. Pure drum and bass subgroup medians were +.0001 and approximately zero. Eligible output gains arose chiefly in pure-eligible cells (**11/12** positive-gain-majority songs), with some increase in mixed cells.

This narrows the next experiment: the current loss largely opens the matrix in cells already dominated by voice, so a fast vocal veto directly on the matrix's eligible output has a clearer target than a generic mixed-cell persistence loss. It still needs a separate persistent eligible path to avoid ducking instrument energy when a voice enters. A causal current-frame vocal detector with a cheap per-band output head is a testable candidate; measure its source ratios and 43-entry retention before any C renderer promotion. The detector must run on the mixture at inference. These labels from true stems are only for training/evaluation. This diagnostic does not establish that the cells are perceptually pure, nor does it make the head-unit budget or car result true.
