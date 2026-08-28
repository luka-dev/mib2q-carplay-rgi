# MHI2Q head-unit audio ceiling — what the firmware can actually do

Ground truth from MU1316-MHI2Q P5087 (`advanced/MU1316-appimg`) + the retroarch-qnx RE of the
Audi audio path. Answers: how far can this box go (local FLAC etc.), and why CarPlay is stereo.

## The hardware / driver ceiling
- **DAC/amp: Qualcomm APQ8064** via QNX `io-audio` + `deva-ctrl-qc.so`; ACDB calibration
  (`mmx2_8064`). Output to the amp over a **TDM** bus.
- **Channels: up to 6 (5.1)** — the driver exposes `/dev/snd/six_channel_speakerp` alongside
  `stereo_speakerp` / `mono_speakerp` / `stereo_to_all_speakersp`.
- **Sample rate: 48 kHz media bus.** Routing strings show TDM slots like `TDM_OUT_ANN_1_24KHZ`
  (voice/prompts at 24 kHz) and a `deva_srf_rate_match` **hardware SRC** that resamples any source to
  the bus rate. There is **no 96/192 kHz path to the amp** — hi-res is downsampled to 48 k.
- **Bit depth: up to 32-bit container** (QSA negotiates `SND_PCM_SFMT_S16_LE`/`S32_LE`); DAC
  effective ~24-bit.
- **FLAC: decoded** (`libflac.so`, `mmedia/flac_decoder.so`, Cinemo `libNmeAudioFLAC.so`) — but the
  output is resampled to 48 k and routed to the speaker config like everything else.

So the true platform max = **48 kHz / 24-bit / up to 5.1 channels**, FLAC-capable. Hi-res (96/192)
buys nothing here — it hits the 48 k TDM bus.

## The audio topology (why per-source, and how to feed it)
Each source has its own virtual channel and its own amp connection, mixed by the DSI audio service:
- CarPlay (dio): media `mpl5_dio_ent`, prompts `ann3_dio_ann`/`ann4_dio_voice`, phone `tel2_dio_phone`.
- amp connection ids (`org.dsi.ifc.audio`): **DIO_MEDIA=162**, GAL_MEDIA=156, ENT_MEDIA_MFP=20.
- A custom app plays by writing PCM to a `mplN_*` device via QNX **QSA** (`snd_pcm`) AND taking focus
  via **`dsi.audio.DSIAudioManagement`** (source selector + volume + ducking). This is exactly how
  `retroarch-qnx` (`src/audio/dsi_audio_client.h`, `drivers/qnx_qsa.c`) outputs sound on this unit —
  the reference for feeding the sink directly, bypassing libairplay.

## Why CarPlay is stereo — and what that means
CarPlay audio is stereo **not because of the DAC** but because `libairplay.so`
`AudioSessionGetSupportedOutputFormats` advertises only ≤2-channel formats (AAC-ELD/LC, ALAC 16/24,
PCM 16/24, all /2), and it delivers stereo into `mpl5_dio_ent`. Stereo/48 k/24-bit is **already the
platform's media-bus ceiling** — so on rate and depth CarPlay is maxed; the only axis where the
hardware exceeds CarPlay is **channels (5.1)**.

Getting 5.1 to CarPlay would need Apple's spatial-audio negotiation (`spatialAudioSupported`) — which
neither the stock 210.81 nor our 280.33.8 libairplay has — PLUS a multichannel dio virtual channel.
Not reachable on this stack. (The amp's own B&O/Logic7 upmix from stereo is separate and already
runs independently of CarPlay.)

## Net for "improving CarPlay audio"
Quality/rate/channels are effectively maxed for what this platform delivers to CarPlay. The only
real levers left are **mixable prompts** (Main+Alt streams already exist; today they duck) and
**latency tuning** (`ALL_OUT_LATENCY`/`ALL_IN_LATENCY` env) — not hi-res or surround.
