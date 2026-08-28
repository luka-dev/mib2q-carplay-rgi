# MHI2Q CarPlay Audio — master reference

Everything we established about audio on the Audi MHI2Q head unit: the hardware ceiling, the CarPlay
path, why it is stereo, the levers that exist, and the plan to force multichannel Spatial / Dolby
Atmos. Consolidates HU_AUDIO_CEILING.md + CARPLAY_SPATIAL_DOLBY_PLAN.md and the session's RE.

Sources: firmware `Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-...` (`advanced/MU1316-appimg`) and its
`extracted/` subset; `Tools/retroarch-qnx`; iOS 26.6 (23G71) carkitd/accessoryd + iOS 27 dyld cache
(`Refferences/ios27_dsc/`); decompiles via IDA.

---

## 1. Hardware / driver ceiling

- **SoC/DAC**: Qualcomm APQ8064, QNX `io-audio` + `deva-ctrl-qc.so`, ACDB calibration (`mmx2_8064`).
  Output to the external amp over a **TDM** bus.
- **Channels: up to 6 (5.1)** — driver exposes `/dev/snd/six_channel_speakerp` (also
  `stereo_speakerp`, `mono_speakerp`, `stereo_to_all_speakersp`).
- **Sample rate: 48 kHz media bus.** Routing strings show TDM slots (`TDM_OUT_ANN_1_24KHZ` = 24 kHz
  voice) and a `deva_srf_rate_match` **hardware SRC** that resamples any source to the bus rate.
  There is **no 96/192 kHz path to the amp** — hi-res is downsampled to 48 k.
- **Bit depth: up to 32-bit container** (QSA negotiates `SND_PCM_SFMT_S16_LE`/`S32_LE`); DAC ~24-bit.
- **True platform max = 48 kHz / 24-bit / up to 5.1, FLAC-capable.** Hi-res buys nothing (48 k TDM).

### Local codec stack (Cinemo, `armle/usr/lib/cinemo/`)
`libNmeAudio.so` (core), `libNmeAudioRenderer.so` (mix/downmix), `libNmeAudioDevice.so` (QSA sink),
and decoders: **Dolby** (`libNmeAudioDolby.so` = DM3+ DDP/E-AC-3, **licence-gated** via
`dolby_license_path`/`dolby_license_mid`), **DTS** (`libNmeAudioDTS.so`), FLAC, AAC, MP3, WMA,
Vorbis, DPCM. This is the LOCAL-media path (files/disc) — NOT CarPlay.

---

## 2. Audio topology (per-source virtual channels + DSI focus)

Each source has its own virtual channel; the DSI audio service mixes/routes them to the amp.
- CarPlay (dio): media `mpl5_dio_ent`, prompts `ann3_dio_ann`/`ann4_dio_voice`, phone `tel2_dio_phone`.
- amp connection ids (`org.dsi.ifc.audio`): **DIO_MEDIA=162**, GAL_MEDIA=156, ENT_MEDIA_MFP=20.
- **How a custom app plays sound**: write PCM to a `mplN_*` device via QNX **QSA** (`snd_pcm`) AND
  take focus via **`dsi.audio.DSIAudioManagement`** (source select + volume + ducking).
  Reference impls of the sink write with arbitrary channels: `Tools/retroarch-qnx`
  (`src/audio/dsi_audio_client.h`, `drivers/qnx_qsa.c`) and Cinemo `libNmeAudioDevice.so`
  (`snd_pcm_open_preferred`, channels from `format.channels`). **The 6-channel sink-write is a solved,
  demonstrated problem on this box.**

---

## 3. CarPlay audio path (libairplay) and why it is stereo

CarPlay audio does NOT go through Cinemo — it rides **libairplay** (stock `app/eso/lib/libairplay.so`,
version 210.81; from-source tree = 280.33.8 SDK).

- **210 audio = PCM passthrough**: libairplay never decodes AAC for CarPlay; PCM-only → ALSA/QSA.
- **Advertised formats** (`libairplay AudioSessionGetSupportedOutputFormats`): AAC-ELD/LC, ALAC
  16/24, PCM 16/24 — **all ≤ 2 channels**. The phone (sender) only sends what we (receiver) advertise,
  so CarPlay is stereo **by our advertisement, not the DAC**.
- **Two streams exist**: `kAudioStreamType_MainAudio` (media) + `kAudioStreamType_AltAudio`
  (prompts/Siri), plus a `MainHighAudio` type in the SDK (stub-marked "wireless only"). Today prompts
  **duck** media (`Delegating ducking of audio to %f within %f seconds`).

### libairplay internals (stock .so, located)
- `_UpdateStreams` (`sub_1DFE8`): builds Main/Alt streams. The big format switch is only LOGGING; the
  real format is `AirPlayAudioFormatToPCM(fmt, param, asbd)` → 36-byte PCM ASBD →
  `CFObjectSetPropertyData(stream, …, asbd, 36)`.
- `AirPlayReceiverSessionSetup` (`0x272b8`): builds the advertised format list + negotiates.
- `AirPlayAudioFormatToPCM` (`0x1b2bc`): format enum → ASBD — **channelsPerFrame lives here**.
- Format enum tops at `0x2000000` = `AAC-ELD/48000/2`. **No multichannel enum value.**

### Latency (tunable via env, `dio_manager.json` audioLatency)
`ALL_OUT_LATENCY=80`, `ALL_IN_LATENCY=95`, `TEL_*`, `SPEECH_*` (ms). Real, safe lever (GH issue #2).

---

## 4. Levers that exist WITHOUT protocol work

- **Mixable audio**: Main+Alt streams already exist; today they duck. Apple `supportsMixableAudio`
  would overlay prompts without dropping media — depends on the amp mixing two streams vs ducking.
- **Latency tuning**: the env values above.
- Quality/rate for CarPlay is already at ceiling (stereo/48 k/24 = Apple's CarPlay main-audio max).

---

## 5. Spatial / Dolby Atmos — feasibility & plan

Goal: force the iPhone to push multichannel Spatial and/or Dolby Atmos over WIRED CarPlay.

### Feasible in principle; the gap is our stack predates Apple's multichannel format bits.
1. **AirPlay is receiver-advertised** → our advertised format bitmask is the lever.
2. **iOS 27 defines multichannel formats** (dyld cache blob `.36`, the AirPlay format table):
   `PCM/48000/16/5·7·9`, `AAC/48000/5`, `AACHE/48000/5`, **`EC3/48000/7`**, `PCM/48000/32`.
   - **Spatial = multichannel PCM (5/7/9)** — discrete channels, no codec licence.
   - **Dolby Atmos = `EC3/48000/7`** — E-AC-3 (DDP JOC).
3. **Our stack lacks the bits**: 210.81 tops at `AAC-ELD/48000/2`; from-source 280.33.8
   `kAirPlayAudioFormat_*` tops at stereo + `Reserved1/Reserved2`. Apple added multichannel later.
4. **Output can do it**: `six_channel_speakerp` + proven QSA 6ch write. Dolby: `libNmeAudioDolby`
   (E-AC-3 decoder, licence-gated) for the EC3 path.
5. **No wired/wireless gate**: carkitd + CarKit store `spatialAudioSupported`/`dolbyAtmosAudioSupported`
   from the accessory config without a transport condition (getter is a plain ivar; parser
   `initWithSessionStatusOptions:` sets them from advertised props, not transport). Apple's Ultra
   spatial CarPlay is wired.

### spatialAudioSupported vs dolbyAtmosAudioSupported
- **dolbyAtmosAudioSupported** = decode a **Dolby Atmos bitstream** (`EC3/48000/7`, JOC). Codec-bound,
  needs the licensed Dolby decoder (`libNmeAudioDolby`) integrated into the CarPlay path.
- **spatialAudioSupported** = **Apple Spatial Audio**, delivered as **multichannel PCM**; the phone
  renders, we just lay PCM into the 6ch sink. No codec licence. **The pragmatic first target.**
- Both are independent `CARSessionConfiguration` properties, advertised the same way as altScreen.

### Plan
- **Phase 0 (cheap, decisive, blocks all):** from iOS 27 blob `.36`, recover the exact **bitmask bit**
  for each multichannel format string (`PCM/48000/16/6`, `AAC/48000/5`, `EC3/48000/7`, …). Confirm
  CarPlay (not only AirPlay→ATV) offers them and whether `spatialAudioSupported` unlocks them.
- **Phase 1 — Spatial (multichannel PCM), no licence:**
  1. from-source libairplay: add the format to the advertised list (`AirPlayReceiverSessionSetup`) +
     a new `kAirPlayAudioFormat_*` enum entry (Phase-0 bit).
  2. Extend `AirPlayAudioFormatToPCM` to emit a 6/8ch ASBD.
  3. Route the multichannel PCM stream to `six_channel_speakerp` via QSA (mirror Cinemo/retroarch).
  4. Advertise `spatialAudioSupported` in `CARSessionConfiguration` (same SetProperty path as altScreen).
  5. Verify the amp accepts discrete 5.1 from the dio source on the unit.
- **Phase 2 — Dolby Atmos (EC3/48000/7), superset:**
  1. Advertise `EC3/48000/7`; set `dolbyAtmosAudioSupported`.
  2. Route the EC3 bitstream to `libNmeAudioDolby` (DDP) → multichannel PCM → sink. Needs the Dolby
     licence active + integrating the Cinemo decoder into the CarPlay audio path.

### Build strategy
Source-level (new enum + new `AirPlayAudioFormatToPCM` case + routing) → do it in **from-source
libairplay (280.33.8)**, not by patching stock 210.81. The config flag rides the **dio patch** we
already use for altScreen. Order: Phase 0 → Phase 1 (spatial PCM) → Phase 2 (Atmos).

---

## 6. What is NOT possible / not worth it
- Hi-res (96/192 kHz) — the 48 k TDM bus resamples everything down.
- Higher bit depth than 24 to CarPlay — already at the CarPlay/main-audio ceiling.
- Wireless CarPlay, Car Keys — out of scope (radios / UWB not present / not this box).

---

## 7. Dolby decoder licence (libNmeAudioDolby) — decoded values

The Cinemo Dolby DDP/E-AC-3 decoder is gated by a Dolby IUDC signed licence (project number +
channel_limit + caps), verified at init (`sub_AB04` → `NmeDDPIUDCVerify`). No external licence file —
the expected values are embedded, obfuscated as `sub_D6B4(enc,k1,k2) = enc ^ ~k2 ^ k1`
(k1=1211559425, k2=-893184683). Decoded:

| field | value |
|---|---|
| project_number | **1500** (0x5DC) |
| **channel_limit** | **6** (0x00000006) |
| caps | 2 |
| API version | 1.1 (major=1, minor=1) |

**`channel_limit=6` = up to 5.1 (6 channels).** Dolby DDP channel_limit convention: 2=stereo,
6=5.1, 8=7.1. So this HU's Dolby decoder IS licensed for **5.1 output** — NOT stereo-only. (Corrects
the earlier "probably stereo" worry.)

**Implication for the Atmos path**: Dolby decode to **5.1 is licensed** — no stereo gate. But the
AirPlay Atmos format `EC3/48000/7` is 7 channels, 1 over the 6 limit → the decoder would clamp/downmix
to 5.1 or reject; full 7.1/Atmos-height (7-9ch) exceeds the licence. **Atmos→5.1 is the licensed,
workable target; 7.1.4 is not.** The licence is signed (`NmeDDPIUDCVerify`) → channel_limit cannot be
forged upward. Still, Spatial (multichannel PCM) remains the cleaner first target — it needs no Dolby
decoder/licence at all.

---

## 8. Spatial 5.1 via Option A (dio 6-channel route) — verified path

Decision: PCM 5.1 Spatial (drop Atmos), delivered through the stock dio audio route (Option A), NOT
by intercepting and writing `six_channel_speakerp` ourselves. Hardware = B&O (surround), so the amp
renders 5.1 (a separate agent reverses the B&O firmware/mixer).

### Verified: the channel count is NOT hardcoded — it flows from the ASBD
`libairplay.so` on this HU embeds the dio platform sink (`dio::CQNXSoundIF`, opens `/dev/snd/*` via
QSA). `AudioStreamPrepare` (0x4a9d0):
- `get_audio_path(type)` (0x49398): media -> `mpl5_dio_ent`, phone -> `tel2_dio_phone`,
  speech -> `ann4_dio_voice`.
- `CAudioDriverIFFactory::create("mpl5_dio_ent")` then `driver->setFormat(channels=*(a1+92), bps, sr)`
  (vtable+16). The **channel count `*(a1+92)` comes straight from the negotiated ASBD**, so a 6ch ASBD
  makes libairplay open `mpl5_dio_ent` with 6 voices — no stereo hardcode to patch.

### Division of labour
- **This repo (libairplay hook)**: advertise the 5.1 PCM format + `spatialAudioSupported` (config,
  altScreen-class); hook `AirPlayAudioFormatToPCM` (0x2c308) to emit a 6-channel ASBD (a3[6]=6). Then
  `AudioStreamPrepare` opens the sink at 6ch automatically.
- **B&O firmware / mixer agent (dependency)**: provision `mpl5_dio_ent` for **6 voices** routed to
  `six_channel_speakerp` -> B&O amp. Stock provisions the CarPlay virtual channel as stereo (2
  voices); opening 6 voices on a 2-voice channel fails with `deva_channel_map: voices not supported`
  (AudioStreamPrepare returns -6736). io-audio already runs `default_device=six_channel_speakerp`, so
  the system output is 6ch — only the `mpl5_dio_ent` virtual-channel voices + route need lifting.

### Channel order to confirm
Apple 5.1 PCM order (L R C LFE Ls Rs) vs the Audi TDM / `six_channel_speakerp` expected order — map
on the mixer side; a mismatch swaps speakers. Also confirm the exact multichannel format the phone
offers (iOS 27 lists PCM/48000/16/5·7·9; the 6ch/5.1 variant is probe-confirmed on connect).

### Net
No patching of libairplay internals — only boundary hooks. The real gate is the dio/mixer 6-voice
route (B&O agent). Coordinate: **`mpl5_dio_ent` -> 6 voices -> six_channel_speakerp.**
