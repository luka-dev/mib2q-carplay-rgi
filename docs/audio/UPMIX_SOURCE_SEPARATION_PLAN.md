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
