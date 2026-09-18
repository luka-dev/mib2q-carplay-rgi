# Upmix: cabin-first target and source-role redesign

2026-09-15 UTC. Whole-chain review requested by the user.
**Recommendation / experiment specification, not an implemented replacement,
trained winner, listening result or vehicle approval.** Production remains HSSR.
The transport remains stereo input to six 5.1 PCM slots, not 7.1 or 19 discrete
speaker outputs. Existing experiments, data and acceptance results are retained.

Implementation update: [C1 candidate-stem rendering reference](UPMIX_C1_CABIN_REFERENCE.md)
now implements the first offline comparison with six original TRAIN passages.
It does not promote their candidate roles or prove the acoustic target; the
redesign recommendation and later teacher/student stages below remain distinct.

Next user direction is [R1: reconstruction conditioned on the extracted part
and original audio](UPMIX_R1_CONDITIONED_RECONSTRUCTION.md). Its source-removal
anchor and restored rear path are explicitly separate. This is an experiment
contract and offline plumbing, not a trained-model quality result.

## Decision: change the target before another architecture sweep

The desired result is a stable front-led music scene with convincing supporting
material around the listener, intact timbre and transients, and no renewed bass
overload. Reconstructing the rear channels of an arbitrary authored 5.1 mix is
not the same task. Those channels can legitimately contain lead-vocal effects
or lead-guitar delays that conflict with our front-only foreground policy.

Recommended chain:

```text
stereo + causal musical history
  -> source-aligned components + musical-role evidence + uncertainty
  -> permission to relocate particular supporting material
  -> separate bounded J525-aware renderer
  -> one common SoundCheck gain + linked safety stage
  -> FL FR C LFE SL SR transport -> factory DSP -> physical cabin
```

The first milestone is an audible reference made from real stems, with an
explicit role manifest and the intended renderer. Compare strong offline
separation and a causal student against that SAME reference. Do not first
launch another long tiny-model run and discover afterwards that its target
does not produce the intended scene.

This is not a claim that stereo-to-5.1 learning is impossible. It is a change
from copying an externally authored layout to learning approved source/role
decisions and testing their placement in this particular playback chain.

## What the existing work actually established

| Evidence | Consequence, and limit |
| --- | --- |
| [Relative-energy P1 comparison](UPMIX_P1_RELATIVE_ENERGY.md) completed 960 updates per arm. Both arms failed reconstruction and active-band criteria, including optimizer examples. | Eight extra envelope weights did not produce a useful quality win. This small fixed test is not a full-corpus training result or proof against all neural upmixing. |
| [AlbumDB paper audit](UPMIX_ALBUMDB_PAPER_AUDIT.md) resolves layout and explains authored placement/effects. | A known downmix gives a reproducible pair, not an approved rear-role label. Independently authored stereo and immersive masters are not automatically aligned ground truth. |
| [Scene F](UPMIX_V16_SCENE_F.md) and [G preflight](UPMIX_V16_SOURCE_G_PREFLIGHT.md) already tested role permissions, finer spectra and complex/source reconstruction. | Renaming the old recipe “semantic” or “full spectrum” is not a new solution. Protection can suppress useful backing; instrument permission can admit lead. |
| [H1 genre-agnostic plan](UPMIX_H1_GENRE_AGNOSTIC.md) proposed per-instance roles and a true-source/offline/learned comparison. Its trained target remained the older family-based formulation. | Execute the unfinished reference-and-role experiment; do not describe it as a previously successful trained role recognizer. |
| [NAS inventory](UPMIX_NAS_ATTEMPT_INVENTORY.md) retains original stems and reusable fixtures. | Reuse the material, but do not promote technically complete teacher caches to semantically verified labels. |

Current P1 already reconstructs with **513 frequency bins**, not 32 output
bands. Its compact temporal representation and short real scored context are
separate limitations. The latest fixed optimizer set contains only 10.24 s of
unique scored audio; repeating it is a fit diagnostic, not genre coverage.

The old Good King Wenceslas and Cities In Dust observations remain required
regression cases. A wordless vocal hook is not backing just because it lacks
recognizable words. More rear energy is not automatically more useful detail.

## Musical target: roles, not an instrument-to-speaker lookup

| Component in the current passage | Default treatment |
| --- | --- |
| Lead, co-lead, lead doubles, wordless hook; principal synth/guitar/growl | Front-protected, including associated wet effects where attributable. |
| Confirmed independent backing harmony/response, secondary guitar/synth, supporting percussion | Candidate for restrained relocation, with coherent timing and timbre. “Vocal”, “guitar” or “high-frequency” alone is insufficient evidence. |
| Verified non-foreground room/texture/ambience | Candidate for a separate diffuse rear treatment; not every decorrelated signal is safe ambience. |
| Unknown source, uncertain role, disagreement or separation artefact | Remains front. It is not automatically assigned to an ambience bucket. |
| Bass/low-frequency body of a foreground source | Do not duplicate/boost it into the rear or split off its upper harmonics as a second rear object. |

Roles can change within a song. A guitar can move from accompaniment to solo;
a dubstep growl can be the foreground melody. Use temporally stable role
decisions with a promptly effective protection veto, not independent rapidly
switching permission for every frequency bin. Frequency-domain processing is
still allowed; “full-band” means coherent source identity, not moving all bass
to the rear or forbidding a protective filter.

No minimum rear-fill quota. Safe front fallback and successful spatial
enhancement are **different outcomes**: fallback can satisfy safety but cannot
pass the useful-rear test. Do not optimize silence into an apparent victory.
Likewise, do not promise a strong separated rear object on every possible
stereo recording: available evidence may not support one.

## Virtual Cabin: the renderer is part of the problem

Use [routing review](UPMIX_CABIN_ROUTING_REVIEW.md),
[static readiness / SoundCheck](UPMIX_STATIC_READINESS_AND_SOUNDCHECK.md), and
the recovered matrix in
[`p1_stereo51_cabin.py`](../tools/stereo-upmix-render/p1_stereo51_cabin.py).

- Target assumption remains **3D OFF**, not a claim that factory processing is
  bypassed. Focus All and minimum Surround must be recorded/verified for the
  eventual test; their current live state has not been read.
- Nineteen drivers are not nineteen controllable sources. Each rear door's
  lower driver and tweeter share the relevant feed; we cannot independently
  send a guitar to one and only a hi-hat to the other. High-frequency content
  can include audible vocal presence. C-pillar amplifier outputs do not create
  extra independently addressable channels in our six-slot transport.
- PCM C=0 does not mute the physical center. PCM LFE=0 does not remove bass
  from L/R or rear channels. No new LFE, bass makeup or automatic treble boost.
- The recovered input matrix is a bus-level electrical proxy, not measured
  seat response or a complete model of downstream EQ, dynamic DSP and fanout.
  Do not invent those transfers or train a purported inverse of them.

### Why generic downmix closure does not preserve the cabin front

For the recovered profile-0/bank-1 proxy, with rounded coefficients and the
approximately -99 dB floor paths omitted, set PCM C=LFE=0. Write front inputs
L/R, rear inputs rL/rR and bus outputs B. These are signal coefficients, not
perceived loudness or measured acoustic percentages:

```text
BL  = a*L + b*rL       a = .794328, b = .707946
BR  = a*R + b*rR
BC  = c*(L+R)         c = .501187
BLs = d*rL            d = 1.995262
BRs = d*rR
```

The current preview uses `L=xL-sqrt(.5)*rL` and likewise for R. Consequently:

```text
BL = a*xL + .146271285*rL
BR = a*xR + .146271285*rR
BC = c*(xL+xR) - .354392726*(rL+rR)
```

Thus an exact generic stereo reconstruction identity is not preservation of
the three front buses after J525. Changing the subtraction to
`b/a = .891251473` cancels same-side front-bus return, but still changes BC.
For arbitrary input, the known matrix imposes:

```text
BC = (c/a)*(BL+BR) - (c*b/(a*d))*(BLs+BRs)
   ≈ .630957237*(BL+BR) - .223872179*(BLs+BRs)
```

With C=LFE=0, arbitrary nonzero rear common-mode and all three unchanged front
buses cannot coexist in this proxy. The exact coefficient formula above was
checked against the code matrix on 4096 normal random six-slot inputs
(`numpy.random.default_rng(51)`) with C/LFE zero; maximum absolute discrepancy
was 8.88e-16. The printed nine-decimal approximation
is not accurate to that tolerance. This validates the algebra for the
floor-truncated proxy, not the complete factory matrix or physical cabin.
Forced anti-phase rear is not the proposed workaround. Allowing PCM C makes
more electrical solutions reachable but can require substantial compensating
gain/headroom; a full inverse is not the default recommendation.

### Separate source relocation from electrical compensation

Keep three distinct signals:

- `e`: an estimated supporting component in its original stereo image;
- `m`: the confidence-bounded, source-aligned part actually removed from front;
- `r`: the rear rendering of approved components after gain/filter/panning.

Use `front = x - m + delta`, where `delta` is a bounded renderer correction
derived from the **actual post-shaping rear signal**, not another learned
source. Do not subtract newly generated/decorrelated ambience as though it
were an aligned component of the original mix. A rear LF veto must also veto
the corresponding front removal; it must not create a spectral hole.

For fixed r, one possible *offline diagnostic* is to choose delta to minimize

```text
wF * ||a*delta + b*r||^2 + wC * |c*sum(delta)|^2 + lambda * ||delta||^2
```

subject to explicit correction, PCM and bus rails. Its reference front is
`x-m`. The weights and bounds are not calibrated yet. This exposes the front
versus center compromise instead of hiding it in a generic downmix constant.
First compare `m=0` with identical source signals/rear gain to isolate the
renderer; add deliberate source removal only as a separate comparison.
If headroom is insufficient, reduce rear demand before global limiting; do not
recover loudness with extra bass or per-channel makeup.

The raw estimator can retain a source-sum/STFT-consistency contract. The
post-placement renderer needs a separate bus/headroom/front-preservation
contract; it need not obey the old arbitrary stereo fold. Do not weaken old
tests or redefine their historical pass/fail to accommodate the new renderer.

## Model hypothesis, only after the reference is useful

Do not pick layer counts by intuition or declare a larger separator the
winner before measuring the actual runtime budget. Suggested research split:

1. A fast, frequency-resolved signal path retaining stereo magnitude, phase,
   level/envelope and onset evidence. Compare a short causal temporal filter
   or richer local context with the existing complex decoder under matched
   conditions, not all architecture changes at once.
2. A slower **past-only** musical-role context over real contiguous passages,
   tracking continuity and role changes. Silence padding is not music history.
3. A small set of functional components/permissions: protected foreground,
   supporting material, independently supported ambience and unresolved
   residual. Do not require perfect separation of every instrument family to
   make a useful relocation, or classify every non-vocal instrument as safe.
4. Strong offline teachers on NAS to propose source signals and labels;
   human-reviewed true stems remain the anchor. Teacher confidence is not
   role ground truth. A noncausal teacher cannot make future evidence available
   to the causal student; ambiguous passages still require fallback.

Mixture consistency is useful but not sufficient: bad estimates can sum to
the input while each component is wrong. Full waveform output is not inherently
hallucination, and complex masks are not inherently clean. Evaluate fidelity,
bleed and stereo image rather than choosing by that label. Unconstrained
generation of extra notes/details is not the next experiment.

Before sizing the student, benchmark FFT/WOLA, inference, state, renderer and
safety together at the real callback sizes. Existing baseline native/guest
tests do not prove a new architecture's export ABI, QNX deadline or physical
HU performance. Record latency, memory, worst-case processing time and state
reset/track-change behavior, not only parameter count or dense MAC estimates.

## Next experiment: a reference ladder, not another blind retraining

All comparisons use the same evaluation role manifest, source alignment,
renderer configuration, SoundCheck ownership, channel order and safety limits.
Separate an **oracle-role** test (supply approved roles to isolate separation
and placement) from a **predicted-role end-to-end** test (no evaluation role
annotation is given to the model at inference). Both are required; oracle-role
results cannot certify role recognition. Mixed lead/backing stems and wet
effects without reliable attribution remain unknown, not safe by filename.

| Stage | Comparison and stop condition |
| --- | --- |
| 1. Target proof | Real, role-confirmed stems: original stereo, current reference processing, and proposed cabin-aware placement. If even correct stems do not produce the wanted effect, revise placement/target before training. Offline bus/PCM screening is provisional; cabin target approval requires real-chain listening or a valid measured reproduction. Do not postpone this question until after full training. |
| 2. Separation ceiling | Strong offline source estimates through the same role policy and renderer. Compare with true-stem rendering; if foreground leaks or timbre collapses here, fix teacher/target before distillation. |
| 3. Causal learnability | Train a bounded student on a small but genuinely varied, role-approved TRAIN set. Evaluate continuous unseen TRAIN passages and fixed DEV; compare against stages 1/2 and a matched baseline architecture trained on the same NEW targets, loss, data, exposure and budget. Old authored-5.1 P1 is a historical reference, not that matched control. If it cannot fit the small set, investigate representation/capacity/context rather than scale the corpus. |
| 4. Coverage and deployment | Expand only after useful-rear and protection both pass. Then artist-disjoint evaluation, physical cabin listening/measurement, chosen-model native parity, real-time timing and installation gates. |

The retained MoisesDB TRAIN/DEV/held-out separation is 181/40/19 songs.
Use TRAIN material for fixture selection and target development; keep DEV
selection accounting explicit and held-out data out of architecture tuning.
Existing 572 short 11-family windows are reusable source material, not approved
musical-role targets and not full-song coverage. Original stems allow longer
contiguous windows without redownloading the corpus.

Select examples by failure mode, not just genre names: main/secondary role
swaps, wordless hooks, lead doubles/backing overlap, dense unison instruments,
wet lead, wide foreground, sparse/mono material, quiet onsets, bright cymbals,
bass-heavy electronic drops and foreground growl harmonics. If real labelled
electronic coverage is absent, report the gap; synthetic stress fixtures do
not prove dubstep generalization. Historical user complaint clips are useful
regressions, not an untouched final test set.

Before scoring new outputs, freeze the fixture, metrics, levels, renderer and
acceptance thresholds. Report both aggregate and worst-case/per-passage values:

- Protected dry and wet source contribution to all available buses; mixed
  source response as well as isolated probes. Numerical PCM protection is not
  a guarantee of physical lead-front-only behavior through unknown factory DSP.
- Useful rear reconstruction/retention AND leakage, full-band and active-band
  timbre/transients, stereo coherence, pumping, chatter and transition stability.
  Keep quiet/unknown rows visible; unavailable ratios are not a pass.
- Front L/R/center changes, LF excess, digital headroom, and limiter duty/depth.
  A global limiter can preserve samples while audibly ducking the whole song.
- Level-matched randomized listening plus the unnormalized gain/energy report.
  Judge front stability, intact music and useful envelopment together; volume
  alone must not win the comparison. No numerical “100% perfect” claim.

Physical validation needs fixed MMI settings and common-scale transfer captures
at controlled low levels before louder playback. Existing capture tooling is
reusable; no actual seat IR is assumed here. A measured `most_inputs` transfer
already includes the input matrix and must not have that matrix applied again.
Unknown/level-dependent transfers cannot be replaced with identity responses
and called a calibrated cabin simulation. Until measurement, bus-proxy analysis
and isolated PCM listening can screen candidates, not prove the in-car effect.

## External evidence informing this recommendation

Primary-ambient decomposition is a useful baseline, not a semantic role oracle.
Paulus and Torcoli report that moving speech/singing reverberation rearward
can change perceived foreground position. Their listening-room results do not
establish our cabin settings or an appropriate rear gain. This supports testing
wet-lead protection rather than declaring all ambience safe.
[EUSIPCO 2022 paper](https://arxiv.org/html/2206.02125v1).

Wisdom et al. describe differentiable STFT and mixture-consistency constraints.
Those are useful mathematical tools; their speech-enhancement result is not
evidence that our musical roles or cabin rendering improve.
[ICASSP 2019 paper](https://research.google/pubs/differentiable-consistency-constraints-for-improved-deep-speech-enhancement/).

A 10 September 2026 Analog Devices preprint reports a deployed 131k-parameter,
1.89 GMAC/s music separator on SHARC-FX, with a look-ahead/quality tradeoff.
It also reports training/continuous-streaming mismatch in its own system.
This motivates measuring real streaming context and complete runtime cost,
not copying its architecture or assuming our QNX HU has that DSP budget.
It does not diagnose our P1 failure or establish a genre-agnostic winner.
[Real-Time Music Source Separation on a Low-Power Audio DSP](https://arxiv.org/html/2609.12201v1).

## Handoff

Relative-energy P1 is complete and not promoted. C1 and the subsequent C2
candidate-source placement runs are complete; they do not establish musical
role or acoustic approval. R1 now adds separately tested reconstruction plumbing,
a completed real hi-hat extraction/error audit and a replay-verified256-step
E-only/X+E fit. The learned X+E result beats scalar attenuation by only .11dB
in source-error SDR on another time of the SAME TRAIN song; it is not a
reconstruction-quality win. No production model, transport or car installation
changed. The subsequent scalar/context factorial is now complete: context X+E
assessment NMSE .746008 versus scalar .894488. A TRAIN-only full-bin complex
linear X+E control reaches .531065, but still loses source energy and is fitted
to one song, not general music. Next: the bounded whole-song-disjoint
ML-controlled-filter hypothesis in the [linear comparison](UPMIX_R1_LINEAR_CONTROL.md)
and [R1 plan](UPMIX_R1_CONDITIONED_RECONSTRUCTION.md). Retain
reference listening and role approval as distinct gates. Reuse existing data,
provenance, streaming and safety infrastructure; do not reuse unapproved rear-role
labels or claim that the proposed learned architecture has already worked.
