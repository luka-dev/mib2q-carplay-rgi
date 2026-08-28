# Forcing CarPlay Dolby Atmos + Spatial audio — feasibility & plan

Goal: make the iPhone push multichannel (Spatial) and/or Dolby Atmos audio over WIRED CarPlay to
this MHI2Q head unit. Feasibility investigated end-to-end 2026-08-28.

## Verdict: feasible in principle. The gap is our stack predates the format bits.

### Proven facts
1. **AirPlay is receiver-advertised**: the phone (sender) only sends audio formats the HU (receiver)
   advertises in its supported-format bitmask. So OUR advertised list is the lever.
2. **iOS 27 defines multichannel formats** (dyld cache blob .36, the AirPlay format table):
   `PCM/48000/16/5`, `PCM/48000/16/7`, `PCM/48000/16/9`, `AAC/48000/5`, `AACHE/48000/5`,
   **`EC3/48000/7`**, `PCM/48000/32`.
   - **Spatial = multichannel PCM** (5/7/9) — discrete channels, no codec licence.
   - **Dolby Atmos = `EC3/48000/7`** — E-AC-3 (DDP JOC).
3. **Our stack lacks these bits**: stock libairplay 210.81 enum tops at `AAC-ELD/48000/2` (bit
   0x2000000); from-source 280.33.8 `kAirPlayAudioFormat_*` tops at stereo + `Reserved1/Reserved2`.
   The multichannel bits were added by Apple after 280.33.8.
4. **The hardware/output can do it**: `/dev/snd/six_channel_speakerp` (5.1) exists; Cinemo
   (`libNmeAudioDevice`) and retroarch (`qnx_qsa.c`) both prove writing N-channel PCM to the sink via
   QSA. Dolby: `libNmeAudioDolby.so` (DDP/E-AC-3 decoder, licence-gated) is present for the EC3 path.
5. **No wired/wireless gate**: carkitd + CarKit store `spatialAudioSupported`/`dolbyAtmosAudioSupported`
   from the accessory config without a transport condition; Apple's Ultra spatial CarPlay is wired.

### libairplay injection points (already located, stock .so)
- `_UpdateStreams` (sub_1DFE8): builds Main/Alt streams; format→ASBD via `AirPlayAudioFormatToPCM`.
- `AirPlayReceiverSessionSetup` (0x272b8): builds the advertised format list + negotiates.
- `AirPlayAudioFormatToPCM` (0x1b2bc): format enum → PCM ASBD — **channelsPerFrame lives here**.

## Plan

### Phase 0 — extract the wire bits (cheap, decisive; blocks everything)
From iOS 27 blob .36 (the AirPlay format-table binary), recover the **bitmask bit value** for each
multichannel format string (`PCM/48000/16/6`, `AAC/48000/5`, `EC3/48000/7`, …). These are the exact
values to advertise. Also confirm CarPlay (not just AirPlay→ATV) offers them and whether
`spatialAudioSupported` in the config is required to unlock them.

### Phase 1 — Spatial (multichannel PCM) — the clean win, no Dolby licence
1. from-source libairplay: add the multichannel PCM format(s) to the advertised list (SessionSetup)
   using the Phase-0 bit; give `kAirPlayAudioFormat_*` the new enum entry.
2. Extend `AirPlayAudioFormatToPCM` to emit a 6/8ch ASBD for it.
3. Route the multichannel PCM stream to `six_channel_speakerp` via QSA (mirror Cinemo/retroarch).
4. Advertise `spatialAudioSupported` in `CARSessionConfiguration` — same SetProperty path as altScreen.
5. Verify the amp accepts discrete 5.1 from the CarPlay (dio) source on the unit.

### Phase 2 — Dolby Atmos (EC3/48000/7) — superset, adds the licensed decoder
1. Advertise `EC3/48000/7`; set `dolbyAtmosAudioSupported` in the config.
2. Route the EC3 bitstream to `libNmeAudioDolby` (DDP decoder) → multichannel PCM → sink.
   Requires the Dolby licence active and integrating the Cinemo decoder into the CarPlay audio path
   (libairplay does not decode EC3 itself).

## Build strategy: from-source, not binary patch
Adding enum values + a new `AirPlayAudioFormatToPCM` case + stream routing is source-level work — do
it in the **from-source libairplay (280.33.8)**, not by patching the stock 210.81 `.so`. The config
flag rides the **dio patch** we already use to advertise altScreen. Recommend Phase 0 → Phase 1
(spatial PCM) first; Atmos (Phase 2) layers the Dolby decoder + licence on top.

---

## Phase 0 progress (2026-08-28) — format bits

**Confirmed:**
- iOS 27 AirPlay format table (dyld cache subcaches `.36`/`.48`) contains the multichannel + Atmos
  format strings: `PCM/48000/16/5`, `PCM/48000/16/7`, `PCM/48000/16/9`, `AAC/48000/5`,
  `AACHE/48000/5`, `EC3/48000/7`, `PCM/48000/32`.
- **`AirPlayAudioFormat` is a `uint64_t`** (from-source `AirPlayCommon.h`). Classic formats occupy
  bits 0–30 (Reserved1/2 = bits 0/1; … `OPUS_48KHz_Mono` = bit 30 = 0x40000000). **Bits 31–63 are
  free** — Apple's multichannel/Atmos formats live there.
- Our from-source **280.33.8 already uses the 64-bit field** = correct build base. Stock 210.81's
  `_UpdateStreams` switch uses a 32-bit `int` for the format (tops at bit 25, AAC-ELD/48000/2) —
  another reason to build from-source, not binary-patch stock.

**Still needed (the exact bit numbers):** the specific bit index Apple assigned to each new format
(bits 31+). These are in the iOS 27 AirPlay binary's `AudioStreamDescriptionToString` /
`StringToAudioStreamDescription` (subcache `.36`/`.48`). `ipsw dyld extract` returned stub dylibs
without the cstring table, so the clean way is a full-cache IDA load (idalib handles dyld caches) or
a dyld extraction that preserves the format table, then read the enum/ternary. Once we have the exact
bits, Phase 1 can advertise them.

**Owning binary hunt**: not AirPlaySenderKit / AVConference / CoreUtils / CarPlay / CoreMediaStream
(extracted, none carried the format strings — extraction dropped shared cstrings). String lives at
byte offset 0x630E38A in subcache `.36`. Next: resolve that offset's owning dylib via the cache map,
extract it intact, read the table.
