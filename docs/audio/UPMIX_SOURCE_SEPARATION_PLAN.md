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

## ⚖️ Licensing

MoisesDB is CC BY-NC-SA 4.0, teacher checkpoints unverified: weights stay
development-only, as the two shipped blobs already are.
