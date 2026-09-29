# Upmix Invariant-Input Probe 2026-09-29

_Track B. Protocol frozen before execution. Four fixed TRAIN windows only; one representation change, no product claim._

## Why this experiment

The [training-path diagnosis](UPMIX_TRAINING_PATH_DIAGNOSIS_2026-09-29.md) showed unused capacity in the trained decoder: actual encoder median SI-SDR 7.80 dB, privileged optimized states 20.59 dB on the same four TRAIN windows. The encoder has gradients and localized low-frequency saturation, not an established global gradient failure. The fixed-window fit also depends strongly on global polarity. This test asks whether an input representation that removes absolute phase helps the unchanged causal encoder learn eligible reconstruction.

This is not a new discovery that stereo phase can be useful. Earlier [fine-role](UPMIX_FINE_ROLE_PROBE_2026-09-28.md) and spatial-matrix variants supplied local stereo features to different heads/routes and failed their short-run gates. `sep9c` also tested an embedding skip. This experiment changes the six inputs to the **original band encoder**, retains its decoder and recurrence, and first measures reconstruction on frozen TRAIN audio. It adds no skip, role head or source loss.

## One intentional change

| Item | Frozen baseline | Candidate |
| --- | --- | --- |
| Six current-bin features | Linked running-level normalized Re L/R, Im L/R, standardized log magnitudes L/R | `log1p` of linked normalized magnitudes L/R, the same standardized log magnitudes L/R, cos/sin interchannel phase |
| Geometry | 2048/512 STFT, 48 bands | Same |
| Encoder/trunk/decoder weights and seed | Original BandSepNet, seed 1 | Same topology, shapes and initial weights |
| Neural arithmetic/parameters | 586,916 parameters, about 125 M estimated neural MAC/s | Same; feature arithmetic differs and device cost is unmeasured |
| Normalization and lookahead | `norm=stream`, no neural lookahead | Same |
| Audio, targets, loss | Four exact manifest TRAIN windows, original audio-loss bundle | Same |
| Optimizer/horizon | AdamW `3e−4`, decay `1e−4`, clip 5, 10k constant-lr steps | Same, fixed 10k even if an intermediate fitting gate passes |

The candidate compresses normalized magnitude with `log1p`; it therefore tests the **whole specified feature replacement**, not an isolated phase-invariance effect. The training mean/std used for log magnitude is computed from the same frozen TRAIN mixture batch. Running scale uses past/current spectral frames. Centered STFT latency and a real stateful streaming implementation remain separate requirements.

The historical baseline `D:\upmix\runs\separator_fit_probe_v1\fit.pt` ran all 10k steps without early termination. Reuse its frozen terminal result rather than spend another identical 10k run. A fresh 20-step baseline smoke must reproduce its initial objective within `1e−6`; both new smokes and the main candidate use the same four raw crop hashes and separator source hash. This is a single-seed finite-fit diagnostic, not a training-variance estimate.

## Frozen success criteria

The 10k candidate must jointly have:

1. Objective ≤ **.05002725** (90% of baseline .055585835).
2. Median waveform SI-SDR ≥ **8.79656 dB** (baseline +1 dB).
3. SI-SDR no worse in at least three of four windows, with no window worse by more than 1 dB.
4. Global-polarity mask and output NSE ≤ **1e−8**. This is mostly guaranteed by the feature construction; success here alone is not reconstruction progress.

Record per-window waveform NSE, normal/inverted SI-SDR, loss curves, gradient norm and descriptive frozen-source transmission. Do not substitute protected component transmission for actual summed-waveform contamination or tune these thresholds after seeing output. No DEV, DSD Test, new gain or renderer selection is part of this experiment.

Passing justifies testing this precise mechanism on a matched causal real-data training pair with waveform, entry/counterfactual and real-C diagnostics. Failure rejects this feature replacement at this finite-fit horizon; it does not reject all invariant inputs or declare stereo insufficient. No automatic long training, new architecture stack or device port is queued.

## Execution and checkpoint compatibility

Sources: [feature replacement and loader](../../tools/stereo-upmix-render/separator_invariant_features.py), [fixed-window runner](../../tools/stereo-upmix-render/separator_feature_probe.py), [guarded PC orchestration](../../tools/stereo-upmix-render/pc/run_separator_feature_probe.ps1).

The pre-hook replaces encoder inputs without adding parameters or changing state-dict shapes. A candidate checkpoint records `feature_mode=invariant6`: **load it with `separator_invariant_features.load`, never plain `sep_net.load`**, which would silently restore the wrong feature semantics. Initial and final numerical parity and polarity are recorded in the run. Code outside Track B, including `hook/audio`, is untouched.

The PC wrapper requires an empty rapid queue, no Python/daemon job, the exact script/dependency hashes and an absent GPU lease. It holds one exclusive lease while running sequential 20-step absolute/invariant smokes, checks baseline parity and invariant behavior, then runs one 10k candidate. All output paths must be new; it does not overwrite or silently resume a failed run. Outputs:

- `D:\upmix\runs\separator_feature_smoke_absolute6_v1`
- `D:\upmix\runs\separator_feature_smoke_invariant6_v1`
- `D:\upmix\runs\separator_invariant6_fit_v1`

No package install, head-unit deployment/restart, Track A change, new data import or listening operation is included.

## Execution start

The frozen protocol and three execution sources were committed as `cff502b`. Both 20-step GPU execution smokes passed, and the baseline initial objective reproduced the original control **exactly** (difference 0). At step 20, baseline/candidate objectives were **.162553/.157803**, candidate polarity-output NSE **0**. These are execution checks, not efficacy evidence. The candidate then began its fixed 10k horizon under one lease; the terminal result below supersedes this launch state when present.

## Completed 10k result and decision

The fixed horizon completed. All four windows, raw crop hashes, 586,916 parameters and **125.139 M estimated neural MAC/s** match the protocol. The [result auditor](../../tools/stereo-upmix-render/audit_separator_feature_result.py) verified exact intervention metadata, checked the actual encoder inputs against the independent feature formula and reloaded the final checkpoint on CPU.

| Metric | Original absolute-phase input | Invariant input |
| --- | ---: | ---: |
| 10k objective | .05558584 | .05746568 |
| Median waveform SI-SDR, dB | 7.79656 | 7.63022 |
| Per-window SI-SDR, dB | 9.600 / 12.315 / 3.643 / 5.993 | 9.448 / 11.796 / 3.421 / 5.813 |
| Per-window reconstruction NSE | .0991 / .0580 / .2868 / .1983 | .1021 / .0636 / .2963 / .2057 |
| Recorded final gradient norm | .2510 | .2303 |
| Entire diagnostic runtime, seconds | 450.6 | 701.7 |

The candidate's objective is **3.38% worse**. Per-window SI-SDR deltas are **−.152 / −.519 / −.222 / −.180 dB**: none is nonworse. This is one deterministic finite-TRAIN fit per recipe, not a training-seed confidence interval or generalization comparison. Runtime includes Python feature pre-hooks and the added polarity evaluations; it does not isolate feature processing cost or predict an int8 device cost.

| Predeclared gate | Result |
| --- | --- |
| Objective ≤ .05002725 | **FAIL** |
| Median SI-SDR ≥ 8.79656 dB | **FAIL** |
| At least 3/4 nonworse, no regression >1 dB | **FAIL**, 0/4 nonworse |
| Polarity mask/output NSE ≤1e−8 | **PASS**, both 0 throughout recorded evaluations |

**Decision: no diagnostic promotion.** This input replacement enforces its intended symmetry but does not improve fitting at 10k. Removing polarity sensitivity is insufficient to fix the measured reconstruction deficit. It is not a proof that all invariant inputs fail: this replacement also compresses magnitude and removes within-channel phase relationships across frequency, not just global sign. A representation with fewer memorization cues might generalize differently; no DEV result was collected to claim that either way. The original input remains the fitting anchor. No real-data extension, C screen, new renderer tuning or Track A port followed.

The CPU formula check had maximum input error **4.77e−7**. Reloaded objective was **.05746311**, versus GPU .05746568, waveform SI-SDR matched within .01 dB, and CPU polarity output NSE was 0. Deliberately loading the same weights with plain baseline feature semantics yielded objective **.24768**. This verifies why `feature_mode` and the dedicated loader are essential, rather than leaving a silent model-semantic mismatch.

### Next discrimination

The [privileged decoder-state result](UPMIX_TRAINING_PATH_DIAGNOSIS_2026-09-29.md#3-encoderstate-inference-versus-decoder) still demonstrates much better outputs with the original trained decoder. The next bounded diagnostic should test **learning those fixed decoder states directly**, against a matched continuation using the audio objective, with the same original encoder, checkpoint and frozen decoder. Freeze state targets, coefficient calibration, horizon and waveform success criteria before execution. A successful state-regression route would support a training/credit-assignment intervention; failure still would not prove a universal encoder or causal-information ceiling. This follow-up is not launched or queued by this report. Do not replace it with another width/feature sweep.

### Artifacts and live state

All files are under `D:\upmix\runs\separator_invariant6_fit_v1`:

| Artifact | SHA-256 |
| --- | --- |
| `complete.json` | `2943b88660a56f52444cc80b7b3fbd235e2c246a430b96004e61f96c2dec3752` |
| `fit.pt` | `4334986a48af549302f9fe1dad1d7c0fe5631a5b1cb0ddeb7fedcc501ca19bd9` |
| `result_audit.json` | `f758b86820cefdd886f619d180207cfb957d850898b0f15bfc308245179cb391` |

`manifest.json` preserves script/feature hashes and crop IDs; `progress.json` preserves the complete curve and source contributions. Source hashes: feature module `fa4185560b05970050756030d27b6b68253502a690eaa01d088e801f4f66de03`, fitting runner `3f95d1d04209dfdc4191d7beb60eb22ecf08b1d817c67f621ed80ef58f57ecda`, result auditor `74f1346370f9e1db70141b7aadd54ebed18755032305416a8eae54e77184def5`.

After the terminal markers and CPU audit, read-back found **no GPU lease, zero pending rapid jobs and no Python/rapid-daemon process**. DSD100 Dev cache is still absent; DSD Test remains undecoded/unscored. The goal, independent instrument coverage, device int8/latency measurements and owner listening gates remain unresolved. No head-unit or Track A file changed.
