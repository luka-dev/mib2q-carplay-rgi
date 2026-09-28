# Causal Two-Tap Deep-Filter Probe

_2026-09-28. Frozen Track B offline architecture test before training; no head-unit or listening result._

## Mechanism and control

The [dual-head test](UPMIX_DUAL_HEAD_CAUSAL_PROBE_2026-09-28.md) improved vocal-entry retention and reduced scalar vocal leak, but lost instrument energy and failed all four source gates. Its complex output still multiplied only the current mixture time-frequency cell. A short causal complex filter could use temporal structure across adjacent STFT frames when instrument and vocal overlap in a cell. A recent [embedded music-separation paper](https://arxiv.org/abs/2609.12201) reports a strictly causal complex FIR improvement on a different architecture and dataset; that is a motivation, not evidence that this Audi model will improve.

`deep2` predicts two stereo complex coefficients per bin and applies `Y[t,c] = W0[t,c] X[t,c] + W1[t,c] X[t−1,c]`, with a zero past frame at stream start. Both coefficients come from the same causal recurrent trunk as the prior 4k complex control. There is no future STFT frame or added filter lookahead. The output is trained with the same eligible-audio STFT plus multiresolution waveform losses. The existing `complex_control/step4000.pt` is the matched control: 48 bands, 2048/512 STFT at 48 kHz, causal 2 s streaming level normalization, features/temporal GRUs/global GRU **36/24/24/24**, current-frame decoder skip, seed 1, half true/half pseudo-labelled batches from 176 MoisesDB train and 774 owner-library masters, 8-second crops, batch 12, 4000 steps, AdamW 3e-4, 2000-step warm-up. The only candidate difference is the second complex output tap. No role head, role CE or rare-cell focus is used.

The candidate has **126.5 M estimated neural MAC/s** before the extra complex FIR, FFT/iFFT and C renderer; the prior control had **94.2 M**. A local 21-frame smoke test gave finite gradients, a frozen-operator source-additivity error ≤ **1.3×10⁻⁷**, and prefix/whole-spectrogram difference ≤ **1.3×10⁻⁷** for the first ten frames. These are code checks, not device compute or latency measurements. The training source hash is `a4855311959c6ed61a190635f878bd63c495608599008e263848950d8f9f6842`; the source/transition diagnostic hashes are `f672c6750b3a11a1534c92fb471d974d621dc38b1f08a779bb0d18e7959c8dab` and `9210200df5f823f5a8e61872a133de261d2cc5d6516b62fd5a6b89d29ce03464`.

## Frozen decision rule

At step 4000, evaluate the candidate and the unchanged control on the fixed DEV12 reanalyzed true-source diagnostic and fixed 43-entry DEV20 transition diagnostic. The inferred two-tap filter is held fixed from the **mixture** and then applied separately to each aligned true source; it is never re-inferred on a clean stem. Eligible transmission must be ≥ **.432**; per-song median vocal/eligible ≤ **.324**, drums/eligible ≤ **.0568**, bass/eligible ≤ **.0339**; entry eligible post/pre retention median ≥ **.85**, p10 ≥ **.60**. Require candidate versus matched control to improve entry p10 by ≥.05 without worsening vocal/eligible by >.01 before attributing a benefit to the second tap. These thresholds are unchanged from reviews 21–23 and fixed before the new run.

Only a passing candidate gets the enriched DEV20 real C renderer with a single global injection gain selected from the paired rear-RMS delta and verified to ±.1 dB. It must then meet guitar ≥.15, keys ≥.15, piano ≥.10, strings ≥.14, voice ≤.02, bass ≤.06, drums ≤.08 and no new any-stem swing song. A passing screen warrants unchanged-gain full DEV40 and separately frozen TEST20, and later actual device int8/latency, Track A port and the owner's deferred blind CarPlay test. DEV12/20/40 are already inspected; none is independent proof. If the source or entry gates fail, stop before C and longer training.

## Results

Pending.
