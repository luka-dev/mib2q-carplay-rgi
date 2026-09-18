# AltScreen MHI2Q — implementation plan (c_hook module)

Builds on `feasibility.md` (primitive inventory) and Cosmin's
proven MHI3 recipe (`../mhi3-altscreen/libaltscreen_hook_ultra.c`).

## Module placement (decided)
Standalone module `c_hook/altscreen/` inside the existing `libcarplay_hook.so`
(same LD_PRELOAD'd .so that already hooks Cinemo iAP2 + libairplay teardown).
NOT a `hook_module_def_t` (that framework is for iAP2 *messages*); this module
patches/calls libairplay C functions directly. Files:
- `altscreen_hook.c` — orchestration, hook bodies, recon (exists).
- ~~`altscreen_patch.c` (inline patcher)~~ — **DROPPED**. Pure LD_PRELOAD
  interposition: VERIFIED 2026-05-25 that libairplay 210.81 calls its own
  exported functions through preemptible `.plt`/GOT thunks
  (`j_AirPlayReceiverSessionSetup`, `j_ScreenStreamProcessData`,
  `j_AirPlayCopyServerInfo`, `j_AirPlayGetFeatures` all `LDR PC,[GOT]`). So an
  LD_PRELOAD'd same-named symbol intercepts even libairplay's *internal* calls —
  no mprotect/byte-patching/ARM-Thumb-encoding risk. (Same mechanism as the
  existing NmeTransport interposition.)
- `altscreen_av.{c,h}` — **NEW**: AVCC parse + Annex-B NAL framing + socket
  forward to the decoder process (ported from Cosmin's `forward_avcc_config`,
  ARM32, no `__thread`).

Plus a **NEW separate binary** `maneuver_render/altscreen_render` (or a sibling): TCP NAL
receiver + OMX H.264 decode + cluster blit, reusing `maneuver_render/platform_qnx.c`
(cluster window + dmdt). Its own `compile_render_qnx.sh` target.

**Build context: no live target yet** — every step must be compile-verifiable on
the QNX VM and self-logging; injection stays gated (`ALTSCREEN_INJECT=0`) until a
device is available to validate the patcher + offsets.

## Architecture (LOCKED 2026-05-24): tap NALs → dedicated decoder process

Decision: the H.264 decoder is its **own separate process** (not inside
libairplay, not inside maneuver_render). Clean separation, restartable/debuggable,
matches the existing "maneuver_render is a separate process driven over TCP" pattern.

```
[libcarplay_hook.so / altscreen module]            [altscreen_render process]
  advertise altScreen (/info inject)                 TCP listen :198xx
  accept type 111 -> 2nd screen session   NALs       recv Annex-B/AVCC NALs
  tap decrypted NALs in ScreenStreamProcessData ───►  OMX H.264 decode
  (short-circuit libairplay's own decode of           -> cluster surface
   the alt stream — see risk below)                   (reuse maneuver_render's
                                                        libdisplayinit + dmdt
                                                        cluster-window code)
                                                      -> MOST encoder -> cluster TFT
```

- The **2nd screen session still must exist** so libairplay receives + **decrypts**
  the 111 stream and fires `ScreenStreamProcessData` with playable H.264 (Cosmin
  confirmed the tap point yields decrypted NALs). We tap there and forward.
- **Risk — double HW decode:** `StartSession` spins up a 2nd `COMXVideoDecoder`+
  `CScreenRender`; a 2nd HW decoder instance may contend/fail on MHI2Q. **Mitigation:**
  in the ssp hook, for the alt (111) stream forward the NALs but **do NOT call the
  real `ScreenStreamProcessData`** → libairplay never decodes the alt stream itself;
  only our decoder process does. (Main 110 stream: always pass through untouched.)
- New process `altscreen_render` reuses `maneuver_render/platform_qnx.c` (cluster window via
  `display_create_window(20)`, dmdt context routing, collision recovery) + adds a TCP
  NAL receiver and OMX H.264 decode → GL/screen blit. Likely a sibling binary built
  by a new `compile_render_qnx.sh` target.

## Hook/call strategy per touch-point — ALL pure LD_PRELOAD interposition
Each interposer exports the same-named symbol, calls the real impl via
`dlsym(RTLD_NEXT)`, and adds our behaviour. (GOT-preemptible — verified.)
| Touch-point | Interposer | Action |
|---|---|---|
| Advertise altScreen feature | `AirPlayGetFeatures` | `return real() \| bit26` |
| `/info` 2nd display | `AirPlayCopyServerInfo` | call real, append alt-display entry to returned dict |
| Accept 111 + 2nd session | `AirPlayReceiverSessionSetup` | call real (does 100/101/110), then create our 2nd screen session + append its `{type:111,port}` to `*respDict` (sidesteps the singleton `sub_26F9C` entirely — no need to de-singleton it) |
| Tap alt NALs | `ScreenStreamProcessData` | if `stream==g_alt_stream` forward+skip-decode, else call real |
| Build 2nd session | call `Screen_Create/SetSecurityInfo/StartSession/DeriveAESKeyForScreen` via `dlsym(RTLD_DEFAULT)` | — |
| Recon (always) | `…ScreenSetDelegate`, `…RequestUI` | pass-through + log |

Note: wrapping `AirPlayReceiverSessionSetup` means we **never touch `sub_26F9C`'s
singleton** — the real handler sets up the main screen as usual, and we add the
alt screen alongside in the wrapper. Cleaner than de-singletoning.

## Component breakdown
1. **ARM32/Thumb patcher** (`altscreen_patch.c`): detect ARM vs Thumb at target;
   build trampoline (save N prologue insns + `LDR PC,[PC,#-4]; <addr>` for ARM,
   or `LDR.W PC,[PC]; <addr>` / `BX`-veneer for Thumb); `mprotect` RWX + memcpy +
   `__clear_cache` (mirror Cosmin's `selfmem_write`, but ARM32 encodings). Reject
   PC-relative prologues. **Highest-risk component — validate on device first.**
2. **/info injection**: hand-build alt-display CFDict. Schema = reverse MHI3
   `AirPlayAltScreenDictCreate @0x5f860` (uuid, type=111, maxFPS, widthPixels/
   heightPixels/widthPhysical/heightPhysical, name, optional altScreenURLs).
   Append to the displays array in the /info path.
3. **Accept 111 + 2nd session**: patch `SessionSetup` so type 111 routes to our
   `altscreen_setup_second(session, streamDict, respDict)` which mirrors `sub_26F9C`
   into our own storage: `Screen_Create` → `DeriveAESKeyForScreen(altID)` →
   `SetSecurityInfo` → `ServerSocketOpen` → echo type 111 in respDict →
   `StartSession`. (Option A: configure its CScreenRender → cluster window here.)
4. **Keep-alive**: rely on the real cluster sink; if iOS still marks inactive, add
   `SetVisibility(alt,1)` + (fallback) `SendCommand` `setScreenRecordingState`.
5. **(Option B)** AVCC/NAL forward → maneuver_render.

## Phasing & validation gates
- **P0 recon** (done in code; needs device): confirm process, live primitive
  addresses, main-screen (110) `Screen_Setup`/`ProcessData` call sequence +
  **MHI2Q struct offsets** (Cosmin's don't port), how main screen binds its window.
- **P1 patcher**: land `altscreen_patch.c`; prove a no-op inline patch on a benign
  libairplay function survives + original still works. Gate before any real patch.
- **P2 advertise**: /info shows 2 displays; iOS proposes altScreen; observe whether
  iOS opens a type-111 SETUP (recon logs it). No 2nd session yet.
- **P3 second session**: accept 111, bring up 2nd screen session; confirm frames
  decode (Option A: appear on cluster window; B: NALs arrive at maneuver_render).
- **P4 stability**: survive the 1-2 s "inactive" window; Apple Maps cluster content
  live on the VC. Add keep-alive only if needed.

## RE findings (2026-05-25) — interposer bodies resolved

### `altscreen_inject_display` — FULLY RESOLVED
Inject point: interpose **`AirPlayReceiverSessionPlatformCopyProperty(a1, a2, a3=propKey, a4, int*a5)`**.
For propKey == `"displays"` (`unk_9D460`) the real fn builds a **CFArray** and appends
the single `Screen_CopyDisplaysInfo` dict → `[main]`. We call real, then append our
alt-display dict → `[main, alt]`.
- Display-dict schema (verified, **MHI2Q keys == MHI3**): `{ type:111, widthPixels,
  heightPixels, widthPhysical, heightPhysical, maxFPS, displayUUID, features }`.
- Keys are CFStrings; build our own via `CFStringCreateWithCString` (value-equal →
  CFDictionary matches). Use `CFDictionarySetInt64` for the ints.
- MHI3 `AirPlayAltScreenDictCreate(out,name,a3,a4,a5,a6,a7,urls)` confirmed the
  arg→key order: a3=maxFPS, a4=widthPixels, a5=heightPixels, a6=widthPhysical,
  a7=heightPhysical (Cosmin's call: name,60,1920,720,180,68,NULL).
- Open: exact CFLite dict key/value-callback symbol for `CFDictionaryCreateMutable`
  (libairplay uses `kCFLDictionary*CallBacksCFLTypes` @0xb84c0/0xb84d8 — try dlsym,
  else `kCFTypeDictionary*Callbacks`); a distinct `displayUUID`; the `features` value.

### `altscreen_handle_alt` — chain resolved (2 sub-items still open)
SETUP stream-entry response (`sub_26EFC`): `respDict["streams"(unk_9E928)]` is a
CFArray of `{ "type":N, "dataPort":port }` dicts. So in the wrapper, after real
SessionSetup, append `{type:111, dataPort:ourPort}` to that array.
2nd-session chain (mirror `sub_26F9C` into our own storage):
1. `Screen_Create(&myScreen)` — calloc 0x1F8 + dispatch queue; `myScreen[122]=-1`.
2. `Screen_UpdateState(myScreen, 1)`.
3. `Screen_Setup(myScreen, altStreamDict)` — stores `latencyMs` (default 70) at +16.
4. `streamID = CFDictionaryGetInt64(altStreamDict, "streamConnectionID"(unk_9E948))`.
5. **`DeriveAESKeySHA512ForScreen(session+456 /*master key*/, 16, streamID, &key,&iv)`**
   — per-stream key from session master + the stream's `streamConnectionID`.
6. `Screen_SetSecurityInfo(myScreen, key, iv)` — `AES_CTR_Init` on ctx at `myScreen+200`.
7. `ServerSocketOpen(...)` → our socket + port.
8. append `{type:111, dataPort:ourPort}` to `respDict["streams"]`.
- **OPEN #1 — RESOLVED (approach A confirmed):** the receive loop is **per-object,
  not a session singleton.** `sub_24150` (`_ScreenStart`) just spawns ONE thread
  `sub_24220` for the session's screen; but `sub_24220` calls
  **`AirPlayReceiverSessionScreen_ProcessFrames(screenObj, netSocket, keyMaterial)`**
  — fully parameterized. So we stand up our **own 2nd screen object + 2nd listen
  socket + our own receiver thread** that replicates `sub_24220`, and libairplay
  does all recv/decrypt/depacketize/decode for the alt stream → fires our tapped
  `ScreenStreamProcessData`.
- **OPEN #2 — RESOLVED:** `SetSecurityInfo(screenObj, key, iv)` is 3-arg; loads
  key/iv into the AES-CTR ctx at `screenObj+0xC8` (+200) (disasm: R1/R2 used,
  AES_CTR_Init on +0xC8). Mirror `sub_26F9C`'s call verbatim with our buffers.

### Receiver thread (replicate `sub_24220` for our 2nd screen)
After SETUP appends `{type:111,dataPort:ourPort}`, spawn `AltScreenReceiver`:
1. `SocketAccept(ourListenSock) -> fd`; `NetSocket_CreateWithNative(&ns, fd)`.
2. `Screen_SetSessionUUID/SetClientDeviceID(myScreen, …)` (from session fields).
3. `Screen_SetTimeSynchronizer(myScreen, cbs)` + `Set{Receive,Auth,NTP}EndTime`.
4. `Screen_StartSession(myScreen, altStreamDict)` → records `g_alt_stream`.
5. `ProcessFrames(myScreen, ns, session+456)` → blocks, receiving/decoding; each
   frame → tapped `ScreenStreamProcessData(ourStream)` → `altav_forward_frame`.
All functions exported/dlsym-able. handle_alt has **no architectural unknowns left**;
remaining = careful coding + a few exact arg orders (DeriveAESKey out-buffers,
SetTimeSynchronizer cbs).

### `AirPlayGetFeatures` advertise — bit value still to confirm
Interpose returns `real() | bit`. Confirm the altScreen bit (MHI3 hardcodes a mask
incl. bit26=0x04000000) and whether features is 32 or 64-bit on 210.81.

## Open RE items (resolve at P0/P1)
- ARM vs Thumb for libairplay 210.81 + exact prologue encodings at patch sites.
- How the MAIN screen session's `CScreenRender` output window is created/bound
  (decides Option A viability + how to retarget the alt one to the cluster).
- `SessionSetup` register contract at the 111 branch (a1=session, a2=streamDict,
  Mutable=respDict) for a clean inline redirect.
- MHI2Q `ScreenStreamPrivate` offsets (type, AVCC ptr/len) — re-derive, don't
  reuse MHI3's +72/+16/+24.
- Which process to LD_PRELOAD (confirm libairplay's host on MHI2Q).
