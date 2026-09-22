# Discrete 5.1 Routing

*The native 5.1 CarPlay path: how decoded multichannel reaches the amp through the deva mirror driver, the CSD width fix, the SAD header, the 8→6 fold, and the magic-cookie codec authority.*

---

## 🎯 The discrete path in one line

Native decoded APAC 5.1 (from the buffered type-103 path) is reordered byte-identically to `L R C LFE Ls Rs` and **bypasses the upmixer** — it does not touch [[HSSR Model|HSSR]]. Discrete 5.1 CarPlay works end to end in the vehicle **[car]** (2026-09-06). The last two faults were both declaration/control mismatches rather than PCM corruption: the SAD header announced two active channels over six populated slots, and the hook blanked `updateFeedback`, removing the sender's clock reference. Both are corrected and vehicle-confirmed.

Provenance: `docs/AUDIO_5_1_STATE.md`, `docs/research/VIRTUAL_CABIN.md`. See [[Stereo to 5.1 Upmix]] for where discrete vs upmix splits, and [[Virtual Cabin and Amp RE]] for the amp side.

## ⚙️ The path, end to end

```mermaid
sequenceDiagram
  accTitle: Discrete 5.1 delivery path
  accDescr: Decoded APAC 5.1 is reordered and folded, written to deva which mirrors mpl1 caps and declares asad=0xfc, through CSD device 78 and LPASS TDM to the MOST ring and the amp.
  participant DEC as APAC/AAC decoder
  participant HOOK as hook (reorder + fold)
  participant DEVA as deva-ctrl-qc
  participant CSD as CSD dev 78 / LPASS
  participant AMP as J525 amp
  DEC->>HOOK: decoded PCM (cookie = codec authority)
  HOOK->>HOOK: reorder to L R C LFE Ls Rs; 8->6 fold if 5.1.2
  HOOK->>DEVA: PCM6 stream + asad=0xfc / 6ch / 24-bit
  DEVA->>CSD: caps mirror mpl1 -> session built 6-channel at attach
  CSD->>CSD: TDM: 6 x 24-bit slots @ offsets 2,5,8,11,14,17
  CSD->>AMP: MOST150 BlockWidth 26 -> SinkNr 0x26
  AMP->>AMP: matrix (bank 1) -> DSP chain -> 19 speakers
```

## 🔧 The two closing fixes

Getting six channels to play took two months because the head unit silently collapsed to stereo through **two independent mechanisms**, each masquerading as a fault somewhere downstream. Both are **[car]**-confirmed.

### The CSD width bug (Qualcomm side)

Both `mpl1_int_ent` and the CarPlay device `mpl5_dio_ent` route to the **same** CSD device 78 (48 kHz / 24-bit). The active width is chosen at route-attach from the driver's static capability record. `mpl5` shipped `min_voices=2, min_fragsize=0x1800`; `mpl1` had `6, 0x4800`. The bug: `deva_update_session_info_from_list @0x54848` copies the record's **minimum** voices/fragsize into the *active* CSD session.

So even after widening the DMA side to 6 voices, CSD advanced `0x4800` bytes/tick while submitting only `0x1800` — one third of each fragment — **tearing at 62.5 Hz** with the sample clock still perfectly correct. Worse, Audioman attaches the route ~14 s **before the PCM channel exists**, so any patch that "reads the live channel" takes its fallback. The symptom in the car: a constant tick ("тцык тцык тцык"), low-pitched voices, mildly-off tempo, correct speakers, recognisable content.

The fixed timer arithmetic (16 ms period) shows why only 6/`0x4800` is correct:

| voices | fragment | frames / 16 ms | effective rate |
|---|---|---|---|
| 2 | `0x1800` | 768 | 48000 — correct |
| 2 | `0x4800` | 2304 | 144000 — 3× fast |
| 6 | `0x1800` | 256 | 16000 — 3× slow |
| 6 | `0x4800` | 768 | 48000 — correct |

**The fix (2026-09-02, confirmed by ear):** make `mpl5`'s caps record **mirror `mpl1` field-for-field** (min/max voices = 6, fragsize `0x4800`). The CSD session is then built six-channel at attach time. Stereo still opens because **QSA inserts a channel conversion** rather than refusing a below-min open. The month-long belief "widening the minima breaks stereo" was simply false.

### The SAD header (libairplay side)

Beside the PCM on MOST rides a **speaker-allocation (SAD) header**: six slots are always physically formed; the SAD says how many are *meaningful*. `asad=0xfc` = slots 1..6 (5.1); `asad=0xc0` = slots 1..2 (stereo). `dio::CQNXSoundIF::createMetadata` described AirPlay's *native* input (2ch/16-bit) and wrote `asad=0xc0` even after the PCM stream was widened to six channels — six sent, two declared, the amp kept L/R. **Fix:** interpose `createMetadata` to announce `asad=0xfc` / 6ch / 24-bit. **[car]** 2026-09-03: centre + surrounds reach the speakers.

A second independent control mismatch was also real: the hook's experimental `updateFeedback` rewrite replaced the stock stream list with an empty list, removing the clock/rate reference — delivery collapsed to ~one packet per second. The production behavior preserves the stock feedback; the vehicle reports `sr=48004.0625`, type 100, normal delivery.

### The two-day retraction

`tools/qsa_play.c` sets no SAD header, so it *inherited* the previous session's `0xc0`. Its L/R-only output was misread as "the amp is innocent", sending the search deep into ADSP/MOST/SHARC/COPP — all found **correct**. The rule that came out of it: **verify through CarPlay, never through `qsa_play`.**

## 💾 The mirror driver

`deploy/deva-ctrl-qc/` ships the fix as a three-step patch of the stock Qualcomm driver:

| Step | What it does |
|---|---|
| `patch_deva.sh` | caps maxima 2→6 + a DMA-side helper injected into the tail of a `__FILE__` diagnostic string (no code cave — `.rodata` shares the R+E segment) |
| `patch_deva_csd_live.sh` | live-channel read (now redundant) |
| `patch_deva_mirror_mpl1.sh` | **the step that actually fixed the car** — makes the minima mirror `mpl1` |

Ships as `deploy/deva-ctrl-qc/deva-ctrl-qc.so.patched` (needs a reboot to map). **Ceiling: 6 is the max a byte can reach** (`deva_csd_start_csd_session_playback` hardcodes 6; no 8-channel output node exists), so CarPlay's 5.1.2 = 8 channels needs a mandatory **8→6 fold** on the hook side. Channel order is Apple canonical `L R C LFE Ls Rs`, no reorder.

## 📊 The 8→6 fold and the amp sink table

5.1.2 height is a hard ceiling — see [[Dolby Atmos]] and [[Virtual Cabin and Amp RE]]. The APAC decoder emits native order **C L R Ls Rs LFE**, permuted to CarPlay order **L R C LFE Ls Rs**. For index 66 (5.1.2), the fold: L/R/C unchanged, LFE −6 dB, each height folded into its matching surround at −6 dB.

Selectable MOST sinks on the amp `8W0035465`:

| Sink | Profile | BlockWidth | Format |
|---|---|---|---|
| `0x21` | Entertainment_2.0 | 4 | 2 × 16-bit |
| `0x26` | Entertainment_5.1_SAD | 14 | 6 × 16-bit |
| `0x2A` | Entertainment_2.0_SAD | 8 | 2 × 24-bit |
| `0x32` | Entertainment_7.1_SAD | 26 | 8 × 24-bit |

CarPlay uses sink `0x26`. Note the advertised sink profile is BlockWidth 14, but the actual stock MOST Connect field for connection 162 is **BlockWidth 26** (`SinkNr 0x26, BlockWidth 0x001a`); the amp's V850 Connect handler checks only that the width is in `1..372` and stores it — it never compares against the profile's own width.

## 🔍 Magic-cookie codec authority

The codec authority on the buffered path is the **inline magic cookie**, not any APAP format index. iOS may alternate AAC-LC (index 23) ↔ APAC (index 65) *inside one session*; the receiver observes the change through the **new inline cookie**, tries the current decoder then its alternate, and the successful decoder plus decoded channels/rate/layout selects stereo upmix or APAC 5.1 reorder.

- **AAC-LC (index 23)** — decoded in-hook via Cinemo `libNmeSDK.so`; APAP time scale `0x0000bb80`, a distinct 39-byte magic cookie, ASC `11 90`; needs no resampling. `dio_manager` does not import `CinemoCreateAudioCodec`, and the AAC plugin has no OMX imports, so one decoder can be owned by the existing APAP receiver thread — no new thread.
- **APAC (index 65/66)** — decoded via `libcarplay_apac.so` (ABI v1).
- **The trap:** an early `[65,23]` run looked session-constant because the instrumentation read `spf` (=1024, APAP field 8) as an index; APAP field 2 is the active `APAudioFormatIndex`.

This cookie-driven APAC ↔ AAC switching has played in **both directions** on the vehicle inside one type-103 session with no dropout **[car]**. The checked-in supervisor advertises `[65,23]` with `AUDIO_SPATIAL=1`; stereo DSP is reversible via `AUDIO_STEREO_UPMIX=off` (then a `dio_manager`/CarPlay restart). See [[Landmines]] for the E2P 0xFC gate that decides whether the 6-channel route exists at all.

## ⚠️ What is confirmed and what stays open

**[car] confirmed:** discrete 5.1 end to end (2026-09-06); buffered stereo (AAC-LC index 23) beside APAC 5.1 (index 65); cookie-driven APAC↔AAC switch both directions, no dropout; phone-lock/background lifecycle survives (the QSA handle transfers to the APAP thread during the type-100 release gap).

**[open] / not done:** non-B&O internal 6ch (no route exists — see [[Virtual Cabin and Amp RE]]); the index-66 native channel order is unverified; live HU call/Siri route verification (host-tested only). Historical fault-localisation (byte transport, CSD topology, AFE slot packing, MOST allocation, amp sink selection, SHARC matrix) is retained in `docs/AUDIO_5_1_STATE.md` §§2–18 as closed clean boundaries — do not re-derive.
