# HSSR — the stereo→5.1 upmix model, end to end

*A long-read on the AI that drives the CarPlay Virtual Cabin upmix: what it is, how
it works, every iteration we tried, and what we actually shipped.*

> **Provenance.** Claims are tagged inline with the source file (and section where
> it helps): `hook/audio/hssr_dsp.c`, `docs/…`, `models/…`, `memory/…`. Line
> numbers drift, so citations are file/section-level. This paper was synthesized
> from the corpus in `docs/UPMIX_*`, `tools/stereo-upmix-render/`, the `hook/audio`
> sources, and the `upmix_*` memory notes; those remain the primary record.

---

## 0. TL;DR

HSSR = **H**igh-**S**electivity **S**emantic **R**ear-routing. It is a small,
causal, quantized GRU **routing controller** — *not* a source separator. It never
splits audio into stems. It listens to a stereo signal 93.75 times a second and
emits, per frequency band, a decision: *how much of this belongs behind the
listener, and where*. A deterministic DSP then builds a 5.1 field from that
decision, on top of the reverse-engineered J525 amplifier "Virtual Cabin"
(`docs/UPMIX_TRAINING_DATA_AND_MODELS.md`, `docs/HSSR_DATASET_TRAINING_AUDIT.md`).

The honest one-paragraph history: we spent ~15 model generations trying to make a
network *separate* music into 12 instrument classes on 240 songs, and it never
worked — an independent audit found the trained model, after a full pass, equalled
"mute plus drums" (`docs/UPMIX_INDEPENDENT_AUDIT_2026-09-17.md`). We abandoned
separation for a **genre-agnostic "second scene"**: move the not-protected,
spatially-wide part of the stereo image behind the listener, delayed so the front
still anchors the sound. The learned part shrank to a single job — a *protection
mask* that keeps lead vocal and bass out of the rear. What ships today is that DSP
second scene **gated by two GRUs**: `q6d` (primary, trained on real MoisesDB stems)
with `V11` kept as a conservative **vocal-protection guard**.

One-line lineage:

> V9 → **V11 selected** (2026-09-07, MUSDB, selectivity 59×) → V12 A–E rejected →
> V13 renderer+data fixes → V14 teachers+review → V15 FLOAT + streaming A/B/C
> rejected → **q6d selected** (2026-09-18, MoisesDB real 6-class, full fine-tune
> from V11), shipped as the primary model with **V11 kept as the vocal guard**.

**Caveat that matters:** "a learned protection mask, not 12 classes" is the design
*direction*. The purpose-built learned mask (the Q4 line) was prototyped, measured
**worse than V11 for vocals**, and shelved. What is deployed is a DSP
azimuth/coherence second scene gated by q6d + the V11 guard
(`docs/UPMIX_Q4_SECOND_SCENE_STATUS.md`, `memory/upmix_second_scene_formulation.md`).

---

## 1. Why the first approach failed

For roughly fifteen architecture generations (V11…Q2) the model was asked to do
supervised source separation / role classification over 240 songs. It never
reached a regime where separation works. The 2026-09-17 independent audit is the
tombstone (`docs/UPMIX_INDEPENDENT_AUDIT_2026-09-17.md`,
`memory/upmix_q2_independent_audit.md`):

- The trained model after a full 24,288-update pass **equals MUTE + drums**; its
  "+0.45 dB vs disabled" compares against an empty control, and "−0.1 dB vs raw
  scalar" means it sits at its own initialization (mask/refinement heads zero-init).
- Per-family DEV NMSE: drums 0.44, but lead 0.81, bass 0.81, guitar 0.90, backing
  1.06, keys 1.84 — most heads output 13–17 dB below target (MSE shrinkage toward
  mute).
- Only **2,352 of 111,132 parameters** actually do per-bin separation; the rest
  compute 32-band descriptors that, for vocal/guitar/keys, are no better than the
  training-set prior. Even *oracle* 32-band hints leave backing NMSE 0.57–0.89 —
  the band-power interface **structurally cannot** separate lead/backing or
  keys/guitar.
- Absent-head leakage was unscored (a −40 dB floor dropped 55% of cells — exactly
  where rear leakage lives); the keys head emitted a −12.8 dB phantom on songs with
  no keys.
- Nobody had ever listened to the output — 0 WAVs rendered.

A static per-bin 2×2 Wiener filter with **zero trainable parameters** beat the
trained model on every family except drums (`…AUDIT…` §10). The conclusion was not
"train longer" but "this problem is mis-posed": 12-class supervised separation on
240 songs cannot work; **the rear loudspeaker needs one mask, not twelve**
(`docs/UPMIX_Q4_SECOND_SCENE_STATUS.md`, `memory/upmix_second_scene_formulation.md`).

---

## 2. The idea we kept: a genre-agnostic second scene

Given the Virtual Cabin, split the stereo image and move part of it behind the
listener as a *second scene* — not ambience, not a copy of the mix, for any genre,
with **no instrument classes** (adopted by Luka 2026-09-17,
`docs/UPMIX_Q4_SECOND_SCENE_STATUS.md`, `memory/upmix_second_scene_formulation.md`).
Rear = (NOT protected: lead vocal / bass / drum core / dominant harmonic line) AND
(spatially wide / decorrelated / texture-like). Three components:

1. **DSP, genre-free** — per-bin pan, inter-channel coherence, texture (fast/slow
   envelope); low frequencies stay front.
2. **The only learned part** — a *protection mask*: what must **not** go rear, with
   an **asymmetric loss** (a leaked lead is far worse than a missed piece of
   support).
3. **Renderer** — rear delayed 10–20 ms, high-passed, decorrelated; front = X −
   α·rear, with matrix-return compensation.

**Why the rear is delayed.** The shipped mechanism delays the *rear only* by 12 ms
(576 frames at 48 kHz) and leaves the front aligned, so the louder, earlier front
anchors the image by the **precedence (Haas) effect** and the rear reads as
distinct objects, not an echo. The measured in-mix rear copy sits ~20–30 dB under
the front and 12 ms late — "which is why Luka hears objects, not a vocal echo"
(`memory/upmix_second_scene_formulation.md`, `docs/UPMIX_Q4_SECOND_SCENE_STATUS.md`).
This is deliberately unlike Dolby, which never sends the dominant object to the rear
(`docs/UPMIX_DOLBY_DISTRIBUTION_PLAN_2026-09-21.md`).

---

## 3. How the runtime works

Output rail is fixed PCM6 `L, R, C, LFE, Ls, Rs`. **C is always digital zero** —
the J525 amplifier derives centre from L/R/LFE, so the hook must not
(`hook/audio/stereo_upmix.h`, `docs/STEREO_TO_5_1_UPMIX_DESIGN.md`).

End to end, one 128-sample hop:

```
stereo S16 in
  │
  ├─► front delay line (512, S16) ───────────────────────► FRONT L/R  (bit-exact delayed input)
  │
  └─► STFT 512/hop128 ─► 32-band features (368) ─► [async GRU worker @ 93.75 Hz]
                                                       │  per band: 6-class, rearability, pan
                                                       ▼
                          routing decode  (vetoes: lead_keep⁴, bass_keep⁵, guard²;
                                           freq gates: 0 <220 Hz, ramp→350, roll >15k)
                                                       │
                              ┌────────────────────────┴───────────────┐
                              ▼                                         ▼
                        object slots (4, pan-steered)            bed (ambience / fallback,
                              │                                   delayed + cross term −0.14)
                              └───────────────► rear synth ◄───────────┘
                                          rear delay 12 ms · Ls/Rs +3 dB@4k shelf
                                                       │
   FRONT L/R  ── minus (1−retention_eff(f))·source ───┤   ◄ retention HF taper 1000:2500
                                                       ▼
                                        [ L  R  C=0  LFE  Ls  Rs ]   float → S16
                                                       │
                                     LFE V2 assist  +  cabin guard (J525 peak, early-out)
                                                       ▼
                                                  PCM6 S16 out
```

### 3.1 Signal path (STFT / WOLA)

- FFT size **512**, hop **128**, 257 half-spectrum bins, **32 perceptual bands**,
  64 four-bin "shape" bands; 4× overlap; latency 512 frames = **10.667 ms**
  (`hook/audio/hssr_dsp.h`).
- Analysis window is a half-sine `sin(π·(n+0.5)/512)`; the synthesis window is that
  divided by the per-phase COLA `Σw²`, with the inverse `1/N` folded in so the
  inverse transform never rescales (`hook/audio/hssr_dsp.c`, WOLA setup).
- In-place radix-2 512-point FFT; twiddles regrouped per butterfly pass for
  contiguous vector loads; first pass is pure add/sub. Because both output channels
  are real, left→real and right→imaginary are packed into one Hermitian spectrum
  and recovered with a **single** inverse FFT (`hssr_dsp.c`).
- **S16 vs float.** The DSP core is float; the boundaries are S16. Critically, the
  **fronts are not the STFT output** — they are the raw input samples read from a
  512-deep S16 delay line, so the front is a **bit-exact delayed copy of the
  input**; only the rear is synthesized float (bounded ±2.0) (`hssr_dsp.c`,
  `hssr_dsp.h`). Q15 output gains: front `32670`≈0.997, front-with-LFE
  `31392`≈0.958, rear default `5898`≈0.18 (ceiling `16384`=0.5), LFE default
  `4096`=0.125 (`stereo_upmix.h`).
- The synchronous path (used by the offline render tools) is **bit-exact and
  reproducible**; the async worker path differs only in scheduling, never in
  arithmetic (`docs/STEREO_TO_5_1_UPMIX_DESIGN.md`).

### 3.2 The controller network

Shape: **368 → GRU(160) → GRU(160) → Linear(264)**, int8 weights with per-row
float dequant scales and float biases, loaded from an `HSSRNEO1` little-endian
blob (CRC32-validated, all floats finite, all scales positive) (`hssr_neon.h`,
`hssr_neon.c`).

```
features (368)  =  32 bands × 9   +   64 shape   +   16 global
      │  standardize (x−mean)·inv_std, clamp ±6, quantize int8 @ 127/6
      ▼
  GRU layer 1  (368→160)   int8 GEMV → per-row dequant → reset-after GRU, h clamp ±1
      ▼
  GRU layer 2  (160→160)
      ▼
  Linear head  (160→264)   int8 GEMV → scale → bias
      ▼
outputs (264)  =  32 bands × [ 6 class logits , rearability , pan ]  +  8 global
                              softmax          σ→smoothstep   tanh±0.85   (only k=3..7 used)
```

451,464 parameters; 469,760-byte export. Runtime inference is not PyTorch: the C
path additionally quantizes matrix inputs and recurrent activations and uses
rational sigmoid/tanh, so loading the exported weights into PyTorch is **not** the
shipped inference (`docs/HSSR_DATASET_TRAINING_AUDIT.md`). Inference: standardize `(x−mean)·inv_std`, clamp ±6, quantize int8
at `127/6`; int8 GEMV (NEON `vmull_s8`/`vpadalq_s16`); PyTorch reset-after GRU
`r,z,n` with rational sigmoid/tanh (bounded cost on old QNX libm); hidden clamped
±1 (`hssr_neon.c`).

**Inputs (368)** — 32 bands × 9 features (log energy, side/mid, ILD, coherence,
phase cos/sin, flux, tonality, slow-energy tail) + 64 shape (per-4-bin mean
log-magnitude) + 16 global (total energy, crest, mid/side, weighted coherence,
sub-120 Hz & low-250 Hz fractions, centroid, flux, transient fraction,
harmonicity, diffuseness, pan, pan-entropy, `current_log_delta`) (`hssr_dsp.c`,
feature extraction).

**Outputs (264)** — 32 bands × 8 (`0..5` six-class softmax logits, `6`
rearability, `7` lateral/pan) + 8 global. The **6-class taxonomy** is the MoisesDB
stem set: `0 vocals, 1 bass, 2 drums, 3 guitar, 4 keys, 5 other/background` — the
last two are the "safe rear" classes (`hssr_dsp.c`,
`docs/UPMIX_C2_SOURCE_SELECTION.md`). Rearability → sigmoid → `smoothstep(0.12,
0.36)` (real positives cluster ~0.25); pan → tanh, clamped ±0.85. Of the 8 global
outputs only `k=3..7` are used at runtime: scene confidence, rear-activity target,
scene stability, fx-present, global safety (`hssr_dsp.c`).

**Async execution.** Inference runs every 4th hop (**93.75 Hz** = 48000/(128·4)).
A `SCHED_RR` priority-17 worker owns the recurrent state; the audio thread submits
features and reads results through two-entry SPSC mailboxes with acquire/release
atomics — no locks, no allocation, no logging on the audio thread
(`hssr_control_worker.c`, `docs/STEREO_TO_5_1_UPMIX_DESIGN.md`). One or two missed
deadlines keep the last stable mask; the third consecutive miss decays the mask
toward a quiet rear (`hssr_dsp.c`, control failsafe).

### 3.3 From controller outputs to routing

Per band, rearability is multiplied by a chain of steep semantic vetoes, then
smoothed asymmetrically (fast attack / slow release) into persistent band controls
(`hssr_dsp.c`, control decode):

- `bass_keep = 1 − 0.85·p[bass]`, applied to the **5th power**.
- `lead_keep = 1 − 0.92·vocal_risk`, applied to the **4th power** (a leaked lead is
  far more audible than missed support). `vocal_risk` is `p[vocal]` reduced by band
  spatiality: a genuinely wide lead may survive; a centred lead cannot.
- drums `×(1−0.35·p[drums])`, guitar `×(1−0.10·p[guitar])`, background/FX lift
  `×(1+0.85·(p[keys]+p[other]))`, then scene-focus and global-safety scaling, then
  makeup `×16.0`.
- Hard frequency gates: **zero rear below 220 Hz**, linear ramp 220→350 Hz, ×0.65
  above 15 kHz, ×0.25 above 20 kHz.

### 3.4 Object slots

4 slots, two per rear half-plane (even→left, odd→right; 0/1 primary, 2/3
secondary). Each band scores per slot from `semantic × directed-pan smoothstep ×
(0.55+0.45·coherence)`, transient-boosted; nothing below 220 Hz; confidence needs
both an energy share and a peak candidate, held ~250 ms (94 hops) with slow
release. A confident object keeps its pan; a weak one follows. At render, each bin
picks the best slot and is gated by per-bin ~45 ms coherence so room/reverb is not
steered (`hssr_dsp.c`, object slots + render).

### 3.5 The protection mask (and the guard model)

The protection mask keeps **lead vocal (class 0) and bass (class 1)** out of the
rear (`docs/UPMIX_C2_SOURCE_SELECTION.md`, `docs/UPMIX_Q2_ROLE_TARGETS.md`). It is
implemented as the steep vetoes above (`lead_keep⁴`, `bass_keep⁵`), the rule that a
centred lead cannot acquire a slot, and a gated fallback-bed permission.

The **guard** is an optional second GRU — V11, whose bias is "everything harmonic
is vocal". Its vocal posterior is a safety net for the primary model, which misses
some heavily-processed leads. In the controller, the guard's vocal probability is
**discounted by the primary model's confidence the band is keys/other**
(`combined = gvocal·(1 − p[keys] − p[other])`), then max-merged into the vocal
probability. The merged `guard_vocal` raises `vocal_risk`, attenuates rearability
by `(1−0.95·guard_vocal)²`, and suppresses focus-rescue — so a voice the guard
hears is protected even when the primary reads it as keys (`hssr_dsp.c`,
`hssr_dsp.h`, `docs/STEREO_TO_5_1_UPMIX_DESIGN.md`). This hybrid veto is the reason
two models ride together.

### 3.6 Front retention and the HF taper (the "too much treble in front" fix)

`front_retention = r` (`AUDIO_STEREO_FRONT_RETENTION`) is the object share left in
the front. Energy conservation bounds it: `r* = sqrt(1−b²)` (b = rear gain); above
`r*` the object localizes forward and the second scene collapses, so `r` is chosen
below `r*` (`docs/UPMIX_DOLBY_DISTRIBUTION_PLAN_2026-09-21.md`).

Measurement showed retention lifted the front **broadband** (+1.3 dB across
0.5–16 kHz), not just the body — because bright objects (leads, cymbals) carry
their treble into the front half they leave behind. That is the "too much treble in
the front" the user reported. The fix makes retention **frequency-dependent**:

```
retention_eff(band) = r · (1 − smoothstep(bandEdgeHz, lo, hi))
```

Full retention below `lo`, none from `hi` up (`hi ≤ lo` disables the taper).
Defaults `lo = 1000 Hz`, `hi = 2500 Hz` (`AUDIO_HSSR_RETENTION_HF=LO:HI`), chosen so
the front top returns to the no-retention state (+0.04 dB at 2–8 kHz) while the body
keeps 92% of the gain (`hssr_dsp.c`, `stereo_upmix.h`,
`docs/UPMIX_STATUS_2026-09-22.md`, `docs/UPMIX_DOLBY_DISTRIBUTION_PLAN_2026-09-21.md`
§7). Deployed 2026-09-22.

### 3.7 Bed delays, cross term, owner attack

When bed handling is enabled, the ambience/fallback "bed" is separated from the
coherent object and distributed like a Dolby residual: undelayed share **0.79**,
per-band delayed share **0.60**, delayed **cross term −0.14** (unit energy for
uncorrelated copies), with per-band L/R delay tables (long at LF ~32/40 ms,
shrinking above 3 kHz) to decorrelate (`hssr_dsp.c`, bed mix). A Dolby-style
per-band attack detector drives three modes (`hssr_dsp.h`):

- **OFF** — retention applied in the time domain, `band_front_keep = 1`.
- **OWNER** — on a transient the band collapses toward its owner (object→rear,
  ownerless bed→front): `band_front_keep = 1 − r·(1 − attack_collapse·w·owner)`;
  `attack_collapse = c` is the single collapse-depth knob.
- **FRONT** (Dolby reference) — the whole bed row collapses forward on every hit.

All bed/attack machinery is env-flagged, **default off, and bit-exact-equal to the
legacy renderer when off** (`stereo_upmix.c`,
`docs/UPMIX_DOLBY_DISTRIBUTION_PLAN_2026-09-21.md`).

### 3.8 Cabin guard, LFE V2, shelves

- **J525 input matrix** — `kInputMatrix[6][8]`, profile-0/bank-1, 6 inputs × 8
  buses, with a −99 dB floor standing in for "no feed" and a largest column norm
  `3.53`. This is a **downstream peak-detection model for the S16 guard, not an
  acoustic model or EQ inverse** (`stereo_upmix.c`, `stereo_upmix.h`).
- **Cabin guard with early-out** — sample limit `32275` (0.985 FS). Since every bus
  obeys `|bus| ≤ 3.53·max|sample|`, if `peak·3.53 ≤ 32275` nothing can clip and the
  guard returns before touching the matrix (the per-frame hot path). Overload is
  first repaired by clamping **only LFE** into its feasible interval (never
  synthesizing or sign-flipping LFE), skipping the 14 non-binding divisions; only
  then does it linked-scale all six channels (`stereo_upmix.c`).
- **LFE V2 assist** — per-side LR4 low-pass, a slow correlation-driven polarity
  move that folds L+R or L−R to preserve coherent anti-phase bass instead of
  cancelling it, plus a bounded dynamic gain (0.96..1.10) fed by the model's
  bass/drum confidence. The assist is **relative** (cap Q15 4096 = 0.125); the
  corrected BANK1 also feeds the sub from L/R/C/Ls/Rs (`stereo_upmix.c`,
  `stereo_upmix.h`).
- **Rear high-shelf** +3 dB @ 4 kHz applied **only to separated Ls/Rs** (after a
  −4 dB @ 500 Hz scene-rear low shelf), before a linked limiter; the un-EQ'd source
  stays the front sidechain, so the shelf is output voicing only and cannot create
  rear content (`stereo_upmix.c`).

### 3.9 NEON and why `-ffp-contract=off`

Every NEON kernel preserves scalar operation order, so results are bit-identical to
the scalar fallback. The QNX/ARMv7 target is VFPv3-d16 with **no FMA** — `a·b+c` is
two separately-rounded ops. A host compiler contracts that into one fused rounding
by default, diverging bit-for-bit. So host/bench builds force
**`-ffp-contract=off`**, which is what lets the offline render tools reproduce the
target's exact bits (`tools/stereo-upmix-render/bench/build_host.sh`,
`apac_decoder/tools/build_qnx.sh`, `docs/UPMIX_RELEASE_RUNTIME_AUDIT.md`).
Contraction — not vectorization — is what would break determinism; this is why the
"bit-identical across host and target" claim holds.

---

## 4. The model iterations

All deployed/trained variants use the **same "HSSR-160" graph**: `GRU(368→160) →
GRU(160→160) → Linear(160→264)`, **451,464 params**, **469,760-byte** export, 32
bands, 6 classes, control rate 93.75 Hz (`tools/stereo-upmix-render/train_hssr.py`,
`docs/HSSR_DATASET_TRAINING_AUDIT.md`). Only weights (and, in some pilots, which
rows are trainable) differ. A width ablation was costed (160→192 = 597k params;
160→256 = 943k) but **never adopted** — the audit found "insufficient width was
never demonstrated", and any width/layer change needs a new versioned model ABI
(fixed C loader/state arrays) (`docs/HSSR_DATASET_TRAINING_AUDIT.md`).

| Gen | Date | What it did | Outcome |
|---|---|---|---|
| **V9** | pre-09-07 | prior production baseline, same graph | superseded by V11 |
| **V11** `hssr-selection-v11` | 2026-09-07 | MUSDB18-derived; sharpened instrument/vocal **selectivity** without graph change | **SELECTED**; level-weighted selection ratio 17.4×→**59.1×** vs V9; epoch 5/6; SHA `ab01463e…`. Now the **guard** |
| **V12 A–E** | 2026-09-08 | fine-tune V11 on 43 private songs + BS-RoFormer 2-stem teacher | **ALL REJECTED**, retain V11. A: vocal leakage +18–49%; B: selectivity 0.937 < 0.95; C: aux loss → 6.78× vocal; D: heldout +22% vocal; E: blend ≈ V11. Shipped bundle = new float hook + original V11 |
| **V13** | 2026-09-08 | (a) bass fix + experimental cabin renderer, both keeping V11; (b) routing-head-only pilot (freeze GRUs, train rearability+pan only) | renderer/data fixes kept; **no new weights selected** |
| **V14** | 2026-09-09 | full 50-song corpus teacher build (587 cores) + listening/content review | **no trainer launched**; finding: teacher filenames must not become training truth; karaoke input-path sensitivity |
| **V15 A/B/C** | 2026-09-09→11 | rebuilt dataset in **FLOAT** (V14's PCM16 clipped peaks on 44/50 songs); streaming routing-head distillation from V11 | **ALL REJECTED**, retain V11. Every candidate moved *more* vocal to the rear |
| **q6d** `hssr-q6d-moises-a` | 2026-09-18 | first trained on **real dataset stems** (MoisesDB 6-class) not pseudo-labels; **full network** fine-tune warm-started from V11 | **SELECTED** as the shipped primary; LR 5e-4, 5 epochs, **best epoch 3**, batch 16, Ryzen NAS CPU; SHA `96abe57d…`. Weakness: **confuses vocals with keys** → why the guard exists |

**Datasets.** MoisesDB v0.1 (240 songs, 12-class taxonomy; used 181 TRAIN / 40 DEV,
TEST sealed) for q6d's real 6-class targets; MUSDB18 (150 tracks) for V11 (now
guard); a 43-song private corpus for V11 supervision and V12/V13 fine-tunes; a
50-song local corpus (587 FLOAT cores) for V14/V15 teachers; AlbumDB / Amvizo /
HiRez for 5.1 reference (`docs/UPMIX_TRAINING_DATA_AND_MODELS.md`,
`docs/HSSR_DATASET_TRAINING_AUDIT.md`). Teacher networks (Demucs6, BS-RoFormer,
Mel-Band RoFormer karaoke, AST/CLAP) only ever **labelled** data; q6d used the
datasets' real stems, not teacher pseudo-labels. Both shipped weights are flagged
**development-only / non-commercial** (MUSDB18 + MoisesDB research licensing).

**Why two models ship.** Primary = q6d (MoisesDB real 6-class). Guard = V11. q6d's
vocal probability is unreliable exactly where it mislabels vocals as keys; the V11
guard's vocal posterior protects that voice. This matches the standing audit rule:
**vocal protection must never be traded away for instrument gains**
(`docs/UPMIX_TRAINING_DATA_AND_MODELS.md`, `docs/UPMIX_V15_ROUTING_REFINEMENT.md`).

---

## 5. Calibration

**The sweeps.** A 105-render grid (retention × rear × bass × LFE) and a 60-second
one-variable grid (2026-09-21) corrected several earlier conclusions
(`docs/UPMIX_TUNING_SWEEP_2026-09-20.md`, `docs/UPMIX_LONG_ONEVAR_2026-09-21.md`,
`memory/upmix_long_onevar_2026-09-21.md`):

- **6-second prefixes are not representative** — they understated bass/mid tilt by
  ~1.5 dB mean (up to 4.8 dB on one track); earlier 6 s conclusions were withdrawn.
- **Front bass is the only control that moves bass**, and it moves the modelled sub
  too (+3.3 dB per 3 dB shelf); retention does **not** touch bass — its tilt
  reduction is entirely front-mid restoration.
- **LFE is not a monotonic sub-volume knob** in the corrected model.
- The old "float candidate" was actually a **bass increase**, not a fix (see §7).

**Frozen production calibration** (Luka, 2026-09-21: *current calibration is OK for
now — do not tune*, `docs/UPMIX_AGENT_HANDOFF_2026-09-19.md`):

| Parameter | Value | Env flag |
|---|---|---|
| Render path | **S16** (float path removed) | — |
| Rear / surround gain | **0.40** (code ceiling 0.50) | `AUDIO_STEREO_SURROUND=0.40` |
| Front bass shelf | **−3 dB** (RBJ 200 Hz, front L/R only) | `AUDIO_STEREO_FRONT_BASS_DB=-3` |
| LFE assist gain | **0 dB re 4096** (assist mode) | `AUDIO_STEREO_LFE_GAIN=0`, `AUDIO_STEREO_LFE=assist` |
| Front retention | **0** (in the frozen-OK config) | `AUDIO_STEREO_FRONT_RETENTION` |
| Retention HF taper | **1000:2500 Hz** | `AUDIO_HSSR_RETENTION_HF=LO:HI` |
| Rear delay | **12 ms** (576 frames), rear-only | (baked in) |
| Models | primary q6d `96abe57d…`, guard V11 `ab01463e…` | `AUDIO_HSSR_GUARD_MODEL` |
| Object slots | 4 (2/side), 250 ms hold | (single path) |

**The bass culprit (root cause, 2026-09-19).** The 2026-09-18 build silently raised
the LFE default from 4096 to 12288 (+9.5 dB) and widened the LFE LR4 48→60 Hz;
since the 5.1 bank feeds the sub from LFE only, the whole sub got +9.5 dB — the
"bass everywhere" the user heard. Fixed: default back to 4096 plus the
`AUDIO_STEREO_LFE_GAIN` knob (`docs/UPMIX_Q4_SECOND_SCENE_STATUS.md`,
`memory/upmix_second_scene_formulation.md`).

**A parked (not shipped) "scene-bed distribution" build** adds, on top of the frozen
config: `AUDIO_STEREO_FRONT_RETENTION=0.5`, `AUDIO_HSSR_BED_DELAYS=on`,
`AUDIO_HSSR_BED_ATTACK=owner`, `AUDIO_HSSR_ATTACK_COLLAPSE=0.75`. Offline it lowers
Ls/Rs 2–8 kHz coherence from 0.50 to 0.36 at a 0.6 dB front-mid cost on hits; code
defaults all off (`docs/UPMIX_Q4_SECOND_SCENE_STATUS.md`).

---

## 6. Realtime

The realtime media path (`AUDIO_REALTIME_UPMIX=on`) runs Spotify and every
non-buffered app through the same HSSR renderer **inside the stock QSA audio
thread**. Period = **768 frames = 16 ms**; it adds 10.7 ms front latency
(`docs/UPMIX_STATUS_2026-09-22.md`).

Five recorded on-car sessions on the old 8 ms build **self-disabled in 3 of 5**
cases (worst period 8057–11079 µs) — the audible rear flicker. The current guard
policy (`hook/audio/rt_upmix_guard.h`, test `tests/rt_upmix_guard_test.c`):

- **Budget 10 ms** (up from 8, which was just half the period, invented before cost
  was measured), leaving 6 ms for the rest of the thread; disable still needs **4
  consecutive** overruns.
- **Cold-start forgiveness**: the first **16 periods** after arm or any timeline
  reset are not judged (cold cache + cold controller worker), logged as `cold_us`.
- **Pause-based revival only**: a ≥500 ms gap = pause/track/app change; a disabled
  upmix waits for such a gap and revives cold. No mid-track rear reappearance; the
  old timer/backoff logic is gone.

The headroom came from a **−28.8% typical / −26.2% loud-block** speedup on
QNX/ARMv7 (QEMU `-icount`), **bit-identical** — cabin-guard early-out, dropped
matrix divisions, inlined biquad, NEON in the FFT butterflies / cabin matrix /
window / overlap-add / bed mix (`docs/UPMIX_STATUS_2026-09-22.md`). **These are
estimates, not on-car measurements** — the stop-line with the real `ms_hist`
distribution has not yet been captured (see §8).

Historical Krait timing anchors the caution: HSSR-object under contention reached
P99 ≈ 21 ms in the R1 budget study — there is **no established free 75–80% budget**
(`docs/UPMIX_R1_RUNTIME_BUDGET.md`).

---

## 7. What was tried and rejected

- **Q4 learned protection mask** — the purpose-built learned mask leaked lead
  in-mix at −7.9 dB where deployed V11 held solo vocals at −23…−26 dB. Worse than
  V11; the whole ML-mask line was paused (`memory/upmix_second_scene_formulation.md`).
- **Float render candidate** — the one-variable grid proved its "tilt reduction"
  was actually a **+1.9 dB bass increase**; the entire float/cabin/quiet path was
  deleted, S16 output byte-identical before/after
  (`docs/UPMIX_LONG_ONEVAR_2026-09-21.md`, `docs/UPMIX_AGENT_HANDOFF_2026-09-19.md`).
- **TRUEPAN** (objects keep own pan) — rear L/R correlation jumped to 0.86–0.90
  (rear went near-mono); wide steering *is* the second-scene width.
- **Front echo pre-compensation** — injected an anti-phase 12 ms copy into the
  centre bus, +4–5 dB at 4–16 kHz; worse than the echo it removed.
- **Vocal-veto hysteresis / max-hold** — latched on transient false positives;
  in-mix rear fell 7–20 dB. Reverted.
- **MPS training refusal** — the M3 Max (≈20–50× the NAS) was refused *only* because
  two runs differed by ≤3e-8 (a self-imposed bit-exact-restart contract research
  doesn't need), leaving a 4-core Ryzen NAS as the real bottleneck
  (`memory/upmix_compute_mac_vs_nas.md`). **Iteration scale, not rigor, was the
  limit.**
- Also rejected: aggressive object-mask coherence gate; object-hold length as a
  lever (the real levers are confidence decay and calm pan). Deliberately **not**
  done in the realtime guard: triggering on real `deviceUnderruns` (risks the QSA
  record intercept that once crashed dio_manager), thread-CPU timing, and
  vectorizing the branchy mask/slot loops.

---

## 8. Where it stands, and what is still unknown

**Deployed 2026-09-22** (commit `b021ec9`, `docs/UPMIX_STATUS_2026-09-22.md`):
single-path scene code, primary q6d + V11 guard, surround 0.40 / front bass −3 /
LFE 0 / retention-HF 1000:2500; realtime type-100 upmix and the bed-distribution
flags are **parked, not shipped**. User verdict after the scene build: "звучать
стало шикарно, отличные яркие объекты сзади"; after the 2026-09-22 deploy: "всё
было отлично".

**Open items — the honest gaps:**

1. **No real on-car period-time distribution** — the realtime numbers in §6 are
   instruction-count estimates; the stop-line with `mean_us`/`cold_us`/`ms_hist`
   has never been captured on a drive.
2. **No Krait full-chain loaded trace** for the shipped path; the release-acceptance
   TARGET gate (P99 ≤ 80% of period, zero underruns over ≥2 h) is **unverified**
   (`docs/UPMIX_RELEASE_ACCEPTANCE_V2.md`).
3. **No blind listening / cabin acoustic evidence** — all tuning is on a conditional
   *electrical* DSP model, never SPL or a measured cabin transfer function.
4. **Known cheap-to-hit misses**: heavily processed/wide vocal intros (read as
   keys/bg), backing vocals as a scene, and electronic wide synth-bass whose
   harmonics >220 Hz q6d reads as keys — semantic, not frequency, errors
   (`docs/UPMIX_Q4_SECOND_SCENE_STATUS.md`).

---

## Source map

- **Runtime:** `hook/audio/hssr_dsp.{c,h}` (STFT/FFT, features, control decode,
  slots, mask/render, bed, attack), `hook/audio/hssr_neon.{c,h}` (quantized GRU),
  `hook/audio/hssr_control_worker.c` (async worker), `hook/audio/stereo_upmix.{c,h}`
  (S16 boundary, J525 matrix + cabin guard, LFE V2, shelves, limiter, rear delay).
- **Training:** `tools/stereo-upmix-render/train_hssr.py`, `train_cabin_v13.py`,
  `train_v15_streaming.py`; `tools/stereo-upmix-render/bench/results/2026-09-07-hssr-selection-v11.md`;
  `models/hssr-q6d-moises-a/manifest.json`.
- **Design & decisions:** `docs/STEREO_TO_5_1_UPMIX_DESIGN.md`,
  `docs/UPMIX_DOLBY_DISTRIBUTION_PLAN_2026-09-21.md`, `docs/UPMIX_C2_SOURCE_SELECTION.md`,
  `docs/UPMIX_Q2_ROLE_TARGETS.md`, `docs/UPMIX_TRAINING_DATA_AND_MODELS.md`,
  `docs/HSSR_DATASET_TRAINING_AUDIT.md`, `docs/UPMIX_INDEPENDENT_AUDIT_2026-09-17.md`,
  `docs/UPMIX_Q4_SECOND_SCENE_STATUS.md`, `docs/UPMIX_CABIN_V12.md`,
  `docs/UPMIX_CABIN_V13.md`, `docs/UPMIX_V15_STREAMING_TRAINING.md`,
  `docs/UPMIX_V15_ROUTING_REFINEMENT.md`, `docs/UPMIX_TEACHER_REVIEW_V14.md`,
  `docs/UPMIX_FULL_FLOAT_DATASET_V15.md`, `docs/UPMIX_TUNING_SWEEP_2026-09-20.md`,
  `docs/UPMIX_LONG_ONEVAR_2026-09-21.md`, `docs/UPMIX_R1_RUNTIME_BUDGET.md`,
  `docs/UPMIX_RELEASE_ACCEPTANCE.md`, `docs/UPMIX_RELEASE_ACCEPTANCE_V2.md`,
  `docs/UPMIX_STATUS_2026-09-22.md`, `docs/UPMIX_AGENT_HANDOFF_2026-09-19.md`,
  `docs/UPMIX_RELEASE_RUNTIME_AUDIT.md`; memory `upmix_*`.
