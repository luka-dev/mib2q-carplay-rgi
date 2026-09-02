# Handoff — CarPlay discrete 5.1 on an Audi MHI2Q head unit

Reverse-engineering handoff. Everything below is either **verified on hardware** or **verified in a
disassembler**, and it is labelled which. Full history in `docs/reference/AUDIO.md` §9–§26.

> **Latest rendering status, 2026-09-02:** keep `AUDIO_SPATIAL=0`. A standalone
> player bypassed CarPlay and proved `mpl5_dio_ent` clean at two voices but
> broken at six. The same-device A/B measured 48,107 frames/s at two voices
> and 48,129 at six, both against 48,000 expected.
> Static analysis then found the surviving defect: the six-channel DEVA timer
> uses `0x4800`, while runtime CSD route setup still copies MPL5's stereo
> `min_voices=2` and `min_fragsize=0x1800`. The current deva binary is not a
> completed 5.1 fix. See
> [`ROUTER_INPUT_WIDTH_AUDIT.md`](ROUTER_INPUT_WIDTH_AUDIT.md).

---

## Latest source status — decoder/render implementation

The current source supersedes the earlier capture-only status retained below for live-test history.
It has not been deployed to the vehicle in this work session.

The type-103 path is now complete through the Audi PCM boundary:

1. APAP BE32 framing, ChaCha20-Poly1305 authentication, extension TLV/cookie selection;
2. APAC decode through an optional, versioned `libcarplay_apac.so` (ABI v1);
3. index 65 normalization from cookie order `C L R Ls Rs LFE` to PCM6
   `L R C LFE Ls Rs`;
4. index 66 validation against CoreAudio Atmos 5.1.2 tag `0x00c20008`, followed by selectable
   `downmix`, `drop`, or 8-channel `passthrough`;
5. a bounded PCM ring drained at stock timing by interposing `snd_pcm_plugin_params/write` on the
   existing `mpl5_dio_ent` handle. The hook does not open a competing PCM device. A stereo
   type-100 stream is expanded to the configured output frame width and remains the immediate
   fallback until decoded type 103 is running; underruns are padded with silence.

The APAC implementation is no longer linked into the LD_PRELOAD object. `libcarplay_hook.so`
loads `/mnt/app/root/hooks/libcarplay_apac.so` only when a buffered APAC stream starts and verifies
the ABI table before use. If the codec is missing or incompatible, capture/control remain alive and
decode fails closed; base CarPlay does not gain a loader dependency on the codec.

User-facing startup policy is:

| variable | meaning |
|---|---|
| `AUDIO_SPATIAL=0/1/true/false` | request APAC 5.1/index 65 and arm the PCM6 fallback |
| `AUDIO_SPATIAL_ATMOS=0/1/true/false` | request the full APAC 5.1.2/index 66 master; implies Spatial |
| `AUDIO_SPATIAL_ATMOS_MODE=downmix` | PCM8 -> PCM6 with heights folded into surrounds at -6 dB |
| `AUDIO_SPATIAL_ATMOS_MODE=drop` | PCM8 -> PCM6 with height channels discarded |
| `AUDIO_SPATIAL_ATMOS_MODE=passthrough` | PCM8; requires a real 8ch HU PCM path and patched amp |

`AUDIO_SPATAIL` is accepted only as a warned compatibility alias for the typo. Old `SPATIAL_*`
variables are read only when none of the new boolean controls is present, for migration of an older
on-unit startup script. The repository startup currently sets
`AUDIO_SPATIAL=0` and Atmos off because the MPL5 CSD-width defect makes
six-channel rendering unsafe to enable.

`SPA_103_MU1316_*` identifiers were removed from common logic. All private VA/offset/prologue data
now lives in a `spa103_private_profile_t` table. The first profile is named `MHI2Q AirPlay 210.81`;
unknown `libairplay.so` builds still fail closed. K1004 must receive its own independently verified
profile because its private offsets differ.

Bench gates passed after this change: the full APAC port suite, 3,980-frame capture integration,
the six-packet end-to-end APAP/decode test, downmix/drop/reorder tests, and the QNX builds of both
shared objects. Both QNX objects have no emutls, exceptions, RTTI, STL/libstdc++ dependency, or
hook-owned ELF constructor. The main hook has no `DT_NEEDED` entry for `libcarplay_apac.so`.
The verified local artifacts are `libcarplay_hook.so` SHA-256
`1312072cb931192fde3dc0027168f77e8a3eaac66cb1da4aab0268a348da3a29` (427,891 bytes)
and `libcarplay_apac.so` SHA-256
`a6a7bc8c3f17dfe170765b04a461259a407269de47dde6fe0a2b3f6b2695044f` (678,911 bytes).
Both hashes have moved since the type-103 work landed, for reasons that are not
it: the hook when the AirPlay SETUP/`/info`/TearDown interpositions were lifted
out of the altScreen module into `hook/framework/airplay_seams.c`, and the codec
by 40 bytes of DWARF when file headers were added — `scripts/apac_flags.sh`
builds with `-g`, so adding four lines to a file shifts every line number under
it. No decoder code changed.

The decoder/render implementation has passed its bench gates, but it must not be
used as an end-to-end acceptance test until the independent deva CSD-width fault
is fixed. After that fix, live validation still needs the two shared objects and
startup file, `audio sink:` counters, audible PCM6, pause/resume, and a long
session. PCM8 passthrough must remain off on the current six-channel HU/amp path.

The QSA seam is fingerprinted by static evidence from the exact AirPlay 210.81 binary
(`SHA-256 01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2`). Its
`snd_pcm_plugin_write` PLT entry is at `0x1b1d8`; the only two callers are the SingleThreadPolicy
writer (`0x47324`, retry `0x473f8`) and ObjectLockPolicy writer (`0x48648`, retry `0x4871c`). At
both calls ARM `r0=handle`, `r1=buffer`, `r2=byte count`, and the stock method compares the return
against that original byte count. This is why the bridge writes an expanded PCM6/PCM8 byte count
to QSA but translates a complete result back to the stock caller's original size.
Stock `prepare` also calls `snd_pcm_plugin_setup` at `0x466d8` (SingleThreadPolicy) and `0x47a08`
(ObjectLockPolicy), then stores the returned voices/fragment geometry for allocation and
`getBytesPerFrame`. The hook therefore translates setup back to the original type-100 geometry
while QSA retains the expanded parameters; without this third interposer the stock layer would see
six channels and the write bridge would expand the buffer a second time.

## 0. The goal

Make an iPhone send **discrete multichannel audio** over **wired CarPlay** to an Audi MHI2Q head unit
(QNX 6.5, APQ8064) so a B&O 3D Advanced amp receives real 5.1 instead of a stereo downmix that the amp
then upmixes algorithmically (Fraunhofer Symphoria).

The head-unit six-channel transport is proven through the DMA ring, but the
MPL5-to-CSD render handoff is **not finished**; see the latest status above. The phone's legacy/realtime CarPlay path
refuses the added multichannel bits. iOS 27 also has a newer buffered path, but our 210.81 receiver
does not implement it. A hook-owned type-103 TCP listener/capture and buffered-control stage is now
deployed: both APAC 5.1 and APAC 5.1.2 negotiation are proven on hardware. The sender still closes
type 103 because decrypt/decode/queue/render is unimplemented; control-plane acceptance alone does
not make an indefinitely stable or audible session.

### Current HU state at handoff (2026-08-31)

The HU is deliberately parked in ordinary stock stereo fallback: `SPATIAL_103=0` and
`SPATIAL_5_1=0`. The remaining type-103 values stay inert at index 65, empty extra index, DCX off,
and mask `0x400000`. No binary or driver patch was removed. Run
`scripts/set_spatial_profile.sh spatial-5.1` to restore the APAC 5.1 capture profile for the next
`dio_manager` generation; `fallback-stereo` restores this standby state. There are no type-103
`/tmp` marker controls.

Live iOS declined type 103 when index 40 was advertised alone and selected stereo type 100
(`0x8000`). With index 65 it selected a genuine type-103 APAP 5.1 stream, displayed Dolby Atmos,
sent a 33-byte `SETMAGICCOOKIE`, and connected to TCP 6032. With index 66 it selected type 103 again;
its 34-byte `dapa` cookie independently reports 48 kHz, 1024 frames and **8 channels**, proving that
the wire stream really is 5.1.2 rather than merely a UI badge. TCP 6033 is not used by this path.

The final AAC bypass test advertised `[40,65]`: index 40 was the AAC-LC 5.1.2 candidate and index 65
was present only to satisfy iOS's spatial capability gate. iOS saw that exact array but selected
`audioFormatIndex=65`, `packetFormatAPAP=true`, `spf=1024`; the 33-byte cookie decoded to 48 kHz,
1024 samples and six channels. The first Play interval lasted about 19 seconds and the second about
11 seconds before sender-driven rate=0/flush/teardown; `dio_manager` stayed alive. Therefore the
visible Spatial Audio session was genuine APAC 5.1, and the installed Cinemo AAC decoder cannot be
used as an APAC decoder or as an AAC bypass for this sender.

A later clean index-65 session is documented in
[`APAC_5_1_LIVE_20260831.md`](captures/APAC_5_1_LIVE_20260831.md). The private local corpus at
`captures/apac_5_1_live_20260831_0205` contains the exact 1,320,019-byte HU APAP audio capture,
matching SETUP/`shk`, 33-byte cookie and control plists. The final four bytes are a dangling
next-record length prefix written during full teardown; after ignoring only that prefix, all 3,980
complete frames authenticate successfully and yield 1,148,875 bytes of plaintext APAC packet data.
Preserve the local raw file unchanged; `/captures/` is deliberately excluded from git because it
contains the session key and captured programme audio.

The profiles are now explicit and persistent. Run `scripts/set_spatial_profile.sh` from the repo:

| profile | type 103 | wire layout | type-100 PCM6 fallback |
|---|---:|---|---:|
| `spatial-5.1` | on, index 65 | APAC 5.1 / 6 ch | kept on |
| `spatial-5.1.2` | on, index 66 | APAC 5.1.2 / 8 ch | kept on |
| `experimental-aac-5.1.2` | on, `[40,65]` | negative-control only: iOS selected APAC 65 | kept on |
| `fallback-5.1` | off | none | on |
| `fallback-stereo` | off | none | off; stock stereo |

The tool edits only the four exact `SPATIAL_5_1`, `SPATIAL_103`, `SPATIAL_103_INDEX`, and
`SPATIAL_103_EXTRA_INDEX` exports in the HU startup file, verifies uniqueness before writing, keeps
a one-time recovery copy, and never replaces the rest of the user's startup script. The setting
applies to the next `dio_manager` generation; no HU reboot is required.

The installed hook at `/mnt/app/root/hooks/libcarplay_hook.so` is build
`78e93829d1f4234c2d28905235bb198b6cef3017cd011914b3599c667e020e39` (404640 bytes). Its copy-back
SHA-256 matches the local build. It adds:

* paired `mainBuffered` / `mainBufferedInfo` negotiation and selectable type-103 index 65/66;
* an exported `AirTunesServer_EnsureCreated` interposer which installs the fingerprint-locked
  buffered-method dispatcher before the first control connection is accepted;
* `SETMAGICCOOKIE`, `GETANCHOR`, `SETRATE`, `SETRATEANCHORTIME`, and `FLUSHBUFFERED` handling on the
  existing RTSP control connection;
* TCP 6032 inclusive-BE32 APAP capture, bounded storage, and teardown-safe threads;
* stock `updateFeedback` rewritten to `{streams:[]}` only while hook-owned type 103 is active;
* an explicit optional second format index used only for the completed `[40,65]` negative test;
* a tested S16 5.1.2-to-5.1 downmix helper, not yet connected to a decoder or DIO5 render path.

Build `dc5545c946961f406060ca328b40c2c200365eca47887e76e2572e7ab75744a0` was verified live:
`SETRATE rate=1` was accepted and the former deterministic 5-second teardown disappeared; the
session remained active for more than two minutes and the playback counter advanced. Live iOS 27
omits `rtpTime` from SETRATE and sends only `mediaTimeValue`, `mediaTimeScale`, and `rate`; accepting
that form was the decisive correction.

Pause exposed the next exact contract. iOS sends `SETRATEANCHORTIME {rate:0}`, followed by a
media-only FLUSH such as `{flushUntilSeq, flushUntilMediaTimeValue, flushUntilMediaTimeScale}`. The
older parser returned HTTP 400 because it incorrectly required both from/until boundaries and RTP
timestamps. Build `b8305141…` was verified live to ACK both rate=0 and that optional-boundary FLUSH.
The later teardown was the user's physical unplug, not a receiver crash. The remaining timeline
rollback came from returning a zero/synthetic clock: only about 0.576 s/27 APAP frames were sent.
Build `d7dc11db…` obtains the MU1316 clock pointer from session+5072 and safely returns an anchor,
but live output still had timeline 0. A follow-up build incorrectly treated session+5072 as the
inline object and crashed on Play. Static stock callers proved the correct contract: Create receives
session+0x13d0 as an output slot and later code loads `[session+0x13d0]`. That unsafe build was
immediately reverted; the timeline-ID source remains under investigation.

The decisive static/live correlation is RTP, not the old clock's absent timeline-ID field. Pioneer
converts requested media time to 48 kHz sample time before replying. Live values match exactly:
`46670057988423/1e9*48000 = 0x85862bdf`, which is the first APAP header media value. The hook had
returned `rtpTime=0`; current source now returns that modulo-2^32 conversion for SETRATE, FLUSH, and
GETANCHOR.

The first live RTP build returned the exact APAP value but still stopped after the initial buffered
window and the iPhone timeline rolled back. Static re-check of the actual iOS sender response parser
found a response-shape mismatch: `SETRATE` expects the complete transport triple
`rtpTime + mediaTimeValue + mediaTimeScale`, the four
`networkTime{Secs,Frac,TimelineID,Flags}` fields, and optional `status` (default zero). The last live
build omitted both media-time fields and is therefore a known regression. Current source restores
the complete triple and includes `status=0`.

The regressed build produced two authenticated 27-packet APAP windows (54/54 packets decrypted and
authenticated). Each packet advances RTP by 1024 samples; the first automatic flush covered exactly
those 27 packets. `SETRATE rate=1` to the automatic `rate=0` request took 9.298 seconds, strongly
matching the sender's 10-second network-clock lock timeout rather than the earlier 5-second method
wall. APAP packets contain a valid TLV prefix followed by the actual APAC payload; the decrypt tool's
`trailing_hex` is not extension-only padding.

The next instrumented live build exposed the clock defect directly: successive returned seconds
jumped by millions and the 64-bit fractions were unrelated. `AirTunesTime` itself was laid out
correctly; the bug was the hook's declaration of `MillisecondsToUpTicks(uint32_t)`. Target
`TickUtils.h` and ARM disassembly both prove a `uint64_t` argument in `r0:r1`; the old declaration
left `r1` undefined and converted the requested +150 ms lead into a random interval. Current source
uses the correct 64-bit ABI.

The corrected-ABI index-65 build authenticated all 2561/2561 captured APAP packets and saved 833952
bytes; the first `rate=1` interval lasted about 30 seconds before sender-driven `rate=0`, flushes and
per-stream teardown. The index-66 run on 2026-08-31 then proved the independent 8-channel contract:
SETUP contained `audioFormatIndex=66`, `packetFormatAPAP=true`, `spf=1024`, and the cookie decoded to
48 kHz / 1024 / 8 channels. In that no-render run the sender issued `rate=0` after about six seconds,
then per-stream teardown; the receiver process did not crash. The visible Dolby Atmos -> Spatial
transition therefore marks loss of the type-103 stream, not an in-place 5.1.2 -> 5.1 conversion.

This remains a protocol/capture sink. It does **not** yet authenticate/decrypt APAP, decode APAC,
downmix, or render type-103 audio. A moving playback counter proves control-plane acceptance, not
audible output.

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
  return spatial ? 0x40001440800 : 0x1440800;    // adds bit 42 = AAC-ELD 48k/5.1.2
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
    "audioOutputFormats": 0x400000,
    "audioOutputFormatsExtended": [65]
}
```

Legacy bit 22 is `AAC-LC/44100/2`; it is the single codec-family compatibility format required to
construct the buffered engine. Index 65 is `APAC/48000/5.1`.
`airPlayDescription_copyCarPlayAudioFormatsExtended` @ `0x25109C5DC` selects type 103 / `media` and
exports the extended list as `{"bufferStream": extendedArray}`.

There is a second mandatory gate: the receiver's setup response must advertise the exact supported
feature token **`mainBuffered`**. `AirPlaySender` searches the setup-response feature array for that
token; only then does it set endpoint property `SupportsBufferedAudio` (the byte at description
offset `+65`) and permit the `bufferStream` path. Merely inserting the extended key can make CarKit's
`spatialAudioSupported` Boolean true, but cannot activate the sender path.

The setup response is a negotiation result, not an independent capability advert: the receiver may
enable `mainBuffered` only when the phone included that token in the control request's `features`
array. The first probe violated this constraint by adding the token unconditionally. The hook now
logs the entire proposal and adds `mainBuffered` only when it was actually proposed.

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

**Stock libairplay 210 cannot decode anything but PCM**: `AudioConverterNew_compat` @ `0x3ddf0` is a
dead stub (`return -6735`), and `ToASBD` rejects every ALAC/AAC bit. This is a libairplay limitation,
not an absence of codecs in the whole firmware: the MHI2Q image also contains Cinemo
`libNmeAudioAAC.so`, `libNmeBaseClasses.so`, and `libNmeSDK.so`. A custom receiver can therefore use
the target's AAC decoder instead of the dead AirPlay compatibility wrapper.

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
| 42 — `AAC_ELD/48000/5.1.2` (the exact bit iOS adds for the old phone-side spatial gate) | `stock=0x8010 offered=0x40000008010` | `ToASBD(fmt=0x8000)` = bit 15, **stereo** |

Both refused. Handset: **iPhone 17 Pro Max, iOS 27 beta 6** — newest hardware, and the same OS the RE
targets, so "device too old" is not the explanation. This is what points at §1's feature flag.

### Initial combined type-103 advert failed before media SETUP (live, 2026-08-29) — cause identified

The first buffered probe enabled both accessory-controlled changes at once: `/info` gained type 103
with extended index 65, and the control response gained `mainBuffered`. Three consecutive connection
generations produced the same sequence:

1. iAP2 authentication completed.
2. Control SETUP completed successfully (`ret=0`).
3. The phone fetched the augmented `/info` response.
4. Within roughly 30–130 ms, the phone sent `AirPlayReceiverSessionTearDown` with `reason=0`.

There was no type-103 SETUP request, no `spatial_setup103_*` capture, no type-100/DIO5 audio setup,
and no hook crash. The later `TIMEOUT_STARTUP` was the supervisor's consequence of the orderly phone
teardown, not its cause. Therefore the phone rejected the combined negotiation contract immediately
after `/info`; it never reached payload transport or codec selection. This result does not implicate
the PCM transport through the DIO5 DMA ring; the later CSD render-handoff defect
is independent and documented at the top of this file.

The subsequent `feature`-only capture recovered the exact control proposal. The iPhone offered 14
features, including `mainBuffered` at array index 5. The hook correctly returned
`enabledFeatures = [mainBuffered, altScreen, viewAreas]`, but the final `/info` had no
`mainBufferedInfo`; the phone again tore the session down 26–50 ms after `/info`.

This is now explained directly by iOS 27 AirPlaySender. At `carEndpoint_Activate_block_invoke_2`
(`0x251082AE0`), endpoint byte `+0x42` (the negotiated `mainBuffered` flag) and CFString
`mainBufferedInfo` are passed to `carEndpoint_validateInfoResponseKeyPresentForFeature`
(`0x25108B10C`). That validator enforces an exact pair in both directions:

* negotiated feature + missing `/info` key → error `-16720`;
* unnegotiated feature + present `/info` key → the same error.

The value must be a dictionary for CarKit. Its optional `DCXEnabled` Boolean is read separately and
sets `dolbyAtmosAudioSupported`; the key's presence is what satisfies AirPlaySender's contract. The
corrected hook therefore publishes `mainBufferedInfo = {}` only for the exact session whose control
reply enabled `mainBuffered`. That correction was subsequently validated on hardware (see §9).
`DCXEnabled` remains absent by default; it can now be enabled only by the explicit probe marker
described below.

---

## 5. Head-unit side (caps and DMA complete; CSD route width incomplete)

* CarPlay audio device = `mpl5_dio_ent`, **device_id 4**, in `deva-ctrl-qc.so`'s 31-slot capability
  table (names VA `0xA5C54` stride 272, caps VA `0xA7D44` stride 48; `.data` VA→file = −0x1000).
* **Patch applied (4 bytes)**, mirroring the stock 6-channel device `mpl1_int_ent`:
  `max_voices 2→6` (file `0xA6E1C`), `max_fragsize 0x1800→0x4800` (`0xA6E24`),
  `max_buffer 0xF000→0x2D000` (`0xA6E28`). The fragsize part is **load-bearing**: the stream moves
  `nTFrames: 768` per transfer = 3072 B at stereo, but **9216 B at 6 channels > the stock 6144 cap**.
* **Surviving defect:** route setup does not use those negotiated maxima. At
  `deva_update_session_info_from_list @0x54848`, record `min_voices` and
  `min_fragsize` become active CSD session fields. MPL5 therefore creates a
  stereo/`0x1800` CSD consumer beside its six-channel/`0x4800` DMA producer.
  Acquire, prepare, and trigger do not re-provision it. A correct dual-mode fix
  must rebuild the CSD stream/context/buffer pool from the live PCM channel.
* **6 channels is a hard ceiling**: `deva_mib2_channel_map` @ `0x54ACC` switches on `{1,2,3,6}` only;
  no device in the table exceeds 6; output nodes are only `mono_/stereo_/six_channel_speakerp`; and
  `deva_csd_start_csd_session_playback` @ `0x24F28` **hardcodes 6** in its `voices > 2` branch.
* **Channel order is Apple canonical** — deva's multichannel stream map is `01 02 03 06 08 09` =
  FL FR FC LFE LB RB = `L R C LFE Ls Rs`. No reorder needed.
* Every media device is **48 kHz only** (rates mask `0x80`, min=max=48000). There is no 44.1 kHz bit
  anywhere in the table — which matters because the buffered/music path (§6) is 44.1-based.
* The amp is already a 5.1 system (16 MOST channels, 19 speakers). It cannot synthesise discrete
  channels from stereo — it upmixes. Discrete 5.1 arrives only when the source delivers 6 channels.
  5.1.2/height is not transportable to this amplifier — **re-confirmed 2026-08-31, on better evidence
  than the original claim.** The amp does advertise an eight-channel sink, `SinkNr 0x32`
  (`Entertainment_7.1_SAD`, MOST-DTCP, 8 x 24-bit, BlockWidth 26), but `SinkInfo` declares its stream
  positions 7 and 8 as `AudioChannelName` `0x13`/`0x14` = Back Left/Right of Center. Height is
  `0x0D`/`0x0F` (Top Front L/R) and appears in none of the amp's 18 sink profiles. So the eight
  channels are rear 7.1, not 5.1.2, and a height layer sent there lands on rear-surround inputs.
  Decode 5.1.2 if the codec makes it convenient, but always fold to six. Details and the selectable
  sink table: `docs/reference/AMP_7_1_SINK.md`; full amp-side RE in
  `Firmwares/SoundAmp/AMP_ENTERTAINMENT_7_1_MOST.md`.

---

## 6. The modern buffered path (statically complete, HU implementation pending)

The implementation-ready static contract is now separated into
[`PIONEER_TYPE103_STATIC_SPEC.md`](PIONEER_TYPE103_STATIC_SPEC.md). It is the authoritative source
for exact SETUP/APAP/security/TLV/cookie/lifecycle/downmix details and for the new ARMv7 AAC/Cinemo
route. This handoff retains the investigation history and hardware evidence.

iOS 27 carries a **second, buffered CarPlay audio stream** where APAC spatial formats live:
`audioOutputFormatsExtended` → `"bufferStream"` → stream type **103** → audio type `"media"`. Its
legacy compatibility value must remain nonzero, but it must also resolve to **one codec family**.
The correct current probe is bit 22 alone, `0x400000` = AAC-LC/44.1/stereo. The earlier
`0x1440800` value came from
`APAudioFormatGetSupportedTransportAudioFormatsForScreenReceiver`; that is a broad transport-family
capability mask, not a valid type-103 CarPlay output entry. It mixes bits {11,18,22,24}: PCM, ALAC,
AAC-LC, and AAC-ELD. The extended array independently names the desired 48 kHz APAC index 65.

The APAC extended indices identified by iOS 27 as CarPlay spatial-master formats are:

| index | descriptor |
|--:|---|
| 65 | `APAC/48000/5.1` |
| 66 | `APAC/48000/5.1.2` |
| 67 | `APAC/48000/7.1` |
| 68 | `APAC/48000/7.1.4` |
| 76 | `APAC/48000/5.1.4` |
| 77 | `APAC/48000/7.1.2` |
| 92 | `APAC/48000/9.1.6` |

For a native APAC result, **index 65 is the only directly renderable target**. It is 48 kHz and maps
exactly to the proven six-channel order `L R C LFE Ls Rs` after decoding. Index 66 carries the desired
5.1.2 master, but must be decoded to eight PCM channels and deliberately downmixed to six before it
reaches the HU device.

There was also a potentially shorter playable route: `APAudioFormatIndexToAPAudioFormat`
proves that every index below 64 maps exactly to its legacy bit (`1ULL << index`). A final table
cross-check corrected an earlier labeling error: extended index **40** is
`AAC-LC/48000/5.1.2`; index **42** is `AAC-ELD/48000/5.1.2`. The MHI2Q firmware
already ships a 32-bit ARM Cinemo AAC decoder. Static analysis of the same-CPU MH2p AirPlay 320
library found its complete `AudioConverter` adapter and both AAC-LC/AAC-ELD decoder construction
paths; the AAC-LC constructor passes
48-kHz rate and eight-channel count into `CinemoCreateAudioCodec` without a discovered channel
clamp. Audi's installed `libNmeSDK.so` exports that factory and its installed `libNmeAudioAAC.so`
exports the matching AAC plugin; `dio_manager` already has `libNmeSDK.so` in DT_NEEDED. It is not
necessary to bench that path for production: although the adapter can request eight channels, the
hardware negotiation test has rejected the route. When the HU advertised `[40,65]`, iOS selected
index 65/APAP rather than 40/AAC. Cinemo therefore remains useful only as an AAC reference and must
not receive APAC.
The type-103 extended-array copy path does not whitelist only APAC: in the recovered sender it makes
a mutable copy and removes legacy index 23, while leaving 40/42 intact. This makes the AAC request
statically valid at that copy layer, but it is not selected by this iPhone when a spatial-gating
APAC index is also present; index 40 alone had already caused iOS to fall back to type 100.

The target also has a genuine Dolby Digital Plus decoder, so extended indices **50**
(`DDPLUS/48000/5.1`) and **53** (`DDPLUS/48000/9.1.6`) are exposed as fail-closed runtime probes.
Whether the type-103 sender accepts either index is unverified. Index 50 is directly compatible with
the HU's six-channel ceiling after decode; index 53 is useful only if the old Dolby core can consume
the stream and render/downmix its Atmos-era layout.

Our libairplay 210.81 has no type-103 receiver implementation. A real implementation needs at least
the setup/control state machine, stream lifecycle, timing/clock synchronization, buffering and
jitter handling, selected-codec framing and decode, loss/retransmit behavior, conversion to interleaved
six-channel PCM, and delivery to `mpl5_dio_ent`. Resampling may still be required for legacy/fallback
content because all HU media endpoints are 48 kHz-only. Advertising the metadata alone is therefore
a negotiation probe, not a playable patch.

Caveat: `Audio type: media` also appears on the **realtime** stream in live logs, so the string
`"media"` alone does not prove the buffered path — the distinct `bufferStream` key does.

### Exact iOS sender activation and latency behavior

`___carEndpoint_activateInternal_block_invoke_2.134` @ `0x2510873D8` creates the ordinary main
CarPlay audio endpoint first. When the negotiated `mainBuffered` state byte is set, its branch at
`0x251087638` immediately creates a second BufferedAudio/media endpoint; an error from that call
aborts endpoint activation. The buffered branch in `carEndpoint_setupAudioStream` @ `0x25108C080`
calls:

```text
APCarPlayAudioFormatsCopyFormatsForAudioType(formats, 103, "media",
                                             &legacyMask, NULL, NULL)
```

Thus the local buffered engine is created from the **legacy** `audioOutputFormats` mask. The extended
array is not requested at this point. Nonzero is necessary but insufficient: inside
`_carPlayAudioFormat_repackageFormatsForStream` @ `0x25107C9B0`, every retained legacy bit for one
audio type must yield the same original ASBD `formatID`. A second codec family takes the `-6705`
path. On wired CarPlay the old `0x1440800` probe is intersected to `0x400800`, retaining bit 11 PCM
and bit 22 AAC-LC; that mixed pair therefore invalidates construction of the complete CarPlay audio
formats object. This directly explains the observed loss of both type 100 and type 103 when
`mainBuffered` was enabled. The new `0x400000` probe retains only AAC-LC, which also satisfies the
function's final type-103 requirement that the buffered family be FourCC `aac `.

The extended list is consumed separately by
`airPlayDescription_isCarPlaySpatialAudioSupported` @ `0x2511541C8`; index 65 is in its accepted
spatial set. The `0x400000` correction is now hardware-confirmed: it produced a real type-103 SETUP
with extended index 65, closing the earlier sender-format hypothesis.

Missing type-103 latency is also not the current gate. The sender's latency scan in
`_carPlayAudioFormat_repackageFormatsForStream` @ `0x25107C9B0` treats omitted `type`, `audioType`,
`sr`, `ss`, and `ch` fields as wildcards. If no entry matches, it assigns zero latency and still
creates the format. The captured MHI2Q `/info` already contains a universal entry with no type or
audio type and `inputLatencyMicros=95000`, `outputLatencyMicros=80000`, so it matches stream 103.

`mainBufferedInfo` may be an empty dictionary; `DCXEnabled` inside it is a separate optional Boolean
queried by `airPlayDescription_isDCXSupportedForSpatialAudio` @ `0x2511545A8`. The hook can add a
real CFLite `true` with `SPATIAL_103_DCX=1` for a controlled Dolby/DCX A/B run. This is a probe-only
capability claim, not proof that the old receiver can decode or render the resulting payload.

### Donor-firmware result: no hidden type-103 receiver

Three newer Audi/Porsche-family AirPlay stacks were checked before implementing a receiver:

| donor | binary | SHA-256 | relevant result |
|---|---|---|---|
| MPR3 US AU P4014 | AArch64 AirPlay 450.14 | `e31ba92d589145c1d3b693bd39f42dff9898aa45ff1447d155af678742a7e081` | `AirPlayReceiverSessionSetup` @ `0x62670` accepts 100/101/102/106/107 and 110/111; 103 reaches `### Unsupported stream type: %d` |
| MHI3 ER AU P4364 | AArch64 AirPlay 450.14.2 | `4c3dee1bfed1554524eaec4d92d152bfd9101cb9b6f0cab4159251002084bf61` | same supported stream set; setup @ `0x63390`; no 103 |
| MH2p US LB636 P2712 | ARMv7 AirPlay 320.17.1 | `f94f1b661be0ff9c1131dbaa2c74c7fe924cd7548e7eb794b5b8b44f15ddc5dc` | same CPU family and a real `AudioConverterNew` @ `0xafc68`, backed by Cinemo AAC; no APAC implementation found |

The 450 stacks do contain real AAC-LC/AAC-ELD/Opus converter code, but not APAC and not the buffered
103 receive state machine. They are useful decoder/API references, not transplantable 103 receivers.
In particular, replacing the MHI2Q 210 library with 320/450 would neither solve 103 nor be ABI-safe.

### Pioneer SPHERA 1.030010: complete native type-103/APAC reference

The Audi/Porsche donors above do not have stream 103, but the latest Pioneer SPHERA firmware does.
**This firmware was not only downloaded: the complete `Pioneer_SPHERA_OTA` package was unpacked and
its Linux root filesystem and relevant binaries were reverse-engineered.** The official source image
is
`/Users/luka/Desktop/AUDI_2/Firmwares/Other/Pioneer_SPHERA_OTA/DMH-WT8000NEX_UC_01030010_20260617.avh`
(version `1.030010`, released 2026-06-17, 277 MiB), with SHA-256
`bd3422e540a618ecf696f22a7eb67539f41b3128a3c0a147d7bc213cad23f461`.

The unpacked package is at
`/Users/luka/Desktop/AUDI_2/Firmwares/Other/Pioneer_SPHERA_OTA/extracted_01030010`. It contains the
decoded `SYSTEM.raw`/`PROGRAM.raw` material, extracted signed partition images, the original rootfs
partition `partitions/01_rootfs_aries-image-lilii.ext4`, a working copy `rootfs.ext4`, and the
mounted/extracted filesystem under `rootfs/`. The ext4 working image has SHA-256
`923744367f0c4a686de9a9654a1e5460abab4770409b6075ec91cfbe63802476`. The analysis below comes
from that extracted filesystem, not from firmware-version metadata or public documentation.

`usr/local/lib/libSPF_CP_PlugInWrapper.so` is AArch64 AirPlay 480.7.4 and contains the complete
implementation missing from the Audi 210/320/450 stacks:

* `getAudioFormats` @ `0x142950` advertises type 100 as audio type `default` and type 103 as
  `media`. The type-103 branch is gated by `s_DeviceSupportsMainBuffered` and initial-info bit 6 and
  adds exact extended indices 65, 66, and 76.
* `_BufferedAudioSetup` @ `0x12c500` creates the TCP server and reader thread at `0x123f08`;
  `_TearDownBufferedStream` is at `0x12c290`. The reader explicitly asks for stream context 103,
  parses APAP, handles loss/retransmit and format/cookie changes, and advances the buffered clock.
* `CarPlayGetBufferedAudioOutputBuf` @ `0x144a40` and
  `CommunicationPluginProxy::getBufferedAudioOutputBuf` @ `0x139ec0` connect the AirPlay buffered
  session to Pioneer's media process. This is a real consumer, not a capability-only advert.
* `APAPDecodeFixedHeader` @ `0x109930`, `APAPDecodeExtension` @ `0x1099f0`,
  `AirPlayReceiverSessionFlushBufferedAudio` @ `0x11a710`, and
  `AirPlayAudioStreamFlushBufferedAudio` @ `0x127e60` provide the missing packet/session contract.

Pioneer's own `AirPlayAudioFormatIndexToASBD` table, executed under Unicorn by
`tools/pioneer_airplay_formats.py`, gives the exact APAC map:

| index | channels | channel-layout tag | PCM equivalent |
|--:|--:|--:|--:|
| 65 | 6 (`5.1`) | `0x00790006` | 44 (`PCM 5.1`) |
| 66 | 8 (`5.1.2`) | `0x00c20008` | 38 (`PCM 5.1.2`) |
| 67 | 8 (`7.1`) | `0x00800008` | 78 (`PCM 7.1`) |
| 68 | 12 (`7.1.4`) | `0x00c0000c` | 84 (`PCM 7.1.4`) |
| 76 | 10 (`5.1.4`) | `0x00c3000a` | 80 (`PCM 5.1.4`) |
| 77 | 10 (`7.1.2`) | `0x00c4000a` | 82 (`PCM 7.1.2`) |

This also settles the terminology: **103 is a stream type**, while 65/66/76 are audio format
indices. The type-100 `default` stream remains the realtime fallback; the genuine APAC spatial
masters are requested on the type-103 `media` stream.

Pioneer integrates the APAC decoder and `SpatialAudioDownMixer` into this library; there is no
separate `libNmeDolby` dependency for this path. `SpatialAudioDownMixerProcess` @ `0x71640` is called
for every output chunk from `CarPlayGetAudioOutputBuffer` @ `0x14b470`. Stock Pioneer renders APAC
to either 4.0 or 4.1: `g_APAC_ch != 5` selects four outputs and
`kAudioMixMatrixSpeakerConfig4_0_AInt`; `g_APAC_ch == 5` selects five outputs,
`kAudioMixMatrixSpeakerConfig4_1_AInt`, and a subwoofer. Bass management is enabled and LFE gain is
-6 dB.

For Audi, the intended target is six outputs with `kAudioMixMatrixSpeakerConfig5_1_AInt` at
`0x4316c8`. Changing only the matrix/count is unsafe: Pioneer also hardcodes 8 or 10 output bytes per
frame for 4/5-channel 16-bit PCM. A 5.1 adaptation must update all of those calculations to 12
bytes/frame together; otherwise the buffer-size mismatch is a credible cause of the same short-play
then teardown/crash behavior seen in earlier experiments.

The stream-103 security contract is now recovered exactly. During SETUP, the receiver reads the
`shk` dictionary value as exactly 32 bytes (`CFDictionaryGetData` @ `0x11b5a4`) and copies it
directly to the stream key slot at `+0x500` (`0x11b704`--`0x11b71c`). There is **no HKDF** on this
path. The ChaCha helper at `0x123aa0` uses the protected body layout:

```text
ciphertext || Poly1305 tag[16] || nonce[8]
```

It uses original ChaCha20-Poly1305 64x64 (eight-byte nonce). Ordinary queued audio authenticates
the original APAP fixed-header bytes `header[4:12]` (8-byte AAD); extension/state packets
authenticate `header[0:12]` (12-byte AAD). The APAP fixed header is 15 bytes. The outer hook capture
continues to use an inclusive big-endian 32-bit frame length.

This was cross-validated rather than inferred only from call arguments: the Pioneer
`chacha20_poly1305_encrypt_all_64x64` implementation @ `0x1bae90` was executed under Unicorn and
produced byte-for-byte identical ciphertext and tag to PyCryptodome's eight-byte-nonce
`ChaCha20_Poly1305`. `tools/pioneer_apap_decrypt.py` implements the recovered frame contract and
tries both legitimate AAD variants per frame. Its `--output-dir` mode preserves each verified body
as a separate file and writes `manifest.tsv`, so audio packets can be passed to a decoder without
destroying their packet boundaries. The earlier raw-`shk` attempts failed because they guessed the
protected-body boundaries and AAD, not because `shk` needed derivation.

### `apac-decode` reference and the macOS APAC oracle

The user-supplied reference is
`/Users/luka/Desktop/AUDI_2/Refferences/apac-decode`. It is the MIT-licensed repository
`https://github.com/fourMs/apac-decode` at commit
`733f9fbc8529be79b83f1ea9162e5470e1d89772`. It contains a small Swift wrapper, README, licence,
and GitHub workflow; it does **not** contain an APAC codec implementation, binary decoder, channel
matrix, or CarPlay receiver. The Swift program delegates decoding to the Apple frameworks already
installed on macOS: it opens the original asset with AVFoundation, selects the first audio track
having at least four channels, requests interleaved Float32 linear PCM through `AVAssetReader`, and
writes the result to CAF. Its CI fallback uses `/usr/bin/afconvert`. The checked-in workflow uses
`macos-26`, even though the README text still says macOS 15 or newer.

That repository was written for APAC captured by an iPhone microphone, described there as
four-channel first-order Ambisonics in ambiX/ACN-SN3D order `W-Y-Z-X`. This is useful proof that
Apple's decoder is externally reachable, but it is **not** evidence for the speaker order or
downmix coefficients of CarPlay's six-channel 5.1 or eight-channel 5.1.2 streams. The important
container finding is that APAC requires its `dapa` configuration/magic cookie. An ordinary FFmpeg
remux can strip that data, so the original Apple container must be retained. On the CarPlay path,
the equivalent per-stream data is supplied before the first compressed frame; Pioneer passes it to
`AirPlayAudioConverterSetProperty(..., 'dmgc', ...)`.

The local bench Mac is macOS 26.5.1 build `25F80`, arm64. `/usr/bin/afconvert` lists `apac` support,
and direct AudioToolbox enumeration confirms both Apple APAC encoder and decoder components. The
decoder is `type='adec' subtype='apac' manufacturer='appl'` and reports supported decode channel
counts 1 through 256, which includes both six-channel index 65 and eight-channel index 66. This is
decoder capability enumeration, not yet proof that a particular CarPlay cookie or channel layout
is accepted.

`tools/apple_apac_oracle.c` is the resulting independent host-side decoder. It does not require a
MOV or CAF wrapper. It validates a captured magic cookie using
`kAudioFormatProperty_FormatInfo`, creates an APAC-to-interleaved-S16LE `AudioConverter`, installs
the same cookie with `kAudioConverterDecompressionMagicCookie`, and feeds raw APAC packets while
preserving each packet boundary and the 1,024-frame packet description:

```sh
mkdir -p build
clang -std=c11 -O2 -Wall -Wextra -Werror tools/apple_apac_oracle.c \
  -framework AudioToolbox -framework CoreFoundation -o build/apple_apac_oracle
build/apple_apac_oracle --probe
build/apple_apac_oracle --inspect cookie.bin
build/apple_apac_oracle cookie.bin apac.pcm frame000.bin frame001.bin
```

The raw-packet path was tested end to end, not merely enumerated. A generated four-channel,
48-kHz, 16-bit test signal could be encoded to CAF/APAC only when its output layout was explicitly
`HOA_ACN_SN3D`:

```sh
afconvert input-4ch.wav output-apac.caf -f caff -d apac -c 4 -l HOA_ACN_SN3D
```

`Ambisonic_B_Format` and an arbitrary eight-channel `WAVE_7_1` APAC encode attempt both returned
`!dat`. That constrains Apple's **encoder** configuration and does not contradict the decoder's
advertised eight-channel support. The successful CAF contained four-channel 48-kHz APAC with
1,024 frames per packet. Its 131-byte cookie began `00 00 00 83 64 61 70 61`, i.e. a complete
`0x83`-byte `dapa` atom. The stream contained 14 packets: 12,000 valid input frames plus 2,048
priming frames and 288 remainder frames, for 14,336 encoded/decoded frames total.

After extracting the complete 131-byte cookie and all 14 compressed packets, the oracle decoded
every packet into exactly 1,024 frames/8,192 bytes of four-channel S16LE PCM. The combined output
was 114,688 bytes with SHA-256
`96c94d0826105fe47c587fd79e8869ce5edbfbacdddab9f4f30c5fecba2ca6a3`.
This proves all of the following: AudioToolbox accepts the complete `dapa` atom as the magic cookie;
the raw packet API works without a container; packet boundaries must be preserved; and the oracle
produces deterministic reference PCM. The generated four-channel HOA cookie is deliberately **not**
a substitute for a real CarPlay 5.1/5.1.2 cookie.

Pioneer's cookie grammar is now reproduced by `tools/pioneer_apac_cookie.py`. A direct `dapa` atom
is accepted, or `frma(apac)` may precede optional `apac` metadata and `dapa`. Decoder payload starts
at `dapa+12`, but its first two bytes are the `0x0800` sync word; GlobalConfig and the exported
sample-rate/frame/channel helpers begin at `dapa+14`. Applying that corrected `+2` offset to the
Apple-generated cookie yields the exact expected 48 kHz, 1,024 frames, and four channels.

For the next controlled CarPlay capture, retain the binary SETUP plist containing `shk`, the raw
inclusive-BE32 APAP capture, and every format/cookie extension. Store the session artifacts with
mode 0600; never print, paste, or commit `shk`. Decrypt into one file per APAP frame so that packet
boundaries are not destroyed:

```sh
umask 077
python3 tools/pioneer_apap_decrypt.py spatial_apap.bin \
  --key-file shk.bin --output-dir decrypted-apap
```

Use `decrypted-apap/manifest.tsv` to identify the verified extension/state body carrying the real
decoder cookie and the ordered audio bodies. Do not concatenate encrypted frames, tags, nonces, or
the APAP fixed headers into the codec input. Inspect the extracted cookie first, then feed only the
ordered APAC packet bodies to the oracle, for example:

```sh
build/apple_apac_oracle --inspect carplay-dapa.bin
build/apple_apac_oracle carplay-dapa.bin carplay-index65.pcm \
  decrypted-apap/000123_audio8.bin decrypted-apap/000124_audio8.bin
```

The exact filenames above are illustrative; their frame numbers must come from the actual manifest.
Start with negotiated index 65 (APAC 5.1), verify six decoded channels and determine their order
with channel-isolated material. Only after stable type-103 5.1 playback should index 66 (APAC
5.1.2) be requested and its eight channels mapped for the 5.1 downmix.

This macOS decoder is a validation oracle, not HU deployment code. It can prove the cookie boundary,
compressed payload boundary, channel count/order, and expected PCM before an ARMv7/QNX decoder or
bridge is implemented. A direct attempt to load Pioneer's full plugin in a generic AArch64 Linux
container reached its dependencies but was abandoned because unrelated Pioneer shared-memory and
process constructors execute at `dlopen` time and fault outside the head-unit environment. The
macOS oracle avoids that environment problem without changing the conclusions recovered from the
Pioneer binary. No HU binary, marker, type-100 PCM/6 fallback, or DIO5 configuration was changed by
this host-side investigation.

The practical target is therefore no longer an undocumented receiver. Reimplement the smallest
Pioneer type-103 subset—SETUP/lifecycle, APAP reader, security, clock/queue, APAC converter, and
downmixer—and bridge its resulting PCM6 to the proven Audi DIO5 DMA path after
fixing the still-stereo CSD render handoff. This is a behavioral
and algorithmic reference, **not a directly loadable donor binary**: Pioneer is AArch64 Linux while
the target MHI2Q userland is ARMv7 QNX. Decode/render remains necessary for audible output, but it
does not explain the fixed five-second teardown; §10 identifies the exact earlier failure as four
missing RTSP buffered-control methods and the sender's 50-retry terminal-error path.

### Hardware-confirmed type-103 request and complete legacy response contract

The corrected `both + legacy 0x400000 + extended index 65 + DCX absent` run reached the transport
plane. iOS sent this real stream descriptor (captured as a mode-0600 binary plist):

```text
{
  type = 103; audioType = media; audioMode = default; isMedia = true;
  audioFormat = 0; audioFormatIndex = 65; packetFormatAPAP = true;
  spf = 1024; clientID = com.apple.Music;
  shk = <32 bytes>; supportsDynamicStreamID = true;
}
```

Stock 210 logged `Unsupported stream type: 103`, returned OSStatus 0, but produced only `{}`. The
iPhone then closed the screen connection (`kConnectionErr -6753`) and restarted the complete
CarPlay session. This was the Play-button "crash": USB/iAP2, feature negotiation, and video were
healthy; the receiver response was incomplete. A type-100 stereo SETUP was also seen immediately
before the 103 request, and DIO5 returned to its six-channel PCM configuration on teardown, so the
existing PCM/6 fallback was not damaged.

The first hook-owned receiver implemented only the obvious legacy pair `{type,dataPort}`. After PF
was corrected to admit 6032, iOS still sent a type-103 TEARDOWN about 4 ms after SETUP, before making
a TCP connection. `/tmp/spatial_apap_*` was never created, and the listener exited with
`total=0 saved=0`. Apple Music showed Dolby Atmos and briefly played the already-running type-100
fallback, then auto-paused and would not resume. This isolated the failure to parsing the type-103
SETUP response, not the firewall, APAP socket, PCM/6 fallback, or Atmos capability negotiation.

The exact reason is visible in iOS 27 `AirPlaySender`'s `_audioStream_resumeAsync_0`:

* `streamID` is read at `0x251110A9C`, but absence is explicitly tolerated; the nearby diagnostic
  says the sender will generate one itself.
* In the no-`streamConnections` branch, `dataPort` is read with an OSStatus out-parameter at
  `0x251110B64` and is mandatory.
* `audioBufferSize` is read the same way at `0x251110D10`; a missing key takes the SETUP error path
  before `APTransportStreamWaitUntilConnected`. This was the immediate teardown.
* `arrivalToRenderLatencyMs` is looked up at `0x251110DB0`; if absent, the sender creates a zero
  number, so this field is optional.

iOS 27 `AirPlayReceiver` confirms the matching producer contract. Its
`AirPlayReceiverSessionSetup` type-103 branch calls `_GeneralAudioSetup`; for this legacy request it
returns:

```text
streams = [ {
  type = 103;
  dataPort = <AudioPort>;
  controlPort = <RTCPPort>;
  audioBufferSize = <receiver buffer size>;
  arrivalToRenderLatencyMs = <receiver latency>;
  streamID = <generated ID>;     // because supportsDynamicStreamID=true
} ]
```

For the current index 65, the exact default is `audioBufferSize=21168000`. The derivation is not a
guess: `APAudioFormatIndexToASBD` gives index 65 format ID `apac`; `APAudioFormatIDToAPCompressionType`
does not map `apac`, so the receiver retains compression type 0, whose branch at `0x250E063A0`
selects 21,168,000. Index 40 has format ID `aac ` and compression type 4; index 42 has format ID
`aace` and compression type 8. Both therefore use the other exact default, 5,000,000, at
`0x250E06394`. The hook chooses between these values from the
actual request's `audioFormatIndex`.

iOS 27's matching receiver implementation returns a `controlPort`, but the current CarPlay sender
does not require or use it for this request shape. In `GeneralAudioSetup`, absence of
`streamConnections` selects the legacy RTP/APAP construction, and the producer returns both
`dataPort` and `controlPort` from `AudioPort` and `RTCPPort` at `0x250E7898C`--`0x250E789D8`.
However, the captured phone request itself has no `controlPort`, and
`audioStream_resumeAsync_0` requires only `dataPort` and `audioBufferSize`; hardware accepted the
response without 6033 and never connected to 6033 after it was added. The modern alternative is a
`streamConnections` dictionary with explicit APAP/RTP/RTCP entries. The receiver accepts APAP
(`'APAP'`) and APAT (`'APAT'`) only on type 103 and validates `shk` as exactly 32 bytes.

Build `3c87e526c6d9da1cc160fc5b5ec946117fa4be08fe406703a1c2be2c68af522b` implements the complete
legacy response. Before publishing it, the hook opens a TCP listener and starts a teardown-aware
drain thread. It saves the first 2 MiB to `/tmp/spatial_apap_<sequence>.bin`, logs the first 32 bytes,
then continues draining without growing the RAM file. This is a transport/capture receiver only:
it deliberately does not label compressed data as PCM and does not touch type 100 or DIO5.

### Hardware-confirmed APAP transport and the five-second pause

The complete response removed the immediate teardown. In two hardware runs the iPhone accepted

```text
{type:103, dataPort:6032, audioBufferSize:21168000,
 arrivalToRenderLatencyMs:0, streamID:21474836481}
```

connected to TCP 6032, and sent approximately 1.2--1.29 MiB in about 4.94 seconds. The captured
`spatial_apap_5.bin` is 1,214,052 bytes (SHA-256
`ed948cec2786dd49bee6f07d835ac321dc6795c4bd57a8064fd9e45b3b74fc5d`). It parses exactly into
3,667 length-prefixed APAP packets with no tail or corrupt length. Each packet carries a 48,000 Hz
timebase; after the initial 44-byte packet, audio timestamps advance by exactly 1,024 samples.
Therefore the sender burst delivered roughly 78 seconds of buffered media in five seconds. This is
real buffered type-103 transport, not the realtime type-100 PCM fallback.

After the burst the peer (the iPhone) closed TCP 6032: the receiver logged `stop=0`, followed by an
orderly iAP2 location stop and CarPlay generation replacement. QNX `sloginfo` contained no SIGSEGV,
SIGABRT, core, dump, or hook/process-crash evidence. The user-visible "five-second crash" is an
orderly sender/session pause and teardown, so the next gate is buffered playback control rather than
SETUP, PF, or APAP framing.

Stock 210's `/feedback` path was recovered to avoid guessing at that control contract:

* `requestProcessFeedback` @ `0x2FE78` calls
  `AirPlayReceiverSessionControl(..., "updateFeedback", ...)` and serializes its response.
* `AirPlayReceiverSessionControl` @ `0x28EE0` returns `{streams:[...]}` and can append only active
  stock type 100/101 entries, each shaped `{type:<100|101>, sr:<value>}`.
* iOS 27 `AirPlayReceiverSessionControl` @ `0x250E6FA50` deliberately returns a successful
  `{streams:[]}` for `updateFeedback`; it does not enumerate buffered 103 there. Consequently an
  empty feedback stream list is valid and absence of a 103 entry is not, by itself, the bug.

Build `e89a3bbeda467a7b9f4f7bdc70069e4087a8f082e92fc8783941573945c40d56` adds a semantics-neutral
interposer on `AirPlayReceiverSessionControl`. While the master marker is present it logs every
command and stock result and writes qualifier/params/response objects as
`/tmp/spatial_controlcmd_<sequence>_*.bplist`. It always calls and returns stock behavior. This build
was used to identify the exact control traffic preceding the peer close before changing any
feedback semantics.

That capture is now complete. During the failing run, `updateFeedback` arrived before and twice
during type 103; stock returned success each time with `{streams:[{type:100,sr:48000}]}`. No
`setRate`, `setRateAndAnchorTime`, or other buffered playback command reached the HU. The iPhone
connected to 6032 and sent 1,477,095 APAP bytes, then closed it after roughly 4.48 seconds. The
session ended with `Reason=0/noErr`; `dio_manager` stayed alive and no signal/core was present. Thus
the user-visible fallback from Dolby to Spatial Audio and subsequent "crash" was again a clean
sender teardown.

The control-port hypothesis was then falsified. The captured SETUP is the legacy CarPlay APAP form:
it has `packetFormatAPAP=true`, `shk`, `spf=1024`, and no `streamConnections`. In iOS 27,
`GeneralAudioSetup` @ `0x250E76C38` selects the legacy RTP/APAP receiver for that shape. Conversely,
the sender's `audioStream_resumeAsync_0` @ `0x25110F454` explicitly rejects APAT for CarPlay; its
separate RTCP connection is not opened for this non-low-latency buffered stream. Hardware confirmed
that after advertising `controlPort=6033`: APAP again connected to 6032, nothing connected to 6033,
and the same clean teardown occurred. APAT feedback/CCFB is therefore not the current gate.

One remaining response mismatch was then tested: while 103 was active, stock 210 kept
reporting its residual realtime stream as type 100. Apple's current receiver instead returns
`{streams:[]}` for `updateFeedback`, including when buffered media is active. Build
`cb8c23abc3c62554459910f4f8b713c8358a5635a82a3e9a8d3c9d96fa697c2a` calls stock first, then
replaces only that `streams` array with `[]` while the hook-owned 103 session is active. Feedback
before 103 setup and after teardown remains stock, so PCM/6, DIO5, and the type-100 fallback are
unchanged.

The hardware A/B with that exact rewrite falsified the feedback hypothesis. The replacement is
visible in the hook log at 25.232, 27.233, and 29.271 seconds, but the phone still closed APAP at
29.366 seconds and then intentionally tore down the session. The capture was 1,596,597 bytes and
parsed into exactly 4,826 complete inclusive-BE32 frames with no tail; the timestamps represent
about 103 seconds of media delivered in roughly 4.5 seconds. The local retained capture is
`/tmp/mhi2_same_cb8c23ab_apap_5.bin`, SHA-256
`3249cde41d698102f10d3e0fe61dbf19b0fabb2f5ad8560645bc75b422f8900f`. There was still no HU
process crash: the screen first reported `kConnectionErr`, the phone requested teardown, and the
session ended with `Reason=0/noErr` and one rebuffer.

Build `1ff579fc86155deed3d09086af73257964d1f4a403b3aac46b4f2cbf658a4e36` added a second TCP listener
on 6033, returns it as `controlPort`, and independently captures the bounded RTCP stream to
`/tmp/spatial_rtcp_<sequence>.bin`. APAP remains on 6032 and type-100 PCM/6/DIO5 behavior is
unchanged. It remains in subsequent builds as diagnostic transport plumbing, but the sender did not
use it in the captured legacy APAP run. The hook still does not decode/render buffered audio.

Build `13aee019f355b1839446a631f3d077e66437c9556ea1b1cf71082845f889aa77` then tested whether the
sender's five-second teardown was caused by the hook draining the entire buffered burst without
backpressure. Its APAP reader parses the inclusive big-endian 32-bit packet length, accepts a maximum
frame of 1 MiB, drains the first 32 frames as prefill, and thereafter sleeps 21.333333 ms per frame
(`1024 / 48000`). The amount accepted before teardown fell from 1,596,597 bytes to only 63,598
bytes, proving that pacing materially changed the transport behavior. Nevertheless the user-visible
sequence was identical: Dolby badge, fallback to Spatial Audio, then pause/teardown at approximately
five seconds. The screen reported its first `kConnectionErr` at 02:06:48.934 and the phone sent
`_requestProcessTearDown` at 02:06:48.969. This falsifies simple over-drain/lack-of-socket-
backpressure as the cause of the fixed timer.

The combined negative results now point to a deeper missing receiver contract. Stock 210 logs
`Unsupported stream type: 103`; the hook currently fabricates the SETUP response and accepts bytes,
but it never creates an internal buffered audio session, installs the supplied security key, decrypts
or decodes packets, advances a playback clock, or registers a renderer. iOS 27's receiver has real
implementations in `APReceiverAudioSessionCreate` @ `0x250E04300`: the legacy
`APReceiverAudioSessionBufferedCreate` path and the feature-selected
`APReceiverAudioSessionBufferedHoseCreate` path. The latter explicitly limits its network queue to 32
frames and suspends reads until playback consumes data. Our pacing A/B emulated only that transport
effect and still failed, so a genuine session/decrypt/decode/render implementation is the leading
hypothesis, not another socket timing tweak. This conclusion is an inference from the matched
five-second behavior; the exact sender-side watchdog condition is not yet named.

> **Superseded by §10 (2026-08-30).** The watchdog condition is now named, and it is not a decode or
> render requirement. The sender asks the *receiver* to start playback over the RTSP control
> connection, using four methods our 210 does not implement; it retries that request 50 times and
> then declares a terminal SetRate error, which is the five-second teardown. Decode is still needed
> for audible output, but it is not what ends the session.

APAP framing and security are now resolved from Pioneer SPHERA 1.030010. The big-endian 32-bit outer
length includes its own four bytes. The APAP fixed header is 15 bytes: signed BE64 media-time value,
signed BE32 timescale, then an unsigned 24-bit big-endian sequence. SETUP `shk` is the direct
32-byte ChaCha20-Poly1305 key; the protected body is `ciphertext || tag[16] || nonce[8]`. Audio uses
fixed-header bytes 4--11 as AAD, while extension/state packets use bytes 0--11. Use
`tools/pioneer_apap_decrypt.py` for the next retained capture; do not repeat the old combinatorial
raw-`shk` guesses.

### Firewall prerequisite: all four profiles, not only `pf.conf`

MU1316's PF rules admit TCP 6030 for the stock screen and drop other inbound `carplay0` traffic.
The existing altScreen work had extended that token to `6030:6031`; type-103 uses TCP 6032 for data,
and the current diagnostic build also reserves TCP 6033 for the optional control-port probe.
Therefore the persistent allow-list must now be
`6030:6033` in **all four** source profiles, because boot selects a profile according to the active
external interface:

```text
/mnt/system/etc/pf.conf
/mnt/system/etc/pf.ecm0.conf
/mnt/system/etc/pf.mlan0.conf
/mnt/system/etc/pf.ppp0.conf
```

Patching only `pf.conf` is insufficient; this car normally loads `pf.ecm0.conf`, and the selection
can change. `deploy/smartphone_integrator/install_persistent_pf.sh` upgrades stock `6030` or either
previous `6030:6031`/`6030:6032` deployment idempotently. After a reboot, verify the active kernel
rule with `/mnt/app/armle/sbin/pfctl -sr`: it must report `port 6030:6033`.

Hardware state for the control-port build: all four `/mnt/system` profiles and RAM copies were
widened to `6030:6033`; a live reload of `pf.ecm0.conf` produced active `port 6030:6033`. Existing
`.pre_spatial103_20260829`/`.stock` backups were preserved.

### Target Dolby decoder: present, licensed, but not APAC

`libNmeAudioDolby.so` is an ARMv7/EABI5 Cinemo wrapper (SHA-256
`1cd02060d4c896ab526cfcf15e6cc34db18c7846d81c7111a4e49c8693c9c28a`) with
`NmeCreateCodecDDPRef` @ VA/file `0x6f38`. At `0xab04` it resolves and loads the sibling module
`NmeAudioDpcm`, looks up the DDPIUDC API, acquires a limited shared-memory Dolby license, and verifies
API version, project number, channel limit, capabilities, and authentication.

The actual Dolby core is `libNmeAudioDpcm.so` (ARMv7/EABI5, SHA-256
`ed447693a1d6848a861ba1becc14a308645b421f9c285176a828723625f4880f`). Its exported entry points are:

| VA/file offset | export |
|---:|---|
| `0x4d84` | `NmeGetDDPIUDCAPIVersion` (returns API `0x00010001`) |
| `0x4fe0` | `NmeCreateDDPIUDC` |
| `0x50d8` | `NmeDDPIUDCProcessMessage` |
| `0x51a4` | `NmeGetDDPIUDCProjectNumber` |
| `0x51c0` | `NmeGetDDPIUDCChannelLimit` |
| `0x51dc` | `NmeDDPIUDCVerify` |
| `0x5378` | `NmeGetDDPIUDCCaps` |

`cinemo_classes.xml` explicitly registers Dolby file extensions `AC3;EC3;EAC3` and the
`NmeCodecDDPRef` class. This proves AC-3/E-AC-3/DD+ decode capability. It does **not** prove APAC
support or an Atmos object renderer; the licensed channel limit still needs a runtime query.

---

## 7. Where everything is

| what | path |
|---|---|
| iOS 27b6 shared cache | `Refferences/ios27_dsc/24A5418b__iPhone18,1/` |
| extracted + IDBs | `.../extracted_dylibs/AirPlaySupport`, `.../carkit_out/CarKit`, `.../aps_out/AirPlaySender`, `.../mt_out/MediaToolbox` |
| current receiver IDB (durable copy) | `.../AirPlayReceiver.i64` |
| APTransport IDB (durable copy) | `.../APTransport.i64` |
| extract more from the cache | `ipsw dyld extract <cache> <ImageName> --output ./out` ; search strings with `ipsw dyld str <cache> --pattern '<regex>'` |
| HU firmware (MU1316) | `Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/` |
| our libairplay (stock 210.81) | `.../extracted/app/eso/lib/libairplay.so` (+ `.i64`) |
| MPR3 donor libairplay | `Firmwares/HU/MPR3_US_AU_P4014/work/targets_iap2_carplay_airplay/libairplay.so` |
| MHI3 donor libairplay | `Firmwares/HU/MHI3_ER_AU_P4364_8W0906961DR/_swup_carplay_aa/extracted_elf_opt/libairplay.so` |
| MH2p donor libairplay | `Firmwares/HU/MH2p_US_LB636_P2712/extracted/app/eso/lib/libairplay.so` |
| Pioneer SPHERA 1.030010 | `Firmwares/Other/Pioneer_SPHERA_OTA/DMH-WT8000NEX_UC_01030010_20260617.avh` |
| extracted Pioneer rootfs | `Firmwares/Other/Pioneer_SPHERA_OTA/extracted_01030010/rootfs/` |
| Pioneer type-103/APAC library | `.../rootfs/usr/local/lib/libSPF_CP_PlugInWrapper.so` |
| Pioneer format-table tool | `tools/pioneer_airplay_formats.py` |
| APAP decrypt/verification tool | `tools/pioneer_apap_decrypt.py` |
| SETMAGICCOOKIE plist tool | `tools/pioneer_magic_cookie.py` |
| AVFoundation APAC reference | `Refferences/apac-decode/` (wrapper/workflow, not codec source) |
| native macOS APAC oracle | `tools/apple_apac_oracle.c` |
| target Cinemo AAC decoder | `.../extracted/app/armle/usr/lib/cinemo/libNmeAudioAAC.so` |
| recovered Cinemo audio ABI | `hook/audio/cinemo_audio_codec_abi.h` |
| authoritative type-103 static contract | `docs/PIONEER_TYPE103_STATIC_SPEC.md` |
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

## 9. Historical controls and experiments (do not enable 5.1 yet)

The control details below remain useful for reproducing the type-103 work, but
the router-width result at the top of this document supersedes their activation
advice. Keep `AUDIO_SPATIAL=0` until the MPL5 CSD stream is dynamically
re-provisioned from the negotiated PCM geometry.

### Safe runtime controls

Type-103 configuration is env-only and is inherited when `carplay_startup.sh` execs `dio_manager`.
Changing it therefore requires a fresh `dio_manager` generation; deleting `/tmp` files has no
effect. On a muted/isolated bench setup:

1. Use `scripts/set_spatial_profile.sh fallback-stereo` on the current driver.
   The former `spatial-5.1`, `spatial-5.1.2`, and `fallback-5.1` experiments are
   suspended until the CSD-width fix. The `status` command is read-only. The
   script does not restart a running session.
2. `SPATIAL_103=1` is the master switch. `0`, `false`, `no`, `off`, empty, or absent disables every
   buffered mutation/capture; the independent type-100 PCM/6 fallback remains unchanged.
3. `SPATIAL_103_MODE` chooses the contract half:
   `feature` = negotiate `mainBuffered` only; `info` = append the type-103 `/info` descriptor only;
   `both` = both changes. Absent defaults to `both`.
4. `SPATIAL_103_INDEX` selects one exact decimal extended format:
   `40` = AAC-LC 5.1.2, `42` = AAC-ELD 5.1.2, `50` = DD+ 5.1, `53` = DD+ 9.1.6,
   `65` = APAC 5.1, `66` = APAC 5.1.2.
   Absent defaults to 65; every other value is rejected and falls back to 65.
5. `SPATIAL_103_EXTRA_INDEX` is normally empty. Values 65/66 append one explicit secondary gate
   index. It exists only to reproduce the completed `[40,65]` AAC negative-control and is not a
   production profile.
6. `SPATIAL_103_MASK` selects the legacy compatibility mask. Absent defaults to the
   corrected `0x400000` single AAC-LC bit. `0x1440800` is accepted only to reproduce the old failed
   mixed-codec probe; all other values fail back to `0x400000`.
7. Optional: set `SPATIAL_103_DCX=1` to add `mainBufferedInfo={DCXEnabled:true}` as a genuine CFLite
   Boolean. `0` leaves the required dictionary empty. Keep it off by default until decode/render
   exists.
8. Reconnect the phone. The hook writes mode-0600 binary plists:
   `/tmp/spatial_control_<sequence>_request.bplist`,
   `/tmp/spatial_control_<sequence>_response.bplist`, and
   `/tmp/spatial_info_<sequence>.bplist`. It also logs every proposed and finally enabled feature.
9. If the phone requests 103, retrieve `/tmp/spatial_setup103_<sequence>_request.bplist`,
   `/tmp/spatial_setup103_<sequence>_hook_response.bplist`, and
   `/tmp/spatial_apap_<sequence>.bin`. The hook logs the stock result, allocated `dataPort`, first
   APAP bytes, total bytes drained, and bounded bytes saved. This path has no separate RTCP/control
   connection on 6033.
10. To restore ordinary negotiation while preserving DIO5 PCM6, select `fallback-5.1` and start a
   fresh `dio_manager` generation.

The feature-only run established that the phone proposes `mainBuffered`, but iOS requires the paired
`mainBufferedInfo` key in `/info`. The broad legacy mask then failed because it mixed PCM and AAC
families. The corrected single AAC bit `0x400000` solved that gate and produced the hardware-confirmed
type-103 request above.

Current installed build is `78e93829d1f4234c2d28905235bb198b6cef3017cd011914b3599c667e020e39`;
it was atomically installed and copied back with the identical SHA-256. Startup selects
`both + legacy 0x400000 + extended index 65 + no extra index + DCX off`. Type-100 PCM/6 and DIO5
remain unchanged as fallback. Index 65 and 66 are both live-proven. Index 40 alone was rejected and
the dual `[40,65]` advertisement selected 65/APAP, so the AAC/Cinemo bypass is closed.

The static pass then found one more mandatory control-plane operation. Codec configuration is not
the SETUP `ct` field and does not arrive on 6033. After media SETUP, iOS sends a custom
`SETMAGICCOOKIE` method on the existing RTSP connection with a binary-plist body containing signed
`magicCookieID` and `magicCookie` CFData. Pioneer parses it at `0x11386c`, stores the first CFData for
each ID, ignores duplicate IDs, and TLV 17 later selects that ID. Stock Audi 210.81 contains neither
the method/keys nor the setter path. Do not add it to OPTIONS/Public: Pioneer's Public list also
omits the custom method, and the sender transmits it anyway.

For MU1316 the exact non-text-patching seam is closed: `AirTunesServer_EnsureCreated@0x2e9ac` places
private request handler `0x302e0` in its callback table at `+0x24`; `HTTPServerCreate` copies it to
the heap server at `HTTPServer+0x64`; the outer server global is image VA `0xbabcc`. Repository
`audio_hook.c` now replaces that slot only after exact prologue/original-pointer checks, handles only
the exact custom method, stores at most 16 cookies of at most 1 MiB, and delegates every other RTSP
method unchanged. A fresh local QNX/ARMv7 build passed with `emutls=0`, SHA-256
`840a3720141b4e7b733f074674b95527614d7693f6e7577e72447360c473e36a`; it was **not deployed**.
Each first accepted cookie is retained in memory and captured mode-0600 as
`/tmp/spatial_magiccookie_<sequence>_<id-hex>.bin` for the decoder bench; teardown restores the
stock request callback.

This private dispatcher path is **MU1316-specific**, not a generic MHI2Q address. Static comparison
against `MHI2Q_CN_AUG22_K1004` found `AirTunesServer_EnsureCreated@0x2e94c` and a different private
image layout. The hook now verifies the MU1316 function prologues and page-aligned image base before
enabling any type-103 private patch. On K1004 or an unknown image it logs a warning and leaves type 103
disabled; type-100 stock fallback remains untouched. Porting 103 to K1004 requires recovering and
fingerprinting that build's private request-handler table rather than reusing MU1316 offsets.

The source and installed build omit `controlPort`/6033 from type-103 SETUP. Only data port 6032 is
needed for this path; keeping the existing broader PF rules is harmless.

This experiment distinguishes three layers without risking speaker damage: CarKit UI capability,
AirPlaySender negotiation, and actual stream transport.

### Exact next steps when type 103 work resumes

1. Do not require a nonzero timeline ID: MU1316's 16-byte `AirTunesTime` has no such field. Keep the
   now-correct 64-bit tick ABI and full transport/network-time response shape, and validate clock
   behavior together with the real decoder/queue/render loop.
2. Repeat `Play -> Pause -> Play`: `SETRATEANCHORTIME rate=0` and the media-only `FLUSHBUFFERED` must
   ACK, its position must advance by the wall playback duration, and the iPhone timeline must not
   jump back.
3. Preserve the existing `SETMAGICCOOKIE` map and APAP TLV-17 ID pairing. It uses the existing RTSP
   connection; do not add a 6033 listener.
4. Implement the exact Pioneer crypto/APAP/TLV state contract and a bounded queue. Reject the
   session on any invalid Poly1305 tag, missing cookie/format, impossible length, or unsupported
   index; never feed encrypted/compressed bytes to DIO5. TLV 17 may select only an ID already stored
   by `SETMAGICCOOKIE`.
5. Implement or port an actual ARMv7/QNX APAC decoder. The macOS AudioToolbox oracle and Pioneer's
   AArch64 Linux decoder are validation references, not deployable target decoders. Start with index
   65 so decoded output is already PCM6; do not send compressed, encrypted, or guessed PCM to DIO5.
6. For index 66, apply the recovered 5.1 matrix: preserve `L/R/C`, attenuate LFE by 0.5, fold each
   height channel into its matching surround at 0.5, use saturating accumulation, and size PCM6
   S16LE output as `frames*12` bytes. Bridge the result into the already proven type-100/DIO5 read
   path only while the 103 session is active; otherwise call stock behavior.
7. Implement the remaining trim, gap/underrun, queue and idempotent teardown behavior. Socket pacing
   alone is not a renderer and has already been falsified as the five-second fix.
8. Decrypt controlled 65/66 captures with `tools/pioneer_apap_decrypt.py`, inspect their cookies with
   `tools/pioneer_apac_cookie.py`, and validate decoder output against `tools/apple_apac_oracle.c`.
9. The fresh source needs only `6030:6032`; reduce all four production PF profiles after the old
   diagnostic build is replaced. Until then leave the current `6030:6033` configuration in place.

The first production milestone is stable index-65 **APAC 5.1 → PCM6 playback**. The second is index
66 **APAC 5.1.2 → PCM8 → tested PCM6 downmix**. Native height rendering remains an amplifier/HU-output
project after the buffered receiver works.

### Remaining low-priority question

Map the individual model labels for `MGGetProductType()` values `0x5BD62321`, `0xA10F2923`, and
`0xFCBBE892` if needed for completeness. The producer and fallback are resolved, and this mapping no
longer affects the project decision: the old gate is local phone state, while the only accessory-side
lever is the separate type-103 buffered protocol.

---

## 10. ⭐ RESOLVED — the five-second teardown is four missing RTSP methods

Static analysis, 2026-08-30. Sender side from `aps_out/AirPlaySender`, receiver side from
`AirPlayReceiver.i64`, both in `Refferences/ios27_dsc/24A5418b__iPhone18,1/`. Nothing here needed
hardware; the unit was offline.

### The finding

After the hoses prime, iOS asks the **receiver** to start playback by sending an RTSP request on the
control connection. iOS 27's own receiver dispatches four such methods in `_HandleHTTPConnectionMessage`
@ `0x250E24B48`:

| method | cstring | handler |
|---|---|---|
| `GETANCHOR` | `0x250ECB249` | `airplayReqProcessor_requestProcessGetAnchor` @ `0x250E40348` |
| `SETRATE` | `0x250ECB253` | `airplayReqProcessor_requestProcessSetRate` @ `0x250E40730` |
| `SETRATEANCHORTIME` | `0x250ECB25B` | `airplayReqProcessor_requestProcessSetRateAndAnchorTime` @ `0x250E40CA4` |
| `FLUSHBUFFERED` | `0x250ECB22C` | `airplayReqProcessor_requestProcessFlushBuffered` @ `0x250E3FEA8` |

A newer POST form also exists at URL `/rateandanchortime` (`0x250ECB386`, same function).

**Our libairplay 210.81 contains none of those strings.** Verified by exact-line `strings | grep -x`
against `Patches/carplay_vc/libairplay.so`: zero hits for all four plus the URL, while `SETUP`,
`RECORD`, `TEARDOWN`, `FLUSH`, `SET_PARAMETER`, `GET_PARAMETER` and `POST` are all present. The 210
CarPlay receiver predates buffered audio entirely.

### Why we never saw it

Our only control-plane instrumentation is the `AirPlayReceiverSessionControl` interposer. An RTSP
method that 210's HTTP dispatcher does not recognise is rejected **before** any session-control call,
so it never reaches that hook. The earlier conclusion "no `setRate` reached the HU" was an artifact of
where we were looking: the request did arrive, and died one layer above.

### The sender-side retry-to-terminal path

1. `bufferedAudioEngine_setRateAndAnchorTimeInternal` @ `0x251136C88` — the local client (Music)
   requests rate 1.
2. `bufferedAudioEngine_handleSetRateToOne_sendAnchorToAllHoses` @ `0x25112D114` — per hose, calls
   hose-control vtable slot `+40` (setRateAndAnchorTime) with an async completion handler.
   `APSAudioProtocolDriverHoseControlGetAnchor` @ `0x251059640` is the same shape on slot `+64`.
   Log strings: `"Starting with SetRateAndAnchorTime"` `0x2511C14BC`, `"Retry GetAnchor."`
   `0x2511C1468`, `"Retry SetRate."` `0x2511C1493`.
3. `bufferedAudioEngine_hoseSetRateAndAnchorTimeCallbackCompletionHandlerInternal` @ `0x25112F12C` —
   retry counter at `hoseDescriptor+172`; while `< 0x32` (50) it re-arms and returns, otherwise it
   calls `bufferedAudioEngine_handleTerminalSetRateError`.
4. `bufferedAudioEngine_handleTerminalSetRateError` @ `0x251059A94` →
   `bufferedAudioEngine_sendSessionFailureEvent` @ `0x25112F8E8` → session teardown.
   `0x2511C1673` spells our case out: `"SetRate 1 FAILURE for hose [...] err = %d SetRate Not Supported"`.

Fifty retries on a sub-100 ms cadence, plus prime time, is the observed ~4.5–5 s — fixed in wall
clock and independent of how many APAP bytes we accepted, which is exactly what both the unpaced
(1,596,597 B) and paced (63,598 B) builds showed.

### A second, longer path exists — not ours

`bufferedAudioEngine_handleSetRateToOne_sendAnchorToAllHoses` first converts the anchor host time to
network time through the engine's clock (`APSNetworkClockConvertUpTicksToNetworkTime`, error string
`0x2511C34C1`). On failure it sets `setRatePendingClockLock` and arms a deadline of
`engine+0x158` ms; `APAudioEngineBufferedCreate` @ `0x250F72884` loads that from pref
`networkClockLockTimeoutMs` with default `MOV W2, #0x2710` = **10 000 ms** (`STR W0,[X21,#0x158]`
@ `0x250F72EEC`). Expiry logs `"Audio Engine waited %d ms trying to resolve clock errors(%d),
dissociating remote endpoints"` and reaches the same `sendSessionFailureEvent`. Error `-71974` is
`"Network clock failed to lock"`, `-71976` is `"Could not agree on grandmaster"`. At 10 s this is not
our five-second teardown, and the retry path above fires first — but it is the *next* wall once the
methods answer, and it is why `bufferedAudioEngine_handleClockLockStateChangedInternal` @ `0x2511381D4`
only calls `sendAnchorToAllHoses` when the clock reports locked.

Clock type is chosen by `endpoint_getPreferredNetworkClockType` (inlined in
`endpoint_ensureSharedContextCreated` @ `0x250D1A24`): it reads a "receiver supports PTP" property
from the endpoint description and falls back to NTP otherwise, logging
`"Receiver supports PTP: %s, NTP forced: %s"` `0x2511FF127`. We declare no PTP, so this session is
NTP. Related accessory-visible levers, if the clock ever becomes the blocker:
`kAPEndpointDescriptionProperty_IsThirdPartyReceiver` and
`kAPEndpointDescriptionProperty_SupportsSharedReceiverClock`, both read by
`APSenderSessionShouldPerformNetworkClockSETPEERS` @ `0x2510FA2BC`.

### Exact wire contract to implement

Bodies are binary plists, same as the SETUP exchange.

**GETANCHOR** — request `{ rate: <uint32> }`. Response on success:

```text
rtpTime, mediaTimeValue, mediaTimeScale          (one APSAudioTransportTime triple)
networkTimeSecs, networkTimeFrac, networkTimeTimelineID, networkTimeFlags
```

On failure the handler instead returns `status: <OSStatus>`. Receiver-side errors it can produce:
`-72152` (no session), `-6718` (no audio session), `-12782` (no callback).

**SETRATEANCHORTIME** — request always carries `rate`; when `rate == 1` it additionally carries

```text
rtpTime, mediaTimeValue, mediaTimeScale
firstAudibleRTPTime, firstAudibleMediaTimeValue, firstAudibleMediaTimeScale
networkTimeSecs, networkTimeFrac, networkTimeTimelineID, networkTimeFlags
```

Response carries `firstAvailableSampleTS`, `firstAvailableMediaTimeValue`,
`firstAvailableMediaTimeScale` when the returned time is valid, plus, for `rate == 1`,
`renderStartNetworkTimeSecs`, `renderStartNetworkTimeFrac`, `renderStartNetworkTimeTimelineID`,
`renderStartNetworkTimeFlags`; `status` on error. Note that Apple's own receiver refuses this when its
clock is not locked (`"Cannot SetRateAndAnchorTime; IsLocked: NO."` `0x250EDD300`) — we are the
receiver, so we define that predicate, but the anchor we return must sit on the phone's NTP timeline
or playback starts at a nonsense instant.

`SETRATE` and `FLUSHBUFFERED` follow the same request-processor shape; decompile
`0x250E40730` / `0x250E3FEA8` when they are needed.

### Where to inject it in our stack

No binary patching required. Every entry point needed to intercept a request and synthesise a
response carries an `R_ARM_JUMP_SLOT` (PLT) relocation in our 210, i.e. it is interposable by the same
`LD_PRELOAD` mechanism the altscreen and audio hooks already use — verified with `objdump -R`:

```text
HTTPMessageReadMessage      HTTPConnectionInitResponse
HTTPMessageCreate           HTTPConnectionSendResponse
HTTPMessageSetBody          HTTPConnectionSendSimpleResponse
HTTPMessageSetBodyLength    CFPropertyListCreateWithData / CFPropertyListCreateData
```

Suggested seam: interpose `HTTPConnectionInitResponse` (or `HTTPConnectionSendResponse`), read the
inbound request's method off the connection, and for the four verbs above build our own 200 plus a
binary-plist body instead of letting stock 210 reject them.

Useful for diagnosis before writing any of that: 210 still contains the CoreUtils RTSP log categories,
they are merely switched off. `gLogCategory_HTTPRequests` (`0xBA6D0`) and `gLogCategory_HTTPResponses`
(`0xBA690`) are exported data symbols; the `LogCategory` layout was read out of `.data` as
`+00 level (int32, -1 = uninitialised)`, `+04 initialLevel`, `+10 name`, stride `0x40`. Both currently
hold `initialLevel = 0xFF` (off), while the live `gLogCategory_AudioStream` (`0xBA490`) holds `30`
(info) — which is why `[AudioStream]` lines appear in slog and RTSP lines do not. Writing `30` into
`+00` and `+04` of the two HTTP categories from the hook's constructor yields a full RTSP transcript
with no protocol code at all.

### Consequence for the plan

§9's "Exact next steps" still hold for *audible* output, but the ordering changes. The first
milestone is no longer decode: it is answering these four methods so the session survives past five
seconds. Order:

1. Implement `GETANCHOR` / `SETRATEANCHORTIME` / `SETRATE` / `FLUSHBUFFERED` (and optionally turn on
   the HTTP log categories first, to see the real requests).
2. Put the returned anchor on the phone's NTP timeline, so the 10 s clock-lock wall is cleared too.
3. Only then decrypt (§6, Pioneer-derived ChaCha20-Poly1305 contract) and decode.
