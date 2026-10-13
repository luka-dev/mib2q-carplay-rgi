# Stereo to 5.1 Upmix

*The end-to-end pipeline from the phone's audio to the six PCM channels the amp renders — where discrete 5.1 and the scene renderer each kick in, and the configuration the startup script ships. State of 2026-10-09: the SCENE backend is the only stereo renderer; the Universal backend (q6d / V11 / v21a) is retired.*

---

## 🧭 At a glance: what each channel gets

For stereo music (type-103 decoded stereo and type-100 realtime media). Spatial Audio / Atmos tracks arrive as real 5.1 and skip this; calls and Siri stay plain stereo. Values are the shipped configuration below; nothing has been listened to in the car with this renderer yet (to verify in the car).

```mermaid
flowchart TB
    accTitle: Scene upmix at a glance
    accDescr: Stereo input enters the scene engine, which analyses it in 1024-sample frames, moves off-centre instruments and the room residual to the rear and keeps the rest in front; the fine mask net steers which instruments move; PCM6 then reaches the B&O amplifier and its 19 speakers.
    stereo["🎵 Stereo input<br/>48 kHz, 1024/256 STFT"]

    subgraph scene ["Scene renderer in the hook"]
        front["Front L/R<br/>the input minus the transferred objects<br/>(energy-conserving, no inverted copy)"]
        engine["Scene engine<br/>per-bin decomposition, stage clusters,<br/>exact unmix, side layer + air"]
        net["Mask net c on its worker thread<br/>eligibility / protect per bin<br/>every 512 frames"]
        rear["Rear Ls/Rs<br/>transferred places + decorrelated side layer<br/>rear delay 0 (variant J)"]
        lfe["LFE V2 assist<br/>60 Hz LR4 from the front,<br/>steered by the net's bass/drums"]
        centre["C = 0<br/>amp derives centre from L+R"]
        out["Output stage<br/>int16, SoundCheck level,<br/>cabin guard (predicts amp bus peaks)"]
    end

    pcm["PCM 6ch<br/>L R C LFE Ls Rs"]
    amp["B&O J525 via MOST150<br/>Virtual Cabin: centre, crossovers,<br/>sub management"]
    spk["🔊 19 speakers"]

    stereo --> engine
    stereo --> net
    net --> engine
    engine --> front
    engine --> rear
    front --> lfe
    front --> out
    rear --> out
    lfe --> out
    centre --> out
    out --> pcm --> amp --> spk

    classDef primary fill:#dbeafe,stroke:#2563eb,stroke-width:2px,color:#1e3a5f
    classDef success fill:#dcfce7,stroke:#16a34a,stroke-width:2px,color:#14532d
    classDef neutral fill:#f3f4f6,stroke:#6b7280,stroke-width:2px,color:#1f2937
    class stereo,spk primary
    class engine,net,rear,front success
    class lfe,centre,out,pcm,amp neutral
```

| Channel / stage | What it gets |
|---|---|
| **Front L/R** | The input itself, 1024 frames late, minus the share of each admitted off-centre place that moved behind; the subtraction is at the place's own pan, so nothing inverted appears in the other front channel (`hook/audio/scene_engine.c`). |
| **Rear Ls/Rs** | The transferred places at their own image plus the residual room (the side layer, decorrelated between Ls and Rs by a static per-bin phase table, low-passed at 7 kHz) and the air copy above 2–2.5 kHz. No delay of our own (`AUDIO_SCENE_REAR_DELAY_MS=0`); the amp's rear bus adds ~11.8 ms by itself `[model]`. |
| **Mask net c** | Four posteriors per bin (vocals, drum core, bass, other) every 512 frames; eligibility = other, protect = vocals + drums + bass. It gates which cells may move as instruments and ducks the spatial transfer and side layer where protected content sits (`hook/audio/scene_mask.c`, `models/scene-masknet-c/manifest.json`). |
| **LFE** | Cabin-Aware LFE V2: a 60 Hz LR4 sub-band of the front, polarity-safe mono fold, level steered by the net's bass and drum posteriors, paired front headroom 31392/32768 (`hook/audio/stereo_upmix.c:25-34`, `:190-281`). |
| **C** | 0 on purpose: the amp derives the centre from L+R. |
| **Output stage** | int16 conversion with the existing clamp, SoundCheck level, and the S16 cabin guard, which predicts the amp's bus peaks and scales if needed (`stereo_upmix.c:284-344`). |
| **Amp** | PCM 6ch over MOST150; the B&O Virtual Cabin handles centre, crossovers and sub management across 19 speakers ([Virtual Cabin and Amp RE](VIRTUAL_CABIN_AND_AMP_RE.md)). |

## 🎯 The big picture

Two things had to be built: get the **right six PCM channels**, and **synthesize them** for stereo content. The amp (J525 B&O) is a complete 5.1 renderer — the [Virtual Cabin](VIRTUAL_CABIN_AND_AMP_RE.md) — so the head unit only feeds it `L R C LFE Ls Rs` and lets it do the acoustics: it derives the centre, manages the sub, crosses over every driver, and fans out to 19 speakers.

The synthesis is the scene renderer of [Stereo-to-Rear Scene Algorithm](UPMIX_SCENE_ALGORITHM_2026-10-08.md) (sections 14–18 are the implementation record), a structural decomposition with one small net for the semantic stage; its numpy reference is `tools/stereo-upmix-render/scene_upmix.py` and the C port in `hook/audio/scene_*.c` matches it to −131 dB on the fixtures (`tools/stereo-upmix-render/port/test_engine.py`). The amp RE is in [Virtual Cabin and Amp RE](VIRTUAL_CABIN_AND_AMP_RE.md) and [Dolby Atmos](DOLBY_ATMOS.md); traps are in [Landmines](../LANDMINES.md).

```mermaid
flowchart TB
  accTitle: Master stereo to 5.1 pipeline
  accDescr: Two transports feed decode and classification, split into a discrete 5.1 path and the scene upmix path, pass the cabin guard into PCM6 and travel down to the amp and speakers.
  subgraph ingest["Ingest"]
    iphone["iPhone CarPlay over NCM"]
    realtime_100["type-100 realtime PCM stereo"]
    buffered_103["type-103 buffered AAC or APAC"]
  end
  subgraph render["Hook render"]
    cookie_decode["decode by inline cookie"]
    classify_pcm["classify codec and channels"]
    discrete_reorder["APAC 5.1 reorder"]
    scene_upmix["scene stereo upmix"]
    cabin_guard["cabin guard S16 peak"]
    pcm6_out["PCM6<br/>upmix C zero; discrete C preserved"]
  end
  subgraph downstream["Head unit and amp"]
    deva_csd["deva asad 0xfc to CSD"]
    most_amp["LPASS TDM to MOST150"]
    j525_dsp["V850 and SHARC chain"]
    speakers_19["19 cabin speakers"]
  end
  iphone --> realtime_100
  iphone --> buffered_103 --> cookie_decode --> classify_pcm
  realtime_100 -->|media only| scene_upmix
  classify_pcm -->|5.1| discrete_reorder --> cabin_guard
  classify_pcm -->|stereo| scene_upmix --> cabin_guard
  cabin_guard --> pcm6_out --> deva_csd --> most_amp --> j525_dsp --> speakers_19
  classDef primary fill:#dbeafe,stroke:#2563eb,stroke-width:2px,color:#1e3a5f
  classDef success fill:#dcfce7,stroke:#16a34a,stroke-width:2px,color:#14532d
  classDef neutral fill:#f3f4f6,stroke:#6b7280,stroke-width:2px,color:#1f2937
  class iphone,speakers_19 primary
  class discrete_reorder,scene_upmix success
  class j525_dsp neutral
```

## ⚙️ Ingest — two CarPlay audio streams

There are two CarPlay audio streams (`docs/reference/IOS_CARPLAY_SPATIAL_WIREFORMAT.md §21`), and which one a track uses decides everything downstream.

| Stream | Advertised as | Codec | On the car |
|---|---|---|---|
| type-100 realtime | `audioOutputFormats` | PCM only | 48k/16/stereo `[car]` |
| type-103 buffered | `audioOutputFormatsExtended` | AAC-LC (23) / APAC 5.1 (65) / 5.1.2 (66) | lossless + spatial; our hook supplies it |

The stock realtime stream never carries multichannel, and phone-side spatial over it is blocked by a phone feature flag (`ScreenSpatialAudioReceiver`) — see [Dolby Atmos](DOLBY_ATMOS.md). So all **multichannel** input rides the buffered type-103 path. On that path the frame is APAP (BE32 + ChaCha20-Poly1305); the **inline magic cookie** decides the decoder, not the APAP index hint — see [Discrete 5.1 Routing](DISCRETE_5_1_ROUTING.md).

The shipped startup advertises `[65,23]` on type 103: primary APAC 5.1 because `AUDIO_SPATIAL_ATMOS=0` (`deploy/smartphone_integrator/carplay_startup.sh:71`), plus AAC-LC stereo via `SPATIAL_103_EXTRA_INDEX=23` (`carplay_startup.sh:109`).

Stereo on the realtime stream is upmixed too. With `AUDIO_REALTIME_UPMIX=on` (`carplay_startup.sh:103`) the same scene renderer runs on type-100 media PCM — Spotify and every app without type 103 — with its own engine state and its own mask worker (`hook/audio/audio_hook.c:3860-3880`). It falls back to plain L/R while a phone, Siri or alert stream is tracked or while type 103 owns the rail (`hook/audio/media_route.h:158`).

## 🔧 Where upmix vs discrete kicks in

After decode, the PCM is classified by codec, channel count and layout (`hook/audio/spatial_downmix.h`):

```mermaid
flowchart TB
  accTitle: Decode and classification branch
  accDescr: Decoded PCM is classified; APAC 5.1 is reordered onto the discrete path while stereo goes to the scene upmix.
  apap_frame["APAP frame and cookie"]
  aac_decode["AAC-LC 23 via Cinemo"]
  apac_decode["APAC 65 via libcarplay_apac"]
  classify_pcm["classify codec and layout"]
  reorder_51["reorder to Apple order"]
  stereo_upmix["stereo to the scene upmix"]
  discrete_path["discrete path bypasses upmix"]
  apap_frame --> aac_decode --> classify_pcm
  apap_frame --> apac_decode --> classify_pcm
  classify_pcm -->|5.1| reorder_51 --> discrete_path
  classify_pcm -->|stereo| stereo_upmix
  classDef primary fill:#dbeafe,stroke:#2563eb,stroke-width:2px,color:#1e3a5f
  classDef success fill:#dcfce7,stroke:#16a34a,stroke-width:2px,color:#14532d
  class reorder_51,discrete_path success
  class stereo_upmix primary
```

iOS may alternate AAC-LC ↔ APAC inside one session; the cookie authority makes the switch seamless. iOS never sends ordinary stereo as APAC 5.1, so normal music decodes as stereo and is upmixed on our side. The 5.1 case is `[car]` end to end.

Decoded 5.1.2 (index 66) is accepted only when SETUP itself negotiated 66, which the shipped config never advertises; otherwise the decoder is rejected as `unsupported-layout` because the native PCM8 order is unmeasured.

## 📊 The scene renderer

The renderer is **structural, not a classifier**: per STFT cell it decides from the two-channel statistics what is a coherent off-centre source and what is room, and moves whole places behind the listener; the only learned part is the mask net, which says which cells may move as instruments and which must stay protected. Every stage lives in `hook/audio/scene_engine.c` (frame and block driver), `scene_engine_math.c` (helpers, tables, the 1024-point real FFT), `scene_clusters.c` (the stage clusters), `scene_mask.c` (the net) and `scene_mask_worker.c` (its thread); `hook/audio/stereo_upmix.c` wraps them with the OFF bypass, the int16 conversion, the LFE V2 assist and the cabin guard. Spec sections 14 and 18 of [the algorithm note](UPMIX_SCENE_ALGORITHM_2026-10-08.md) record every design change the implementation forced and the port's numbers.

One frame: a sine-windowed **1024-sample STFT at hop 256** (ERB-pooled groups for the pooled statistics); per bin the 2×2 covariance of L and R, tracked with three time constants (8 ms for the fast pan gate, 43 ms for the Wiener scalar and the clustering, 300 ms for the direction of the residual), is decomposed into a coherent primary with a pan and an orthogonal residual; the centre-removed pan of the primary feeds the **stage clusters** (at most three admitted places with dwell, 2 s release, shadow rejection, merges and a song memory); for every admitted place each bin is unmixed exactly against the centre (`[L; R] = [a; b]·s + [1; 1]/√2·m`, so a centred source stays in front sample-exactly) and the fraction `alpha` of the place is transferred to the rear at its own pan and subtracted from the front at the same pan; the residual becomes the side layer, decorrelated between Ls and Rs by a static per-bin phase table and low-passed at 7 kHz, with the air copy ramping in at 2.0–2.5 kHz; the **mask net c** posteriors (one row per 512 input frames) set the instrument transfer (`inst_alpha`, `inst_gamma`) and duck the spatial transfer and the side layer by the protect mask (`duck_depth`); the engine writes `L R 0 0 Ls Rs`; `stereo_upmix.c` then converts to int16, adds the LFE V2 assist from the front and runs the cabin guard.

```mermaid
flowchart TB
  accTitle: Scene renderer signal flow
  accDescr: The 1024-by-256 STFT feeds a per-bin two-by-two decomposition, the stage clusters with song memory, the exact unmix and transfer, the side layer with air and the mask net semantic stage, producing six channels that pass LFE V2 and the cabin guard.
  stereo_in["stereo S16, any call size up to 1024"]
  stft["STFT 1024 / 256, sine window"]
  decompose["per-bin 2x2 eigen decomposition<br/>primary with pan + residual"]
  clusters["stage clusters with song memory<br/>at most 3 places, dwell and release"]
  unmix["exact unmix against the centre<br/>transfer alpha of each place"]
  side["side layer + air<br/>static phase table, 7 kHz low-pass"]
  mask_net["mask net c every 512 frames<br/>eligibility / protect, duck"]
  six_out["L R 0 LFE Ls Rs"]
  lfe_v2["LFE V2 assist from the front"]
  guard["S16 cabin guard"]
  stereo_in --> stft --> decompose --> clusters --> unmix --> six_out
  decompose --> side --> six_out
  stereo_in --> mask_net --> unmix
  mask_net --> side
  six_out --> lfe_v2 --> guard
  classDef primary fill:#dbeafe,stroke:#2563eb,stroke-width:2px,color:#1e3a5f
  classDef success fill:#dcfce7,stroke:#16a34a,stroke-width:2px,color:#14532d
  classDef neutral fill:#f3f4f6,stroke:#6b7280,stroke-width:2px,color:#1f2937
  class stereo_in,guard primary
  class clusters,unmix,side success
  class mask_net neutral
```

### Knobs

Read once per `dio_manager` start (a change applies at the next HU boot of the CarPlay child); every knob is clamped to its range with an `audio config: invalid …` line and the default on a bad value (`hook/audio/audio_hook.c:134-169`, `hook/audio/audio_hook.h:59-96`). The scene values in the startup block are the engine defaults of variant J, spelled out so the running configuration can be read from the file.

| Knob | Shipped (`carplay_startup.sh`) | Default | Range | What it is |
|---|---|---|---|---|
| `AUDIO_STEREO_UPMIX` | `on` (`:73`) | off | on, off | on = the scene renderer; off = the exact `L,R,0,0,0,0` bypass, which always wins |
| `AUDIO_REALTIME_UPMIX` | `on` (`:103`) | off | on, off | the same renderer on the stock type-100 media stream, own state and own mask worker |
| `AUDIO_STEREO_LFE` | `assist` (`:74`) | off | off, assist | Cabin-Aware LFE V2 |
| `AUDIO_STEREO_LFE_GAIN` | `0` (`:76`) | 0 | −24..+12 dB re 4096 | assist level; not the whole subwoofer feed (the corrected BANK1 also feeds the sub from L/R/C/Ls/Rs) |
| `AUDIO_SCENE_MASK_BLOB` | `$H/scene_masknet_c.bin` (`:92`) | unset | path | the fine mask net c weights (`SMSKNET1`, 32,544 B); unset, unreadable or rejected = spatial only, logged once |
| `AUDIO_SCENE_MASK_SYNC` | not exported | 0 | 0, 1 | 1 = the net on the audio thread (the port tests' exact timing); 0 = the scene mask worker |
| `AUDIO_SCENE_ALPHA` | `0.7` (`:97`) | 0.7 | 0..1 | spatial relocation fraction of an admitted place |
| `AUDIO_SCENE_INST_ALPHA` | `0.9` (`:95`) | 0.9 | 0..1 | instrument transfer fraction (needs the blob) |
| `AUDIO_SCENE_INST_GAMMA` | `1` (`:94`) | 1 | 0.1..4 | eligibility exponent; 1 = full rear (kit letters I/J), 2 = the balanced fallback H |
| `AUDIO_SCENE_DUCK_DEPTH` | `0.25` (`:96`) | 0.25 | 0..1 | duck of the spatial transfer and the side layer by the protect mask (needs the blob) |
| `AUDIO_SCENE_REAR_DELAY_MS` | `0` (`:93`) | 0 | −50..50 | 0 = variant J: our rear and the amp's own front bleed meet the rear doors in phase |
| `AUDIO_SCENE_SIGMA_MIN` | not exported | 0.08 | 0.01..1 | place width floor; 0.15 is the documented second candidate (spec §15) |
| `AUDIO_SCENE_SIDE_LP_HZ` | `7000` (`:98`) | 7000 | 0 = off, 1000..24000 | side-layer low-pass |
| `AUDIO_SCENE_BLEED_CANCEL` | `0` (`:99`) | 0 | 0..0.5 | variant K's cancel of the amp's −3 dB front copy in the rear bus (0.355); 0 = J |

What the renderer no longer has: `AUDIO_STEREO_SURROUND`, `FRONT_BASS_DB`, `FRONT_TREBLE_DB`, `REAR_BODY_DB`, `REAR_AIR_DB`, `FRONT_RETENTION` and every `AUDIO_HSSR_*` knob left with the Universal backend; the fronts and rears leave at the engine's own levels (unity scale, as the car kit renders) and only the LFE assist and the cabin guard apply on top (`hook/audio/stereo_upmix.h:72-83`).

### MMI level

**Sound → Sound effects → UpMix** is a row added to the stock screen (`java_patch/com/luka/carplay/tone/UpmixToneUi.java`, wired by the replaced tone `Activator`; any failure there falls back to the stock bank and screen, so the worst case is no row). It takes its texts from the tone string table, where the installer puts them at the fixed slots 9900..9905, far past the stock ~900 (`deploy/lang/upmix_lang.py` with our `ldzip.py`, run by M.I.B.'s micropython; English, Spanish, French; a slot holding someone else's text is never overwritten), and falls back to the same texts built into the jar; it sends `CMD_UPMIX` to `hook/audio/upmix_level.c`, which saves the level in `/mnt/persist/mhi2q-carplay/upmix_level` and answers with the sticky `EVT_UPMIX_LEVEL`, so the row shows what the hook applies. Both scene instances take the level on their own thread at init and on every period (`spa_stereo_upmix_set_level`). **[code]**

| Level | `alpha` | `inst_alpha` | `duck_depth` | `side_lp_hz` |
|---|---|---|---|---|
| Off | bypass: `L, R, 0, 0, 0, 0`, still 1024 frames late | | | |
| Moderate | 0.5 | 0.7 | 0.35 | 7000 |
| Normal (default) | the `AUDIO_SCENE_*` knobs above | | | |
| Full | 0.9 | 1.0 | 0.15 | 11000 |
| Aggressive | 1.0 | 1.0 | 0.10 | 12000 |

A change fades over 300 ms with an ease in/out curve between the scene and the plain stereo, and the engine keeps running under Off so it comes back warm; the tuning itself switches at the next period. The rear delay never changes with the level. Full was chosen from a 2026-10-13 simulation through the hook renderer and the J525 electronic model (`tools/soundamp-virtual-cabin/amp_dsp_chain.py`): in the rear doors our rear rises above the amp's own front copy by +1.8 dB above 7 kHz (4.7 → 6.5 dB) with the vocal still about 9 dB under the front. Not yet heard in the car. **[sim]**

### The mask worker and its timing

The net costs 1.37 M ARMv7 instructions per 512-frame control on the QEMU bench (`models/scene-masknet-c/manifest.json`), so by default it runs on the **scene mask worker** (`hook/audio/scene_mask_worker.c`, SCHED_RR priority 17, started per SETUP on the type-103 path and kept across re-params on the realtime path; `audio_hook.c:1720-1724`): two job slots of 512 frames, two result slots, generation and sequence numbers, stale results dropped. The audio thread pays only the hand-off (about 1.1 k instructions per 768-frame period on the bench) and installs the newest finished row at every 512-frame control boundary (256, 768, 1280 … pushed frames, `hook/audio/stereo_upmix.h:42-60`), so a row applies **one control (10.7 ms) later** than on the audio thread, one more per control the net needs beyond its period — the engine's slow instrument attack absorbs that. A control that arrives while both slots are full is refused, never waited for: the engine keeps its last row and the net's 2048-sample history gets a 512-frame splice. The count is `skipped=N` in the stop line below; 0 is the only healthy value. If the worker thread cannot start, the net runs on the audio thread (`AUDIO_SCENE_MASK_SYNC=1` chooses that on purpose).

### Latency

The engine emits a sample only once its 1024-sample analysis frame is complete, so `spa_stereo_upmix_process_s16` primes its output FIFO with 1024 zero frames and every call then emits exactly the frames it was given: output frame *i* is input frame *i − 1024* rendered, **21.3 ms** on both paths (`stereo_upmix.h:31-38`). The retired Universal renderer lagged 512 frames and never reported them (the type-103 SETUP response carried `arrivalToRenderLatencyMs: 0` since the first build, and the car is tuned to that), so OFF still reports 0 and **SCENE reports only the extra 512 frames, 11 ms** (`audio_hook.c:8184-8200`). What the phone does with the field is to verify in the car.

### Track-change reset

A new song must not inherit the previous song's places and song memory. On the type-103 path the APAP **transition id** of the stream is sample-exact: a changed id calls `spa_stereo_upmix_scene_track_change` (`audio_hook.c:2798-2805`). On the realtime path the hook reads the iAP2 **NowPlayingUpdate** (0x5001) persistent identifier on Cinemo's receive thread and sets an atomic flag that stock's audio thread consumes at its next period (`audio_hook.c:9056-9087`, `:3954-3955`). Both reset the engine only — every place fades out, the histogram and the song memory are cleared, the tables are kept because the reset runs inside the realtime callback — while the net's stream carries on (`stereo_upmix.c:515-526`, `scene_clusters.h:24`).

### Fail-closed contract

The upmixer never silently picks another renderer `[code]`:

| Condition | Output |
|---|---|
| `AUDIO_STEREO_UPMIX=off` | exactly `L,R,0,0,0,0`; every other audio knob ignored |
| blob unset, unreadable or rejected | spatial only (no instrument transfer, no duck), one line `SCENE: mask blob unreadable … running without a mask` or `… rejected … (expected 32544 bytes of SMSKNET1) …` (`audio_hook.c:1677-1693`) |
| upmix init fails on the type-103 path | `buffer103: stereo upmix init failed status=…; exact L/R passthrough retained` (`audio_hook.c:1983`) |
| process error at render time | exact L/R widening, one ERROR line (`audio_hook.c:3275`) |
| worker thread cannot start | the net on the audio thread, `audio upmix 103|rt: scene mask worker failed to start; net on the audio thread` (`audio_hook.c:1723`) |
| invalid knob value | `audio config: invalid …`, the default used |

Every reset (prefix FLUSH, pause/rate teardown, cookie or codec change, SETUP replacement, session stop) advances a process-wide generation; the worker drops rows of an older generation (`stereo_upmix.c:528-560`). Env values are read once per `dio_manager` process — a change applies at the next start of the CarPlay child, a replaced `.so` too.

## ⚙️ SoundCheck and the output order

Per buffered frame the order is fixed `[code]` (`audio_hook.c:3290-3310`):

```text
decoded S16 → reorder (APAC 5.1) or scene upmix incl. LFE V2 and the S16 cabin guard → SoundCheck gain (S16) → optional SoundCheck limiter → PCM ring
```

SoundCheck is a **programme** gain applied after the upmix on purpose: the renderer's statistics and the net's log-power features see the decoded level, not a −9 dB AAC gain (`audio_hook.c:3302-3306`). SETUP carries `soundCheckMediaKind {"1":-16}`; APAP type 103 carries MainLoudnessParameters (TLV 11) and media kind (TLV 12). The gain is `target − p0`, positive gain capped at `−1 − p2`, rounded Pioneer-style `floor(x+0.5)`, bounded −128..+20 dB (`soundcheck.c:80-92`); first metadata applies immediately, later changes ramp over 20 ms (`soundcheck.c:7`), FLUSH resets history without forcing unity. Observed tuples: AAC `[-6.5,-1,0.96875]` → −9 dB, the retained APAC tuples → +1 dB. The RMS fallback and sample-peak limiter are separate opt-ins, both off in the shipped config. `-16` is the captured protocol value, not a published Apple rule.

> 📌 **Open `[code]`:** the S16 cabin guard runs inside the upmix, *before* SoundCheck, and the SoundCheck limiter is off. A positive SoundCheck gain (APAC +1 dB) is therefore applied after the bus bound was checked. A 2026-09-08 diagnostic proved that any limiter placed after S16 clipping cannot restore channel ratios (`[30000,31000]` at +1 dB → `32767,32767`). No clipped song has been captured.

## ⚡ The realtime path in the car

The stock type-100 media stream (Spotify, every app without buffered 103) is upmixed inside stock's own QSA thread: period 768 frames = 16 ms (`audio_hook.c:1472`), own engine state and own mask worker, allowed only for realtime periods, 2→6, a tracked media stream and no phone/Siri/alert owner (`media_route.h:158`). The deadline guard (`hook/audio/rt_upmix_guard.h:27-41`): 12 ms budget per period, 4 consecutive overruns disable the upmix (rears silent, fronts untouched), 64 cold periods (~1 s) are excused after every arm or break, revival only after a gap of ≥ 500 ms in the stream.

```mermaid
stateDiagram-v2
  accTitle: Realtime upmix guard lifecycle
  accDescr: A realtime sink arms the guard with sixty-four excused cold periods, judges each period against the 12 ms budget, disables after four consecutive overruns and revives only after a break of at least 500 ms in the stream.
  [*] --> cold_start: params on a 2 to 6 sink
  cold_start --> judged: 64 periods excused
  judged --> judged: period within 12 ms
  judged --> disabled: 4 consecutive over 12 ms
  disabled --> cold_start: gap of 500 ms or more
  judged --> cold_start: gap or DSP reset
  disabled --> [*]: handle closed
  judged --> [*]: handle closed
```

**Cost on the QEMU ARMv7 bench** (`tools/stereo-upmix-render/bench/`, [Scene Backend Deploy](../build-and-deploy/SCENE_BACKEND_DEPLOY.md)) `[model]`: the engine 0.31–0.34 M instructions per 256-frame hop (0.98 M per 768-frame period at the shipped flags, spatial only 0.31 M per hop), the net 1.37 M per 512-frame control on the worker, the hand-off 1.1 k per period. The only instructions-to-Krait calibration so far (the Universal runs) scales this to **1.1–1.8 ms of the 16 ms period on the audio thread** against the 12 ms guard, 1.6–2.5 ms per control on the worker, 3.5–5.6 ms per period with `AUDIO_SCENE_MASK_SYNC=1`. The guard histogram in the `stop` line is the only real number; none has been collected with this renderer yet. The Universal-era car sessions (three of five disabled themselves on the 8 ms build) and its cost work are in [Upmix History and Results](UPMIX_HISTORY_AND_RESULTS_2026-10-09.md).

Deliberately **not** done: triggering on real `deviceUnderruns` (it reaches into the QSA write interposer that once crashed dio_manager — see [Landmines](../LANDMINES.md)), and thread CPU time instead of wall time (`CLOCK_THREAD_CPUTIME_ID` exists on QNX 6.5).

## ✅ What ships

The on-unit file set, staged by `scripts/prepare_install.sh` ([Deploy and Release](../build-and-deploy/DEPLOY_AND_RELEASE.md)):

| Path | File |
|---|---|
| Upmix + all interposers | `/mnt/app/root/hooks/libcarplay_hook.so` (1,441,797 B unprotected; 2.1 MB with Universal) |
| APAC decoder (dlopen'd) | `/mnt/app/root/hooks/libcarplay_apac.so` |
| Model | `/mnt/app/root/hooks/scene_masknet_c.bin` (32,544 B, sha256 `58cdc72f…`, from `models/scene-masknet-c/`); `hssr_model.bin`, `hssr_guard.bin` and `hssr_mask.bin` are no longer shipped, the uninstaller still removes them from older installs |
| Startup | `/mnt/app/root/hooks/carplay_startup.sh`, the scene block live at lines 77–99 |
| Java (needs a unit restart) | `/mnt/app/eso/hmi/lsd/jars/carplay_hook.jar` |
| Firmware | `/mnt/app/eso/bin/apps/smartphone_integrator` |
| 6-voice mirror driver | `/mnt/app/armle/lib/dll/deva-ctrl-qc.so` |

The current package is `build/install_scene_j_unprotected.zip` (2026-10-09, sha256 `74fbad0a…`; hook `33b6ee9b…`, APAC `1c25799f…` unchanged since 2026-09-22). It has not been installed on the unit yet (to verify in the car).

**How to confirm the running config** from `/tmp/carplay_hook.log` at the next phone connect (the log is WARN-only by default, `touch /mnt/app/carplay_verbose` lifts it to INFO):

| Line | Level | Proves | Code |
|---|---|---|---|
| `SCENE: mask net ready path=/mnt/app/root/hooks/scene_masknet_c.bin bytes=32544` | WARN | the blob passed size, magic and dimensions | `audio_hook.c:1693` |
| `buffer103: render … stereoUpmix=scene frontGainQ15=31392 sceneMask=worker stereoLfe=assist lfeGainQ15=4096 …` | INFO | the buffered path's configuration (`sceneMask` = `worker`, `sync`, `none` for no blob, `off` for the bypass) | `audio_hook.c:1945-1960` |
| `buffer103: response committed {type:103,…,arrivalToRenderLatencyMs:11,…}` | WARN | the phone was told the renderer's extra 512 frames | `audio_hook.c:8391` |
| `audio realtime upmix: armed (backend=scene init=0 period=768 frames, scene_mask=worker)` | WARN | the type-100 path armed | `audio_hook.c:3875` |
| `audio upmix 103\|rt: scene mask worker stopped rows=A/B skipped=N stale=… superseded=… resets=…` | WARN | at stream stop: B controls handed to the worker, A finished, N refused because both slots were full | `audio_hook.c:1739` |
| `audio realtime upmix: stop periods=… mean_us=… worst_us=… cold_us=… disables=… revivals=… ms_hist=…` | WARN | the real period-time distribution (9 buckets of 1 ms, last = ≥ 8 ms) | `audio_hook.c:3893` |
| `audio realtime upmix: DISABLED …` / `back after a … ms break` | WARN | guard tripped / revived | `audio_hook.c:3970`, `:3942` |

Read `skipped` and `disables` first. If `disables > 0`, check whether the overruns fall in loud passages (our cost) or are scattered (scheduler preemption).

Whether any of this runs depends on `AUDIO_SPATIAL`. Only B&O external-amp cars have a 6-channel route, so the intended policy is `AUDIO_SPATIAL=0` unless E2P 0xFC reads `02` — but that gate is assigned to the installer and no installer script implements it yet; the startup script hardcodes `AUDIO_SPATIAL=1` for this unit (`carplay_startup.sh:66-70`). See [Landmines](../LANDMINES.md).

## ⚠️ The honest gaps

- **One sighted listener, no blind test** — the scene renderer was accepted offline (51 reference checks, the fixture gates, the car kit on headphones), then driven on 2026-10-09 (realtime 19 min: mean 2.0 ms / worst 6.0 ms per 16 ms period, guard never tripped; worker `skipped=1` of 106,490 rows; type 103 `sceneMask=worker`, `drop=0`) and accepted by the owner on 2026-10-10: "sounds good, really good" `[car]`. No level-matched A/B against `AUDIO_STEREO_UPMIX=off`, the blinded car kit was not played, and the resume fix `a00959cc` is still to be heard ([Scene Backend Deploy](../build-and-deploy/SCENE_BACKEND_DEPLOY.md)). `[open]`
- **+11 ms reported latency** — the phone sees a changed `arrivalToRenderLatencyMs` for the first time; its effect on A/V alignment is unknown. `[open]`
- **SoundCheck after the cabin guard** — a positive programme gain is applied after the bus bound; limiter off (see above). `[open]`
- **Amp timing is static RE** — the ~11.8 ms rear-bus lag that variant J relies on and the bleed coefficients behind `AUDIO_SCENE_BLEED_CANCEL` were never measured in the cabin; the one-tone probe of [Upmix Radical Directions](UPMIX_RADICAL_DIRECTIONS_2026-10-07.md) decides J against K. `[model]`
- **J525 state unreadable** — SSH to the head unit is not a J525 memory transport; `/dev/io-amp` on RCC is the internal TI DSP. Active bank/B/EQ selector are hypotheses in every model number. `[open]`
- **No blind listening / cabin acoustic evidence** — all tuning is on a conditional electrical model and headphones, never SPL. `[open]`
- **Known limits of the method** — mixes with no coherent off-centre place (most modern pop) get only the side layer and the net's instrument stage; two coherent sources at one frequency cannot be separated by any two-channel statistic (spec §12, §14). `[model]`

Runtime code: `hook/audio/{scene_engine,scene_engine_math,scene_clusters,scene_mask,scene_mask_worker,stereo_upmix,soundcheck}.{c,h}`, `hook/audio/scene_fft.h`, `hook/audio/scene_simd.h`, `hook/audio/rt_upmix_guard.h`, `hook/audio/media_route.h`.

## 📜 History

Until 2026-10-09 the stereo renderer was the HSSR **Universal** backend: the q6d routing GRU, the V11 guard and the v21a per-cell mask net on a 512/128 STFT, with the surround / front-bass / retention / bed / vocal-gate / mask-drive knobs and the calibration of [Upmix Calibration and Tuning](UPMIX_CALIBRATION_AND_TUNING.md). It was retired with the SCENE backend's first package; the inventory and the plan are in [Upmix Code Retirement](UPMIX_CODE_RETIREMENT_2026-10-09.md), every iteration with its numbers in [Upmix History and Results](UPMIX_HISTORY_AND_RESULTS_2026-10-09.md), and the last tree that has the code, the models and the Universal tooling is git tag `upmix-universal-last-2026-10-09` (commit `1501fc7d`). [HSSR Model](HSSR_MODEL.md) describes that renderer.

## 🔗 Related notes

- [Stereo-to-Rear Scene Algorithm](UPMIX_SCENE_ALGORITHM_2026-10-08.md) — the algorithm, its offline results, the DEV40 comparison, the semantic stage, the port record (§14–§18).
- [Scene Backend Deploy](../build-and-deploy/SCENE_BACKEND_DEPLOY.md) — the package, the measured cost, the A/B levers, the rollback, what to read in the logs.
- [Upmix History and Results](UPMIX_HISTORY_AND_RESULTS_2026-10-09.md) · [Upmix Code Retirement](UPMIX_CODE_RETIREMENT_2026-10-09.md) — what came before and why it left.
- [Upmix Rework Baseline](UPMIX_REWORK_BASELINE_2026-10-07.md) · [Upmix Rework Proposal](UPMIX_REWORK_PROPOSAL_2026-10-07.md) · [Upmix Radical Directions](UPMIX_RADICAL_DIRECTIONS_2026-10-07.md) — why the rear was faint and unstable, and the redesign that became the scene renderer.
- [Virtual Cabin and Amp RE](VIRTUAL_CABIN_AND_AMP_RE.md) (the amp) · [Dolby Atmos](DOLBY_ATMOS.md) · [Discrete 5.1 Routing](DISCRETE_5_1_ROUTING.md) · [Landmines](../LANDMINES.md).
- [Buffered Audio Path](BUFFERED_AUDIO_PATH.md) · [Call and Siri Routing](CALL_AND_SIRI_ROUTING.md) — the type-103 transport the upmix renders into, and what happens when a call or Siri takes the output.
- [J525 Wiring and Speakers](J525_WIRING_AND_SPEAKERS.md) — the rear doors are mid-bass speakers, which bounds what the rear can carry.
- Long-form: [UPMIX](../research/UPMIX.md), [VIRTUAL_CABIN](../research/VIRTUAL_CABIN.md), [DOLBY_ATMOS_TEARDOWN](../research/DOLBY_ATMOS_TEARDOWN.md).

## 📚 Sources

Merged originals were removed from the tree after this note absorbed them; they remain in git history (commit 23972aa). They describe the Universal renderer and are history since 2026-10-09:

- `UPMIX_STATUS_2026-09-22` — 22.09 deploy hashes, HF taper, −28.8 % cost work, realtime deadline policy and the five recorded sessions.
- `STEREO_TO_5_1_UPMIX_DESIGN` — the Universal contract: output order, OFF bypass, fail-closed model load, generation reset, runtime split.
- `UPMIX_STATIC_READINESS_AND_SOUNDCHECK` — SoundCheck math, order and the reproduced headroom failure (still the SoundCheck section above).
- `UPMIX_GUARD_RETENTION_2026-09-20` — the corrected guard table (still the cabin guard above) and the S16 transfer boundary fixes.
- `UPMIX_AGENT_HANDOFF_2026-09-19` — env knob list, host renderer recipe, the 21.09 "do not tune" decision.
- `UPMIX_DOLBY_DISTRIBUTION_PLAN_2026-09-21` — the scene-bed flags and the HF taper knob.
