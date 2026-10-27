# Causal reference encoder: matched fixed-decoder experiment

_2026-09-29. Track B. Owner authorized the causal-student sequence after the completed SIMO check. Protocol frozen before training._

---

## 🎯 Question and scope

The [pretrained SIMO reference](UPMIX_BSRNN_REAL_C_CHECK_2026-09-29.md) passes the observed offline DEV40 source/entry/real-C gates, but has 108.7 M parameters and unrestricted future within chunks. The next question is whether a stronger **causal encoder** can overcome the current student's fitting gap before compression.

This implements the [research review's first controlled experiment](UPMIX_RESEARCH_REVIEW_2026-09-29.md#proposed-execution-protocol): four exact TRAIN crops, the original trained decoder, and unchanged true-audio supervision. SIMO stays frozen as the quality reference. This first pair does not imitate its output or internal coordinates, change datasets, strengthen the treble loss, run DEV, or compress. Those interventions require a successful causal mechanism to preserve. The failed [raw-state supervision](UPMIX_DECODER_STATE_SUPERVISION_2026-09-29.md) remains evidence against that particular shortcut.

## 🏗️ Architecture and cost

| Component | Fresh original encoder | Normalized residual reference |
| --- | --- | --- |
| Features and bands | Original complex quadratures + standardized log magnitudes, 48 original bands | Identical features and band geometry |
| Level normalization | Stereo-linked causal 2 s EMA | Identical EMA |
| Band input | Linear → tanh, 48 features | Frame-local LayerNorm over each band's flattened features → linear, 96 features |
| Time processing | Original two unidirectional GRUs | Four blocks: feature LayerNorm → unidirectional 96-unit GRU → projection → residual |
| Frequency processing | Original two local mixers | Each block then uses feature LayerNorm → band-bidirectional GRU (48 units/direction) → projection → residual |
| Output interface | Original 32 local + 32 global coordinates | Shared linear 96→32 local projection; original global GRU 32→32 |
| Global-GRU initialization | Seed 1, original construction | Bit-identical initial state to control, verified by hash |
| Decoder | Original trained affine complex stereo-mask head | Same exact tensors, frozen in both arms |
| Total / trainable parameters | 586,916 / 320,416 | 1,350,960 / 1,084,460 |
| Estimated neural MAC/s | 125.139 M | **2,167.950 M** |

MAC estimates use the original multiply-accumulate convention for linear, convolution and recurrent matrices. Biases, normalization, activations, FFTs, memory traffic and renderer are excluded; these are not measured device costs. The reference is approximately 14.45× the 150 M device allowance. It is a PC diagnostic that asks whether useful causal behavior exists in this encoder package; it is not a deployment candidate or authorization to increase the car budget.

Normalization spans features within one frame, never the time axis. Bidirectionality is over frequency bands at the same frame. The fixed-frame network's future-perturbation test is required before and after training. The centered host STFT/iSTFT and whole-array convenience path do not certify stateful streaming or total chain latency.

```mermaid
flowchart LR
    accTitle: Matched causal encoder experiment
    accDescr: Two fresh encoders receive the same four TRAIN mixtures and use identical frozen trained decoders; the same true-audio objective and saved-checkpoint audit decide whether the stronger causal mechanism deserves further training.
    truth["Four frozen TRAIN stems"] --> mix["Same stereo mixtures and STFT"]
    mix --> control["Fresh original encoder"]
    mix --> reference["Normalized residual causal encoder"]
    control --> head_a["Frozen original decoder"]
    reference --> head_b["Bit-identical frozen decoder"]
    head_a --> audio["Same true-audio objective"]
    head_b --> audio
    truth --> audio
    audio --> audit["CPU replay, causality and joint fitting gate"]
```

## 🔬 Frozen training protocol

- Input: exactly the four 8-second MoisesDB TRAIN crops and int16 hashes in `separator_fit_probe_v1/manifest.json`; split membership/hash must match. Eligible = slot 6; vocal = 0+1, drums/percussion = 2+3+4, bass = 5. No remix or pseudo labels in this experiment.
- Trained decoder comes from the original 10k checkpoint, SHA-256 `0e560b9924feea88f3e61b97253734c60a02a3901c7636779e107653d7e77e41`. Strict replay must reproduce its saved acoustic objective within `1e−5` and per-crop SI-SDR within `.001 dB` before either fresh encoder trains.
- Both encoders start fresh from seed 1. The original global-GRU initialization is exactly common. Copy only the trained decoder; mark every decoder parameter `requires_grad=False` and check its hash at every evaluation/save. No trained encoder warm start.
- Both use fresh AdamW, constant learning rate `3e−4`, weight decay `1e−4`, gradient clipping 5, float32 with TF32 disabled, and **exactly 10k steps**. No early acoustic stopping or extension. One whole four-crop batch per step; evaluate every 250 steps, preserving 1k/4k/10k results and all intermediate curves.
- Objective is unchanged: original linear magnitude/complex STFT loss + original multiresolution magnitude audio loss. Saved training statistics are fixed constants, equal in both arms. No source-loss coefficient or high-frequency weighting is selected after results.
- Record waveform SI-SDR/NSE, actual objective, attributed eligible/vocal/drum/bass transmissions, reconstruction/transmission in 0–300 Hz / 300 Hz–8 kHz / 8–14 kHz / 14–24 kHz, gradient/clipping counts, control input-tanh saturation by band, nominal cost, checkpoint/source/head hashes and elapsed time.

This is a package comparison with different compute and capacity, not an isolated normalization ablation or equal-compute contest. Equal steps do not establish that either architecture has converged.

## ✅ Predeclared diagnostic gate

The 10k reference must jointly satisfy the following against **both** the fresh matched frozen-head control and the historical jointly trained 10k model:

1. Actual audio objective ≤90% of each control.
2. Median waveform SI-SDR ≥1 dB above each control.
3. At least three of four crops nonworse in SI-SDR against each control.
4. No crop worse by more than 1 dB against either control.

Historical measurements are replayed from the exact checkpoint; previous report values are approximately `.055586` objective / `7.797 dB` median. Decoder parity and network causality must also pass. A 20-step smoke cannot earn promotion. Source and band metrics describe the mechanism; this fitting gate is not a relaxed product gate.

If the package fails, stop this recipe and inspect optimization/interface evidence. Failure does not rule out all causal separation or all normalization. If it passes, preserve the demonstrated mechanism for broader causal training, true-stem-anchored output distillation and later compression; four-crop fitting alone does not authorize those later product claims.

## 🧪 Preflight, execution and verification

CPU construction check passed before protocol freeze: parameter/MAC totals above, identical initial global-GRU SHA-256 `238ae28f00c77aa22fb41ccbbdd3c560b42ff5fe0f89ecc2c25856d754983282`, future-perturbation prefix NSE **0** in both networks with changed future outputs. This is a synthetic fixed-STFT test, not a trained audio result.

The runner requires no live Python/rapid daemon, empty GPU queue, exact source and anchor hashes, and a new output directory. It takes one exclusive GPU lease; executes a **20-step smoke of both arms**, then the full 10k pair sequentially; then runs the independent saved-checkpoint audit on CPU. Unique C: logs avoid the previous shared D: log-lock failure. No environment install or head-unit operation.

The CPU auditor verifies checkpoint/head/source/window hashes and semantics, parameter/nominal-cost counts, finite tensors, future-frame causality, and recomputes the acoustic measurements and all gates. Declared CPU/GPU replay tolerances: objective `1e−4`, per-crop SI-SDR `.05 dB`, source transmission `.005` absolute; promotion booleans must agree. Numerical parity is separate from acoustic success.

| Artifact | Location |
| --- | --- |
| Reference model | [separator_causal_reference.py](../../../tools/stereo-upmix-render/separator_causal_reference.py) |
| Paired experiment | [separator_causal_reference_probe.py](../../../tools/stereo-upmix-render/separator_causal_reference_probe.py) |
| CPU auditor | [audit_separator_causal_reference.py](../../../tools/stereo-upmix-render/audit_separator_causal_reference.py) |
| Exclusive runner | [run_separator_causal_reference.ps1](../../../tools/stereo-upmix-render/pc/run_separator_causal_reference.ps1) |
| PC smoke / full root | `D:\upmix\runs\separator_causal_reference_smoke_v1` / `separator_causal_reference_v1` |
| Each arm | `architecture.json`, `progress.json`, `fit.pt`, `complete.json` |
| Pair | `manifest.json`, `complete.json`, `audit.json` |

Source SHA-256: model `764a594242d26e7f75884d122b91656adc3037b53a7c54f602b74678f58a07d0`; probe `e514fff84f29b0cc52236b6f4a2095cccd79b5c1777a77eb5f29a60347f37ccc`; auditor `2f21ef3c14a09acc6e643c784a64fe384917c37801101dacca08f26fce7cf2e7`; runner `ec5296aa0741eafdbd4451b39e25c02fe5d01b0831ca6cf5a56e0c7443ea3928`. Sources, crop hashes and all measured architecture details are saved again in the run manifest.

### Undefined-source audit correction

During training, a read-only progress summary encountered an undefined transmission for an absent bass source. The training metric correctly returns `null` below its fixed energy floor. This exposed that the not-yet-executed CPU auditor would attempt subtraction on an array containing `None`. Before final auditing, its comparison was corrected to require **identical undefined positions** and compare only valid numeric pairs. No missing source is assigned zero, and no training model, loss, target, checkpoint or gate changed. Both training arms completed in the original process; this was not a failed acoustic run.

Corrected auditor SHA-256 `1765c9407c850ef640707f54bf1327c0ef1aa2cb71d9a691d0dde0ac703add3f`; runner hash table updated for future launches, SHA-256 `2825c628d4c74ae5c66e86268d0c915d86eee29ffcfc96115f48e1bc2905e267`. The already active PowerShell run had passed the original preflight; it invoked the corrected auditor after training. The final audit records its own executed source hash. The model/probe fingerprints above remain unchanged.

## 📊 Completed 10k result

Protocol/source commit **`de861b23`** preceded training. Both 20-step smoke arms, both full 10k arms and the independent CPU checkpoint audit completed. **All eight predeclared fitting comparisons passed**, with decoder parity and fixed-STFT future-prefix NSE **0** before and after training. This supports further causal training; it does not satisfy the product gates.

| Metric | Fresh original, frozen decoder | Residual reference, same frozen decoder | Historical joint 10k, replayed |
| --- | ---: | ---: | ---: |
| Actual audio objective | .05706710 | **.04532031** | .05558142 |
| Median waveform SI-SDR, dB | 7.754112 | **10.499900** | 7.796632 |
| Full training elapsed, seconds | 409.323 | 1,013.072 | Historical run |

Against the matched fresh control, the reference reduced objective **20.584%** and improved median SI-SDR **2.745788 dB**. Against the historical jointly trained model, these were **18.461%** and **2.703268 dB**. The historical checkpoint was replayed with TF32 disabled; its small difference from the earlier rounded report is within the frozen replay tolerance.

### Per-crop reconstruction

All four crops improved against both controls. IDs and exact 8-second offsets, cache hashes and TRAIN membership are in the [run manifest](UPMIX_CAUSAL_REFERENCE_ENCODER_2026-09-29_manifest.json).

| Frozen TRAIN crop ID | Fresh original SI-SDR, dB | Reference SI-SDR, dB | Historical SI-SDR, dB | Reference − fresh, dB |
| --- | ---: | ---: | ---: | ---: |
| `f4b735de-14b1-4091-a9ba-c8b30c0740a7` | 9.444428 | 12.372866 | 9.599886 | +2.928437 |
| `1fc37390-1769-452d-9bea-19025be4c467` | 12.243742 | 14.355640 | 12.315094 | +2.111898 |
| `ad6c3742-e517-42eb-9dec-e74ed05387cc` | 3.590248 | 6.811723 | 3.642384 | +3.221476 |
| `fd6e4b4a-f33a-4f3c-aa6e-7c65fd5dc0bc` | 6.063796 | 8.626934 | 5.993379 | +2.563138 |

Waveform NSE in the same order changed **.102247/.058664/.288845/.195928 → .054905/.038838/.161112/.117979**. These are repeated fits to known training crops, not unseen-song performance.

### Learning curves and optimization evidence

| Step | Original objective | Original median SI-SDR, dB | Reference objective | Reference median SI-SDR, dB |
| --- | ---: | ---: | ---: | ---: |
| 1,000 | .06568507 | 6.582145 | .06064538 | 7.472159 |
| 4,000 | .05924069 | 7.415839 | .05100829 | 9.345152 |
| 10,000 | .05706710 | 7.754112 | .04532031 | 10.499900 |

Both runs have all 41 scheduled evaluations, steps 0–10k at intervals of 250, in the raw progress exports below. No clipped optimization steps occurred. Final gradient norm was .120890 for the control and .170040 for the reference. The reference continued improving through 10k; this experiment does not measure its converged ceiling.

The control's median input-tanh saturation fraction (absolute activation >.95) across the first 11 bands rose **.421840→.499806**; across all bands **.036743→.115714**, with final maximum band .836995. The reference omits that input tanh. This observation does not isolate saturation as the cause: width, depth, residual processing, frequency recurrence and local interface also changed together.

### Remaining source and frequency problems

| Median attributed source transmission | Fresh original | Reference |
| --- | ---: | ---: |
| Eligible instruments | .821695 | .880152 |
| Vocal | .063274 | **.076691** |
| Drums/percussion | .015109 | .013498 |
| Bass | .074805 | **.090090** |

One crop has no measurable bass; its transmission stays undefined and is excluded from the median. **Vocal and bass contributions increased**, despite better summed reconstruction. These frozen-mask attributions can cancel in the summed waveform; they are neither a rear-share measurement nor proof of audible contamination. They nevertheless prevent treating reconstruction success as a demonstrated selectivity improvement. Vocal-entry, natural-song source attribution and real-C output must be tested together.

| STFT band | Original error NSE | Reference error NSE | Original eligible transmission | Reference eligible transmission |
| --- | ---: | ---: | ---: | ---: |
| 0–300 Hz | .284928 | .172315 | .748765 | .870629 |
| 300 Hz–8 kHz | .121665 | .096453 | .893167 | .950792 |
| 8–14 kHz | .400355 | .381964 | .560895 | .578638 |
| 14–24 kHz | .535331 | .509892 | .445206 | .490618 |

Most improvement is in the low/mid bands. High-frequency transmission improves modestly and **treble loss remains unresolved**. These TRAIN-crop STFT measurements differ from the SIMO full40 waveform protocol; they cannot establish a treble advantage over the frozen SIMO reference.

## 🔎 Saved-checkpoint audit and evidence

The CPU auditor strictly loaded both saved checkpoints, verified finite tensors and identical frozen decoder SHA-256 **`d04d9fd5a73ebb3788240772b3be0fb1ecf420e059ddfc2a0bf5aeff1f4fec05`**, and reproduced all eight gate decisions. Prefix future-perturbation NSE remained **0** in both trained networks on CPU. This tests the network's fixed-frame temporal causality, not complete streaming STFT state or measured latency.

| CPU versus GPU discrepancy | Original | Reference | Frozen tolerance |
| --- | ---: | ---: | ---: |
| Objective, absolute | 6.333e−7 | 1.438e−6 | 1e−4 |
| Maximum crop SI-SDR, dB | 7.629e−6 | 1.907e−6 | .05 |
| Maximum valid source transmission | 5.960e−7 | 2.384e−7 | .005 |

| Exact captured PC artifact | Repository copy |
| --- | --- |
| Full completed pair and all gate decisions | [Complete](UPMIX_CAUSAL_REFERENCE_ENCODER_2026-09-29.json) |
| Sources, architecture, frozen crop and split hashes | [Manifest](UPMIX_CAUSAL_REFERENCE_ENCODER_2026-09-29_manifest.json) |
| Independent CPU saved-checkpoint verification | [Audit](UPMIX_CAUSAL_REFERENCE_ENCODER_2026-09-29_audit.json) |
| All original-arm evaluations | [Original progress](UPMIX_CAUSAL_REFERENCE_ENCODER_2026-09-29_original_progress.json) |
| All reference-arm evaluations | [Reference progress](UPMIX_CAUSAL_REFERENCE_ENCODER_2026-09-29_residual_progress.json) |
| Both 20-step smokes; no acoustic promotion | [Smoke](UPMIX_CAUSAL_REFERENCE_ENCODER_2026-09-29_smoke.json) |

Exports preserve the PC bytes, including CRLF. Complete SHA-256 **`5125791f3226d6e41a315276b0372df39c2f2d1ea8c8a2d0c0fab872ecc5626c`**; audit SHA-256 **`97edcb233c3d59cb6d31aa967806babffbee6b133d4781febce1f8065bbad9b3`**. The audit identifies the corrected auditor source recorded above. Model/probe hashes did not change during training.

Checkpoints remain under `D:\upmix\runs\separator_causal_reference_v1\original\fit.pt` (SHA-256 `7d3d1fde1f8bd2c64bd539292a7cb0eac4bce19b8377b06f7c22a32ed4b4e470`) and `residual96x4\fit.pt` (`682c09f1d253fb694bfde6fa43e1bfd1168f9161398a3abd969b0ad7fec1603a`). They require the reference loader and original feature/normalization semantics; do not load them permissively through a different model class.

## 🧭 Decision and next gates

**Preserve this causal encoder package as a successful fitting reference.** A frozen decoder that previously admitted much better privileged states now receives substantially better states from an actual causal encoder on all four crops. The gain is measurable without new data, teacher imitation or a new audio loss. It does not identify which architectural change caused it, prove generalization or validate compression.

Next prepare broader true-stem training and source-specific TRAIN/DEV comparisons with frozen duration, sampling and held-out splits. Use truth as the primary target; any SIMO output distillation is a separate, audited intervention. Decide protected-source and high-frequency loss changes through matched comparisons rather than tuning them into this completed result. Require natural-song source/entry and real-C success before choosing what to preserve through compression. The 2.168 G estimated neural MAC/s reference is about 14.45× the allowance; this run does not establish that its gain survives the required reduction.

Full DEV40, 20 untouched instrument-covered songs, ≤150 M **measured device int8** MAC/s, ≤85 ms additional chain latency and owner blind live CarPlay remain open for a causal candidate. The prior SIMO full40 result stays a separate noncausal quality reference. No causal prototype was ported or heard in the car.

Terminal PC readback: **zero active Python, GPU lease absent, rapid queue empty**. Full pair and CPU audit markers are complete; no next training is queued. Track A's files, others' uncommitted files, the head unit and shared packages were untouched. Never reboot or restart the unit/j9; no Co-Authored-By; no credentials or private dataset link in output.
