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
| **htdemucs_ft's own mask drives, amplitude, G = 1.0** (`--teacher-mask`, 2026-09-26) | **0.03 / 0.14**, 11/40 swinging | **0.20 / 0.23, 8/39** | 0.14 / 0.12 | 0.16 / 0.18, 5/27 | 0.11 / 0.16 | 0.11 / 0.21, 8/40 |

**The teacher reaches the oracle.**  htdemucs's mask through the same
renderer gives guitars 0.20 (top bands 0.29–0.41), keys 0.16, voice 0.03,
drums 0.11 — the oracle's numbers, with somewhat more swing (8/39 guitar,
11/40 vocal songs).  The milestone (guitars ≥ 0.15 at voice ≤ 0.04) is met by
a 40 M-parameter separator at the car's STFT: the evidence is in the mixture,
and the distillation route has a real ceiling equal to the oracle.  What is
missing is a student that can copy the teacher within the budget — the
question is now capacity and training, not information.

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
| v9a | conv2048 + **input standardisation** (raw log powers −16..+8 saturated the tanh/GRU gates; the C runtime standardises its features too), mastering gain 0..6 dB | 0.71 / 0.74 at step 1000 — still no discrimination | standardisation was necessary, not sufficient (astra review 3) |
| v10 | the probe transplanted: its descriptor, one eligibility logit, plain BCE on owned cells, targets from the control hop | DEV AUC eligible-vs-protected 0.66; **through the DSP driving the rear (G = 1.0, amplitude): guitars 0.12 with 1/39 songs swinging, keys 0.14, but voice 0.08 (top bands 0.26–0.32) and drums 0.29** | the mask-driven renderer is stable even with a weak mask; the mask's discrimination is what is missing (oracle: guitars 0.20, voice 0.03, drums 0.12) |
| v11 | softmax over the five roles with fractional stem-power targets (astra point 4), same descriptor | vocal-vs-other AUC **0.80** at step 1000 (= the probe), eligible-vs-protected 0.66; through the DSP: guitars 0.13 (1/39 swinging), keys 0.14, voice 0.09, drums 0.28 | the pipeline reproduces the probe; the per-cell ceiling (accuracy ~0.7) is what now limits voice and drums |
| sweep on v11 | operating point of the mask at routing time: eligibility threshold 0.3 / 0.5, sharpening ², ³ | running | astra: sweep retention vs leakage, do not retrain first |
| v12 | v11 + the three previous controls stacked as inputs (32 ms causal context, stateless) | AUC 0.68 / 0.80; through the DSP: guitars 0.13, keys 0.15, voice 0.10, drums 0.29 — same as v11 | 32 ms of context changes nothing |
| v11 @ threshold 0.3 | routing-time operating point | voice 0.05, drums 0.17 — but guitars 0.07, keys 0.08; 0 swinging songs | the threshold cuts both sides evenly: at AUC 0.80 the frontier is unfavourable |
| v13 | v11 + a per-band GRU context (the band's recent history, 8 values per band appended to every cell), 66k params, stateful | eligible-vs-protected AUC 0.66 → **0.75**, IRM error 0.21 → 0.17; through the DSP: **guitars 0.16** (2/39 swinging), keys 0.14, but voice 0.11, drums 0.33, bass 0.15 | the context makes the mask bolder, not cleaner: more rear everywhere; the milestone's rear side is met, the voice side is not |
| v11 operating-point sweep | threshold 0.3 / 0.5, sharpening ² / ³, both | voice / guitars: 0.05 / 0.07, 0.01 / 0.03, 0.05 / 0.07, 0.03 / 0.04, 0.02 / 0.03 (raw 0.09 / 0.13) | every point sits on the same line, guitars ≈ 1.4 × voice: the ranking cannot separate better at any threshold; the classifier, not the operating point, is the limit |
| v11, rear = "other" only | routing-time role choice (cymbals no longer eligible) | drums 0.28 → **0.17**, guitars 0.13 → 0.12, voice 0.09 → 0.08 | the drum-core leak was the cymbals class; "other only" is the routing default from here |
| v14 | v13 warm-started, softmax CE + ramped leak / retention terms (astra point 5), rear = other only at routing | mask space after the ramp: vocal-owned 0.035, other-owned 0.21; **through the DSP: voice 0.02, guitars 0.06 (8/39 swinging), keys 0.07, drums 0.16, bass 0.07** | the ramp lands exactly where the veto lands (0.02 / 0.06): protection bought with retention, same information |
| v13 + the per-band vocal veto on top (`--mask-keep-gate`), rear = other only | the old gate as a hard multiplier over the driven mask | voice 0.11 → **0.02**, guitars 0.16 → 0.06, keys 0.07, drums 0.15, bass 0.06; 4/39 swinging | the FIRST lever that cuts voice more than guitars (5.5× vs 2.7×; the threshold sweep cut both evenly): band-level presence is real evidence. Retention is the price → soft α (`--vocal-gate 0.5 / 0.7`, astra's `e·(1−α·p)`) queued |
| v11 + the veto on top, rear = other only | same lever on the weaker net | voice 0.01, guitars 0.05, keys 0.06, drums 0.14, bass 0.05; **0 swinging songs in any stem** | the veto + driven mask is the most *stable* renderer measured so far; only the rear amount is missing |
| v16 | conv2048 under the softmax loss (read through the softmax) | **AUC 0.76 / vocal-vs-other 0.82 — the best ranking so far** — but the probabilities stay flat (vocal-owned 0.46 / other-owned 0.52 eligibility); DSP: voice 0.07, guitars 0.08, drums 0.18, nothing swings | ranks well, refuses to commit: the 23 Hz front-end sees the difference, the objective/capacity did not turn it into a mask. v15 (TFNet) crashed on a kwarg, re-queued behind v20 |
| v13 + the veto as a soft multiplier (`--vocal-gate` α) | α = 0.5 / 0.7 | voice 0.04 / 0.03, guitars 0.09 / 0.08, keys 0.09 / 0.08, drums 0.17 / 0.16, 2/39 swinging | the soft gate lands at guitars ≈ 2.3 × voice — better than the threshold line (1.4×), still short of the rear side |
| v17 | **whole-spectrum net** (astra review 4, point 1): strided frequency conv over all 257 cells → GRU(64) bottleneck → per-cell decoder with a full-resolution skip; 170k params, 86 M MAC/s; softmax loss | AUC 0.75 / **vocal-vs-other 0.82** (with v16 the best ranking measured) but flat probabilities (0.43 / 0.51); DSP: voice 0.08, guitars 0.11, keys 0.10, drums 0.17, bass 0.11, nothing swings | same picture as v16: the conv nets rank, they do not commit. Calibration (`mask_net.py calib`: `e' = σ(a·logit e + b)` fitted on DEV to the true eligible fraction, astra review 3 point 3) is the next number |
| v17 calibrated | Platt fit on DEV (`calib`) | a = 1.03, b = 0.00: **v17 is already calibrated** — its 0.36 in vocal-owned cells is the conditional mean of what it extracts; v16: a = 1.48, b = 0.66, still 0.46 | the flatness is missing evidence, not a temperature |
| **correction** | the training logs re-read: v11 0.41 / 0.55, v13 0.49 / 0.61 in mask space | **every softmax net is flat** — the "sharp 0.06 / 0.27" attributed to v13 above was v14 *after the ramp*. v17 (0.43 / 0.51) is the least leaky of the CE nets in mask space; no GRU state drift (per-segment eligibility identical from a fresh state over 60 s, `diag_drift.py`) | the frontier is the conditional mean of weak evidence; sharpness so far came only from the ramp or the gate, both trading retention |
| v21a | full_probe (context + descriptor, 128 M MAC/s) | AUC 0.76 / 0.82, mask space 0.45 / 0.55; DSP: voice 0.09, guitars 0.13, keys 0.11, drums 0.19 | on the line (1.4×) |
| v17 + learned presence head, α = 0.7 | routing `e·(1−α·p_band)` from the jointly trained band vocal-presence head | voice 0.08 → 0.05, guitars 0.11 → 0.07, keys 0.07, drums 0.14, 0 swinging | the learned presence cuts evenly (1.4× on both sides); the DSP's heuristic veto (comodulation with the voice body) cut voice 5.5× vs guitars 2.7× — the heuristic is the better protector so far |
| v18 | v17 + ramp | mask space 0.036 / 0.15; DSP: voice 0.02, guitars 0.05, keys 0.05, drums 0.17, 9/40 vocal and 6/39 guitar songs swinging | = v14's point exactly: the ramp on the whole-spectrum net lands on the same line, with more swing. (The first stems run hung in `CloseHandle` on the PC's WinBtrfs volume; the harness is now resumable and watchdogged) |
| v21b | v21a + salience-weighted CE | identical: 0.44 / 0.57 mask space, DSP voice 0.09, guitars 0.13 | the loud cells are not the issue; the evidence is weak everywhere |
| v22 | **ceiling study A**: full_probe at 8× width, 2-layer *bidirectional* GRU (non-causal, GPU only), 12 s; stopped at step 2000 of 6000 (the runner restart killed it) | AUC 0.78 / 0.83, train = DEV, mask space 0.40 / 0.51; DSP: voice 0.08, guitars 0.13, keys 0.10, 4/39 swinging | 20× the parameters, non-causal: the same line. Width is not the lever |
| v22b | **ceiling study B**: TFNet at hidden 32 (bidirectional GRU *across all 257 bins* at full resolution + causal GRU over time per bin) under the softmax loss, 12 s | AUC 0.79 / **vocal-vs-other 0.85** (best ranking of all), mask space 0.44 / 0.56; DSP: voice 0.09, guitars 0.13, keys 0.11, drums 0.17, nothing swings | evidence across the bins helps the ranking a little and the mask not at all: the same line. Neither ceiling leaves 0.4 / 0.55 — with these targets and losses no small model commits |
| **train vs DEV** | `diag_overfit.py`: the same evaluate() on 16 TRAIN songs vs 16 DEV songs | v11 0.74 / 0.66, v13 0.79 / 0.76, v17 0.80 / 0.75, v21a 0.81 / 0.76, v22 (3.7 M, bidir, step 2000) 0.81 / 0.76 AUC; mask space flat on TRAIN too (vocal-owned 0.39–0.49) | **not data starvation: every net underfits its own training set.** A 3.7 M-param non-causal net fits no better than the per-cell probe. htdemucs (40 M params, 4096-point hybrid, trained on ~150 songs) reaches at the same STFT what none of ours fits — capacity + training, not information, is the wall |
| v20 (revised) | full_probe + presence, 12 s, 8000 steps, ramp from 5000, **teacher VOCAL distillation** (`--teacher-mode vocal`: the student's vocal probability against htdemucs's vocal fraction in every audible cell, salience-weighted, every step, weight 1.0, the whole library) | queued behind v22b | the teacher supplies the sharp cross-spectrum voice evidence over 220 h of real masters; drums / bass / other roles still from MoisesDB only |
| v23 | full_probe with the 4-hop mean target | AUC 0.78 / 0.81, mask space 0.47 / 0.54; DSP: voice 0.08, guitars 0.12, keys 0.11 (38 songs, 2 renders failed on the PC's volume) | the smoother target changes nothing |
| **v20** | full_probe + presence, 12 s, batch 4, 8000 steps, ramp from 5000, **teacher vocal distillation** on 2908 real masters every step (weight 1.0) — ran to the end under the training guard without a crash | mask space after the ramp **0.047 / 0.25** (best 6250) — the best mask-space trade so far (v14 0.035 / 0.22, v18 0.036 / 0.15); DSP: voice 0.02, guitars 0.08, keys 0.09, drums 0.18, bass 0.09 — but 7/39 guitar and 11/40 vocal songs swinging; presence α 0.5: 0.02 / 0.07, 9 + 7 swinging | the teacher moves the point a little off the line (guitars ≈ 4 × voice instead of 3 ×) and buys nothing on the milestone's rear side; the vocal channel alone does not teach the student what *other* is |
| v20 before the ramp | steps 1000–5000 | eligibility **0.84–0.93 in vocal-owned cells** (v17: 0.43) while AUC stays 0.70 / 0.82 | the vocal-only distillation broke the role calibration: pushing p_vocal down where the teacher hears no voice sends the softmax mass to *other*, so before the ramp everything is eligible. Distilling one channel of a 5-way softmax is wrong; the teacher must supply the whole target → v24 |
| **htdemucs's own mask through the DSP** (`--teacher-mask`, the excerpt separated by the teacher, eligibility = its *other* stem, amplitude, G = 1.0) | the ceiling of the distillation route | **voice 0.03, guitars 0.20 (top bands 0.29–0.41), piano 0.14, keys 0.16, bass 0.11, drums 0.11; 8/39 guitar songs swinging** | **= the oracle** (0.03 / 0.20 / 0.14 / 0.17 / 0.07 / 0.12). The 40 M-param teacher extracts from the mixture everything the milestone needs — the evidence is in the signal at the car's STFT; what is missing is a student that gets it out. The distillation route has a real, measured target |
| v24 | v20 recipe with **role distillation** (`--teacher-mode roles`): the full 5-way soft target from a four-stem htdemucs cache (vocals / drums / bass / other, 600 real masters, cymbals fold into drum_core) every step | stopped at step 750: eligibility 0.78 → 0.89 in vocal-owned cells from step 250 (v17 alone: 0.45) — the same drift as v20 | the teacher's "other" is a residual (master − stems) carrying every source's leakage, so unconfident teacher cells teach "other is everywhere" — exactly the Codex warning. Distil only where the teacher is confident → v24b |
| v24b | v24 + `--teacher-conf 0.8`: teacher cells count only when one role holds > 0.8 of the cell | stopped at step 750: still 0.77 → 0.87 → 0.82 in vocal-owned cells | not the teacher's uncertainty: a real master is mostly *instrumental* cells, so the confident teacher cells are a flood of "other" positives — a class-prior shift, which a weak discriminator (AUC 0.8) obeys |
| v24c | v24b + `--teacher-balance`: every dominant role gets the same total weight in each teacher batch | stopped at step 1000: 0.79 / 0.73 / 0.82 / 0.82 — the same drift | not the prior either. `diag_domain.py` (step ~750 snapshot vs v21a): eligibility in vocal-owned cells rises on MoisesDB dev (0.41 → 0.64), train (0.47 → 0.71) *and on the teacher's own songs* (0.34 → 0.46): every teacher recipe pushes "other" up everywhere |
| v25t | **diagnostic**: train on the teacher's labels ALONE (softmax CE everywhere, 449 real masters), evaluate on MoisesDB DEV | **0.34 / 0.38** in vocal- / other-owned cells, AUC 0.63 / 0.77 — no drift at all | **the teacher was never the cause.** What v20 and v24a–c shared was `--loss softmax_ramp` from scratch with `ramp-start 5000`: the *retention* term (`missed`, eligible energy held back) was **not on the ramp** — 5000 steps of "maximise eligibility in eligible-owned cells" with no leak counterweight, which a weak discriminator satisfies by making everything eligible (0.8–0.9). Every "distillation drift" of the day was this term. Fixed: `missed` rides the ramp |
| v26a | clean role distillation (confident + balanced teacher cells) under plain softmax CE, 4000 steps | mask space 0.44 / 0.55, AUC 0.74 / 0.83 (= v21a); **teacher-vs-student on identical cells** (`diag_copy.py`, 12 held-out masters, 14.7 M audible cells): correlation with the teacher's other-share 0.34 → **0.61**, weighted MAE 0.20 → 0.16 — but where the teacher says e > 0.8 (7.8 % of cells) the student says 0.37 on average and exceeds 0.8 in 0.7 % of cells (v21a: 0.33, 0.0 %) | KD makes the student *follow* the teacher (corr ×1.8) without letting it *commit*: the conditional mean rises from 0.33 to 0.37 in the teacher's confident cells. DSP pending |
| v26a through the DSP | | voice 0.09, guitars 0.12, keys 0.11, drums 0.19, bass 0.13, 2/39 swinging | = v21a: the clean role distillation changes nothing the renderer can use |
| v26b | v26a + the fully ramped leak / retention terms | mask space **0.052 / 0.22** (best 1750), AUC 0.72 / 0.75; DSP: voice 0.02, guitars 0.06, keys 0.07, drums 0.19, bass 0.08, 6/39 guitar and 10/40 vocal songs swinging; presence α 0.5: 0.02 / 0.05 | the ramp lands where it always lands (v14 0.02 / 0.06, v18, v20): the clean role KD did not move the trade. Role distillation is closed; v27 (direct eligibility) is the last distillation form |
| v27a | **direct eligibility distillation** (astra 6): Huber on sqrt(e) + 0.2 BCE + 0.1 role CE, energy-weighted control target, 4000 steps | mask space 0.49 / 0.60 (best-AUC ckpt 0.44 / 0.52), AUC 0.75 / 0.84; DSP: voice 0.08, guitars 0.12, keys 0.11, drums 0.18, 2/39 swinging; rear error vs oracle 0.21 (gain-fit 0.17) | the direct target lifts the other-owned eligibility to 0.60 (highest so far) but the vocal-owned one to 0.49 with it: the same line. Every distillation form (vocal channel, roles, eligibility) now measured on this student |
| v27b | v27a + the ramped terms | mask space 0.045 / 0.21 (best 0.040 / 0.195); DSP: voice 0.02, guitars 0.06, keys 0.06, drums 0.16, bass 0.07, 5/39 swinging; rear error vs oracle 0.31 (gain-fit 0.29), treble 0.12 | the ramp's point once more. **Every distillation form on the 128 M student is now measured: vocal channel, roles (raw / confident / balanced), eligibility — the same line.** |

| v29a / v29b | **the frequency-modelling student** (astra review 6, point 2): strided freq convs 257→129→65 over features + descriptor, bidirectional GRU across the 65 bins (hidden 40/dir), causal GRU over controls per bin (16), full-resolution decoder with the 13 local inputs — 28k params, **210 M MAC/s** (the budget with the two controllers retired); role distillation (confident + balanced, 774 four-stem masters), softmax 5000 steps → ramp 2000 | v29a: **AUC 0.80** / 0.85 (best of all students), irm_mae 0.154 (best), mask space 0.42 / 0.55; DSP: voice 0.08, guitars 0.12, keys 0.10, drums 0.19, bass 0.13, 2/39 swinging; rear error vs oracle 0.19 (gain-fit 0.15), treble 0.09 / 0.07 (best). v29b (ramp): mask 0.042 / 0.196, DSP voice 0.02, guitars 0.05, keys 0.06, drums 0.19, 5/39 swinging, rear error 0.30 | the frequency student ranks best and reconstructs the oracle's rear best, and its rear share through the DSP is the same line as every per-cell student: better evidence, still no commitment; the ramp lands where it always lands |
| v29r11 | the ramp weight sweep (astra 7 point 5): leak : missed = 1 : 1 on the frequency student | mask 0.12 / 0.33; DSP voice 0.04, guitars 0.08, keys 0.08, drums 0.19, 5/39 swinging; rear error 0.21 (gain-fit 0.19) | a point between raw (0.08 / 0.12) and the 3 : 1 ramp (0.02 / 0.05) on the same line: the weights choose where on the line, nothing more. v29r12 (1 : 2) pending |
| diag7 a / b / c | **the confidence diagnostic** (astra 7): the frequency student, teacher-only on 4 fixed masters, evaluated on the same 4, no augmentation, 1500 steps; a = as is, b = no weight decay, c = b + pure/mixed strata balanced within each role | all three: AUC 0.93 / 0.96, target entropy H = 0.52, **CE − H = 0.27** (c: 0.30), logit std 2.1, pre-clip gradient norm 0.4, **clipping 0 %**; eligibility in other-owned cells 0.36 / 0.37 / 0.46, vocal-owned 0.21 / 0.21 / 0.26 | b = a: not weight decay, not the decoder's scale, not clipping. c lifts both sides a little: weighting moves the operating point, not the fit. None of the arms closes the 0.27-nat gap on data it has seen 40 times. By the review's rule this points past optimisation and weighting — to the inputs / representation (or to a plumbing fault still unfound: target alignment for `ewmean` is 3 hops off the feature hop by construction) |

**State at the end of 2026-09-26.**  The oracle and the teacher prove the
target (guitars 0.20 at voice 0.03 through this renderer).  Twenty-nine
students at ≤ 176 M MAC/s — six front-ends, three ceilings, five losses,
three distillation forms, two operating-point families — sit on one
frontier, and the overfit test says why: they rank (AUC up to 0.96 when
memorising) but never become confident, because nothing in their input tells
them how *pure* a cell is.  The classical matrix ties them on rear share and
loses by an order of magnitude on the oracle-referenced rear error, which
is the metric that now separates routing from width.  What is deployable
today: the mask-driven renderer with v13 + soft veto 0.5 (voice 0.04,
guitars 0.09, 2/39 swinging, rear error 0.15 vs the car candidate's 0.41
and 24/39).  What is left to try for the confident rear: the 230 M student
built for frequency modelling (review 6, point 2) — the only untested lever
that could carry a purity cue — and the listening study that decides what
"confident" means by ear.
| v25 (foreign run on the PC, 26.09 16:23) | TFNet hidden 32 + role distillation + ramp from 5000, 8000 steps | pre-ramp 0.48 / 0.61, **AUC 0.77 / 0.87** (best vocal-vs-other ranking measured); after the ramp **0.053 / 0.14** (best 7250), AUC 0.72 / 0.66; DSP: voice 0.02, guitars 0.05, keys 0.07, drums 0.18, bass 0.03, 7/39 swinging | launched by the previous instance of this session during the 16:05 restart (same script template); the bigger TF student with the clean ramp lands at v18's point — the best ranker still cannot commit, and the ramp trades it away as always |
| v28o | **overfit test** (astra 6): teacher-only on 4 masters, evaluated on the same 4 | died at step 1250 (host RAM); at 1000–1250: **AUC 0.92 / 0.96** on the memorised songs, yet eligibility 0.32–0.34 in other-owned cells, 0.21–0.24 in vocal-owned. `diag_softmax.py`: in teacher-dominant cells (target ≈ 0.9 on one role) the student's softmax is ≈ 0.45–0.57 on that role and 0.2–0.25 on each of two others — *the same on its own training songs and on held-out ones* (v26a held-out: other-dominant → p_other 0.51, vocal-dominant → p_vocal 0.44) | **the student ranks (AUC 0.96 when memorising) but never becomes confident, not even on 4 songs it has seen 65 times.** The balanced owned-cell AUC does not involve the mixed cells; relative to *them* the dominant cells are not distinguishable, so the softmax settles near the mixed-cell mean. What the student lacks is a *purity* cue (how much of the cell is one source) — exactly what eligibility (a = sqrt e) needs and what a separator has. v27's direct sqrt-eligibility target attacks this head-on |
| v20 (launches 1–3) | | died within ~200 steps each time: access violation in `c10.dll`; `faulthandler` shows the faulting thread has no Python frame (torch / driver native thread) — the GPU is shared with demucs and two unkillable zombie processes hold CUDA contexts. Not the mmap (removed anyway: cache windows are now plain file reads), not the growing file (json marker). Mitigation: `train_guard.ps1` restarts from `last.pt` (optimizer state, `--start-step`) until `train_done.txt`; batch 4 | the PC needs a reboot |
| v21a / v21b | **full_probe**: the whole-spectrum context (skip + local + global) *and* the proven per-cell descriptor feed a 32-wide decoder (128 M MAC/s); v21b adds salience-weighted CE (loud cells weigh more, like the DSP metric) | queued | does context + sharp per-cell input beat both parents? |
| v18 | v17 + ramped leak / retention | queued | |
| v20 | **the no-compromise run**: whole-spectrum net + band vocal-presence head (routing `e·(1−α·p)`, 300 ms release), 12 s sequences, 8000 steps (5000 discrimination, then the ramp), teacher protection-only on the whole library (`D:\music`, 3367 real masters through htdemucs_ft, compact `[vocals, master − vocals]` cache, ≥ 600 songs at start) | queued | Phase 2 at scale; presence α swept 0 / 0.5 / 0.8 at routing |

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

### Review 4 (astra, at the frontier)

Asked with the whole state of play after v13/v14: every model and every
operating point sits on one line, guitars ≈ 1.4 × voice, and the milestone's
voice side (≤ 0.04) is missed by ~3×.  The diagnosis: **missing
cross-frequency information** — but "AUC is not accuracy", the frontier is not
shown to be irreducible, and v13's better ranking may still help at matched
leakage.  Ranked:

1. **Whole spectrum + temporal memory at a shared bottleneck (A1 + A3), first.**
   Log power, ILD, cos/sin IPD over all 257 cells → kernel-5 stem (4→16 ch) →
   three depthwise-separable stride-2 layers (16→32→48→64) → flatten 33×64 →
   64 → GRU(64,64) → per-cell decoder on [16-ch full-resolution skip, local
   context, global context] (48→24→5).  ≈ 84 M MAC/s neural, room below 150 M
   incl. features and quantisation.  Train on multi-second sequences with
   state warm-up; a long receptive field must actually connect distant
   harmonics.  Acceptance stays guitars ≥ 0.15 at voice ≤ 0.04; no forecast.
   → **v17** (`--arch full`, 170k params, 0.92 M MAC per control = 86 M MAC/s).
2. **Run the vocal-veto A/B now; a learned presence head second (A4).**
   "Presence × eligibility" has the wrong polarity; the rule is
   `e' = e_cell · (1 − α·p_presence)`, trained jointly, smoothed with fast
   attack / ~300 ms release, applied before `a = sqrt(e)`.  It is the old veto:
   it cannot create rear content, only protect a mask that is already useful.
3. **Teacher data now, conservatively (A5, C(a)).**  Interleave real-master
   examples; on confident teacher-vocal cells a low-weight protection loss,
   uncertain cells masked out; teacher silence is never permission.  Audit
   the 94 % precision by energy and frequency (treble).  No full teacher-role
   training (14 % of the vocal energy in "other" teaches the exact failure).
   → **v19** (`--teacher-cache`, every 2nd step, weight 0.3, cells with
   teacher vocal share > 0.9 only).
4. Explicit harmonic features only after the shared-context baseline (A2):
   guitar is harmonic too; pitch trajectories + envelope + position together
   discriminate, harmonicity alone does not.
5. Loss changes are operating-point tools (A6): finish v14's DSP run
   (mask-space averages do not predict rendered leakage), mine high-energy
   vocal false permissions, keep a supervised retention term.

On D: 0.08–0.11 own-cell energy in the rears ≈ −11 … −9.6 dB, audible as
spaciousness or vocal wash with the 12 ms delay depending on the cabin; keep
≤ 0.04 pending level-matched listening; delay / high-pass changes buy no
selective 2×.  **Stop:** threshold sweeps, more isolated per-band memory,
selecting on the 40 DEV songs (TEST stays untouched).  Re-run the oracle with
other-only eligibility — already the case: the oracle protects the drums
stem whole, so its 0.20 / 0.03 / 0.12 ceiling *is* the other-only ceiling.

Queued behind v15/v16 (`run_v17.ps1`): v17 full-spectrum softmax → DSP; v18 =
v17 + ramp → DSP; v19 = v18 + teacher protection-only → DSP.

## 🛠️ Phase 1 — into the DSP

New blob `HSSRMSK1`, a second job on the existing control worker (own state,
generation reset, delayed-dry fallback), mask applied in the render loop
where the gate is today; QEMU instruction count and a real Krait bench
(`tools/stereo-upmix-render/bench/`) before any car run.  Float parity ≤ 2 LSB
with the host renderer (`-ffp-contract=off`).

**Done 2026-09-26 — the float reference.**  `hook/audio/hssr_mask.{h,c}`
implements the `full_probe` student operation for operation (standardised
features → stem conv → three depthwise-stride-2 stages → flatten → GRU(64) →
global / local / presence heads → per-cell decoder → softmax, eligibility =
p_other); no allocation, no locks in `spa_mask_infer`; blob `HSSRMSK1` =
magic + u32 count + float32 arrays (`mask_net.py export-blob`, 173 711
floats = 0.69 MB for the 128 M MAC/s student).  Parity against PyTorch on a
synthetic scene (`mask_net.py dump-features` → `mask_infer_test`): 749
controls, **max |Δ eligibility| 3.6 × 10⁻⁷**.

**Done 2026-09-27 — the production path (Track A).**  The mask net is wired
into the car's pipeline, float, behind two env knobs that default to off:
`AUDIO_HSSR_MASK_MODEL` (else `hssr_mask.bin` beside the model, absent = off)
and `AUDIO_HSSR_MASK_DRIVE` (0..2, 0 = off, 1.0 = the measured operating
point).  Per control the audio thread extracts the 257 × 4 per-bin features
of the control hop (log|L|², log|R|², cos/sin IPD — `mask_net.py
features()`), the control worker runs the mask net after q6d and the guard
(job / result extended, state reset on generation change), the audio thread
applies the result one control later like the GRU controls: the object mask
becomes `sqrt(e) · drive` with the bin's own balance as its pan, the per-band
vocal gate stays on top (= the harness's `--mask-drive 1.0 --mask-keep-gate`),
and three missed controls hand the object back to the heuristic path (never
a silent rear).  Renderer `--mask-model FILE --mask-drive-prod G` and bench
`--mask-model` exercise the same code; host parity with the test path on a
synthetic scene: rear RMS difference 4 % (the test path's mask comes from
Python's whole-file STFT, the production path's from the DSP's own hop
analysis).  The QNX hook builds (`libcarplay_hook.so`, emutls = 0).  **QEMU instruction count (2026-09-27, `qnx_qemu_bench.sh --variant latest
--async-controller`, 1024-frame blocks = 21.3 ms):** baseline 4.54 M
instructions per block; with the float mask net **30.6 M** — the mask alone
costs ~26 M instructions per block ≈ **1.22 G instructions/s** in scalar
float, i.e. roughly a whole Krait core.  The float reference is therefore a
parity tool, not the runtime.  Where it goes: the per-cell decoder (57→32→32→5
for 257 cells ≈ 0.8 M MAC per control = 60 % of the net) — a [257 × 57] ×
[57 × 32] matrix product per control, ideal for an int8 NEON GEMM (8 cells per
vector); the convs / flat / GRU (~0.5 M MAC) fit float NEON.  Phase 1b so far (same bench, per 1024-frame block): float 30.6 M → int8
decoder (per-row weight scales, one dynamic input scale per control, NEON
`vmull.s8`/`vpadal`; vs float max |Δe| 0.017, corr 0.99991) 25.3 M → + NEON
float taps for the convs / gemvs (`axpy` / `dot` with `vmlaq_f32`) and a
rational tanh (|err| < 2e-3, 16k calls per control) **12.5 M** — the mask now
costs ~8 M instructions per block ≈ 4 M per control ≈ 3–4 ms of a Krait core
per 10.67 ms control period: feasible on the worker with ~2.5× margin, not
yet frugal.

**Phase 1b, the profile and the second pass (2026-09-27 12:30).**  A QEMU
TCG plugin (`bench/qemu_fnprof_plugin.c`, per function and per source line)
against the same run without the mask put the 3.9 M instructions per control
where nobody would have guessed: the scalar int8 quantiser of the decoder
inputs 0.7 M (31 k calls, branches and converts), the scalar rational tanh
0.4 M (30 k calls), the int8 dots 0.5 M, the decoder's float glue 0.3 M, the
float convs as `axpy` (load + load + mla + store per 4 MACs) 0.6 M, the
depthwise stride-2 taps with bounds checks 0.2 M — and **0.5 M in libm**: QNX
6.5's `expf` / `logf` run in software double precision on this softfp build
(~180 instructions a call; the feature prep, the softmax and the feature
extractor made ~2.8 k calls per control).  The second pass, all in
`hssr_mask.c` (+ two lines in the extractor): float-epsilon polynomial exp /
log (degree-6 on [−½, ½] with the exponent in the float bits; the atanh
series of the mantissa, |err| ~1e-7) for every transcendental; the quantiser
16 lanes at a time (`vcvt` after a sign-copied ½, saturating narrows, bit-
identical to the scalar rounding); the tanh 4-wide (same rational form, `vrecpe`
+ 2 Newton steps); the decoder in 4-row groups over the input chunks kept in
registers (`vmull.s8` + `vmlal.s8` before one `vpadal`, horizontal sums
pairwise, the scale · dot + bias and the tanh 4-wide); the convs in direct
form over zero-padded rows (one load + one `vmla` per 4 MACs, the last block
of a row recomputed at n−4 instead of a scalar tail), the stride-2 depthwise
via `vld2` + `vext` lane windows; the gemvs 4 rows per pass sharing the input
loads.  The scalar path (host, PC harness) accumulates in the same order, so
the NEON path differs only by reduction lane order and the reciprocal
estimate.  **Result: 12.5 M → 8.47 M per block, the mask net 3.92 M → 1.97 M
instructions per control** (~1.3–2 ms of a Krait core per 10.67 ms at IPC
1–1.5).  Parity: host runtime vs PyTorch unchanged (max |Δe| 0.017 = the int8
quantisation, mean 0.001, corr 0.99991); new vs previous host build max 8e-4,
mean 6e-8; **NEON (QEMU) vs scalar (host) max |Δe| 1e-6** on 12 controls
(`--mask-selftest`, `bench/mask_selftest_compare.py`).  Left on the table
(~0.15 M): 4-output-row grouping of the pointwise / stem convs with
transposed weights (`vmla` by lane instead of `vdup` + `vmla`), and, if the
Krait bench shows the 540 KB flat-layer weight stream missing L2, int8 for
`flat_w` (135 KB).  What QEMU can still say about memory (`qemu_l1d_plugin`,
one 16 KB / 4-way / 64 B L1D, the Krait geometry): the mask adds per control
937 k data accesses and 24 k L1D misses (2.5 %, the same rate as the DSP
itself) = 1.47 MB of line fills per control ≈ 138 MB/s — the 0.69 MB float
model plus the ~200 KB of stage scratch stream through L1 once per control;
whether they stay in the 1 MB L2 next to the DSP is the one question only
the Krait bench answers (bandwidth-wise it is nothing, latency-wise the
sequential streams are prefetchable).

**Phase 1b, third pass (2026-09-27 13:30, commits 6d667c7).**  Per-line
attribution at -O3 is unreliable (inlined intrinsics land on `arm_neon.h`),
so the next cut came from the function counts plus `objdump` histograms: the
pointwise stage spent 1.6 instructions per MAC (`vmlaq_n` = `vdup` + `vmla`
plus the address arithmetic per 4 MACs), and the decoder's first layer, once
macro-expanded over 32 rows, spent 601 of its 1787 instructions per cell on
`vldr`/`vstr` — register spills.  (1) The convs now run four output rows per
pass over weights transposed at load time (`[input][output]`, one vector
load of four rows' weights, `vmla` by lane): the same per-row accumulation
order, bit-identical output.  (2) The decoder layers as 4-row loops with the
input chunks in registers and the float results parked in a 32-float buffer
(`#pragma GCC unroll 1`): spills gone.  (3) The flat layer int8: its input is
the last stage's tanh output (fixed scale 127 like decoder layers 2/3), per-
row weight scales; vs PyTorch max |Δe| 0.0168, mean 0.0011, corr 0.99991 —
the same as before — and 540 KB → 135 KB of weights per control.  (4) Cells
more than 60 dB below the control's loudest bin are not decoded and keep
their last eligibility (`SPA_MASK_SKIP_DB`, compile-time, 0 = off; a bin at
−60 dB cannot produce an audible rear; 29.8 % of the cells on the synthetic
scene, the harness measures the effect on music with a skip-0 build beside
it).  **1.97 → 1.77 → 1.46 M instructions per control** (blocks 8.47 → 8.07
→ 7.45 M; the white-noise bench barely skips, so (4) is not in that number);
NEON vs scalar max |Δe| 4e-5 (a 1-LSB int8 flip on the flat input now and
then, through the GRU).  The L1D model: 649 k accesses per control (was 937
k) but 23 k misses (was 24 k) — with 16 KB the misses are the scratch
buffers re-touched between stages, not the weights; what shrank is the
unique working set per control, ~1.0 MB → ~0.6 MB (weights 0.29 MB float +
0.14 MB int8, state ~0.2 MB), which is the L2 question on Krait.  The 40-song
production-path A/B of this pass (`prod10a` skip 0, `prod10b` skip 60 dB, both
int8 flat): both rows equal `prod9` — voice 0.04, guitars 0.07, keys 0.08,
rear −37.6 dBFS, error 0.16 / 0.15; with the skip one vocal swing fewer (1/40)
and bass max 0.14 vs 0.16.  The skip and the int8 flat layer ship.

**On the unit (2026-09-27 19:00, Krait 1512 MHz, `stereo_upmix_bench` in
/ramdisk, 400 blocks of 1024 frames).**  Synchronous (the nets inside the
audio thread): 4.67 ms per block without the mask, 7.33 ms with it → **the
mask net costs 1.33 ms per control** (2 controls per block; 1.46 M
instructions → IPC ~0.7; the timer's grain is ~1 ms, so the means are the
numbers).  With the production layout (`--async-controller
--realtime-pacing`): the audio thread stays at 4.0 ms per block with or
without the mask (+1 %, the feature extraction), and the worker completed
839 of 839 control jobs, busy 0, stale 0 — no late control in 8.5 s of
paced operation with all three nets.  Against the failsafe threshold (three
late controls ≈ 32 ms) that is a 24× margin; on music the 28 % cell skip
takes it to ~1.2 ms.  Deployed over SSH the same evening (hook, blob,
startup, `carplay_ncm_cleanup.sh`, jar updated; 12 files byte-identical;
every file read back and compared); the unit reboots on Luka's decision.

**In the car (2026-09-27 evening).**  First boot with the mask: "the rear is
almost gone".  The health line (new: `HSSR health rt: controls lateNow
failsafeTotal maskReady maskLateNow maskFallbacks worker busy stale`, beside
the periodic sink stats) showed `maskReady=0` with every worker job on time:
`spa_stereo_upmix_reset()` memsets the runtime and never handed the guard and
mask model pointers back (init did), and the hook resets on every stream
start — **the mask net had never run in the car**; the PC harness never
resets.  Fixed (c43a17c, regression test fails on the old code).  With the
mask live: the same complaint, quieter — "instruments were vivid in 22.09,
they flew, but they were there".  The 22.09 config through the harness
(`car_0922`, the row the ear remembers): guitars 0.17 rear, keys 0.18, drums
0.26, **voice 0.08 with 8/40 songs above 15 %**, rear −36.3 dBFS, 27/39 guitars
swinging — the mask at drive 1.0 sends back less than half of every
instrument (guitars 0.07) at almost the same level (−37.6).  The drive curve
(40-song harness, gate 0.5, air −9; "rel" = 1-s range / the stem's own rear
share, the level-independent swing; the `> 0.30` count grows with the level
by construction and misled the morning's "2/40"):

| drive | guitars | voice (>15 %) | drums | rear dBFS | guitars rel swing | voice rel |
|---|---|---|---|---|---|---|
| 22.09 heuristic | 0.17 | 0.08 (8) | 0.26 | −36.3 | 2.35 | 4.8 |
| 1.0 | 0.07 | 0.04 (0) | 0.17 | −37.6 | 2.20 | 3.1 |
| 1.3 | 0.13 | 0.06 (0) | 0.26 | −35.6 | 2.03 | 3.1 |
| **1.5** | **0.17** | **0.08 (4)** | 0.30 | −34.4 | **1.78** | 3.0 |
| 1.7 | 0.21 | 0.10 (8) | 0.33 | −33.6 | 1.64 | 2.9 |

Level-matched (drive 1.5 = the 22.09 shares), the mask path's guitars swing
a quarter less relative to their level (0.34 vs 0.48 absolute range) and the
voice a third less, with half the songs above 15 % voice (4 vs 8) at the same
voice share; it is not the "never swings" the thin drive-1.0 row suggested
(all rows final on the 40 songs).  Two more knobs were built and rejected on the harness: a
treble shelf on the drive (`AUDIO_HSSR_MASK_TREBLE_DB`, +3/+6: drums 0.44/
0.50, voice 0.10/0.12 — cymbals and voice sit in the "other" cells' highs)
and an eligibility gamma (`AUDIO_HSSR_MASK_GAMMA` 0.7/0.5: voice 0.14/0.16 —
the student's voice/guitar contrast, 0.41 vs ~0.55, is too small to sharpen);
surround 0.5 + rear body 0 at drive 1.3: voice 0.12.  **Operating point
shipped in the evening: drive 1.5, rear air −9 (the mask rear was 1.6 dB
darker than 22.09 above 2.5 kHz), gate 0.5**; the car says "on rock songs
there is something behind, still little" at 1.3.  The ceiling is the
student's (Phase 0): the fullness the ear wants at 22.09's stability or
better needs the separator (Track B).  On the unit: 1.33 ms per control, the
worker never late over 30 k controls, no failsafe, no fallback.

**The production path validated on the 40 DEV songs (2026-09-27 12:00,
`--mask-model v21a.msk1 --mask-drive-prod 1.0 --vocal-gate 0.5`).**  Test
path (Python STFT mask, `--inject student`) and production path (the DSP's
own hop features → the C runtime) give the **same row**: voice 0.04 (0/40 >
15 %, 2/40 swinging), guitars 0.07, keys 0.08, piano 0.04, bass 0.07, drums
0.17, rear −37.5 dBFS, rear error vs the oracle 0.16 (gain-fit 0.15); the
int8-decoder build reproduces it to the second decimal (rear −37.6, the rest
identical).  The pick: v27a + veto 0.5 ties on share (voice 0.04 / guitars
0.07 / keys 0.07) but swings more (voice 4/40, guitars 3/39, drums 3/40 vs
2/40, 1/39, 0/40); v29a + veto 0.5 the same story (voice 0.03 / guitars 0.07
/ keys 0.08, swinging 3/40, 3/39, 2/27, drums 1/40) — **v21a stays the
blob**: the same shares with the fewest swings.  The rerun with the
Phase 1b runtime (fast exp/log, clamped tanh, padded convs, int8 decoder —
the PC build of the exact shipped arithmetic, `prod9`) gives the identical
row again: voice 0.04 (2/40), guitars 0.07 (1/39), keys 0.08 (1/27), rear
−37.6 dBFS, error 0.16 (gain-fit 0.15).  Next: the Krait bench on
the unit (`stereo_upmix_bench --mask-model`, the QNX build), then the car
install (blob beside `hssr_model.bin`, `AUDIO_HSSR_MASK_DRIVE=1.0`,
`AUDIO_HSSR_VOCAL_GATE=0.5`).

## 📏 Classical baselines through the same harness (2026-09-26, `baseline_upmix.py`)

Reviewers will ask (astra review 5); the answer is humbling.  40 DEV songs,
the same per-stem rear-share metric:

| | vocals | guitar | piano | keys | bass | drums | swinging |
|---|---|---|---|---|---|---|---|
| **passive matrix** (S = (L−R)/√2, 7 kHz LP, 12 ms, −3 dB) | 0.05 | **0.13** | 0.19 | **0.21** | 0.02 | **0.05** | guitars 4/39, vocals 7/40, keys 4/27 |
| primary/ambient (Avendano–Jot, simplified: a = 1 − coherence) | 0.02 | 0.06 | 0.11 | 0.12 | 0.02 | 0.07 | drums 18/40, vocals 12/40 |
| our best student (v13 + soft veto 0.5) | 0.04 | 0.09 | — | 0.09 | 0.09 | 0.17 | 2/39 |
| our best raw student (v21a / v22b) | 0.09 | 0.13 | 0.08 | 0.11 | 0.12 | 0.19 | 0–1 |
| htdemucs mask | 0.03 | 0.20 | 0.14 | 0.16 | 0.11 | 0.11 | 8/39 |
| oracle | 0.03 | 0.20 | 0.14 | 0.17 | 0.07 | 0.12 | 7/39 |

**The companion metric answers (astra review 6, `--oracle-ref`): rear
reconstruction error against the oracle's Ls+Rs from the same renderer,
latency-aligned, normalised squared error, median over the DEV songs:**

| | broadband | after one gain fit | treble 2–16 kHz | gain-fit |
|---|---|---|---|---|
| passive matrix | **5.59** | **0.99** | 2.41 | 1.00 |
| v13 + soft veto 0.5 | 0.15 | 0.15 | 0.09 | 0.08 |
| v21a (raw student) | 0.23 | 0.15 | 0.12 | 0.08 |
| the car candidate (heuristic path, master 0ffe8eb) | 0.41 | 0.36 | 0.25 | 0.17 |

The matrix's rear shares *nothing* with the oracle's: after the best possible
gain it is as far from the target as silence (error 1.0), and 5.6× too loud
before it.  The students' rears are within 0.15 of the oracle waveform; the heuristic
candidate that is going to the car sits at 0.41 (0.36 after gain) with 24/39
guitar and 29/40 drum songs swinging — the mask-driven students are both
closer to the oracle and an order of magnitude steadier.  So
the rear-share tie was the metric's blind spot, not a real tie: the matrix
extracts width, the mask routes sources.  Both numbers go into the paper.

A 1970s difference-signal matrix sits on our students' frontier by the
rear-share metric — more keys, far less drums and bass, voice 0.05 by construction
(centre-panned sources cancel).  The metric cannot tell a routed source from
a difference-signal wash (anti-phase artefacts, mono incompatibility, reverb
and width riding along), which is exactly why the listening study is the
missing experiment and why "stable at useful rear energy" must be judged by
ear.  For the paper: the matrix is the baseline to beat *perceptually*; on
the objective metric only the teacher and the oracle beat it.

## 🎚️ Depth layering (per-source depth) — after the single-class student

Today one class goes rear (`other`: everything but vocals / drums / bass) at
one depth (G = 1.0); instruments inside it differ only in pan (each cell keeps
its own L/R balance), never in depth.  Real 5.1 mixes layer the scene: pads /
strings / backing deep, keys mid, lead guitar near the front.  Two ways:

1. **By instrument class** — split `other` into `guitar / keys / rest` with a
   G per class.  Deferred: the student cannot yet separate voice from the
   rest; guitar-vs-keys is harder; htdemucs (4 stems) cannot label it
   (`htdemucs_6s` has guitar / piano but is markedly weaker).  Only if a
   student that really separates ever exists.
2. **By signal property, no classes** — one multiplier on the mask from what
   the DSP already measures per cell: inter-channel coherence, stationarity
   (pad vs pluck), width.  Sustained + decorrelated + wide → deeper;
   transient + narrow + coherent → nearer.  This is the classical
   primary/ambient idea (Avendano & Jot) inside our renderer:
   `G_cell = G · f(coherence, stationarity)`.  Cheap, no new network,
   testable as an A/B in the stem harness (guitars / keys / pads by MoisesDB
   group, swing), deployable at once.

Order: finish the one-class student first (v24 →), then route 2 as a
separate A/B, route 1 only with a separating student.  Note that the users'
complaint was *instability*, not "guitars too shallow": layering is the
"gorgeous" step, not the fix, and it must be judged by ear, not by the metric.

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

## 🔁 Review 6 (astra, after the teacher-through-DSP, the confound and the baselines)

1. **Distil the rendered quantity.**  Finish the clean two-stage run unchanged
   (keep the stage-one checkpoint: it separates representation learning from
   routing-objective damage).  Next: **direct eligibility distillation** —
   predict e, supervise the amplitude `a = sqrt(e)` (Huber on `sqrt ê` vs
   `sqrt e_T`) + 0.2 BCE(ê, e_T) + 0.1 role CE; all audible cells with capped
   energy weights, confidence filtering only on the auxiliary role labels
   ("excluding ambiguous cells leaves exactly the difficult boundaries
   unsupervised"); teacher target matched to the deployed control update:
   energy-weighted mean eligibility over the held hops `ē = Σ|X|²e / Σ|X|²`;
   no sharpening; 600 masters first, the library after.  AUC 0.85 is not a
   capacity wall: compare teacher and student on identical cells, overfit a
   few songs; a non-causal teacher may use future context a causal student
   cannot — "copying its eligibility is plausibly much easier than copying
   its waveforms, but the ceiling is unmeasured."
2. **Budget → frequency modelling.**  At ~230 M: freq conv 257→129 (k5,
   12→24 ch, 17 M) → freq conv 129→65 (k3, 24→32, 14 M) → bidirectional
   frequency GRU over 65 bins (hidden 40/dir, 105 M) → causal temporal GRU
   shared across bins (input 80, hidden 16, 28 M) → full-resolution decoder
   (interpolate 96 + 12 local descriptors → 16 → 1, 42 M) = 207 M.
   Frequency bidirectionality costs no future time.  Reconcile 200 vs 230 M,
   benchmark activations / memory traffic / requantisation / worst-case
   worker deadlines; retire the controllers only after a functional ablation.
3. **The matrix is a serious competitor; postpone the hybrid.**  It shows
   rear-share rewards width extraction as much as routing.  Companion metric:
   **oracle-referenced rear reconstruction error** — oracle Ls/Rs from the
   same renderer, latency-aligned, bandwise normalised squared error, reported
   at fixed gain and after one global positive gain fit (level vs
   waveform/image mismatch); no per-bin phase fitting.  Rear correlation alone
   is useless (identical difference signals in both rears correlate +1).
4. **Listening: five conditions** — stereo/front-only, passive matrix, best
   deployable student, clean-KD candidate, oracle routing (the teacher in a
   separate engineering check); loudness-matched, rear level logged; excerpts
   with sustained guitar/keys, vocal entries, width changes, drum fills,
   30–60 s; rate separately *identifiable rear source*, *stable rear
   position/presence*, *vocal intrusion*, *distracting diffuse wash*; identify
   the rear instrument before scoring; duplicate trials.
5. **Stop:** interpreting contaminated runs; optimising sharpness as
   progress; tuning solely against rear share; treating the 40 inspected DEV
   songs as final evidence — the 24-song TEST split stays untouched until the
   next winner is chosen.

## 🔎 Review 7 (astra, on the commitment failure)

1. **Diagnose confidence before changing architecture.**  Ranking: conditional
   averaging from unresolved purity/context; optimisation or implementation
   pathology; intrinsic input insufficiency (unproven — "no single finite
   training run proves that the inputs contain no purity information").
   High dominant-vs-dominant AUC does not establish pure-vs-mixed
   discrimination, and four songs are still millions of cells.  **One
   diagnostic run, three arms on a tiny fixed set of complete windows
   stratified by role and purity, no augmentation:** (a) the model + soft
   CE; (b) unbounded linear logits, no weight decay; (c) b + purity-stratum
   balancing.  Log the exact weighted target entropy, CE − entropy, logit
   scale, pre-clip gradient norms and clip frequency.  a fits → duration /
   data complexity; only b → decoder / optimisation; only c → weighting /
   capacity allocation; none → audit target alignment, normalisation,
   gradient plumbing before blaming information.
2. **Go to the listening study; stop architecture-shopping.**  Provisional
   product candidate: frequency student + soft veto, with the raw frequency
   student as a mandatory comparison (measure the veto's rear-error cost
   first), the heuristic candidate and the oracle as references; compare at
   intended levels *and* level-matched (quiet rears win leakage judgements
   trivially).  One more model experiment only: the diagnostic above.
3. **The student's rear is not "the oracle at lower level".**  The gain fit
   removes 0.04 of 0.19; substantial shape mismatch remains.  A gain sweep
   is legitimate but preserves leakage ratios (guitars 0.12 → 0.20 would take
   voice 0.08 → ~0.13); choose the point with the lowest *fixed-gain* rear
   error under pre-declared voice and swing limits, then listen.
4. **Confidence interventions, by risk:** temperature calibration fitted on
   dominant cells, selected on all strata + DSP; two heads (hard role CE on
   dominant cells + purity Huber on all, `e = p_other · q̂`, `a = sqrt e` — an
   approximation that cannot manufacture missing purity); hard CE with label
   smoothing on dominant cells only; margin loss last.
5. **One small ramp sweep is worthwhile** (leak:missed 1:1 and 1:2) against
   calibrated raw-student points at matched voice share; keep the ramp only
   if it wins reconstruction, guitars or swing at that leakage.
6. Host RAM: don't blame CUDA caching yet; isolate by measuring before CUDA
   init, after model creation, after batches; workers / caches / pinned
   batches are the plausible causes.

## 🧭 Review 8 (astra, on the system architecture — 2026-09-27 01:00)

Asked whether "per-cell role eligibility at 512/128, predicted causally,
driving the rear at sqrt(e)" is the right decomposition at all.  Verdict:
**"The architecture is wrong as a final commitment to the product goal.  It
is a useful, stable baseline, but 'predict fractional ownership, then send
that fraction of the mixture rearward' makes separation uncertainty directly
audible."**

* Ranking of what is wrong: (1) **the target** — five-way cell ownership is
  indirect; classification rewards explaining ambiguous cells, the product
  rewards reconstructing usable rear content; (2) input / representation —
  longer analysis and context plausibly missing, but a waveform front-end is
  not intrinsically the answer; (3) the grid — hard, not proven unlearnable;
  (4) consumption — magnifies uncertainty but is not the bottleneck ("a
  transfer curve cannot manufacture discrimination"); (5) do not retreat to
  ambience synthesis for this brief.
* **The one bet: replace the eligibility interface with an estimate of the
  rear-eligible AUDIO** — a two-output protected / eligible separator with
  longer analysis (2048/512) and 0.5–1 s of causal history, complex spectral
  estimation (not `(1 − p_protected) × mixture`, which recreates today's
  system), its eligible estimate fed into the existing rear object path, the
  front kept.  Design budget 120–150 M MAC/s after retiring the controllers
  (unverified), ~50–65 ms aligned pipeline delay.
* **The one-day interface test:** inject the oracle's eligible-stem *audio*
  and the teacher's eligible-stem *audio* through the object path; compare
  each with its mask-driven version at matched rear instrument level; measure
  vocal leakage, instrument share, swing, artefacts.  This decides whether
  reconstruction beats masking perceptually; it cannot validate a causal
  150 M separator — nothing using existing masks can.  If reconstruction
  brings little, keep the present interface.
* Hysteresis on a good ranking: legitimate when errors are brief and true
  eligibility persists; may never open on material the student scores at
  0.37; test connected persistent regions, not cell latches.  Rear error 0.15
  after gain fit does not show only commitment is missing — the decisive
  test is more guitar/key energy at *matched* vocal leakage.
* Longer analysis: yes; 2048/512 is the same 93.75 decisions/s, not fewer;
  keep a short-window transient / protection path.
* **Timing correction:** at 48 kHz, 512/128 is a 2.67 ms hop and 10.67 ms
  between controls — the "43 ms application lag" claimed earlier is wrong;
  measure timestamp-to-application latency before comparing architectures.
* **Stop:** the ramp-weight sweep, dominant-cell calibration as a route,
  the whole-library four-stem teacher (pause; the material suffices), and
  the C port of the frequency student (defer: it hardens a compromise).

## 🔌 The interface test (2026-09-27 02:00, astra review 8)

The rear-eligible AUDIO through the object path (test build `--rear-audio`,
harness `--inject`), same renderer, same 40 DEV songs, G = 1.0:

| | vocals | guitar | piano | keys | bass | drums | **guitar** swinging (historical) | rear level |
|---|---|---|---|---|---|---|---|---|
| oracle **mask** (real-stem eligibility × mixture) | 0.03 | 0.20 | 0.14 | 0.17 | 0.07 | 0.12 | 7/39 | −37.2 dBFS |
| oracle **audio** (the non-protected stems themselves) | **0.01** | **0.26** | **0.17** | **0.23** | 0.01 | 0.07 | **3/39** | −37.6 dBFS |
| teacher **mask** (htdemucs eligibility × mixture) | 0.03 | 0.20 | 0.14 | 0.17 | 0.11 | 0.11 | 8/39 (vocal 11/40) | −37.0 dBFS |
| teacher **audio** (htdemucs "other" stem) | **0.01** | 0.20 | 0.15 | 0.19 | 0.04 | 0.08 | 5/39 (vocal 4/40) | −38.0 dBFS |
| teacher-2 **audio** (BS-RoFormer vocals + demucs drums/bass → "other") | **0.01** | **0.23** | **0.17** | **0.22** | 0.06 | 0.10 | **3/39** (vocal 7/40) | −38.0 dBFS |
| **optimal bounded real mask** (ideal ratio mask of the eligible stem at 2048/512, mixture phase, `--inject irm`; astra 9) | 0.01 | 0.23 | — | 0.23 | 0.02 | 0.08 | 3/39 | −37.8 dBFS |

Rear error vs the mask-oracle render: teacher mask 0.04, teacher audio 0.18,
teacher-2 audio 0.23 — the audio rows differ from the mask render by
construction (they are *cleaner* than it), so the rear-share / swing columns
are the comparison that counts here.

**All-stem recount (28.09, original `stems.csv` files, unique songs where any `range_1s > 0.30`):**

| Renderer input at G 1.0 | Bowed strings rear share | Any-stem swinging songs / 40 |
| --- | ---: | ---: |
| Oracle mask × mixture | 0.106 | 14 |
| Oracle eligible audio | 0.180 | **8** |
| Teacher mask × mixture | 0.098 | 23 |
| Teacher eligible audio | 0.131 | 16 |
| Teacher-2 eligible audio | 0.183 | 20 |
| Optimal bounded real mask/audio | 0.169 | **7** |

These counts come from `D:\upmix\runs\iface8\{mask_oracle,inject_oracle,mask_teacher,inject_teacher,inject_teacher2,inject_irm}\stems.csv`. The original 3/39, 5/39 and 7/39 numbers above count **guitar** only and cannot certify the goal's any-stem limit. On the 39 guitar-present songs, oracle audio still swings in 8, above the ≤ 5/39 target. Oracle audio resolves six of the oracle-mask's 14 swinging songs and adds none, but eight eligible-instrument swings remain, including guitar, piano and keys. Thus clean extraction helps substantially; **even perfect eligible audio does not by itself prove the requested stability**. The C object/bed controller remains part of the problem, and no offline result here substitutes for the owner's car verdict.

The oracle-audio row otherwise clears the listed rear-share limits on this full set: guitar 0.255, keys 0.230, piano 0.171, strings 0.180; voice 0.009, bass 0.013, drums 0.069. Median `slope_per_6dB` is +0.005 for guitar, +0.002 for keys and +0.010 for winds, all within ±0.05. This is an upper bound using true stems, not a deployable candidate. Its vocal-entry ducking has not been measured with the transition diagnostic, so the numerical goal remains unproven even aside from the eight swinging songs.

The real htdemucs eligible-audio condition resolves nine of its mask condition's 23 swinging songs but adds two, leaving 16. Relative to true eligible audio, it resolves one swing and adds nine. The measured gap therefore includes both residual movement in the renderer with perfect audio and errors in extracted audio; optimizing the extractor alone cannot guarantee the stability limit.

Rear levels match within 0.4 dB (−37.2 vs −37.6 dBFS), so the oracle
mask/audio difference is not a level effect. Reconstruction gives more instruments
behind (guitars 0.20 → 0.26), less voice (0.03 → 0.01), and fewer guitar swings
(7 → 3). The mask's ceiling was never the renderer's ceiling —
it was the mask interface's: a fraction of the *mixture* carries the mixture's
other sources with it; clean audio does not.  **A real separator's estimate keeps the advantage.**  With the same
htdemucs stems, audio instead of mask: voice 0.03 → 0.01, bass 0.11 → 0.04,
vocal-swinging songs 11 → 4, guitars unchanged at 0.20 — and with the cleaner
BS-RoFormer vocals the instruments follow the oracle-audio row (guitars 0.23,
keys 0.22, 3/39 **guitar** swinging) at the same 0.01 voice, one dB quieter overall. The
interface test is decided: *the rear-eligible audio through the object path*
is the architecture; the mask interface was the ceiling.  What remains is
the hard part — a causal separator that produces such audio inside the
budget (2048/512 with 0.5–1 s of history, complex or magnitude+phase mask,
trained on the teachers' *audio* with a spectral/waveform loss, not per-cell
CE) — and nothing measured so far establishes that it fits ≤ 150 M MAC/s.

## 🎛️ The separator student (2026-09-27 04:00, `sep_net.py`)

The first feasibility run of the architecture the interface test chose:
two-way (protected = vocals + drums + bass, eligible = the rest), causal,
2048/512 at 48 kHz: log|L|, log|R| → strided frequency convs 1025 → 513 →
257 → 129 → a causal GRU per position (shared) + a global causal GRU → a head
at 257 positions giving 4 bins × 2 channels of magnitude-ratio mask → linear
upsample to 1025 → eligible = mask × mixture STFT (mixture phase) → iSTFT.
31.7k params, **154 M MAC/s** + one 2048-point FFT/iFFT pair per hop.  Loss
in the audio domain (L1 on |STFT| + 0.5 L1 on the complex STFT of the
estimate vs the eligible target), 950 songs (MoisesDB train + the four-stem
teacher cache), 8 s windows, 6000 steps.  Evaluated on DEV as SI-SDR of the
estimate vs the true eligible audio and the vocal leak (estimate energy in
vocal-owned cells / vocal energy), then through the DSP with
`--inject student:CKPT` beside the oracle-audio / teacher-audio rows above.

| run | | result |
|---|---|---|
| sep1 | the recipe above, 6000 steps (25 min) | SI-SDR of the estimate vs the eligible truth −5.9 → **−2.3 dB** by step 2000 and flat after; vocal leak 0.24–0.41. The checkpoint selection (`si_sdr − 20·leak`) picked step 750 (leak 0.06, SI-SDR −4.3): a near-silent estimate — through the DSP: rear −42 dBFS (5 dB below every other row), guitars 0.02, voice 0.01 | not a feasibility answer yet: a 31k-param magnitude mask trained 25 min does not separate (SI-SDR must clear the mixture's own baseline by a wide margin); the selection metric was wrong. sep2: select by SI-SDR, log the mixture baseline, 20 000 steps |
| IRM ceiling | the optimal `[0,1]` real mask at the student's grid, mixture phase | SI-SDR vs the eligible truth **+5.9 dB** (mixture −6.0), median of 40 DEV songs; through the DSP: guitars 0.23, keys 0.23, voice 0.01, 3/39 — the oracle-audio row | **the real-mask output type is not the ceiling**: a bounded magnitude mask can carry the confident rear; sep1/sep2's failure is representation / learning, not the mask form |
| sep3 | **the bandwise recurrent separator** (astra 9 §3): 48 unequal bands, band encoders, shared band GRUs 48→32→32, two frequency mixers, global GRU, direct bin decoders, stereo complex ratio masks, 587k params, **125 M MAC/s**; complex L1 + multi-resolution spectral (+ SI-SDR after 20k), half MoisesDB / half four-stem pseudo-labels, 150k steps, batch 12, 8 s, AdamW 3e-4 warm-up 2k + cosine | queued behind sep2 (~17 h) | the bounded feasibility experiment; gates: guitars ≥ 0.18, voice ≤ 0.03, ≤ 3/39, level-matched |
| sep2 | sep1 + selection by SI-SDR, the mixture baseline logged, 20 000 steps | best **−1.76 dB** at step 11000 (mixture −6.3, IRM ceiling +5.9), vocal leak 0.25; DSP: voice 0.04, guitars 0.12, piano 0.11, keys 0.13, bass 0.12, drums 0.11, 5/39 swinging, rear error 0.17 (gain-fit 0.15), level −38.1 | +4.5 dB over the mixture, a third of the way to the real-mask ceiling; the shared hidden-16 recurrence is the limit (astra 9) |
| sep3 (stopped at 60k) | | −0.86 at 3000, −1.3 at 6000, −0.95 at 10000, −0.79 at 16000–20000, −0.47 at 24000, −0.34 at 28000, −0.60 at 32000, −0.43 at 36000, −0.51 at 40000, −0.32 at 43000, **best −0.24 dB at 47000**, −0.41 at 52000, −0.36 at 60000; vocal leak 0.09–0.17; 0.39 s/step | the early dip was high-LR noise; after 24k the curve is flat within ±0.15 dB around −0.35 — **+2 dB is not on this curve**; stopped at 60k for the capacity diagnostic (astra 10). Its best checkpoint goes through the renderer at G 1.0 / 1.3 |
| sep3 @ 24k through the DSP | the −0.47 dB checkpoint's audio, G = 1.0 | **voice 0.03**, guitars 0.10, keys 0.10, piano 0.08, bass 0.07, drums 0.10, 3/39 swinging, rear error 0.15 (gain-fit 0.14), level −38.7 dBFS (0.7 dB below sep2) | the extra 1.3 dB of SI-SDR bought less leak (voice 0.04 → 0.03, bass 0.12 → 0.07), not more instruments (guitars 0.12 → 0.10): the estimate got more conservative. Voice already at the gate; guitars at 0.10 of 0.18. Gain sweep on the same checkpoint next |
| sep3 best (47k, −0.24 dB) through the DSP | G 1.0 / 1.3 | G 1.0: voice 0.02, guitars 0.09, keys 0.10, bass 0.07, drums 0.09, 5/39 swinging, level −39.5 dBFS, rear error 0.15; G 1.3: voice 0.03, guitars 0.13, keys 0.15, bass 0.10, 10/39 swinging, level −37.8 | the later, slightly better-SI-SDR checkpoint is more conservative than the 24k one (guitars 0.10 → 0.09, level −38.7 → −39.5); at the stable gain the instruments sit at ~0.10 and recovery needs gains that bring > 10/39 swings — exactly the stop-rule picture of review 10 for *this* implementation. The capacity diagnostic (sep5) says whether width changes it |
| sep3 @ 24k, gain sweep | G = 1.3 / 1.6 on the same audio | G 1.3: voice 0.04, guitars 0.16, keys 0.16, bass 0.11, **12/39 swinging**, level −36.9, rear error 0.19 (gain-fit 0.15); G 1.6: voice 0.05, guitars **0.18**, keys 0.19, bass 0.13, **15/39 swinging**, level −36.1 | gain buys the instruments (0.18 at 1.6 with voice still 0.05) but the swing explodes 3 → 12 → 15: the student's errors are *temporal* — its estimate's level fluctuates, and amplification amplifies the fluctuation. The binding gate is now stability, not leak (astra 9: "leakage location and temporal stability matter more than aggregate SI-SDR"). Next: a slow per-band gain envelope on the estimate (fast attack, ~300 ms release) before injection, to see whether the swing drops at G 1.3 |
| sep3 @ 24k, G 1.3 + held per-band share (`--inject-smooth`) | release 300 / 800 ms | 300 ms: voice **0.08**, guitars 0.18, 10/39 swinging, level −35.0; 800 ms: voice 0.10, guitars 0.20, 14/39, level −34.1 | **wrong direction**: holding the estimate's share keeps energy in when the estimate correctly drops for a vocal entry — voice doubles, the swing barely moves. The student's level changes are not noise to smooth; they are decisions, right and wrong. Stability must come from a better estimate, not a hold (astra 9: "delayed release prolongs vocal leakage") |
| review 10 (astra, 27.09 15:45) | on the band model's plateau | **leading diagnosis: inadequate cross-band temporal modelling** — the recurrence is per band, frequency integration only through shallow mixers and a 32-dim global summary; "recognising an instrument requires tracking harmonics and competing sources jointly". Then supervision (contaminated pseudo-targets reward vocal retention; check that MoisesDB "other" and teacher "other" follow the same instrument policy), then loss balance; not LR/steps/crop/normalisation/init ("an unbounded mask initialised at 0.3 has no attenuation lock"; SI-SDR is scale-invariant). **One-run test: the same topology at ~2× widths, same data / losses / schedule, compared at matched steps.** On the swing: admission errors amplified by the renderer — keep G = 1.0, improve separation; no generic mask smoothing (retains the previous assignment), no onset penalty. **Change tonight: do the capacity diagnostic, not sep4** — RoFormer vocals + demucs drums/bass is not automatically a cleaner residual (independent separators assign overlaps differently; audit before generating 774). Truth-only next if widening yields little. Decision: continue at SI-SDR ≥ +2 dB, leak ≤ 0.05, guitars/keys ≥ 0.20, voice ≤ 0.02, ≤ 5/39 at an operating gain; ≥ 1 dB + clear renderer gain justifies another iteration; stop this implementation if controlled diagnostics leave it near 0 dB with instruments ~0.10 at the stable gain. Audit: normalisation inversion, units, alignment, STFT boundaries, SI-SDR; **whole-window normalisation is non-causal** — streaming normalisation before any deployment claim |
| night 27→28.09, revised (`run_sep5.ps1`) | sep3 stopped at 60k, its best through the renderer at G 1.0 / 1.3; then **sep5 = the capacity diagnostic**: band widths ×2 (feat 96, GRUs 64, global 64; 1.2 M params, 394 M MAC/s — budget ignored on purpose), identical data / losses / schedule, from scratch, batch 6 × accum 2, 40k steps; compared with sep3 at 3k / 10k / 20k / 40k | running: **−1.19 / −0.87 / −0.70 / −0.76 / −0.57 / −0.64 / −0.58 dB** at 1k / 2k / 3k / 5k / 6k / 8k / 10k vs sep3's −1.57 / −1.09 / −0.86 / −1.24 / −1.31 / −1.23 / −0.95; leak 0.13–0.18 | **finished 40k: −0.42 dB at 40k, best −0.34 at 18k**, the last 22k steps inside −0.35…−0.52 (leak 0.10–0.15) vs sep3's −0.79 at 20k / −0.51 at 40k / −0.24 best at 47k: a 0.3–0.45 dB lead at matched steps that sep3 later closes with more steps, and a plateau that width does not move. **Doubling the width buys ~0.4 dB against a 5.3 dB ceiling — capacity is not the wall.** By review 10's rule this points to a structural bottleneck or supervision: truth-only (sep6) and the band RNN (sep7) follow tonight |
| sep5 best (18k, −0.34 dB) through the DSP | G 1.0 / 1.3 | G 1.0: voice 0.03, guitars 0.12, keys 0.12, bass 0.08, drums 0.10, 5/39 swinging, level −38.1 dBFS, rear error 0.13 (treble 0.10); G 1.3: voice 0.04, guitars 0.17, keys 0.18, bass 0.11, **14/39 swinging**, level −36.2, error 0.18 | the wide model is a little bolder than sep3's best (guitars 0.09 → 0.12, level +1.4 dB, error 0.15 → 0.13) at the same voice-leak class (0.02 → 0.03) — but still the ~0.10 instrument picture at the stable gain: 3× the compute changes the renderer's numbers by a hair, exactly what its 0.1 dB SI-SDR difference predicts; at G 1.3 the same gain-buys-instruments-and-swing trade as sep3 (guitars 0.17 for 14/39) |
| **gap decomposition** (`diag_gap.py`, 27.09, sep3 best 47k and sep5 best 18k on the 12 DEV excerpts) | where the 5.5 dB to the IRM live: SI-SDR of the student, of its magnitude with the mixture phase, of the IRM; recall of eligible-dominant cells (energy kept / truth energy where the eligible stem dominates), leak into protected-dominant cells; per-band magnitude bias on eligible-dominant cells; SI-SDR per 2-s segment | **phase: worthless** — student −0.24 / magnitude-only −0.25 (sep5 −0.34 / −0.34): the complex mask acts as a real mask. **Leak: at the IRM's level** — 0.06 (sep5 0.09) vs the IRM's 0.05. **Recall: the hole** — sep3 0.38, sep5 0.49 vs the IRM's 0.89. **Band bias on eligible-dominant cells** (dB, student vs truth): <250 Hz −7.9, 250 Hz–1 kHz −5.5, 1–3 kHz −5.4, 3–8 kHz −10.0, **8–14 kHz −19.0**, >14 kHz −14.6 (sep5: −7.4 / −3.9 / −4.2 / −9.0 / −20.7 / −14.8); mean and mean-absolute agree, so it is a systematic bias, not noise. Per song the gap to the IRM is 4–6 dB everywhere except one (mixture −17.7 dB: recall 0.05, student −13.4, IRM +5.7). Segments: p10/p90 −3.2/+2.3; the 12 excerpts contain **no vocal entry** (they are the loudest-vocal 20 s, vocals are continuous) | the students are **hedged, treble-less real masks**: they keep the eligible stem's mid body at −4…−5 dB and erase its treble at −19 dB even where it dominates, while leaking no more than the ideal mask into protected-dominant cells (review 11's correction: the VOCAL leak is 3× the IRM's — 0.09 vs 0.03 — so "no more leakage than the IRM" holds for the protected class as a whole, not for the voice). That is the renderer's picture too — guitars at 0.10 with no air, and a gain that recovers the level also raises the (already IRM-level) leak into a swing. Two causes fit: (1) the loss — linear-magnitude L1 and log1p (linear for small values) make a quiet treble cell nearly free to miss; the model takes the prior "treble = cymbals/sibilance" and attenuates; (2) identification — telling a guitar's 6 kHz partial from a cymbal needs the harmonic series below it (cross-band, sep7). Width (sep5) moved the operating point along the recall/leak frontier (recall 0.38 → 0.49, vocal leak 0.09 → 0.16), not off it — the same one-frontier picture as the mask students. **v2 (review 13 §6, source attribution: the frozen mask applied to the true stems, reanalysed audio; 28.09 00:00):** transmitted eligible energy / eligible energy — sep3 **0.29** (0.32 on eligible-dominant cells), sep5 0.37 (0.42), sep6 0.33 (0.39); transmitted **vocal** energy / vocal energy — sep3 **0.124** (0.108 on vocal-owned cells), sep5 0.199, sep6 0.104; transmitted protected energy 0.063 / 0.085 / 0.059. Energy-weighted transmitted eligible fraction per band (dB) — sep3: <250 Hz −7.3, 250 Hz–1 kHz −4.6, 1–3 kHz −4.1, 3–8 kHz −4.4, **8–14 kHz −13.6, >14 kHz −14.6**; sep5 −6.7 / −3.2 / −2.8 / −3.6 / −20.1 / −23.1; sep6 −7.5 / −4.5 / −3.9 / −4.7 / −22.2 / −27.3. The cell-weighted bias over-weighted quiet cells (3–8 kHz read −10 where the energy says −4.4); the shape stands: the body passes at −4…−5 dB, the air at −14…−27 dB, and one eighth to one fifth of the vocal's energy passes with it. sep7 (band RNN): recall 0.36 (0.41), vocal 0.139, protected 0.092, bands −7.5 / −3.6 / −2.9 / −3.7 / −14.5 / −17.1. **The IRM by the same attribution: recall 0.77, vocal transmission 0.105** — the ideal ratio mask itself passes a tenth of the vocal energy (wherever voice and instruments share a cell it passes the ratio), so the students' true vocal leak (0.10–0.14) sits *at* the ideal mask's, and review 11's "3× the IRM" was an artefact of the cell metric. The whole gap is under-recovery: 0.29–0.37 of the eligible energy transmitted against the ideal mask's 0.77 |
| sep8 (`run_sep8.ps1`, after sep7) | **the loss diagnostic** (review 10's third rank, now with evidence): sep3's model and data with the magnitude terms in the LOG domain with a relative floor (`--loss log --loss-floor 3e-3 --loss-scale 0.74`: L1 of log(|·| + 3e-3 · mean|X_mix|) at 2048/512 and in the three multi-resolution terms, the floor taken from the mixture at each resolution's own grid, detached, clamped ≥ 1e-7; one fixed scale on the bundle so its **parameter**-gradient norm matches the old bundle's — review 13: the output-gradient calibration gave 0.007 and would have silenced the bundle, `diag_lossfloor.py` §3; 0.0 % of the eligible energy lies below the floor in any band, 0.1 % above 14 kHz; the complex L1 term unchanged), so a −19 dB miss in a treble cell costs what a −19 dB miss in a bass cell costs; 40k → DSP at G 1.0 / 1.3 | **finished: −2.19 / −1.23 / −1.11 at 1k / 2k / 7k, −0.85…−0.75 from 24k, best −0.73 at 35k, −0.75 at 40k; vocal leak 0.13–0.15** (sep3 0.15–0.17). Attribution at 26k: eligible transmission 0.36 (sep3 0.29), vocal 0.167 (0.124), **air 8–14 kHz −8.3 dB (sep3 −13.6), >14 kHz −10.0 (−14.6), 3–8 kHz −2.6 (−4.4)** — the first change of the mask's *shape* this night. **DSP G 1.0: voice 0.04, guitars 0.11, keys 0.11, bass 0.07, drums 0.11, 9/39 swinging, level −38.7, error 0.14; G 1.3: voice 0.05, guitars 0.16, keys 0.16, 18/39** | **the recall rose only with the leak**: the log loss gives the estimate its air back (+5 dB at 8–14 kHz) and 7 points more eligible transmission, but the vocal transmission rises with it (0.124 → 0.167, above the ideal mask's 0.105) and the renderer pays in voice (0.04) and swings (9/39 vs 4–6) for +0.02 of guitars — the same frontier, a bolder point on it. Review 11's gate (+1 dB at matched leak, recall → 0.60) is not met; the loss governs the *shape* of the hedge (which bands are erased) but not the discrimination. Four levers now — width, supervision, structure, loss — each moved along the frontier; future context (sep9 / sep9c / sep10) is the last untested axis, and the rapid loop is the tool for the target/conditioning ideas of review 14 |
| **transition eval** (`diag_transitions.py`, 27.09 19:30, review 11 §4; 20 DEV songs, true stems) | vocal ENTRIES (rise > 10 dB between adjacent 2-s windows, landing above −40 dBFS; 53 events), EXITS (48) and SUSTAINED-vocal controls (54), ≤ 3 per song per kind; 12-s excerpts (4 s before / 8 s after) after 10 s of state warm-up; per 0.5 s: vocal leak (estimate energy in vocal-owned cells / vocal energy there) and eligible retention (estimate energy in eligible-dominant cells / truth energy there); median / p90 | **sep3 best** — entries: retention 0.57 before → 0.49 after (post/pre **0.87**, p90 1.33), leak 0–2 s after entry 0.19 / 0.51, **peak 0.66 / 1.08**, steady (4–8 s) 0.19 / 0.45, settling 0.0 / 2.4 s; exits: retention 0.43 → 0.58 (post/pre **1.40**, p90 2.46), leak peak 0.79 / 1.95; sustained: retention 0.39 → 0.38 (post/pre 0.98), leak steady 0.115 / 0.27, peak 0.30. **sep5 best** — entries 0.65 → 0.60 (0.89), leak steady 0.21, peak 0.70 / 1.26; exits 0.53 → 0.67 (1.15, p90 1.86); sustained 0.49 (0.96), leak 0.17 | the instability is **transition-driven**: with a stable scene the estimate is stable (sustained post/pre 0.98) and its leak sits at the eval's ~0.1; at a vocal entry it **ducks the instruments by ~13 % (median) and lets 2/3 of the vocal energy through in its worst half-second** (p90: more estimate than vocal in vocal-owned cells); at an exit it **releases the instruments by +40 % (p90 +146 %)** — the rear breathes with the voice, which is exactly the "back and forth" and the renderer's swing count. Quieter vocals leak relatively more (0.19–0.21 after an entry vs 0.11 sustained vs 0.09 on the loudest-vocal eval): the hedged mask attenuates the protected class by a level-independent factor, so a soft voice survives. Width (sep5) keeps more instruments (retention +0.1) and releases less at exits (1.15) but leaks more — the frontier again. Worst cases (sep3): ducking 765d5131 @14 s retention 0.85 → **0.12** (the instruments all but vanish from the estimate when the voice enters), 0a589d65 @156 s 0.89 → 0.35, 92ba4404 @15 s 1.03 → 0.44; peak leak 69dff382 @92 s 4.1 (sep5 8.0 — the first half-second of a soft entry, where the estimate carries several times the vocal's own energy in the cells the voice owns). **v2 (review 13 §6: the frozen mask applied to the true vocal / eligible stems of the same segment, reanalysed; directed 0.25-s crossings, overlapping events rejected → 43 entries / 40 exits / 54 sustained; 28.09 00:00):** sep3 entries — transmitted eligible fraction 0.57 before → 0.36 after, **post/pre median 0.72, p10 0.38, p90 1.07**; true vocal leak 0–2 s after entry 0.16 / 0.44 (median / p90), **0–2 s peak 0.24 / 0.59**, steady 4–8 s 0.16 / 0.47; settling median 0.0 s, p90 1.9 s, none censored; exits 0.35 → 0.54, **post/pre 1.47** (p10 1.12, p90 2.48), vocal-tail leak 0–2 s 0.47; sustained 1.04 (p10 0.78, p90 1.52), leak 0.13. sep5: entries 0.77 (p10 0.37), peak 0.29, steady 0.19; exits 1.26; sustained 0.99, leak 0.18. sep6: entries **0.70** (p10 0.34), peak 0.23, steady 0.15; exits 1.36; sustained 1.03, leak 0.11. sep7: entries 0.76 (p10 0.32), peak 0.34, steady 0.21; exits 1.41; sustained 1.03, leak 0.16. Worst ducking (sep3) 765d5131 @14 s 0.85 → 0.10, 0a589d65 @156 s 0.88 → 0.22, 8f36f17f @19 s 0.57 → 0.17; worst 0–2 s peak 737356b2 @172 s 0.92. With proper attribution the breathing is **stronger** than the v1 cell ratios showed: the mask passes 28 % less of the instruments' energy in the 8 s after a voice enters than in the 4 s before (one event in ten loses > 60 %), and 47 % more after a voice leaves; the vocal itself passes at 16 % of its energy after an entry (peaks 24 %, p90 59 %) |
| sep6 (`run_sep6.ps1`, after sep5) | **the supervision diagnostic**: the 125 M band model on MoisesDB truth only (176 songs, ±6 dB stem gains), same schedule, 40k steps → DSP at G 1.0; compared with sep3 (50 % pseudo-labels) at matched steps | **finished 40k: −0.81 at 40k, best −0.66 at 30k**; −0.85 at 11k, −1.0…−1.15 at 15–20k, −0.72 at 21k (the SI-SDR term), then −0.66…−0.84 to the end; **vocal leak 0.066–0.075 from 21k on** (sep3: 0.15–0.17; sep5: 0.10–0.16). vs sep3 at matched steps: −0.95 / −0.79 / −0.51 at 10k / 20k / 40k → truth-only is ≈ 0.1–0.3 dB behind sep3 with half its vocal leak. **DSP at G 1.0 (best, 30k): voice 0.02, guitars 0.10, keys 0.11, bass 0.07, drums 0.09, 4/39 swinging, level −39.1 dBFS, rear error 0.17 (treble 0.11)** — sep3 best at G 1.0 was 0.02 / 0.09 / 0.10 / 0.07 / 0.09, 5/39, −39.5, 0.15: the same renderer to the second decimal. **Supervision is not the lever:** the 774 pseudo-labelled masters bought sep3 a little boldness and twice the vocal leak in the eval metric, and nothing in the renderer; removing them removes nothing either. Review 11's sep6 gate (+1 dB at matched leak, recall → 0.60) is not met. **Its error has the same shape** (`diag_gap`, `diag_transitions` on sep6 best): recall 0.45, protected leak 0.06, vocal leak 0.073; band bias <250 Hz −10.7, 1–3 kHz −5.9, 3–8 kHz −11.5, **8–14 kHz −24.5**, >14 kHz −19.3 — clean truth makes the treble erasure *deeper*, not shallower; transitions: ducking at vocal entries **0.81** (sep3 0.87; worst 0.81 → 0.14), release at exits 1.27, sustained 1.03, leak steady 0.16 after an entry / 0.094 sustained. Whatever the supervision, the 125 M band model with this loss converges to the same hedged, treble-less mask that breathes with the voice | if truth-only matches or beats sep3, the pseudo-labels bought nothing (or hurt) and the residual-"other" policy is the suspect; if it is clearly worse, the 176 songs are too few and cleaner pseudo-labels (audited, not assumed) are the lever |
| sep7 (`run_sep7.ps1`, after sep6) | **the structure diagnostic** (review 10's leading diagnosis, "inadequate cross-band modelling"): sep3's band model with the two depthwise k3 mixers replaced by a BSRNN-style **bidirectional GRU across the 48 bands at every frame** (`--band-rnn 32`; frequency is not causal, so the recurrence over time stays causal), 599k params, ~180 M MAC/s (budget exceeded on purpose; the 140 M version is `--band-rnn 16`), same data / losses / schedule as sep3 and sep5, from scratch, 40k → DSP at G 1.0 / 1.3 | **finished: −1.28 / −1.16 / −1.02 at 3k / 10k / 20k (behind sep3), −0.58 / −0.53 at 24k / 28k, best −0.33 at 30k**, then −0.43…−0.49 (leak 0.10–0.11) — the same −0.3…−0.5 band as sep3 (−0.34 @28k) and sep5 (−0.34 @18k). **DSP G 1.0: voice 0.03, guitars 0.12, keys 0.10, bass 0.08, drums 0.09, 6/39 swinging, level −38.3 dBFS, error 0.15; G 1.3: voice 0.04, guitars 0.17, keys 0.16, 16/39, −36.5, 0.18** — sep5's renderer to the second decimal | **structure is not the lever either**: a bidirectional GRU across all 48 bands, 1.4× the budget, lands on the same plateau and the same renderer as the mixers. Review 12's reading stands — three levers (width, supervision, spectral communication) each ≤ 0.4 dB; the untested axes are the loss (sep8, running) and future context (sep9 / sep9c / sep10). Note review 13's caution: sep7's catch-up from 24k coincides with the SI-SDR term at 20k, so its early deficit vs sep3 is not evidence against the band RNN |
| evaluation-path audit (review 10, E; `diag_ceiling.py`, 27.09) | the mixture, the identity mask through `stft`/`istft`, and the bounded real mask (IRM, mixture phase) through the **exact** `evaluate()` loop (same 12 DEV excerpts, same `si_sdr`) | mixture −6.34 dB = identity mask −6.34 dB (the STFT round trip is exact); **IRM +5.27 dB** (leak 0.031); per song the mixture spans −17.7…−2.1 and the IRM +0.5…+7.1 | no scaling / alignment / boundary bug caps the students: the reference numbers of the training log are the same path. The ceiling on *this* path is **+5.3 dB**, not the +5.9 measured on 40 songs × 60 s in the renderer harness — the 12-excerpt eval is 0.6 dB harder; the students' distance to the ceiling is 5.5 dB, not 6.2 |
| night 27→28.09, superseded (`run_sep4.ps1`) | sep3 stopped at 60k (plateau), its best checkpoint through the DSP at G 1.0 / 1.3; then **BS-RoFormer four-stem pseudo-labels** for the same 774 masters (`teacher_cache4r`, `--model roformer2`, ~4 h); then **sep4** = the band separator warm-started from sep3's best, truth : pseudo = 3 : 1, RoFormer labels, SI-SDR term from the start, 80k steps at 2e-4 → DSP at G 1.0 / 1.3 | queued | the two remaining within-budget levers of review 9: cleaner pseudo-labels and truth-heavier sampling. If sep4 does not clear −0.3 dB by a wide margin, the ~125 M band separator is measured as insufficient and the feasibility experiment is negative |

## 🧪 Review 9 (astra, on the separator student — 2026-09-27 11:30)

1. **Feasibility: plausible enough for one bounded experiment, not
   established.**  No verified published 48 kHz stereo causal < 200 M MAC/s
   MUSDB-like result exists to lean on; +2 dB SI-SDR is a research target,
   not a promise, and aggregate SI-SDR does not guarantee the teacher's rear
   numbers — leakage location and temporal stability matter more.
2. **Diagnosis of sep1/sep2:** (i) capacity *allocation* — a shared hidden-16
   recurrence has almost no source identity or cross-frequency reasoning,
   the budget sits in the 1025-bin head; (ii) the frequency representation —
   narrow features + interpolated output prevent precise discrimination, the
   global mean discards frequency identity; (iii) the `[0,1]` real mask cannot
   restore cancellation or correct phase — its ceiling is unmeasured; (iv)
   two early plateaus make "just train for days" unconvincing; (v) loss
   replacement alone will not rescue it; (vi) "other" is heterogeneous but
   vocals + drums + bass is no more compact.  **First diagnostic: the optimal
   bounded real mask** (ideal ratio mask of the eligible stem, mixture phase)
   through SI-SDR and the DSP — separates an output-type limit from a
   learning failure.
3. **The feasibility model to build (≈ 125 M neural MAC/s at 2048/512):** 48
   unequal bands (narrow at LF), learned band encoders and *direct* bin
   decoders (no interpolation): per-band projection 6 features/bin → 48
   (27.7 M) → shared band GRU 48→32 (34.6 M) → 32→32 (27.6 M) → two
   depthwise/pointwise frequency mixers (10.1 M) → global pooled GRU 32
   (0.6 M) → band + global state → four outputs per bin (24.6 M); inputs =
   normalised Re/Im L/R + log magnitudes + band identity; output = stereo
   **complex ratio masks**, initialised near attenuation, allowed beyond
   unity.  ~25 M reserved for transforms; benchmark real cycles.  Loss:
   complex L1 + multi-resolution spectral, a modest SI-SDR term after
   stabilisation, energy checks.  Data: ~half MoisesDB truth, half
   pseudo-labels from the 3367 masters (RoFormer where it demonstrably cuts
   vocal contamination), duplicates filtered across splits, stem-remix
   augmentation on true stems.  **150k steps, batch 12, 8 s crops, AdamW
   3e-4 warm-up + cosine; 20–60 GPU-hours provisionally.**
4. **Suppression is not safer:** `eligible = mixture − protected` is the same
   residual parameterisation; a conservative subtraction gain *increases*
   protected leakage.  The safe asymmetry is a conservative rear admission
   gain selected against protected leakage.
5. **Ship / commit gates:** the per-cell student + soft veto goes to the car
   now as the next listening build.  Commit the separator to C only when
   full-DEV, level-matched results reach **guitars ≥ 0.18, voice ≤ 0.03,
   ≤ 3/39 swinging**, acceptable drum/bass leakage and measured compute /
   latency; median SI-SDR ≥ +2 dB as a supporting milestone only.  Timeline:
   1–2 weeks feasibility, 2–4 more for quantisation, C integration and
   listening; if the bounded experiment fails, keep the current candidate
   and revise budget or ambition.

## 🧭 Review 11 (astra, on the gap decomposition — 2026-09-27 19:00)

Asked with the sep3/sep5 results, the evaluation-path audit, the gap decomposition and tonight's queue; asked what to change in sep7/sep8 before they start. Ranked answer:

1. **Ranking of causes:** loss weighting plus conditional uncertainty first, inadequate cross-band identification second; the global input scale is plausible but weakened by the usable log-magnitude inputs; phase is irrelevant. Correction to our reading: "no more leakage than the IRM" is too strong — protected-cell leak is comparable for sep3, the **vocal** leak is 3× the IRM's, sep5 exceeds both; and eligible dominance does not make a cell easy to identify from the mixture. **The single most informative test is sep8, judged at matched vocal leakage.** A positive result convicts the objective; a negative one does *not* prove identification failure (loss, features and ambiguity stay coupled), and log-L1 keeps the conditional-median tendency — it changes the frequency weighting, not the ambiguity.
2. **sep8 safeguards (applied):** floor per resolution from the *mixture*, detached, in that grid's units; keep 3e-3 unless a substantial share of eligible treble lies under it (measured: 0.0 %); **one fixed scale on the new magnitude bundle** matching the old bundle's output-gradient norm, otherwise the unchanged 0.5 complex term silently changes weight (measured 0.007). Warning: the floor bounds sensitivity but creates no dead zone — for a silent target the loss is log(1 + |Y|/f) with slope 1/f, so −50 dB punishes tiny leaks hard and can reinforce conservatism; log-L1 is a diagnostic, not a guaranteed recall cure. Next loss if sep8 suppresses further: per-band-normalised linear L1 with fixed training-set scales and a minimum scale. Tonight **no** asymmetric costs, dominance weighting (training-only oracle information in the cost), spectral convergence or earlier SI-SDR.
3. **sep7 unchanged:** the post-temporal band BiGRU (hidden 32) is a clean test of spectral communication; no per-band normalisation, no interleaving, no move before the time GRUs tonight. Compare at matched steps with the same selection rule and include sep3 at 40k beside its 47k best. 180 M tests structure, not feasibility: if positive, test hidden 16 (~140 M) at once.
4. **Transition eval tonight** (`diag_transitions.py`): vocal rises > 10 dB between adjacent 2-s windows with an absolute level threshold, 12-s excerpts (4 s before, 8 s after), capped per song, with instrumental / sustained-vocal / vocal-exit controls; run continuously with state warm-up; measure vocal leak after entry, peak leak and settling time, eligible retention before vs after (normalised to the true stem), the renderer's excess rear-share change vs the oracle; report distributions and worst cases. Training crops unchanged tonight; if failures concentrate at transitions, a later run with 25 % entry-centred crops.
5. **Normalisation is a separate experiment:** the window scalar is non-causal — replace it later by a causal running level with explicit start-up, identical in training and inference; for the treble, fixed per-band training-set scales or causal band levels *while keeping absolute-level features* (unqualified per-band normalisation erases spectral-envelope evidence).
6. **Gates:** repeatable **+1 dB SI-SDR at matched vocal leakage, recall toward ≥ 0.60, meaningful treble recovery**, with renderer gains and no added swings. sep6 wins → supervision is actionable, test teacher labels against truth on shared material; sep7 wins → compress the spectral recurrence to budget; sep8 wins → keep the objective, then causal normalisation and transitions. All fail → close the on-budget separator track and ship mask-drive + the teacher-informed result. Paper wording offered: *"Across the tested architectures, supervision sources, and objectives, compact causal estimators did not approach the teacher's rear-eligible reconstruction at acceptable leakage and stability. These results establish a limitation of the evaluated approach and training regime, rather than an impossibility result for separation within the compute budget."*

## 🔭 Review 12 (astra, on the four-run pattern — 2026-09-27 23:30)

Asked with the table of the four controlled runs (sep3 / sep5 width / sep6 truth-only / sep7 band RNN; sep8 loss queued), the error decomposition of three checkpoints and the transition eval. Headline: **"the wall is demonstrated under-recovery by this model family, not a demonstrated ceiling of causal separation."** Ranked:

1. **What the pattern says.** The controls weaken "just add width / remove pseudo-labels / add frequency communication"; they do not isolate temporal information, optimisation, or the shared representation/output bottleneck. A 43-ms window is *not* 43 ms of context (the recurrent state summarises the past) — **missing future context is the untested axis**, and sep7's frequency-bidirectional GRU does not test it. "Conditional-median hedge" is plausible but not established (complex L1 + magnitude + SI-SDR make something more complicated than a scalar median); the extreme treble deficit also warrants checking loss weighting and the mask parameterisation. The IRM shows the synthesis path can carry the rear *when the sources are known*; it says nothing about learnability from the input. A large non-causal model on the same features is an empirical reference, not a ceiling; a learned frontier needs several protection-weight operating points, compared as recall vs protected leak at matched output level (gain sweeps raise both energies together).
2. **Transitions = source competition**, not a unique diagnosis: the model suppresses shared regions when a voice becomes plausible; missing future evidence and the loss's conservatism both fit; persistent post-entry leak argues against a purely brief onset problem. **First deployable change: 8 frames of lookahead (~85 ms), trained with exactly that availability** — onset evidence without more recurrent compute (account for the STFT latency separately). Then entry-centred crops if onset tails stay poor; a retention-continuity penalty only later and only referenced to the true stem (genuine eligible energy changes across entries; enforcing continuity can preserve vocal leak). Admission smoothing downstream stays useful but cannot restore instruments the separator removed.
3. **Product: ship stability; 0.20 is unproven.** The gain tests already reject amplification. A static air bed is a *perceptual* experiment, not better separation: test it with the separator at its stable gain, sweep the bed level, measure the incremental vocal/sibilant rear energy with guitars, keys and swings; "−12 dB" needs a reference; master treble carries vocals and cymbals, so a bed can violate both protection and eligibility. A guitars+keys target could simplify training if that is the product priority; "vocal vs rest" changes the task and needs independent drum/bass rejection. Neither guarantees "voice never".
4. **If sep8 repeats the plateau, in order:** (i) a parameter-matched **temporal-bidirectional twin** on the same features/labels/objective with seconds of future context, scored on crop interiors at a matched horizon — tests whether accessible future information helps this family (improvement actionable; failure proves nothing about causality); (ii) **the 8-frame lookahead** on the current backbone — the decisive product experiment: better recall at matched vocal leak *and* better renderer shares without added swings; (iii) if onsets still fail, half the crops from training-set vocal entries/exits, sustained controls evaluated too; (iv) only if product scope permits, guitars+keys supervision, reported with the original renderer metrics. **Not:** more width sweeps, phase heads, arbitrary frequency mixers, blind training extensions, gain increases; a large offline reference only if the paper must separate model-family failure from an information limit.
5. **Confounds to resolve:** the 8-s scalar normalisation is non-causal if computed from the complete crop (audit streaming normalisation and crop boundaries); match checkpoints and exposure (sep3's 47k best vs an unfinished sep7 is not equal-horizon; sep7's catch-up from 24k may be the 20k loss change); sep6's lower leak with worse SI-SDR is a legitimate suppression trade; cell-energy leak includes interference — a peak ratio 0.66 is not 66 % of the actual vocal energy; reconcile the vocal-leak summaries and bootstrap by song.

**Closing statement offered:** *"These experiments establish a stable operating point near 0.10 instrument rear share for the tested estimator family. They do not establish a fundamental causal ceiling; budget-compatible lookahead is the highest-priority remaining product test."*

**Applied (night 27→28.09):** `--lookahead L` (output delay + the frame's own band embedding into the decoder; L = 8 → decoder input 112, ~142 M MAC/s) and `--bidir` (time GRUs bidirectional at half hidden per direction, the non-causal twin) in `sep_net.py`; queued **sep9 = lookahead 8** (the product test, after sep8; takes sep8's log loss only if sep8 clears review 11's +1 dB gate) and **sep10 = the bidirectional twin** (the diagnostic, after sep9).

## 🔍 Review 13 (astra, code and protocol review — 2026-09-27 23:20)

Codex read `sep_net.py`, the four diagnostics and the runners (read-only) and ran synthetic checks. Verdict: *"sep8 needs robustness fixes; sep9 needs a decoder-matched control; sep10 is a reasonable diagnostic."* Ranked, with what was done:

1. **Restart / queue integrity (fixed before sep8).** On `--init` the best score restarted at −1e9, so the next eval could overwrite a better checkpoint; and replaying the scheduler after loading the optimizer's LR compounded the warm-up multiplicatively — **10.5× the intended LR at step 1000** (after the cosine milestone the replay happened to be right). Now `last.pt` carries `sched` and `best`, and resume restores both. All launchers could exhaust 60 failures and still render and write DONE: now a run without `train_done.txt` + `sep_net.pt` is marked FAILED and never rendered (the queue still advances), and a DSP log without its summary marks FAILED too.
2. **sep8 loss — composition fine, plus a guard and a corrected scale.** Floors are mixture-derived, detached, per window/channel, at each resolution's own units — correct. Silence gave log(0) − log(0) = NaN (reproduced): floors now clamp ≥ 1e-7. The output-gradient calibration measures what it says but **does not establish equal optimisation pressure**: for Y = M·X the mask gradient carries a factor |X|, so the 1/f output gradients at near-silent cells vanish on the way to the parameters. Measured (`diag_lossfloor.py` §3, 8 batches): at the output-calibrated 0.007 the new bundle's **parameter** gradient was 60–400× smaller than the old bundle's — **sep8 would have trained on the complex term alone.** Parameter-gradient matching gives **`--loss-scale 0.74`** (median old/new 0.74, range 0.42–2.6); runners updated before sep8 started. Floor 3e-3 kept (0 % of eligible energy below it; no evidence for raising or clipping).
3. **sep9 lookahead — timing right, comparison fixed.** The recurrent path sees through t + 8 and no further; e[t] into the decoder is sensible (delaying input features would supply no future evidence); outputs stay indexed to frame t, so evaluate()/separate_array() need no shift. But the skip adds **197k parameters** (784k vs 587k, ~144 M MAC/s): an improvement would conflate context with decoder capacity → **sep9c = the same decoder skip at L = 0** (`--dec-skip`) queued right after sep9. The tail clamps *states*, not masks (e[t] still varies): a trained end-of-stream convention, never for streaming chunk boundaries — comment corrected. Deployment must delay every renderer path by the 85 ms plus the STFT's own buffering; injection alignment in the harness is unverified.
4. **Normalisation — a separate paired run, not tonight.** The whole-window scalar leaks arbitrarily distant future and differs between 8-s training, 20-s eval and whole-file inference, so sep9 is not yet a strict bounded-lookahead model. Proposed streaming form: stereo-linked q[t] = mean over frequency and channel of |X[t]|, s[t] = α·s[t−1] + (1−α)·q[t], α = exp(−512 / (48000·2)) (2-s constant, a starting value), s[0] = q[0], denominator max(s, 1e-6); carry level and GRU states across chunks; identical start-up in training. Test in matched L = 0 / L = 8 retraining.
5. **sep10 twin — fine.** 582k vs 587k is an acceptable match (per-direction capacity halves, so a negative cannot prove future information useless). Keep the same 20-s excerpts as the primary comparison (trimming only sep10 would bias it); add common interior scores and an 8-s context-matched eval for every model later. The MAC formula wrongly kept unidirectional costs — corrected (111 M).
6. **Diagnostics — redo before interpreting tomorrow (in progress).** Recall/leak are output-energy ratios, not source attribution: expose the predicted mask, apply it separately to the true vocal / eligible / protected STFTs and measure transmitted vocal energy against vocal energy on identical support (never rerun the net on isolated stems); standardise on reanalysed audio (`diag_gap` used raw predicted STFTs, the eval reanalyses); reject insufficient-support denominators and report coverage; add energy-weighted band ratios beside the cell-weighted dB bias. `refine()` finds the first block past a threshold, not a directional crossing — select the nearest correctly directed crossing and reject overlapping events; settling must hold within tolerance for three consecutive bins (unsettled = censored); the 0–8 s "peak" can catch another event — report 0–2 s peaks separately and p10 retention for ducking. The bounded-ratio oracle is a reference, not a universal ceiling; negligible learned-phase benefit does not make phase worthless.
7. **Protocol — mostly fine; freeze before sep9.** The absolute +0.75 gate is acceptable but is not +1 dB against a matched-horizon baseline; record the branch once and compare both future-context models against the selected-loss causal control. G 1.0 primary, 1.3 secondary; report fixed-40k and best-within-40k; held-out songs for conclusions beyond the 12 loud-vocal excerpts. STFT layout passed identity reconstruction; float conversion before stem gains is correct; SI-SDR sign correct (monitor its relative gradient after 20k). Shared RNG across prefetch threads breaks reproducibility (deterministic batch seeds — noted, not changed tonight); "half/half" is probabilistic; two-slot teacher caches must not be used (their "rest" includes protected instruments — they are not).

**Queue after review 13:** sep7 (DSP finishing) → sep8 (log loss, scale 0.74) → sep9 (lookahead 8) → **sep9c (decoder-matched L = 0 control)** → sep10 (bidirectional twin) → **sep11 / sep12 (the streaming-normalisation pair: sep9c's and sep9's models with `--norm stream`)**.

### Review 13 follow-up — every finding, its fix, its status (28.09 00:00)

| finding | fix | status |
|---|---|---|
| resume restarted `best` at −1e9 and replayed the scheduler onto the loaded LR (10.5× at 1k) | `last.pt` carries `sched` + `best`; resume restores both | **done** before sep8 (1198eff) |
| runners could render and write DONE after 60 failed tries | FAILED marker; no render without `train_done.txt` + checkpoint; DSP log without its summary → FAILED | **done** for sep8…sep12 |
| log loss: silence → log(0) − log(0) = NaN | floors `clamp_min(1e-7)` | **done** |
| output-gradient calibration ≠ optimisation pressure | parameter-gradient calibration → `--loss-scale 0.74` (was 0.007) | **done** before sep8 started |
| lookahead adds 197k decoder parameters → conflates context with capacity | `--dec-skip` flag; **sep9c** = the L = 0 twin with the identical decoder input | **queued** |
| tail comment said "holds the mask"; it holds the states | comment corrected; convention documented as end-of-stream only | **done** |
| whole-window level scalar is non-causal (a frame moves 1e-2 when a later frame is scaled) and differs between 8-s / 20-s / whole-file | `--norm stream`: stereo-linked causal EMA, τ = 2 s, s[0] = q[0], closed-form per 1024-frame chunk (exact to 6e-7 vs the loop; a frame moves **0** when a later frame is perturbed; 30k-frame songs finite); default unchanged | **done**; **sep11 (L = 0) / sep12 (L = 8) queued** as the matched pair after sep10, **gated**: they run only if sep9 leads sep9c by ≥ 0.5 dB or sep8 clears +0.75 dB, otherwise they mark themselves SKIPPED (the pair only matters to carry a winner into its strictly causal form) |
| bidir MAC formula kept unidirectional costs | corrected (111 M) | **done** |
| recall / leak were output-energy ratios, not attribution; `diag_gap` on raw STFTs while the eval reanalyses | v2: frozen mask on the true stems, reanalysed, coverage, energy-weighted band ratios, IRM reference | **done**, rerun for sep3/5/6/7 |
| `refine()` not a directional crossing; overlapping events; settling without tolerance; 0–8 s peak | v2: nearest directed crossing, overlap rejection, 3-bin tolerance with censoring, 0–2 s peak, p10 ducking | **done**, rerun |
| medians on 12 excerpts without uncertainty | song-level bootstrap 90 % CI on SI-SDR / recall / vocal leak in `diag_gap` | **done** (next runs) |
| shared RNG across prefetch threads | deterministic per-batch seeds `[seed, batch index]` | **done** (sep9 onward) |
| +0.75 gate is absolute, not matched-horizon | decision table for the morning: sep9 vs **sep9c** (same loss, same 40k), sep10 vs sep8/sep3 at 40k *and* best-in-40k; G 1.0 primary | **protocol noted** |
| common interior scores + 8-s context-matched eval for sep10 | post-hoc diagnostic on the checkpoints (not in `evaluate()`, which must stay fixed across the night) | **morning** |
| deployment must delay every renderer path by the 85 ms + STFT buffering; injection alignment unverified | harness: the estimate stays indexed to frame t, so no shift; the hook-side delay is Track A's when/if a lookahead student is chosen | **noted** |
| two-slot teacher caches must not be used | not used (teacher_cache4 is four-stem) | n/a |
| IRM is a reference, not a ceiling; learned-phase benefit ≈ 0 does not make phase worthless | wording in the plan and the paper | **noted** |


## 🚀 Review 14 (astra, on throughput and a next generation — 2026-09-28 00:40)

Asked how to get 3–5× more answers per night from one GPU (a controlled run costs 2.7 h: 2.1 h training, 35 min renderer harness, 10 min diagnostics) and whether to stop iterating on the band model and design a next generation from what was learned. Headline: **"Do not abandon the band model yet; abandon architecture-only iteration."** Four variants converging on the same transmission and renderer behaviour point at a shared target / loss / conditioning bottleneck; the students already match the ideal mask's vocal transmission while discarding far more eligible energy — evidence *against* simply strengthening suppression.

**A. Throughput, ranked.** Removing every minute of evaluation and diagnostics gives only 1.36×; 3–5× needs faster training, validated rejection of weak runs, or more hardware. First change: a **sequential precision benchmark** on identical batches and checkpoints (fp32 vs TF32 vs bf16 autocast, fp32 kept for the sensitive reductions and SI-SDR; measure real cuDNN GRU time, memory, gradient and loss agreement — do not assume autocast speeds GRUs up). Then: (1) precision + batch tuning, provisionally 1.2–1.7×, possibly zero with these GRUs; linear LR scaling is not automatically valid; (2) a **screening harness** — 20 songs at one gain, ~9 min instead of 35, ~1.2× overall; keep the entry/exit failures and hard songs; finalists always get 40 × 2 (screening cannot certify ≤ 5/39); (3) the harness on CPU overlapping the next training — hides most evaluation time *if* student inference also runs on CPU and the process never initialises CUDA, after a contention soak test (three Track A harnesses halved our step rate earlier); (4) a second GPU / cloud training worker ≈ 2× independent capacity — moving only diagnostics helps much less; two workers plus measured training gains are the credible route to 3×; (5) eval cadence and storage — evaluate less often before the loss switch, keep transition checkpoints, move caches and checkpoints off WinBtrfs to NTFS SSD if the stalls are real; (6) **early stopping is unvalidated** — four crossing trajectories cannot support it; reconstruct prospective decisions from complete curves and test them on held-out completed runs, requiring a low false-rejection rate on *renderer* winners. Also: 4-s crops are a different recipe, not a shortcut; SI-SDR from step 1 is an objective experiment, not a licence for early stopping; `torch.compile` low priority (fused cuDNN GRUs leave little to compile); two 7-GB trainings on one GPU is an unproven stability experiment, nowhere near 2×; pre-rendered crops only if profiling shows input stalls, and as a regenerating cache.

**B. Next generation, ranked.** (1) **A task-specific hybrid: compact band separator + explicit protection conditioning + coarse gain controller.** Predict the rear-eligible audio with guitars/keys weighted heavily; feed a small vocal-presence / protection representation into the separator so protection *conditions recovery* instead of triggering a global veto whenever singing becomes plausible; predict coarse gains plus a limited-resolution residual mask (one real mask shared by both channels at first); train entry/exit continuity explicitly by penalising *unexplained suppression of the eligible stems* while keeping the vocal-leak penalties and fast genuine protection; train against teacher **audio**, truth stems, multi-resolution spectral losses and **source-attribution losses**; a differentiable renderer surrogate only after validating its ranking against the real C renderer, which stays the selector. (2) A compact streaming BSRNN-style backbone with correctly specified causal normalisation — inside that hybrid, not as another capacity increase; bidirectionality is an oracle unless bounded to the permitted future. (3) The narrow guitars+keys target with separate protection supervision — matches the owner's priority, reduces target ambiguity, may drop other rear content. (4) More truth data and representation distillation only afterward: MUSDB18-HQ has no isolated guitar/key targets, Slakh has labels and controlled overlaps but a synthetic-domain gap; teacher intermediate features may encode future context the student cannot have. STFT: start at 1024/256 plus cheap causal longer-context features; count analysis, output delay and synthesis latency together — 85 ms lookahead is not 85 ms end-to-end. **150 M MAC/s does not establish Cortex-A9 feasibility**: FFTs, non-linearities, memory traffic and real int8 kernels need device measurements.

**C. The 48 hours.** Tonight: finish sep8 / sep9 / sep9c / sep10 (loss, decoder conditioning, permitted lookahead, future-context headroom); stop width-only and unconstrained architecture sweeps. Tomorrow daytime: the precision benchmark, the screening harness, the CPU-overlap soak test; compare every finished model on identical attribution and transition subsets. Second night: the normalisation pair, then **a matched baseline / hybrid pair on the winning recipe** — the highest-value experiment, with promotion *targets* (not predictions): eligible transmission **≥ 0.45** (vs 0.29–0.37), vocal transmission no worse than the matched baseline, materially reduced entry suppression, renderer **guitars ≥ 0.15, voice ≤ 0.02, ≤ 5/39** on the full harness; SI-SDR stays diagnostic. Teacher-level 0.20+ is **plausible but unproven** at this budget; if it does not come, ask first for a stronger core and **3–5× compute headroom**, then latency; keep the measured voice constraint and never promise "voice never" on unseen music.

## ⚡ Review 15 (astra, on the rapid test loop — 2026-09-28 01:20)

The owner asked for a fast loop with a trustworthy reference point. My design (4k-step GPU runs + attribution / transition diagnostics + a 20-song CPU screen + a leaderboard, validated by 4k/10k/40k pairs) went to Codex. Verdict: **build it as a paired mechanism screen with staged promotion (4k → 10k → 40k), not a miniature substitute for 40k training** — 4k can show that an intervention changes the mask's behaviour; whether the change survives training and improves rendering is a later question.

1. **Proxy.** Keep the full recipe (LR schedule, warm-up, batch, crops, SI-SDR at 20k) so the proxy predicts the recipe; a short cosine or SI-SDR-from-1 is a different recipe needing its own transfer validation. At 4k promote promising *or ambiguous* candidates, reject only clear failures. Early metrics by usefulness: eligible transmission **at matched vocal transmission** (incl. eligible-dominant passages) → entry ducking / exit release with absolute pre/post transmission and vocal leak → air transmission on excerpts that *have* 8–14 kHz eligible energy → renderer guitars/keys + rear level + oracle error jointly → SI-SDR and swings/20 (descriptive only). Two traps: a transparent mask "improves" transmission and ducking while failing separation (every recovery needs the vocal-leak constraint); a permanently suppressed mask has perfect ducking. Validation needs **8–12 distinct interventions** with better / worse / unchanged final outcomes (checkpoints of one run are not independent), ~80 % directional agreement outside the uncertainty band, no confidently promoted vocal-leak regression, confirmation on 3–4 later interventions; bootstrap paired differences by song. Four tied final outcomes validate nothing.
2. **GPU.** Exclusive ownership: interleave rapid runs with the long runs' CPU harness windows, or pause the queue at a clean boundary; two tenants fitting in 16 GB proves nothing about throughput or reliability, and the native crashes make concurrency avoidable uncertainty in the reference experiment. Benchmark precision separately (speed, stability, short-run metric agreement), then freeze it across candidates and controls.
3. **Anchor and deltas.** The anchor = the frozen current recipe at the same training age, matched seed / data order / eval / precision / renderer config, code hashes recorded; 40k models and teacher / oracle / IRM as context only. Run a second anchor seed immediately, three seeds for finalists. Promotion triggers (engineering starting points, not significance): eligible transmission **+0.03** with vocal transmission up ≤ **0.01**; entry ducking **10 points closer to unity** with p10 and absolute transmission not worse; guitars/keys **+0.02** under voice / level / oracle-error guardrails; effects must exceed measured run variability, with paired song-level uncertainty.
4. **First eight rapid experiments** (one factor each vs the frozen anchor): (1) anchor replication, second seed; (2) **attribution loss** — the predicted mask on the aligned truth stems, vocal-output energy + eligible reconstruction-error losses normalised by active source energy with floors, auxiliary gradients at 10–20 % of the baseline's; (3) **band-balanced reconstruction** — per-band L1 / floored target-band energy, capped weights (does weak-band gradient explain the treble erasure?); (4) **oracle activity conditioning** — truth-derived vocal activity concatenated to every frame, labelled non-deployable (the ceiling of conditioning before building a detector); (5) **entry-centred sampling** — 25 % of crops from vocal entries with pre-entry context; (6) **guitars+keys target** with the original evaluation target kept; (7) **bounded shared mask** — one real sigmoid mask for both channels; (8) **STFT 1024/256** with matched receptive field, compute and latency reported. Read sep8 before spending a slot on log-loss floors; defer an entry-specific suppression penalty (it can reward continuity where overlap requires suppression).
5. **Self-deception guards.** Freeze the 20-song screen and the transition definitions; reserve untouched songs for promotion checks (choosing from DEV repeatedly turns it into training feedback); include sustained vocals, sparse eligible content, cymbals, hard pans, difficult entries; identical alignment / channel / renderer-state / gain across references; per-song deltas and failures beside aggregates. **A 4k gain is "an early mechanism response" until it survives 10k, the SI-SDR switch and final rendering.**

**sep8 at 26k (attribution, 28.09 01:11; the log loss, still training):** SI-SDR −0.80, eligible transmission **0.36** (sep3 best 0.29), vocal transmission **0.167** (sep3 0.124; IRM 0.105), protected 0.072; bands <250 −8.3, 250–1k −3.9, 1–3k −3.0, 3–8k **−2.6** (sep3 −4.4), **8–14 kHz −8.3** (sep3 −13.6, sep6 −22.2), >14 kHz **−10.0** (sep3 −14.6). The first *shape* change of the night: the log loss restores the air by 5 dB and the presence band by 2 dB — at the price of more vocal transmission (0.167, above the ideal mask's) and a lower SI-SDR. Whether that is a better operating point or the same frontier is the renderer's call (voice share vs guitars at G 1.0) at ≈02:25.

**The rapid queue (28.09 01:25, in order; each ≈ 25 min once the GPU is free):** `anchor_s1` (the frozen recipe), `anchor_s2` (`--seed 2`, the noise floor), `logloss` (sep8's recipe at 4k — the first 4k/40k validation pair), `bandbal` (`--bandbal 0.026`), `attr` (`--attr 0.069`), `sharedmask` (`--mask shared_sigmoid`). Aux weights from `diag_auxgrad.py`: 15 % of the baseline's parameter-gradient L2 at init (band-balanced L1 at weight 1 has 5.7× the baseline's gradient, the attribution loss 2.2×). Context rows on the same 20-song screen: `ctx_sep3best`, `ctx_oracle`, `ctx_irm`, `ctx_teacher` (CPU, running). Soak check: one CPU screen (2 render jobs + CPU student inference) beside sep8's training left its step rate at ~200 s / 1000 — no measurable contention. Implemented experiments: (2) `--attr W` = transmitted vocal energy / vocal energy + eligible reconstruction error / eligible energy, floored, on the predicted mask applied to the batch's true stems; (3) `--bandbal W` = per-band magnitude L1 / floored target-band level (cap 100×); (7) `--mask shared_sigmoid` = one sigmoid mask for both channels (387k params, 107 M MAC/s). Not yet: (4) oracle activity conditioning, (5) entry-centred crops, (6) guitars+keys (needs a re-cache: the 7-slot cache folds guitars and keys into "other"), (8) 1024/256.

**Built from this review:** `rapid_daemon.ps1` (a queue file of `tag|args` jobs; holds `D:\upmix\runs\GPU_LEASE` while jobs are queued — the long runners wait on it — trains 4k with the full recipe and `--save-at 4000`, runs the attribution and transition diagnostics on the GPU, releases the GPU, runs the frozen 20-song screen on the CPU, and rebuilds the leaderboard with `rapid_board.py`), the anchor (`anchor_s1`, `anchor_s2` = second seed), and the context rows (teacher, oracle, IRM, sep3 best) on the same 20-song screen.

### sep9 result (lookahead 8 = 85 ms, 28.09 06:40)

sep3's backbone + 8-frame output delay + the frame's own band embedding into the decoder (784k params, 144 M MAC/s), sep3's loss. SI-SDR −1.20 / −0.98 / −0.94 / −0.65 / −0.68 at 3k / 10k / 20k / 30k / 40k, best −0.63 at 29k, vocal leak 0.10–0.13. **Renderer G 1.0: voice 0.03, guitars 0.10, keys 0.11, bass 0.06, drums 0.09, 4/39 swinging, level −38.6, error 0.14; G 1.3: voice 0.04, guitars 0.16, keys 0.16, 14/39.** The same frontier as sep3 / sep5 / sep7: 85 ms of future evidence does not move the plateau or the renderer. The decoder-matched control (sep9c) and the bidirectional twin (sep10) stay paused per review 16 — with no gain over sep3 there is nothing for the control to attribute.

### The shared-mask promotion pair at 10k (28.09 evening)

| 20-song screen, G 1.0 | anchor 4k (2 seeds) | sharedmask 4k | anchor 10k | sharedmask 10k |
| --- | --- | --- | --- | --- |
| Eligible / vocal transmission | 0.42–0.47 / 0.20–0.24 | 0.48 / 0.171 | 0.39 / 0.226 | 0.44 / 0.247 |
| Air 8–14 kHz | −23…−24 dB | −28.8 | −21.8 | −19.7 |
| Entry ducking median / p10 | 0.75–0.76 / 0.39–0.43 | 0.83 / 0.58 | 0.79 / 0.40 | 0.84 / 0.43 |
| Guitar / piano / keys / strings | 0.11–0.12 / 0.06–0.07 / 0.09–0.10 / 0.13 | 0.11 / 0.09 / 0.11 / 0.16 | 0.12 / 0.07 / 0.11 / 0.11 | 0.13 / 0.08 / 0.11 / 0.12 |
| Depth follows level: guitar / wind | +0.11…+0.12 / −0.10…−0.23 | +0.02 / +0.03 | +0.13 / −0.15 | +0.05 / −0.15 |
| Swings (all stems) | 2–3 | 1 | 3 | 5 |

**Not promoted.** At 10k the shared mask keeps a smaller guitar depth-follows-level (+0.05 vs +0.13) and a little more air and guitar, but the entry-ducking p10 falls back to the anchor's band (0.43 vs 0.40, inside seed noise), the vocal transmission rises above the anchor's (0.247 vs 0.226) and it swings more (5 vs 3). The 4k stability signal was an early mechanism response that did not survive training — exactly the case review 15 warned about, and the first proxy-validation data point: a 4k gain in ducking / swings did **not** predict 10k.

## 🛑 Review 16 (astra, "just training separators is so-so" — 2026-09-28 05:00)

Verdict: **stop broad separator training for 2–3 days.** Five controlled runs gave no useful jump, several exceed the budget, and vocal transitions still move the instruments; the shared-mask result matters because it changes the *failure mechanism* (stability), not because it separates better. Next, in order:

1. **Owner's in-car listening test first (highest expected value).** Four conditions on identical excerpts through the same chain, level-matched, blind, repeated: front-only stereo, Track A deployable, shared-mask candidate, teacher-fed renderer (aspirational). Excerpts with vocal entries/exits, sustained instrumentals, dense choruses, exposed vocals, some outside the DEV set. Questions: instruments clearly behind? any voice behind? does an instrument move or duck when singing starts? would you use it daily? If even the teacher disappoints → rendering / cabin presentation, not extraction; if the teacher delights and all deployables fail → extraction is the real bottleneck.
2. **Persistent rear membership by construction (Track A controller):** separate *membership* (does this stream belong in the rear — slow, hysteresis, minimum dwell, conservative admission) from *protection* (is its current audio safe — fast). Not a smoothed mask (that prolongs vocal leak). Log membership changes and eligible-audio loss separately.
3. **Optimise the product directly, without neural training first:** 10–30 controller parameters (admission thresholds, hysteresis, dwell, protection strength, gain envelopes) searched with bounded search / CMA-ES against the real C renderer, as constrained optimisation (voice / bass / drums / stability as constraints, maximise instrument delivery across families, with entry/exit ducking, depth-follows-level, rear spectral balance, a minimum useful rear level) — otherwise silence wins; validate on untouched songs.
4. **Oracle-cue upper bound** for a cheap decomposition: ground-truth vocal / drum / bass cues drive the same persistent controller while the mixture is routed; train small causal cue estimators only if the oracle succeeds.
5. Separator as optional *admission evidence* (sustained agreement), never as the rear audio or a frame-wise gain.
6. Half a day of real head-unit profiling (deadline misses, memory traffic, thermals, latency) before assuming more than 150 M MAC/s.
7. Multi-second lookahead is a separate product decision: media buffering is not automatically accessible future PCM; measure where decoded audio exists and seek / pause / track-change behaviour first.

**Applied 05:00:** paused `attr`, `sharedmask_log`, sep9c, sep10, sep11, sep12 (runner files kept; relaunch = start the `run_*.ps1` / re-add the queue lines; note in `D:\upmix\runs\rapid\PAUSED.txt`). Kept: sep9 (already half-trained) and the sharedmask / anchor pair at 10k.

**Playback correction (later 28.09):** the four-condition in-car proposal above cannot be run as written. Live CarPlay supplies stereo PCM and the head-unit hook performs the upmix, so only front-only and Track A's shipped build currently run in the car. A student joins after a Track A-owned hook port and device profile; the non-causal teacher cannot join. The [CarPlay listening protocol](UPMIX_CARPLAY_LISTENING_PROTOCOL.md) gives the executable two-condition car test and a separately labelled rear-only headphone preview for student versus teacher. Neither six-channel WAV playback nor `qsa_play` proves the car chain.

**Oracle renderer full-40 result (later 28.09):** with true eligible audio injected into the real C renderer, rear air −40 dB reduced unique any-stem swings 8 → 5/40 but lowered rear RMS by 0.575 dB, just outside the frozen ±0.5 dB gate. Front retention 0.75 reduced swings 8 → 1/40 but lowered the six-song median string rear share to 0.134, below the 0.14 target. Neither passes the full-harness gate as tested. An explicitly posthoc screen of surround 0.427 with air off and retention 0.70 is prepared. The separate frozen 20-song TEST split is the independent validation set; all 40 DEV songs have already influenced selection. Details and paired results: [oracle renderer probe](UPMIX_ORACLE_RENDERER_PROBE_2026-09-28.md). The production-mask screen uses Track A's frozen v21a binary without oracle injection; it must be read separately before proposing a live candidate.

**Production-mask knob screen (later 28.09):** with Track A's v21a binary and no oracle injection on the enriched 20 DEV songs, baseline had 15 any-stem swing songs and voice/bass/drums rear shares 0.075/0.150/0.285. Rear air off reduced swings only to 14 and protected shares to 0.064/0.136/0.168; front retention 0.75 reduced swings to seven but protected shares remained 0.058/0.128/0.230 while guitar and keys fell below target. Neither is a candidate. The paired result supports a mask-selectivity bottleneck for these simple renderer changes, while the oracle result isolates a separate renderer stability limit. See [production probe](UPMIX_PRODUCTION_RENDERER_PROBE_2026-09-28.md). The posthoc oracle full-40 knob adjustment is running; no live or device conclusion follows from it.

**Exploratory oracle knob follow-up (later 28.09):** air off with surround raised to 0.427 restored the rear level but created two new swing songs, for 9/40. Front retention 0.70 gave 4/40 swings, guitar/keys/piano/strings 0.185/0.187/0.128/0.140166, voice/bass/drums 0.009/0.013/0.068, unchanged rear RMS and slopes within ±0.05. It passes the measured full-40 C-render gates, but was chosen after inspecting that set and clears the string limit by only 0.000166. The frozen independent 20-song TEST render is now running. This is still perfect injected eligible audio, not a causal model, device result or listening verdict; see [oracle renderer probe](UPMIX_ORACLE_RENDERER_PROBE_2026-09-28.md).

**Independent oracle TEST result (later 28.09):** on 20 previously frozen, nonoverlapping TEST songs, retention 0.70 cut any-stem swings 5 → 2/20, but keys were only 0.128 against the unchanged 0.15 target. There were five keys songs, one string song and no measurable wind slope. The independent render gate fails; the apparent full-40 pass is not a robust validation. The user's offered 0.12 keys threshold had not been adopted before inspecting TEST. Do not retune on this holdout. It is not the goal's literally named untouched DEV subgroup, and the oracle signal remains non-deployable. See the [probe report](UPMIX_ORACLE_RENDERER_PROBE_2026-09-28.md).

**Static product-knob CMA-ES pilot (later 28.09):** 18 bounded candidates, ten settings, six fixed enriched DEV songs, Track A's v21a mask and the real C renderer, no oracle injection. None met the small-screen constraints. The lowest scalar score had 0/6 swings but rear RMS 4.03 dB below baseline and guitar/keys 0.089/0.072, with voice/bass/drums still 0.029/0.127/0.141; a stronger-instrument point had guitar/keys 0.206/0.164 but protected 0.062/0.149/0.296 and 3/6 swings. This search tests existing static settings, not the proposed new persistent source-membership controller, and does not prove a global budget limit. Do not promote a point to full40 or the car; see [CMA renderer pilot](UPMIX_CMA_RENDERER_PILOT_2026-09-28.md).

### Review 16 closeout and architecture decision record (2026-09-28)

The [handoff investigation ledger](UPMIX_SEPARATOR_HANDOFF.md) consolidates every completed separator, membership, oracle-renderer, production-renderer and CMA iteration, including failed promotions, paired metrics, recovery steps and current PC state. The night report's queue dates and the early review-16 proposal are historical snapshots. **There is no deployable Track B candidate, device benchmark or owner listening verdict.** The 20-song validation set is frozen **TEST**, not “untouched DEV”; all 40 DEV songs have been inspected. The independent oracle retention-.70 result misses the unchanged keys ≥0.15 gate (0.128), and that upper bound uses noncausal true eligible audio.

**What the failed iterations actually isolate:** (1) sep9's 85-ms lookahead reached the old delivery/leakage frontier, and the shared-mask 4k stability gain reversed at 10k; (2) simple global membership and even true-stem activity latches failed because they did not preserve a useful source identity; (3) true eligible audio still swung 8/40 in the original C renderer, including six of eight swing songs without a qualifying vocal entry, so motion is not solely a voice-entry effect; (4) simple air/retention knobs helped the oracle screen but not the real v21a mask screen, where voice/bass/drum leakage stayed high; (5) static ten-knob CMA favored a quieter rear and produced no feasible point in 18 trials. These are bounded negative results for the tested models, controllers and knobs, not a proof that the goal is impossible on the head unit.

**Next architectural branches, in order of information value:**

1. Obtain the owner's [live CarPlay listening score sheet](UPMIX_CARPLAY_LISTENING_PROTOCOL.md) for front-only and Track A. The student and teacher are separately labelled offline headphone previews until a student is ported by Track A; there is no valid six-channel WAV or `qsa_play` shortcut to the car chain.
2. Prototype **persistent source identity with fast protection** as a new offline controller. Membership must encode continuity of a guitar/key/string stream across vocal entries, with slow admission and dwell; separate fast voice/bass/drum rejection must prevent transient rear leakage. Compare directly to the failed activity latches, and log both identity changes and lost eligible audio. Run a new **oracle identity/cue upper bound** through the C harness before training estimators. The earlier true-stem protection weights were not this experiment.
3. If that bound is good, build only the small **causal** cue estimator it requires and measure int8 load and latency on device (≤150 M MAC/s, ≤85 ms added). The current separator's float MAC estimate and whole-window normalization cannot certify this. If the bound is poor, investigate **renderer/cabin presentation** and front/rear depth before another separator sweep. The front-retention knob can lower the rear-share denominator without proving audible depth.
4. Treat voice/bass/drum, swing, useful rear level and instrument shares as **hard feasibility gates** in any controller search; then rank feasible points. The six-song CMA pilot showed that a scalar penalty can prefer near-silence. Preserve song-level pairing and the frozen TEST set; require full40 plus independent validation and the owner's ear before deployment.

Any hook change belongs to Track A. Track B may prepare PC-side prototypes and evidence without touching `hook/audio/hssr_mask.*` or the int8 port. Do not restart the head unit, install into the PC environment while runs use it, or expose NAS credentials or the MoisesDB link.

### Review 17: proceed offline before listening (2026-09-28)

The owner asked to continue without waiting for the car listening session. The numerical failures already justify changing the **current implementation architecture**; listening is still required for the final product verdict. Correct the earlier review-16 priority: a persistent-membership latch on the original mixture cannot separate protected audio already present in the admitted cells. The true-stem oracle mask × mixture had **14/40** all-stem swing songs, true eligible audio **8/40**, and ideal bounded real-mask **audio** **7/40**. The last result also prevents the stronger claim that a real mask representation is fundamentally incapable. The trained sep3–9 estimators, their losses/temporal model/normalization and the renderer interface are the tested failure, while the bounded ideal mask remains a substantial unachieved target. The renderer is a separate architectural problem because true eligible audio itself still swings.

**Offline decision sequence:**

1. Run the ideal bounded mask/audio baseline and the **already selected** front-retention-.70 condition on the original full40 through the real C renderer. [`run_irm_renderer_full40_ret070.ps1`](../../tools/stereo-upmix-render/pc/run_irm_renderer_full40_ret070.ps1) uses the frozen oracle full40 song manifest, verifies a new baseline against the original `iface8/inject_irm` stem rows within .005, and checks the same hard gate. It is a CPU-only bound; the knob was selected on DEV oracle audio, so even a pass is exploratory, not a new holdout or deployable result. The run must not select further values on TEST.
2. Diagnose the original oracle-audio swing windows as a **front/rear object-ratio** problem. Test a stable object/front split with true eligible audio, then with ideal-mask audio, maintaining rear RMS and instrument families. A global retention increase can make the rear-share fraction steadier by increasing the front denominator, as the .70 TEST keys failure illustrates; that is not a robust renderer fix.
3. Once a renderer path survives both bounds, train a small **causal eligible-audio estimator** against the remaining gap to the ideal mask/audio target. Model selection needs source-attributed vocal-entry retention and the real C harness, not SI-SDR alone. Slow identity is an optional part of this estimator if a demonstrated continuity error remains; it is not a replacement for extraction. Freeze gates and compare at matched gain and training age before any 40k promotion.
4. Keep live CarPlay listening, unit int8 MAC/s and ≤85 ms latency as later product gates. There is no need to wait for the owner to run offline falsification tests, and no basis yet to port a candidate into Track A's hook.

**Review-17 result:** the [architecture-gate report](UPMIX_ARCHITECTURE_GATE_2026-09-28.md) records the completed preselected ideal bounded-mask/audio full40 test. Front retention .70 cut any-stem swings **7→3/40**, no new, rear RMS unchanged, but strings **.169→.132 < .14**. This is a failed gate despite ideal mask information. A corrected diagnostic reproduced the ten original true-audio swing-window share differences within .001 and decomposed output energy per unit of true stem energy: median rear change **+6.14 dB**, front **−2.20 dB**, rear-side larger in **10/10** traces. Air off reduced rear change to **+5.04 dB** but left six of ten original trace windows above .30; front retention .75 left rear change **+6.14 dB** while reducing the front-side change and leaving one of ten original windows above .30. The comparisons use existing C outputs at fixed windows, not new full-song scores or a listening verdict. The target for the next host-only renderer prototype is the **rear object/bed path at retained instrument level**, not another global front-retention adjustment. The TEST20 oracle keys failure has already been inspected; use a new independent set to validate future architectural selections.

### Review 18: host-only direct-object renderer bound (2026-09-28)

The owner authorized continuing the offline architecture investigation without listening. [`build_direct_object_host.py`](../../tools/stereo-upmix-render/build_direct_object_host.py) compiles a new test-only path from a **copy** of Track A's C renderer outside `hook/audio`, plus an unmodified-source control. For injected true eligible audio, the path sends the isolated stereo object directly rearward, uses the same signal for front transfer, and bypasses dynamic pan projection and the universal mixture fallback. A second arm also removes the ambience/air bed. These are host executables, not a head-unit patch; their hashes and exact modifications are in the [direct-object report](UPMIX_DIRECT_OBJECT_RENDERER_PROBE_2026-09-28.md).

The frozen enriched-20 screen contained the eight original oracle swing songs and twelve deterministic controls. Control C outputs reproduced 115 old stem rows within .005. Direct object with the old bed stayed at **8/20** swings, resolved one and created one; removing the bed gave **6/20**, resolving only two. Removing rear body with surround .40 gave **2/20** swings but raised rear RMS **2.114 dB**. A single surround coefficient **.3235**, calculated from the twelve nonswing controls' paired +1.843525 dB rear-RMS shift and frozen before the rerender, gave **0/20** swings, all eight resolved, none new, rear RMS +.270 dB, passing the predefined mechanism screen. This is an iteration sequence, not isolation of one internal DSP term.

The **same** binaries and coefficient then passed every measured true-audio full-40 C-render gate: **8→0/40** any-stem swing songs, guitar/keys/piano/strings **.233/.235/.242/.231**, voice/bass/drums **.0038/.0084/.0097**, rear RMS **+0.472 dB** versus the verified control (close to the ±.5 dB limit), and guitar/keys/wind level slopes **+.0126/+.0132/+.0050**. All 231 fresh control rows matched the saved baseline within .005. This shows an architecture change in the renderer can eliminate the measured motion **given true isolated eligible audio**, while maintaining the numerical rear level and source shares. It is a DEV-selected upper bound, not a causal model, device benchmark or listening result. It also does not include the required source-attributed entry-ducking check.

The frozen renderer then passed the **ideal bounded real-mask/audio full40** without retuning: **7→0/40** swings, guitar/keys/piano/strings **.205/.215/.213/.208**, voice/bass/drums **.0044/.0112/.0090**, rear RMS **+0.075 dB**, and level slopes within ±.05. The 231-row control again matched the saved baseline within .005. This is a noncausal representation upper bound and a clear correction to any claim that bounded real-mask audio itself is incapable; the trained estimators have not approached this bound. Neither DEV40 set is an independent validation of this selected architecture. All 40 DEV songs and 20/24 MoisesDB TEST songs have already been inspected, leaving no honest new 20-song independent set in this split.

The fixed point also produced **5→0/20** swings on reused TEST20 true audio and **4→0/20** on reused TEST20 ideal-mask/audio, with no new swing, all observed family/protection/level limits met. Both formal TEST20 verdicts remain **NO** because this set contains **no valid wind slope**; keys have only five songs, strings one. The true-audio TEST rear RMS shift was **+0.490 dB**, near the ±.5 bound; ideal-mask/audio was **+0.044 dB**. This is secondary robustness, not untouched validation, and the numerical evidence still lacks listening and device tests.

The already-trained sep9 best checkpoint was then compared on enriched DEV20 with **byte-identical injected audio** in original versus selected host C, no new weights. The renderer change cut swings **6→1/20**, resolving five with none new, rear RMS −.366 dB and acceptable level slopes. But guitar/keys/piano remained **.105/.144/.089** and bass **.070**, missing .15/.15/.10 and ≤.06, while voice/drums improved to **.019/.016**. The screen therefore rejects full40 promotion. This is the direct separation of bottlenecks: the selected renderer route can meet the numerical gate with true or ideal-mask audio, while the existing sep9 estimator cannot provide the necessary eligible/protected content. This does not establish an impossibility bound for another causal estimator. Details, binary/checkpoint hashes and PC paths are in the [direct-object report](UPMIX_DIRECT_OBJECT_RENDERER_PROBE_2026-09-28.md).

### Review 19: matched causal source-loss screen (2026-09-28)

The owner asked to continue the offline architecture work without listening. A budget-sized (`107 M` estimated neural MAC/s), zero-lookahead, streaming-normalized, shared-mask anchor and source-attribution candidate were trained sequentially to 4k with identical seed/data/order. The candidate alone adds explicit eligible retention and vocal/drum/bass transmission terms, with a gradient-calibrated weight frozen before training. This is a mechanism screen, not a device-causal or int8 claim; whole-file Python inference still needs a stateful streaming implementation. The [causal source-loss report](UPMIX_CAUSAL_SOURCE_PAIR_2026-09-28.md) records the runner, gradient calibration, checkpoints, diagnostics, source hashes and exact gates.

The fixed 12-song diagnostic found median eligible recall **.46→.49** while vocal transmission increased **.156→.195** and drum/percussion transmission **.070→.088**. On 43 vocal entries, post/pre eligible retention median/p10 changed **.835/.588→.847/.618**: p10 crossed .60, median still missed .85, and early vocal leakage increased. The real host C screen on the same enriched DEV20 and frozen direct/no-bed/body-0 renderer yielded **0/20** swing songs for both; candidate guitar/keys/piano/strings **.118/.156/.110/.189**, voice/bass/drums **.0218/.0822/.0098**. Anchor guitar/keys/voice/bass were **.104/.143/.0157/.0680**. Paired median rear level increased **+.668 dB**. Inputs were byte-identical between C conditions within each model. The precommitted 4k→10k rule failed on eligible recall (+.03 < +.05), guitar share (+.016 < +.02), bass share (+.013 > +.01) and level (>+.5 dB). Neither 10k extension nor full40 student promotion follows.

The direct C renderer fixes measured swing with good isolated or ideal-mask audio. This source-loss attempt mainly opens a weak estimator mask, carrying more protected audio and increasing rear level. It does not isolate an inherent impossibility of the mask representation: ideal bounded-mask/audio passed full40 numerically, and causal 4k training is far from that bound. The next offline model comparison should test **selective source ownership at matched rear level** with the same preselected source-attribution, entry and C gates. A genuinely independent real-stem set remains unavailable locally; the 774 owner-library cache entries are teacher pseudo-labels, and the MoisesDB DEV40 and TEST20 already inspected cannot be called untouched validation. No Track A or head-unit file was changed. Device cost/latency, independent validation and owner listening remain open.

### Review 20: causal four-role ownership and true-target bound (2026-09-28)

The [four-role report](UPMIX_CAUSAL_ROLE_PROBE_2026-09-28.md) tests a causal 125 M estimated-neural-MAC/s variant whose shared eligible mask is one of four competing probabilities (eligible, voice, drums/percussion, bass). It uses the prior 4k shared-mask anchor, matched data/seed/age and a gradient-calibrated four-role power CE at 30% of the audio-loss gradient. The model's fixed 12-song reanalyzed eligible transmission fell **.462→.351**; vocal transmission fell **.156→.110**, but vocal/eligible rose **.405→.435** and bass/eligible **.0424→.0434**. All four precommitted source-selectivity gates failed. Vocal-entry retention on 43 events worsened **median/p10 .835/.588→.788/.539**. The improved scalar DEV vocal-leak score therefore came from closing the mask, not useful source selection.

The same frozen C path on enriched DEV20 gave **0/20** swing songs for both trained models. A single gain calculated from the raw paired rear-RMS difference of −.4827 dB yielded **+.0001 dB** actual matched level on rerender, with byte-identical injection within each C condition pair. At matched level the four-role model's guitar/keys/piano/strings were **.100/.165/.110/.207**, voice/bass/drums **.0207/.0834/.0127**. It misses guitar, voice and bass limits. Neither 10k training nor full40 student promotion is justified.

The privileged true-stem four-role **power target**, however, gave median eligible/vocal transmission **.660/.036** on the fixed source diagnostic. Its direct-C enriched DEV20 screen passed all measured mechanism gates: **7→0/20** swings, guitar/keys/piano/strings **.209/.227/.221/.223**, voice/bass/drums **.001/.004/.004**. The unchanged target also passed every measured full DEV40 C gate: **8→0/40** swings, guitar/keys/piano/strings **.2086/.2157/.2204/.2144**, voice/bass/drums **.00103/.00449/.00294**, rear RMS **+.313 dB** and slopes within ±.05. The student is far from that representation bound. On audible true eligible-dominant cells (power fraction ≥.8), its mean eligible output was **.495** versus target **.920**; **1.6%** of all time/frequency cells were audible with predicted eligible probability >.8, versus **6.1%** with target eligibility >.8. On confident vocal cells it predicted eligible **.200** versus target **.028**. The immediate diagnosis is underconfident ownership in this trained model; the oracle uses unavailable true stems and says nothing about achievable causal/device accuracy. A frozen-checkpoint `softmax(logits/T)` sweep raised eligible transmission from **.351 at T=1** to **.538 at T=.5** but kept vocal/eligible ratio **.403**, effectively the anchor's **.405**; simple probability sharpening is insufficient. The next one-change offline test should directly weight rare confident eligible cells while checking protected/eligible transmission at matched rear level. Prior wider and cross-band models with earlier objectives did not solve the frontier, so simply increasing width is not the selected next step.

### Review 21: rare confident-cell focus and ranking bound (2026-09-28)

The [role-focus report](UPMIX_CAUSAL_ROLE_FOCUS_2026-09-28.md) tests that preselected next lever: one additional equal-role CE term on ≥.8 source-dominant audible cells, calibrated to 30% of the base gradient, with identical four-role architecture/data/seed and 4k training age. The focused model **failed** the frozen source/entry gates. Reanalyzed eligible transmission **.337** versus shared-mask anchor **.462**; vocal/eligible **.439** versus **.405**; bass/eligible **.0350** versus **.0424** but above the demanded 20% reduction; entry retention median/p10 **.786/.520** versus **.835/.588**. The extra loss raised predicted own-role probability on confidently bass cells **.609→.794**, while confidently eligible cells stayed **.495→.490** against target .920. As precommitted, there was **no C screen or 10k/full40 continuation** for this checkpoint.

The same eligible underconfidence appears on sampled real train, DEV and pseudo-labelled owner songs; unfocused predicted eligible means **.391/.495/.443** on ≥.8 eligible-owned cells against target **.923/.920/.934**. Confident eligible versus protected cell ranking AUC is still **.857 train/.892 DEV**, but these easy-cell ranks ignore mixed-cell energy. On DEV12, median **62.9%** of eligible energy lies in pure-eligible cells and **30.8%** in mixed cells; **20.7%** of vocal energy also lies in mixed cells. A fixed global *hard* threshold at .55 on the focused score gives exploratory eligible transmission **.485** and vocal/eligible **.326** (near but above the .324 preselected gate), while .65 lowers the ratio to **.289** but drops eligible transmission to **.368**. This shows some useful score ordering, yet the viewed-after-the-fact binary mask is not a stable/audible CarPlay candidate. The next host-only architecture probe should freeze a smooth causal confidence mapping and evaluate source attribution, vocal entries, C swing and matched rear level before any promotion. No device or listening claim follows from this threshold diagnostic.

### Review 22: frozen smooth confidence mapping (2026-09-28)

The [confidence-map report](UPMIX_CAUSAL_CONFIDENCE_MAP_2026-09-28.md) tests two preselected mappings of the same frozen focused four-role checkpoint: a .55-centred, .10-logit-softness sigmoid of eligible-versus-protected logit margin, immediately or after a causal 80 ms per-bin EMA. Static source attribution on the fixed DEV12 passed all four source gates, with eligible transmission **.4670** and vocal/eligible **.3231** versus the shared-mask anchor **.4624/.4050**; the vocal margin to the .324 gate is only **.0009**. Yet vocal-entry retention on 43 fixed DEV20 entries was median/p10 **.826/.473**, both below **.85/.60**. The EMA variant had source ratios vocal/drums/eligible **.4091/.0674**, above **.324/.0568**, and entry **.832/.462**. The precommitted rule rejects both before real C and longer training. This is evidence against these two output mappings, not a proof that architecture changes are futile. A next architecture probe must represent persistent eligible evidence separately from fast vocal protection, then measure attribution on mixed cells and vocal entries before C promotion. The owner deferred listening; no deployable/device result was created.

### Review 23: independent complex audio and role heads (2026-09-28)

The [dual-head report](UPMIX_DUAL_HEAD_CAUSAL_PROBE_2026-09-28.md) froze a matched 4k pair with causal complex eligible-audio output and, in the candidate, a separate four-role auxiliary head (estimated neural cost **94/126 M MAC/s**). The candidate lowered scalar vocal leak **.161→.091** and improved 43-entry eligible retention **.703/.418→.837/.542** median/p10, yet still missed **.85/.60**. It transmitted less eligible energy on fixed DEV12 (**.385→.362**) and its vocal/eligible ratio, though improved against its complex control (**.504→.450**), remained worse than the shared-mask anchor **.405** and far above **.324**. Both arms failed all four preselected source gates and were rejected before C or longer training. Independent heads alone do not provide the missing instrument preservation at this age; the result does not prove a general architecture limit. One bounded next mechanism to test is a causal short complex FIR filter over past STFT frames, with the same matched control and source/entry gate before C.

### Review 24: causal two-tap complex filtering (2026-09-28)

The [deep-filter report](UPMIX_CAUSAL_DEEP_FILTER_PROBE_2026-09-28.md) tests the fixed current-plus-previous-frame complex FIR head against the same causal complex control, 4k steps and identical data/seed/trunk. The candidate's temporary 2k scalar gain vanished by 4k: SI-SDR/vocal leak **−1.23/.158** against control **−1.21/.161**. It modestly improved fixed DEV12 eligible transmission **.385→.398**, vocal/eligible **.504→.476**, and 43-entry retention **.703/.418→.786/.447**, but failed every preselected source ratio/recall gate and both entry targets. No C/full40/TEST20/device promotion follows. This bounds one two-tap mechanism at this age; it does not show the input lacks extractable information. Next investigate whether the 8 s reset-crop training differs materially from continuous 60 s inference before choosing another model or longer train.

### Review 25: continuous-context audit (2026-09-28)

The [long-context audit](UPMIX_CONTINUOUS_CONTEXT_AUDIT_2026-09-28.md) compared each frozen 4k complex checkpoint on six identical DEV 60 s mixtures, with the recurrent and running-level state either held or reset every 8 s. Only tail frames after 16 s and 1 s after each reset counted. Median continuous-minus-reset normalized complex error was **−.001 dB** for control and **−.004 dB** for two-tap; only **3/6** and **2/6** songs were worse continuously, all individual differences within **±.05 dB**. The precommitted +.5 dB / ≥4-song mismatch gate did not fire. Source-transmission differences were similarly small. Training on 8 s reset crops remains a theoretical risk for other architectures, but is not a measured cause of these two failures. The next discriminating question is whether vocal/instrument stereo geometry supplies useful spatial cues beyond the present diagonal per-channel masks; test an oracle spatial upper bound before another costly train.

## 📝 Publication (astra review 5, 2026-09-26)

Asked whether there is a paper in this work.  Verdict: **a credible systems +
evaluation paper now, a useful white paper immediately; no evidence yet for a
new separation method or a general impossibility result.**

* Ranking: (1) system + evaluation — AES convention / DAFx, WASPAA with
  perceptual validation; (2) the evaluation method (rear share / swing /
  flicker) — WASPAA / DAFx, ICASSP with validation, *if* it predicts perceived
  instability across several renderers, not only ours; (3) the negative result
  — arXiv / GitHub white paper, weak as standalone conference material ("25
  variants are evidence of a persistent engineering frontier, not proof of a
  fundamental limit"); (4) a positive result — first in rank *only* with a new
  mechanism; successful distillation alone is not novelty.  Write one
  integrated paper before splitting.
* Reviewers will demand: a blinded listening study (~24–30 listeners, 8–12
  excerpts, separate ratings for depth stability / vocal anchoring /
  envelopment / preference; MUSHRA-inspired, but stereo is a baseline, the old
  car a comparator, the oracle a diagnostic condition — plus deliberately
  unstable and rear-heavy anchors); baselines (documented passive
  sum/difference matrix, a classical primary/ambient decomposition, the old
  and fixed heuristic renderers, the oracle, a teacher-driven renderer;
  commercial systems cited, not measured); paired song-level bootstrap
  statistics (frames are not samples; explain every denominator 28 / 39 / 40
  / 120); reproducibility (renderer, metric definitions, generators, configs,
  device latency / load) and a frozen MUSDB18-HQ evaluation beside MoisesDB.
* Cite first: Avendano & Jot (frequency-domain multichannel upmix), Faller
  (multiple-loudspeaker playback of stereo), Goodwin & Jot (primary-ambient
  decomposition), Rouard / Massa / Défossez (HTDemucs).  Primary/ambient
  extraction, coherence masks, decorrelation and source-informed routing are
  established; the oracle-through-renderer is an application of a known
  methodology; rear share resembles routing-energy / leakage measures, swing
  resembles image-instability measures — the specific definitions and their
  perceptual validation may be publishable.
* Corrections to our reading: train AUC ≈ DEV AUC does not establish
  underfitting (AUC is ranking, not calibration); conditional-mean outputs are
  expected under uncertainty — "does not commit" needs an operating-point
  analysis; the oracle shows feasible renderer behaviour with privileged
  information, not a perceptual ceiling.
* Minimum experiments (weeks): finish teacher-through-renderer; best student
  vs the classical baselines on one locked test set; drive-vs-veto ablation at
  matched rear energy and temporal smoothing; the listening study with
  metric–rating correlation.  Audit the ≥ 10 dB ownership rule (coverage,
  threshold sensitivity, what it excludes); separate "stable because quiet"
  from "stable at useful rear energy".  Stop broad architecture sweeps.
* Claim: a reproducible deployment case study and a measured
  stability–selectivity trade-off.  Do not claim recovered surround intent,
  universal budget impossibility, or listening superiority before testing.
  Check dataset / audio / weights licences separately; the vehicle is one
  tested configuration, no manufacturer claims; reverse-engineering detail
  proportional to reproducibility.

## ⚖️ Licensing

MoisesDB is CC BY-NC-SA 4.0, teacher checkpoints unverified: weights stay
development-only, as the two shipped blobs already are.
