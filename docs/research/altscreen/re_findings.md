# MHI2Q AltScreen — raw RE reference (addresses, signatures, offsets)

All from fresh IDA (idalib) static analysis, 2026-05-23..25.
Binaries:
- **MHI2Q** `…/MU1316-appimg/eso/lib/libairplay.so` — ARM32 QNX, srcvers **210.81**, `.text` VA == file offset.
- **MHI2Q** `…/eso/bin/apps/dio_manager`, `…/armle/usr/lib/libNmeSDK.so`.
- **MHI3** `…/_swup_carplay_aa/extracted_elf_opt/libairplay.so` — AArch64, 450.14.2 (reference).

> Symbols are reached via `dlsym` in the hook (not hardcoded). Addresses here are
> for navigation/verification only; the `j_*` thunks are PLT, the bare names .text.

## libairplay 210.81 — interposition is sufficient (PLT/GOT preemptible)
Internal calls go through preemptible `.plt`/GOT thunks → an LD_PRELOAD same-named
symbol intercepts even libairplay's *own* internal calls. Verified:
| thunk | loads PC from GOT | real impl |
|---|---|---|
| `j_AirPlayReceiverSessionSetup` @0x18c04 | — | `AirPlayReceiverSessionSetup` @0x272b8 |
| `j_ScreenStreamProcessData` @0x1a644 | GOT @0xb9740 | `ScreenStreamProcessData` @0x84fec |
| `j_AirPlayCopyServerInfo` @0x1abb4 | GOT @0xb9910 | `AirPlayCopyServerInfo` @0x22254 |
| `j_AirPlayReceiverSessionScreen_Setup` @0x1a818 | GOT @0xb97dc | `…Screen_Setup` @0x2a868 |
→ **No inline patching / mprotect / ARM-Thumb encoding needed.**

## Screen-stack functions (MHI2Q)
| Symbol | VA | Notes |
|---|---|---|
| `AirPlayReceiverSessionSetup` | 0x272b8 | stream switch: 100/101=audio, 110→`sub_26F9C`, else→"Unsupported stream type" |
| `sub_26F9C` | 0x26f9c | 110 screen setup. **Singleton**: screen obj `*(s+5140)`, listen sock `s+5144`, done-flag `s+5136`, hardcodes type 110. key master `s+456`. |
| `sub_26EFC` | 0x26efc | appends a stream entry to `respDict["streams"]` (the response array) |
| `sub_24150` | 0x24150 | `_ScreenStart`: spawns ONE receiver thread `sub_24220(session)`; flag `session+1288` |
| `sub_24220` | 0x24220 | `AirPlayScreenReceiver` thread: `SocketAccept(s+5144)`→fd→`NetSocket_CreateWithNative`→`StartSession`→**`ProcessFrames(screenObj, netSocket, s+456)`** (per-object, NOT singleton) |
| `AirPlayReceiverSessionScreen_Create` | 0x2b044 | `calloc(1,0x1F8)` + dispatch queue; sets `obj[122] = -1` |
| `AirPlayReceiverSessionScreen_Setup` | 0x2a868 | stores `latencyMs` (key `unk_A0098`, default 70) at `obj+16` |
| `AirPlayReceiverSessionScreen_SetSecurityInfo` | 0x2a9d8 | **3-arg(obj,key,iv)**: `AES_CTR_Init` on ctx at `obj+0xC8` (+200); inited flag `obj+0x1E4` (+484) |
| `AirPlayReceiverSessionScreen_StartSession` | 0x2a3b8 | `ScreenStreamCreate`(→`obj+492`)+copy props+`ScreenStreamStart` |
| `AirPlayReceiverSessionScreen_ProcessFrames` | 0x2b154 | the recv/decrypt/depacketize/decode loop (per-object) |
| `AirPlayReceiverSessionScreen_SetVisibility` | 0x29e80 | →`ScreenStreamSetVisibility`; gated on stream obj `*(s+492)` |
| `AirPlayReceiverSessionScreen_UpdateState` | 0x29fa8 | state at `obj[125]`, cb `obj[124]`; 0=NONE 1=INIT 2=READY 3=STOPPING 4=STOPPED |
| `AirPlayReceiverSessionScreen_CopyDisplaysInfo` | 0x2ab88 | builds ONE display dict from `ScreenCopyMain` props |
| `AirPlayReceiverSessionPlatformCopyProperty` | 0x1d6c0 | prop `"displays"`(`unk_9D460`) → **CFArray [oneDisplay]** ← **inject point** |
| `AirPlay_DeriveAESKeySHA512ForScreen` | 0x2d5fc | key/iv from `"AirPlayStreamKey%llu"`/`"AirPlayStreamIV%llu"`, `%llu` = stream-ID |
| `ScreenStreamProcessData` | 0x84fec | per-stream (operates on passed obj); decoder `obj+50/52`, AnnexB `obj+40/44`, NAL buf `obj+60` |
| `AirPlayReceiverSessionSendCommand` | 0x28460 | generic HU→iOS: POST `/command` RTSP/1.0, binary-plist `{type,params}` |
| `AirPlayReceiverSessionRequestUI` | 0x28884 | template: builds `{type:"requestUI",params}`→SendCommand |
| `AirPlayGetFeatures` | 0x1b904 | features value for /info (key `unk_9DFF0`) ← advertise lever |

## CFDict key strings (MHI2Q rodata)
| key | addr |
|---|---|
| `streams` | 0x9e930 (also `unk_9E928`) |
| `streamConnectionID` | 0x9e950 (`unk_9E948`) ← the AES stream-ID |
| `dataPort` | 0x9e96c (`unk_9E964`) |
| `timingPort` | 0x9e980 |
| `latencyMs` | 0xa00a0 (`unk_A0098`) |
| `displays` | 0x9d468 / 0x9df74 (prop sel `unk_9D460`) |
| display dict: `displayUUID` | `unk_A00BC` |
| display dict: `features` (flags) | `unk_A00CC` |
| display dict: `maxFPS` | `unk_A0010` (=0xa0018 str) |
| display dict: `widthPixels` | `unk_A006C` (=0xa0074 str) |
| display dict: `heightPixels` | `unk_A0080` (=0xa0088 str) |
| display dict: `widthPhysical` | `unk_A00E0` (=0xa00e8 str) |
| display dict: `heightPhysical` | `unk_A00F8` (=0xa0100 str) |

## MHI3 alt-display schema (positive control)
`AirPlayAltScreenDictCreate @0x5f860 (out, name, a3,a4,a5,a6,a7, urls)`:
`{ name:a2, type:111, maxFPS:a3, widthPixels:a4, heightPixels:a5, widthPhysical:a6,
heightPhysical:a7, [altScreenURLs:a8] }`. Cosmin's call: `(name,60,1920,720,180,68,NULL)`.
MHI3 display keys (string adjacency): widthPixels@0x1180f8, heightPixels@0x118110,
displayUUID@0x1182f8, maxFPS@0x118458, widthPhysical@0x118468, heightPhysical@0x118480,
features@0x118498, primaryInputDevice@0x1184b0; `displays`@0x115218, `features`@0x115150.
MHI3 has native `HasFeatureAltScreen`@0x58ce0 + `addAltDisplay` (both ABSENT on MHI2Q).

## dio_manager (MHI2Q) — 0 altScreen scaffolding
Plug callbacks: `s_sessionScreenCreatedCallback`@0x154cec (→`ScreenSetDelegate(s_screenStateChanged)`),
`s_sessionCopyProperty`@0x157940, `s_handleSessionControl`@0x157b7c. iOS-facing AirPlay
API set is pre-Ferrite (`RequestUI`,`ChangeModes`,`SetNightMode`,`SetLimitedUI`,`TearDown`,
`ChangeModes`,`RequestSiriAction`) — no `setScreenRecordingState`/`updateDisplayInfo`.

## Cinemo iAP2 (Gate-3 blocker context)
`libNmeSDK.so`+`dio_manager`: only `CINEMO_METANAME_IAP_SUPPORTS_IAP2_CONNECTION` +
`…_SUPPORTS_CARPLAY`. No `WirelessCarplayTransportComponent`/subparam-17/themeAssets.
Wire surface = raw `ICinemoIAP_SendIAP2`/`ReadIAP2`. (See gate_analysis.md.)
</content>
