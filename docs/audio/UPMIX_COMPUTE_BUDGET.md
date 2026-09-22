# Upmix Compute Budget

*What a candidate upmix model may cost on the head unit's Krait core, how that was measured, and what the training machines could do — the arithmetic that admitted HSSR and blocked CUpGAN, H1 and Q2.*

---

## 📋 The short answer

On the APQ8064 Krait path the int8 HSSR controller costs 449,280 MAC per control (42.1 M MAC/s) and the whole NEON HSSR upmix measured **4.28 ms mean / 9.07 ms max per 21.33 ms period** in isolation — but under contention its P99 reached 21.15 ms, the whole period. There is **no measured free 75–80% budget**. The audit's working ceiling for a new model is about **200 M MAC/s int8 (≈100 M float32)** on an async worker, not in the packet path. Parameter count is not a cost model. Runtime deadline policy for the shipped path is in [Stereo to 5.1 Upmix](STEREO_TO_5_1_UPMIX.md).

## ⚙️ Units and periods

At 48 kHz a *PCM frame* is one sample of every channel, not an STFT frame; the benchmark call is 1,024 PCM frames = 21.333 ms.

| Work item | FFT / hop | Calls per second | Per 1,024 PCM frames |
|---|---|---:|---:|
| HSSR analysis/synthesis, G9 | 512 / 128 | 375 | 8 hops |
| HSSR neural controller | every 4th hop | 93.75 | 2 controls |
| P1 / Q2 full-bin models | 1024 / 256 | 187.5 | 4 hops |
| Upmix block benchmark | ≤ 1024 frames | 46.875 | 1 call |

An every-hop worker needs sustained throughput of 2.667 ms per hop128 or 5.333 ms per hop256. Buffering moves a deadline and adds latency; it never relaxes throughput.

## 📊 Measured Krait wall time

GCC 8.5 QNX standalone runs over SSH on the unit, 512 calls × 1,024 frames, clock quantum ~1.007 ms (2026-09-05, `tools/stereo-upmix-render/bench/results/2026-09-05-hssr-*.md`) [car]:

| Profile | Mean ms | P95 | P99 | Max | Period share |
|---|---:|---:|---:|---:|---:|
| music-stage, no network | 1.81 | 3.02 | 4.03 | 6.04 | 8.5% |
| HSSR-NEON | 4.28 | 6.04 | 8.06 | 9.07 | 20.1% |
| HSSR-active | 4.49 | 7.05 | 9.07 | 11.08 | — |
| HSSR-object | 5.02 | 7.05 | 9.07 | 13.09 | 23.5% |
| HSSR-object, loaded repeat | 6.00 | 12.09 | 21.15 | — | ~28% |

HSSR added ~2.47 ms to music-stage and a 75 s live trace ran with no underrun. The load composition of the loaded run is not reproducible — treat it as sensitivity evidence, not a reference. The int8 GEMV alone measured 186.88 µs per control for H160 (≈ 2.4 GMAC/s int8, matrix core only, number recovered from memory, primary stdout lost); it does not transfer to float32. A baseline CPU figure for all of `dio_manager` during CarPlay without the upmix has never been measured [open].

## 📊 Instruction and MAC estimates

QEMU Cortex-A15 instruction counts, calibrated but **not Krait cycles**:

| Frozen measurement | Instructions per 1,024 frames | Boundary |
|---|---:|---|
| V12 legacy | 6.10 M | historical upmix path |
| V12 float + Sound Check + matrix guard | 6.61 M | float path, no decode/QSA |
| V13 cabin synchronous | 7.80 M | cabin upmix |
| G9 reliable, fast tanh | 33.23 M | FFT + model + WOLA only |
| P1 default libm | 30.95 M | FFT + model + WOLA only |
| P1 experimental fast SiLU | 17.96 M | same, optional build |
| R1 frozen 144-param probe | 6.98 M | kernel only, untrained weights |
| R1 linear matrix, E-only / X+E | 1.85 M / 2.65 M | frame kernel only |

R1 memory: the 144-param probe holds 600 B weights, 32,864 B state and 57,456 B work (90,920 B before caller spectra); the linear matrix holds 32,848 B, with 16,416 B (E-only) or 32,832 B (X+E) of active coefficients and 8,208 B scratch. Each extra source needs its own state and another evaluation, so N refiners cost about N× the neural work. The first R1 probe's screening cap was ≤ 15 M added dense MAC/s (80,000 MAC per hop256), an experiment scope rather than measured headroom.

Unit trap: P1's 7.74 M is *per 256-sample hop* while V12's 6.61 M is *per 1,024-sample block*.

Dense MAC per second (weights only, excluding FFT, features, nonlinearities and memory):

| Model | Params | Dense MAC/s | Status |
|---|---:|---:|---|
| HSSR-160 controller (shipped) | 451,464 | 42.12 M int8 | [code] shipped |
| HSSR width 192 / 256 (costed) | 597,000 / 943,368 | 55.73 M / 88.13 M | never trained |
| P1 baseline | — | 138.69 M | research |
| G9 direct decoder | — | 227.23 M | research |
| Q2 reference | 111,132 | 485.1 M float | too big |
| H1 multiscale | 56,492 | ~990.5 M | too big |
| CUpGAN-A, paper figure / disclosed core at 48 kHz | ~14,000 | 287 M / 715 M | blocked |

Width formula for the two-GRU graph: weights `3HF + 9H² + HO` with F = 368, O = 264; biases `12H + O`. A third 160-wide GRU would add 154,560 params. Any width or layer change needs a new model ABI because the C loader and state arrays are fixed-size (`hook/audio/hssr_neon.h:17-19`).

Q2 on the target would also copy ~1.68 MB of state per call plus a ~1.65 MB chain — 6–7 MB of `memcpy` per 1,024 frames, never budgeted. The Q3 design target was ≤ 1–1.5 M MAC per hop int8 with one rear iFFT pair (≈ 0.4 ms GEMV per hop, ~8% of a core before FFT).

Sources: `UPMIX_R1_RUNTIME_BUDGET`, `UPMIX_INDEPENDENT_AUDIT_2026-09-17`, `HSSR_DATASET_TRAINING_AUDIT`, `UPMIX_H1_GENRE_AGNOSTIC`.

## ❌ CUpGAN-A: measured out

Fraunhofer's conditional GAN ([Upmix Literature](UPMIX_LITERATURE.md)) looked ideal for ambience extraction but no weights or code are public. The disclosed four ConvNeXt blocks cost `4·(21·3·3 + 21·84 + 84·21) = 14,868` MAC per time-frequency cell = 657 M MAC/s at 44.1 kHz, **2.49× the paper's own 264 M figure**. `tools/cupgan-bench/` built the disclosed dense core with QNX GCC 8.5:

| Build | ARMv7 instructions per 512-sample hop | Per audio second |
|---|---:|---:|
| Scalar, shipping-like | 48.90 M | 4.584 G |
| Packed pointwise + explicit NEON | 18.00 M | 1.687 G |

Even the optimized lower bound is 99.2% of one 1.7 GHz core at IPC 1.0 (165% at IPC 0.6) — ~21.2 ms of CPU per 1,024-frame callback against a 5 ms gate, before STFT, output projection or FiLM. Verdict: blocked pending a smaller retrained model. In the current hook `AUDIO_STEREO_UPMIX` accepts only on/off; any other value such as `cupgan` logs an error and falls back to off (`hook/audio/audio_hook.c:144-160`) [code].

Beyond cost, the paper does not specify: IBN channel grouping, the conversion from the six output features back to complex STFT, the FiLM/control-net layout, precision and quantization, padding, the GELU approximation, and any 48 kHz model. At 48 kHz its 1,024 window is 21.33 ms and its 512 hop 10.67 ms. The integration gate it was given: licensed weights (or a clean-room model under another name), parity with a trusted reference, a full QNX measurement including STFT, nothing allocated or locked in the callback, a loudness-matched blind comparison against the deterministic candidate, and a native APAC 5.1 bypass.

Sources: `CUPGAN_A_FEASIBILITY`, [cupgan_aes_2026.pdf](../sources/papers/cupgan_aes_2026.pdf).

## 🔧 What the async split owns

FFT, masks and WOLA stay on the audio thread; only the 368 → 264 recurrent network runs on the persistent worker, with two-entry SPSC queues and generation ownership (`hook/audio/hssr_control_worker.c`). The worker *requests* `SCHED_RR` priority 17 but discards the result, `(void)pthread_setschedparam(...)` (`hook/audio/hssr_control_worker.c:115-131`) [code], so the priority is not proven. Offloading does not remove work or make the second core uncontended; a second per-hop model needs its own owner, bounded state, reset generations and a delayed-dry fallback.

Promotion gates for any new model: freeze architecture and weights; report MACs, other ops, state bytes and latency separately; prove parity and reset/chunk handling with no allocation, I/O or locks in the audio path; then paired **real Krait full-chain traces** under representative load (P50/P95/P99/max, worker misses, QSA underruns). None of that exists yet for the shipped path either [open].

## ⚡ Training machines

| Machine | Spec | Used for |
|---|---|---|
| NAS | Ryzen 3 2300X, 4 cores, 8.25 GB RAM, Torch 2.10 CPU, ~7 TiB free | every sealed run; Q2 ~1.9 s per update; q6d 2,889 s for 5 epochs on 4 threads |
| Mac | M3 Max, 14 cores, 36 GiB, MPS | teachers (AST on MPS 0.36–0.43 s vs 1.74 s CPU); Q3 epoch 0 in 45 min |

MPS was barred from sealed training because exact-restart qualification failed — yet two uninterrupted processes of the same study already differed (first at update 11 by 5.96e-8; max parameter 2.98e-8, Adam 3.73e-9). That nondeterminism is not a resume bug; sealing weights by hash after training is enough for research. The refusal made the 4-core NAS the bottleneck. The SSH crop bridge cost ~29 s network time per 33 MB crop against 0.884 s of NAS compute per 128-frame chunk.

Sources: `UPMIX_Q2_MPS_RESTART_QUALIFICATION`, `UPMIX_RELEASE_DATA_AUDIT`, `UPMIX_FULL_FLOAT_DATASET_V15`, `UPMIX_R1_RUNTIME_BUDGET`.

## 📚 Sources

Merged originals were removed from the tree after this note absorbed them; they remain in git history (commit 23972aa).

| Source | What it holds |
|---|---|
| `UPMIX_R1_RUNTIME_BUDGET` | Units, Krait wall time, QEMU and MAC tables, R1 probes |
| `UPMIX_INDEPENDENT_AUDIT_2026-09-17` | Q2 cost, int8 GEMV figure, 200 M MAC/s ceiling |
| `CUPGAN_A_FEASIBILITY` | CUpGAN architecture, MAC discrepancy, QNX benchmark |
| `HSSR_DATASET_TRAINING_AUDIT` | Width ablation arithmetic |
| `UPMIX_Q2_MPS_RESTART_QUALIFICATION` | MPS/CPU nondeterminism study |
| `UPMIX_RELEASE_DATA_AUDIT` | NAS and Mac capacity, SSH bridge cost |
