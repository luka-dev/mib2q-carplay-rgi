# Causal Cross-Channel Matrix Student

_2026-09-28. Frozen Track B short training screen before measuring outputs; no head-unit or listening claim._

## Why this one architecture change

The [spatial-cue upper bound](UPMIX_SPATIAL_CUE_UPPER_BOUND_2026-09-28.md) fit true-stem-informed 2×2 stereo matrices on each DEV song's first 30 s and tested on its second 30 s. Compared with a diagonal matrix, cross-channel terms reduced median vocal/eligible transmission to **.647×**, improved complex eligible error **.633 dB** in **11/12** songs and increased eligible transmission; the preselected gate passed. This does not show that a student can infer the matrix from ordinary stereo. It does justify one controlled training experiment, rather than another width or score-calibration sweep.

The saved matched control is `D:\upmix\runs\dual_head_causal4k\complex_control\step4000.pt`, SHA-256 `A38DCCB9DAD5AF382AC34EB38CBD3FB2E6114E60C0C34B4BA8C40279ABB93F12`. The candidate changes only the eligible-audio output from a diagonal stereo complex mask to a full **2×2 complex matrix per time-frequency cell**, including L→R and R→L cross terms. The matrix is inferred causally from the stereo mixture; the same inferred operator must be frozen and applied separately to true stems in attribution diagnostics. It is initialized with diagonal real gain .3 and zero cross terms. No true stems are inputs at inference, and no oracle per-song matrix is copied into the student.

Both control and candidate use 48 bands, 2048/512 STFT at 48 kHz, causal 2 s level normalization, zero explicit lookahead, features/temporal GRUs/global GRU **36/24/24/24**, current-frame decoder skip, seed 1, half true MoisesDB and half pseudo-labelled owner-library batches (176/774 songs), 8-second crops, batch 12, 4000 steps, AdamW 3e-4, 2000-step warm-up and identical eligible-audio losses. The candidate's larger decoder costs **126.5 M estimated neural MAC/s**, versus **94.2 M** for the control, before matrix multiplies, FFT/iFFT and renderer. The local 21-frame smoke test produced finite gradients, frozen-operator source additivity error ≤ **1.3×10⁻⁷** and prefix/whole-spectrogram error ≤ **1.3×10⁻⁷**. Neither is an int8/device timing result. Frozen training source SHA-256: `3ab06adf52192c06bbd4891ee2a3628f24ca53d54b69de8408f8cf04be8c66d8`.

## Frozen decision rule

At 4k, compare candidate and saved control on the fixed DEV12 reanalyzed true-source attribution and 43-entry DEV20 transition diagnostic. Require eligible energy transmission ≥ **.432**; song-median vocal/eligible ≤ **.324**, drums/eligible ≤ **.0568**, bass/eligible ≤ **.0339**; vocal-entry eligible post/pre retention median ≥ **.85** and p10 ≥ **.60**. Also require the candidate to improve the matched control's vocal/eligible ratio and entry p10, so a newly passing aggregate cannot hide a loss of the mechanism the oracle selected. The source/operator diagnostic uses the full mixture-inferred matrix on every aligned source; applying just its diagonal would invalidate the test.

Only if all source and entry gates pass, run the frozen enriched DEV20 real-C direct/no-bed/body0 screen. Pick one positive output gain from the paired rear-RMS difference, verify ±.1 dB matching, and require guitar ≥.15, keys ≥.15, piano ≥.10, strings ≥.14, voice ≤.02, bass ≤.06, drums ≤.08 and no new any-stem swing song. Then consider longer training and unchanged-gain full DEV40 plus separately frozen TEST20, with adequate role coverage, actual ≤150 M int8 MAC/s and ≤85 ms extra latency measured on the head unit, Track A-owned port, and the owner's deferred blind CarPlay verdict. These inspected DEV diagnostics are mechanism screens, not independent proof. If the 4k source/entry gate fails, stop this matrix student before C or longer training.

## Results

Pending.
