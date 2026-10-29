# Spatial-Memory Horizon Upper Bound

_2026-09-28. Review 34, privileged causal-history diagnostic; both fixed-window and recency runs completed._

## Fixed question

The [spatial upper bound](UPMIX_SPATIAL_CUE_UPPER_BOUND_2026-09-28.md) fit a frequency-wise 2×2 complex stereo matrix from the first 30 seconds of true eligible and mixture audio, then improved held-out second-half eligible transmission, protected rejection and complex error over a diagonal fit. Trained matrix students still fail source and vocal-entry gates. Before adding a persistent spatial state to the network, test how much **past true-stem-informed history** the useful oracle needs. The answer selects a reasonable slow-state horizon; it is not a deployable estimator or independent validation.

For the same first 12 already-inspected DEV 60 s songs, fit the unchanged 1%-ridge full 2×2 matrix separately on **0–2 s**, **0–8 s** and **0–29.9 s**, using each window's full stereo mixture and true eligible stem. Freeze each per-song operator and evaluate all three on the identical **30.1–60 s** segment. No second-half source enters a fit; all rows remain privileged because the first-half true stem is unavailable live. Apply each frozen mixture-fitted operator to the aligned true eligible/vocal/drum/bass sources without re-inference, and report song-median eligible transmission, per-song protected/eligible ratios and normalized complex eligible error. Require ≥8 valid songs per protected family and preserve missing silent-source values.

The predeclared architecture interpretation is: an 8 s history is adequate to motivate a short persistent spatial state only if, versus the 30 s full-matrix oracle on the same held-out half, its median paired eligible transmission is no worse than **−.03**, median paired normalized complex error no worse than **+.3 dB**, median vocal/eligible ratio no more than **1.25×** (valid paired songs), and median drum/bass ratios no more than **+.01** each. Apply the same criteria to 2 s as a diagnostic, but select the shortest passing horizon only after both are measured. If neither passes, a short slow state has no upper-bound support; a longer causal history or a different cue is needed before training another 4k student. The separate DEV12 source/DEV20 entry/real-C/device/car gates remain unchanged.

## Results

The guarded CPU run completed all 12 songs. Output: `D:\upmix\runs\spatial_memory_horizon12\result.json` and `run.txt`; [`run_spatial_memory_horizon.ps1`](RETIRED_TOOLS.md#tool-887f743d6c3b) checks code hashes, the empty GPU queue and no competing run. The fixed 30 s reference reproduced the earlier oracle (eligible **.4617**, vocal/eligible **.3183**, drums/eligible **.0250**, bass/eligible **.0102**).

| First-window fit | Eligible transmission | Vocal/eligible | Drums/eligible | Bass/eligible | Paired eligible minus 30 s | Paired error minus 30 s |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| First 2 s | .3324 | .8948 (n=7) | .7629 (n=7) | .9563 (n=7) | −.0627 | +4.2895 dB |
| First 8 s | .7592 | .9410 (n=9) | .9296 (n=9) | .9641 (n=9) | +.2880 | +5.6172 dB |
| First 30 s | .4617 | .3183 (n=11) | .0250 (n=12) | .0102 (n=12) | reference | reference |

Neither short *initial* window passed the frozen gate. The first 8 s fit often passes much more of **everything**, rather than isolating eligible audio, and some short-window operators have near-zero eligible output, so protected/eligible ratios are undefined in 3 or 5 songs. The 2 s and 8 s ratio coverage is itself below the required eight songs for the 2 s row. This result does **not** prove that an 8 s memory state is intrinsically too short: a song intro may lack vocal or eligible examples needed to identify the operator. It only rejects fitting one fixed operator from the song's first 2 or 8 seconds.

### Precommitted recency follow-up

To separate *short memory* from *uninformative intro*, fit the unchanged full 2×2 matrix on **all 0–29.9 s** of the same song but weight past frames by `exp((t − 29.9 s)/τ)` with τ = **2, 8, 30 s**, alongside the original uniform 30 s fit. Weighted least squares uses the same 1% ridge and true eligible target; only the time weights differ. All operators remain frozen for the identical 30.1–60 s test. Apply the same four short-history comparisons above to τ = 2 and 8 s, with the uniform fit as reference; τ = 30 s is a gradient between short and uniform. If τ = 8 s passes where first-8-s failed, a causal slow spatial state with an ≈8 s decay has upper-bound support; if it also fails, a faster-forgetting state has no support from this test. This is privileged inference with true-stem first-half fitting and says nothing yet about learning the state from stereo alone or meeting the real vocal-entry, C, device or car gates.

The guarded CPU run completed all 12 songs. Output: `D:\upmix\runs\spatial_recency_bound12\result.json` and `run.txt`; [`run_spatial_recency_bound.ps1`](RETIRED_TOOLS.md#tool-a41291346db3) checks source hashes, the GPU lease, queue and competing processes. The fit uses every first-half frame; τ only changes its weight. The reference column is the **same uniform operator** as the first-window run. Paired deltas are song medians, so they need not equal a difference of the two unpaired medians.

| First-half weighting | Eligible transmission | Vocal/eligible | Drums/eligible | Bass/eligible | Paired eligible Δ | Paired complex-error Δ | Frozen short-decay gate |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| Uniform | .4617 | .3183 (n=11) | .0250 (n=12) | .0102 (n=12) | reference | reference | reference |
| τ=2 s | .4102 | .3037 (n=11) | .0214 (n=12) | .0096 (n=12) | −.0073 | +.1817 dB | **pass** |
| τ=8 s | .3898 | .2758 (n=11) | .0226 (n=12) | .0087 (n=12) | −.0228 | −.0117 dB | **pass** |
| τ=30 s | .4301 | .3006 (n=11) | .0241 (n=12) | .0109 (n=12) | −.0097 | −.0286 dB | gradient only |

For τ=2/8 s, the paired vocal/eligible ratio factors were **.9410/.9151** (11 valid); paired drums/eligible deltas **−.0026/−.0017** (12) and bass/eligible **−.0008/−.0011** (12). Thus both short decays met the *predeclared median* gate. They do not dominate every song: τ=2/8 s increased normalized complex error by up to **+.901/+.517 dB** in an individual song; τ=8 s lost up to **.0866** eligible transmission in one song. The short initial-window fits were often unidentifiable because first-8-s eligible power was at least 20 dB below its first-30-s mean in **3/12** songs and vocal power in **7/12**. These counts explain why a state should learn from available evidence and retain it across sparse passages, rather than initialize once from a fixed intro.

**Architecture decision.** The privileged result supports testing a **causal evidence-accumulating spatial state with seconds-scale decay and separate fast protected-source evidence**, rather than another scalar loss on the existing matrix. It does not select τ=2 over τ=8 for a product: both fits saw all 30 s, both use unavailable true eligible audio, and the median gate hides song outliers. A finite 2 s context, stereo-only state learning, real vocal-entry behavior and the 150 M int8/device limit remain unproved. The next bounded test must estimate the slow state from mixture alone, keep eligible output absolute level and protected rejection in the same source/entry gate, and compare a state-reset control on the same songs and training age before spending on the real C/full40 harness. No Track A/head-unit file was changed.
