# MHI3 CarPlay AltScreen — Master Knowledge Base (A→Z)

**Last updated:** 2026-05-17
**Scope:** Everything we know, did, tried, abandoned, and concluded about
enabling Apple CarPlay AltScreen (instrument-cluster / Virtual Cockpit
display) on the Audi MHI3 head unit by reverse-engineering and binary-patching
its CarPlay stack.

This is the single source of truth. Individual session writeups live in
`findings/*.md`; this document consolidates all of them plus the live RE.

---

## 0. Goal

Make an iPhone, when connected to the Audi via CarPlay (wireless), render a
secondary CarPlay "AltScreen" surface onto the car's instrument cluster
(Virtual Cockpit). Apple calls this **altScreen** / gauge-cluster-screen.
Stock Audi MHI3 firmware ships single-screen CarPlay only; the altScreen
feature is present in the SDK remnants but disabled at the platform-plug
level.

The end state requires three things to all be true:
1. iOS must *accept* altScreen during the CarPlay handshake (protocol gate).
2. iOS must *keep* the session alive (not tear down after acceptance).
3. The head unit must *render* the secondary H.264 stream onto a real
   cluster display surface and route input back.

As of 2026-05-17 we have **#1 solved**, **#2 under active diagnosis**, and
**#3 not started**.

---

## 1. Target system (confirmed live via SSH)

```
uname:    Linux mib3h-ivi 4.4.36 #1 SMP PREEMPT aarch64 GNU/Linux
arch:     aarch64 (ARM64)
OS:       general embedded Linux (NOT QNX — QNX was MHI2)
glibc:    2.26  (libs in /gpl/lib/, libc.so.6 → /gpl/lib/libc.so.6)
init/sup: enforcer + smartphone_integrator (process supervisor w/ JSON config)
security: AppArmor in enforce mode, profiles in /etc/enforcer/apparmor/
```

Access: `mhi3_ssh.sh` → `ssh root@0.tcp.ngrok.io -p 15230` (ngrok tunnel).
No `ps`, `pkill`, `pgrep` on device — use `/proc/[0-9]*/comm` to find PIDs.

### Process model

- `smartphone_integrator` — supervisor, always up. Spawns `dio_manager` lazily.
- `dio_manager` — the CarPlay worker. **Only spawns when an iPhone connects.**
  - dlopens `libairplay.so` + Cinemo iAP2 stack (`libNme*.so`, `libesoiap2.so`) lazily.
  - Hosts the AirPlay receiver session + iAP2 control channel.
- Firmware variant under test: `MHI3_ER_AU_P4364_8W0906961DR` (original Audi FW).
- libairplay version string: `srcvers=450.14.2`.

### Key on-device paths

| Path | What |
|---|---|
| `/opt/smartphone_integration/bin/dio_manager` | CarPlay worker binary (we patch this) |
| `/opt/smartphone_integration/bin/dio_manager.stock` | our stock backup (md5 `eee053c9340f15f3140456cb7a8b574d`) |
| `/opt/smartphone_integration/lib/libairplay.so` | Apple/Cinemo AirPlay receiver (we patch this) |
| `/opt/smartphone_integration/lib/libairplay.so.preloghttp` | backup before HTTP-log patch (md5 `6b4ccd0b5cb9e412fcb1501da657d8c5`) |
| `/opt/smartphone_integration/lib/libesoiap2.so` | iAP2 protocol library |
| `/etc/enforcer/apparmor/dio_manager` | AppArmor profile for dio_manager |
| `/etc/enforcer/apparmor/smartphone_integrator` | parent profile w/ transition rule |
| `/opt/smartphone_integration/altscreen_stream` | friend's earlier hook attempt — IGNORE |

---

## 2. How CarPlay AltScreen works (the protocol, both sides)

### 2.1 iOS-side activation flow (decoded from iOS 26.1 / build 23B85 frameworks)

Decompiled `CoreAccessories.framework` / AirPlaySender / CarKit `.mm` files.
The flow when the HU's `/info` advertises altScreen:

```
1. HU sends GET /info response with displays array [main, alt(type=111)]
2. iOS parses displays → CARSessionConfiguration.mm:912-915 stores _displays
                       → CARSessionConfiguration.mm:956 _hasGaugeClusterScreen = true
3. iOS scans enabledFeatures, sees @"altScreen"
                       → Activate.mm:319 sets *(v44+63) = 1  (the altScreen bit)
4. iOS validates display count vs feature
                       → Activate.mm:844-859: altScreen ⇒ require ≥ 2 displays
5. iOS validates each negotiated info-key present
                       → Activate.mm:946-1012 (hevcInfo, mainBufferedInfo, etc.)
                         No altScreenInfo key required — good.
6. iOS fetches CarPlay apps with cluster scenes
                       → Activate.mm:627 APCarPlay_CRFetchInstrumentClusterURLs
                       → CarKit.mm:2962 → XPC to carkitd → returns CFArray of URLs
7. iOS attaches URL list to activation request
                       → Activate.mm:660 CFDictionarySetValue(theDict,@"altScreenURLs",v67)
8. iOS sends activation request (incl altScreenURLs) to HU over AirPlay control channel
9. HU is expected to: store URLs, ACK positively, then drive a cluster scene
10. If HU doesn't progress → iOS times out / tears down
```

iOS error codes seen / mapped:
| Code | Source | Meaning |
|---|---|---|
| -16720 | Activate.mm LABEL_415 | Generic activation failure |
| -16721 | Activate.mm:649 | kCFLNotPreparedErr — cluster URL fetch empty |
| -6740 | libairplay propagated | kCFLNotPreparedErr — session-not-prepared |
| -1581397112 / 0xA1BDCB88 | runtime, not a source literal | likely CFL/Mach error from activation timeout |

### 2.2 HU-side gates (from libairplay.so + dio_manager RE)

```
iPhone                                          Head Unit
──────                                          ─────────
(0) Bonjour TXT srcvers=450.14.2          ─→    passes regardless
(1) iAP2 Identification (Param 21 sub17    ─→   libesoiap2.so emits sub17
    themed-assets capability)                   UNCONDITIONALLY → PASS on MHI3
(2) CarKit checks supportsThemeAssets     ←─               PASS
(3) GET /info request                     ─→    AirPlayCopyServerInfo @ 0x4F900
    reads displays array                  ←─    platform plug via server->cb[+0x30]
                                                STOCK: returns single display
                                                PATCHED: we inject alt-display
(4) RTSP SETUP enabledFeatures:[…altScreen]─→   sub_50990 parses, writes bool to
                                                *(session+0x18)+0x127
(5) HasFeatureAltScreen reads +0x127      ←─    friend patch: forces return 1
                                                enables stream type 111 setup
(6) RTSP RECORD type=111, H.264 frames    ─→    ScreenStreamProcessData @ 0x103120
                                                receives NALs (AVCC framed) — NO SINK
```

| Gate | Symbol / location | Why it fails on stock Audi |
|---|---|---|
| 3 | `AirPlayReceiverServerPlatformCopyProperty` → `server->cb[+0x30]("displays")` | Plug can't build 2-display dict → -6740 |
| 5 | `AirPlayReceiverSessionHasFeatureAltScreen @ 0x58CE0` reads `*(session+0x18)+0x127` | Byte never set (gate 3 already failed) |

Gate 3 fails *first*. That's why simply flipping `enabledFeatures.altScreen:true`
in stock `dio_manager.json` instantly tears the session down with -6740 (proven,
`findings/altscreen_true_fails.md`).

---

## 3. Strategy evolution — what we tried

### 3.1 Attempt 1: LD_PRELOAD hook — ABANDONED

Plan: preload a `.so` into dio_manager that hooks libairplay symbols via
inline AArch64 trampolines (`LDR x17,#8 ; BR x17 ; <addr>`), resolving targets
with `dlsym(RTLD_DEFAULT,…)`.

Built `src/altscreen_hook.c` (full) and `src/altscreen_hook_min.c` (minimal).
Compiled with `zig cc -target aarch64-linux-gnu.2.24` to match device glibc.

**Result: FAILED universally.** Even an empty `.so` whose only constructor is
`open("/tmp/x")` causes dio_manager to fail/crash on load. Proven by bisecting
down to a do-nothing `.so`. Conclusion: dio_manager refuses any LD_PRELOAD
(likely AppArmor + the enforcer's strict launch environment, or a sanity check
on its own maps). Documented in `findings/dio_crash_with_hook.md`.

**Pivot: binary patching instead.**

### 3.2 Attempt 2: binary patching — CURRENT APPROACH

Patch the on-disk binaries directly. No new `.so`, no runtime injection.
Use inline trampolines / code-cave overwrites of verified-unused functions.

---

## 4. Binary-patching toolchain & techniques

### 4.1 Cross-compile (for the abandoned hook, kept for reference)

`zig cc -target aarch64-linux-gnu.2.24` — matches device glibc ≤ 2.26.
Installed zig via Homebrew. `src/Makefile` auto-detects zig cc / aarch64-gcc.

### 4.2 AArch64 patching primitives we use

- **Inline trampoline (absolute):** `LDR x17,#8 ; BR x17 ; .quad target` (16 bytes).
- **PC-relative branch:** `B target` / `BL target` (26-bit signed imm × 4, ±128 MB).
- **Code cave:** overwrite an *exported but never-called* function (verified zero
  code xrefs) with our thunk, then redirect a `BL` at the patch site into it.
- **Instruction encoders** (hand-rolled in Python, in `patch_dio_manager.py`):
  `bl`, `b`, `adr`, `adrp`, `add_imm`, `movz`, `movk`, `stp_pre`, `stp_off`,
  `ldp_off`, `ldp_post`, `str_imm`, `ldr_imm`, `cbz`, `mov_reg`.

### 4.3 Hard-won encoding gotchas

- `MOV Xd, SP` must be `ADD Xd, SP, #0` — the ORR-based `mov_reg(d, 31)` encodes
  reg 31 as **XZR**, producing `MOV Xd, XZR` (zeroes the reg). Bit us in v1.
- Frame sizes: STP pre-index imm7 is scaled by 8 (so `#-0x180` → imm7 = -48).
- Always disassemble-verify after patching; don't trust the encoder blindly.

### 4.4 ELF VA → file offset

- `libairplay.so`: LOAD0 (text) f_off=0 vaddr=0 → **VA == file offset** for .text.
  LOAD1 (data) f_off=0x1607d0 vaddr=0x1707d0 → **.data file_off = VA − 0x10000.**
- `dio_manager`: LOAD0 starts at file offset 0 → text VA == file offset.

### 4.5 AppArmor requirement (critical)

The patched dio_manager won't even exec unless:
1. The `dio_manager` AppArmor profile is loaded (`enforce`).
2. After swapping the binary you MUST reload **both** profiles so the parent's
   transition rule rebinds:
   ```
   /sbin/apparmor_parser -B -r /etc/enforcer/apparmor/dio_manager \
                                /etc/enforcer/apparmor/smartphone_integrator
   ```
   Reloading only `dio_manager` → "profile transition not found" denial. Learned
   the hard way (`findings/dio_manager_patch_iAP2_teardown.md`).

---

## 5. The patches (in detail)

### 5.1 dio_manager v1 — alt-display dict injection (superseded by v2)

- **@0x2B57F4**: `BL .CFArrayAppendValue` → `BL 0x2E8280` (redirect to thunk).
- **@0x2E8280** (overwrites unused `dio::display::addAltDisplay`, 164-byte cave):
  1. original `CFArrayAppendValue(array, main_dict)`
  2. build name CFString "Audi AltScreen" (ASCII enc 0x600) via `.CFStringCreateWithCString`
  3. `.AirPlayAltScreenDictCreate(&alt, name, fps=60, w=1920, h=720, wphys=180, hphys=68, NULL)`
  4. append alt-dict to array, release.
- **Result:** first-ever `[AirPlay] ### Alt Screen supported: Y` on MHI3.
  But session tore down ~7 ms later with airplay -6740.

### 5.2 dio_manager v2 — alt-display + viewAreas (CURRENTLY DEPLOYED)

`src/patch_dio_manager.py` — the active patcher. Two byte changes:

- **@0x2B57F4**: `BL` redirected to `0x2E8A10` (`87 cc 00 94`).
- **@0x2E8A10** (216-byte thunk overwriting unused `dio::display::addAdjacentAreas`,
  236-byte cave):
  1. original `CFArrayAppendValue(array, main_dict)`
  2. build name CFString "Audi AltScreen" (ASCII)
  3. `.AirPlayAltScreenDictCreate(&alt, name, 60, 1920, 720, 180, 68, NULL)`
  4. build single-Surface `std::vector` on stack `{x=0,y=0,w=1920,h=720}`
  5. `dio::view::addAlternateViewAreas(alt, &vec, 0, &empty_vec)` @ 0x2E8800
  6. append alt to array, release.

Detection sentinels in the patcher:
- `ORIGINAL_CALL_INSTR = 23 93 fc 97`
- `V1_CALL_INSTR = a3 ca 00 94`
- `PATCHED_CALL_INSTR = 87 cc 00 94` (v2)

Commands: `--verify`, `--revert`, `--dump-thunk`, default = patch.

**Result:** still 6 ms teardown with `Reason=0/0x0 noErr` (clean iOS-initiated).
The viewAreas theory (Activate.mm:1440-1577) was wrong — adding them didn't help.

### 5.3 libairplay friend patches (Cosmin, already on device)

- `HasFeatureAltScreen @ 0x58CE0` → `MOV W0,#1 ; RET` (forces return 1).
  Patcher: `src/patch_libairplay.py`. ORIGINAL `c0 00 00 b4 / 00 0c 40 f9`.
- `SessionSetup @ 0x63390`: `B.EQ`→`B` at `0x63DF0` (forces stream 111 through
  MainScreen setup path).

These are inert unless dio_manager's `/info` advertises altScreen.

### 5.4 libairplay HTTP-log enable (THIS SESSION, deployed)

`src/patch_libairplay_loghttp.py` — 2-byte data patch:
- `gLogCategory_HTTPRequests.defaultLevel` @ VA 0x178cb4 (file 0x168cb4): `0xff`→`0x14`
- `gLogCategory_HTTPResponses.defaultLevel` @ VA 0x178d14 (file 0x168d14): `0xff`→`0x14`

Activates the built-in `LogHTTP()` @ 0xb0dc0 which prints every RTSP/HTTP
request+response (`"HTTP Request:  %.*s\n"`, `"HTTP Response: %.*s\n\n"`). The
log gate is `*level > 40 ⇒ suppress`; default 255 suppressed everything,
20 enables. Deployed; **awaiting iPhone-connect capture.**

LogCategory struct layout (from `LogCategory_Initialize @ 0xb57c0`):
`+0` current level (-1 until init, then copied from +4), `+4` default level
(what we patch), `+8…+24` name/parent/link ptrs, `+56` output sink.

---

## 6. Why the original Option-2 thunk was dropped

User picked "patch CopyProperty + SetProperty to log every key." Mid-build we
realized:
- `AirPlayReceiverSessionCopyProperty @ 0x58df0` special-cases only 4 keys
  (clientOSBuildVersion, ipAddress, TransportType, statistics); everything else
  forwards to dio_manager's platform plug, which ALREADY logs them
  (`[CDIOManager] sessionCopyProperty : Property: "…"`).
- So a CopyProperty hook adds visibility for just those 4 keys — low value.
- `LogHTTP` already exists and captures the ENTIRE RTSP exchange (methods,
  URIs, headers, bodies — not just property reads). Strictly more informative,
  and enabled by a 2-byte data patch vs a 70-byte code-cave trampoline.

---

## 7. The current failure (what we're diagnosing)

From `log22.log` (v2-patched device, wireless connect):
```
05:02:36.857471  [AirPlay] ### Alt Screen supported: Y      ← gate passed
05:02:36.895101  [AirPlay] AirPlay session started … Rec=3 ms: 0/0x0 noErr
05:02:36.895756  [CDIOManager] sessionCopyProperty Property:"modes"
                 [CDIOManager] sessionCopyProperty Property: initialPermanentEntity=[…]
05:02:36.901029  [AirPlay] Teardown                          ← 5.9 ms after start
                 [AirPlay] AirPlay session ended: Dur=0 s Reason=0/0x0 noErr
later:           [CDIOManager] onServiceStarted <iAP2> iAP2 migration required …
                 second iAP2 (over WiFi): authentication=0 identification=0 → CONNECTED → ERROR
                 [WorkerThread] terminate Error airplay code: -6740
```

Facts:
- iOS makes only **2** CopyProperty queries that reach dio_manager before teardown.
- `Reason=0/0x0 noErr` ⇒ **deliberate iOS-initiated** teardown, not an error.
- A second iAP2 session (BT→WiFi migration) fails auth/identification AFTER teardown
  (recovery attempt, not the cause).

### Hypotheses ruled out
1. ✗ altScreenURLs *handler* missing — libairplay forwards SetProperty to platform
   plug, so dio_manager would see/log it. (But: see open question below.)
2. ✗ viewAreas missing — added in v2, no change.
3. ✗ iAP2 themed-assets subparam 17 missing — verified emitted UNCONDITIONALLY
   in `libesoiap2.so` at 0x51BA0-0x51BB4 inside
   `CIAP2ControlSessionModuleIdentMsgComposer::identificationInformation @ 0x4F9A0`.

### Still open
- **What exactly does iOS do in the 5.3 ms between the last CopyProperty and
  TEARDOWN?** ← the HTTP-log patch is meant to answer this.
- Is the teardown driven by an RTSP `SET_PARAMETER`/`GET_PARAMETER` exchange we
  mishandle, or does iOS just send TEARDOWN unilaterally after an internal check?
- Is the BT→WiFi iAP2 migration auth failure causal or merely a downstream symptom?

---

## 8. Complete symbol / offset reference

### dio_manager
| Symbol | VA | Size | Use |
|---|---|---|---|
| `dio::createMainDisplaysDictionary` | 0x2B5610 | 0x330 | builds /info displays |
| `dio::CDIOManager::getDisplays` | 0x2B70F0 | | platform plug entry |
| patch call site (CFArrayAppendValue) | 0x2B57F4 | | we redirect this BL |
| `dio::display::addMainDisplay` | 0x2E81E0 | 0x9C | USED |
| `dio::display::addAltDisplay` | 0x2E8280 | 0xA4 | UNUSED — v1 cave |
| `dio::view::addMainViewAreas` | 0x2E8550 | 0x2A8 | USED |
| `dio::view::addAlternateViewAreas` | 0x2E8800 | 0x20C | UNUSED — we CALL it |
| `dio::view::addAdjacentAreas` | 0x2E8A10 | 0xEC | UNUSED — v2 cave (overwritten) |
| `infoResponse` | 0x2BDB20 | | logs some /info keys |
| `s_sessionCopyProperty` | 0x2BF200 | | platform plug copy-property |

### libairplay.so (srcvers 450.14.2)
| Symbol | VA | Size | Notes |
|---|---|---|---|
| `AirPlayCopyServerInfo` | 0x4F900 | | builds /info |
| `AirPlayGetFeatures` | 0x4EAA0 | | hardcodes `\| 0x6104040280` — bit 26 altScreen always set |
| `AirPlayReceiverServerPlatformCopyProperty` | 0x4D630 | 0x10c | statusFlags special-case, rest→plug |
| `AirPlayReceiverSessionPlatformSetProperty` | 0x4DD40 | 0x28 | thin forwarder |
| `AirPlayReceiverSessionHasFeatureAltScreen` | 0x58CE0 | 0x20 | reads +0x127; friend→return 1 |
| `AirPlayReceiverSessionCopyAltScreenURLs` | 0x58D00 | 0x48 | reads +0x128 (never written→NULL) |
| `AirPlayReceiverSessionCopyProperty` | 0x58DF0 | 0x358 | 4 special keys, rest→plug |
| `AirPlayReceiverSessionSetProperty` | 0x59150 | 0xB0 | 1 special key, rest→plug |
| `AirPlayReceiverSessionSetup` | 0x63390 | | friend B.EQ→B at 0x63DF0 |
| `AirPlayReceiverSessionTearDown` | 0x61C10 | 0x7cc | |
| `AirPlayReceiverSessionSendiAPMessage` | 0x5E800 | | sends iAP2 over BT ctrl chan |
| `AirPlayAltScreenDictCreate` | 0x5F860 | | dio_manager v2 calls this |
| `ScreenStreamProcessData` | 0x103120 | | H.264 NAL sink (downstream nullop) |
| `sub_50990` (RTSP request handler) | 0x50990 | 0x1ed4 | SET_PARAMETER dispatch @ 0x511F8 |
| `LogHTTP` | 0xB0DC0 | | the logger we enabled |
| `LogCategory_Initialize` | 0xB57C0 | | copies +4→+0 on first init |
| `gLogCategory_HTTPRequests` | 0x178CB0 | | patched +4 |
| `gLogCategory_HTTPResponses` | 0x178D10 | | patched +4 |
| `gLogCategory_AirPlayReceiverCore` | 0x178190 | | TransportType log gate |
| `j_LogPrintF` (PLT) | 0x41EC0 | | |
| `"altScreenURLs"` CFString const | 0x115338 | | **ZERO code xrefs** |
| `"uiContextURLs"` CFString const | 0x115350 | | **ZERO code xrefs** |

### libesoiap2.so
| Symbol | VA | Notes |
|---|---|---|
| `CIAP2ControlSessionModuleIdentMsgComposer::identificationInformation` | 0x4F9A0 | size 0x6090 |
| subparam 21 (WirelessCarplay) emission | 0x519C0 | `MOV W1,#0x15; BL sub_48370` |
| subparam 17 (themed-assets) emission | 0x51BA0-0x51BB4 | UNCONDITIONAL — confirmed |

### Session struct fields (libairplay, base = `*(session+0x18)` = platformData)
| Offset | Meaning |
|---|---|
| +0x127 | hasFeatureAltScreen byte (friend forces consumers to see 1) |
| +0x128 | altScreenURLs storage (getter reads it; NOTHING writes it) |
| +0x288 (a1+288) | transport type int (1=Enet,2=WiFi,8=USB,16=Direct,32=BTLE,64=WFD) |

---

## 9. The architectural question (MHI2 vs MHI3)

An earlier writeup (`findings/firmware_ceiling.md`) concluded the project was
infeasible because the HU lacks a cluster compositor. **That verdict was based
on MHI2-style limitations.** The user corrected: MHI3 is a different platform
and has more of the SDK present. Evidence for the optimistic read:
- `[AirPlay] ### Alt Screen supported: Y` appeared — never seen on MHI2.
- libairplay exports `AirPlayAltScreenDictCreate`, `…CopyAltScreenURLs`,
  `HasFeatureAltScreen`, view-area helpers — full SDK surface.
- dio_manager has `addAltDisplay` + `addAlternateViewAreas` (unused but present).

What's genuinely missing/unknown on MHI3:
- A *writer* for `altScreenURLs` (+0x128) — orphan getter, dead CFString consts.
- A confirmed H.264 sink for stream type 111 (ScreenStreamProcessData has no
  downstream we've traced).
- ESO Display Manager registration for the cluster surface
  (`Virtual_Display_Cluster_ETH` / `Layer_Map_Route_Guidance_Native`).
- HID input routing for cluster touches/knobs.

Whether these are *absent* or merely *not-wired* is the core open question.
The HTTP-log capture is the next data point.

---

## 10. What's left for a fully working patch (roadmap)

### Phase 1 — Diagnosis (current)
- [x] Deploy HTTP-log patch
- [ ] Capture eso log during one iPhone-connect failure cycle
- [ ] Identify the RTSP exchange immediately before `[AirPlay] Teardown`

### Phase 2 — Close the protocol gate (keep session alive)
Depending on capture, ONE of:
- **2A altScreenURLs handler** (most likely): patch `sub_50990` SET_PARAMETER
  branch (@0x511F8) to parse the URL array, store at `*(session+0x18)+0x128`,
  return 200 OK. The getter @0x58D00 then reads it back.
- **2B iAP2 fix**: RE the BT→WiFi iAP2 migration auth handshake in libesoiap2.so;
  understand why the second session's auth/identification = 0. Possibly patch
  libesoiap2 or enforcer JSON.

### Phase 3 — Real rendering (the hard part)
- **H.264 sink**: tap `ScreenStreamProcessData @ 0x103120` NALs (we already had
  the tap logic in `altscreen_hook.c`); decode via GStreamer/OpenMAX or splice
  into existing video pipeline.
- **ESO Display Manager**: register a Displayable / claim the cluster layer
  (probe with `IPL_CONFIG_DIR=/etc/eso /usr/bin/dmdt-ivi -i` / `-l`).
- **HID input**: route cluster touch/knob to iOS as input events over iAP2.

Effort estimate: Phase 1 ~hours; Phase 2A ~1 day; Phase 2B ~3 days; Phase 3 ~2-3 weeks.

---

## 11. Project file map

```
mhi3-altscreen/
├── README.md                              # project doc + session timeline + revert
├── src/
│   ├── patch_dio_manager.py               # v2 patcher (alt-dict + viewAreas) — ACTIVE
│   ├── patch_libairplay.py                # friend's HasFeatureAltScreen=1 patcher
│   ├── patch_libairplay_loghttp.py        # HTTP-log enable (this session) — ACTIVE
│   ├── altscreen_hook.c / _min.c          # LD_PRELOAD experiment — ABANDONED
│   ├── Makefile / build.sh                # zig cc cross-compile
│   └── lib*.so                            # built hook artifacts (unused)
├── work/
│   ├── libairplay.device.so               # patched copy deployed to device
│   └── libairplay.device.so.loghttp.stock # pre-patch backup
├── findings/
│   ├── MASTER_KNOWLEDGE.md                # THIS FILE
│   ├── session_2026-05-16_loghttp.md      # HTTP-log patch session
│   ├── firmware_ceiling.md                # early (MHI2-style) closing verdict — superseded
│   ├── ios_altscreen_protocol_decoded.md  # iOS-side activation flow
│   ├── dio_manager_patch_iAP2_teardown.md # v1 patch + teardown analysis
│   ├── next_steps_after_info_patch.md     # 3 paths forward
│   ├── altscreen_true_fails.md            # stock altScreen:true → -6740
│   ├── dio_crash_with_hook.md             # LD_PRELOAD crash proof
│   └── cores/                             # crash dumps
└── dumps/
    └── pid17861_altScreenFalse_carplayActive/  # live dio_manager memory dump
```

---

## 12. Lessons learned (gotchas, in order of pain)

1. **LD_PRELOAD is dead on dio_manager** — any `.so` crashes it. Binary-patch only.
2. **AppArmor double-reload** — reload both dio_manager + smartphone_integrator
   profiles after swapping the binary, or exec is denied.
3. **`MOV Xd, SP` ≠ `mov_reg(d,31)`** — use `ADD Xd, SP, #0`.
4. **Cave size matters** — v2 thunk (216 B) didn't fit the v1 cave (164 B);
   relocated to addAdjacentAreas (236 B).
5. **.data VA ≠ file offset** in libairplay (delta 0x10000); .text VA == file offset.
6. **`Reason=0/0x0 noErr` is deliberate**, not an error — iOS chose to close.
7. **The /info gate is genuinely moveable on MHI3** (the `Alt Screen supported: Y`
   line is real and reproducible) — don't repeat the MHI2 "impossible" verdict.
8. **Device has no `ps`/`pkill`** — enumerate `/proc/[0-9]*/comm`.
9. **dio_manager spawns lazily** — patches take effect on next iPhone connect, no
   need to keep it running; killing it just forces a clean respawn.
10. **`altScreenURLs`/`uiContextURLs` CFString consts have zero xrefs** in
    libairplay — the writers were stripped; this is the prime suspect for Phase 2A.

---

## 13. How to reproduce the current state from scratch

```bash
SSH='ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p 15230 root@0.tcp.ngrok.io'
SCP='scp -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -P 15230'

# 1. dio_manager v2 patch
$SCP root@…:/opt/smartphone_integration/bin/dio_manager ./dio_manager.dev
python3 src/patch_dio_manager.py ./dio_manager.dev
$SCP ./dio_manager.dev root@…:/opt/smartphone_integration/bin/dio_manager

# 2. libairplay HTTP-log patch (friend patches already present)
$SCP root@…:/opt/smartphone_integration/lib/libairplay.so ./work/libairplay.device.so
python3 src/patch_libairplay_loghttp.py ./work/libairplay.device.so
$SCP ./work/libairplay.device.so root@…:/opt/smartphone_integration/lib/libairplay.so

# 3. AppArmor reload (mandatory)
$SSH '/sbin/apparmor_parser -B -r /etc/enforcer/apparmor/dio_manager /etc/enforcer/apparmor/smartphone_integrator'

# 4. Connect iPhone (wireless CarPlay) → dio_manager spawns → capture log
```

---

*End of master knowledge base. Keep this updated as the single source of truth;
append new session writeups to `findings/` and cross-link them here + in the
README session timeline.*
