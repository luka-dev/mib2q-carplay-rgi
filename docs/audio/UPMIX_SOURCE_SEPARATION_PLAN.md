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
0.14 MB int8, state ~0.2 MB), which is the L2 question on Krait.

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

| | vocals | guitar | piano | keys | bass | drums | swinging | rear level |
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

Rear levels match within 0.4 dB (−37.2 vs −37.6 dBFS), so this is not a
level effect.  Reconstruction beats masking on every axis at the oracle level: more of every
instrument behind (guitars 0.20 → 0.26), less voice (0.03 → 0.01), less than
half the swing (7 → 3).  The mask's ceiling was never the renderer's ceiling —
it was the mask interface's: a fraction of the *mixture* carries the mixture's
other sources with it; clean audio does not.  **A real separator's estimate keeps the advantage.**  With the same
htdemucs stems, audio instead of mask: voice 0.03 → 0.01, bass 0.11 → 0.04,
vocal-swinging songs 11 → 4, guitars unchanged at 0.20 — and with the cleaner
BS-RoFormer vocals the instruments follow the oracle-audio row (guitars 0.23,
keys 0.22, 3/39 swinging) at the same 0.01 voice, one dB quieter overall.  The
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
| sep5 best (18k, −0.34 dB) through the DSP | G 1.0 (G 1.3 running) | G 1.0: voice 0.03, guitars 0.12, keys 0.12, bass 0.08, drums 0.10, 5/39 swinging, level −38.1 dBFS, rear error 0.13 (treble 0.10) | the wide model is a little bolder than sep3's best (guitars 0.09 → 0.12, level +1.4 dB, error 0.15 → 0.13) at the same voice-leak class (0.02 → 0.03) — but still the ~0.10 instrument picture at the stable gain: 3× the compute changes the renderer's numbers by a hair, exactly what its 0.1 dB SI-SDR difference predicts |
| sep6 (`run_sep6.ps1`, after sep5) | **the supervision diagnostic**: the 125 M band model on MoisesDB truth only (176 songs, ±6 dB stem gains), same schedule, 40k steps → DSP at G 1.0; compared with sep3 (50 % pseudo-labels) at matched steps | queued | if truth-only matches or beats sep3, the pseudo-labels bought nothing (or hurt) and the residual-"other" policy is the suspect; if it is clearly worse, the 176 songs are too few and cleaner pseudo-labels (audited, not assumed) are the lever |
| sep7 (`run_sep7.ps1`, after sep6) | **the structure diagnostic** (review 10's leading diagnosis, "inadequate cross-band modelling"): sep3's band model with the two depthwise k3 mixers replaced by a BSRNN-style **bidirectional GRU across the 48 bands at every frame** (`--band-rnn 32`; frequency is not causal, so the recurrence over time stays causal), 599k params, ~180 M MAC/s (budget exceeded on purpose; the 140 M version is `--band-rnn 16`), same data / losses / schedule as sep3 and sep5, from scratch, 40k → DSP at G 1.0 / 1.3 | queued | if the band RNN moves the plateau by ≥ 1 dB where 2× width moved it 0.4, the wall was the band-independent recurrence and the budget version is the next student; if it sits with sep3/sep5, structure is not the lever either and the track closes on supervision / data |
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
