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

### Phase 0 — exact bits: tooling wall (2026-08-28)
Reading the exact multichannel bit numbers from the iOS 27 AirPlay format table is BLOCKED in this
environment:
- Ghidra headless (7.0.0): **OOM (Java heap)** importing the full cache; MCP import can't select a
  single module from a dyld cache.
- IDA idalib: can't open the full `dyld_shared_cache_arm64e` in one shot.
- `ipsw dyld extract <dylib>`: produces a valid Mach-O but **drops the shared __cstring table** — the
  extracted CoreUtils/CarPlay/AVConference/CoreMediaStream/AirPlaySenderKit carry neither the format
  strings nor the table code (no immediate `0x2000000` in CoreUtils → `AudioStreamDescriptionToString`
  is not in that build). `o2a`/`str` either hang on indexing or are subcache-ambiguous.
- The string lives in subcaches `.36`/`.48` (byte offset ~0x630E38A in `.36`).

To finish cleanly later: bump Ghidra `-Xmx` + use its single-module dyld loader, or extract with a
tool that folds shared cstrings (`dyldextractor`, newer ipsw), then read the enum/ternary.

**Chosen path: get the bit by on-device probe in Phase 1.** Field is uint64, bits 0–30 known/full;
multichannel is a few bits at 31+. In from-source libairplay advertise a candidate multichannel
format at a probe bit and read the phone's negotiated format back from the RTSP/SETUP exchange; a
couple of tries pins the exact bit. Treat the bit as a `#define` TODO until then.

---

## Implementation strategy: HOOK stock libairplay (not from-source)

Decision: implement as an LD_PRELOAD hook of stock `libairplay.so` (210.81), like our dio hook — NOT
a from-source rebuild.

### Hookability (verified in the stock .so)
All target functions are exported (T) -> interposable via LD_PRELOAD (internal calls go via `j_`
thunks / PLT):
- `AudioSessionGetSupportedOutputFormats` @0x497bc — returns __int64 (64-bit capability mask; can
  carry multichannel bits).
- `AirPlayReceiverSessionSetup` @0x272b8 — builds advertised format list + negotiates.
- `AirPlayAudioFormatToPCM` @0x2c308 — format->ASBD; channels land at a3[6]. Takes `int a1` (32-bit).
- `AudioStreamCreate` @0x49d38, `AudioStreamSetProperty`/`AudioStreamCopyProperty` — building blocks.

### The catch: stock's format pipeline is 32-bit
Only the capability query is 64-bit. The negotiated/stored format is 32-bit (`_UpdateStreams` reads
`*(_DWORD*)(v2+16)`; `AirPlayAudioFormatToPCM` takes `int`). A multichannel format at bit 32+ cannot
flow through stock's internal path unchanged — "OR in a bit" is not enough.

### Consequence: the hook REIMPLEMENTS the multichannel path (stock keeps stereo)
1. `AudioSessionGetSupportedOutputFormats` hook: OR-in the multichannel capability bit.
2. `AirPlayReceiverSessionSetup` hook: advertise the multichannel format; on the phone selecting it,
   drive our own AudioStream setup (via exported `AudioStreamCreate`/`AudioStreamSetProperty`) with a
   6ch ASBD, bypassing stock's 32-bit format storage.
3. `AirPlayAudioFormatToPCM` hook: for our format, fill a 6-channel ASBD (a3[6]=channels).
4. Route the multichannel PCM to `six_channel_speakerp` (dio side / output callback).
5. Advertise `spatialAudioSupported` in `CARSessionConfiguration` via the dio /info patch (altScreen-class).

Substantial hook (a small multichannel negotiator), not a one-liner, but it fits the existing
LD_PRELOAD infra and avoids maintaining a full from-source libairplay. Exact format bit stays a
probe-determined `#define` (Phase 0 tooling wall).
