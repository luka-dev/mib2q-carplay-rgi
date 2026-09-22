# HSSR Model

*The AI that drives the CarPlay Virtual Cabin upmix: what it is, why 12-class separation failed, the second-scene design, the runtime, every iteration V9→q6d, and the shipped calibration.*

---

## 🎯 What HSSR is

HSSR = High-Selectivity Semantic Rear-routing. It is a small, causal, quantized GRU **routing controller** — not a source separator. It never splits audio into stems. It listens to a stereo signal 93.75 times a second and emits, per frequency band, a decision: *how much of this belongs behind the listener, and where*. A deterministic DSP then builds a 5.1 field from that decision, on top of the reverse-engineered [[Virtual Cabin and Amp RE|J525 Virtual Cabin]].

The honest one-paragraph history: ~15 model generations tried to make a network *separate* music into 12 instrument classes on 240 songs, and it never worked — an independent audit found the trained model equalled "mute plus drums". Separation was abandoned for a genre-agnostic **second scene**: move the not-protected, spatially-wide part of the stereo image behind the listener, delayed so the front still anchors the sound. What ships is that DSP second scene gated by two GRUs: `q6d` (primary, real MoisesDB stems) with `V11` kept as a conservative vocal-protection guard.

Provenance: `docs-obsidian/Research/HSSR_UPMIX_MODEL.md`, `hook/audio/hssr_dsp.{c,h}`, `hook/audio/hssr_neon.{c,h}`. See also [[Stereo to 5.1 Upmix]] and [[Dolby Atmos]].

## 📚 Why the first approach failed

For roughly fifteen generations (V11…Q2) the model was asked to do supervised source separation over 240 songs. It never reached a regime where separation works. The 2026-09-17 independent audit is the tombstone (`docs-obsidian/Sources/UPMIX_INDEPENDENT_AUDIT_2026-09-17.md`):

- The trained model after a full 24,288-update pass equals `MUTE + drums`; "+0.45 dB vs disabled" compares against an empty control, "−0.1 dB vs raw scalar" means it sits at its own initialization.
- Per-family DEV NMSE: drums 0.44, but lead 0.81, bass 0.81, guitar 0.90, backing 1.06, keys 1.84 — most heads output 13–17 dB below target (MSE shrinkage toward mute).
- Only 2,352 of 111,132 parameters did per-bin separation; even *oracle* 32-band hints left backing NMSE 0.57–0.89 — the band-power interface structurally cannot separate lead/backing or keys/guitar.
- A static per-bin 2×2 Wiener filter with zero trainable parameters beat the trained model on every family except drums.

The conclusion was not "train longer" but "this problem is mis-posed": 12-class supervised separation on 240 songs cannot work; the rear loudspeaker needs **one mask, not twelve**.

## 🎯 The second-scene design

Given the Virtual Cabin, split the stereo image and move part of it behind the listener as a *second scene* — not ambience, not a copy of the mix, for any genre, with no instrument classes (adopted by Luka 2026-09-17). Rear = (NOT protected: lead vocal / bass / drum core / dominant harmonic line) AND (spatially wide / decorrelated / texture-like). Three components:

1. **DSP, genre-free** — per-bin pan, inter-channel coherence, texture (fast/slow envelope); low frequencies stay front.
2. **The only learned part** — a *protection mask*: what must not go rear, with an asymmetric loss (a leaked lead is far worse than a missed piece of support).
3. **Renderer** — rear delayed 10–20 ms, high-passed, decorrelated; front = X − α·rear.

**Why the rear is delayed.** The shipped mechanism delays the rear only, by 12 ms (`SPA_SCENE_REAR_DELAY_FRAMES = 576` at 48 kHz, `hook/audio/stereo_upmix.h:31`), and leaves the front aligned, so the louder, earlier front anchors the image by the precedence (Haas) effect and the rear reads as distinct objects, not an echo. The in-mix rear copy sits ~20–30 dB under the front and 12 ms late — "which is why Luka hears objects, not a vocal echo". This is deliberately unlike Dolby.

> **Caveat.** "A learned protection mask, not 12 classes" is the design *direction*. The purpose-built learned mask (the Q4 line) was prototyped, measured worse than V11 for vocals, and shelved. What is deployed is a DSP azimuth/coherence second scene gated by q6d + the V11 guard.

## 🔧 How the runtime works

Output rail is fixed PCM6 `L, R, C, LFE, Ls, Rs`. **C is always digital zero** — the amplifier derives centre, so the hook must not.

```mermaid
flowchart TB
  accTitle: HSSR runtime signal path
  accDescr: One hop splits into a delayed front and an STFT feature path feeding the GRU controller, routing, object slots and the scene bed, then rear synthesis, LFE assist and the cabin guard to PCM6.
  subgraph analysis["Analysis and control"]
    stereo_in["stereo S16 in"]
    stft_features["STFT to 368 features"]
    gru_worker["async GRU worker 93.75 Hz"]
    routing_decode["routing vetoes and gates"]
  end
  subgraph synthesis["Scene synthesis"]
    object_slots["four object slots"]
    scene_bed["scene bed delays and cross"]
    rear_synth["rear synth 12 ms"]
    front_delay["front delay line 512"]
    front_out["front minus retention"]
  end
  subgraph output_stage["Output stage"]
    lfe_assist["LFE V2 assist"]
    cabin_guard["cabin guard early-out"]
    pcm6_out["PCM6 S16 out"]
  end
  stereo_in --> front_delay --> front_out
  stereo_in --> stft_features --> gru_worker --> routing_decode
  routing_decode --> object_slots --> rear_synth
  routing_decode --> scene_bed --> rear_synth
  rear_synth --> lfe_assist
  front_out --> lfe_assist
  lfe_assist --> cabin_guard --> pcm6_out
  classDef primary fill:#dbeafe,stroke:#2563eb,stroke-width:2px,color:#1e3a5f
  classDef success fill:#dcfce7,stroke:#16a34a,stroke-width:2px,color:#14532d
  classDef neutral fill:#f3f4f6,stroke:#6b7280,stroke-width:2px,color:#1f2937
  class stereo_in,pcm6_out primary
  class rear_synth,front_out success
  class gru_worker,cabin_guard neutral
```

### Signal path (STFT / WOLA)

- FFT size 512, hop 128, 257 half-spectrum bins, 32 perceptual bands, 64 four-bin "shape" bands, control every 4th hop; latency 512 frames = 10.667 ms (`hook/audio/hssr_dsp.h:15-24`).
- Half-sine analysis window; synthesis window divided by the per-phase COLA `Σw²` with the `1/N` folded in. In-place radix-2 512-point FFT; both output channels packed into one Hermitian spectrum and recovered with a single inverse FFT.
- The DSP core is float; the boundaries are S16. The fronts come from a 512-deep S16 delay line; only the rear is synthesized float (bounded ±2.0).
- The synchronous path (offline render tools) is bit-exact; the async worker differs only in scheduling, never arithmetic.

### The controller network

Shape `368 → GRU(160) → GRU(160) → Linear(264)` (`hook/audio/hssr_neon.h:17-19`), int8 weights with per-row float dequant scales, loaded from an `HSSRNEO1` CRC32-validated blob (`hook/audio/hssr_neon.c:258-272`). 451,464 parameters, 469,760-byte export (64-byte header + payload, `hssr_neon.h:22-34`).

```mermaid
flowchart TB
  accTitle: HSSR controller network
  accDescr: 368 standardized features feed two GRU layers and a linear head producing 264 outputs split into per-band class logits, rearability, pan and global scalars.
  input_features["368 features 32x9 plus 80"]
  standardize_int8["standardize and int8 quantize"]
  gru_layer_1["GRU layer 1 368 to 160"]
  gru_layer_2["GRU layer 2 160 to 160"]
  linear_head["linear head 160 to 264"]
  control_outputs["264 control outputs"]
  input_features --> standardize_int8 --> gru_layer_1 --> gru_layer_2 --> linear_head --> control_outputs
  classDef primary fill:#dbeafe,stroke:#2563eb,stroke-width:2px,color:#1e3a5f
  classDef success fill:#dcfce7,stroke:#16a34a,stroke-width:2px,color:#14532d
  class input_features,control_outputs primary
  class gru_layer_1,gru_layer_2 success
```

The 368 features are 32 bands × 9 + 64 shape + 16 global; they are standardized, clamped to ±6 and quantized at 127/6 (`hook/audio/hssr_neon.c:378`). The 264 outputs are 32 bands × [6 class logits, rearability, pan] + 8 globals.

Runtime inference is not PyTorch: the C path quantizes matrix inputs and recurrent activations and uses rational sigmoid/tanh (bounded cost on old QNX libm), so loading the exported weights into PyTorch is not the shipped inference. The 6-class taxonomy is the MoisesDB stem set: `0 vocals, 1 bass, 2 drums, 3 guitar, 4 keys, 5 other/background` — the last two are the "safe rear" classes. Of the 8 global outputs only `k=3..7` are used: scene confidence, rear-activity target, scene stability, fx-present, global safety.

### Routing, vetoes and object slots

Per band, rearability is multiplied by a chain of steep semantic vetoes, then smoothed asymmetrically (fast attack 0.35 / slow release 0.035) (`hook/audio/hssr_dsp.c:642-761`):

- `bass_keep = 1 − 0.85·p[bass]`, applied to the 5th power.
- `lead_keep = 1 − 0.92·vocal_risk`, applied to the 4th power (a leaked lead is far more audible than missed support); `vocal_risk = p[vocal]·(1 − 0.80·spatiality)` — a genuinely wide lead may survive, a centred lead cannot.
- drums `×(1−0.35·p[drums])`, guitar `×(1−0.10·p[guitar])`, background/FX lift `×(1+0.85·(p[keys]+p[other]))`, then scene-focus and global-safety scaling, then makeup `×16.0` (`hssr_dsp.c:17`).
- Hard frequency gates: zero rear below 220 Hz, ramp 220→350 Hz, ×0.65 above 15 kHz, ×0.25 above 20 kHz (`hssr_dsp.c:750-758`).

**Object slots**: 4 slots, two per rear half-plane (even→left, odd→right). Each band scores per slot from `semantic × directed-pan smoothstep × (0.55+0.45·coherence)`, transient-boosted; nothing below 220 Hz; confidence needs both an energy share and a peak candidate, held 250 ms (`SPA_HSSR_SCENE_HOLD_HOPS = 94`, `hssr_dsp.c:940-941`). At render each bin picks the best slot, gated by per-bin ~45 ms coherence so room/reverb is not steered.

### The protection mask and the guard model

The protection mask keeps lead vocal (class 0) and bass (class 1) out of the rear, implemented as the steep vetoes above plus the "centred lead cannot acquire a slot" rule. The guard is a second GRU — V11, whose bias is "everything harmonic is vocal" — loaded from `AUDIO_HSSR_GUARD_MODEL` or, by default, `hssr_guard.bin` beside the main model (`hook/audio/audio_hook.c:1754-1771`).

Its vocal posterior is **discounted by the primary's confidence** the band is keys/other (`combined = gvocal·(1 − p[keys] − p[other])`), then max-merged into the vocal probability; it also overrides the wide-lead escape and scales rearability by `(1 − 0.95·guard_vocal)²` (`hssr_dsp.c:613-673`). So a voice the guard hears is protected even when q6d reads it as keys. This hybrid veto is why two models ride together.

### Front retention and the HF taper

`front_retention = r` is the object share left in the front. Energy conservation bounds it: `r* = sqrt(1−b²)` (b = rear gain); above `r*` the object localizes forward and the second scene collapses, so `r` is chosen below `r*`. The shipped value is `r = 0.5` (`deploy/smartphone_integrator/carplay_startup.sh:75`; code default 0, `hook/audio/audio_hook.c:183-194`).

Measurement showed ungated retention kept each bright object's treble in the front (+1.3 dB in the front 2–16 kHz, per the code comment at `hssr_dsp.c:1576-1578`). The fix makes retention frequency-dependent: `retention_eff(band) = r · (1 − smoothstep(bandEdgeHz, lo, hi))`, defaults `lo=1000 Hz`, `hi=2500 Hz` (`hook/audio/stereo_upmix.h:42-43`, `hssr_dsp.c:1579-1582`); `AUDIO_HSSR_RETENTION_HF=LO:HI` overrides it and `HI<=LO` restores flat retention. With `AUDIO_HSSR_BED_ATTACK=owner` the per-band front keep becomes `1 − r·(1 − collapse·w·owner)` (`hssr_dsp.c:1583-1590`). Committed 2026-09-21 (`48b9851`).

### Scene-bed distribution

Shipped on (`carplay_startup.sh:76-78`), default off in code (`hook/audio/audio_hook.h:78-81`):

- **Bed delays** (`AUDIO_HSSR_BED_DELAYS=on`) — only the bed is decorrelated; each band mixes `0.79·now + 0.60·delayed_same − 0.14·delayed_opposite` (unit energy) through a 16-hop per-bin ring, with per-band delays of 1–15 hops (2.667 ms each), longer at low frequency and different for Ls and Rs (`hssr_dsp.c:52-63`, `:1190-1313`). Objects stay coherent.
- **Bed attack** (`AUDIO_HSSR_BED_ATTACK=owner`, `AUDIO_HSSR_ATTACK_COLLAPSE=0.75`) — the ported Dolby transient detector (`β = 0.9637` per hop, `hssr_dsp.c:65-66`, `:1171-1189`). In owner mode a hit fades the delayed copies where a second-scene object owns the band and collapses an ownerless bed row forward (×0.39); `front` mode is the Dolby reference, always forward.

### Async execution and the realtime guard

Inference runs every 4th hop (93.75 Hz = 48000/(128·4)). A `SCHED_RR` priority-17 worker owns the recurrent state (`hook/audio/audio_hook.c:1963-1964`, `:3959`; `hook/audio/hssr_control_worker.c:116-131`); the audio thread submits features and reads results through two-entry mailboxes (`hssr_control_worker.h:45-46`) — no locks, no allocation, no logging on the audio thread. One or two missed deadlines keep the last stable mask; the third consecutive miss decays it toward a quiet rear (`hssr_dsp.c:1160-1165`).

The realtime media path (`AUDIO_REALTIME_UPMIX=on`, shipped, `carplay_startup.sh:86`) runs the same renderer inside the stock QSA thread on its own state and worker; period 768 frames = 16 ms, +10.7 ms front latency. Old 8 ms builds self-disabled in 3 of 5 recorded sessions. The current guard (`hook/audio/rt_upmix_guard.h:24-34`):

| Rule | Value |
|---|---|
| Budget per period | 10 ms |
| Disable after | 4 consecutive overruns |
| Cold-start forgiveness | 16 periods |
| Revival | only after a ≥ 500 ms gap in the stream (no mid-track rear reappearance) |

Speedup numbers are QEMU `-icount` estimates, bit-identical, not on-car measurements.

## 📊 The model iterations

All variants use the same "HSSR-160" graph (`GRU(368→160)→GRU(160→160)→Linear(160→264)`, 451,464 params); only weights differ. A width ablation (160→192 = 597k; 160→256 = 943k) was costed but never adopted.

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
| V9 | pre-09-07 | prior production baseline, same graph | superseded by V11 |
| V11 | 2026-09-07 | MUSDB18-derived; sharpened selectivity, no graph change | **SELECTED**; selection ratio 17.4×→59.1×; SHA `ab01463e…`. Now the guard |
| V12 A–E | 2026-09-08 | fine-tune V11 on 43 songs + BS-RoFormer teacher | all rejected; A: vocal leakage +18–49%; ship = new float hook + original V11 |
| V13 | 2026-09-08 | bass fix + cabin renderer; routing-head-only pilot | renderer/data fixes kept; no new weights |
| V14 | 2026-09-09 | 50-song corpus teacher build + review | no trainer launched; teacher filenames must not become truth |
| V15 A/B/C | 2026-09-09→11 | rebuilt dataset in float; streaming distillation | all rejected; every candidate moved *more* vocal to the rear |
| q6d | 2026-09-18 | first trained on real dataset stems (MoisesDB 6-class); full network fine-tune from V11 | **SELECTED** as shipped primary; best epoch 3; SHA `96abe57d…`. Weakness: confuses vocals with keys → why the guard exists |

Both shipped blobs are in the repo: `models/hssr-q6d-moises-a/hssr_model.bin` (SHA-256 `96abe57d…`) and `models/hssr-selection-v11/hssr_guard.bin` (`ab01463e…`).

**Datasets.** MoisesDB v0.1 (240 songs, 181 TRAIN / 40 DEV) for q6d's real 6-class targets; MUSDB18 (150 tracks) for V11; a 43-song private corpus for V11/V12/V13; a 50-song local corpus for V14/V15 teachers. Teacher networks (Demucs6, BS-RoFormer, karaoke, AST/CLAP) only ever labelled data — q6d used real stems, not pseudo-labels. Both shipped weights are flagged development-only / non-commercial.

## 📌 Calibration

The 60-second one-variable grid (2026-09-21) corrected several earlier conclusions:

- **6-second prefixes are not representative** — they understated bass/mid tilt by ~1.5 dB mean.
- **Front bass is the only control that moves bass** (+3.3 dB per 3 dB shelf); retention does not touch bass.
- **LFE is not a monotonic sub-volume knob** in the corrected model.

**The bass culprit (2026-09-19).** The 2026-09-18 build silently raised the LFE default from 4096 to 12288 (+9.5 dB) and widened the LFE LR4 48→60 Hz; since the 5.1 bank feeds the sub from LFE, the whole sub got +9.5 dB — the "bass everywhere" the user heard. Fixed: default back to 4096 (`hook/audio/stereo_upmix.h:35`) plus the `AUDIO_STEREO_LFE_GAIN` knob, dB re 4096, −24..+12 (`hook/audio/audio_hook.c:276-295`).

Luka, 2026-09-21: *current calibration is OK for now — do not tune.* The scene-bed variant (retention 0.5, bed delays, owner attack) was chosen after a headphone A/B the same day and is what ships; the full shipped table is in [[Stereo to 5.1 Upmix]].

## ❌ What was tried and rejected

- **Q4 learned protection mask** — leaked lead in-mix at −7.9 dB where V11 held solo vocals at −23…−26 dB. Worse than V11; the ML-mask line was paused.
- **Float render candidate** — the one-variable grid proved its "tilt reduction" was actually a +1.9 dB bass increase; the float/cabin/quiet path was deleted (commit `9255485`).
- **TRUEPAN** (objects keep own pan) — rear L/R correlation jumped to 0.86–0.90 (rear went near-mono); wide steering *is* the second-scene width.
- **Front echo pre-compensation** — injected an anti-phase 12 ms copy into the centre bus; worse than the echo it removed.
- **Vocal-veto hysteresis / max-hold** — latched on transient false positives; in-mix rear fell 7–20 dB. Reverted.
- **MPS training refusal** — the M3 Max was refused only because two runs differed by ≤3e-8; iteration scale, not rigor, was the limit.

## ⚠️ Where it stands

**Deployed 2026-09-22:** single-path scene code, primary q6d + V11 guard, surround 0.40 / front bass −3 / LFE 0 / retention 0.5 with HF taper 1000:2500 / bed delays on / owner attack at 0.75, and the realtime type-100 upmix on (`carplay_startup.sh:63-86`). User verdict after the scene build: "звучать стало шикарно, отличные яркие объекты сзади".

**Open items** — no on-car period-time distribution `[open]`; no Krait full-chain loaded trace, TARGET gate unverified `[open]`; no blind listening / cabin SPL `[open]`; known semantic misses (processed vocal intros, wide synth-bass) read as keys `[car]`. See [[Landmines]].
