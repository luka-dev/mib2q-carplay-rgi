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
