# V15 routing refinement: A regression and guarded B

2026-09-11, local-only. User authorized evaluation and refinement. No installation,
no deletion of calculated sources/teachers/models, no semantic label approval.

## Candidate A: do not promote

All four original epochs completed. Aggregate pseudo-validation improved by 5.13%
(0.57726372 to 0.54764421), while vocal-negative loss worsened every epoch and
finished 33.53% higher (0.00397417 to 0.00530668). The absolute negative loss is
small; this percentage must not be interpreted as 33.53% audible vocal leakage.

Exported-C validation on 1,024 real V15 validation controls passed: candidate
logit MAE 0.00006187, p99 0.001561 versus the differentiable emulator. Both GRUs,
semantic and global/LFE exported weights and corresponding C controls are exactly
unchanged. The emulator itself is approximate, not bit-identical C arithmetic.

C mixture-conditioned attribution on **all 100 validation clips from 9 songs**
at nominal input level found more teacher-vocal rear transfer in every clip:

| Paired metric | Mean, with equal song weights | Clip median |
|---|---:|---:|
| Vocal rear-transfer change | +0.07132 dB | +0.04409 dB |
| Instrumental rear-transfer change | +0.06260 dB | +0.03543 dB |
| Instrument-minus-vocal selectivity change | -0.00872 dB | -0.01682 dB |

95th percentile vocal increase was +0.19893 dB. This is a small but consistent
proxy regression, not a proven perceptual catastrophe. It does not support
installing A as a better separator. Reports are in
`build/hssr-v15-c-eval-a-validation/{report,comparison}.json` and
`build/hssr-v15-routing-a/export-c-check.json`.

## Fix the experiment's objective and selection rule

The original weighted aggregate allowed instrument gains to compensate for
vocal protection regressions. Vocal-negative optimizer loss was also scaled
down to 0.25. A low negative BCE can be relatively insensitive to rises in
already small probabilities. These are objective weaknesses; they do not prove
a unique cause for every observed C-output change.

`train_v15_streaming.py --policy guarded-v2` starts again from the V11 baseline,
not A, retaining the same source partitions, admission windows and frozen layers.

- Restore vocal-negative optimizer weight to 1.
- Add a baseline-relative routing-logit penalty weighted by squared teacher
  vocal energy share. Its small 0.05-logit margin asks protected bands to sit
  slightly below baseline. A 0.25-weight pan anchor discourages movement of
  vocal-dependent source vectors. Total anchor weight is 2. These are experiment
  hyperparameters, not calibrated psychoacoustic guarantees.
- Check both 0 and -12 dB validation inputs.
- Select an epoch only if total loss improves **and every case family at each
  input level is no worse than the baseline**. Otherwise retain epoch zero.
- Bind guarded-run resume to trainer/runtime source hashes as well as manifests,
  audit decisions and original binary. Never edit an active run to change policy.

Two-epoch B output: `build/hssr-v15-routing-b`. The one-case smoke run passed
and correctly retained the baseline when the selected validation family worsened.
Smoke coverage alone cannot test the complete acceptance criterion.

B completed both epochs and selected neither. Final ordinary-level mixture loss
rose from 1.150553 to 1.158554 while vocal-negative loss fell from 0.00397417 to
0.00376165. At -12 dB the corresponding changes were 1.218433 to 1.227434 and
0.00033880 to 0.00031356. Its exported `candidate.bin` is the retained baseline.
`export_v15_diagnostic.py` separately exports final rejected weights for C
measurement, without overriding this selection decision.

The first 12 contextual diagnostic clips (2 source songs, nominal level) show
small reductions in both teacher-vocal and instrumental transfer: source-balanced
means -0.01207 and -0.00952 dB; selectivity +0.00255 dB. This does not establish
a perceptually meaningful improvement. Do not call a quieter rear a better
separator. Full-song B diagnostic follow-up is under `build/hssr-v15-routing-b-checks`.

## Guarded C: exact C features and risk-mass ablation

The separate PCM-to-feature check found normalized Python/C feature MAE
0.00009097, p99 0.00171393 and maximum 0.29787 on a real 1,024-control stream.
This is not a uniform time shift: C begins counting controls only after its first
complete 512-sample analysis window. The existing Python subsampling matches that
timing. Feature arithmetic differences are not bit-exact despite low mean error.

`--policy guarded-v3 --feature-oracle build/hssr-v15-routing-a/validate_c` uses
actual C feature emissions through pipes for every training/validation input.
Teacher targets remain FLOAT with matching control count/timeline. No disk feature
cache is created; recurrent emulation remains approximate and independently checked.

V3 also changes anchor reduction from a conditional risk-weighted average to
the mean of risk-weighted penalties over admitted bands/frames. Thus a very small
vocal fraction does not receive the same total penalty as a dominant vocal.
A regression test verifies that vocal share 0.1 receives 0.01 of full-share anchor
weight. This is an ablation, not a claimed unique explanation of B's tradeoff.
The same strict two-level, per-family selection rule remains. Two-epoch C output:
`build/hssr-v15-routing-c`. Its baseline is freshly evaluated using C features;
raw loss changes across V2/V3 must not be attributed solely to learned weights.

`finish_v15_candidate.py` waits for that run, verifies export, and measures full
validation songs at 0/-12 dB. If training retained epoch zero, it instead measures
rejected diagnostic weights on only the first two validation songs and does not
expose heldout. For selected weights, heldout full-song regression evaluation is
allowed only after the prospective numerical validation gate: mean vocal delta
<=0 dB, mean selectivity delta >=0 dB, mean instrumental delta >=-0.1 dB and vocal
delta p95 <=+0.1 dB, at both levels. These are experimental regression thresholds,
not acoustic approval. No automatic installation or promotion occurs. Output:
`build/hssr-v15-routing-c-checks/{status,result}.json` and stage logs/reports.

## FLOAT-aware C evaluation and limits

`capture_cabin_transfers.c --stdio` sends transfer matrices and actual output
through binary pipes; the existing exclusive-create file interface remains.
`evaluate_v15_c.py` verifies native teacher provenance, applies common headroom,
rounds only the renderer input to S16, and attributes the rounding residual
separately. Captured transfers must reconstruct actual C output within 2e-5 and
vocal + instrumental + rounding responses must add within 2e-6.

No feature/transfer/audio cache is created. Detailed per-case reports persist.
`--whole-songs` joins the verified non-overlapping FLOAT cores on their exact
timeline, keeps C recurrence continuous for the whole song, and retains 10-second
diagnostic-window metrics. Teacher estimates may be discontinuous at core joins;
the reconstructed source timeline is not. Memory is bounded to one song, not one
clip in this mode. This machine has 36 GiB RAM; disk guard remains 22/27 GiB.

This instruments cabin-policy DSP **before** final rear shelf, Sound Check,
linked protection and physical speaker filters. It measures teacher-estimated
vocal/instrument contributions under mixture controls, not isolated stem reruns,
musical-role truth or measured driver acoustics. Cross-energy is reported; stem
energies are not additive percentages.

Whole-song A comparison completed in `build/hssr-v15-c-eval-a-songs`, on all nine
validation sources at 0/-12 dB. At nominal level, mean vocal/instrument changes
were +0.04901/+0.05384 dB; at -12 dB, +0.02996/+0.03737 dB. Vocal transfer rose in
all nine songs at both levels. Mean selectivity deltas are slightly positive,
but median deltas are negative at both levels; aggregation can change the sign
of a very small effect. This does not overturn the decision against promoting A.
B/C must undergo their own checks before heldout
regression evaluation and level-matched listening. No heldout result has been
used to fit the B objective or select its checkpoint.

Tests: the complete stereo-upmix Python test suite passes (50 tests after these
additions). Includes exact pipe/file output equality, explicit
rounding additivity, negative-logit anchor gradients and rejection of a hidden
family-level regression, whole-song timeline joins, C feature counts and the
post-training numerical gate. No claim of newly improved acoustic quality yet.
