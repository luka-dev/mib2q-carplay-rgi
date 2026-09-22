# HSSR Model

*The AI that drives the CarPlay Virtual Cabin upmix: what it is, why 12-class separation failed, the second-scene design, the runtime, every iteration V9→q6d, and the frozen calibration.*

---

## 🎯 What HSSR is

HSSR = **H**igh-**S**electivity **S**emantic **R**ear-routing. It is a small, causal, quantized GRU **routing controller** — *not* a source separator. It never splits audio into stems. It listens to a stereo signal 93.75 times a second and emits, per frequency band, a decision: *how much of this belongs behind the listener, and where*. A deterministic DSP then builds a 5.1 field from that decision, on top of the reverse-engineered [[Virtual Cabin and Amp RE|J525 Virtual Cabin]].

The honest one-paragraph history: ~15 model generations tried to make a network *separate* music into 12 instrument classes on 240 songs, and it never worked — an independent audit found the trained model equalled "mute plus drums". Separation was abandoned for a **genre-agnostic second scene**: move the not-protected, spatially-wide part of the stereo image behind the listener, delayed so the front still anchors the sound. What ships is that DSP second scene **gated by two GRUs**: `q6d` (primary, real MoisesDB stems) with `V11` kept as a conservative vocal-protection guard.

Provenance: `docs/research/HSSR_UPMIX_MODEL.md`, `hook/audio/hssr_dsp.{c,h}`, `hook/audio/hssr_neon.{c,h}`. See also [[Stereo to 5.1 Upmix]] and [[Dolby Atmos]].

## 📚 Why the first approach failed

For roughly fifteen generations (V11…Q2) the model was asked to do supervised source separation over 240 songs. It never reached a regime where separation works. The 2026-09-17 independent audit is the tombstone (`docs/UPMIX_INDEPENDENT_AUDIT_2026-09-17.md`):

- The trained model after a full 24,288-update pass **equals MUTE + drums**; "+0.45 dB vs disabled" compares against an empty control, "−0.1 dB vs raw scalar" means it sits at its own initialization.
- Per-family DEV NMSE: drums 0.44, but lead 0.81, bass 0.81, guitar 0.90, backing 1.06, keys 1.84 — most heads output 13–17 dB below target (MSE shrinkage toward mute).
- Only **2,352 of 111,132 parameters** did per-bin separation; even *oracle* 32-band hints left backing NMSE 0.57–0.89 — the band-power interface **structurally cannot** separate lead/backing or keys/guitar.
- A static per-bin 2×2 Wiener filter with **zero trainable parameters** beat the trained model on every family except drums.

The conclusion was not "train longer" but "this problem is mis-posed": 12-class supervised separation on 240 songs cannot work; **the rear loudspeaker needs one mask, not twelve.**

## 🎯 The second-scene design

Given the Virtual Cabin, split the stereo image and move part of it behind the listener as a *second scene* — not ambience, not a copy of the mix, for any genre, with **no instrument classes** (adopted by Luka 2026-09-17). Rear = (NOT protected: lead vocal / bass / drum core / dominant harmonic line) AND (spatially wide / decorrelated / texture-like). Three components:

1. **DSP, genre-free** — per-bin pan, inter-channel coherence, texture (fast/slow envelope); low frequencies stay front.
2. **The only learned part** — a *protection mask*: what must **not** go rear, with an asymmetric loss (a leaked lead is far worse than a missed piece of support).
3. **Renderer** — rear delayed 10–20 ms, high-passed, decorrelated; front = X − α·rear.

**Why the rear is delayed.** The shipped mechanism delays the *rear only* by 12 ms (576 frames at 48 kHz) and leaves the front aligned, so the louder, earlier front anchors the image by the **precedence (Haas) effect** and the rear reads as distinct objects, not an echo. The in-mix rear copy sits ~20–30 dB under the front and 12 ms late — "which is why Luka hears objects, not a vocal echo". This is deliberately unlike Dolby.

> **Caveat.** "A learned protection mask, not 12 classes" is the design *direction*. The purpose-built learned mask (the Q4 line) was prototyped, measured **worse than V11 for vocals**, and shelved. What is deployed is a DSP azimuth/coherence second scene gated by q6d + the V11 guard.

## 🔧 How the runtime works

Output rail is fixed PCM6 `L, R, C, LFE, Ls, Rs`. **C is always digital zero** — the amplifier derives centre, so the hook must not.

```mermaid
flowchart TB
  accTitle: HSSR runtime signal path
  accDescr: One hop splits into a delayed front and an STFT feature path feeding the GRU controller, routing decode, object slots and bed, rear synthesis, then LFE assist and cabin guard to PCM6.
  inp["stereo S16 in"]
  fdl["front delay line 512 S16"]
  stft["STFT 512/hop128 -> 32-band features (368)"]
  gru["async GRU worker @ 93.75 Hz"]
  route["routing decode: vetoes + freq gates"]
  obj["object slots (4, pan-steered)"]
  bed["bed (delayed + cross term)"]
  rear["rear synth: 12 ms delay, +3 dB @4k"]
  front["FRONT L/R = delayed input minus retention"]
  lfe["LFE V2 assist"]
  cg["cabin guard (J525 peak, early-out)"]
  out["PCM6 S16 out"]
  inp --> fdl --> front
  inp --> stft --> gru --> route
  route --> obj --> rear
  route --> bed --> rear
  rear --> lfe
  front --> lfe
  lfe --> cg --> out
  class inp,out primary
  class rear,front success
  class gru,cg neutral
  classDef primary fill:#dbeafe,stroke:#2563eb,color:#1e3a5f
  classDef success fill:#dcfce7,stroke:#16a34a,color:#14532d
  classDef neutral fill:#f3f4f6,stroke:#6b7280,color:#1f2937
```

### Signal path (STFT / WOLA)

- FFT size **512**, hop **128**, 257 half-spectrum bins, **32 perceptual bands**, 64 four-bin "shape" bands; 4× overlap; latency 512 frames = **10.667 ms**.
- Half-sine analysis window; synthesis window divided by the per-phase COLA `Σw²` with the `1/N` folded in. In-place radix-2 512-point FFT; both output channels packed into one Hermitian spectrum and recovered with a **single** inverse FFT.
- The DSP core is float; the boundaries are S16. The **fronts are the raw input samples from a 512-deep S16 delay line** — a bit-exact delayed copy; only the rear is synthesized float (bounded ±2.0).
- The synchronous path (offline render tools) is **bit-exact**; the async worker differs only in scheduling, never arithmetic.

### The controller network

Shape **368 → GRU(160) → GRU(160) → Linear(264)**, int8 weights with per-row float dequant scales, loaded from an `HSSRNEO1` CRC32-validated blob. **451,464 parameters**, 469,760-byte export.

```mermaid
flowchart TB
  accTitle: HSSR controller network
  accDescr: 368 standardized features feed two GRU layers and a linear head producing 264 outputs split into per-band class logits, rearability, pan and global scalars.
  feat["features 368 = 32x9 + 64 shape + 16 global"]
  std["standardize, clamp +/-6, int8 @127/6"]
  g1["GRU layer 1 (368 to 160)"]
  g2["GRU layer 2 (160 to 160)"]
  lin["Linear head (160 to 264)"]
  outp["outputs 264 = 32 x [6 class, rearability, pan] + 8 global"]
  feat --> std --> g1 --> g2 --> lin --> outp
  class feat,outp primary
  class g1,g2 success
  classDef primary fill:#dbeafe,stroke:#2563eb,color:#1e3a5f
  classDef success fill:#dcfce7,stroke:#16a34a,color:#14532d
```

Runtime inference is **not** PyTorch: the C path quantizes matrix inputs and recurrent activations and uses rational sigmoid/tanh (bounded cost on old QNX libm), so loading the exported weights into PyTorch is not the shipped inference. The **6-class taxonomy** is the MoisesDB stem set: `0 vocals, 1 bass, 2 drums, 3 guitar, 4 keys, 5 other/background` — the last two are the "safe rear" classes. Of the 8 global outputs only `k=3..7` are used: scene confidence, rear-activity target, scene stability, fx-present, global safety.

### Routing, vetoes and object slots

Per band, rearability is multiplied by a chain of steep semantic vetoes, then smoothed asymmetrically (fast attack / slow release):

- `bass_keep = 1 − 0.85·p[bass]`, applied to the **5th power**.
- `lead_keep = 1 − 0.92·vocal_risk`, applied to the **4th power** (a leaked lead is far more audible than missed support); `vocal_risk` is `p[vocal]` reduced by band spatiality — a genuinely wide lead may survive, a centred lead cannot.
- drums `×(1−0.35·p[drums])`, guitar `×(1−0.10·p[guitar])`, background/FX lift `×(1+0.85·(p[keys]+p[other]))`, then scene-focus and global-safety scaling, then makeup `×16.0`.
- Hard frequency gates: **zero rear below 220 Hz**, ramp 220→350 Hz, ×0.65 above 15 kHz, ×0.25 above 20 kHz.

**Object slots**: 4 slots, two per rear half-plane (even→left, odd→right). Each band scores per slot from `semantic × directed-pan smoothstep × (0.55+0.45·coherence)`, transient-boosted; nothing below 220 Hz; confidence needs both an energy share and a peak candidate, held ~250 ms (94 hops). At render each bin picks the best slot, gated by per-bin ~45 ms coherence so room/reverb is not steered.

### The protection mask and the guard model

The protection mask keeps **lead vocal (class 0) and bass (class 1)** out of the rear, implemented as the steep vetoes above plus the "centred lead cannot acquire a slot" rule. The **guard** is an optional second GRU — V11, whose bias is "everything harmonic is vocal". Its vocal posterior is a safety net for the primary model's mislabelled leads. The guard's vocal probability is **discounted by the primary's confidence the band is keys/other** (`combined = gvocal·(1 − p[keys] − p[other])`), then max-merged into the vocal probability — so a voice the guard hears is protected even when q6d reads it as keys. **This hybrid veto is why two models ride together.**

### Front retention and the HF taper

`front_retention = r` is the object share left in the front. Energy conservation bounds it: `r* = sqrt(1−b²)` (b = rear gain); above `r*` the object localizes forward and the second scene collapses, so `r` is chosen below `r*`. Measurement showed retention lifted the front **broadband** (+1.3 dB across 0.5–16 kHz) — the "too much treble in front" the user reported — because bright objects carry their treble into the front half they leave behind. The fix makes retention frequency-dependent: `retention_eff(band) = r · (1 − smoothstep(bandEdgeHz, lo, hi))`, defaults `lo=1000 Hz`, `hi=2500 Hz`. Deployed 2026-09-22.

### Async execution and the realtime guard

Inference runs every 4th hop (**93.75 Hz** = 48000/(128·4)). A `SCHED_RR` priority-17 worker owns the recurrent state; the audio thread submits features and reads results through two-entry SPSC mailboxes with acquire/release atomics — no locks, no allocation, no logging on the audio thread. One or two missed deadlines keep the last stable mask; the third consecutive miss decays it toward a quiet rear.

The realtime media path (`AUDIO_REALTIME_UPMIX=on`, parked) runs through the same renderer inside the stock QSA thread; period 768 frames = 16 ms, +10.7 ms front latency. Old 8 ms builds self-disabled in 3 of 5 recorded sessions. Current guard: **budget 10 ms**, disable needs **4 consecutive** overruns, **16-period cold-start forgiveness**, and **pause-based revival only** (a ≥500 ms gap revives cold; no mid-track rear reappearance). Speedup numbers are QEMU `-icount` estimates, **bit-identical**, not on-car measurements.

## 📊 The model iterations

All variants use the same **"HSSR-160" graph** (`GRU(368→160)→GRU(160→160)→Linear(160→264)`, 451,464 params); only weights differ. A width ablation (160→192 = 597k; 160→256 = 943k) was costed but **never adopted**.

*Timeline of the model lineage from V9 through the shipped q6d.*

```mermaid
timeline
  title HSSR model lineage
  pre 09-07 : V9 prior production baseline
  2026-09-07 : V11 SELECTED (MUSDB, selectivity 59x) : now the guard
  2026-09-08 : V12 A-E all rejected : V13 renderer + data fixes
  2026-09-09 : V14 teachers + review (no trainer)
  2026-09-09 to 11 : V15 FLOAT A/B/C all rejected
  2026-09-18 : q6d SELECTED (MoisesDB real 6-class) : shipped primary
```

| Gen | Date | What it did | Outcome |
|---|---|---|---|
| **V9** | pre-09-07 | prior production baseline, same graph | superseded by V11 |
| **V11** | 2026-09-07 | MUSDB18-derived; sharpened selectivity, no graph change | **SELECTED**; selection ratio 17.4×→**59.1×**; SHA `ab01463e…`. Now the **guard** |
| **V12 A–E** | 2026-09-08 | fine-tune V11 on 43 songs + BS-RoFormer teacher | **ALL REJECTED**; A: vocal leakage +18–49%; ship = new float hook + original V11 |
| **V13** | 2026-09-08 | bass fix + cabin renderer; routing-head-only pilot | renderer/data fixes kept; **no new weights** |
| **V14** | 2026-09-09 | 50-song corpus teacher build + review | **no trainer launched**; teacher filenames must not become truth |
| **V15 A/B/C** | 2026-09-09→11 | rebuilt dataset in **FLOAT**; streaming distillation | **ALL REJECTED**; every candidate moved *more* vocal to the rear |
| **q6d** | 2026-09-18 | first trained on **real dataset stems** (MoisesDB 6-class); **full network** fine-tune from V11 | **SELECTED** as shipped primary; best epoch 3; SHA `96abe57d…`. Weakness: **confuses vocals with keys** → why the guard exists |

**Datasets.** MoisesDB v0.1 (240 songs, 181 TRAIN / 40 DEV) for q6d's real 6-class targets; MUSDB18 (150 tracks) for V11; a 43-song private corpus for V11/V12/V13; a 50-song local corpus for V14/V15 teachers. Teacher networks (Demucs6, BS-RoFormer, karaoke, AST/CLAP) only ever **labelled** data — q6d used real stems, not pseudo-labels. Both shipped weights are flagged **development-only / non-commercial**.

## 📌 Calibration

The 60-second one-variable grid (2026-09-21) corrected several earlier conclusions:

- **6-second prefixes are not representative** — they understated bass/mid tilt by ~1.5 dB mean.
- **Front bass is the only control that moves bass** (+3.3 dB per 3 dB shelf); retention does **not** touch bass.
- **LFE is not a monotonic sub-volume knob** in the corrected model.

**The bass culprit (2026-09-19).** The 2026-09-18 build silently raised the LFE default from 4096 to 12288 (+9.5 dB) and widened the LFE LR4 48→60 Hz; since the 5.1 bank feeds the sub from LFE only, the whole sub got +9.5 dB — the "bass everywhere" the user heard. Fixed: default back to 4096 plus the `AUDIO_STEREO_LFE_GAIN` knob.

The frozen production values are in [[Stereo to 5.1 Upmix]]. Luka, 2026-09-21: *current calibration is OK for now — do not tune.*

## ❌ What was tried and rejected

- **Q4 learned protection mask** — leaked lead in-mix at −7.9 dB where V11 held solo vocals at −23…−26 dB. Worse than V11; the ML-mask line was paused.
- **Float render candidate** — the one-variable grid proved its "tilt reduction" was actually a +1.9 dB bass increase; the float/cabin/quiet path was deleted.
- **TRUEPAN** (objects keep own pan) — rear L/R correlation jumped to 0.86–0.90 (rear went near-mono); wide steering *is* the second-scene width.
- **Front echo pre-compensation** — injected an anti-phase 12 ms copy into the centre bus; worse than the echo it removed.
- **Vocal-veto hysteresis / max-hold** — latched on transient false positives; in-mix rear fell 7–20 dB. Reverted.
- **MPS training refusal** — the M3 Max was refused only because two runs differed by ≤3e-8; iteration scale, not rigor, was the limit.

## ⚠️ Where it stands

**Deployed 2026-09-22** (commit `b021ec9`): single-path scene code, primary q6d + V11 guard, surround 0.40 / front bass −3 / LFE 0 / retention-HF 1000:2500; realtime type-100 upmix and the bed-distribution flags are **parked**. User verdict after the scene build: "звучать стало шикарно, отличные яркие объекты сзади".

**Open items** — no on-car period-time distribution **[open]**; no Krait full-chain loaded trace, TARGET gate unverified **[open]**; no blind listening / cabin SPL **[open]**; known semantic misses (processed vocal intros, wide synth-bass) read as keys **[car]**. See [[Landmines]].
