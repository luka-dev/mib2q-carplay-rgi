# Upmix Decoder-State Supervision 2026-09-29

_Track B. Frozen TRAIN-only continuation protocol. Same encoder and fixed decoder, different supervision paths._

## Question

The [training-path investigation](UPMIX_TRAINING_PATH_DIAGNOSIS_2026-09-29.md) obtained median SI-SDR **20.59 dB** by optimizing inputs to the unchanged trained decoder, versus **7.80 dB** from the actual trained encoder. The [input-invariance test](UPMIX_INVARIANT_INPUT_PROBE_2026-09-29.md) enforced polarity symmetry but did not improve reconstruction. This experiment tests whether direct teaching of one good decoder-state assignment improves the existing encoder's output, compared with a matched audio-loss continuation.

These privileged target states are labels during training only. They are not inferred from stereo, may use future target audio during optimization, and are not a deployable teacher. A causal encoder still receives the same stereo mixture and emits its own states at inference. Good target states exist; their causal predictability is not established.

## Freeze before training

- Four exact 8-second MoisesDB TRAIN windows from `separator_fit_probe_v1/manifest.json`, same crop and split hashes. No new windows, gain, remix, pseudo labels, DEV or DSD Test.
- Both arms start from the original seed-1 `separator_fit_probe_v1/fit.pt` at step 10k, SHA-256 `0e560b9924feea88f3e61b97253734c60a02a3901c7636779e107653d7e77e41`.
- Original causal stream-normalized BandSepNet, absolute complex input features, no skip/lookahead. **Every decoder parameter is fixed in both arms.** The encoder, time GRUs, cross-band mixers and global GRU are trainable.
- Fresh AdamW in each arm, learning rate `3e−4`, decay `1e−4`, gradient clipping at 5, **2k further steps**, seed 1. This is a matched fresh-optimizer continuation, not restoration of the old optimizer state.

## Target construction and supervision

Reproduce the completed fixed-decoder privileged relaxation: capture actual local states `h[B,T,48,32]` and shared global state `g[B,T,32]` from the anchor, optimize only those values with Adam `.03` for 2k steps using the original audio objective, constrain local/global ranges to `[−3,3]` / `[−1,1]`. Decoder weights stay exactly fixed. Check zero replay residual and require the final objective to reproduce the prior .016720 result within `1e−4`. Save the resulting target tensors and hashes before student training.

| Arm | Training objective | Common output evaluation |
| --- | --- | --- |
| Audio control | Original linear magnitude/complex + multiresolution magnitude audio objective | Same audio objective, waveform SI-SDR/NSE and target-state MSE |
| State supervision | **Pure** `.5(MSE(h,h*)+MSE(g,g*))`, multiplied by one frozen calibration coefficient | Same |

The coefficient equals audio-gradient L2 norm divided by state-gradient L2 norm over trainable encoder parameters on the initial frozen TRAIN batch. This matches the initial aggregate gradient magnitude. Record their cosine and both norms before either continuation. It does not guarantee matched later gradients or optimizer updates; Adam also rescales coordinates. No coefficient or horizon is tuned after the paired results.

State coordinates are not unique; decoder null directions and unequal output sensitivity can make ordinary coordinate MSE an imperfect target. Both groups' means have equal weight, not equal acoustic contribution. Failure of this particular assignment/objective cannot prove that the encoder cannot learn some equivalent good states or that audio credit assignment is the only problem.

## Predeclared diagnostic success

Relative to the final matched audio continuation, the state arm must jointly have:

1. Actual audio objective at least **10% lower**.
2. Median waveform SI-SDR at least **1 dB higher**.
3. At least **three of four** windows with nonworse SI-SDR, none worse by over 1 dB.
4. State MSE reduced by at least **50%** from its own initial value, confirming the intended training mechanism.

All decoder parameters must remain bit-identical to the anchor. Report curves, per-window acoustic results, state errors and gradient norms. Lower state error alone is not success; waveform deterioration is a failed diagnostic even with successful state imitation. Passing supports testing this precise training mechanism on a proper real-data pair, not product promotion. No C/deployment or long-run job is automatically queued.

## Reproducibility and execution

[Probe source](../../tools/stereo-upmix-render/separator_state_supervision_probe.py) and [guarded PC runner](../../tools/stereo-upmix-render/pc/run_separator_state_supervision_probe.ps1). Output: `D:\upmix\runs\separator_state_supervision_v1`, with `targets.pt`, `teacher.json`, `calibration.json`, `manifest.json`, two arm directories and final `complete.json`. Model checkpoints retain the ordinary baseline feature semantics; `targets.pt` is a training artifact, not an inference model input.

The runner checks source, anchor and privileged-reference hashes, refuses an existing output/pending rapid queue/Python or rapid-daemon process, and acquires an exclusive GPU lease. Target optimization and the two continuations run sequentially on that one tenant. It releases only its own token. No package install, head-unit restart/deployment or Track A file change is included. Device int8 cost, full-chain latency, untouched instrument coverage and owner listening remain separate gates.

## Execution correction before the paired continuations

Protocol/source commit: `ea87de0`. The initial `separator_state_supervision_v1` produced verified targets (teacher median SI-SDR **20.57 dB**, audio objective **.016716**) but stopped before either student arm at gradient calibration: `sep_net.load` returns an evaluation-mode model, and CUDA/cuDNN recurrent backward requires training mode. This was an orchestration bug, not a numerical training result.

The correction explicitly calls `modules.train()` before differentiating the calibration model and both continuation models. No topology, objective, data, coefficient-selection method or horizon changed. The failed directory was preserved. The corrected run uses fresh output **`D:\upmix\runs\separator_state_supervision_v2`**, reuses the saved target tensors after checksum/source/window/bounds/reconstruction checks, and records their provenance. It does not repeat target optimization or restart a student checkpoint. Earlier `v1` output descriptions above are the original protocol; the terminal results below use `v2`.
