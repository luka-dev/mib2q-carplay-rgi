# Upmix Training-Path Diagnosis 2026-09-29

_Track B. Completed TRAIN-only diagnostics after the owner requested a thorough investigation of the repeated failures. This records measured causes and limits, not another product candidate._

## Conclusion

The available student does not reconstruct even four fixed TRAIN excerpts accurately after 10k favorable fitting steps. Giving its **unchanged trained decoder** independently optimized local/global states reduces its remaining objective by **69.9%** and raises median reconstruction SI-SDR from **7.80 to 20.59 dB**. This establishes substantial unused capacity in that decoder on these excerpts. The current encoder/state inference and its optimization are a larger demonstrated limitation than the decoder alone. It does not identify which encoder feature, recurrence or optimization change will work on new songs.

A second result concerns the decision process: a complex-ratio oracle reconstructs the clean eligible waveform almost exactly while its frozen source-filter decomposition reports nonzero protected contributions. The oracle and clean eligible waveform produce effectively identical output through the real C renderer. **Frozen-filter protected transmission is a counterfactual sensitivity diagnostic, not the amount of audible voice in the summed eligible waveform.** It must not be used alone as a logically necessary product acceptance gate. Existing poor student/C results remain poor; this finding does not retrospectively promote any candidate.

The [effectiveness audit](UPMIX_EXPERIMENT_EFFECTIVENESS_AUDIT_2026-09-29.md) supplies the other decision-process problems: 4k screens before the planned 20k SI-SDR switch, noncausal normalization, unsuitable repeated-subset controls and incomplete independent instrument coverage. Those are separate from this TRAIN reconstruction result.

## Fixed evidence and scope

Four 8-second MoisesDB TRAIN windows were selected deterministically by SHA ordering of song IDs, then the first of twelve evenly spaced starts with eligible energy fraction .03–.90 and vocal energy fraction at least .03. Selection preceded training. Their raw int16 crop hashes, split hash, source energy fractions, code hashes and exact sample offsets are in `D:\upmix\runs\separator_fit_probe_v1\manifest.json`.

| TRAIN ID | Artist | Start, seconds |
| --- | --- | ---: |
| `f4b735de-14b1-4091-a9ba-c8b30c0740a7` | Drawing Hands | 25.54544 |
| `1fc37390-1769-452d-9bea-19025be4c467` | Firefly | 18.40908 |
| `ad6c3742-e517-42eb-9dec-e74ed05387cc` | Firefly | 19.35535 |
| `fd6e4b4a-f33a-4f3c-aa6e-7c65fd5dc0bc` | Firefly | 19.18181 |

Three examples sharing an artist is acceptable for this finite fitting check. They are not a representative or independent evaluation corpus. All experiments below use these exact TRAIN windows; no DSD Test, untouched instrument set or new DEV selection was used.

The model is the original 586,916-parameter BandSepNet, `norm=stream`, `mask=complex2`, no neural lookahead, estimated neural arithmetic about 125 M MAC/s. This estimate does not certify device int8 cost or the entire renderer chain. The centered 2048/512 STFT and synthesis require a separate latency/accounting and streaming-parity check.

## 1. Cache and numerical path

The [raw/cache audit](../../tools/stereo-upmix-render/audit_fit_cache_consistency.py) re-read the original WAVs for the exact four windows. Its resampling origin is aligned to the 147/160 polyphase grid, with margins; a **single common scale per song across all seven groups** accounts for the historical peak normalization. Native rates were 44.1 kHz, expected files were present, and no selected cached sample hit the int16 clipping boundary.

| TRAIN ID prefix | Raw/cache residual, dB | Maximum residual, int16 LSB |
| --- | ---: | ---: |
| `f4b735de` | −65.58 | 3.24 |
| `1fc37390` | −56.07 | 3.26 |
| `ad6c3742` | −63.01 | 4.32 |
| `fd6e4b4a` | −65.45 | 3.19 |

This rules out a gross source swap, timeline shift or resampling failure **in these selected windows**, not in every song or imported corpus. Small differences include quantization and float accumulation/resampling arithmetic. The fitted scale is not an independent reconstruction of the original cache normalization.

STFT→ISTFT identity normalized squared error was **6.33e−14**. With `M = E conj(X) / (|X|² + 1e−12)`, eligible reconstruction objective was **6.05e−8**, and four waveform SI-SDR values were **115.74 / 106.36 / 108.04 / 114.61 dB**. Therefore the target and audio-loss path can approach zero numerically. This privileged, unbounded ratio is not a student or feasible inference rule. Its mask absolute-value median/p90/p99/p99.9 were **.131 / 1.001 / 2.468 / 7.825**.

## 2. Can the student fit its own four windows?

The [finite-fit probe](../../tools/stereo-upmix-render/separator_fit_probe.py) trains on one fixed resident GPU batch: no remix, gains, pseudo labels or new windows. It uses the existing linear magnitude-plus-complex loss and multiresolution magnitude bundle, constant AdamW learning rate `3e−4`, weight decay `1e−4`, and gradient clipping at 5. This is a deliberately favorable diagnostic, not the production cosine schedule. The frozen sanity gate was two evaluations closing at least 80% of the initial-to-ratio-oracle objective gap, capped at 10k steps.

| Step | Objective | Initial-to-oracle gap closed | Median waveform SI-SDR, dB |
| --- | ---: | ---: | ---: |
| 0 | .207214 | 0% | −6.36 |
| 1k | .067681 | 67.3% | 5.92 |
| 4k | .059205 | 71.4% | 7.07 |
| 6.5k | .057120 | 72.4% | 7.42 |
| 10k | .055586 | 73.2% | 7.80 |

The gate was **not reached**. Final per-window SI-SDR was **9.60 / 12.32 / 3.64 / 5.99 dB**; reconstruction squared-error ratios were **.0991 / .0580 / .2868 / .1983**. The objective kept improving slowly, so this is not proof of a hard capacity ceiling. The maximum gradient norm at recorded evaluations was **.251**, far below the clipping limit; the probe checks every step for finite loss and gradient. These checks rule out recorded clipping dominance or numerical explosion, not vanishing gradients in a particular layer.

A bounded real magnitude-ratio reference achieved objective **.037064**, lower than this student's .055586 on the same four windows. The comparison demonstrates a useful target rule beyond this fitted checkpoint; it does not prove that the current encoder can infer that rule.

## 3. Encoder/state inference versus decoder

The [decoder-state probe](../../tools/stereo-upmix-render/separator_decoder_oracle_probe.py) first captures the trained decoder's exact inputs and reproduces its output with **zero replay residual**. It then removes the encoder and optimizes a separate local state for each band/frame and a shared global state for each frame. Local states stay in `[−3,3]`, global states in `[−1,1]`, the amplitude ranges allowed by this baseline's recurrent and tanh residual paths. Two independent arms start from the same checkpoint states and run 2k steps each:

| Arm | Objective | Remaining checkpoint objective | Median SI-SDR, dB |
| --- | ---: | ---: | ---: |
| Actual trained encoder + decoder | .055586 | 100% | 7.80 |
| Free bounded states, **fixed trained decoder weights** | .016720 | 30.1% | 20.59 |
| Free bounded states and decoder weights | .009475 | 17.0% | 27.26 |

The first relaxation proves the existing decoder can achieve much better reconstruction without wider output layers. Allowing weights to coadapt improves further. These states use complete training targets, arbitrary per-frame values and future optimization; they are neither causal cues nor feasible student states. This is an existence result for those decoder outputs, not a rigorous minimum-error bound or generalization claim.

The baseline has 32 local and 32 global state values and one linear per-band complex-mask decoder. Its five widest bands contain 85/85/85/85/89 bins. The resulting low-dimensional mask basis is a concrete representation constraint, but the successful fixed-weight relaxation shows that blaming that constraint alone would miss a large part of the observed error.

## 4. Why source-filter transmission can reject a clean waveform

For the mixture-inferred frozen operator, `M(V+D+B+E)` decomposes algebraically into four contributions. Their energies do not add: the contributions contain cross terms and can cancel. In the exact-ratio oracle, the sum reconstructs clean `E`, while vocal transmission on the four windows is **.186 / .080 / .111 / .244** and non-silent bass transmission is **.148 / .270 / .068**. Calling those numbers audible voice/bass contamination in the final summed eligible waveform is incorrect.

The [real-C consistency audit](../../tools/stereo-upmix-render/audit_separator_oracle_renderer.py) rendered the exact eligible target and the reconstructed ratio output through the same `upmix_render_inj.exe`, fixed historical harness `shipped` knobs, model and guard, `--mask-drive 1`, and one gain shared by every window and arm. It did not change renderer parameters or touch the hook.

| TRAIN ID prefix | Rear-input PCM maximum difference, LSB | Six-channel output NSE | Maximum output difference, LSB |
| --- | ---: | ---: | ---: |
| `f4b735de` | 1 | 9.35e−10 | 2 |
| `1fc37390` | 1 | 7.72e−9 | 3 |
| `ad6c3742` | 1 | 6.96e−10 | 2 |
| `fd6e4b4a` | 1 | 1.02e−9 | 3 |

This demonstrates equivalent C output despite nonzero protected frozen-filter contributions. It does **not** demonstrate zero voice in the entire rear: the shared renderer also receives the original mixture, and its other rear paths remain active. Nor does it prove that a learned mask remains correct when vocal level changes. Frozen-source transmission is useful for that counterfactual sensitivity, and actual vocal-entry stability remains required.

The harness's rear-share estimate is also conditional: it examines cells where a dry source dominates the actual remainder by 10 dB and excludes frequencies below 300 Hz. It is **not OLS attribution** of the complete output. Before the harness's additional time/weight validity checks, the four windows' eligible source-energy coverage in these cells is **27.9 / 49.8 / 23.4 / 54.4%**; drum coverage only **3.9–4.7%**, and non-silent bass coverage about **.000035–.878%**. This broad-group example does not estimate coverage for every separate instrument. A small own-cell rear share cannot certify the unmeasured overlapping or low-frequency energy.

## 5. Physical polarity stress check

The [CPU audit](../../tools/stereo-upmix-render/audit_separator_polarity.py) negates both mixture and target, then negates the inferred output back. A correct diagonal ratio is unchanged by this global polarity flip. Exact target audio, energy and source identities are otherwise unchanged.

The finite-fit checkpoint's objective worsened **.05558→.13688**, with median SI-SDR **7.80→−.97 dB**. Thus its apparent four-window fitting gain includes sensitivity to the particular waveform realization; it is not evidence of source identity learning. The historical Cambridge 4k control was much less affected on those same TRAIN crops: objective **.11905→.11953**, median SI-SDR **1.50→1.58 dB**, although output-difference squared-energy ratios were **.0227–.0856**. This stress test does not establish polarity sensitivity as the main cause of the production frontier, and does not compare the two checkpoints as a matched efficacy pair.

An initial CPU launch failed before evaluation because the generic loader's CUDA availability result disagreed with initialization after hiding devices. The diagnostic now explicitly loads owned checkpoint tensors and the model on CPU. The corrected `polarity_audit_v2.json` is terminal; the failed launch supplied no result and used no training step.

## 6. Encoder activations and gradients

The [CPU activation audit](../../tools/stereo-upmix-render/audit_separator_encoder.py) reproduced the fixed checkpoint objective within CPU/GPU arithmetic tolerance and measured both seed-1 initialization and the trained model on the identical batch/stats.

| Measurement | Initial encoder | Fitted encoder |
| --- | ---: | ---: |
| Encoder tanh values with absolute value > .95 | 14.6% | 22.0% |
| Absolute value > .99 | 10.7% | 15.3% |
| Mean tanh derivative | .698 | .575 |
| First four low-frequency bands, abs > .95 | 75.7% | 79.6% |
| Bands starting above 8 kHz, abs > .95 | .52% | 8.77% |

There is localized saturation, particularly below about 375 Hz, but no evidence here that the entire encoder is dead. Fitted block gradient L2 norms were encoder **.0902**, first/second GRU **.2275/.0410**, cross-band mixers **.0183/.0102**, global GRU **.0107**, decoder **.0165**. Every audited block receives a nonzero gradient. Norms from differently sized blocks are not direct contribution rankings, and a single-batch backward pass does not establish long-run optimization health. Low-frequency saturation does not by itself explain the historical treble suppression. The next intervention must test feature/state learning rather than asserting a severed gradient path or universal tanh failure.

## What changes next

1. **Correct the decision protocol before another main comparison.** Retain the owner's C instrument/protected/stability thresholds. Evaluate actual eligible-waveform reconstruction, its absolute scale, the real-C output, vocal counterfactuals and entries together. Keep frozen-source transmission as a sensitivity/ownership diagnostic; do not automatically identify its component energy with audible contamination. Historic negatives are not promoted, and no threshold is tuned on these four windows.
2. **Investigate the encoder's bottleneck with one controlled mechanism.** Predeclare a same-window causal comparison of the existing encoder versus a polarity-invariant current-frame representation (magnitude plus relative stereo phase/level) with a direct local evidence route. Use the measured localized saturation and nonzero block gradients when selecting the exact change; no globally dead encoder was found. Preserve decoder/output, loss, seeds and total horizon. Do not stack a new dataset into this diagnostic. Recheck reconstruction and polarity, then test the chosen mechanism on real training and DEV; a finite-fit gain alone is not promotion. Existing skip/spatial variants in the ledger must be compared before treating this as a new idea.
3. **Use a real learning horizon for the subsequent data experiment.** Only after the fitting mechanism/protocol are defined, compare same-song causal original Moises TRAIN against audited DSD Dev addition. The main comparison retains a fixed 40k horizon and its 4k/10k/20k/40k checkpoints rather than replacing it with independent short cosine runs. Measure convergence and source exposure, not merely the last short-run score.
4. **Keep the product boundary.** DSD100 downloaded and passed full byte CRC/SHA, but its Dev cache has not run; Test audio remains undecoded/unscored. Four broad sources cannot provide the untouched guitar/keys/piano/strings acceptance set. Track A coordination, measured int8 full-chain ≤150 M MAC/s, ≤85 ms additional latency and the owner's deferred live-CarPlay blind test are still required.

No new architecture/data efficacy job is queued by this report. The completed diagnostics released their GPU leases; the rapid queue is empty. Existing `operator_anchor4k` remains terminal. Track A, the head unit, other agents' dirty files and the PC Python environment were untouched.

## Reproducible artifacts

| Artifact | PC path under `D:\upmix\runs\` | SHA-256 |
| --- | --- | --- |
| Finite-fit terminal JSON | `separator_fit_probe_v1\fit_complete.json` | `47b4c447bab0bc421cbbdf4d733f522bd5b16944adbf4179e0893c2a60448515` |
| Final TRAIN checkpoint | `separator_fit_probe_v1\fit.pt` | `0e560b9924feea88f3e61b97253734c60a02a3901c7636779e107653d7e77e41` |
| Decoder relaxation | `separator_decoder_oracle_probe_v1\decoder_result.json` | `e802c7a214b55c4eefa73feff9c2f5b174682b4d842ba71f9031f12c56f0ac0f` |
| Real-C oracle parity | `separator_oracle_renderer_audit_v1\result.json` | `5c4f8d694e90e87083b71e4398ff5e07c08aa83667c95e198653a7ece87bd350` |

The adjacent `manifest.json`, `references.json`, `progress.json`, `cache_audit.json`, `polarity_audit_v2.json`, `encoder_audit.json` and logs preserve the per-window results. Diagnostic sources are committed in `tools/stereo-upmix-render/`; guarded single-GPU runners are [`run_separator_fit_probe.ps1`](../../tools/stereo-upmix-render/pc/run_separator_fit_probe.ps1) and [`run_separator_decoder_oracle_probe.ps1`](../../tools/stereo-upmix-render/pc/run_separator_decoder_oracle_probe.ps1). Both refuse an existing output, pending rapid queue or existing lease and remove only their own lease token. A session interruption did not restart any completed work: terminal files and live state were checked before continuing.

## Follow-up: input symmetry did not repair reconstruction

The [invariant-input experiment](UPMIX_INVARIANT_INPUT_PROBE_2026-09-29.md#completed-10k-result-and-decision) completed the proposed first representation comparison. Same original trunk/decoder, 10k horizon and fixed TRAIN windows: polarity output NSE 0, but objective **.05559→.05747**, median SI-SDR **7.80→7.63 dB**, four per-window regressions. It fails the frozen reconstruction gate, with no generalization or device conclusion. CPU feature/checkpoint parity passed. Next directly test state-learning supervision against matched audio-objective continuation before another representation sweep; keep the original encoder/decoder anchor and predeclare the TRAIN-only protocol. No next GPU job is queued.

## Follow-up: direct state teaching did not close the encoder gap

The [matched pure-state/audio continuation](UPMIX_DECODER_STATE_SUPERVISION_2026-09-29.md#completed-matched-2k-result) completed 2k per arm with the original fixed decoder. State target MSE fell only 5.86%, audio loss was 19.63% worse than control, median SI-SDR 7.79→7.54 dB; all gates failed. CPU audit confirmed correct targets and unchanged decoder. A descriptive per-band projection found 45.44% of initial coordinate error invisible to that band's decoder, so this plain coordinate assignment is not a clean proof about inference capacity or acoustic optimization. The current privileged states remain a useful representation demonstration, not an easily learnable latent teacher. Next hypothesis: redistribute the existing 48-band budget to narrower high-frequency groups while preserving complex features, topology and neural MAC totals; predeclare one paired fitting protocol, no sweep or automatic extension. GPU/queue are clear and no deployment or listening claim follows.

## Follow-up: same-budget frequency redistribution

The [completed band-allocation probe](UPMIX_BAND_ALLOCATION_PROBE_2026-09-29.md#completed-10k-result-and-decision) changed only the 48 frequency groups, keeping nominal neural MACs, parameter count and original absolute complex features/objective. The exact four TRAIN windows improved only .76% in objective and .155 dB in median SI-SDR at 10k; one window regressed .338 dB. The frozen joint diagnostic gate failed. CPU loader/crop/budget parity passed. No new causal encoder recipe has materially closed the known fitting gap, and no hard capacity/generalization bound follows from these finite negative probes. Keep the original geometry for the separate full-horizon efficacy question.
