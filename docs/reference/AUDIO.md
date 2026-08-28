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
