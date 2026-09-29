# Causal Separator Long-Context Audit

_2026-09-28. Frozen host-only diagnostic, before seeing results._

## Question

The causal complex control and two-tap filter train on random **8 s** windows with fresh GRU and 2 s running-level state, but live CarPlay runs continuously. The [two-tap probe](UPMIX_CAUSAL_DEEP_FILTER_PROBE_2026-09-28.md) failed the source/entry gates at 4k. Before deciding whether to change the architecture or training schedule again, test whether its uninterrupted context itself hurts the eligible estimate. A recent [DSP separation paper](https://arxiv.org/abs/2609.12201) documents a continuous-inference collapse in a different block-trained model; it does not establish that our GRU model has the same behavior.

On the **first six already-inspected DEV songs**, take the first 60 s of identical true-stem mixtures. For each frozen `complex_control/step4000.pt` and `causal_deep2_4k/step4000.pt`, infer once through the whole mixture STFT and again in contiguous **8 s / 750-frame** segments that reset GRU and running level. Apply the inferred operator from each mixture to aligned true eligible/vocal stems. Compare only segments after 16 s and omit the first **1 s** after each reset, reducing seam/startup effects. The whole and reset paths see the same source audio in each compared frame. This diagnostic intentionally tests the combined state-reset effect; it does not isolate GRU from normalization or demonstrate frame-by-frame device behavior.

Primary metric: songwise normalized complex-STFT error `10log10(sum|estimate−eligible|² / sum|eligible|²)` on compared tail frames, and its continuous-minus-reset difference. A convincing mismatch requires median difference **≥+0.5 dB** with the continuous path worse in **≥4/6 songs** for a checkpoint. Also report paired eligible transmission and vocal/eligible source-attributed transmission; a lower error produced only by a quieter rear is not a product win. If the mismatch gate fires, isolate GRU state versus running normalization before changing training; otherwise do not cite 8 s crop resets as an observed cause of the source/entry failures. Neither result promotes the failed 4k students to C, nor replaces an independent set or owner listening.

## Results

Pending.
