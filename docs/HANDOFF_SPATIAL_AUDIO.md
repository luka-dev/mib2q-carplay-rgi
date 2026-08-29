# Handoff — CarPlay discrete 5.1 on an Audi MHI2Q head unit

Reverse-engineering handoff. Everything below is either **verified on hardware** or **verified in a
disassembler**, and it is labelled which. Full history in `docs/reference/AUDIO.md` §9–§25a.

---

## 0. The goal

Make an iPhone send **discrete multichannel audio** over **wired CarPlay** to an Audi MHI2Q head unit
(QNX 6.5, APQ8064) so a B&O 3D Advanced amp receives real 5.1 instead of a stereo downmix that the amp
then upmixes algorithmically (Fraunhofer Symphoria).

The head-unit 6-channel PCM path is **finished and proven**. The phone's legacy/realtime CarPlay path
refuses the added multichannel bits. iOS 27 also has a newer buffered path, but our 210.81 receiver
does not implement it. This handoff records why the cheap path is closed and the exact next probe.

---

## 1. ⭐ RESOLVED — the gate is local phone-side feature state

`APAudioFormatIsScreenSpatialAudioReceiver` in **AirPlaySupport** @ `0x22669B1F4` (iOS 27 beta 6,
iPhone18,1 shared cache):

```c
bool APAudioFormatIsScreenSpatialAudioReceiver(void)
{
    if (!APSSettingsIsFeatureEnabled(CFSTR("ScreenSpatialAudioReceiver")))   // <-- THE GATE
        return 0;
    v1 = MGGetProductType();
    if (v1 != 0x5BD62321 && v1 != 0xA10F2923 && v1 != 0xFCBBE892)
        return IsAppleTV() != 0;
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
* The first unresolved call is `MGGetProductType()` and the fallback is `IsAppleTV()`. The three
  values are product-type enum values, not FourCCs. Their precise per-model labels remain unresolved;
  local Apple call sites group the same values with default low-latency-surround and Atmos receiver
  behavior. This identity does not change the gate result.

### Exact `APSSettings` precedence and cache

`APSSettingsIsFeatureEnabled` @ `0x2266529DC` does this:

1. Return a value from its mutex-protected, per-process dictionary cache if present.
2. Read an integer app preference named `ScreenSpatialAudioReceiver` with
   `CFPreferencesCopyAppValue`. The default application ID is exactly **`com.apple.airplay`**. A
   present nonzero value wins.
3. Otherwise call `_os_feature_enabled_impl("AirPlay", "ScreenSpatialAudioReceiver")`.
4. Cache the resulting Boolean. `APSSettingsSetValue` does not invalidate this cache, so set an
   override before the first query or restart the process that hosts AirPlay.

The system feature evaluator is `/usr/lib/system/libsystem_featureflags.dylib`. Its fast path opens
the read-only shared-memory table **`com.apple.featureflags.shm`**. Its file-backed slow path layers:

* `/System/Library/FeatureFlags/Unified`
* `/System/Library/FeatureFlags`
* `/Library/Preferences/FeatureFlags`

For this flag it reads `Domain/AirPlay.plist`; a feature entry is a dictionary whose `Enabled` value
is Boolean. It also reads the usual `Global.plist`, `Profiles.plist`, and `Settings.plist` layers.
The environment override is a colon-separated list in `FEATUREFLAGS_ENABLED` or
`FEATUREFLAGS_DISABLED`; the exact entry here is `AirPlay/ScreenSpatialAudioReceiver`.

### Consequence

There is **no connected-accessory, MFi, vehicle identity, entitlement, or network input** in either
the AirPlaySupport wrapper or the feature evaluator: the latter receives only the domain and feature
strings and consults local preference/file/shared-memory/environment state. A separate privileged
service could pre-provision a local profile or the shared-memory table (including from a server
rollout), but the evaluator itself cannot dynamically key the result on the current car.

On a rooted/jailbroken handset, the three useful override points are therefore:

* the integer `com.apple.airplay` app preference `ScreenSpatialAudioReceiver` (shortest path);
* `/Library/Preferences/FeatureFlags/Domain/AirPlay.plist`, with
  `ScreenSpatialAudioReceiver = { Enabled = true; }`;
* `FEATUREFLAGS_ENABLED=AirPlay/ScreenSpatialAudioReceiver` in the relevant process environment.

All require a process restart if the gate has already been queried. This closes the accessory-side
route to the old bit-42 gate; changing `/info` cannot turn it on.

---

## 2. ⭐ RESOLVED — the modern `audioFormats` shape

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

`spatialAudioSupported` is **not a Boolean we set**. CarKit gets the top-level `audioFormats` value,
requires an `NSArray`, fast-enumerates it, requires each entry to be an `NSDictionary`, and asks each
dictionary for the exact key `audioOutputFormatsExtended`. Any non-null value sets the ivar at
`+0x15`; it does not validate the value's type or contents. `W4 = 0x10` is merely the
`countByEnumeratingWithState:objects:count:` batch capacity: **16 array objects, not 16 channels or an
audio enum**.

Our head unit publishes the old shape (stock `AirPlayReceiverServerPlatformCopyProperty` @ `0x1fe08`
in libairplay 210.81): two dictionaries, stream type 100 main and type 101 alt, each with an integer
`audioOutputFormats` mask.

iOS 27's `APCarPlayAudioFormatsCreate` @ `0x25107B82C` accepts stream types 100, 101, 103, 106, and
107. Each entry has:

* `type`: numeric stream type;
* `audioType`: a string, with `media` selected here and `default` as fallback;
* `audioOutputFormats`: the legacy numeric bitmask, which must remain nonzero even when an extended
  array exists;
* optional `audioOutputFormatsExtended`: an array of numeric **`APAudioFormatIndex` values**, not a
  second bitmask and not an array of dictionaries.

For the new buffered media path, the minimal candidate added alongside the existing entries is:

```text
{
    "type": 103,
    "audioType": "media",
    "audioOutputFormats": 0x1440800,
    "audioOutputFormatsExtended": [65]
}
```

Index 65 is `APAC/48000/5.1`. `airPlayDescription_copyCarPlayAudioFormatsExtended` @ `0x25109C5DC`
selects type 103 / `media` and exports it as `{"bufferStream": extendedArray}`.

There is a second mandatory gate: the receiver's setup response must advertise the exact supported
feature token **`mainBuffered`**. `AirPlaySender` searches the setup-response feature array for that
token; only then does it set endpoint property `SupportsBufferedAudio` (the byte at description
offset `+65`) and permit the `bufferStream` path. Merely inserting the extended key can make CarKit's
`spatialAudioSupported` Boolean true, but cannot activate the sender path.

The `bufferedAPACSurround` setting defaults enabled in this build. Thus the two accessory-controlled
metadata requirements are a type-103 entry with a nonempty extended array and the `mainBuffered`
feature token. They can make the phone attempt the modern path; they do not make our old receiver
capable of handling it.

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

## 6. The modern buffered path (identified, still unimplemented)

iOS 27 carries a **second, buffered CarPlay audio stream** where APAC spatial formats live:
`audioOutputFormatsExtended` → `"bufferStream"` → stream type **103** → audio type `"media"`. Its
required legacy mask `0x1440800` contains bits {11, 18, 22, 24}: 44.1 kHz stereo PCM, ALAC, AAC-LC,
and AAC-ELD. That mask is a compatibility/selection prerequisite; the extended array separately
names APAC formats and can contain the 48 kHz index 65.

The exact extended indices accepted by iOS 27's CarPlay spatial predicate are:

| index | descriptor |
|--:|---|
| 65 | `APAC/48000/5.1` |
| 66 | `APAC/48000/5.1.2` |
| 67 | `APAC/48000/7.1` |
| 68 | `APAC/48000/7.1.4` |
| 76 | `APAC/48000/5.1.4` |
| 77 | `APAC/48000/7.1.2` |
| 92 | `APAC/48000/9.1.6` |

Our hardware ceiling makes **index 65 the only sensible target**. It is 48 kHz and maps exactly to
the proven six-channel order `L R C LFE Ls Rs` after decoding.

Our libairplay 210.81 has no type-103 receiver implementation. A real implementation needs at least
the setup/control state machine, stream lifecycle, timing/clock synchronization, buffering and
jitter handling, APAC framing and decode, loss/retransmit behavior, conversion to interleaved
six-channel PCM, and delivery to `mpl5_dio_ent`. Resampling may still be required for legacy/fallback
content because all HU media endpoints are 48 kHz-only. Advertising the metadata alone is therefore
a negotiation probe, not a playable patch.

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
* **Do not derive `APAudioFormatIndex` from descriptor-string file order.** The early legacy subset
  can look offset by two, but later insertions break that relationship. Derive indices only from the
  formatter's switch/table or `ToASBD`'s literal cases. That is how the exact APAC map in §6 was
  recovered.

---

## 9. Next experiment and remaining work

### First experiment: metadata-only, fail closed

On a muted/isolated bench setup:

1. Preserve the working type-100/type-101 entries.
2. Add the type-103 / `media` entry from §2 with legacy mask `0x1440800` and extended array `[65]`.
3. Add `mainBuffered` to the receiver's setup-response supported-feature array.
4. Do **not** route or describe an unknown selected payload as PCM. Refuse/abort type 103 until its
   framing has been captured and decoded.
5. Capture the full setup exchange, sender logs, stream-control requests, ports, timing traffic, and
   first payload packets. Check separately whether CarKit reports spatial support, whether the phone
   emits `bufferStream`, and whether it attempts to start type 103.

This experiment distinguishes three layers without risking speaker damage: CarKit UI capability,
AirPlaySender negotiation, and actual stream transport.

### If type 103 is attempted

Document the control protocol and APAC packet framing before writing audio output code. Then build a
receiver that decodes index 65 to six-channel 48 kHz PCM and feeds the already-proven HU path. Keep a
hard reject for every other extended index; the platform cannot render 5.1.2, 7.1, or higher.

### Remaining low-priority question

Map the individual model labels for `MGGetProductType()` values `0x5BD62321`, `0xA10F2923`, and
`0xFCBBE892` if needed for completeness. The producer and fallback are resolved, and this mapping no
longer affects the project decision: the old gate is local phone state, while the only accessory-side
lever is the separate type-103 buffered protocol.
