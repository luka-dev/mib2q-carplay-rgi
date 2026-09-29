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

## Completed matched 2k result

The corrected `v2` ran both continuations from the identical 10k anchor. Before training, the audio/state gradients were **.252522/.280728**, giving frozen state coefficient **.899527**; gradient cosine **.658415**. Both arms' initial audio loss, state loss and per-window waveform scores matched exactly. Decoder weights remained **bit-identical** in both saved checkpoints.

| Metric after 2k further steps | Audio continuation | Pure state supervision |
| --- | ---: | ---: |
| Actual audio objective | .05530235 | .06615708 |
| Median waveform SI-SDR, dB | 7.79414 | 7.53621 |
| State-coordinate MSE | .646579 | .607725 |
| Per-window SI-SDR, dB | 9.575 / 12.359 / 3.630 / 6.014 | 8.967 / 11.511 / 2.843 / 6.106 |
| Waveform reconstruction NSE | .0996 / .0575 / .2884 / .1984 | .1129 / .0676 / .3241 / .1931 |
| Recorded final gradient norm | .2334 | .3253 |
| Runtime, seconds | 79.9 | 64.0 |

State MSE started at **.645583**: direct teaching reduced it only **5.86%**, not the frozen 50% mechanism target. Its audio objective was **19.63% worse** than the audio control. Per-window SI-SDR deltas were **−.608 / −.848 / −.787 / +.092 dB**, only one nonworse. All four gates failed. **Diagnostic promotion: NO.** No coefficient retuning, longer extension, DEV/C screen, data addition or device port followed.

The audio continuation's small objective improvement over the .055586 anchor does not close the fitting gap. Direct coordinate teaching is not a shortcut to the 20.57 dB privileged output on this tested assignment/horizon. This does not isolate a universal encoder capacity limit: the freely optimized states need not satisfy the recurrence, their coordinates are not unique, and the plain coordinate loss has different acoustic weighting.

### Independent CPU audit and target mismatch

The [CPU auditor](../../tools/stereo-upmix-render/audit_separator_state_supervision.py) replayed the saved targets through the original decoder (objective **.01671428**, within CPU/GPU tolerance of .016716), reloaded both student checkpoints, verified both decoders against the original anchor and recomputed audio/state losses, waveform scores and all frozen gates. All consistency checks passed.

It also measured decoder-visible state error as a descriptive check, **without changing the loss or promotion rule**. For each band, a rank-truncated SVD at relative singular-value threshold `1e−6` splits the concatenated local/shared-global error into a decoder row-space projection and an error in that band's null space. The first eleven four-bin decoders have rank **16** for 64 input coordinates; the next eleven rank **32**; the remaining 26 rank **64**.

| Descriptive coordinate measure | Initial anchor | Audio continuation | State supervision |
| --- | ---: | ---: | ---: |
| Local-state MSE | 1.24121 | 1.24212 | 1.17713 |
| Shared-global-state MSE | .04996 | .05104 | .03833 |
| Per-band concatenated error energy in that decoder's null space | 45.44% | 45.47% | 45.86% |
| Actual STFT output gap to privileged teacher, NSE | — | .17225 | .18298 |

Those null directions do not change the **particular band's** immediate linear decoder output. Shared global state is repeated per band in this calculation; it is not a decomposition of the full jointly constrained shared-global operator. Nor is a hidden direction necessarily useless for recurrence, cross-band context or another decoder. Therefore do not state that 45% of the network or all state labels are wasted. The measurement shows why plain state-coordinate error is an imperfect acoustic surrogate. Indeed the state arm moves closer in coordinate MSE while its actual output gap to the teacher grows **6.23%**.

### Next architectural hypothesis

Keep the original waveform objective/anchor rather than extend this raw state-MSE recipe. The measured decoder geometry and earlier treble loss support one **band-budget redistribution** experiment: keep 48 bands, input/hidden/output widths, recurrence and estimated neural MAC/s fixed, while reducing the widest high-frequency bands. For example, replace the original widths `4×11, 8×11, 16×13, 32×8, 85×4+89` with `8×11, 16×11, 24×13, 32×8, 38×4+41`. Both cover exactly 1025 bins in 48 bands; maximum width falls **89→41**. Encoder/decoder matrix-weight totals, shared recurrence and nominal neural MACs remain unchanged, while the frequency allocation changes.

This tests whether narrower instrument/harmonic groups make current-frame evidence and masks learnable, not a claim that low bands can safely lose detail. Low-frequency source protection and midrange instruments may regress; absolute complex features and phase information stay intact. Before execution, freeze a matched original/new-geometry TRAIN fitting protocol, source/loader semantics and waveform gates. Passing finite fitting would still require real-data, entry/counterfactual and C confirmation; no independent/device/ear gate is relaxed. Review the earlier skip/geometry history before running, and select this one intervention rather than a grid. No such run is queued by this report.

### Artifacts and terminal state

Directory: `D:\upmix\runs\separator_state_supervision_v2`.

| Artifact | SHA-256 |
| --- | --- |
| Final paired `complete.json` | `bb21ac6e9b167c9d88e8bbe470d33d239bad009badb9fe8d12a2037339643e99` |
| `result_audit.json` | `ad3a5a88f0ee018798d975e0e85855c741656e6424f2e5e3c0c7aa2dba21fb7f` |
| Frozen `targets.pt` | `8c37dc3c9f0971e9360bf9a9c421d036d2551f8d557d9fa44931954f3aacd040` |
| Audio continuation `audio/fit.pt` | `a8c6813e4f1270f2edfcec63c2913fcf116526187937a732cf9639f153733008` |
| State continuation `states/fit.pt` | `c68798844035681bfe616c5c7840e2985a4a670cee17949d6221f5d6d0376202` |

Probe source SHA-256: `51bc861ea09533aa1d2ae58f98e0cfb38b81f6b8a9b909758990a9fa0c8b48d1`; final CPU auditor: `64ed86f3782708963593f47d429aeac5e7d31c8aceac13350a30edb5cc581ace`. The `v1` target-only attempt remains preserved, with no student result. Corrected run source/mode handling was committed as `fbc8eb1` before final auditing.

Post-run read-back: no GPU lease, zero rapid pending jobs, no Python/rapid daemon. DSD Dev cache is still absent; Test remains undecoded/unscored. No head-unit/Track A/package operation occurred. The overall product goal remains unmet.
