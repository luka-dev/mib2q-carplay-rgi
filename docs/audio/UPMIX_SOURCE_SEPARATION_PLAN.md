# Upmix: a rear scene decided per source (plan, 2026-09-25)

*Branch `feature/upmix-source-separation`.  Why the current rear cannot be made
"confident" by tuning, what was tried before and why it failed, and the plan
for a per-bin protect/rear mask network sized for the head unit.*

---

## 🎯 Goal

A rear scene whose contents are decided **once per source**, not per bin per
hop from weak evidence.  Voice, bass and the drum core stay in front always;
guitars, keys, synths and the wide texture go behind at a constant depth; the
bed and the air stay as they are.  The remaining motion must be the mask's
own noise, which can be smoothed because the depth no longer changes with
every syllable.

## 📚 What the history says (read before designing)

The full ledger is in [Upmix Training History](UPMIX_TRAINING_HISTORY.md) and
[HSSR Model](HSSR_MODEL.md); the points that shape this plan:

| Line | What it was | Why it failed | Lesson |
|---|---|---|---|
| V16, G1–G17, H1, P1, Q2 | separation *as the product*: 12 instrument heads, complex reconstruction, NMSE | 240 songs, a 32-band power interface where only 2,352 of 111k parameters did per-bin work; a zero-parameter Wiener filter beat Q2; H1 ~990 M MAC/s | do not reconstruct stems; do not judge by NMSE; do not go through band powers |
| Q3 mask-net | per-bin softmax over 13 slots, FFT 1024, 1.09 M params, 725 M MAC/s | never costed; 3.6× the budget; direction changed to the second scene | a per-bin mask **can** learn (beat Q2 on lead/bass/guitar/keys after one epoch) — it was the size and the frontend that were wrong |
| Q4 ProtectNet | per-bin "protect" share, 294k params, 578 M MAC/s, stems + teacher cache | in-mix lead leak −7.9 dB vs V11's −23 dB (one memory note, no report) | the exact shape we want, but judged by leak alone with the old renderer and never sized |
| V12, V15 | fine-tune / distil the routing head on BS-RoFormer two-stem teacher targets | every candidate moved *more* voice rearward; teacher leaks became labels | teacher labels only where the teacher is sure, asymmetric loss, MoisesDB stems as the only arbiter |
| q6d (shipped) | HSSR-160 controller on real MoisesDB 6-class targets, asymmetric rear loss | works as a *band* controller; cannot separate within a band | real stems + asymmetric loss is the recipe that worked |
| 2026-09-25 gate | per-bin comodulation with the voice body | halves the vocal leak, but a guitar sharing the band still follows the vocal phrases | the per-bin decision is the missing piece, not another band rule |

Budget facts ([Upmix Compute Budget](UPMIX_COMPUTE_BUDGET.md)): the audit's
ceiling is ≈ 200 M MAC/s int8 on the async worker; the two shipped GRUs use
≈ 84 M; there is no measured free 75 % headroom.  Export is a fixed
`HSSRNEO1` blob (368/160/264 only) — a new head means a new ABI.

## 🔬 Phase 0 — prove the mask on the PC, no hook changes

Everything runs on the training PC (`100.127.113.11`, RTX 5080, MoisesDB at
`D:\upmix\datasets\moisesdb\moisesdb_v0.1`, 3,367 library songs at `D:\music`).

1. **Cache at the car's geometry.**  FFT 512, hop 128, half-sine window,
   48 kHz — the same STFT the DSP runs, so the mask never needs a second FFT.
   Per song: the unit-gain stem sum plus a "mastered" copy (limiter + tilt EQ,
   random) to close the domain gap; targets per bin = ideal ratio masks of
   four groups **vocals / drums+percussion / bass / other**, plus a
   `protect` = vocals + bass + drum core.  Artist-disjoint split, the q6d one
   (181 / 40 / 19).
2. **Model.**  Per-bin features (log |L|², log |R|², cos/sin IPD, band
   context) → small causal net → per-bin sigmoid for the four groups.  Two
   candidates: (a) a per-bin MLP fed by a per-band GRU context (≈ 0.3 M MAC
   per control → 30 M MAC/s at 93.75 Hz); (b) a causal 2-D conv over
   bins × hops (≈ 1 M MAC per control).  Both an order of magnitude under Q3.
3. **Loss.**  Asymmetric: a vocal/bass/drum-core cell sent rear costs 4×; a
   missed "other" cell costs 1×; smoothness over time.  No NMSE anywhere.
4. **Judge with the harness, not a proxy.**  The renderer gets a test-only
   `--protect-mask FILE` input (per control, per bin) that replaces the
   vocal-gate factor; `upmix_stem_tests.py` then reports the same rear share
   / swing / flicker per stem as today, on the DEV songs, against the
   2026-09-25 candidate at equal rear brightness.  Pass = vocal own-cell rear
   ≤ 0.05, guitar/keys rear ≥ the candidate's, swing and flicker ≤ the
   candidate's.

### Review (Codex, 2026-09-25) and what it changed

Asked whether the grouping is right.  Agreed with caching the roles apart and
training the first model with all vocals in front, with four corrections, all
adopted:

1. **Rear eligibility must be explicit.**  "Rear = m_other" would never send
   cymbals back; the DSP takes the *eligible* masks (other + cymbals/hi-hat)
   with their own gains.  Percussion (congas, claps) is not rear-safe by
   default: it stays with the drum core in the first model.  The cache keeps
   the original stem labels, so a regrouping never rebuilds it.  A per-bin
   mask is still not object tracking — the harness keeps measuring
   within-source depth consistency and temporal stability.
2. **Backing vocals behind** stays a later opt-in experiment: sustained
   backing evidence *and* a calibrated bound on lead contamination, ambiguity
   returns the voice forward, loss on lead energy actually admitted rear,
   hard cases (doubles, harmonies, alternating singers) in the data.
3. **Ideal ratio masks are targets, not a safety label**: the loss also
   carries the vocal energy the eligible masks would admit rear, normalised
   per frame so quiet consonants and breath count; stem gains are randomised
   with the targets recomputed; the model is judged by leakage, support,
   depth fragmentation and syllabic modulation, never by mask loss.
4. **Distillation**: teacher confidence is not correctness; HTDemucs' four
   stems cannot label lead/backing or core/cymbals and must not be made to;
   validate the confidence rule on MoisesDB for false rear permission first,
   keep the teacher at low weight beside a permanent real-stem anchor,
   freeze thresholds on DEV, test once on the untouched TEST split.

Cache v2 therefore holds seven groups — lead vocal, other vocal, drum core,
cymbals/hi-hat, percussion, bass, other — as mmap-able int16 with the labels
beside; training merges them (`--merge`, default vocals whole, percussion
with the core, cymbals and other eligible).

### Phase 0 findings so far (2026-09-25/26, 40 DEV songs through the real DSP)

* **A mask that only protects is not enough.**  Trained masks v2–v4 collapsed
  to "protect everything" (other admitted 0.02–0.08): vocal bleed sits in
  nearly every 93.75 Hz cell, so any energy-accounting leak term wins over
  support.  Through the DSP the v2 mask gave an empty rear (guitar 0.01).
* **The oracle says where the ceiling is.**  With the protect mask computed
  from the *real* stems (everything but vocals/bass/drums eligible) and the
  DSP otherwise unchanged: vocals 0.01 (top bands 0.04–0.09), drums 0.20 →
  0.08 with their swing 0.42 → 0.10, bass 0.04 — but guitars only 0.06 →
  0.09 and keys 0.08 → 0.11, guitar swing 0.35 → 0.32.  A perfect protect
  mask stabilises what it protects and does **not** create a confident rear:
  the amount and the flicker of the rear are decided by the DSP's own object
  heuristics (slots, ridge, coherence, per-bin memories), which the mask only
  vetoes.
* Therefore the mask must **drive** the rear, not veto it: rear object =
  eligibility × the bin's content at a constant per-source depth, pan from
  the bin's measured balance, the heuristic object path bypassed.  The test
  build has `--mask-drive G` for exactly this; the oracle through it is the
  true ceiling of a mask-driven renderer and the next number to read.

**The mask-driven ceiling** (oracle eligibility drives the rear object,
`--mask-drive`, energy share used as the gain, 40 DEV songs):

| | vocals | guitar | piano | keys | bass | drums |
|---|---|---|---|---|---|---|
| candidate (22.09 knobs + gate) | 0.01 / swing 0.21 | 0.06 / 0.35, 24/39 songs > 0.30 | 0.03 / 0.17 | 0.08 / 0.28 | 0.04 / 0.11 | 0.20 / 0.42 |
| oracle protect only | 0.01 / 0.07 | 0.09 / 0.32, 22/39 | 0.08 / 0.17 | 0.11 / 0.26 | 0.04 / 0.11 | 0.08 / 0.10 |
| **oracle drives, G = 1.0** | 0.01 / 0.08 | **0.16 / 0.24, 7/39** | 0.12 / 0.12 | 0.15 / 0.20 | 0.04 / 0.08 | 0.08 / 0.10 |
| oracle drives, G = 0.5 | 0.01 / 0.05 | 0.04 / 0.07 | 0.02 / 0.04 | 0.05 / 0.07 | 0.01 / 0.03 | 0.06 / 0.06 |

With the energy share converted to an amplitude gain (`--oracle-amp`,
`a = sqrt(eligibility)`, Codex review 2):

| | vocals | guitar | piano | keys | bass | drums |
|---|---|---|---|---|---|---|
| **oracle drives, amplitude, G = 1.0** | 0.03 / 0.12 | **0.20 / 0.22, 7/39** | 0.14 / 0.12 | 0.17 / 0.19 | 0.07 / 0.11 | 0.12 / 0.15 |
| oracle drives, amplitude, G = 1.5 | 0.06 / 0.19 | 0.36 / 0.31, 20/39 | 0.28 / 0.17 | 0.31 / 0.21 | 0.15 / 0.19 | 0.16 / 0.24, 13/40 |

At G = 1.0 the guitars' top bands sit at 0.39–0.43 rear — the 22.09 car's
level (0.47–0.53) — with a third of its swinging songs and the voice at 0.03.
G = 1.5 buys a fuller rear and brings the swing back (20/39) and the bass
with it (0.15): the amplitude mapping at unity gain is the operating point.
That is the "confident rear" the plan is after, and it exists with a perfect
mask; everything else is how close a network gets to the oracle.

### Review 2 (Codex, after the oracle)

Ranked by expected impact, all adopted as the next steps:

1. **Finish the mask-driven oracle first.**  Rear = `G · a · X` with `a` a
   bounded *amplitude* gain (an energy-share mask is not that gain — convert),
   separate constant depths for "other" and cymbals; pan = the mixture's L/R
   balance averaged over a local T-F region with extremes compressed, never
   instantaneous per-bin pan or discrete slots.  Gains, not audio, are
   smoothed: protection engages within one hop and releases over 80–150 ms;
   permission opens over 30–60 ms, releases over 80–150 ms unless protection
   overrides.  Shared cells (0.3–0.7): front retention plus a conservative
   continuous taper, never "hold the last decision" through new vocal
   evidence.
2. **Treat the collapse as an objective / implementation bug until
   disproven.**  Overfit one balanced excerpt with the pure supervised loss and
   audit alignment, logits vs probabilities, reductions and gradient
   magnitudes (`mask_net.py audit`).  Then: two independent sigmoid heads —
   *eligible support* and *protected presence* — combined conservatively at
   routing time, balanced BCE-with-logits on confident cells normalised per
   group, ramp the contamination penalty only after support is useful, hard
   examples last.  Per-frame normalisation needs floors and validity masks.
   The pure-IRM run is an objective ablation, not a separability bound.
3. **Buy spectral structure before recurrent width**: five adjacent bins at
   93.75 Hz are unlikely to tell guitar/keys from voice; the first candidate
   is a 2048/512 analysis for the mask net with a small frequency-convolution
   front-end and causal temporal context, under 150 M MAC/s including
   features, masks aligned by timestamp and the analysis delay counted in
   the protection timing.
4. **Stop**: widening bin-wise MLPs before the audit; teacher "other" as
   permission; judging models through the veto-only DSP; the 176 M MAC/s
   TFNet is a diagnostic, not a deployment candidate.

### Training log (Phase 0)

| run | net / loss | DEV mask space (vocal-owned → rear / other-owned → rear) | note |
|---|---|---|---|
| v2–v4 | per-bin MLP, ownership loss | 0.02–0.03 / 0.02–0.08 | protect-everything collapse |
| v7 | TFNet 67k, balanced BCE-with-logits, contamination ramp from step 1200 | before the ramp 0.73 / 0.78 — no discrimination; after 0.025 / 0.14 | the balanced loss opens the eligible cells, but on 512-point features the net cannot tell vocal cells from other cells across songs (it can on one excerpt), so the ramp collapses it again |
| v8a | conv2048: 2048-point analysis, frequency conv stride 4 (23 Hz resolution into each DSP bin), causal GRU per bin, 3.9k params, ~0.97 M MAC per control | 0.74 / 0.79 before the ramp, 0.025 / 0.18 after — identical to v7 | three architectures, one curve: not the features |
| probe | balanced vocal-owned vs other-owned cells, local spectrum only, small classifier, proper split | **DEV AUC 0.81 / 0.81** (512 and 2048 features), accuracy 0.71 | the cells ARE separable per cell; the mask nets were not seeing it |
| v9a | conv2048 + **input standardisation** (raw log powers −16..+8 saturated the tanh/GRU gates; the C runtime standardises its features too), mastering gain 0..6 dB | running | the probe standardised its inputs, the nets did not |

### Review 3 (astra, after the probe)

Standardisation is plausible but not shown to be the sole cause (v9a with it:
still 0.71 / 0.74 at step 1000).  Three objective problems named: per-output
positive/negative balancing destroys calibration (each sigmoid learns under an
artificial prior, and two such outputs summed as "other + cymbals" mean
nothing); the clamp at 1 kills gradients and hides compensating errors; the
ambiguous-cell term may dominate.  The contamination ramp then rewards the
easiest solution, suppress everything — a consequence, not evidence.

Prescription, in order: (1) transplant the probe's exact recipe into the
pipeline — its descriptor (spectrum relative to the local mean + the mean +
IPD + position), standardisation from representative augmented TRAIN data,
one direct eligibility logit, plain BCE on owned cells, no ambiguous term, no
penalty, no clamp — and reproduce the DEV AUC first (`--arch probe --loss
probe`, v10); (2) align the target's temporal support with the features
(`--target-hop single`); (3) calibrate afterwards with soft-target BCE on the
eligible power fraction, keep `a = sqrt(e)`; (4) roles, if needed, as a
softmax over fractional stem-power targets, not independent sigmoids; (5)
discrimination first, protection later with an explicit retention constraint;
(6) fixed LR and a small sweep before any architecture change.  The best
cell-level predictor of the DSP result is eligible-energy retention vs
ineligible-energy leakage weighted by real stem power, per protected class;
swings need the DSP itself.  Engineering milestone at G = 1.0: guitars ≥ 0.15,
voice ≤ 0.04, ≤ 10/39 swinging songs.  Stop: architecture searches and gain
increases; keep TEST untouched.

## 🛠️ Phase 1 — into the DSP

New blob `HSSRMSK1`, a second job on the existing control worker (own state,
generation reset, delayed-dry fallback), mask applied in the render loop
where the gate is today; QEMU instruction count and a real Krait bench
(`tools/stereo-upmix-render/bench/`) before any car run.  Float parity ≤ 2 LSB
with the host renderer (`-ffp-contract=off`).

## 📈 Phase 2 — the library through a teacher

Demucs (`htdemucs_ft`) on the RTX 5080 over `D:\music` gives ~14× more
material than MoisesDB and, unlike the stem sums, real masters.  Used as
**soft targets where the teacher is confident** (mask < 0.1 or > 0.9),
never as truth; DEV stays MoisesDB.  This is what V12/V15 got wrong.

### The teacher measured (2026-09-25, `teacher_label.py validate`)

htdemucs_ft on 40 MoisesDB DEV excerpts (60 s, loudest vocal), its ideal
ratio masks at the car's STFT against the real stems, cells ≥ 220 Hz:

| teacher "other" share > | coverage | protected energy inside those cells | of which vocal | "other" recall |
|---|---|---|---|---|
| 0.60 | 33 % | 18.1 % | 11.2 % | 72 % |
| 0.80 | 26 % | 12.8 % | 6.8 % | 61 % |
| 0.90 | 19 % | 8.8 % | 4.4 % | 49 % |
| 0.95 | 14 % | 6.6 % | 3.0 % | 38 % |

The teacher leaves **14 % of the vocal energy in "other"** (median; p90
24 %).  That is the V12/V15 mechanism in one number: a mask trained on
teacher rear-permission would learn to send a seventh of the voice behind.
Confidence gating does not repair it — even at 0.95 the confident rear cells
still hold 6.6 % protected energy and cover only 14 % of the mix.

The protection direction is clean enough (run 2, same songs):

| teacher "vocals" share > | precision (truly vocal energy inside) | recall (of all vocal energy) |
|---|---|---|
| 0.80 | 90.8 % | 73 % |
| 0.90 | 93.6 % | 62 % |
| 0.95 | 95.1 % | 50 % |

Decision: teacher songs never supervise rear permission.  They supervise
**protection only** — cells where the teacher's vocal share exceeds 0.9 are
labelled protected (the 6 % of non-vocal energy this over-protects costs a
little rear, the safe direction) — at low weight beside the real-stem anchor,
with no loss term at all on the other cells of a teacher song.

## 🔭 Neighbours on GitHub (survey 2026-09-26, what is worth taking)

Nothing public does role-aware per-bin masks at 512/128 inside 200 M MAC/s;
four things are worth borrowing, the rest is either our own idea in another
coat or the teacher path we measured out:

* **s3a-decorrelation-toolbox** (Surrey, ISC) — transients are *not*
  decorrelated but randomly panned; harmonic and noise parts are decorrelated
  each their own way.  Exactly the hi-hat lottery / transient flicker of the
  rear bed: a Phase 1 renderer rule (do not run hits through the bed delays).
* **sweetspotsoundsystem/upmix-rt** (MIT) — clean allpass decorrelators
  (~4 ms) for the rear, an alternative to the per-band delay comb.
* **HS-TasNet** (L-Acoustics weights, 23 ms streaming, 4 stems) — too heavy
  for the car, but the ceiling for the separability probe (what AUC a real
  low-latency separator reaches at our cells) and a *causal* teacher for
  protection labels, unlike HTDemucs.
* **arXiv 2609.12201** (MSS on SHARC-FX, 1.9 GMAC/s for 4 stems): confirms
  that 200 M MAC/s buys a protection mask, not a separator, and warns that
  nets trained on windows with zero initial state collapse in streaming after
  ~2 s.  Phase 1 gate: masks from the C runtime over a whole song must match
  the windowed Python evaluation (a per-bin net without memory is immune;
  any temporal GRU is not).
* `ffmpeg af_surround` / FreeSurround — the classic steering decoders, "back
  and forth" in pure form; worth one run through the stem harness as the
  public baseline in the doc, nothing more.

Not useful: GPL per-bin MLPs (our idea without protection), MagicSpatial
(the DSP second scene without learning), stem-routing via Demucs (the teacher
path: 14 % of the voice in "other"), generative / offline upmixers.

## ⚖️ Licensing

MoisesDB is CC BY-NC-SA 4.0, teacher checkpoints unverified: weights stay
development-only, as the two shipped blobs already are.
