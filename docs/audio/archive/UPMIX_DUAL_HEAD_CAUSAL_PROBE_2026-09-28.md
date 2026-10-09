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

Both sequential GPU runs reached step 4000 in the original lease-guarded PC session; no second GPU tenant or pip change was introduced. The control finished in **748 s** with DEV 20 s SI-SDR **−1.21 dB** and its scalar vocal-leak diagnostic **.161**. The complex/role candidate finished in **882 s** with SI-SDR **−1.53 dB** and vocal leak **.091**. This is an apparent leak/retention trade-off, not a source-attribution or renderer verdict. A local fixed-input prefix check of `separate_stft` differed by at most **1.9×10⁻⁷** on the first ten frames whether run alone or as part of a 21-frame spectrogram; it supports zero-lookahead computation for the Python model, not a streaming port.

The transient loss of new Tailscale/SSH connections ended without intervention. The **existing training session returned exit 0 and its completion line**; both `train_done.txt` files and step-4000 checkpoints were verified, the GPU lease and rapid queue were empty, and [`run_dual_head_causal_diag.ps1`](../../../tools/stereo-upmix-render/pc/run_dual_head_causal_diag.ps1) completed the frozen CPU diagnostics. Checkpoint SHA-256: control `A38DCCB9DAD5AF382AC34EB38CBD3FB2E6114E60C0C34B4BA8C40279ABB93F12`; candidate `B33EB75AF32DBDFD4A878D703F1D53B7BAC5431AA3224D0F86F62737F013E2F7`.

| Fixed DEV12 reanalyzed source metric | Shared-mask anchor | Complex control | Complex plus role head |
| --- | ---: | ---: | ---: |
| Eligible energy transmitted | .4624 | .3851 | **.3623** |
| Vocal / eligible, per-song median | .4050 | .5036 | **.4497** |
| Drums / eligible | .0710 | .0830 | .0845 |
| Bass / eligible | .0424 | .0484 | .0429 |
| Frozen source gates passed | — | **0/4** | **0/4** |

The extra role head reduces absolute vocal transmission from **.1790 to .1290**, but also reduces eligible transmission from **.3851 to .3623**. Its vocal/eligible ratio improves against the matched control, yet is worse than the shared-mask anchor and exceeds the ≤.324 gate. On the same 43 vocal entries, eligible post/pre retention median/p10 improves **.703/.418 → .837/.542**; the candidate still misses ≥.85/.60. Its pre-entry eligible transmission is already lower (median **.414** versus control **.503**), so improved post/pre ratio alone cannot establish a useful rear.

**Reject both before C under the frozen rule.** No gain-matched renderer screen, full40/TEST20 promotion, device port or listening claim follows. This paired result says that an auxiliary protected-role prediction improves some vocal-entry behavior at this training age, but does not fix aggregate instrument recall or selectivity. It does not isolate whether data, optimisation, temporal context, per-bin output limits or compute ultimately dominates.

One external architecture warning informs a later test, not this verdict: a recent [embedded music-separation study](https://arxiv.org/abs/2609.12201) found that a different convolutional separator trained on independently padded short blocks deteriorated during continuous frame-by-frame inference, despite good block-wise scores. Our model has causal GRUs and 8 s reset crops, so that paper does not establish the same failure here. For a future model passing source and entry screens, compare uninterrupted 60 s inference with controlled state resets and inspect per-time source transmission before a device claim.

PC artifacts: `D:\upmix\runs\dual_head_causal4k\{complex_control,complex_role}\step4000.pt`; at the run root, `{complex_control,complex_role}.selectivity.json`, matching `.txt` files, and `transitions.txt`. The source JSON contains per-song rows; transition text contains the 43-entry summary and worst events. The PC was reachable again after the transient; no head-unit or Track A file changed.
