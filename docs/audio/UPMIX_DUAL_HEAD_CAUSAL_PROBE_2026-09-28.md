# Dual-Head Causal Eligible-Audio Probe

_2026-09-28. Frozen Track B offline architecture test; results pending. No head-unit or listening claim._

## Mechanism and matched pair

Review 22's frozen score mapping let a four-role student pass the fixed source-attribution screen only marginally, while eligible-audio retention still fell on vocal entries. Symmetric 80 ms score inertia worsened protected-source leakage. A single real-valued mixture mask cannot restore the phase of overlapping sources, and a role probability can be useful to learn even when using it directly as the rear mask is unstable. This experiment keeps a causal recurrent band trunk, but separates a **stereo complex eligible-audio output** from an **auxiliary four-role classification output**. The latter learns fast vocal, drum and bass evidence; it does not directly gate or duck the audio output. Both heads see the current-frame band embedding and the causal recurrent state, so the audio head can preserve context at entries while the role head records new protection evidence. Whether it actually learns this separation is the question, not an assumption.

The control is `complex2` and the candidate is `complex_role`, otherwise matched: 48 causal bands, 2048/512 STFT at 48 kHz, streaming 2 s level normalization, zero explicit lookahead, features/temporal GRUs/global GRU **36/24/24/24**, current-frame decoder skip, seed 1, 176 true MoisesDB train songs plus 774 owner-library pseudo-labelled masters with half/half batches, 8-second crops, batch 12, 4k steps, AdamW 3e-4, 2k warm-up, ordinary eligible-audio losses. The candidate adds four role logits per bin and the established soft true-stem-power role CE weight **.279254**; it does not use the previously failed rare-cell focus loss. Both are trained from scratch for the same age; no reviewed DEV threshold selects a mask mapping. Estimated neural cost at 93.75 frames/s is **94.2 M MAC/s control / 126.5 M candidate**, before FFT, iFFT, renderer and int8 implementation. The estimate and host causality do not certify the ≤150 M measured device or ≤85 ms extra-latency requirement.

Frozen `sep_net.py` source SHA-256: `052dfef7a4d9ae3dde70e3fd84692d4546f8393f649e567db081616c7771bbec`. A local 8192-sample forward/backward smoke test produced finite gradients for both heads, output shape `[1,17,1025,2]`, and the expected neural MAC estimates. This does not substitute for PC training or streaming-device verification.

## Decision rule fixed before outputs

First confirm both runs reached step 4000, produced finite audio/losses and kept the same checkpoint source hash and train recipe. Run the fixed 12 DEV excerpt, reanalyzed true-source diagnostic for each versus the frozen shared-mask 4k anchor; run the fixed 20 DEV song transition diagnostic. A model can enter enriched DEV20 C rendering only if eligible transmission ≥ **.432**, per-song median vocal/eligible ≤ **.324**, drums/eligible ≤ **.0568**, bass/eligible ≤ **.0339**, and vocal-entry post/pre eligible retention median ≥ **.85**, p10 ≥ **.60**. Also compare candidate versus its matched control at the same age; require no worse vocal-entry p10 and no worse vocal/eligible by >.01 before claiming the extra role head helps. The source thresholds are the existing review-21/22 screen, not reselected for complex masks.

For a passing model, run the same frozen direct/no-bed/body0 real C renderer on enriched DEV20, one global gain selected to match paired median rear RMS within ±.1 dB, then require guitar ≥.15, keys ≥.15, piano ≥.10, strings ≥.14, voice ≤.02, bass ≤.06, drums ≤.08 and no new any-stem swing song. Only then promote to original full DEV40 and separately frozen TEST20 at the unchanged gain. These DEV sets are already inspected and do not supply independent product proof. Device int8 measurement, owner hearing and Track A's hook-port decision remain separate gates. If neither model passes source plus entry gates, stop this pair before C and document which mechanism failed.

## Results

Pending.
