# Handoff — CarPlay discrete 5.1 on an Audi MHI2Q head unit

Reverse-engineering handoff. Everything below is either **verified on hardware** or **verified in a
disassembler**, and it is labelled which. Full history in `docs/reference/AUDIO.md` §9–§25a.

---

## 0. The goal

Make an iPhone send **discrete multichannel audio** over **wired CarPlay** to an Audi MHI2Q head unit
(QNX 6.5, APQ8064) so a B&O 3D Advanced amp receives real 5.1 instead of a stereo downmix that the amp
then upmixes algorithmically (Fraunhofer Symphoria).

The head-unit half is **finished and proven**. The phone refuses. This handoff is about *why*, and
whether it can be changed.

---

## 1. ⭐ THE KEY FINDING — the gate is a named phone-side feature flag

`APAudioFormatIsScreenSpatialAudioReceiver` in **AirPlaySupport** @ `0x22669B1F4` (iOS 27 beta 6,
iPhone18,1 shared cache):

```c
bool APAudioFormatIsScreenSpatialAudioReceiver(void)
{
    if (!APSSettingsIsFeatureEnabled(CFSTR("ScreenSpatialAudioReceiver")))   // <-- THE GATE
        return 0;
    v1 = <device-identifier function>();          // an unresolved cross-dylib stub
    if (v1 != 0x5BD62321 && v1 != 0xA10F2923 && v1 != 0xFCBBE892)
        return <fallback check>() != 0;
    return 1;
}
```

The CFString was resolved from the constant struct: **`"ScreenSpatialAudioReceiver"`** (26 chars).

Why this is the crux:

* Its only caller of interest is `APAudioFormatGetSupportedTransportAudioFormatsForScreenReceiver`
  @ `0x22669E3F4`:
  ```c
  return spatial ? 0x40001440800 : 0x1440800;    // spatial adds exactly ONE bit: 42
  ```
* **It takes no arguments.** It is pure phone-side state — *upstream* of anything the accessory
  publishes. Nothing we put in the head unit's AirPlay `/info` can flip it.
* The three constants are **not FourCCs** (no printable ASCII), most likely hashed product
  identifiers; note a non-matching device can still qualify via the fallback branch. So the
  **feature flag dominates**.

### Question 1 (highest value): what turns `ScreenSpatialAudioReceiver` on?
Trace `APSSettingsIsFeatureEnabled` in AirPlaySupport. Specifically:
- Which backing store does it read — a `CFPreferences` domain (`com.apple.airplay`?), a
  `/Library/Preferences/FeatureFlags/*.plist`, an entitlement, MobileGestalt, or a
  server-fetched/staged-rollout config?
- Is it cached per-process or re-read?
- Is there **any** input path from the connected accessory (a vehicle allowlist keyed on
  manufacturer/model from `/info`, a certification bit, an MFi capability)? Volvo EX90 was the launch
  vehicle for "CarPlay Spatial Audio, powered by Dolby Atmos" (Aug 2026), which smells like a
  per-vehicle allowlist — if such a list exists, find where it lives and what it keys on.

---

## 2. ⭐ Question 2 — the one lever still on OUR side: the shape of `audioFormats`

This is the most promising actionable lead, because it is something we can change.

`-[CARSessionConfiguration initWithSessionStatusOptions:propertySupplier:]` in **CarKit**
@ `0x1C2008F7C` sets two ivars (offsets decoded from the getters: `LDRB W0,[X0,#0x15]` /
`[X0,#0x16]`):

```asm
1C20099B0  ADRL  X2, cfstr_DCXEnabled          ; CFString "DCXEnabled"
...
1C2009A10  STRB  W0, [X20,#0x16]               ; dolbyAtmosAudioSupported  <- a plain Bool key
loc_1C2009A18:
           ADRL  X2, cfstr_audioFormats        ; CFString "audioFormats"
1C2009B50  MOV   W4, #0x10                     ; 16
1C2009B54  BL    <scan the formats array>
1C2009B64  MOV   W8, #1
1C2009B68  STRB  W8, [X20,#0x15]               ; spatialAudioSupported <- DERIVED from the scan
```

So `spatialAudioSupported` is **not a flag we set — it is inferred by scanning the `audioFormats`
array we publish**. Our head unit publishes the *old* shape (stock `AirPlayReceiverServerPlatformCopyProperty`
@ `0x1fe08` in our libairplay 210.81 builds an array of two dicts, `'d'`=100 main and `'e'`=101 alt, each
with an integer `audioOutputFormats`).

**Sub-questions:**
- What exactly does that scan match, and what is `W4 = 0x10` (16)? A channel count? An audio type? An
  enum case? Decompile the callee.
- iOS 27's `AirPlaySender` also parses a key we have **never published**:
  **`audioOutputFormatsExtended`** (`0x2511D5199`). See
  `airPlayDescription_copyCarPlayAudioFormatsExtended` @ `0x25109C5DC`, which — behind a settings check
  **and** a byte flag at `desc+65` — calls
  `APCarPlayAudioFormatsCopyFormatsForAudioType(formats, 103, "media", …)` and publishes the result
  under the key **`"bufferStream"`**.
- Follow `APCarPlayAudioFormatsCreate` @ `0x25107B82C` and
  `carPlayAudioFormat_repackageFormatsForStream` @ `0x25107C9B0`: **what /info shape does iOS 27
  actually want**, and would publishing the newer shape (or `audioOutputFormatsExtended`) change what
  the phone offers? Our SDK is 210.81; iOS 27 may be reading a format description our vintage never
  emits.

---

## 3. What is PROVEN on hardware (do not re-derive)

Live capture via `sloginfo -w` on the unit, stock `io-audio ... log_level=high` plus our own hook log.

**Baseline, stock:**
```
[AudioStream] <AirPlay> Supported output formats: 0X8010      (stream type 100 = main)
[AudioStream] <AirPlay> Supported output formats: 0X10        (type 101 = alt)
[AudioStream] <iPhone>Audio type: media
[AudioStream] <AirPlay> Audio Format: (48000[kHz] | 16[bits] | channels: 2)
[AudioStreamPrepare] Stream type: 100 | Format = 1819304813 | Ch = 2 | SR = 48000 | BPS = 16
[CQNXSoundIF] [open]<AUDIO> Open the path: "mpl5_dio_ent"(direction: 1)
```
`1819304813` = `'lpcm'`. **The realtime CarPlay audio stream is raw PCM, 48 kHz, 16-bit, stereo.**

**The AirPlayAudioFormat bit table**, rebuilt from *our* `AirPlayAudioFormatToASBD` @ `0x2bda0`
(literal cases; all 16 PCM cases cross-checked against the ordered descriptor strings — PASS):

| bits | family |
|--:|---|
| 2–17 | PCM (bit 11 = `PCM/44100/16/2`, **bit 15 = `PCM/48000/16/2`**) |
| 18–21 | ALAC |
| 22–23 | AAC-LC (44.1 / 48) |
| 24–25 | AAC-ELD (44.1 / 48) |
| 39–42, 44… | post-210 additions (the multichannel ones) |

`0x8010` = bits {4, 15}. Matches the static read of `AudioSessionGetSupportedOutputFormats` @ `0x497bc`
(`AudioSessionGetSupportedInputFormats()` returns `16`; output ORs `0x8000` for main, `16` for alt).

**Our stack cannot decode anything but PCM**: `AudioConverterNew_compat` @ `0x3ddf0` is a dead stub
(`return -6735`), and `ToASBD` rejects every ALAC/AAC bit.

**The 6-channel chain is alive**, observed running minutes before a CarPlay session:
```
DEVA: session_csd_setup: Setting up session while DEVICE_ID = <1> is being attached
DEVA: session_csd_setup: pcm format => sample_rate=48000, channels=6, bit_per_sample=24
```
Device 1 = `mpl1_int_ent` (internal media). CarPlay is device 4 = `mpl5_dio_ent`.

---

## 4. What was TRIED LIVE and failed (the negative result)

Rig: patched `deva-ctrl-qc.so`, rebuilt hook, `SPATIAL_5_1=1`, verbose logging. The hook interposes
`AudioSessionGetSupportedOutputFormats` and `AirPlayAudioFormatToASBD` via `LD_PRELOAD` into
`dio_manager`. **The interposition provably works** — our probe shows the augmented mask:

| advertised bit | `getSupportedFormats(type=100)` | what the phone then selected |
|---|---|---|
| 44 — `PCM/48000/16/5.1` | `stock=0x8010 offered=0x100000008010` | `ToASBD(fmt=0x8000)` = bit 15, **stereo** |
| 42 — `AAC_LC/48000/5.1.2` (the exact bit iOS adds for spatial) | `stock=0x8010 offered=0x40000008010` | `ToASBD(fmt=0x8000)` = bit 15, **stereo** |

Both refused. Handset: **iPhone 17 Pro Max, iOS 27 beta 6** — newest hardware, and the same OS the RE
targets, so "device too old" is not the explanation. This is what points at §1's feature flag.

---

## 5. Head-unit side (complete, for context)

* CarPlay audio device = `mpl5_dio_ent`, **device_id 4**, in `deva-ctrl-qc.so`'s 31-slot capability
  table (names VA `0xA5C54` stride 272, caps VA `0xA7D44` stride 48; `.data` VA→file = −0x1000).
* **Patch applied (4 bytes)**, mirroring the stock 6-channel device `mpl1_int_ent`:
  `max_voices 2→6` (file `0xA6E1C`), `max_fragsize 0x1800→0x4800` (`0xA6E24`),
  `max_buffer 0xF000→0x2D000` (`0xA6E28`). The fragsize part is **load-bearing**: the stream moves
  `nTFrames: 768` per transfer = 3072 B at stereo, but **9216 B at 6 channels > the stock 6144 cap**.
* **6 channels is a hard ceiling**: `deva_mib2_channel_map` @ `0x54ACC` switches on `{1,2,3,6}` only;
  no device in the table exceeds 6; output nodes are only `mono_/stereo_/six_channel_speakerp`; and
  `deva_csd_start_csd_session_playback` @ `0x24F28` **hardcodes 6** in its `voices > 2` branch.
* **Channel order is Apple canonical** — deva's multichannel stream map is `01 02 03 06 08 09` =
  FL FR FC LFE LB RB = `L R C LFE Ls Rs`. No reorder needed.
* Every media device is **48 kHz only** (rates mask `0x80`, min=max=48000). There is no 44.1 kHz bit
  anywhere in the table — which matters because the buffered/music path (§6) is 44.1-based.
* The amp is already a 5.1 system (16 MOST channels, 19 speakers). It cannot synthesise discrete
  channels from stereo — it upmixes. Discrete 5.1 arrives only when the source delivers 6 channels.
  5.1.2/height is impossible on this platform (no height source over MOST, the SHARC is a legacy
  Dolby/DTS decoder + upmixer, not an object renderer).

---

## 6. The other known path (large, unimplemented)

iOS 27 appears to carry a **second, buffered CarPlay audio stream** where lossless/spatial actually
live: `audioOutputFormatsExtended` → key `"bufferStream"` → audio **type 103**, sub-key `"media"`.
Evidence in §2 above. Its base transport mask is all-44.1 kHz PCM/ALAC/AAC (music formats), which is
why the mask `0x1440800` never intersects our 48 kHz realtime advert — a contradiction that took a
while to resolve. Our libairplay 210.81 has no notion of this stream type. Implementing it would also
need resampling, since the hardware is 48 kHz-only.

Caveat: `Audio type: media` also appears on the **realtime** stream in live logs, so the string
`"media"` alone does not prove the buffered path — the distinct `bufferStream` key does.

---

## 7. Where everything is

| what | path |
|---|---|
| iOS 27b6 shared cache | `Refferences/ios27_dsc/24A5418b__iPhone18,1/` |
| extracted + IDBs | `.../extracted_dylibs/AirPlaySupport`, `.../carkit_out/CarKit`, `.../aps_out/AirPlaySender`, `.../mt_out/MediaToolbox` |
| extract more from the cache | `ipsw dyld extract <cache> <ImageName> --output ./out` ; search strings with `ipsw dyld str <cache> --pattern '<regex>'` |
| HU firmware (MU1316) | `Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/` |
| our libairplay (stock 210.81) | `.../extracted/app/eso/lib/libairplay.so` (+ `.i64`) |
| deva driver | `.../extracted/app/armle/lib/dll/deva-ctrl-qc.so` |
| our hook source | `hook/audio/audio_hook.c`, `hook/framework/logging.c` |
| deva patch script | `deploy/deva-ctrl-qc/patch_deva.sh` (idempotent, cross-checks vs `mpl1_int_ent`) |
| full history | `docs/reference/AUDIO.md` §9–§25a |

---

## 8. Traps that cost time — do not repeat

* **The unit has no `strings`, `od`, `cmp`, `wc`, `tail`, `printf`**, and its `grep` has no `-a` and
  **returns 0 on binaries even when the pattern is present**. Verify on-unit binaries by copying them
  back to the host and comparing there.
* **`/mnt/app` is mounted read-only** → `mount -uw /mnt/app` first. And a live `.so` cannot be
  overwritten ("Resource busy") — stage a copy next to the target on flash, `rm` the busy original,
  then `mv` into place.
* **Hook logging is off by default** (production = WARN+ERROR). It needs `/mnt/app/carplay_verbose` or
  `/tmp/carplay_verbose` — and note `/tmp` is `/dev/shmem`, i.e. RAM, cleared on reboot. A framework
  bug hid this for a whole session: `log_init()` checked the marker but *nothing calls it* (logging is
  lazy), and the lazy auto-init in `log_write()` ignored the marker. Fixed — but if INFO lines are
  missing, check this first.
* **Never conclude from the stock line `<AirPlay> Supported output formats:`** — it is printed *inside*
  the real function, before our interposer adds its bit. It cannot distinguish "hook failed" from
  "phone refused". Use the hook's own probe.
* **Probe-only mode is mandatory when testing a new bit.** `/tmp/spatial_bit` accepts `<bit>[:<ch>]`,
  re-read per call (no reboot); `<bit>:0` advertises the bit but *refuses* it in `ToASBD`. Without it,
  a selected foreign-format bit (38 = 8ch, 47 = 32-bit float, 42 = AAC) would be described by our
  16-bit `lpcm` ASBD and feed **full-scale noise into a 19-speaker amp**.
* Format-bit indices: the descriptor strings in file order are **offset by 2** from the real bit
  numbers. Derive them from `ToASBD`'s literal cases, not from `strings` output order.

---

## 9. What a useful answer looks like

Ranked by value:

1. **How `APSSettingsIsFeatureEnabled("ScreenSpatialAudioReceiver")` is satisfied** — the backing
   store, and whether anything accessory-side (a vehicle allowlist, a certification/MFi bit, an /info
   field) feeds it. If it is purely a staged-rollout server flag, say so plainly; that closes the
   project's cheap path and is worth knowing definitively.
2. **What the CarKit `audioFormats` scan (`W4 = 0x10`) matches**, and whether publishing a newer /info
   shape — or `audioOutputFormatsExtended` — changes what iOS offers. This is the only remaining lever
   on our side of the wire.
3. **The identity of the three constants** `0x5BD62321 / 0xA10F2923 / 0xFCBBE892` and the function that
   produces them (an unresolved cross-dylib stub in the extracted AirPlaySupport).
4. A realistic scope for implementing the buffered media stream (type 103) in a 210.81-era libairplay.

Negative answers are valuable. "This is server-gated and unreachable from the accessory" is a perfectly
good result — it stops further work on a dead path.
