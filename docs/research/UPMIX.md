# UPMIX — the whole stereo→5.1 pipeline, end to end

*The master map: from the phone's audio to the 19 speakers, every stage, with
diagrams. The deep detail lives in three companion papers — this ties them
together:*

- **The AI** → [`HSSR_UPMIX_MODEL.md`](HSSR_UPMIX_MODEL.md)
- **The amp / Virtual Cabin** → [`VIRTUAL_CABIN.md`](VIRTUAL_CABIN.md)
- **Dolby Atmos teardown** → [`DOLBY_ATMOS_TEARDOWN.md`](DOLBY_ATMOS_TEARDOWN.md)
- **Traps** → [`LANDMINES.md`](LANDMINES.md)

> Truth levels are marked **[car]** (confirmed on the unit), **[model]** (offline
> render / static RE), **[open]** (unverified).

---

## 1. The big picture

Two things had to be built: **get the right six PCM channels**, and **synthesize
them for stereo content**. The amp (J525 B&O) is a complete 5.1 renderer — the
"Virtual Cabin" — so the head unit only feeds it `L R C LFE Ls Rs` and lets it do
the acoustics.

```
                                iPhone
                                  │   CarPlay / AirPlay over USB-NCM
                                  ▼
                 ┌──────────── two audio transports ────────────┐
                 │  realtime type-100         buffered type-103  │
                 │  PCM only, STEREO          APAC / AAC-LC      │
                 │  (stock path)              (our hook supplies)│
                 └───────────────────────────────┬──────────────┘
                                                  ▼   decode; the inline magic
                                                      cookie is codec authority
                                        classify the decoded PCM
                          ┌───────────────────────┴───────────────────────┐
                          ▼                                                 ▼
                 APAC 5.1 / 5.1.2                                    stereo AAC-LC
                 reorder → (8→6 fold)                                      │
                          │                                                ▼
                    DISCRETE 5.1                                   ┌───── HSSR UPMIX ─────┐
                 (bypasses the upmix)                              │ STFT → GRU control → │
                          │                                        │ routing → objects+bed│
                          │                                        │ → rear synth + front │
                          │                                        │ (delayed) → PCM6     │
                          │                                        └──────────┬───────────┘
                          └──────────────────┬──────────────────────────────┘
                                             ▼
                          cabin guard (S16 peak) → PCM6  [ L R C=0 LFE Ls Rs ]
                                             ▼
              deva-ctrl-qc (6-voice, asad=0xfc) → CSD dev78 → LPASS/AFE TDM (6×24)
                                             ▼
                    MMX INIC → MOST150 ring (BlockWidth 26) → RCC INIC → J525 amp
                                             ▼
        V850 + SHARC:  input matrix (profile0/bank1) → DSP chain → 16 amplified outputs
                                             ▼
                             19 speakers  —  the "Virtual Cabin"
```

The rest of this doc walks each stage top to bottom.

---

## 2. Ingest — where the audio comes from, and what triggers the upmix

There are **two** CarPlay audio streams (`docs/reference/AUDIO.md §21`):

```
 "audioOutputFormats"          → type 100  REALTIME   PCM only → 48k/16/STEREO on the car [car]
 "audioOutputFormatsExtended"  → type 103  BUFFERED   AAC-LC(23) / APAC 5.1(65) / 5.1.2(66)
                                            lossless + spatial live here; OUR hook supplies it
```

The stock realtime stream never carries multichannel, and phone-side spatial over
it is blocked by a phone feature flag (`ScreenSpatialAudioReceiver`) — see the
Dolby paper. So all multichannel and all upmix input rides the **buffered type-103**
path. On that path:

```
 APAP frame (BE32 + ChaCha20-Poly1305) ─► TLV/cookie extract ─► decode
    │                                          the 33-byte inline COOKIE decides the
    │                                          decoder, NOT the APAP index hint  [car]
    ├─ AAC-LC (index 23)  → Cinemo libNmeSDK  ─┐
    └─ APAC   (index 65/66) → libcarplay_apac ─┤
                                               ▼
                                classify decoded PCM (codec + channels + layout)
                                 ├─ 5.1 / 5.1.2 → reorder to L R C LFE Ls Rs (+8→6 fold) → DISCRETE
                                 └─ stereo        → HSSR UPMIX
```

iOS may alternate AAC-LC ↔ APAC **inside one session**; the cookie authority makes
that switch seamless. iOS never sends ordinary stereo as APAC 5.1, so normal music
decodes as stereo and is upmixed on our side. **[car]** for the 5.1 case end to end.

---

## 3. HSSR upmix — the core

HSSR is a **causal routing controller**, not a separator: it decides, per frequency
band 93.75×/s, how much of the signal belongs behind the listener and where. A
deterministic DSP builds the field from that decision. One 128-sample hop:

```
stereo S16 in
  │
  ├─► front delay line (512, S16) ───────────────────────────► FRONT L/R (bit-exact delayed input)
  │
  └─► STFT 512 / hop 128 ─► 32-band features (368) ─► [async GRU worker @ 93.75 Hz]
                                                          │ per band: 6-class, rearability, pan
                                                          ▼
                        routing decode  (vetoes lead_keep⁴, bass_keep⁵, guard²;
                                         freq gates 0<220 Hz, ramp→350, roll >15k)
                                                          │
                              ┌───────────────────────────┴───────────────┐
                              ▼                                            ▼
                        object slots (4, pan-steered)             bed (ambience / fallback,
                              │                                    delayed + cross term −0.18)
                              └──────────────► rear synth ◄────────────────┘
                                        rear delay 12 ms · Ls/Rs +3 dB @4k shelf
                                                          │
   FRONT L/R  ── minus (1 − retention_eff(f))·source ────┤   ◄ retention HF taper 1000:2500
                                                          ▼
                                     [ L  R  C=0  LFE  Ls  Rs ]   float → S16
```

Key design choices (detail in the HSSR paper §2–§3):
- **The front is a bit-exact delayed copy of the input** (S16 delay line), not the
  STFT output — only the rear is synthesized float.
- **Second scene, by precedence.** The rear is delayed **12 ms** so the louder,
  earlier front anchors the image (Haas) and the rear reads as distinct objects,
  not an echo. This is the deliberate departure from Dolby, which never sends the
  dominant object rearward.
- **C = 0 on purpose** — the amp derives the centre from L+R (§7).

### The controller network

```
features (368) = 32 bands × 9  +  64 shape  +  16 global
      │  standardize, clamp ±6, int8 @127/6
      ▼
  GRU(368→160) → GRU(160→160) → Linear(160→264)     int8 GEMV, rational σ/tanh, reset-after GRU
      ▼
outputs (264) = 32 bands × [ 6 class logits , rearability , pan ]  +  8 global
                            softmax          σ→smoothstep   tanh      (k=3..7 used)
```

451,464 params; the 6 classes are the MoisesDB stems (0 vocals, 1 bass, 2 drums,
3 guitar, 4 keys, 5 other) — 4/5 are the "safe rear" classes. **Two models ride
together:** primary **q6d** (real-stem trained) + a **V11 guard** whose vocal
posterior protects voices q6d mislabels as keys. Full lineage V9→…→q6d in the HSSR
paper §4.

### Rear-scene construction (all env-flagged extras default off, bit-exact when off)

```
 masked object ──► object slots ──► rear (×0.40)  ──┐
                                    +3 dB@4k shelf   │   rear delay 12 ms
 residual/bed ──► split ──► per-band delayed copies ─┤   + cross term −0.18
                            (Dolby-residual shape)   │   owner attack: collapse to band owner
 front ◄── retention r·(1−smoothstep(f,1000,2500)) ──┘
 LFE  ◄── V2 assist: LR4 low-pass + correlation-driven L±R fold, dynamic gain, cap 0.125
```

---

## 4. Cabin guard → PCM6 out

Before output, an S16 **cabin guard** predicts each amp bus peak (using a linear
copy of the amp's profile-0/bank-1 matrix) so no bus clips. It is a **peak-detection
model, not acoustics**:

```
 peak · 3.53 (largest column norm) ≤ 32275 ?  ── yes ─► return (hot-path early-out)
                    │ no
                    ▼   repair via LFE-only feasible interval (never synthesize/flip LFE)
                    ▼   else linked-scale all six channels
                 PCM6 [ L R C=0 LFE Ls Rs ] S16  ────────────────► to deva
```

---

## 5. Down to the amp — deva / MOST / J525

Getting six channels here took two months and two independent fixes (LANDMINES +
Virtual Cabin paper §3). Both are **[car]**-confirmed:

```
 PCM6 ─► deva-ctrl-qc  ── caps mirror mpl1 (6 voices; min==active geometry) ──┐
    │                     createMetadata announces asad=0xfc / 6ch / 24-bit    │
    ▼                                                                          ▼
 CSD dev 78 (48k/24) ─► LPASS/AFE  TDM_OUT_MPL  (6 × 24-bit slots, "ENT_MOST")
    ▼
 MMX INIC (OS81xxx) ─► MOST150 synchronous ring (BlockWidth 26) ─► RCC INIC ─► J525 amp
    ▼
 J525:  V850 (MOST/FBlock, EQ)  +  SHARC AD-2148x (matrix + DSP chain)
    ▼
 profile-0 / bank-1 input matrix (6 in → 8 buses) ─► delays ─► EQ ─► mixers ─► selectors
    ▼                                                  ─► output EQ ─► limiters ─► gain
 16 amplified T38 outputs ─► 19 speakers
```

What the amp does for us (so the hook doesn't): **derives the centre** (C bus from
L+R at −6 dB), **manages the sub** (its own LFE bus at +3 dB), crosses over every
driver, and fans out to 19 speakers. That is the Virtual Cabin. The bus→speaker
binding and per-driver EQ are **[model]/inferred**, not recovered — see the Virtual
Cabin paper §4.

Two gates decide whether any of this runs:
- **`AUDIO_SPATIAL`** gated on **E2P 0xFC** — only B&O (external) cars have a
  6-channel route; internal-amp cars get stereo only (LANDMINES).
- **Realtime vs buffered** — the realtime media path can also run the upmix under a
  10 ms guard, but that is parked; the shipped path is the buffered stream.

---

## 6. The knobs — frozen production calibration

Luka, 2026-09-21: *current calibration is OK — do not tune.* (HSSR paper §5.)

| Parameter | Value | Env flag |
|---|---|---|
| Render path | **S16** | — |
| Rear / surround gain | **0.40** (ceiling 0.50) | `AUDIO_STEREO_SURROUND=0.40` |
| Front bass shelf | **−3 dB** @200 Hz (front L/R) | `AUDIO_STEREO_FRONT_BASS_DB=-3` |
| LFE assist | **0 dB re 4096**, assist mode | `AUDIO_STEREO_LFE_GAIN=0`, `AUDIO_STEREO_LFE=assist` |
| Front retention | **0** (frozen-OK) | `AUDIO_STEREO_FRONT_RETENTION` |
| Retention HF taper | **1000:2500 Hz** | `AUDIO_HSSR_RETENTION_HF=LO:HI` |
| Rear delay | **12 ms** (576 frames) | baked in |
| Models | q6d `96abe57d…` + guard V11 `ab01463e…` | `AUDIO_HSSR_GUARD_MODEL` |
| Object slots | 4 (2/side), 250 ms hold | single path |

Parked (not shipped) "scene-bed distribution": retention 0.5 + `AUDIO_HSSR_BED_DELAYS=on`
+ `AUDIO_HSSR_BED_ATTACK=owner` + `AUDIO_HSSR_ATTACK_COLLAPSE=0.75`.

---

## 7. What ships, and where the files live

Deployed 2026-09-22 (commit `b021ec9`), each verified by read-back **[car]**:

```
 /mnt/app/root/hooks/         libcarplay_hook.so   (upmix + all interposers)
                              libcarplay_apac.so   (APAC decoder, dlopen'd)
                              hssr_model.bin  (q6d)   hssr_guard.bin  (V11)
                              maneuver_render  altscreen_render  flag_atlas.rgba
                              carplay_{startup,monitor,processes,cleanup}.sh
 /mnt/app/eso/hmi/lsd/jars/   carplay_hook.jar     (needs a unit restart to load)
 /mnt/app/eso/bin/apps/       smartphone_integrator   (firmware, whole-file)
 /mnt/app/armle/lib/dll/      deva-ctrl-qc.so         (the 6-voice mirror driver)
```

Source homes for a manual build (models in `models/`, firmware in `deploy/`) are in
`docs/RELEASE_ARTIFACTS.md`. The `.so`/renderers come from `build/unprotected/` and
`build/` respectively; the jar from `build/`.

---

## 8. The honest gaps

- **No real on-car realtime distribution** — the type-100 upmix numbers are
  instruction-count estimates; the stop-line `ms_hist` is uncaptured. **[open]**
- **No Krait full-chain loaded trace** — the release-acceptance TARGET gate is
  unverified. **[open]**
- **No blind listening / cabin acoustic evidence** — all tuning is on a conditional
  *electrical* model, never SPL. **[open]**
- **Phone-side spatial (Atmos)** — blocked phone-side; lever exhausted. **[car]**
- **Amp DSP model** — static RE, not bit-exact, bus→speaker binding inferred. **[model]**
- **Known misses** — processed vocal intros read as keys, wide synth-bass read as
  keys (semantic, not frequency). **[car]**

---

## 9. Where to go deeper

| Question | Read |
|---|---|
| How the model works, every iteration, calibration, realtime | [`HSSR_UPMIX_MODEL.md`](HSSR_UPMIX_MODEL.md) |
| How the amp was reversed, why 6ch was hard, the Virtual Cabin | [`VIRTUAL_CABIN.md`](VIRTUAL_CABIN.md) |
| What Atmos is on CarPlay, why it's blocked, what we borrowed | [`DOLBY_ATMOS_TEARDOWN.md`](DOLBY_ATMOS_TEARDOWN.md) |
| Traps not to re-hit | [`LANDMINES.md`](LANDMINES.md) |
| Assemble/deploy a release by hand | `docs/RELEASE_ARTIFACTS.md` |
| The runtime code | `hook/audio/{hssr_dsp,hssr_neon,hssr_control_worker,stereo_upmix}.{c,h}` |
