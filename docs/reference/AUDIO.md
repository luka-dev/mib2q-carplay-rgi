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

---

## 9. AirPlay audio-format bit map (iOS 27) — extracted statically

From AirPlaySupport `_APAudioFormatCopyAudioFormatsToReadableString` (iOS 27,
`Refferences/ios27_dsc/.../extracted_dylibs/AirPlaySupport`), the bit->format-string table. The
`AirPlayAudioFormat` mask is uint64; multichannel lives above bit 31. Key values:

| format string | bit | value |
|---|---|---|
| **PCM/48000/16/5.1** (our Spatial target) | 44 | `0x100000000000` |
| PCM/48000/16/5.1.2 | 38 | `0x4000000000` |
| PCM/48000/32f/5.1 (32-bit float) | 47 | `0x800000000000` |
| PCM/48000/32f/5.1.2 | 48 | `0x1000000000000` |
| DDPLUS/48000/5.1 (Dolby, 5.1) | 50 | `0x4000000000000` |
| DDPLUS/48000/9.1.6 | 53 | `0x20000000000000` |

Full layout family exists: PCM 16 & 32f in 5.1 / 5.1.2 / 5.1.4 / 7.1 / 7.1.2 / 7.1.4 / 9.1.6;
QAAC/QAACHE/QC3/DDPLUS multichannel variants. Format-string layout notation is `codec/rate/bits/L.F.H`
(e.g. 5.1.2 = 5 main + 1 LFE + 2 height).

### The pipeline is 64-bit clean — bit 44 is NOT truncated (earlier note was wrong)
The format is a `uint64` end to end (verified in IDA, imagebase 0):
- `AirPlayReceiverSessionSetup@0x276dc` reads `audioFormat` via **`CFDictionaryGetInt64`** (R1:R0 =
  hi:lo), stores the **full 64-bit QWORD** at `stream+0x10`, and at `0x2770c` calls
  `AirPlayAudioFormatToASBD` with the whole u64 (bit 44 intact).
- `_UpdateStreams`(sub_1DFE8)`@0x1e514` does **`LDRD R0,R1,[R4,#0x10]`** (64-bit load) → `ToPCM` →
  `ASBD`, format intact. `ToPCM`'s first arg is 64-bit too (IDA mis-typed it as `int a1`; it forwards
  `(a1,a2)` as the u64 to ASBD).

So there is **no 32-bit truncation** (the old `AudioStreamPrivate+16`=_DWORD claim was wrong; it's a
QWORD). The one and only blocker is `AirPlayAudioFormatToASBD@0x2bda0`: an exact-match table that
rejects every high bit (`if (HIDWORD(a1)) return -6735`). Both callers reach it through the .plt/GOT,
so a single LD_PRELOAD interposer on `AirPlayAudioFormatToASBD` (the mechanism altscreen already uses)
removes the rejection and returns a 6ch ASBD for both the SETUP validation and the stream build. No
SETUP rewrite / flag / sentinel needed. See section 10.

### Channel order
Apple 5.1 PCM order = canonical **L R C LFE Ls Rs**. Must be matched to the Audi
`six_channel_speakerp` order (deva mixer / B&O agent side); a mismatch swaps speakers.

---

## 10. 5.1 design — single 64-bit ASBD interposer (final)

The pipeline is 64-bit clean (section 9), so the earlier SETUP-rewrite / flag / sentinel design is
unnecessary. Two hooks, nothing else — both are plain LD_PRELOAD interpositions of libairplay exports
that the .plt/GOT routes even for intra-library calls (proven by altscreen's SessionSetup interposer).

1. **`AudioSessionGetSupportedOutputFormats(int streamType)`** — OR-in `SPA_FMT_5_1` (bit 44) for
   `streamType==100` (main audio) so the phone offers/selects 5.1. Stereo path untouched.

2. **`AirPlayAudioFormatToASBD(uint64 fmt, uint32 *asbd, uint32 *bits_out)`** — when `fmt==bit 44`,
   fill the 9-dword ASBD for 48000 / 16-bit / 6ch (`bpf = 6*16/8 = 12`; layout confirmed in section 9)
   and `return 0`; else fall through to the real function. This single interposer is hit by BOTH the
   SETUP validation (`AirPlayReceiverSessionSetup@0x2770c`) and the stream build
   (`_UpdateStreams→ToPCM→ASBD`), so it removes the `-6735` rejection and delivers the 6ch ASBD
   everywhere. The 6ch count then flows unpatched to `AudioStreamPrepare` → `CQNXSoundIF` → snd_pcm on
   `mpl5_dio_ent` (channels come straight from the ASBD, section 8).

Why not `AirPlayAudioFormatToPCM`? It just delegates to ASBD; hooking ASBD covers both entry points
with one function.

`spatialAudioSupported` (head-tracked "Spatial Audio" UX) is a SEPARATE optional flag on
CARSessionConfiguration; if wanted, altscreen's existing SessionSetup wrapper can add "spatialAudio"
to the control-SETUP enabledFeatures via the `spa_augment_setup_response` seam. Discrete 5.1 PCM does
not depend on it.

### To verify on the unit (only remaining unknowns)
- Channel order L R C LFE Ls Rs vs `six_channel_speakerp` (mixer / B&O agent).
- `mpl5_dio_ent` provisioned for 6 voices (B&O agent) BEFORE arming, else the 6ch open has nowhere to
  land.
- The ASBD 3rd arg (`bits_out`) is written from stock `LABEL_24 (*a3 = a2[7])` — confirm on a real 6ch
  ASBD log that nothing else reads a wider field.

Enable with `export SPATIAL_5_1=1` (value-checked; off by default). Env change needs a dio-supervisor
relaunch, per the getenv-presence-check trap.

---

## 11. HU-side path: does discrete 5.1 even reach the amp? (deva-ctrl-qc RE)

The amp (SoundAmp doc) is genuinely 5.1/surround-native (19 speakers, 16-ch Class-D, Symphoria).
So the binding question is the HU→amp link. Traced statically this session (imagebase 0):

### Topology
App opens a QSA PCM `mplN_<owner>_ent` → `io-audio -d qc` (**deva-ctrl-qc.so**, Qualcomm
APQ8064/WCD9310 ADSP) → mixer → output device → AFE → **MOST** → B&O amp. Sources:
`deva_srf_platform.c` / `deva_mixer.c` (paths in the binary).
- CarPlay = **`mpl5_dio_ent`** (libairplay opens it directly; channels come from the ASBD → my hook = 6).
- GAL media = `mpl6_gal_ent`; nav/tel/ann have their own `annN_*` / `telN_*` nodes.

### Key findings (good news — the HU is NOT stereo-locked)
1. **Output to the amp is 6-channel.** io-audio's default device is **`six_channel_speakerp`**
   (`io-audio -d qc default_device=six_channel_speakerp`), alongside `mono_speakerp` / `stereo_speakerp`
   / `stereo_to_all_speakersp`. The AFE/MOST port is 6-wide by default.
2. **The deva mixer has a real 6-channel (5.1) playback path.** `build_playback_mixer` (in
   `deva_setup_mixer`) switches on a channel-position mask and builds the PCM/IO mixer element with the
   matching voice count + channel-map table:
   | mask | voices | channel-map |
   |---:|---:|---|
   | 1  | 1 (mono)   | `unk_A5814` |
   | 3  | 2 (stereo) | `unk_A5424` |
   | **63** | **6 (5.1)** | `unk_A5520` |
   So a 6-voice mixer element with a dedicated 5.1 channel map exists in the shipping driver — not a
   stereo-only build.
3. GAL configures its OWN media sink stereo (`gal.json` media.channels=2 → `mpl6_gal_ent`) — an app
   choice, not a driver limit. CarPlay's `mpl5_dio_ent` is independent.
4. Per-device struct stride 0x160; the first field (`5` for mpl5, `6` for mpl6) is the device **index**
   (mpl5/mpl6), not a channel count.

### Remaining unknowns (the last mile — best confirmed on-unit, read-only)
A. **`mpl5_dio_ent`'s instantiated width** — is its mix_config mask 63 (6ch) or 3 (stereo)? Sets whether
   a 6ch open passes through discretely or gets downmixed at the device. (Static: buried in the PCM-cap
   / mix_config build; on-unit: open the device and read `max_voices` / channel map.)
B. **ADSP/ACDB COPP topology** on the ent→MOST path — a 6-in→6-out passthrough (true discrete) vs a
   2→6 up-mix. Lives in the opaque Qualcomm ACDB blobs (`/etc/acdb/mmx2_8064/*`); practically settled by
   streaming discrete 6ch test tones on-unit and checking which amp channels move.
C. **MOST synchronous allocation** for the ent stream = 6 channels (MOST NetServices/INIC). The amp is
   MOST Gen2 16-ch, so bandwidth is not the limit.

### Verdict
The HU is 6-channel-capable end to end at the driver + output level — far better than "stereo-only +
amp Symphoria up-mix". Discrete 5.1 from CarPlay is **plausible, not architecturally blocked**. The
decision now rides on (A) the `mpl5_dio_ent` device width and (B) the ADSP topology, both best nailed
by a targeted on-unit probe rather than more static grind on ACDB blobs.

### §11 UPDATE — last mile SOLVED statically (deva caps table)

The three unknowns are resolved by reading the deva-ctrl-qc platform capability table.

**Tables** (from `deva_mib_get_session_handles_from_platform_id`, platform_id=mib2==2, 31 slots):
names @`0xA5C54` (stride 272, name at +16), **caps @`0xA7D44`** (stride 48), ctx @dynamic (stride 352).
Devices are 1-indexed (slot 0 reserved). Caps record dwords: [1]=format(1<<n), [2]=rates,
[3]=min_rate, [4]=max_rate, **[5]=min_voices, [6]=max_voices**, [7]/[8]=frag min/max.

**max_voices read directly:**
| idx | device | max_voices |
|--:|---|--:|
| 1 | `mpl1_int_ent` (internal media) | **6** |
| 2 | mpl3_mirror_lnk | 2 |
| 3 | mpl4_sdis_a2ls | 2 |
| 4 | **`mpl5_dio_ent` (CarPlay)** | **2** |
| 5 | mpl6_gal_ent (GAL) | 2  ← matches gal.json media.channels=2 (parse cross-check ✓) |

**(A) The bottleneck is `mpl5_dio_ent` = 2 voices** — a stereo cap on the CarPlay entertainment PCM
in the deva platform table. NOT the output (`six_channel_speakerp`=6ch) and NOT the mixer.

**(B)+(C) already solved:** `mpl1_int_ent` = **6 voices**, i.e. the internal-media path already streams
6 discrete channels through the same chain (mixer 6ch path → `six_channel_speakerp` → MOST → amp). So
the discrete-6ch topology + MOST allocation exist and are exercised today. **The amp needs no change**
(it already receives 6 discrete from internal media). The earlier "provision mpl5 for 6 voices / B&O
dependency" framing was wrong — it is a HU-side deva cap, not an amp change.

**The mixer follows automatically:** `build_playback_mixer`'s channel mask = `(1<<voices)-1`
(1→1, 2→3, 6→63). Bump mpl5 voices to 6 → the mixer auto-selects the 63/6-voice element (channel-map
`unk_A5520`). No separate mixer edit.

### The fix (two pieces)
1. **libairplay**: the 64-bit `AirPlayAudioFormatToASBD` interposer (section 10) → 6ch ASBD for bit44.
2. **deva-ctrl-qc.so**: raise `mpl5_dio_ent` caps from 2→6 voices — caps record idx 4 at
   VA `0xA7D44 + 48*4 = 0xA7E04`, dwords [5]/[6] (`+0x14`/`+0x18` → VA `0xA7E18`/`0xA7E1C`), value
   `0x02`→`0x06`. (Also review [7]/[8] fragsize, currently 0xC00 = per-2ch; mpl1's 6ch uses 0x4800.)
   Translate VA→file offset via section headers before patching.
   **First check** `deva_parse_commandline_parameters` (it references `mpl5_dio_ent`) for a per-device
   voices override — if one exists, this becomes a `DEVA_ARGS` line in `startup.sh` (no binary patch).

Net: discrete CarPlay 5.1 to the B&O amp is **not architecturally blocked** and needs only (1) the
libairplay ASBD hook + (2) a small `mpl5_dio_ent` 2→6 voices bump on the HU. No amp-side work.
