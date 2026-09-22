# CarPlay AltScreen on MHI2Q — Feasibility & Build Plan (re-opened 2026-05-24)

> Supersedes the "CLOSED / IMPOSSIBLE" verdict in `gate_analysis.md`
> **for the cluster-video angle only.** That doc's *wireless-altScreen-as-Apple-
> intended* gate analysis still stands; what changed is the engineering framing:
> the cluster display is solved (`maneuver_render`), and a full primitive inventory of
> MHI2Q `libairplay.so` 210.81 shows the screen data-plane is present and
> reusable. This document is the live plan for *driving a second (cluster)
> CarPlay screen on MHI2Q by reusing libairplay's own primitives*.

Binaries (all re-opened fresh in IDA, idalib, 2026-05-24):
- MHI2Q `…/MU1316-appimg/eso/lib/libairplay.so` (ARM32 QNX, srcvers **210.81**)
- MHI2Q `…/eso/bin/apps/dio_manager`, `…/armle/usr/lib/libNmeSDK.so`
- MHI3 `_swup_carplay_aa/extracted_elf_opt/libairplay.so` (AArch64, 450.14.2) — positive control
- iOS 26.1 (23B85) decompiled `.mm` — sender-side contract

Companion: `../mhi3-altscreen/` (friend "Cosmin" got the 111 stream on MHI3;
his `libaltscreen_hook merge manual.c` is the architectural template).

---

## 0. The key reframing

Earlier the project looked blocked on three walls. The reverse eliminated all three:

| Wall (feared) | Reality (verified) |
|---|---|
| Must replicate AES/crypto from scratch | ❌ false — keying is **per-stream by stream-ID** |
| No screen-stream receiver in 210.81 | ❌ false — full receiver present, **per-stream** |
| No cluster display sink | ❌ false — `maneuver_render` (Display Manager + MOST) already does it |

What remains is **assembly from existing callable primitives**, not reimplementation.

---

## 1. MHI2Q primitive inventory (libairplay.so 210.81, file-offset == .text VA)

| Capability | Symbol | VA | Notes |
|---|---|---|---|
| Screen frame receive/decode (per-stream) | `ScreenStreamProcessData` | `0x84fec` | operates wholly on passed `ScreenStreamPrivate*` (decoder `+50/52`, AnnexB hdr `+40/44`, NAL buf `+60`). No singleton. |
| Create/start a ScreenStream | `ScreenStreamCreate` / `ScreenStreamStart` | `0x85ce4` / `0x84da0` | |
| Per-stream AES key/IV | `AirPlay_DeriveAESKeySHA512ForScreen` | `0x2d5fc` | derives from labels `"AirPlayStreamKey%llu"` / `"AirPlayStreamIV%llu"` (`%llu` = stream-ID) → **a 2nd stream just calls this with the alt stream-ID** |
| Create a screen session | `AirPlayReceiverSessionScreen_Create` | `0x2b044` | |
| Screen security info | `AirPlayReceiverSessionScreen_SetSecurityInfo` | `0x2a9d8` | |
| Start screen session | `AirPlayReceiverSessionScreen_StartSession` | `0x2a3b8` | ScreenStreamCreate→copy props→ScreenStreamStart |
| Visibility (active) report | `AirPlayReceiverSessionScreen_SetVisibility` | `0x29e80` | → `ScreenStreamSetVisibility`; gated on screen-stream obj `*(s+492)` |
| Screen state machine | `AirPlayReceiverSessionScreen_UpdateState` | `0x29fa8` | NONE/INITIALIZING/READY/STOPPING/STOPPED at `s[125]`, cb at `s[124]` |
| Build display info | `AirPlayReceiverSessionScreen_CopyDisplaysInfo` | `0x2ab88` | single-display today |
| **Generic HU→iOS event** | `AirPlayReceiverSessionSendCommand` | `0x28460` | **POST `/command` RTSP/1.0**, body = binary-plist of `{type:<cmd>, params:{…}}`, content-type `application/x-apple-binary-plist`, via `HTTPClientSendMessage(*(session+704), msg)`; optional reply cb |
| Typed command wrappers (template) | `AirPlayReceiverSessionRequestUI` / `…ChangeModes` | `0x28884` / `0x28aec` | each builds `{type:"requestUI"/"changeModes", params}` and calls SendCommand |
| Stream-type SETUP switch | `AirPlayReceiverSessionSetup` | `0x272b8` | 100/101=audio, 110=`sub_26F9C` screen, **else→"Unsupported stream type"** (rejects 111) |
| 110 screen setup (singleton) | `sub_26F9C` | `0x26f9c` | one screen at `*(s+5140)`, socket `s+5144`, flag `s+5136`, **hardcodes type 110** (`CFDictionarySetInt64(d, type_key, 110)`); derives key from `s+456` |
| Internal control pipe (local) | `AirPlayReceiverSessionScreen_SendCommand` | `0x2a1cc` | `writev` 1-byte cmd to loopback `s+488` — **NOT** the iOS channel |

Command-dict CFString keys (libairplay constants): `type`=`unk_9E938`, `params`=`unk_9EA94`, plus value consts e.g. `requestUI`=`unk_9EAD0`, `changeModes`=`0x9eb38`.

dio_manager screen plug (no altScreen scaffolding — must build dicts ourselves):
`s_sessionScreenCreatedCallback @0x154cec` (→ `ScreenSetDelegate(s_screenStateChanged)`),
`s_sessionCopyProperty @0x157940`, display keys `widthPixels/heightPixels/widthPhysical @0x195ca4`.

---

## 2. iOS 26.1 contract (sender side — what keeps altScreen alive)

The cluster stream is `APEndpointStreamScreen` whose source is a **FigVirtualDisplay**.
The ~1-2 s "inactive" destroy = `screenstream_virtualDisplaySourceTerminated` →
`"display source terminated"` (`AirPlaySender_19.mm:376`). HU must satisfy, in order:

1. Advertise altScreen capability → iOS adds `@"altScreen"` (`CarKit.mm:3698` → `Activate.mm:319`). Needs iAP2 subparam-17 (themeAssets) — see §5.
2. `altScreenURLs` non-empty (else activation aborts **-16721**, `Activate.mm:636`). Comes from cluster-scene-capable apps. **Apple Maps yes, Waze no — inherent, app-side.**
3. HU sends event `updateDisplayInfo` (1-element displays array) — `AirPlaySender_18.mm:4474`.
4. HU provides remote screen port (`screenstream_ensureTransportStream`, `Resume.mm:1016`).
5. **HU sends event `setScreenRecordingState = 1`** — the explicit encode-ON for the 2nd stream (`AirPlaySender_18.mm:4640`). Without it iOS sets up transport but **does not encode**.
6. HU consumes frames within `data send timeout` (`stream+1184`).
7. HU keeps periodic feedback; don't report stream stale (`numStaleStreams`).

`sourceVersion` is telemetry only (no version gate). Events #3/#5 are AirPlay
event messages on the **same `/command` channel** `SendCommand` already speaks.

---

## 3. Build plan (assembly, phased)

### Phase 0 — recon (safe, no behavior change) — *current artifact*
LD_PRELOAD module into the process that maps libairplay (same one the existing
`libcarplay_hook.so` already targets). Resolve every §1 primitive via
`dlsym(RTLD_DEFAULT)` and log addresses. Interpose the cross-module-callable
screen entry points (pass-through + log) to capture the **live** main-screen (110)
call sequence, session/struct pointers, and `/command` traffic. Goal: learn the
exact runtime layout before injecting. → `c_hook/altscreen/altscreen_hook.c`.

### Phase 1 — advertise altScreen + accept 111
- iAP2: inject subparam-17 (themeAssets) into the **USB** transport component
  (Param 20) of the outgoing Identification (wired path) — hook the Cinemo
  `SendIAP2` (see `gate_analysis.md` §Gate-3; raw-byte splice).
- `/info`: post-process the displays array (hook `AirPlayCopyServerInfo @0x22254`
  or `Screen_CopyDisplaysInfo`) to append a 2nd (cluster) display dict
  (schema from MHI3 `AirPlayAltScreenDictCreate`: uuid, type=111, maxFPS, w/h
  pixel+physical, altScreenURLs).
- `SessionSetup @0x272b8`: inline-patch the type switch so 111 is accepted
  (route to a **new** 2nd-screen setup, NOT `sub_26F9C` which is singleton).
- Diagnostic goal: see iOS open a type-111 transport.

### Phase 2 — instantiate the 2nd screen session (reuse primitives)
For the 111 stream, replicate `sub_26F9C`'s orchestration into **our own** storage:
`Screen_Create` → `DeriveAESKeySHA512ForScreen(altStreamID)` → `SetSecurityInfo`
→ `ServerSocketOpen` (2nd socket) → `StartSession`. All calls exist.

### Phase 3 — drive the iOS control events (keep it alive)
Via `AirPlayReceiverSessionSendCommand(session, dict, cb, ctx)` build & send:
`{type:"updateDisplayInfo", params:{displays:[…]}}`, then
`{type:"setScreenRecordingState", params:{… on=1 …}}`; call `SetVisibility(alt,1)`
+ `UpdateState(READY)`; keep feedback. Exact params: take from Cosmin's working
MHI3 set + live recon.

### Phase 4 — frames to cluster
Tap the 2nd stream's NALs in `ScreenStreamProcessData` (by `*(ss+72)`==111) →
forward over socket → `maneuver_render` (decode + Display Manager displayable → MOST →
cluster). `maneuver_render` already owns the cluster surface; add a YUV/H.264 texture path.

---

## 4. Open integration questions (resolve on-device / from Cosmin)
- Does iOS 26.1 require `setScreenRecordingState` for 111, or auto-start on RECORD
  (as the 110 main screen does)? — live test.
- Exact `params` for `updateDisplayInfo` / `setScreenRecordingState` (binary-plist
  shape). — Cosmin's working set + iOS RE.
- Which process maps libairplay on MHI2Q (LD_PRELOAD target) — confirm on device.
- Wired transport: confirm iOS initiates the altScreen stream over USB (MHI3 test
  was wireless; subparam-17 goes in Param 20 for wired).

## 6. Cosmin's proven MHI3 recipe (`../mhi3-altscreen/libaltscreen_hook_ultra.c`) → MHI2Q port

The **final working** MHI3 hook (got the 111 stream) is simpler than feared — it
does **NOT** send `setScreenRecordingState`/`updateDisplayInfo`/any `SendCommand`.
Its recipe: force the capability + build a correct 2-display `/info` + keep the
local displayable setup from failing → the native screen pipeline + iOS bring up
the 111 stream → tap the NALs. So §2 items #3/#5 (explicit control events) are a
**fallback, not the primary path** — try the recipe below first.

Working MHI3 hooks: (1) `HasFeatureAltScreen→1`; (2) `addMainDisplay`→native
`addAltDisplay` (2-display /info); (3) `dint_create_displayable_media`→fake
96-byte stub on real-returns-0 (prevents "inactive" teardown by giving a local
sink); (4) `ScreenStreamProcessData`+`SetAVCC`→tap NALs (SPS/PPS via AVCC parse)
→ forward.

### MHI2Q port mapping (two native pillars are ABSENT on 210.81 — hand-build them)
| MHI3 native | MHI2Q | Port action |
|---|---|---|
| `HasFeatureAltScreen→1` | no such fn | inject altScreen into `/info`+features (`AirPlayCopyServerInfo @0x22254` / `Screen_CopyDisplaysInfo @0x2ab88`) |
| native `addAltDisplay` | absent | **hand-build** alt-display dict; reverse MHI3 `addAltDisplay` / `AirPlayAltScreenDictCreate @0x5f860` key-set for the schema |
| `dint_create` fake stub | `maneuver_render` is a **real** sink | route to real cluster surface; fake only if teardown still fires |
| `ScreenStreamProcessData` tap (MHI3 AVCC @ +16/+24, type @ +72) | present `@0x84fec`, **AVCC @ +40/+44**, decoder @ +50/+52 | tap there; re-confirm MHI2Q offsets (do NOT reuse MHI3's) |
| native 2nd screen session | singleton `sub_26F9C` | create 2nd session manually (Create→`DeriveAESKeySHA512ForScreen(altID)`→SetSecurityInfo→`ServerSocketOpen`→StartSession) |

Reusable nearly verbatim (adapt ARM32 + no `__thread`): `forward_avcc_config`
(AVCDecoderConfigurationRecord → SPS/PPS), Annex-B NAL framing + socket forward,
cmd socket. Forward target = `maneuver_render` (not Cosmin's `:19820` external decoder).

## 5. Hard constraints (QNX)
- **No `__thread`** in the LD_PRELOAD'd .so (QNX 6.5 ARMv7 gcc 4.4 emutls trap) —
  use globals + `pthread_mutex` / `pthread_key`. (Cosmin's MHI3 hook uses `__thread`;
  do **not** copy that part.)
- ARM32/Thumb inline patching (BX trampoline), not AArch64 — Cosmin's patcher
  does not port.
- Build via `compile_hook.sh` (ntoarmv7-gcc 4.4, `-std=gnu99`).
</content>
