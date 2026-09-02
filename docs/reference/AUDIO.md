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
- **deva caps (dependency, now implemented)**: provision `mpl5_dio_ent` for **6 voices**; opening
  6 voices against its stock 2-voice cap fails with `deva_channel_map: voices not supported`
  (AudioStreamPrepare returns -6736). The later static MOST audit proved that connection 162 already
  selects `eSNK_DTCP51`, the amplifier's `SinkNr 0x26` / `BlockWidth 14` profile. There is no separate
  B&O sink-route patch; see `docs/DIO_MEDIA_SINK_STATIC_AUDIT.md`.

### Channel order to confirm
Apple 5.1 PCM order (L R C LFE Ls Rs) vs the Audi TDM / `six_channel_speakerp` expected order — map
on the mixer side; a mismatch swaps speakers. Also confirm the exact multichannel format the phone
offers (iOS 27 lists PCM/48000/16/5·7·9; the 6ch/5.1 variant is probe-confirmed on connect).

### Net
No patching of libairplay internals — only boundary hooks. The device-cap gate was the
`mpl5_dio_ent` 6-voice cap; the stock external-amplifier logical route is already DTCP 5.1.
Coordinate: **`mpl5_dio_ent` -> 6 voices -> six_channel_speakerp.**

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
   **Confirmed a binary patch, not config:** `deva_parse_commandline_parameters` is a name→id
   `strcasecmp` chain (for `default_device=`/`dev_id=`); it exposes NO per-device voices knob, so the
   voice count can only be changed in the static caps table above.

Net: discrete CarPlay 5.1 to the B&O amp is **not architecturally blocked** and needs only (1) the
libairplay ASBD hook + (2) a small `mpl5_dio_ent` 2→6 voices bump on the HU. No amp-side work.

---

## 12. iOS side — does "Spatial (Y) without Dolby (N)" send raw multichannel PCM? (IMPORTANT correction)

Checked iOS 27 `AirPlaySupport` (the format-negotiation layer our libairplay-class receiver talks to).
Two decompiled builders settle it:

**`_audioFormatDescriptionUtils_CreateAudioFormatDescriptionListForBufferedSender`** — what the phone
OFFERS to send for buffered audio (Apple Music streaming). It appends:
- 22, 23 — AAC/ALAC **stereo** base
- `if (atmosAllowed)` → 39, 40 — **Dolby Atmos EC3** (DDP JOC)
- `if (losslessAllowed)` → 21 (+ 20/86 for 16/20-bit) — lossless ALAC
- `if (feature && APAC_allowed)` → **APAC** (Apple Positional Audio Codec — Apple's spatial codec)
- passthrough list
- **NO raw multichannel PCM.** The buffered sender never offers PCM 5.1/7.1.

**`_APSAudioFormatDescriptionListCreateForBufferedReceiverWithAudioCapabilities`** — what a receiver
ADVERTISES it can accept. Raw multichannel PCM appears here: inside `if (feature && atmosAllowed)`, it
loops the peer's caps and, for an `lpcm` entry, appends **65 (PCM 5.1) if channels>=6**, **67 if >=8**.
So raw multichannel PCM is an ACCEPT-side format for realtime/medium-latency streams, gated on the
peer advertising `lpcm >= 6ch`.

### Conclusion (corrects §5/§10 optimism)
- **Three separate multichannel transports** exist in iOS: raw multichannel **PCM** (65/67),
  **Atmos EC3** (39/40), and **APAC** (Apple's spatial codec).
- **Apple Music surround is delivered as EC3-Atmos or APAC — NOT raw PCM.** The buffered sender has no
  raw-multichannel-PCM offer, so advertising PCM 5.1 alone makes Apple Music fall back to **stereo**
  AAC/ALAC. This is exactly why the press always pairs "Spatial + Dolby Atmos": for Apple Music the
  spatial content *is* the Atmos/APAC stream.
- Raw multichannel PCM (our hook's path) is only taken by **realtime/app audio that itself outputs
  multichannel PCM** (some games/apps), where the phone renders to PCM and the receiver advertised
  `lpcm >= 6ch`. It will carry those — but not the Apple Music catalogue.

### What this means for us
- Our libairplay ASBD hook + deva 2→6 patch **still delivers discrete 5.1 for realtime/app PCM audio**
  (and any source the phone renders to PCM). Keep it.
- **For Apple Music surround specifically, PCM-only is not enough.** The realistic options:
  1. **EC3 (Atmos) decode in the car** — we already have `libNmeAudioDolby` (E-AC-3, licensed
     channel_limit=6). Advertise EC3/48000 + `dolbyAtmosAudioSupported`, route the EC3 bitstream
     through libNmeAudioDolby → 6ch PCM → the same 6ch sink. This is the path that actually gets
     Apple Music surround. Heavier (decoder integration into the CarPlay audio path).
  2. **APAC** — Apple's codec; not available to us, skip.
- Open nuance to confirm on-unit: whether CarPlay Apple Music uses the *buffered* or a
  *medium-latency/realtime* sender path (the raw-PCM-multichannel accept path lives on realtime). If
  CarPlay music turns out to ride a realtime path that DOES emit multichannel PCM, the PCM route could
  still get it — but the buffered-sender evidence says plan for EC3.

### §12 CORRECTION — the buffered-sender evidence does NOT apply to CarPlay

The §12 analysis above decompiled `CreateForBufferedSender` — that is the **AirPlay-2 buffered** path
(phone streams *encoded* audio to a HomePod/speaker that decodes). **CarPlay is not that path.**

CarPlay audio is **realtime PCM**: the phone decodes the content on-device (Apple Music AAC included)
and sends finished **PCM** to the car; libairplay never decodes (stock "210 audio = PCM passthrough").
So the wire format to the car is PCM, always, and the phone renders whatever the AirPlay negotiation
advertised. Consequences:
- Today libairplay advertises **stereo** PCM → the phone downmixes everything (Atmos masters too) to
  stereo PCM. That is why CarPlay is stereo — by our advertisement.
- Advertise **6ch** PCM (the hook) → the phone renders to 6ch PCM. The multichannel-PCM accept path
  (65/67) is exactly this machinery. **No EC3/APAC needed for CarPlay** — those are the AirPlay-speaker
  / buffered story.
- So the PCM route (libairplay ASBD hook + deva 2→6) is the correct first target for CarPlay,
  including Apple Music, contrary to the §12 conclusion which mis-applied the buffered path.

**The one real unknown stays on-unit:** whether the phone renders an Apple Music *spatial/Atmos master*
to discrete 5.1 PCM for CarPlay when we advertise 6ch + `spatialAudioSupported`, or always downmixes to
stereo for CarPlay regardless. The transport (realtime multichannel PCM) is sound; only Apple's
content-side render decision for the CarPlay realtime path needs a live connect to confirm.

---

## 13. External evidence — first shipping CarPlay Spatial (Volvo EX90), and our stance

MacRumors, 2026-08-28: "Spatial Audio in CarPlay Now Rolling Out in Volvo Vehicles" — Spatial Audio in
CarPlay **powered by Dolby Atmos**, rolling out to Volvo EX90 (also EX60/ES90) via a **car software
update**, **requires the optional Bowers & Wilkins** sound system.

What it establishes:
- CarPlay Spatial Audio is real, shipping (iOS 26 era), and **branded/powered by Dolby Atmos** — the
  content is Atmos (matches "the press always pairs Spatial + Atmos": for CarPlay they are one feature).
- It needs a **premium Atmos-class sound system** (B&W in Volvo ≈ **B&O in our Audi** — same tier).
- It shipped as a **car-side software update**, i.e. NOT a trivial "advertise a bit" — the car's
  software had to add spatial/Atmos handling. This aligns with our work being real (hook + deva +
  possibly a decode path), not a flag flip.

What it does NOT settle: whether the wire format to the car is an **EC3-Atmos bitstream** (car decodes)
or **phone-rendered multichannel PCM** (car plays PCM). iOS has both mechanisms; the article doesn't
say which CarPlay uses.

Dolby machinery already present in OUR stack:
- HU `libNmeAudioDolby` (Cinemo E-AC-3 decoder, licence channel_limit=6) — today only on the LOCAL
  media path, NOT wired into CarPlay.
- B&O amp SHARC ROM carries Dolby/DTS decoders on some variants (SoundAmp datasheet, §2 of the amp doc).

**Stance (stable — keep both paths, let a live connect decide):**
1. **PCM path** — libairplay ASBD hook + deva 2→6 (cheap). Works immediately if CarPlay negotiates
   multichannel PCM.
2. **EC3-Atmos path** — route the EC3 bitstream through `libNmeAudioDolby` → 6ch PCM → the 6ch sink
   (heavier). Needed if CarPlay insists on an Atmos bitstream.

Decisive next step is a single on-unit iPhone (iOS 26+) CarPlay connect with our 5.1 advertisement +
`spatialAudioSupported`: capture which audio format the phone actually offers/selects, then commit to
that path. Do not pre-commit to PCM-only.

---

## 14. Where to announce Dolby/Spatial — the advertisement point (step 1 RE)

Question: where does our stack advertise `spatialAudioSupported`/`dolbyAtmosAudioSupported`, and do we
announce Dolby at all? Answer: **we don't — nowhere in the HU firmware** (grep of the whole extracted
tree for spatialAudio/dolbyAtmos/sessionStatusOptions = zero hits, incl. stock libairplay 210.81). Our
stack predates these; they must be added from scratch.

### How iOS consumes them (confirmed)
- CarKit `-[CARSessionConfiguration initWithSessionStatusOptions:propertySupplier:]` (@0x1c2008f7c)
  parses the accessory's **`sessionStatusOptions`** dict: keys at GOT `0x1E00FA4C0`/`4C8`/`4D0` →
  config ivars +280 / +30(BYTE) / block_invoke_6. Property names confirmed (`spatialAudioSupported`,
  `dolbyAtmosAudioSupported`; literal `@"spatialAudioSupported"` seen in iOS 26.1 sources).
- These are **session-level accessory capabilities**, distinct from (a) the per-stream SETUP
  `enabledFeatures` (where altScreen lives) and (b) the `AudioSessionGetSupportedOutputFormats` mask
  (the audio FORMAT lever our hook already touches).

### The emit rail on our side
libairplay's capability surface = `AirPlayCopyServerInfo` (/info dict) + `AirPlayGetFeatures` +
`AirPlayGetStatusFlags`. altScreen already proves the pattern: a new iOS-27-era capability is injected
into our old stack via the **dio-patch SetProperty into the session config + our altscreen hook merging
into the SETUP response** ([[altscreen_ios26_gate_verified]], "altScreen" in /info extendedFeatures →
carEndpoint). **spatial/dolby ride the same session-config rail — NOT the audio hook.**

### Two levers, both required
1. **FORMAT** — `AudioSessionGetSupportedOutputFormats` (audio hook) advertises the PCM channels we accept.
2. **CAPABILITY** — `spatialAudioSupported`/`dolbyAtmosAudioSupported` in `sessionStatusOptions` unlock
   the phone offering spatial/Atmos at all (per the Volvo "powered by Atmos" evidence, §13). Advertised
   via the altScreen rail.

### Still to pin before implementing
- The exact **wire-key CFString** for each in the `sessionStatusOptions` dict (cross-dylib CFString in
  CarKit/carkitd; property name known, wire key to be read from IDA at implementation time).
- Whether announcing `dolbyAtmosAudioSupported` makes the phone send EC3 (then we need the
  libNmeAudioDolby decode path, §13) or escalate to multichannel PCM. Gate the announcement OFF by
  default (announcing Dolby without an EC3 decode path risks the phone sending EC3 we can't play).

### §14 UPDATE — the announcement rail, resolved end to end

Corrections + the decisive finding.

**Correction to §14:** `sessionStatusOptions` is a **bitmask** (`unint64_t`), not a dict — CarKit tests
it with `& 2` / `& 4` / `& 8`. The dict CarKit actually reads capabilities from is fetched by the
propertySupplier with key **`CARkAPEndpointProperty_EndpointInfo`** (symbol visible at CarKit
`0x1c20093b0`) = **the accessory's AirPlay `/info` dictionary**. Every capability is then a keyed
lookup on that dict (`X0`=/info, `X2`=CFString key).

Verified ivars: **`spatialAudioSupported` = +21**, **`dolbyAtmosAudioSupported` = +22**.
Parse shapes differ:
- **dolbyAtmos** — an **explicit BOOL** (NSNumber) read from a sub-dict of /info.
- **spatial** — **INFERRED**: CarKit iterates an **array of dicts** from /info and sets +21 = 1 when an
  element carries the key it looks for.
- Neither is an `extendedFeatures` string — the full iOS 27 vocabulary is `uiContext, focusTransfer,
  h.264Level5.1, mainBuffered, altScreen, enhancedSiri, hevc, fileTransfer, uiSync,
  vehicleStateProtocol, sessionManagement, videoPlayback, logTransfer` (no spatial/atmos). So the
  altScreen string-list rail is NOT how these are announced.
- The feature is **iOS 27+**: iOS 26.1 CarKit has no spatial/atmos symbols at all.

**The decisive finding — our audio hook already IS the spatial announcement rail.**
Stock `AirPlayReceiverServerPlatformCopyProperty` (`0x1fe08`) serves the /info `audioFormats` property
by building exactly an **array of two dicts** (one per audio type) — and fills each with our formats:
```c
for (i = 0; i < 2; i++) {                       // v21[] = 'd'(0x64=100 main), 'e'(0x65=101 alt)
    d = CFDictionaryCreateMutable(...);
    CFArrayAppendValue(arr, d);
    CFDictionarySetInt64(d, <type>, v11, 0);
    in  = AudioSessionGetSupportedInputFormats(v11);
    if (in)  CFDictionarySetInt64(d, <audioInputFormats>,  in,  HIDWORD(in));
    out = AudioSessionGetSupportedOutputFormats(v11);        // <<< OUR HOOK
    if (out) CFDictionarySetInt64(d, <audioOutputFormats>, out, HIDWORD(out));  // full 64-bit
}
```
So: the array-of-dicts CarKit scans for spatial **is** `/info audioFormats`, its `audioOutputFormats`
value comes straight from the function our hook interposes, and it is written as a **full 64-bit**
int (high dword included) — bit 44 survives into /info. Bonus confirmation: `'d'` = 0x64 = **100** and
`'e'` = 0x65 = 101, so the hook's `streamType == 100` guard targets exactly the main-audio entry.

**Practical answer**
- **Spatial**: no separate flag to add — advertising the multichannel bit through the hook already puts
  it in the /info array CarKit inspects. The on-unit connect tells us whether iOS accepts it as the
  spatial signal.
- **Dolby Atmos**: a genuinely separate, explicit BOOL key in /info. Candidate wire key `dolbyAtmos`
  (camelCase string present in the iOS 27 shared cache). Adding it means extending the /info dict —
  same platform `copyProperty` rail (or the dio-patch SetProperty path altScreen uses). **Keep it OFF
  until an EC3 decode path exists** (§13): announcing Atmos without decode risks the phone sending EC3
  we cannot play.
- Not pinned statically: the exact CFString key names (they live in the shared cache; the CFString
  structs point at cache-relative offsets, and the interned string pool makes neighbour analysis
  useless). Resolve with a cache-aware tool or read them at runtime if the Dolby announce is pursued.

---

## 15. DECISIVE — CarPlay spatial transport is AAC 5.1.2, NOT Dolby and NOT PCM

Answer to "does Apple Music 5.1 need Dolby?": **no — and it isn't PCM either.** Found the phone-side
function that defines what iOS may send to a CarPlay ("screen") receiver, iOS 27 AirPlaySupport:

```c
__int64 APAudioFormatGetSupportedTransportAudioFormatsForScreenReceiver(_QWORD *a1)
{
  bool spatial = APAudioFormatIsScreenSpatialAudioReceiver();   // device-model + feature gate
  if (a1) *a1 = 0x400000;                                        // bit 22
  return spatial ? 0x40001440800LL                               // bits 11,18,22,24 + bit 42
                 : 21235712;                                     // 0x1440800 = bits 11,18,22,24
}
```
**The entire spatial delta is ONE bit: 42.** (`0x40000000000`)

### What bit 42 is
`APAudioFormatSelectTransportFormatForAudioSourceInputFormat` picks the transport format and logs its
groups, which pins the neighbourhood:
- 5.1 case tries **bit 39** (`0x8000000000`) then **bit 41** (`0x20000000000`) → log: *"Supported format
  does not include **AAC-ELD/AAC-LC 48k 5.1** format"*
- 5.1.2 case tries **bit 40** (`0x10000000000`) then **bit 42** (`0x40000000000`) → log: *"Supported
  format does not include **AAC-ELD/AAC-LC 48k 5.1.2** format"*
- stereo fallback tries bit 21 / bit 23 → *"AAC-LC 48k stereo"*

So bits 39–42 are the AAC-LC/AAC-ELD 48 kHz 5.1 and 5.1.2 pairs. The exact order, confirmed from
the descriptor/ASBD tables, is `39=AAC_LC/5.1`, `40=AAC_LC/5.1.2`, `41=AAC_ELD/5.1`, and
**`42=AAC_ELD/5.1.2`**
(the descriptor strings `AAC_ELD/48000/5.1`, `AAC_ELD/48000/5.1.2`, `AAC_LC/48000/5.1`,
`AAC_LC/48000/5.1.2` all exist in AirPlaySupport).

### Consequences — three of them are hard
1. **Dolby is NOT the CarPlay transport.** No EC3/DDPLUS bit appears in the screen-receiver mask at all.
   Announcing `dolbyAtmosAudioSupported` will not make the phone send EC3 over CarPlay, and the §13
   "EC3 via libNmeAudioDolby" plan does not apply to CarPlay. (Atmos remains the *content* brand; the
   phone renders it into the AAC 5.1.2 transport.)
2. **PCM multichannel is NOT offered to CarPlay either.** Raw multichannel PCM (bits 65/67 in the
   description-list API, `PCM/48000/16/5.1` = bit 44) belongs to *buffered* receivers (HomePod-class),
   not to a screen receiver. **Our hook currently advertises bit 44 — a bit the phone never offers for
   CarPlay — so it can never be selected; the session would just stay stereo.**
3. **It is 8 channels (5.1.2 = 5 + LFE + 2 height) and it is AAC-compressed.** That breaks two of our
   assumptions: our stack is *PCM passthrough and never decodes AAC* ([[audio_210_pcm_passthrough]]),
   and our sink is **6-channel** (`six_channel_speakerp`), so 5.1.2 would need an 8→6 fold.

### What a real CarPlay spatial implementation would require
- Advertise **bit 42** (`AAC_ELD/48000/5.1.2`) in `audioOutputFormats` for stream type 100 (the /info
  rail from §14 still holds — that part was right).
- Add an **AAC-ELD multichannel decoder** into the CarPlay audio path (the HU has Cinemo AAC decoders,
  but they are on the local-media path, not CarPlay).
- **Downmix 8 → 6** (fold the two height channels) for `six_channel_speakerp`, or find an 8-channel
  route. The deva `mpl5_dio_ent` 2→6 patch stays necessary but is no longer sufficient.
- The phone side is additionally gated by `APAudioFormatIsScreenSpatialAudioReceiver` (device-model +
  feature flag) — so it also depends on the iPhone model.

**Status of our current work:** the deva 2→6 patch and the /info rail remain valid; the *format bit* in
`audio_hook.c` (`SPA_FMT_5_1 = 1<<44`) is aimed at the wrong target for CarPlay and must become bit 42
if we pursue this — together with the AAC-decode and 8→6 fold work above. Not changed yet.

---

## 16. Is 6 the ceiling, or can the sink be 8? — SETTLED: 6, hard

We already forced the sink to 6 (`mpl5_dio_ent` max_voices 2→6). Since the CarPlay spatial transport
is 5.1.2 = **8 channels** (§15), the obvious next question is whether the same byte could be 8.
It cannot. `deva-ctrl-qc.so` caps at 6 in **three independent places**:

1. **No 8-channel device exists.** The 31-slot device table (names VA `0xA5C54`, stride 272; caps VA
   `0xA7D44`, stride 48) has max_voices ∈ {1, 2, 3, 6}. The only ≥6 entry is `mpl1_int_ent` (6/6).
   Output nodes are exactly `mono_speakerp`, `stereo_speakerp`, `six_channel_speakerp` — no 8ch node.

2. **`deva_mib2_channel_map` @0x54ACC** switches on the device's voice count with
   `case 1 / case 2 / case 3 / case 6` and a `default:` that logs
   `"deva_channel_map: voices = %d not supported"` and returns −1. There is no case 8.

3. **The CSD playback setup hardcodes 6.** `deva_csd_start_csd_session_playback` @0x24F28:
   ```c
   if (voices > 2) {                                  /* the ONLY multichannel branch */
       openAudioStreamV2(0, 2, 8*rate, 28, 32);       /* 28 = multichannel stream type */
       setStreamMultichannel        (stream, 6, &ch_map_stream);   /* literal 6 */
       setContextMultichannel       (ctx,    6, &ch_map_ctx);      /* literal 6 */
       setStreamFormatPcmMultichannel(0, stream, sr, 6, ...);      /* literal 6 */
   } else {
       setStreamFormat2(..., voices, ...);            /* 1..2 */
   }
   ```
   **Any** `voices > 2` is configured as exactly 6 channels — the count is not even read from the caps
   table. So 8ch would need real driver surgery (new CSD channel map + an 8ch output node + amp-side
   TDM/MOST slot work), not a byte.

**⇒ the 8→6 fold from §15 is mandatory**, not an optimisation. There is no 8-channel route on this HU.

### Bonus: the channel order is Apple's (open item from `deploy/deva-ctrl-qc/README.md` — resolved)
The two hardcoded channel maps in the branch above are 6-byte QCOM `PCM_CHANNEL_*` arrays
(`FL=1 FR=2 FC=3 LS=4 RS=5 LFE=6 CS=7 LB=8 RB=9`):

| map | bytes | order |
|---|---|---|
| stream/decoder (`setStreamMultichannel`, `setStreamFormatPcmMultichannel`) | `01 02 03 06 08 09` | FL FR FC LFE LB RB = **L R C LFE Ls Rs** |
| context/device (`setContextMultichannel`)                                  | `01 02 08 09 03 06` | FL FR LB RB FC LFE = L R Ls Rs C LFE |

The **stream side wants Apple canonical `L R C LFE Ls Rs`** — which is exactly what our ASBD assumes,
so no reorder is needed on our side. The driver itself re-orders to L R Ls Rs C LFE for the device.

### 2026-09-02 correction: the `min_voices` asymmetry is not harmless
`deva_mib2_channel_map` reads the **min_voices** dword (caps `+20`), not max_voices (`+24`) — every
stock device has min == max so it never mattered. Our patch set mpl5 to min=2 / max=6, so a
`SND_PCM_IOCTL_QUERY/GET_CHANNEL_MAP` on `mpl5_dio_ent` would return the *stereo* map. The old text
stopped there and incorrectly concluded that playback was unaffected. A second
consumer exists: `deva_update_session_info_from_list @0x54848` copies
`min_voices` to active CSD session channels (`+0x84`) and `min_fragsize` to its
buffer size (`+0x6c`). Leaving both minima at their valid stereo bounds therefore
also leaves a six-voice MPL5 route configured as stereo. The capability range is
correct; using its lower bounds as active geometry is the defect. See §26.

### Patch fixed: 1 byte → 4 bytes (fragment size had to move with the voice count)
Raising `mpl5_dio_ent` to 6 voices alone was **incomplete**. The record's `max_fragsize` is `0x1800`
= 6144 B, which is 1536 frames at stereo but only **512 frames at 6ch** — a 3× shorter fragment
(10.7 ms vs 32 ms @48 kHz) than any other path on this HU. The stock 6-channel device `mpl1_int_ent`
carries `0x4800` precisely for that reason, and `max_buffer` tracks it (`max_fragsize * max_frags(10)`:
`0x1800*10 = 0xF000` vs `0x4800*10 = 0x2D000`). So the patch now mirrors mpl1 on all three:

| field | file off | stock | patched |
|---|--:|--:|--:|
| max_voices | 0xA6E1C | 2 | **6** |
| max_fragsize | 0xA6E24 | 0x1800 | **0x4800** |
| max_buffer | 0xA6E28 | 0xF000 | **0x2D000** |

Both **min** fields stay stock — every edit only *widens* the accepted range, so stereo with the hook
off is unchanged. `deploy/deva-ctrl-qc/patch_deva.sh` verifies against the live mpl1 record and
asserts exactly 4 differing bytes.

Two earlier worries are now closed, both negative:
- **Mixer coupling: none.** `build_playback_mixer` (inlined in `deva_setup_mixer` @0x3C248) switches on
  `mix_config[6]`, hardcoded to **1** by `deva_pcm_and_mixer_device_init_platform_mib` @0x5707C. The
  card mixer is one volume/mute tree; it never reads a device's voice count.
- **The `channel_map` ioctl path is unreachable, but the field is not inert.** The only wrapper for that ioctl is
  `snd_pcm_query_channel_map`, and a grep of the whole MU1316 image finds **no importer** — the symbol
  exists only inside `libasound.so.2`. Independently, route setup consumes the same
  minimum as active playback geometry; §26 supersedes the earlier negative conclusion.

---

## 17. What the Dolby Atmos capability actually does on the phone (it is a CONTENT switch, not a transport switch)

Natural follow-up to §15: if CarPlay never carries EC3, what is `dolbyAtmosAudioSupported` for? Traced
in iOS 27 `MediaToolbox` + `AirPlaySupport`. **It selects a different asset, not a different wire format.**

### The chain (MediaToolbox)
1. **`FPSupport_GetAtmosSupport(audioInfo, …, &canPassthrough, &supported)` @0x196E415B8** reads Atmos
   support as a **property of the current audio output device** (or, for AirPlay, of the
   `FigBufferedAirPlayOutput` object). Route capability in, two booleans out — the ones behind the log
   line `ac3IsDecodable:%s ec3IsDecodable:%s atmosIsDecodable:%s, … atmosCanPassthrough:%s`.
2. **`itemfig_isAtmosSupported` @0x1970D46DC** combines them per media item:
   route-can-passthrough **OR** `itemfig_isSpatializationAllowed` (the phone can render it itself)
   **OR** `FPSupport_GetMaxPhysicalOutputChannelCount` is non-zero → then `itemfig_isTrackDecodable`.
3. If that says yes, **HLS alternate selection picks the Atmos rendition**:
   `FigAlternateSelectionUtility_AlternateListHasATMOSAlternate` @0x196E2F9A0,
   `FigAlternateHighestAtmosBitrateFilterCreate` @0x196CFDB74 (`HighestAtmosBitrateFilter`),
   driven by the `AutoSwitchAtmosVariants` preference (`fpfsi_handleAutoSwitchAtmosVariantsChanged`).

So the flag literally means *"this output deserves the Atmos master"* — the player fetches a different,
higher-tier variant of the track. It never changes what goes over the link.

### For CarPlay it is branch 2 that fires
The car cannot take an Atmos bitstream — there is no EC3 bit in the screen-receiver mask at all (§15).
So the phone takes the Atmos master, **renders it on-device**, and ships the result as
`AAC_ELD/48000/5.1.2` (bit 42) on this old screen-receiver branch. **Dolby = the source tier;
AAC 5.1.2 = the wire.** That is exactly why
the press always pairs "CarPlay Spatial Audio" with "powered by Dolby Atmos" (§13) — they are two
different layers, not two names for one thing.

### What the accessory's declaration does NOT do
`APAudioFormatIsScreenSpatialAudioReceiver` @0x22669B1F4 — the gate that adds bit 42 — is
`APSSettingsIsFeatureEnabled(<feature>)` **plus a device-model check**, nothing else. The car's
declared capability never enters it. Likewise `APAudioFormatAtmosIsAllowedForCurrentDeviceAsSender` is
a `dispatch_once` device-level allowance, and `APAudioFormatFilterFormatsForSpatialSender` gates on
`APSIsAPMSpeaker` + `APAudioFormatGetBufferedPassthroughSupportedFormats` — the **buffered**
(HomePod-class) path, not ours.

### The badge is derived, not declared
`APSBadgingFormatInfoCopyBadgeType` @0x22663D760 picks the "Dolby Atmos" / "Dolby Audio" /
"Apple Spatial Audio" label (`kAPSBadgingFormatInfoType_*`) from the **actual codec fourCC + channel
count** of the running stream. Declaring a capability does not paint a badge; carrying the format does.

### Not traced (honest gap)
The hop from CarKit `CRVehicle.supportsDolbyAtmos` / `CARSessionConfiguration.dolbyAtmosAudioSupported`
(+22) into the CoreAudio route property that `FPSupport_GetAtmosSupport` reads. CarKit itself never
calls `setSupportsDolbyAtmos:` — the consumer lives outside it. The architecture above is proven; that
one link is inferred.

---

## 18. "Declare Dolby but hand over only PCM 5.1" — impossible; the transport vocabulary is AAC-only

The full body of `APAudioFormatSelectTransportFormatForAudioSourceInputFormat` @0x22669C568 settles it.
`a1` = the source's input format class, `a2` = the **supported/transport mask**, `*a3` = the chosen wire
format. Every branch picks a bit **out of `a2`**, and the complete set it can ever emit is:

| branch | tries, in order | bits |
|---|---|--:|
| early-out (stereo, no high bits) | `0x400000` | 22 |
| 5.1.2 source | `0x10000000000` then `0x40000000000` | 40, 42 |
| 5.1 source | `0x8000000000` then `0x20000000000` | 39, 41 |
| stereo | `0x200000` (settings-gated) then `0x800000` | 21, 23 |

**Bits 21, 22, 23, 39, 40, 41, 42 — every one of them AAC.** There is no branch that can emit a PCM bit
(44/38/47) or a DDP/EC3 bit (50/53). Our `SPA_FMT_5_1 = 1<<44` is not merely absent from the
screen-receiver mask (§15) — it is absent from the selector's entire vocabulary.

The fallback is a strict ladder with a hard floor:
`5.1.2 → 5.1 → stereo → APSSignalErrorAt("Supported format does not include AAC-LC 48k stereo format")`.
Each step logs *"Supported format does not include AAC-ELD/AAC-LC 48k 5.1[.2] format. Checking next…"*.

### So: declaring Dolby while offering only PCM 5.1 changes nothing
1. The Dolby declaration adds no bits to `a2`. `a2` is (screen-receiver mask) ∩ (what we advertise), and
   the screen mask is fixed by feature-flag + iPhone model (§17) — our declaration never enters it.
2. Our PCM bit can never be selected, so the ladder falls all the way to **stereo AAC** — exactly what
   we get today, with or without the Dolby claim.
3. The one thing the claim *can* do is the §17 content switch: make the player fetch the **Atmos master**
   and then fold it down to stereo for the wire. More data, more battery, no benefit, and a fold-down
   instead of the native stereo master. Strictly a downside.

**⇒ There is no shortcut.** Discrete multichannel over this branch requires bit 42 *and* AAC-ELD multichannel
decode *and* the 8→6 fold (§15/§16). Advertising a capability we cannot honour only risks the classic
advertised-but-unhandled failure mode.

---

## 19. CORRECTION to §15/§18 — the bit table, what we actually advertise, and a contradiction

Before planning any implementation I rebuilt the format table from **our own** libairplay 210 instead of
trusting the iOS-side reading. `AirPlayAudioFormatToASBD` @0x2bda0 decodes format→ASBD with literal
cases; those literals pin the bit indices exactly (cross-check of all 16 PCM cases against the ordered
descriptor strings: **PASS**).

| bits | family | notes |
|--:|---|---|
| 2–17 | **PCM**, 16 combos | bit 11 = `PCM/44100/16/2`, bit 15 = `PCM/48000/16/2` |
| 18–21 | ALAC | bit 21 = `ALAC/48000/24/2` |
| 22–23 | AAC-LC | bit 22 = 44.1, bit 23 = 48k |
| 24–25 | AAC-ELD | bit 24 = 44.1, bit 25 = 48k |
| 39–42, 44… | post-210 additions | the multichannel ones (§15) |

This re-reads the §18 selector correctly: its stereo branch tries bit 21 = **ALAC 48k** (settings-gated
— lossless preference) then bit 23 = **AAC-LC 48k**, which is exactly what its error string says. Good.

### What our HU advertises today
```c
AudioSessionGetSupportedInputFormats()  { return 16; }          // bit 4  = PCM/16000/16/1 (mic)
AudioSessionGetSupportedOutputFormats(t){ return GetInput() | (t==101 ? 16 : 0x8000); }
```
→ main (type 100) = bits {4, 15} = **`PCM/48000/16/2`**. And `AudioConverterNew_compat` @0x3ddf0 is a
**dead stub** (`return -6735`), while ToASBD rejects every ALAC/AAC bit — so this stack is genuinely
PCM-only, exactly as [[audio_210_pcm_passthrough]] said.

### The contradiction — and what it invalidates
`APAudioFormatGetSupportedTransportAudioFormatsForScreenReceiver` returns `0x1440800` =
{11 `PCM/44100/16/2`, 18 `ALAC/44100/16/2`, 22 `AAC-LC/44100/2`, 24 `AAC-ELD/44100/2`} — all 44.1 kHz.
Intersect that with what we advertise, {4, 15}: **empty**. Yet CarPlay audio demonstrably works on this
unit today. Therefore **that function is not the gate for our main audio stream** — it governs some
other (newer, AirPlay-audio-over-CarPlay) path.

Consequences, stated plainly:
- **Still solid:** bit 42's identity (`AAC_ELD/48000/5.1.2`) and that it is the one bit iOS 27 adds when
  spatial is permitted; that our stack cannot decode AAC or ALAC at all; what we advertise today.
- **NOT established (downgrade §15/§18):** that bit 44 (`PCM/48000/16/5.1`) "can never be selected", and
  that the transport vocabulary is "AAC-only". Both were inferred from the mask above, which provably
  does not describe our stream. **Bit 44 may still be live for the PCM path.**

### Resolving it costs one connect, no code, no deploy
The probes already in `audio_hook.c` print both halves:
```
getSupportedFormats(type=100): stock=0x... offered=0x...   <- confirms {4,15} and our +bit44
ToASBD(fmt=0x...) => ...                                    <- the format iOS actually chose
```
Read those two lines on one phone connect and the branch is decided:
- **negotiated = a PCM bit** → the PCM path is the live one; try bit 44 for real (deva patch + hook are
  already aimed there), and only the fold/decode questions of §15 fall away.
- **negotiated = an AAC/ALAC bit** → impossible on this stack (stub converter), so it would mean the
  audio rides a different component than we think — re-open the trace there.
Nothing else should be built until that line is read.

---

## 20. Amp-side reality (B&O 3D Advanced) — settles the target at discrete 5.1, and retires 5.1.2

Owner-supplied spec + our RE agree; recording it so it stops being re-litigated.

**The amp is already a 5.1 system.** B&O 3D Advanced: 16 MOST channels, 19 speakers, 5.1 surround +
Fraunhofer Symphoria 3D. Nothing to "force" — it *is* the 5.1 target.

**What it does today with CarPlay.** The HU sends **stereo** over MOST; the amp synthesises the field
across all speakers with Symphoria. That is an **algorithmic upmix, not discrete channels** — the amp
cannot manufacture discrete 5.1 from stereo, only spread it. Discrete 5.1 reaches it only when the
*source* actually delivers 6 channels (internal media / DVD modes).

**⇒ the win we are chasing, stated precisely:** replace a synthetic upmix of a stereo downmix with the
phone's genuine discrete 5.1. Same speakers, real channels.

### 5.1.2 / Atmos height is out — three independent stops
1. **No height source.** 5.1.2 is object/height content; nothing on the B9 platform (~2016) carries a
   height layer over MOST from MIB2. Firmware cannot create channels absent from the input.
2. **The amp is not an Atmos renderer.** The SHARC ROM has legacy Dolby/DTS decoders, not an object
   renderer. It is upmixer + EQ/crossover + power stage.
3. **"3D" here is Symphoria synthesis, not a discrete top layer.** The height drivers (pop-up tweeters /
   dash 3D) are already fed — by the upmix, not by a separate Atmos track.

This also reframes §16: the 8→6 fold would only ever be needed if the *phone* offered nothing but bit 42
(AAC 5.1.2). Folding then discards exactly the height layer the amp cannot render discretely anyway —
so the fold is a cost on the phone-facing side, **not a musical loss**. It is not, and never was, a
delivery target.

### Convergence with §19
Both lines now point the same way: the ideal wire format is **PCM 48k/16/6ch — bit 44** — because it is
a native match end to end (6ch PCM in → `six_channel_speakerp` → MOST → amp), needs **no decoder and no
fold**, and lands exactly on what the amp wants. That is what the current hook already targets, and §19
reopened the possibility that it is live. The pending measurement decides it.

### The one open item left on this boundary
**How does the amp learn to stop upmixing?** Checked the HU side: `app/` contains no Symphoria / upmix /
sound-mode control at all — the only channel signalling is Qualcomm AFE-level `ChannelConfig` per port
(`audio_service`), which is DSP-internal and not amp-facing. So the switch is most likely **implicit in
the MOST channel count**, and `mpl1_int_ent` (6 voices, internal media) is the existence proof that a
6-channel stream reaches the amp and is used as such.

The failure mode to watch on the first live test: if the amp instead picks discrete-vs-upmix by **source
identity** (media source ⇒ expect 5.1, phone source ⇒ expect stereo), our 6 channels could be
down-mixed or partly ignored even though they arrive. Listen for a real centre/LFE, not just width.

---

## 21. RESOLVES §19 — iOS 27 has a SECOND CarPlay audio stream (buffered "media"), and that is where lossless/spatial live

Chasing "declare Atmos to get the Atmos master, render it to 5.1, hand us PCM 5.1" turned up the piece
that explains everything. `AirPlaySender` (the phone-side parser of **our** /info) has two format keys:

```
0x2511d54cd  "audioOutputFormats"           <- what our libairplay writes today
0x2511d5199  "audioOutputFormatsExtended"   <- NEW, we have never published it
```

`airPlayDescription_copyCarPlayAudioFormatsExtended` @0x25109C5DC:
```c
if ( settingsEnabled(<feature>, 1) && *(BYTE *)(desc + 65) ) {          // feature flag + a desc flag
    APCarPlayAudioFormatsCopyFormatsForAudioType(formats, 103, "media", 0, &out, 0);
    dict = CFDictionaryCreateMutable(...);
    CFDictionarySetValue(dict, "bufferStream", out);                    // ← the tell
}
```
(Key CFStrings resolved from the constant structs: `"bufferStream"`, `"media"`, and for
`carPlayAudioFormat_getFormatsKeyForAudioType` @0x25107B5C0: type 107 → `audioInputFormats`,
type 100 → `audioInputFormats`/`audioOutputFormats`, else → `audioOutputFormats`.)

**Audio type 103, sub-key `media`, published under `bufferStream`.** Strongly indicated reading: iOS 27
carries a **buffered media stream** over CarPlay, separate from the classic realtime audio stream — and
that is the high-quality music path.

### This dissolves the §19 contradiction
Everything that refused to line up now does:
- `GetSupportedTransportAudioFormatsForScreenReceiver` returning **44.1 kHz PCM/ALAC/AAC-LC/AAC-ELD** —
  those are *music* formats. It describes the buffered media stream, not our realtime stream. Hence the
  empty intersection with our 48 kHz realtime advert, while CarPlay audio still works.
- `APAudioFormatSelectTransportFormatForAudioSourceInputFormat` emitting only ALAC/AAC (§18) — it is the
  buffered-music selector, with ALAC 48k as the settings-gated lossless preference.
- `APAudioFormatFilterFormatsForSpatialSender` gating on `APSIsAPMSpeaker` +
  `GetBufferedPassthroughSupportedFormats` (§17) — buffered again.
- Why a shipping car (Volvo EX90, §13) gets CarPlay spatial at all: it implements this stream.

### What that means for "force Atmos, take PCM 5.1"
The idea is sound in mechanism but the two halves sit on **different streams**:

| half | stream | cost |
|---|---|---|
| Atmos-tier **content** | buffered media stream (type 103, `audioOutputFormatsExtended` → `bufferStream`) | **large** — a stream type our 210 libairplay has no notion of |
| PCM 5.1 **transport** | classic realtime stream (type 100, `audioOutputFormats`, bit 44) | small — hook + the deva patch we already built |

So we cannot simply declare Atmos and have the master arrive rendered on the realtime stream. Getting
the Atmos content tier means implementing the buffered stream. Conversely the cheap PCM-5.1 lever stays
on the realtime stream — where the content is whatever iOS renders live, today stereo.

**The open question this creates, and it is the right one to measure:** does iOS render *multichannel*
into the realtime stream when the receiver advertises a multichannel PCM format? No evidence either way
yet. If yes, bit 44 alone gets us discrete 5.1 (§20's target) with no buffered-stream work at all.

### Order of work is unchanged
Still one connect with the existing probes first (§19). It now also answers a second question for free:
whether the phone ever asks us for `audioOutputFormatsExtended` / a type-103 stream.

### 21a. Sample rate: the realtime CarPlay stream is 48 kHz, and our hardware is 48 kHz ONLY

Worth pinning, because §21's two streams differ on rate and it changes the cost of each.

From the deva caps table (§16 dump), the rates field is a QNX `SND_PCM_RATE_*` bitmask
(bit 3 = 16000, bit 7 = 48000 — cross-checked on two devices: `ann3_dio_ann` mask `0x8` @16 kHz,
`mpl5_dio_ent` mask `0x80` @48 kHz):

| device | rates mask | min_rate | max_rate |
|---|--:|--:|--:|
| `mpl1_int_ent`, `mpl3`, `mpl4`, **`mpl5_dio_ent`**, `mpl6`, `mpl7`, `tel1`, `sse`, `ent1`, `mic*` | `0x80` | 48000 | 48000 |
| `ann1_int_nav`, `ann2_int_sds` | `0x1000` | 24000 | 24000 |
| `ann3`…`ann11`, `tel2`…`tel5` | `0x8` | 16000 | 16000 |

**Every media device is a single fixed rate, and for the CarPlay device that rate is 48000 — there is no
44.1 kHz bit anywhere in the table.** Consistent with what we advertise (bit 15 = `PCM/48000/16/2`) and
proven by existence: CarPlay audio works today, so the realtime stream is 48 kHz.

Consequences for the two streams:
- **Realtime (type 100), 48 kHz — native.** Bit 44 is `PCM/48000/16/5.1`: same rate, same bit depth,
  just 6 voices. Nothing to resample; the deva patch is the only HU-side change. This is why the cheap
  lever is cheap.
- **Buffered media (type 103) — 44.1 kHz base.** `GetSupportedTransportAudioFormatsForScreenReceiver`'s
  base set is all 44.1 kHz (Apple Music masters). Our media devices cannot open 44.1 at all, so that
  path would additionally need resampling in dio/libairplay before deva. Another cost on top of §21's
  "implement a stream type we don't have". (Note the spatial addition, bit 42, is 48 kHz — Atmos masters
  are 48k — so only the *base* music formats are the mismatch.)

---

## 22. The realtime path is ASBD-based, not bitmask-based — which makes "Atmos master → PCM 5.1" the best plan we have

`APAudioFormatSelectorRealTimeCreate` @0x251049A1C (AirPlaySender) is the **realtime** counterpart of the
buffered selector from §18, and it works on a completely different principle:

```c
FigEndpointStreamCopyProperty(stream, <kSupportedPCMFormats>, ...);   // a LIST, from the stream
...
do {                                            // stride 0x38 = 56 bytes per entry
    Log("%@ SupportedPCMFormat[%d]: [%{asbd}]", sel, i, entry);
    if ((chosen & 1) == 0) {                    // take the first if nothing already negotiated
        *(_OWORD *)(sel +  8) = *(_OWORD *)entry;        // 40-byte ASBD copied wholesale
        *(_OWORD *)(sel + 24) = *(_OWORD *)(entry + 16);
        *(_QWORD *)(sel + 40) = *(_QWORD *)(entry + 32);
    }
    entry += 56; chosen = 1;
} while (...);
```
and `realTimeAudioEngine_setTransportAudioFormatInternal` @0x2510431C8 takes an
`AudioStreamBasicDescription *` and logs `%{asbd}` — no format bit anywhere.

**⇒ On the realtime stream the channel count is just an ASBD field.** There is no ladder of
"5.1.2 → 5.1 → stereo → error" and no AAC-only vocabulary — that was the *buffered* selector (§18/§21).
Multichannel PCM is structurally representable here, which also fits our PCM-only stack perfectly.

### So the answer to "can the phone take the Atmos master and push it as PCM 5.1?"
Not senseless — it is the **strongest** version of the plan, and it needs only the cheap lever:

1. Advertise a 6-channel PCM format (bit 44 = `PCM/48000/16/5.1`, rate-native per §21a).
2. iOS turns our advertised formats into HAL/ASBD descriptions (`APCarPlayAudioFormatsCreate`
   @0x25107B82C → `APCarPlayAudioFormatsGetHALDescriptions` @0x250F69A40) — the list the selector above
   enumerates. A 6-channel entry makes the CarPlay route a 6-channel route.
3. **The content half may then come along for free.** §17's `itemfig_isAtmosSupported` succeeds via its
   third branch, `FPSupport_GetMaxPhysicalOutputChannelCount != 0` — a multichannel route qualifies, so
   Apple Music selects the Atmos rendition (`HasAtmosAlternates` / `AutoSwitchAtmosVariants`).
4. The phone renders that master into the route's format — 6 channels of PCM — because the realtime
   stream *is* the phone's rendered output, not a passthrough of an encoded asset.

That is one change (advertise 6ch) buying both halves, on the stream we already own, at the rate our
hardware already runs, with the deva patch already built. No buffered stream, no AAC decoder, no 8→6
fold.

### What is proven vs inferred here
- **Proven:** the realtime selector is ASBD/list-based with no codec ladder; the transport format is set
  from an ASBD; our stack and the mpl5 device are 48 kHz PCM and now 6-voice capable.
- **Inferred, and the two things to verify:** (a) that bit 44 survives `APCarPlayAudioFormatsCreate` /
  `carPlayAudioFormat_repackageFormatsForStream` @0x25107C9B0 into a 6-channel HAL description for a
  CarPlay stream; (b) that the CarPlay route's channel count is what
  `FPSupport_GetMaxPhysicalOutputChannelCount` reports, so the content branch fires.

Both fall out of the same experiment: deploy the deva patch, advertise bit 44, connect, and read
`ToASBD` plus the phone's `SupportedPCMFormat[i]` enumeration.

### 22a. CORRECTION to §22 step 3 — 6 channels alone does NOT unlock the Atmos content tier

§22 claimed a multichannel route would satisfy `itemfig_isAtmosSupported` via its
`FPSupport_GetMaxPhysicalOutputChannelCount` branch. That was read off a heavily *outlined* decompile
where the decompiler had mapped shared registers to misleading names. Reading the real signature fixes it.

`FPSupport_GetMaxPhysicalOutputChannelCount(a1, int *a2, int *a3)` @0x197315E4C returns an **OSStatus**
and writes the channel count to `*a2`:
```c
if (bufferedAirPlayOutput /* a1+16 */) {
    ... FigBufferedAirPlayOutputGetCMBaseObject(...) -> query <atmos property>
    *a2 = (prop == kCFBooleanTrue) ? 16 : 2;          // 16 = a SENTINEL, not a channel count
} else if (audioSession /* a1+24 */) {
    ret = FPSupport_GetAudioSessionOutputChannelCount(session, ..., &count);
    *a2 = count;                                       // the REAL route channel count
}
```
Two consequences, one good and one bad:

- **Good, and load-bearing:** for a non-buffered route — *ours* — the count comes from
  `FPSupport_GetAudioSessionOutputChannelCount`, i.e. the genuine channel count of the current audio
  session output. So **a 6-channel CarPlay route is visible to the media stack as 6.**
- **Bad:** the call site in `itemfig_isAtmosSupported` reads
  `if (status == 0 && count == 16)` — the decompiler's `MaxPhysicalOutputChannelCount` there is the
  *return status*, and `a11` is the count. **16 is the buffered-AirPlay Atmos sentinel above, not
  "multichannel".** 6 ≠ 16, so a 6-channel CarPlay route does **not** fire that branch.

The remaining branches are `FPSupport_GetAtmosSupport` (a property on the audio *device* — i.e. the
declared capability) and `itemfig_isSpatializationAllowed` @0x1970D4BBC (which consults
`FPSupport_CopyAudioSpatializationCapabilities` / `kFigSpatialCapabilitiesKey_alwaysSpatialize`; its
decompile shows a single `return 0`, so its real result may live in an outlined tail — treat as unknown).

### The caution this raises before spending a reboot
Transport and content are now clearly separable:
- **Transport half still works:** a 6-channel route means the phone renders its output into 6 channels.
- **Content half does not come free:** with stereo/lossless source material and no spatializer engaged,
  a 6-channel route plausibly carries **L/R plus four silent channels** — which would sound *worse* than
  today, where Symphoria at least fills all 19 speakers from the stereo feed (§20).

So the experiment must listen for the right thing: not "is there sound", but **whether centre/LFE/surround
carry real, distinct content**. If they are silent, the transport works and the content tier is the
missing half — and that is when the declared-capability route (`supportsSpatialAudio` /
`supportsDolbyAtmos`, §17) becomes worth trying, on top of the 6-channel advert rather than instead of it.

---

## 23. FOUND — the two levers that set the Atmos/spatial capability, and one of them is our formats array

Disassembling `-[CARSessionConfiguration initWithSessionStatusOptions:propertySupplier:]` @0x1C2008F7C
around the ivar stores (offsets confirmed by decoding the getters: `LDRB W0,[X0,#0x15]` =
`spatialAudioSupported`, `LDRB W0,[X0,#0x16]` = `dolbyAtmosAudioSupported`):

```asm
1C20099B0  ADRL  X2, cfstr_DCXEnabled        ; CFString "DCXEnabled"  (len 10)
1C20099B8  MOV   X0, X22
1C20099BC  BL    <property lookup>
1C20099E8  TBZ   W0, #0, loc_1C20099F8       ; conditional cast (Bool?) failed -> X21 = nil
1C2009A04  CBZ   X21, loc_1C2009A18          ; nil -> fall through to the audioFormats path
1C2009A0C  BL    <get Bool value>
1C2009A10  STRB  W0, [X20,#0x16]             ; *** dolbyAtmosAudioSupported = DCXEnabled ***

loc_1C2009A18:
           ADRL  X2, cfstr_audioFormats      ; CFString "audioFormats" (len 12)
           ...
1C2009B50  MOV   W4, #0x10                   ; 16
1C2009B54  BL    <scan formats>
1C2009B64  MOV   W8, #1
1C2009B68  STRB  W8, [X20,#0x15]             ; *** spatialAudioSupported = (scan succeeded) ***
```

Two separate levers, and they work very differently:

| ivar | source | shape |
|---|---|---|
| `dolbyAtmosAudioSupported` (+0x16) | key **`DCXEnabled`** | a plain **Bool** we declare |
| `spatialAudioSupported` (+0x15) | key **`audioFormats`** | **derived** — set by scanning the formats array we already publish |

**`spatialAudioSupported` is not a flag at all — it is inferred from what we advertise.** That is the
formats array our `AirPlayReceiverServerPlatformCopyProperty` @0x1fe08 already builds (§14), the one our
hook already augments. So the spatial half is reachable through the *same* edit as the transport half.

And `DCXEnabled` is the long-missing wire key for the Dolby declaration — a Bool, not a bitmask.

### Which repairs the plan §22a broke
§22a was right that 6 channels alone will not satisfy `itemfig_isAtmosSupported` (that branch tests the
buffered sentinel `== 16`). But the branch that *does* apply to us is `FPSupport_GetAtmosSupport`, which
reads a property off the audio **device** — and the CarKit flags above are what populate the route. So
the content tier is reachable, just through declaration rather than through channel count:

1. advertise a 6-channel PCM format → transport (and, per the scan above, plausibly
   `spatialAudioSupported` too);
2. publish `DCXEnabled = true` → `dolbyAtmosAudioSupported` → the content tier;
3. phone fetches the Atmos master and renders it into the route's 6 channels.

Which is exactly the plan as originally proposed — the correction was only about *why* step 2 is
mandatory and cannot be substituted by step 1.

### Still to pin down
- **Which dictionary `DCXEnabled` must appear in.** The key name is certain; its container (X22 — the
  `propertySupplier` result vs the AirPlay /info dict fetched as `CARkAPEndpointProperty_EndpointInfo`)
  is not yet traced. That determines whether we publish it from libairplay's /info builder or elsewhere.
- **What the `audioFormats` scan actually matches** (the `W4 = 0x10` argument). If 16 is a channel-count
  or an audio-type selector, it tells us exactly which entry makes `spatialAudioSupported` true.

---

## 24. LIVE MEASUREMENT (on-unit, stock, no patch) — the static analysis is confirmed

Captured with `sloginfo -w` across a phone reconnect on the untouched unit: stock `deva-ctrl-qc.so`
(26 Feb 2018, 694904 B), `SPATIAL_5_1` commented out in `carplay_startup.sh:206`, nothing deployed.
`io-audio` runs with `log_level=high`, so all of this is **stock logging**.

### 1. What we advertise — exactly as decompiled
```
[AudioStream] <AirPlay> Supported output formats: 0X8010     (stream type 100, main)
[AudioStream] <AirPlay> Supported output formats: 0X10       (stream type 101, alt)
```
`0x8010` = bits {4, 15}, `0x10` = bit {4}. Precisely what §19 predicted from
`AudioSessionGetSupportedOutputFormats` @0x497bc (`GetSupportedInputFormats()==16`, OR `0x8000` for main
and `16` for alt). **Static reading validated on hardware.**

### 2. What CarPlay actually negotiates today
```
[AudioStream] <iPhone>Audio type: media
[AudioStream] <AirPlay> Audio Format: (48000[kHz] | 16[bits] | channels: 2)
[AudioStreamPrepare] Stream type: 100 (main) | Format = 1819304813 | Ch = 2 | SR = 48000 | BPS = 16
[CQNXSoundIF] [open]<AUDIO> Open the path: "mpl5_dio_ent"(direction: 1)
DEVA: deva_aquire: ado_pcm_t found at device_id = 4 <mpl5_dio_ent>, stream_type=2
```
Format `1819304813` = `'lpcm'`. **The realtime CarPlay stream is raw PCM, 48 kHz, 16-bit, stereo — i.e.
bit 15 — on `mpl5_dio_ent`, device_id 4.** Exactly the device (and index) the deva patch targets.

### 3. The 6-channel chain is live on this unit RIGHT NOW
```
DEVA: session_csd_setup: Setting up session while DEVICE_ID = <1> is being attached
DEVA: session_csd_setup: pcm format => sample_rate=48000, channels=6, bit_per_sample=24
```
Device 1 = `mpl1_int_ent`. So discrete 6-channel 48 kHz playback into `six_channel_speakerp` → MOST → amp
is not an inference from the caps table any more — it was **observed running**, minutes before the
CarPlay session. §20's "the win is joining that path" is now measured, not argued.

### 4. The fragsize fix is proven necessary (not speculative)
```
[AudioStream] [thread]<AUDIO>[kAudioStreamType_MainAudio]<Tx> nTFrames: 768, nTxFramesInBytes: 3072
```
768 frames × 4 B/frame (2ch × 16-bit) = 3072 B. **At 6 channels the same 768 frames are 9216 B — larger
than mpl5's stock `max_fragsize` of 6144.** The 0x1800 → 0x4800 correction in §16a/`patch_deva.sh` is
therefore load-bearing, not tuning: without it a 6-channel transfer cannot fit a fragment.

### 5. The phone never asked for the extended/buffered stream
No `Extended`, no `bufferStream` anywhere in the capture — consistent with our /info not publishing
`audioOutputFormatsExtended` (§21). Caveat on that section: `Audio type: media` appears here on the
**realtime** stream type 100, so the string `"media"` alone does not prove the buffered path; the
distinct `bufferStream` key still does.

### Baseline established — what a successful experiment must change
| line | today | on success |
|---|---|---|
| `Supported output formats` | `0X8010` | `0X8010` + our multichannel bit |
| `Audio Format` | `48000 \| 16 \| channels: 2` | `channels: 6` |
| `session_csd_setup: pcm format` (device 4) | `channels=2` | `channels=6` |
| `nTxFramesInBytes` | 3072 | 9216 |

---

## 25. LIVE RESULT — the realtime CarPlay stream refuses multichannel, whatever we advertise

Full experiment on the unit: patched deva (6 voices, 0x4800 fragsize, 0x2D000 buffer, verified
byte-identical), new hook, `SPATIAL_5_1=1`, verbose logging fixed. **The interposition provably works** —
the hook's own probes show our augmented mask reaching the negotiation:

| tuned bit | advertised (`getSupportedFormats` type 100) | phone selected (`ToASBD`) |
|---|---|---|
| 44 — `PCM/48000/16/5.1` | `0x8010` → **`0x100000008010`** | `0x8000` = bit 15, **stereo** |
| 42 — `AAC_ELD/48000/5.1.2` (the bit iOS itself adds for the old spatial gate) | `0x8010` → **`0x40000008010`** | `0x8000` = bit 15, **stereo** |

Both refused. Not our bug: `ARMED`, the `+bit` in the offered mask, and the phone's own selection all
appear in the same log. **Advertising a multichannel format on the realtime stream is not sufficient —
iOS simply does not pick it.**

Two hypotheses remain, and they are not mutually exclusive:
1. **The phone does not qualify for spatial at all.**
   `APAudioFormatIsScreenSpatialAudioReceiver` @0x22669B1F4 is `APSSettingsIsFeatureEnabled(<feature>)`
   **plus a device-model check** (§17). If either fails, bit 42 is absent from the phone's transport mask
   and no advert of ours can put it back. This depends on the handset and its iOS build — our RE is
   against an iOS 27 shared cache, and a phone on an older build may not carry the path at all.
2. **Multichannel lives only on the buffered media stream** (§21): `audioOutputFormatsExtended` →
   `bufferStream` → audio type 103, which we do not implement and which is 44.1 kHz-based (§21a).

What this closes: the cheap lever is exhausted. The deva patch, the 6-channel chain (§24 saw it running
on `mpl1_int_ent`), the ASBD emission and the announce rail are all correct and in place — the missing
piece is entirely on the phone's side of the negotiation.

What it does NOT close: the declaration route (§23, `DCXEnabled`) was never tested. But per §17 that flag
drives the *content* tier, not the transport — with the transport refusing multichannel it cannot help
on its own, so it is only worth wiring once a multichannel transport is actually selectable.

### Safety note that earned its keep
Probe-only mode (`<bit>:0`) advertises a bit but refuses it in `ToASBD` if selected. Without it, a
selected bit 42 would have had our 16-bit `lpcm` ASBD describing AAC wire data — full-scale noise into
a 19-speaker amp. It cost five lines and should stay the default way to test any new bit.

### 25a. The gate has a name: `ScreenSpatialAudioReceiver`

The handset is an **iPhone 17 Pro Max on iOS 27 beta 6** — newest hardware, and the exact OS our RE
targets. So "the device is too old" is not the explanation. Resolving the CFString constant in
`APAudioFormatIsScreenSpatialAudioReceiver` @0x22669B1F4 gives the real gate:

```c
if (!APSSettingsIsFeatureEnabled(CFSTR("ScreenSpatialAudioReceiver"))) return 0;   // <-- the gate
v1 = <device identifier>();
if (v1 != 0x5BD62321 && v1 != 0xA10F2923 && v1 != 0xFCBBE892)
    return <fallback check>() != 0;
return 1;
```
The three constants are not FourCCs (no printable ASCII) — hashed product identifiers, most likely; and
note that a non-matching device can still qualify through the fallback. **The dominant gate is the
feature flag, and it takes no arguments** — it is purely phone-side state, upstream of anything the
accessory advertises. Nothing we publish in /info can switch it on.

That explains every observation at once: our bit reaches the negotiation (proven in our own log), and
iOS never even considers it, because `GetSupportedTransportAudioFormatsForScreenReceiver` returns the
non-spatial mask for this device.

### Where that left the project at the time

The bullets formerly here concluded that the HU side was finished, leaving the
path armed was harmless, and only the buffered stream remained. Those were the
conclusions of the realtime-negotiation experiment, not final rendering facts.
They are superseded by the standalone two-voice/six-voice vehicle test and §26.
~~The current repository intentionally keeps `AUDIO_SPATIAL=0`.~~ **Superseded 2026-09-02:**
the CSD geometry defect is fixed and confirmed on the car, so `AUDIO_SPATIAL=1` is the default
again. See `deploy/deva-ctrl-qc/BUILD_CHAIN.md`.

---

## 26. CORRECTION — MPL5 widens its DMA stream but leaves the routed CSD consumer stereo

The earlier conclusion that the HU path was complete is superseded by a direct
vehicle measurement and a static route audit. A standalone QSA player sent the
same known-good material directly to `mpl5_dio_ent`, with CarPlay, libairplay,
APAC, the hook, and its ring absent. At two voices it sounded excellent and
delivered 2,445,312 frames in 50,831 ms (**48,107 frames/s**). At six voices it
negotiated the correct geometry and delivered the same frame count in 50,807 ms
(**48,129 frames/s**), yet produced the same recognisable, low-pitched,
continuously ticking audio as CarPlay. Both are against 48,000 expected. Six
correlated channels failed just like discrete 5.1, eliminating channel
separation as the cause.

MPL1 and MPL5 both route through output 128 and CSD device id `0x4e`/78, but
their effective input widths are selected from different 48-byte MIB records:

```text
mpl1 / timer 1: min_voices=6, min_fragsize=0x4800
mpl5 / timer 4: min_voices=2, min_fragsize=0x1800
```

Those are valid capability lower bounds, but
`deva_update_session_info_from_list @0x54848` incorrectly treats them as the
active format when a timer is attached to the shared CSD session:

```text
record +0x14 (min_voices)   -> session +0x84 (active channels)
record +0x1c (min fragment)-> session +0x6c (CSD buffer bytes)
```

The current binary's helper at `0x8f2c8` independently sees negotiated
`channel+0x10 == 6` and changes the DEVA timer/DMA fragment to `0x4800`.
Nothing in acquire, prepare, or trigger rebuilds the already-created CSD
stream/context/buffer pool. The resulting six-voice run therefore has a
six-channel `0x4800` producer but a stereo `0x1800` consumer.

`session_csd_stream_playback @0x69084` copies and submits exactly the session's
`0x1800` bytes before the timer advances the DMA cursor by `0x4800`. It consumes
only 256 of each 768 source frames, skips 512, and interprets successive source
words as stereo pairs. With correlated six-channel input, each sample becomes
three adjacent stereo frames before the next 16-ms block jumps forward. Total
wall-clock duration remains correct, while the repeated samples lower the local
pitch and the jump creates a **62.5-Hz** seam. This specifically predicts every
observed symptom.

The smallest controlled six-channel diagnostic is to change both MPL5 minima:

```text
file 0xa6e18 / VA 0xa7e18: min_voices   2      -> 6
file 0xa6e20 / VA 0xa7e20: min_fragment 0x1800 -> 0x4800
```

Both are required, and neither is a production fix: they remove the clean
stereo mode. Correct dual-mode behavior must take channels and fragment size
from the live PCM channel (`timer+0x11c`, then channel `+0x10/+0x94`) and rebuild
the CSD stream, context, and buffer pool on every 2/6 transition before the
timer starts. Route-before-open must be handled; overwriting fields after
allocation is not sufficient.

This mismatch is statically proven and is the leading root cause. The isolated
QEMU image cannot execute MPL5's Audioman-created route, so acoustic causality
still needs the controlled six-only A/B. Full addresses, table records, call
chain, harness scope, and fix constraints are in
[`../ROUTER_INPUT_WIDTH_AUDIT.md`](../ROUTER_INPUT_WIDTH_AUDIT.md).
