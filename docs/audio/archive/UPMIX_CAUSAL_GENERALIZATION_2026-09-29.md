# Causal reference: broad true-stem generalization

_Track B. Protocol frozen2026-09-29; both40k checkpoints, CPU audit and full40 source-entry/C comparison completed2026-09-30. Causal PC reference above the device compute allowance._

## 🎯 Question and scope

The [completed causal fitting pair](UPMIX_CAUSAL_REFERENCE_ENCODER_2026-09-29.md) improved median SI-SDR 7.754→10.500 dB and objective by 20.584% on four known TRAIN crops, with the same trained decoder. Its vocal/bass attributions increased, high frequencies remain weak and the reference costs 2.168 G estimated neural MAC/s. It needs a natural-song generalization test before compression.

Compare **the same two causal encoder packages on the complete original MoisesDB TRAIN pool**, with matched draws and supervision. This tests whether the demonstrated fitting advantage survives broader training. Mixed-corpus training, teacher output distillation and loss changes remain separate comparisons after this result. The renderer comparison uses the already frozen original/direct C routes; there is no hook port.

## ✅ Completed full40 result, 2026-09-30

Both40k arms, saved-checkpoint CPU replay, **40 DEV songs ×60s ×both C routes**, exact231 stem-row coverage per route and **265 full-song source-attributed events per arm** completed. Evaluator and supervisor returned0; final readback has no Python evaluator/trainer and no GPU lease. No next job is queued. Main [complete decision](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_complete.json): **all11 frozen relative quality gates pass; absolute causal product gate fails**. This preserves the stronger causal reference for a targeted mechanism experiment; it is not a product completion.

### Fixed direct-C result at the original gain

| Metric | Control40k | Reference40k | Absolute requirement | Reference result |
| --- | ---: | ---: | ---: | --- |
| Guitar rear share | .103295 | .121142 | ≥.15 | Fail |
| Keys rear share | .104612 | .159371 | ≥.15 | Pass |
| Piano rear share | .091219 | .134890 | ≥.10 | Pass |
| Strings rear share | .102565 | .108989 | ≥.14 | Fail |
| Voice rear share | .015153 | .005917 | ≤.02 | Pass |
| Bass rear share | .053625 | .052086 | ≤.06 | Pass |
| Drums rear share | .009577 | .008540 | ≤.08 | Pass |
| Any-stem swinging songs | 0/40 | 0/40 | ≤5 | Pass |
| Guitar/keys/wind absolute depth-level slopes | .015740/.019570/.017812 | .021117/.024371/.021009 | ≤.05 each | Pass |
| Source-attributed vocal-entry ducking median | .806338 | .803796 | ≥.85 | Fail |
| Vocal-entry ducking p10 | .432415 | .419774 | ≥.60 | Fail |
| Direct-C median rear RMS, dBFS | −39.393480 | −39.027645 | Within.5dB across arms | Pass, +.365834dB |

The reference passes **10/14** recorded observed direct-route gates. Its direct/original-C rear difference is only **−.032572dB**; the control direct/original-C difference was−.645444dB. Gains were fixed before inference; no quieter-rear rescue or posthoc gain adjustment. The between-arm+.365834dB meets the frozen tolerance but is not exact acoustic level matching.

**Renderer dependence remains material:** with the original C route, control→reference any-stem swings **10→15/40**, drums rear share **.093867→.095876** (both fail); reference guitar/keys/piano/strings **.121066/.130421/.101760/.089652**, voice/bass **.015993/.052854**. The direct-route0/40 was already present in the control. Thus the improved encoder alone does not repair the original renderer's stability and must not be credited for removing swings.

### Extraction and vocal-entry evidence

Natural20s paired DEV SI-SDR gain remains **+1.302827dB**,90% interval **[1.051355,1.443251]**,39/40 nonworse; independently recorded by the completed evaluator. On full60s, median SI-SDR is **−.375794→+.713427dB**, actual-mixture-mask eligible/vocal/drum/bass transmissions **.429580/.127615/.025759/.029043 → .450696/.077266/.022632/.027400**. Upper-band eligible transmission improves **.007559→.021407 (8–14k)** and **.003135→.011576 (>14k)**, but remains weak; transmission is extraction energy, not rear share.

Entry coverage is84 events in36 songs, with83 valid ducking ratios; exits79 and sustained controls102, total265 across all40 songs in each arm. Median absolute eligible pre/post entry retention improves **.501457/.385067→.645218/.490729**. Ratio median/p10 slightly worsen **.806338/.432415→.803796/.419774**, within the predeclared .02 relative allowance but below absolute .85/.60. Sustained median/p10 ratios improve **.967229/.705597→.993441/.736453**. The reference preserves more instruments on both sides of a vocal entry, but still ducks when singing starts.

Verified local captures: all four CSV exports contain identical unique231 song/stem keys; all40 extraction closures per arm ≤**4.881e−14**;265 event rows per arm cover exactly40 songs and satisfy≤1e−9 attribution closure. Final model hashes equal independently audited40k hashes; final evaluator protocol hash matches its exact exported protocol. No independent20, device int8/latency or owner listening test is claimed. Estimated reference neural compute remains2.168G MAC/s versus≤150M device requirement.

### Decision and next experiment

The larger causal encoder package gives a reproducible DEV reconstruction/selectivity improvement and reaches the keys/piano rear thresholds, so the architecture comparison has a useful positive result. **Guitar, strings and vocal-entry retention remain the binding measured failures**, plus the current reference's compute cost. This single-seed, warm-started package comparison is not an equal-compute ablation or proof of a unique cause.

Follow the frozen decision plan: retain both terminal checkpoints and the stronger reference; prepare **one matched source/loss mechanism experiment** on the unchanged best true-stem recipe, using per-song guitar/strings and entry errors to select its intervention. First distinguish mask/target representation limits from objective tradeoffs on these observed failures. Compression becomes useful once the target behavior is established; a new architecture sweep or merely extending this finished horizon is not justified by this result. No new job, hook port or shared-package install is started by this report update.

### Exact completed evidence

| Artifact | Exact PC capture |
| --- | --- |
| All final relative/absolute decisions and both arms | [Complete result](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_complete.json) |
| Reference full40 source/C summaries | [Reference full40](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_residual_full40.json) |
| Frozen C/source evaluation protocol | [C protocol](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_c_protocol.json) |
| All265 control transition rows | [Control transitions](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_original_transitions.json) |
| All265 reference transition rows | [Reference transitions](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_residual_transitions.json) |
| Control231 original/direct C stem rows | [Original C](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_original_baseline_stems.csv), [direct C](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_original_direct_stems.csv) |
| Reference231 original/direct C stem rows | [Original C](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_residual_baseline_stems.csv), [direct C](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_residual_direct_stems.csv) |

- `complete.json` SHA-256 `5337c8e64a0b9203a819ab03d34fa37d9b04a40abd699ffd453621edf7a047a9`.
- `residual_full40.json` SHA-256 `3d3153d92d4db26683b8a28b32fe78846bbe2fddc2b124cfd4cd983a16299c59`.
- `c_protocol.json` SHA-256 `df5d16048b236c33b53ebf66ac2301233e2a5ba232beb3ca9bac851be911908e`.
- `original_transitions.json` SHA-256 `7888f1af4ce4ba1291b0e9b1c3f0fdc3821d61d14f1ad0b54fa57f6f52344096`.
- `residual_transitions.json` SHA-256 `01e016513faba6eaa1d54496f6d939329ab198bf6e77065eeea29739b70214fc`.
- `original_baseline_stems.csv` SHA-256 `87e35f75d9329f2a737d0ec53505a919f37c22a1c60b03a3af345359a5e6c24c`.
- `original_direct_stems.csv` SHA-256 `5e80ae1d16d51f8734a9934e340936c7e24a5d4734eb38635984aeeaa796e7a4`.
- `residual_baseline_stems.csv` SHA-256 `94ebaef6b941574f0a5155e9a61b7167da041151397b299eebed82a9a1c865c0`.
- `residual_direct_stems.csv` SHA-256 `dd3162489e36bad2a280801c2f3e9b286aafe551fd6d9367cd7493777b8cbe7a`.

The following preparation, pause and live snapshots are preserved as history; they no longer describe a running job.

## 🧊 Frozen initialization and data

- Both warm-start from their exact completed 10k fitting checkpoints. Each had the same four TRAIN crops, horizon, frozen trained decoder and common normalization statistics; their trained encoders differ. Fresh optimizer in both arms. Report this as a warm-started package comparison, not training from scratch or an isolated equal-compute ablation.
- Original checkpoint SHA-256 `7d3d1fde1f8bd2c64bd539292a7cb0eac4bce19b8377b06f7c22a32ed4b4e470`; residual96×4 `682c09f1d253fb694bfde6fa43e1bfd1168f9161398a3abd969b0ad7fec1603a`.
- The common decoder starts bit-identical, SHA-256 `d04d9fd5a73ebb3788240772b3be0fb1ecf420e059ddfc2a0bf5aeff1f4fec05`. **All parameters, including decoder, now train** in both arms. Record decoder gradients and hashes; freezing it indefinitely would unnecessarily constrain broad learning.
- Retain common fitting statistics `mean=-3.6760525703430176`, `std=2.43422794342041` to preserve checkpoint semantics. They are fixed TRAIN-derived constants, not statistics over DEV or future input; broad recalibration is not tested here.
- Cache `D:\upmix\datasets\masknet_cache7`: **176 TRAIN / 40 DEV / 24 Test** present. Seven real-stem int16 slots at 48 kHz. Existing split hash `3972aa752b44d872c84b002c217735ca2ff5da0653bdeeb8c1003262f0bd265f`. Preparation verified disjoint song IDs and zero TRAIN/DEV artist or normalized artist/title overlap. This is a metadata audit, not a full audio-fingerprint duplicate audit.
- Training draws all 176 songs uniformly, valid 8-second offsets uniformly, separate ±6 dB gains for the seven slots. The mixture is exactly the sum of those gained stems. Eligible = slot 6; vocal = 0+1; drums/percussion = 2+3+4; bass = 5. No same-window limiter or independent target scaling.
- No pseudo labels, remix, extra corpus or privileged inference. In particular, DSD Test and the 24 Moises Test audio files stay unopened. The separate 20 untouched instrument-covered song gate remains reserved.
- Step-local RNG `[seed=1, step]` fixes donor, offset and gain sequence independently of optimizer/evaluation activity. A rolling descriptor SHA, per-song exposure and source-audibility counts accompany every saved progress row; the CPU auditor requires identical sequence hashes across arms. Descriptor hashes identify sampling decisions, not full-cache audio hashes.

## 🏃 Full training horizon

| Setting | Both arms |
| --- | --- |
| Added broad-training steps | **40,000**, after the completed 10k fitting initialization |
| Batch / duration | 4 × 8 seconds; no gradient accumulation |
| Total sampled windows | 160,000 per arm, 355.56 hours of augmented crop exposure |
| Optimizer | Fresh AdamW, weight decay 1e−4, clip norm 5 |
| Precision | Float32; cuDNN/matmul TF32 disabled |
| Schedule | 2k linear warmup to 3e−4, cosine to 3e−5 at 40k |
| Objective | Unchanged `separator_fit_probe.objective`: linear magnitude/complex STFT + original multiresolution magnitude loss |
| Loss switches | None; no new source, treble, SI-SDR or teacher term |
| Progress / recovery | Every 250 steps, atomic checkpoint with optimizer and exposure history |
| Acoustic snapshots | Step 0 / 1k / 4k / 10k / 20k / 30k / 40k |
| Selection | Fixed terminal 40k in both arms, no DEV-selected best checkpoint |

This continues the demonstrated causal recipe on broad data with a common newly trainable decoder. It is distinct from the deferred DSD efficacy protocol, whose pseudo mix and SI-SDR stage are not silently imported. Equal steps and crop exposure do not establish equal compute or convergence. Preserve curves through 40k; do not reject solely on a noisy 4k ranking. Nonfinite output/gradient, changed inputs, numerical audit failure or a worker stall are execution failures, not acoustic negative results.

## 🔎 TRAIN/DEV curves and CPU audit

Prepare **12 fresh TRAIN monitoring songs**, deterministically ranked by ID hash, excluding all four fitting-song IDs. Their natural crops use the established loudest-vocal selection, with up to 10 s of preceding causal warmup and 20 s scored. Training still samples all 176 songs. DEV uses all 40 declared songs under the identical crop/warmup convention. Freeze crop starts, duration and int16 byte hashes before GPU execution.

At each acoustic snapshot, preserve every song's objective, waveform SI-SDR/NSE, frozen actual-mixture-mask eligible/vocal/drum/bass attribution, frequency-band error/transmission and attribution closure. Quiet absent sources remain undefined. These contributions can cancel in summed audio, so increased attributed energy is a selectivity concern to assess jointly with reconstruction and actual C output.

Both 20-step TRAIN-only GPU smokes precede full training. A CPU replay of their two fresh TRAIN monitor crops must verify strict saved-checkpoint semantics, trained decoder change, source coverage and prefix future-perturbation NSE ≤1e−10. Final 40k checkpoints receive the same independent CPU replay. Tolerances: objective 1e−4 absolute, SI-SDR .05 dB, transmission .005. The auditor checks finite weights, exact checkpoint/protocol hashes and matched sample-sequence digest. This is temporal causality of the fixed-STFT neural network; stateful waveform transform, total latency and device execution remain uncertified.

## 🎛️ Fixed terminal DEV40/source-entry/C comparison

After both 40k arms and CPU audit, evaluate **all 40 previously inspected DEV songs × 60 s** from the original real-stem harness. No independent Test is spent on this development decision.

- Use original harness normalization `0.7 / dry_mix_peak`, injection gain **1.0**, no fitted gain or clipping to rescue a model.
- Infer each whole natural excerpt continuously. Apply the **actual mixture-inferred complex mask** to each true source, verifying their summed attribution reproduces the estimate, normalized residual ≤1e−9. No reset at 11 s, overlapping model chunks or isolated-stem network inference.
- Render both arms through the frozen original C and frozen `direct_no_bed_body0_cal` C route (rear body 0, surround .3235). Model/guard/C hashes are frozen by the runner. This holds routing constant between architectures and retains the separate renderer comparison.
- Require the exact original **231 song/stem rows**, including the previously corrected two quiet stems. Use the existing metric helper's corrected scaled-lead eligibility expression. Record source shares, any-stem unique-song swings, guitar/keys/wind depth/level slopes and per-song rear RMS.
- Detect source-attributed entries/exits/sustained controls using unchanged `diag_transitions` over each full cached DEV song, up to three events per kind. Infer 22-second event excerpts with 10 seconds preceding scored context; record absolute eligible pre/post retention, ducking median/p10 and vocal leakage, not just ratios.
- Save each completed song's output/input hashes and rows for verified resume. A model exceeding PCM headroom causes an explicit execution/representation failure; do not tune gain after seeing it.

### Predeclared relative quality gate

This gate decides whether broad separation/protection jointly improved enough to preserve for the next mechanism study; it does **not** substitute for product acceptance:

1. Terminal DEV20s paired-song median SI-SDR gain ≥.5 dB, with 90% song-bootstrap interval lower bound >0 (2,000 resamples, seed 0).
2. Full60s eligible transmission nonworse; vocal, drum and bass median transmissions each ≤control +.01 absolute.
3. Entry ducking median and p10 each ≥control −.02; absolute pre/post eligible medians each ≥.95×control.
4. Fixed direct-C median rear RMS within .5 dB of the control. If this fails, report the fixed-gain results and level difference; no posthoc level-matched architecture claim.

Always publish measured source/instrument/entry/C deltas and failed gate names. A reconstruction-only win with failed protection prompts one matched source/loss mechanism experiment. A borderline result prompts a matched second seed rather than a success declaration. TRAIN gain without DEV gain prompts a coverage/generalization audit before compressing. Neither pattern alone diagnoses overfitting or data insufficiency.

### Unchanged product gates

For each C route record the seven source thresholds, any-stem swings ≤5, absolute median depth/level slopes ≤.05 and entry ≥.85/.60 median/p10. Retain keys ≥.15, with the owner's .12 alternative reported separately if considered; do not silently lower it. The original/direct route rear-level difference is also recorded using the existing half-dB renderer screen.

Even a DEV40 aggregate pass cannot qualify this PC reference: **≤150 M measured device int8 MAC/s**, **≤85 ms additional latency**, the **20 untouched instrument-covered songs**, Track A port coordination and the owner's blind live CarPlay verdict remain open. No Track A files or unit actions are part of this run.

```mermaid
flowchart LR
    accTitle: Broad causal reference comparison
    accDescr: Two completed fitting checkpoints train on the same 176 true-stem TRAIN songs for 40k steps, receive identical TRAIN and DEV monitoring and a CPU replay, then compare on frozen source, transition and real-C tests before considering compression.
    checkpoints["Both completed causal fitting checkpoints"] --> train["Matched 176-song true-stem training"]
    train --> curves["Fixed TRAIN12 and DEV40 curves"]
    curves --> audit["Terminal saved-checkpoint CPU audit"]
    audit --> harness["DEV40 source, entry and frozen C routes"]
    harness --> decision["Joint measured result before compression"]
```

## 🖥️ Reproducibility and execution state

| Artifact | Location |
| --- | --- |
| Frozen data/protocol manifest | [Exact PC manifest](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_manifest.json) |
| Trainer, sampler, crop monitor and CPU auditor | [train_causal_reference_generalization.py](../../../tools/stereo-upmix-render/train_causal_reference_generalization.py) |
| Terminal source/entry/C comparison | [eval_causal_reference_generalization.py](../../../tools/stereo-upmix-render/eval_causal_reference_generalization.py) |
| Exclusive supervised runner | [run_causal_reference_generalization.ps1](../../../tools/stereo-upmix-render/pc/run_causal_reference_generalization.ps1) |
| PC root | `D:\upmix\runs\causal_reference_generalization_v1` |
| Each training arm | `last.pt`, `stepN.pt`, `progress.json`, `eval_stepN.json`, `train_complete.json` |
| Terminal evidence | `audit.json`, `full40/`, `complete.json` |

Manifest SHA-256 **`d5583a4d4b01682dbdd9bc853a5595249c838e3589eb27b7f5f60178c691d9d6`**. Preparation on PC verified cache layout, metadata/split separation, checkpoint hashes and decoder/statistics parity, then froze evaluation crops. It did not decode Test, infer DEV or train.

Protocol commit **`e07027eb`** preceded GPU execution. First launch on 2026-09-30 completed the original 20-step smoke, but PowerShell `Start-Process` lost its exit code after `Refresh` and the supervisor stopped conservatively; no full run began, no Python or lease remained. A separate CPU exit probe reproduced a valid exit code 0 when the native process handle is retained. The supervisor now retains that handle, treats undefined exit codes as failures and reuses only completed, matching-protocol smoke markers before CPU auditing. Original smoke/checkpoint/logs are preserved; no training model, loss, schedule, manifest or acoustic gate changed. A read-only import preflight also initially omitted the tools working directory; corrected preflight passed CPU import and PowerShell parse without GPU execution.

Original-arm snapshot before the owner-requested reorder, **2026-09-30: both 20-step smokes and independent CPU replay passed; full original-arm training was active**. Corrected wrapper PID **10456**, Python launcher/worker **34632/29684** (one interpreter tree, not two GPU tenants), GUID lease owned by `causal_generalization`. Live supervisor log `C:\Users\LuKa\AppData\Local\Temp\upmix_causal_generalization_live_v2.log`; worker logs have distinct C: names recorded there. Rapid queue empty. Original progress export at **step 500**, 2,000 sampled windows, **all 176 TRAIN songs reached**, 86.54 s recorded, zero clipped steps; sampled training objective .195257, decoder gradient norm .078611. This is one stochastic training-batch loss, not a DEV improvement or an estimate of the final result. This was the initial sequence; the later priority amendment below supersedes its live-process/order details. Both terminal40k caps and the final CPU/full40/source-entry/C gates remain pending. The exclusive runner requires empty rapid queue and no Python/rapid daemon, exact source/manifest/model/C hashes, and acquires its own GUID lease. It uses the already existing isolated BSRNN venv with the unchanged shared torch stack; no installs. C: stdout/stderr logs and plain persistent cache reads avoid known WinBtrfs traps. A 20-minute log-silence watchdog terminates only its own launched worker tree and preserves artifacts; resumption is explicit and restores the same fixed schedule and checkpointed state. No automatic horizon extension or orphan lease cleanup.

## 🔀 Owner-requested execution order amendment, 2026-09-30

The owner asked to give the new architecture the GPU before completing the control. At the latest inspected control checkpoint, added step25k was active; its declared DEV SI-SDR still improved **−2.097 at10k → −1.747 at20k**. This is not evidence of a fully converged ceiling, and the remaining15k cannot be assumed to make no difference.

Change **execution order only**: preserve the control's last complete checkpoint, optimizer, sampling digest and original logs; run residual96×4 to40k first; then resume the control to its original40k cap before final CPU/source-entry/C decisions. Keep all model/data/statistics/loss/learning-rate/crop/gain/gate definitions and the original manifest unchanged. The existing20k snapshots provide an earlier common-age comparison for information; they do not select a new checkpoint or replace the terminal gate.

Before releasing its lease, validate ownership of the live original supervisor/worker tree, terminate only that tree, archive the last atomic checkpoint and verify it on CPU with [audit_causal_reference_priority_pause.py](../../../tools/stereo-upmix-render/audit_causal_reference_priority_pause.py). Strict checkpoint mode/protocol/stats, finite model/optimizer, fixed40k scheduler position, exposure counters and descriptor digest must pass. Interrupted work since the last250-step checkpoint may be replayed; the archived checkpoint is the authoritative effective training age, not a newer progress JSON. Keep the immutable pause archive and amendment record. The modified runner requires `-Resume -PrioritizeReference` and the verified pause evidence before starting the changed order.

Amendment/source freeze commit **`4e7c6913`** preceded the pause. The positively identified original supervisor10456 and child interpreter tree34632/29684 were terminated; no other Python work was active. The last complete checkpoint was **added step28,750**, 115,000 windows. CPU validation passed: strict original-mode/frozen-protocol/stats, finite weights and optimizer, exact saved scheduler position, exposure counts and sampling digest. The archived checkpoint is byte-identical to the resumable `original/last.pt`, SHA-256 **`0dd6f28ffb35579813c07643af6150a3f40247ee505da017a21dcd3b7b8b0c0e`**; sampled-descriptor digest **`3b10fb9573fb7221b816b3f0eaaba1b1c2a751282d9046f785df44f259175743`**. The original progress snapshot also ends28,750. The old lease was moved into this run's own pause archive after verification.

**Historical priority-training snapshot: residual96×4 was active**, captured through added **step29,000**, under the sole GPU lease. Priority wrapper20256, Python launcher/worker43460/51060 (one interpreter tree), C: supervisor log `upmix_causal_generalization_live_priority_v1.log`; new worker log names are recorded there. The original control is paused at28,750 with **11,250 steps remaining**. The priority runner automatically runs reference40k, resumes original40k using saved optimizer/history and the unchanged step-indexed sampler/schedule, then performs final CPU/full40 source-entry/C evaluation. The declared20k matched-age monitor comparison is now captured below; it is informational and does not replace terminal40k selection.

| Scheduling/evidence artifact | Repository capture |
| --- | --- |
| CPU pause verification and unchanged40k requirements | [Priority amendment](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_priority_amendment.json) |
| Owned stopped process tree and UTC timestamp | [Process stop record](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_priority_process_stop.json) |
| Complete control progress through28,750 | [Paused control progress](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_original_paused_progress.json) |
| Original declared10k monitor | [Control10k](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_original_step10000.json) |
| Original declared20k monitor | [Control20k](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_original_step20000.json) |
| New reference captured progress through1k | [Reference priority progress](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_residual_priority_progress.json) |

Archive on PC: `priority_pause/original_last.pt`, `original_progress_before_pause.json`, `original_supervisor.log`, `process_stop.json`, `old_gpu_lease.txt`. Pause-audit artifact SHA-256 **`7b76aa5a73d3d70d1fc97cd0211c81a836d3990838e87fa628148a6f01aa1251`**. Report the pause as owner steering and a resumable order change; do not call the28,750 control a completed40k model or evidence that future improvement is impossible. No model/data/loss/gain/threshold/manifest change, reboot, driver reset, unit action, extra GPU tenant or shared-package install.

## 📈 Informational matched20k result, 2026-09-30

Both arms now have the declared added20k snapshot on the identical frozen TRAIN12/DEV40 natural crops. Their sampling digest at20k is identical: `76d2ed4b6b34666ab2618f7c70562fc7f335d2db44be896dddafb9d0019366b3`, **80,000 windows each**. This is a comparison at equal training exposure, not equal compute. Latest reference progress capture is29,000/40,000, 116,000 windows, all176 TRAIN songs, zero clipped steps; control remains paused28,750. No final CPU/full60s/entry/C result exists yet.

| Frozen monitor metric at20k | Original | Residual96×4 |
| --- | ---: | ---: |
| TRAIN12 median SI-SDR, dB | −1.741768 | +0.103475 |
| DEV40 median SI-SDR, dB | −1.746900 | −0.082041 |
| DEV40 objective | .364772 | .330738 |
| DEV40 median eligible attributed transmission | .319297 | .430342 |
| DEV40 median vocal attributed transmission | .142786 | .102618 |
| DEV40 median drum attributed transmission | .023592 | .023434 |
| DEV40 median bass attributed transmission | .018522 | .021538 |
| DEV40 8–14k eligible transmission | .002888 | .006920 |
| DEV40 14–24k eligible transmission | .002256 | .003812 |

The difference of DEV medians is **+1.664859 dB**, objective decreases **9.330%**, eligible attributed energy rises .111046, vocal falls .040168, drums are nearly unchanged and bass increases .003017. These transmission statistics describe extraction under the actual mixture-inferred mask; **they are not rear shares**. The joint extraction direction is encouraging at20k, but upper-band transmission remains below1% of eligible source energy. This has not resolved treble loss or established rear stability, vocal-entry retention, level parity or product acceptance.

The DEV median difference at10k was +1.296771 dB; the reference's DEV median improves −.800233→−.082041 from10k→20k. Preserve both curves; do not declare a converged control, early-select the reference or infer the terminal paired-song bootstrap gate from differences of aggregate medians. Continue both original40k caps and the frozen final audit/C protocol.

| New exact PC evidence | Repository capture |
| --- | --- |
| Reference10k, all frozen song rows | [Reference10k](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_residual_step10000.json) |
| Reference20k, all frozen song rows | [Reference20k](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_residual_step20000.json) |
| Reference progress through29k, including matching20k sampler digest | [Reference29k progress](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_residual_progress_29000.json) |

SHA-256 respectively: `c51373ce91d19ddac6344c57040e4b3520a608ea7397681425eefcf1e38fcfa6`, `edaac6409b029be8f52c6e8c358cdd7b90c8eec2679f543a1bd843bd66f9ef75`, `da6c6e64ce94910b11d796f289454e47388a6ad77085a6f7e057fa2e52359f83`. Original20k and paused progress captures remain linked above. No source, initialization, data, loss, manifest, gain, horizon or gate changed for this update.

## 📈 Reference30k monitoring update, 2026-09-30

Latest live check: reference **37,250/40,000** remains active under the same owned supervisor/interpreter tree and lease; control stays paused28,750. Final CPU audit and full source-entry/C evaluation have not started.

The reference's declared DEV40 natural-crop monitor at30k has median SI-SDR **+.186781 dB**, versus −.082041 at20k (**+.268822 dB**). Objective .330738→.333745 is slightly worse; eligible/vocal/drum/bass attributed transmissions change **.430342/.102618/.023434/.021538 → .407860/.093647/.028673/.024243**. This is continued reconstruction improvement with mixed source/objective changes, not improvement on every criterion. Upper-band eligible transmission is **.010223 (8–14k) / .007967 (14–24k)** and remains weak.

Control has no30k snapshot yet, so this is a within-reference curve update, **not a matched30k architecture comparison**. Fixed terminal40k selection, learning schedule, gain, data and joint gates remain unchanged. Full per-song rows are preserved in [Reference30k](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_residual_step30000.json), SHA-256 `ca20d3d0ab31b6ee98fd290c4248b99efe976cfdd71edee93668e068c944437e`.

## 🏁 Both terminal40k checkpoints and CPU audit, 2026-09-30

Both arms have completed their original **40,000 added steps**. The control resumed its exact28,750 optimizer/history and reached40k; the reference also reached40k. The independent [terminal CPU audit](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_terminal_audit.json) **passed**, with the identical160k-window sampling digest `ec6c437bbb1d1b7c61d25a8abbda265abaf67ffe904dce5e1eda90dea75823e6`, unchanged protocol hash and strict saved-checkpoint replay. Prefix future-perturbation NSE is0 for both fixed-STFT neural networks; waveform streaming latency/device execution remain unqualified.

| Frozen DEV40 natural20s monitor at40k | Original | Residual96×4 |
| --- | ---: | ---: |
| Median SI-SDR, dB | −1.443339 | +.284302 |
| Objective | .356960 | .327978 |
| Eligible attributed transmission | .357370 | .417543 |
| Vocal attributed transmission | .156708 | .099341 |
| Drum attributed transmission | .027366 | .028529 |
| Bass attributed transmission | .022014 | .023086 |
| 8–14k eligible transmission | .004530 | .009331 |
| 14–24k eligible transmission | .002841 | .010302 |

Difference of aggregate SI-SDR medians is **+1.727641 dB**; objective decreases **8.119%**. More precisely, applying the unchanged evaluator's `paired_sdr` function to the exact exported40k song rows gives **median paired improvement +1.302827 dB**, 90% song-bootstrap interval **[+1.051355,+1.443251] dB**, and **39/40 songs nonworse** (2,000 resamples, seed0). The two predeclared natural20s reconstruction subgates pass. This does not decide the remaining full60s/source/entry/rear-level joint gates. Drums/bass slightly increase and upper-band transmission remains around1%.

Latest C evaluation check: control complete on all40 songs/231 stem rows plus transitions; reference **33/40 songs**, still running, its transitions and final decision pending. The [completed control full40](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_original_full40.json) remains below absolute acceptance: direct-C guitar/keys/piano/strings **.103295/.104612/.091219/.102565**, voice/bass/drums **.015153/.053625/.009577**, **0/40 swings**, but instrument and vocal-entry thresholds fail, and direct-C rear level is **−.645444 dB** versus its original C route. These are control results, not the candidate's final outcome. Do not infer a final reference C win from its improved20s extraction monitor.

Terminal model SHA-256: original `ba984a6e680b22ac6dc6b3d113ed79adbc007f5e2523f227e04ee85755d4fc7d`; reference `3a0400e912b9a855fb40307ad12c2bfbb81e496d5c1072dbb44d17c9912a9d1b`. Evaluator interpreter tree3184/44452 under priority supervisor20256 and the same owned lease; C: evaluator stdout `upmix_causal_generalization_7f92a1b2-4740-4fe7-8b27-94e182038f3a.stdout.log`. No new training job, source/loss/data/gain/gate change or hook/unit action.

| Exact terminal evidence | Repository capture |
| --- | --- |
| Original30k curve snapshot | [Original30k](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_original_step30000.json) |
| Original40k song rows | [Original40k](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_original_step40000.json) |
| Reference40k song rows | [Reference40k](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_residual_step40000.json) |
| Strict CPU audit and common sampler digest | [Terminal audit](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_terminal_audit.json) |
| Completed control C/source/transition results | [Control full40](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_original_full40.json) |

Exact artifact SHA-256 in table order: `7815b8237b203de7412daec4f003674a8fea02789ee4a2b7ff2d54a5a92b892c`, `35fc39aef7e7094d681f8a1e09d154d29e5a4f4c678d9727c289ae4852b561c1`, `fcda95347b87d6ce696363bedbb4c0eed3a323ac3e301b76592db147def8adf6`, `68d7cd69fa04933dc083d39f90ffe1282f251c0dbd3f9b02534452fb06069e42`, `bfbc70e357aa913854d84df9b422609c69800546bcc0adf3901489f0b832136a`. Earlier active-training/pause snapshots below and above this section are historical.

## 🧪 Completed smoke evidence and live snapshot

Both smokes used **the identical 80 sampled windows**, reaching 59 distinct TRAIN songs, with sequence SHA-256 `0d48421888b525c7a4c387957ac2e61f426edfa60c20fe581111f16621daac4c`. Every source/offset/gain descriptor matched. Decoder gradient norm .079162 (original) / .074431 (reference), zero clipped steps; saved decoder tensors changed in both arms. CPU reconstruction replay and source-coverage checks passed, future-prefix NSE **0** in both trained networks.

Largest CPU/GPU absolute discrepancies across both replay crops/arms: objective **3.109e−5**, SI-SDR **1.287e−5 dB**, transmission **6.557e−7**, below the frozen tolerances. These are execution checks, not an acoustic promotion: 20 steps on a broad pool do not assess learned generalization.

| Captured evidence | Repository copy |
| --- | --- |
| Independent CPU smoke audit, exact matched sequence | [Smoke audit](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_smoke_audit.json) |
| Original smoke: two fresh TRAIN crops | [Original smoke](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_original_smoke.json) |
| Reference smoke: same crops | [Reference smoke](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_residual_smoke.json) |
| Original checkpoint before broad training, TRAIN12/DEV40 | [Original initial monitor](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_original_initial.json) |
| Captured original-arm progress through step 500 | [Original live snapshot](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_original_progress.json) |

### Early control monitoring; reference arm pending

The original control's first declared broad-training snapshot is now captured. This establishes that a positive four-crop fit does not mean useful unseen-song separation at initialization. It does not compare the two broad-trained architectures or justify an early stopping decision.

| Original control monitor | Before broad training | Added step 1,000 |
| --- | ---: | ---: |
| Fresh TRAIN12 median SI-SDR, dB | −7.053622 | −3.590665 |
| DEV40 median SI-SDR, dB | −5.285403 | −2.942938 |

[Exact original step-1k metrics](UPMIX_CAUSAL_GENERALIZATION_2026-09-29_original_step1000.json) retain every song/source/band row. DEV 8–14 kHz eligible transmission is still .000910 at this early control age. The matched reference's broad results are pending, and the full40k schedule/selection/gates remain unchanged.

CPU smoke audit SHA-256 `e36f98e78c9fe50b4add923f88dc2d49d6528e7d2ec38e98fe68ef9fd5ace581`. Exports retain captured PC bytes under the narrow attributes rule. The initial monitor and training snapshot are recorded without selecting checkpoints or changing the protocol. Fetch final curves/results separately when terminal markers exist; a captured live snapshot is not automatically current.

Never reboot/reset the PC, GPU driver, head unit or j9. English docs, own paths only, no Co-Authored-By, no credentials or private dataset links in output.
