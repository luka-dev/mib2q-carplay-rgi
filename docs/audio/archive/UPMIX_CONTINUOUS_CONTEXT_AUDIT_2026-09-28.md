# Causal Separator Long-Context Audit

_2026-09-28. Frozen host-only diagnostic, before seeing results._

## Question

The causal complex control and two-tap filter train on random **8 s** windows with fresh GRU and 2 s running-level state, but live CarPlay runs continuously. The [two-tap probe](UPMIX_CAUSAL_DEEP_FILTER_PROBE_2026-09-28.md) failed the source/entry gates at 4k. Before deciding whether to change the architecture or training schedule again, test whether its uninterrupted context itself hurts the eligible estimate. A recent [DSP separation paper](https://arxiv.org/abs/2609.12201) documents a continuous-inference collapse in a different block-trained model; it does not establish that our GRU model has the same behavior.

On the **first six already-inspected DEV songs**, take the first 60 s of identical true-stem mixtures. For each frozen `complex_control/step4000.pt` and `causal_deep2_4k/step4000.pt`, infer once through the whole mixture STFT and again in contiguous **8 s / 750-frame** segments that reset GRU and running level. Apply the inferred operator from each mixture to aligned true eligible/vocal stems. Compare only segments after 16 s and omit the first **1 s** after each reset, reducing seam/startup effects. The whole and reset paths see the same source audio in each compared frame. This diagnostic intentionally tests the combined state-reset effect; it does not isolate GRU from normalization or demonstrate frame-by-frame device behavior.

Primary metric: songwise normalized complex-STFT error `10log10(sum|estimate−eligible|² / sum|eligible|²)` on compared tail frames, and its continuous-minus-reset difference. A convincing mismatch requires median difference **≥+0.5 dB** with the continuous path worse in **≥4/6 songs** for a checkpoint. Also report paired eligible transmission and vocal/eligible source-attributed transmission; a lower error produced only by a quieter rear is not a product win. If the mismatch gate fires, isolate GRU state versus running normalization before changing training; otherwise do not cite 8 s crop resets as an observed cause of the source/entry failures. Neither result promotes the failed 4k students to C, nor replaces an independent set or owner listening.

## Results

The CPU run completed on the six frozen inspected DEV songs. The same 60 s mixture STFT fed both inference paths; each song/checkpoint pair compared about 38 s of tail audio after reset warm-up exclusions. The full output and its source operator were evaluated before each deliberate 8 s reset. Results are paired at the song level:

| Frozen checkpoint | Median continuous-minus-reset normalized complex error | Songs with continuous path worse | Median eligible-transmission difference | Median vocal/eligible difference | +.5 dB / 4-of-6 gate |
| --- | ---: | ---: | ---: | ---: | --- |
| Causal complex control, 4k | **−.001 dB** | 3/6 | **+.004** | **−.0003** | **No** |
| Causal two-tap filter, 4k | **−.004 dB** | 2/6 | **+.006** | **−.0011** | **No** |

The individual error differences ranged from **−.047 to +.048 dB** for control and **−.050 to +.044 dB** for the two-tap filter. These are small relative to the precommitted +.5 dB criterion; neither model shows the large continuous-context degradation hypothesized from the external paper. This paired result weakens 8 s state resets as the cause of the existing source/entry failures for these checkpoints. It does **not** verify a physical streaming implementation: the comparison uses whole-STFT Python inference and manually reset spectral chunks, omits one second after each boundary, and covers only six already-inspected DEV songs. No C renderer, device, or listening claim follows.

PC artifact: `D:\upmix\runs\continuous_context_audit\result.json` with every song and both paths; `run.txt` contains the concise readout. The code and frozen PC runner are [`diag_long_context.py`](../../../tools/stereo-upmix-render/diag_long_context.py) and [`run_long_context_audit.ps1`](../../../tools/stereo-upmix-render/pc/run_long_context_audit.ps1). The observed next bottleneck remains source discrimination and retention on mixed/incoming-vocal audio, not a demonstrated long-state collapse.
