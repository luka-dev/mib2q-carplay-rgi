# BSRNN pretrained reference inference — 2026-09-29

## Scope

Owner-approved item 1: run the downloaded upstream pretrained weights without fine-tuning on the **four exact TRAIN crops** from `separator_fit_probe_v1/manifest.json`. This uses the unmodified [pinned replication implementation](https://github.com/magronp/bsrnn/tree/94adc8c184ac9b1d2e496fcb6e78cfe3fb78fc9c), both BSRNN-large and optimized SIMO from [the publisher's record](https://zenodo.org/records/17516442). Provenance and licenses are in the [intake report](UPMIX_BSRNN_REFERENCE_INTAKE_2026-09-29.md).

No fine-tuning or new encoder is part of this run. This is a noncausal PC reference; its pretrained corpus, future context, model size and output head differ from our causal student. A successful run establishes an executable reference, not why the causal student fails or whether compression will meet the Audi budget.

## Frozen input and source semantics

| TRAIN song UUID | Artist | Start, seconds | Duration |
| --- | --- | ---: | ---: |
| `f4b735de-14b1-4091-a9ba-c8b30c0740a7` | Drawing Hands | 25.545438 | 8 s |
| `1fc37390-1769-452d-9bea-19025be4c467` | Firefly | 18.409083 | 8 s |
| `ad6c3742-e517-42eb-9dec-e74ed05387cc` | Firefly | 19.355354 | 8 s |
| `fd6e4b4a-f33a-4f3c-aa6e-7c65fd5dc0bc` | Firefly | 19.181813 | 8 s |

The runner verifies the frozen split hash, every UUID's TRAIN membership, exact start/sample count and each selected int16 crop hash. Mixture is the exact sum of the seven float-converted cache slots, without augmentation. Source mapping for this diagnostic is **vocals = 0+1, bass = 5, drums = 2+3+4, other = 6**. Thus BSRNN `other` corresponds to this fitting target; it is not a synonym for every rear-eligible role in every renderer experiment. Instrument families remain combined.

48 kHz mixture is resampled once to 44.1 kHz. Each crop is peak-normalized once for inference; the same scalar is restored to every predicted source before resampling to 48 kHz. No source gain is fitted, no limiter is applied, and no source is replaced by mixture subtraction. Forward processes each complete 8-second crop with no OLA. This is the upstream fader's whole-crop branch for an excerpt shorter than its 11-second segment.

## Loading and runtime

- Actual upstream `BSRNN` and its real Lightning base, transforms, band split, recurrent blocks and heads are imported directly. No model reimplementation or stub is substituted.
- Architecture arguments come from each checkpoint's saved hyperparameters. The current evaluation YAML supplies `sdr_win/sdr_hop` missing in older checkpoint metadata; this changes no architecture or inference parameter.
- Each checkpoint is required, size/SHA-256 verified and its nondefault pickle globals screened against the observed OmegaConf/builtin set. Public Lightning bundles contain optimizer `defaultdict` objects rejected by PyTorch 2.11's weights-only unpickler even when allowlisted. The verified publisher artifacts therefore use explicit `weights_only=False`; this is not a general untrusted-checkpoint loader. Optimizer state is discarded, never executed.
- `load_state_dict(strict=True)` must succeed with zero missing/unexpected keys. Every loaded state tensor is compared exactly against the checkpoint. All tensors must be finite. Parameters are frozen and inference uses `torch.inference_mode()`.
- Runtime: isolated `D:\upmix\venvs\bsrnn_reference`, created with `--system-site-packages`. Installs went only into that venv while the PC was idle. Shared torch/torchaudio remain `2.11.0+cu128`; upstream's 2.7 downgrade was not performed. Lightning and pytorch-lightning 2.5.5, OmegaConf 2.3.0, pandas 3.0.6, museval 0.4.1, musdb 0.4.3. The run records package versions, executable, torch location and a full runtime freeze.
- One exclusive `GPU_LEASE`, no rapid queue or daemon. Runner removes only its token's lease in `finally`. The RTX 5080 is a host reference; its forward timing is not a Krait int8 MAC/s or latency measurement.

## Reproducible tools and output

| Artifact | Location |
| --- | --- |
| Strict inference script | [bsrnn_reference_infer.py](../../tools/stereo-upmix-render/bsrnn_reference_infer.py) |
| Exclusive GPU runner | [run_bsrnn_reference.ps1](../../tools/stereo-upmix-render/pc/run_bsrnn_reference.ps1) |
| Isolated dependency recipe | [bsrnn_reference_requirements.txt](../../tools/stereo-upmix-render/pc/bsrnn_reference_requirements.txt) |
| PC result root | `D:\upmix\runs\bsrnn_pretrained_train4_v1` |
| PC progress / final audit | `progress.json` / `complete.json` under that root |
| PC execution log | `D:\upmix\runs\bsrnn_pretrained_train4_v1.log` |

For each UUID folder: `mixture.wav`, four `truth_<source>.wav`, four `simo-bsrnn-opt/<source>.wav`, four `bsrnn-large/<source>.wav`. All are 48 kHz stereo float32 WAV; the wrapper checks finite values, exact length, peaks and bit-exact float WAV readback. Values above unity, if any, are reported rather than clipped. The summary includes hashes for every WAV.

## Setup failures and recovery

1. Initial CPU import failed because `musdb` imports `stempeg`, which requires `ffmpeg` and `ffprobe` on PATH even though inference reads cached arrays. Existing `D:\upmix\tools\ffmpeg\bin` was added to the runner's process PATH. No new FFmpeg installation or upstream change.
2. First runner failed before checkpoint loading: the wrapper incorrectly treated the split's UUID-to-partition mapping as partition-to-UUID lists. Corrected to filter UUIDs marked `train`; frozen split/crop hashes did not change. Empty output and log preserved as `bsrnn_pretrained_train4_v1_aborted_split` and its `.log`.
3. Second runner saved inputs but failed before any estimated stem at the weights-only `defaultdict` limitation described above. Preserved as `bsrnn_pretrained_train4_v1_aborted_weights_only` and its `.log`. The successful run must have its own final completion marker; neither interrupted attempt is an audio result.

## Result and next step

**Completed successfully:** five strictly loaded checkpoints, 20 forward calls, 32 predicted source WAVs, 16 truth WAVs and four mixture WAVs. Every WAV is finite, exactly 384,000 stereo frames at 48 kHz, and has bit-exact float32 readback. Maximum prediction peak **0.468938**, samples above unity **0**. Maximum allocated GPU memory **2.079 GB**. No trainable parameter was updated.

| Model | Parameters | State tensors | Missing / unexpected; tensor parity |
| --- | ---: | ---: | --- |
| SIMO optimized | 108,727,540 | 2,870 | 0 / 0; exact |
| Large bass | 32,797,192 | 650 | 0 / 0; exact |
| Large drums | 41,024,392 | 950 | 0 / 0; exact |
| Large other | 36,417,160 | 782 | 0 / 0; exact |
| Large vocals | 36,417,160 | 782 | 0 / 0; exact |

These are millions of parameters, not neural MAC/s measurements. The four large models total **146,655,904 parameters**; SIMO has **108,727,540**. Neither reference is a device-qualified candidate.

### Descriptive reconstruction of `other`

SI-SDR uses zero-mean signals, per-channel projection and channel mean, matching the earlier fitting diagnostic's convention. nSDR is fixed-gain stereo reference/error energy; no gain or distortion filter is optimized. These four TRAIN excerpts are a loader/acoustic sanity check, not a held-out score, source-leakage gate or a controlled architecture comparison. The publisher trained on MUSDB18-HQ; this turn did not establish corpus independence from these MoisesDB songs.

| Song prefix | SIMO SI-SDR, dB | Large SI-SDR, dB | SIMO nSDR, dB | Large nSDR, dB |
| --- | ---: | ---: | ---: | ---: |
| `f4b735de` | 12.024 | 7.976 | 12.286 | 8.576 |
| `1fc37390` | 19.190 | 14.362 | 19.160 | 14.526 |
| `ad6c3742` | 8.082 | 3.486 | 8.996 | 5.163 |
| `fd6e4b4a` | 10.002 | 6.951 | 10.499 | 7.830 |
| **Four-song median** | **11.013** | **7.463** | **11.392** | **8.203** |

The earlier 10k small causal fitting baseline was **7.797 dB** median SI-SDR on the same crop bytes: SIMO is **+3.216 dB**, while the large single-source bundle is **−0.333 dB**. This historical comparison changes pretraining, capacity, future context, normalization and head simultaneously. It establishes that the executable optimized offline reference recovers this target better here; it does **not** identify which ingredient causes the gain or prove that the gain survives causality/compression. No listening verdict was collected.

Full results, saved hyperparameters, tensor counts, checkpoint/WAV hashes and exact runtime: [machine-readable completed run](UPMIX_BSRNN_PRETRAINED_INFERENCE_2026-09-29.json). Script SHA-256: `1fc41e4451e212fa0ff662d699b000ff8080e06dd9b7956926b2e130e4aaa30b`.

### Next

Use SIMO as the stronger offline reference for a predeclared same-input comparison of instrument reconstruction, protected-source leakage and vocal-entry stability, then the real C renderer at fixed gain. Four short TRAIN crops cannot establish stable rear placement. The proposed causal normalized residual encoder remains unimplemented; adapt/test its useful mechanisms with a matched control before compression. The 20 untouched DEV songs and DSD Test stayed unread in this item. No new training, renderer/Track A change or head-unit action occurred.

Terminal readback: **no GPU lease, rapid queue empty, zero Python processes**, upstream checkout clean, runtime freeze present and 52 WAVs. Shared Python still lacks the newly installed reference packages; torch/torchaudio stayed `2.11.0+cu128`. Completed item 1; full source/entry/C/device/independent-song/owner-ear gates remain open.


## Completed follow-up

The subsequent [full40 source/entry/real-C check](UPMIX_BSRNN_REAL_C_CHECK_2026-09-29.md) completed: SIMO with the frozen direct route passes all observed offline gates (0/40 swings; entry retention .875674/.745983 median/p10). The original route still gives 11/40 swings. That report contains the full fixed-gain CSVs, attribution, per-song failures and recovery history. This supersedes the pending follow-up above; causality, device, independent data and owner ears remain open.
