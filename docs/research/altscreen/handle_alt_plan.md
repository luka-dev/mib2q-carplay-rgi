# AltScreen via HOOK-on-stock — handle_alt plan + verified RE

**Approach (LOCKED):** everything via the LD_PRELOAD hook `c_hook/altscreen/` on top of
the **STOCK MHI2Q libairplay 210.81** (NOT the from-source 280 rebuild — that stays a
separate experiment). Rendering is a **separate process** `altscreen_render/` (own folder).

Target unit: MU1316 / MHI2Q / APQ8064 / QNX 6.5. Build: docker `qnx65-armv7-toolchain`
(VM offline) → `build/libcarplay_hook.dockerbuild.so`.

---

## iOS 26.1 activation gate — VERIFIED (CarKit decompile, iPhone18-3_26.1_23B85)

`Refferences/iPhone18-3_26.1_23B85_Restore/…/CarKit.framework/CarKit/` (decompiled `.mm`).
The phone offers the cluster/altScreen stream (type **111**) based on the car's `/info`:

1. **`extendedFeatures` ∋ `"altScreen"`** — `CARSessionConfiguration.mm:624` `setExtendedFeatures`
   → `updateCarCapabilities@626`; `"altScreen"` is a real AirPlay feature string
   (`CarKit.mm:3698 CRCarPlayFeaturesAsAirPlayFeatures`). Drives `CRFetchInstrumentClusterURLs`
   (`CARSession.mm:2015`).
2. **`displays` ∋ a display with `properties:["Instrument Cluster"]`** →
   `_hasGaugeClusterScreen = [_displays bs_containsObjectPassingTest:…]`
   (`CARSessionConfiguration.mm:956`); the test = `properties` contains `"Instrument Cluster"`
   → `showsInstruments` (`CARDisplayInfo.mm:409`). Independent of extendedFeatures; identifies
   WHICH display the 111 stream targets.

**Both are required.** Stock 210 emits NEITHER (0 occurrences of `extendedFeatures`/`displays`/
`altScreen` in the stock binary) → the hook must synthesize both in `/info`.

---

## Status

| # | Piece | Status |
|---|---|---|
| 1a | advertise `extendedFeatures=["altScreen"]` in `/info` | ✅ done, builds |
| 1b | `displays=[main,cluster]` in `/info` (gauge cluster) | ✅ done, builds |
| 2 | `handle_alt` — accept 111 + bring up alt stream + teardown | ✅ coded, builds (gated) |
| 3 | tap `ScreenStreamProcessData` → forward NALs | ✅ coded, builds |
| 4a | `altscreen_render/` receiver + framing + file-capture sink | ✅ builds Mac+QNX, wire proven |
| 4b | `altscreen_render/` OMX AVC decode + cluster present | ✅ ported+host-checked; on-unit build/validate pending |

**UNTESTED on-unit** (offline). handle_alt gated by env `ALTSCREEN_HANDLE_ALT` (dark by
default); fail-safe (any missing stock symbol → skip handle_alt, advertise still works).

## altscreen_render (external process) — increment 1 DONE ✅
`altscreen_render/` = TCP SERVER on :19830 (hook connects per session), reads `[u32 be len][annexb]`
framing → video sink. Files: `main.c` (accept/serve loop), `frame_io.c/.h` (framed reader, unit-tested),
`sink.h` + `sink_file.c` (Mac/debug: append Annex-B → `.h264`, ffplay-inspectable) + `sink_omx_qnx.c`
(QNX stub), `test_feed.c` (socketpair self-test), `Makefile`.
- Mac: builds clean, `make test` PASS, **live TCP e2e verified** (framed SPS+IDR → capture byte-identical).
- QNX: cross-compiles via docker toolchain (9.8K ARM EABI5), receive-only stub.
## altscreen_render 4b — OMX decode + cluster present PORTED ✅ (host-checked)
`sink_omx_qnx.c` = faithful port of the proven from-source `screenstream.c` OMX+QNX-Screen
zero-copy pipeline, libairplay wrapper (CFRuntime/ulog/AVCC/H264 helpers) stripped:
- `OMX.qcom.video.decoder.avc`: EnableDecoderOrderMode → ConfigurePorts (in=AVC, out=qcom 32m
  `0x7F000004`) → Idle → AllocateBuffer(in) + **OMX_UseBuffer(out = Screen window vaddrs)** →
  Executing → FillThisBuffer.  `_OnFillDone` → `screen_post_window` the buffer directly (no de-tile).
- **Input already Annex-B** (hook converted) → fed straight to `OMX_EmptyThisBuffer`; the FIRST
  message (hook's SPS/PPS header) flagged `OMX_BUFFERFLAG_CODECCONFIG`. No avcC/H264 code here.
- **Geometry known up front** = advertised cluster size (env `ALTSCREEN_W/H`, default 1440×542):
  CarPlay encodes the 111 stream at the display size we advertised, so zero-copy window can be built
  before Idle.  If the decoded geometry ever differs (SPS ≠ advertised), `OMX_EventPortSettingsChanged`
  now triggers a full **output-port reconfig** (`_ReconfigOutputPort`, run on the feed thread):
  PortDisable → FreeBuffer → rebuild Screen window at the new size → PortEnable → re-`OMX_UseBuffer`
  → re-prime; `winReady` dropped during rebuild so present skips the half-built window.
- Window: QNX Screen, FORMAT `0x1000C` / USAGE `0x484` (stock literals), bound to the cluster display
  (`SCREEN_PROPERTY_DISPLAY`, last enumerated; env `SS_CLUSTER_DISPLAY_INDEX`). group
  `dio_manager.airplay.qnx.screen_render.alt`, ID "111".  Env `SS_SCREEN_FORMAT` fallback.
- **Verification:** `make hostcheck` (host C/type-check vs stub `screen.h` + OMX shims) = clean; Mac
  file-sink build + framing self-test = PASS; QNX receiver syntax = clean.  **Cannot link in docker**
  (no BSP screen/OMX); real build on VM/unit via `compile_render_qnx.sh` + `-lscreen -lOmxCore`
  (**confirm OMX-core lib name on-unit**).  **ON-UNIT UNVALIDATED.**

## altscreen_render files
`main.c` (accept/serve), `frame_io.c/.h` (framing, unit-tested), `sink.h`, `sink_file.c` (Mac capture),
`sink_omx_qnx.c` (QNX OMX+Screen), `test_feed.c` (self-test), `hostcheck/screen/screen.h` (stub),
`Makefile` (Mac / QNX / hostcheck).

## #3 NAL tap — IMPLEMENTED ✅ (RE-verified, builds 277K)
Global interpose of exported `ScreenStreamProcessData(self, data, len, ts, dict, done, ctx)`.
- **Discriminator main(110) vs alt(111):** thread identity. RE (`ProcessFrames` sub_2b154) confirms
  ProcessData is called INLINE on the same thread → `alt_slot_for_thread()` (pthread_equal vs each
  active `slot->thread`) is NULL for the stock main stream, non-NULL for ours. Main stream → pass to
  real; alt stream → tap + swallow (return 0 + fire `done(ctx)`; do NOT let stock decode — its OMX
  targets the main display).
- **Data is already AES-decrypted** — `ProcessFrames` runs `AES_CTR_Update` before calling ProcessData.
- **Forward:** per-frame `data/len` = AVCC → `altav_forward_avcc_frame` converts to Annex-B (lengthSize
  from `self+52`) → TCP `127.0.0.1:19830`. SPS/PPS = `self+40`/`+44` (Annex-B header, built by stock
  `_ScreenStreamSetProperty` via `H264ConvertAVCCtoAnnexBHeader` from opcode-1 avcC) → forwarded raw
  once (deduped by ptr in `slot->cfgLast`). Wire = Annex-B (render feeds OMX raw, no conversion).
- **ScreenStreamPrivate offsets (MU1316, verified in 2 funcs):** +40 cfg ptr, +44 cfg len, +52 lengthSize.
- **On-unit checks:** (a) interposition of the internal libairplay→ProcessData call actually fires
  (if the stock MAIN screen still works after deploy, it fires for both — self-consistent; MHI3
  precedent); (b) `self+40/44/52` offsets hold on the live build.

---

## #1a/#1b — advertise (DONE)

Interpose the stock `/info` builder **`AirPlayCopyServerInfo`** (NOT `SetProperty` — stock
210 never reads `config[extendedFeatures]`, so the old SetProperty inject was a no-op). Add
to the returned CFMutableDictionary:
- `extendedFeatures = ["altScreen"]`.
- `displays = [main, cluster]`: main from stock `AirPlayReceiverSessionScreen_CopyDisplaysInfo`
  (ignores its arg → pass NULL → real `ScreenCopyMain` uuid/size); cluster synthesized:
  `{name, type:111, maxFPS:60, widthPixels:1440, heightPixels:542, widthPhysical:180,
  heightPhysical:68, uuid:<distinct>, properties:["Instrument Cluster"]}`.

CF objects built with libairplay's OWN CFLite via dlsym (hand-built CFStrings vs interned
constants = the old "displays-era" crash). Callbacks: `kCFLArrayCallBacksCFLTypes`,
`kCFL Dictionary{Key,Value}CallBacksCFLTypes`.

---

## #2 — handle_alt (CODED)

**Key finding:** stock 210 EXPORTS the entire screen lifecycle → handle_alt is pure
**orchestration of exports**, not reimplementation. Cloned from stock `_ScreenSetup`
(`sub_26F9C`) + `_ScreenThread` (`sub_24220`).

### Entry point
Interpose **`AirPlayReceiverSessionSetup@0x272b8`**: its stream loop handles 100/101/110 and
**skips** unknown types (111) WITHOUT aborting → real returns 0 + `*respOut` = response dict.
Then we: find the type==111 desc in `request["streams"]`, run `alt_setup`, append the 111
response desc to `*respOut`. Interpose **`AirPlayReceiverSessionTearDown`** to quit+join our
receiver thread before the session is freed.

### Stock session/server struct offsets (bytes, MU1316 210.81 — RE-verified)
| field | offset |
|---|---|
| server* | session **+12** (→ qosDisabled +452, timeoutDataSecs +456, screenStreamOptions **+464**) |
| transportType (int) | session +80 |
| peerAddr sa_family (u8) | session +85 |
| sessionUUID[16] | session +112 |
| clientDeviceID (u64) | session +136 |
| sessionID (u32 low) | session +152 |
| **aesSessionKey** | session **+456** |

Screen-session stays **opaque** (from `Screen_Create`, only handed to exports) — no
screen-session internal offsets needed. Alt state (altSess/altSock/thread) lives in a
hook-side table keyed by session ptr (stock has no altScreen slots).

### Stock exports used (all confirmed present, `nm -D`)
`AirPlayReceiverSessionScreen_{Create,Delete,Setup,SetSecurityInfo,StartSession,ProcessFrames,`
`UpdateState,SetTimeSynchronizer,SetSessionUUID,SetClientDeviceID,SetReceiveEndTime,`
`SetAuthEndTime,SetNTPEndTime,SendCommand}`, `AirPlay_DeriveAESKeySHA512ForScreen`,
`ServerSocketOpen`, `SocketAccept`, `SocketSetQoS`, `NetSocket_{CreateWithNative,Delete,GetNative}`,
`CFAbsoluteTimeGetCurrent`, `UpTicks`, `CFDictionary{GetInt64,GetValue,CreateMutable,SetInt64,SetValue}`,
`CFArray{GetCount,GetValueAtIndex,CreateMutable,AppendValue}`.

### Recipe (alt_setup — clone of sub_26F9C, type 111)
```
Screen_Create(&altSess); UpdateState(altSess,1)
Screen_Setup(altSess, streamDesc, *(u32*)(session+152))
connID = streamDesc["streamConnectionID"]      // require != 0
DeriveAESKeySHA512ForScreen(session+456, 16, connID, key[16], iv[16])
SetSecurityInfo(altSess, key, iv); memzero key/iv
ServerSocketOpen(family=*(u8*)(session+85), SOCK_STREAM=1, TCP=6, -6030, &port, -180224, &altSock)
if !server->qosDisabled: SocketSetQoS(altSock, 12)
resp = {type:111, dataPort:port}; _AddResponseStream(responseParams, resp)
pthread_create(alt_thread_main)
```
`_AddResponseStream` (stock sub_26EFC replica): append `resp` to `responseParams["streams"]`
(create the array if absent).

### Receiver thread (alt_thread_main — clone of sub_24220)
```
SocketAccept(altSock, 10, &fd); close(altSock)
SetSessionUUID(altSess, session+112); SetClientDeviceID(altSess, *(u64*)(session+136))
NetSocket_CreateWithNative(&netSock, fd)
SetTimeSynchronizer(altSess, {ctx=session, OUR alt_get_ntp, OUR alt_get_upticks_near})
Set{Receive,Auth,NTP}EndTime(altSess, CFAbsoluteTimeGetCurrent())
StartSession(altSess, *(void**)(server+464) /*screenStreamOptions*/)
ProcessFrames(altSess, netSock, *(int*)(server+456) /*timeoutDataSecs*/)
```

### Time synchronizer — NO fragile addresses
`ProcessFrames` only calls the sync callback if a per-stream flag (screen-session +44, set from
stream cfg +78 bit1) is set; else it uses `UpTicks()`. So we supply our OWN trivial callbacks
returning `UpTicks()` instead of the lib's internal `_ScreenGetSynchronizedNTPTime`
(`sub_24568`/`sub_2455C`). Value only affects presentation late-frame detection → moot for the
tapped cluster stream. **Zero internal-address dependency.**

---

## #3 — tap ScreenStreamProcessData (NEXT)

Interpose **`ScreenStreamProcessData`**: this is where `ProcessFrames` hands the decrypted
frame to the ScreenStream (verified in `ProcessFrames`, opcode 0:
`ScreenStreamProcessData(a1[123], data, len, …)` after `AES_CTR_Update`). For the **alt**
stream: grab the decrypted NAL(s), forward them, and **skip** the real decode (no 2nd in-dio
HW decoder). For the **main** stream: pass through to real.

**Distinguish alt vs main:** the ScreenStream object `a1[123]` differs per screen session. Need
to record our alt session's ScreenStream handle (or tag it) so the interposer can tell them
apart. TBD: capture the alt ScreenStream pointer (e.g. right after StartSession creates it) into
the side table, and compare in the interposer.

**Transport = TCP (decided, not SHM):**
- Data crossing hook→render is **compressed** H.264 (~4–8 Mbit/s ≈ 1 MB/s, ~60–240 NAL/s), not
  raw frames → SHM's zero-copy win doesn't apply.
- TCP overhead on QNX (loopback via io-pkt) ≈ **~0.2–1% of one core** + µs latency
  (`TCP_NODELAY` set → no Nagle). Reducible to ~0.2% by **batching one write per frame/access-unit**.
- SHM cons that kill it HERE: SHM segment + process-shared lock **leak/wedge** when dio
  (per-session) dies; re-attach on peer restart; a ring overrun corrupts the other process's
  memory — all bad on a per-session-restarting CarPlay-critical process, for a sub-1% saving.
- **Hard requirement (either transport):** the forward must be **non-blocking + lossy**
  (bounded queue, drop-on-overflow) so it never stalls the `ProcessFrames` receiver thread
  (a stall → iOS backpressure → stream dropped). Same pattern as the coverart async worker.

Wire format: reuse **`altscreen_av.{c,h}`** (AVCC → Annex-B NAL framing, already done) over TCP
(SS_FWD_PORT_ALT default **19831**).

---

## altscreen_render/ (separate folder, external process)

New folder (sibling of `maneuver_render/`). Reuse `maneuver_render/platform_qnx.c` (Screen/EGL
window + cluster displayable). Pipeline: recv NAL (TCP :19831) → OMX H.264 decode → cluster
displayable (20 / ctx 74) → MOST → VC. Separate process = fault isolation (GL/OMX crash must not
kill CarPlay) + it can be a DM-registered graphics client (dio isn't).

---

## On-unit test (when unit is online)

1. Deploy hook; `ALTSCREEN_HANDLE_ALT=1`; plug iPhone; open **Apple Maps** (has a cluster scene;
   avoids app-side `-16721` empty-URLs).
2. Expect logs: `injected extendedFeatures=[altScreen]` + `injected displays=[main,cluster]`
   (advertise) → `111 stream offered — bringing up altScreen` → `alt-screen set up on port N` →
   `alt-screen receiver started` (= the HU received the stream).
3. If `NmeTransport::Recv`-style seam validation needed, the first-bytes logs report it.
4. Then #3 tap → frames reach altscreen_render → picture on cluster.

## Open items
- #3: tag/identify the alt ScreenStream to distinguish it in the `ScreenStreamProcessData` tap.
- Confirm response key strings (`"streams"`, `"type"`, `"dataPort"`, `"streamConnectionID"`) match
  stock interned constants by value (CFLite compares by value — should be fine).
- Verify `AirPlayReceiverSessionScreen_UpdateState` is exported (else handle_alt fail-safe skips).
- On-unit: does iOS actually open 111 with our 2-display /info; stability of the alt session.

---

# NEXT capabilities — knob input (HID) + working zone (insets)

Both are **Java-driven mode/focus decisions** → both need a **Java→hook command channel**
(the bus is currently one-way hook→Java). Good news: `c_hook/framework/bus.c` already has the
inbound-dispatch infra (`bus_on(type, handler, ctx)` + `slot->handler` + `g_htable_rw`) — it is
just **not wired to a reader**. Making the bus bidirectional = add a reader on the connector
socket that parses inbound frames and dispatches to `slot->handler`. The altscreen module then
registers handlers for `CMD_KNOB` / `CMD_ALT_ZONE`.

## Capability A — knob (крутилка) input → altScreen

**Why from Java:** on this HU the steering-wheel knob is a vehicle-bus input **owned by the
Java HMI** (it drives the native cluster/settings UI). Java is the focus ARBITER: at some focus
the knob changes car settings in the cluster; at altScreen focus it should drive CarPlay. The
hook never sees the raw knob and shouldn't — Java sends only what it routes to altScreen.

**Flow:**
```
Java (knob events + focus arbitration)
  ──[altScreen focus]──▶ Java→hook CMD_KNOB {delta, select}
  ──▶ hook: HIDDevicePostReport on a cluster virtual-HID device (bound to altScreen displayUUID)
  ──▶ iOS routes to the cluster scene
```
Java sends **semantics** (rotate ±1, select press/release); the hook builds the HID descriptor +
report (near libairplay HID API).

**Mechanism — fully exported in stock 210 libairplay (`nm -D`):**
- `HIDDeviceCreateVirtual` — create the virtual knob device.
- `HIDDPadCreateDescriptor` (rotary/dpad) + `HIDButtonsCreateDescriptor` (select) — knob descriptor.
- `HIDDevicePostReport` — post the report; `HIDDeregisterDevice`, `HIDDeviceCopyProperty`,
  `HIDBrowser*`, `HIDCopyDevices`.
- Per-display binding: HID device carries a **`displayUUID`** (SDK `HIDUtilsDarwin.c:154`
  "Display this HID device is associated with"); get the cluster UUID via
  `AirPlayReceiverSessionScreen_CopyDisplayUUID(altSess)`.
- Advertise on the cluster display dict: `primaryInputDevice = 3` (Knobs) +
  `displayFeatures = 0x02` (kAirPlayDisplayFeatures_Knobs).
- Wire command: `kAirPlayCommand_HIDSendReport = "hidSendReport"`, key `hidReport`.

**Open digs:** exact knob HID descriptor (rotary + select) format iOS expects; where in Java the
wheel-knob events are captured + the focus arbitration point.

## Capability B — working zone (fullscreen ↔ splitscreen) = insets (+ optional offset)

**Mechanism — iOS 26.1 safe-area insets (per screen).** CarKit `CARSessionConfiguration
viewAreaInsets` (NSEdgeInsets top/left/bottom/right) + `CARScreenInfo … additionalInsets`
(source = the display dictionary). iOS **re-lays-out** its content within `(display − insets)`.
Stock 210 emits no insets (iOS-26.1 addition) → the hook **synthesizes** the insets key in the
cluster display dict (like extendedFeatures/displays).

**Two distinct geometric levers:**
1. **insets** (iOS-side, /info cluster dict) — pin iOS's SAFE CONTENT to a sub-region. To pin the
   safe area to the LEFT: `{left:0, top:0, right:<reserved right>, bottom:0}`. iOS still renders
   the FULL frame; insets only move the content layout, not the physical video.
2. **stream offset / dest-rect** (render-side, `altscreen_render`) — where the decoded frame
   physically lands on the cluster displayable (x,y,w,h).

**Can we do split with insets ONLY (no stream shift)?**  YES, mechanism-wise — a right inset
makes the safe area the left part of the still-fullscreen stream. The catch is what covers the
frame's RIGHT half physically:
- If the **VC (cluster compositor) crops the MOST video to the visible zone / draws native gauges
  over the right** (notes: "VC draws gauges/tabs itself; HU only feeds video" → VC is a
  compositor) → **inset-only is enough, no shift.**
- If the VC shows the video fullscreen with no native overlay → the right half shows CarPlay
  background → then a dest-rect crop/shift is needed.
- Tradeoff: inset-only is simpler but iOS encodes the unused right half (wasted bitrate).
- **TODO: confirm VC split-mode behaviour (does the cluster crop the video to the zone?).**

**DECISION (chosen):** working zone = **dynamic insets ONLY**. No render-side stream shift /
dest-rect, no hook→altscreen_render control channel. Splitscreen = redefine the cluster safe
zone via insets; rely on the VC to compose the visible split (accepted). The stream stays
fullscreen; altscreen_render always blits full → simpler render + no control plumbing.

**Runtime fullscreen↔split** is Java-driven: `CMD_ALT_ZONE {mode, insets}` on the bus → the hook
dynamically redefines the cluster insets. → **LOAD-BEARING open question: the live-update
mechanism** — how iOS re-reads/applies changed insets MID-session (it reads /info at setup).
Candidates to RE:
  - a live screen-property push (`AirPlayReceiverSessionScreen`/`ScreenStreamSetProperty` — the
    session already pushes screen props live in StartSession) — IDEAL if insets can ride it;
  - a "server info changed" / re-GET-/info trigger the HU can raise;
  - a CarKit session-config-update path (CARSessionConfiguration is re-fetchable).
  If no live path exists, fall back to static per-mode insets set at setup (less flexible).

**Open digs:** exact insets key name iOS 26.1 reads from the display dict; **the runtime
insets-update mechanism (load-bearing).**

## Shared plumbing summary
| node | work |
|---|---|
| **bidirectional bus** (shared) | add reader on connector socket → dispatch inbound frames to existing `bus_on` handler table |
| A knob | hook creates cluster virtual-HID (`HIDDeviceCreateVirtual`, bound `displayUUID`); Java→`CMD_KNOB`; hook `HIDDevicePostReport` |
| B zone | Java→`CMD_ALT_ZONE`; hook dynamically redefines cluster **insets** (inset-only, no stream shift). Load-bearing dig = live insets-update mechanism |

## PROGRESS — coded + builds (hook side)

- **Bidirectional bus (DONE).** `bus.c`: replaced the one-way `socket_peer_closed` poll with a
  blocking `read_frame` in the connector thread → reads inbound frames (16B header + payload) and
  dispatches to the existing `bus_on` handler table (lock held across callback). `MAX_TYPES` grown
  to `0x120` to cover the `0x01xx` command block. Disconnect detect is now immediate (recv==0).
- **Command types (DONE).** `bus_protocol.h`: `CMD_KNOB 0x0110` (binary `[int8 wheel][u8 buttons]`,
  buttons bit0=press), `CMD_ALT_ZONE 0x0111` (binary, zone/insets — format TBD).
- **Knob HID (DONE, hook side).** `altscreen_hook.c`: knob HID report descriptor verbatim from SDK
  `Examples/HIDKnob.c` (4-byte report `[buttons][X][Y][Wheel]`); lazy `HIDDeviceCreateVirtual`
  (props: `hidDescriptor`=CFData, `displayUUID`=`CLUSTER_DISPLAY_UUID` == the /info cluster uuid,
  `name`); `bus_on(CMD_KNOB)` → `HIDDevicePostReport(dev, [buttons,0,0,wheel], 4)`. All stock
  exports (`HIDDeviceCreateVirtual`/`HIDDevicePostReport`/`CFDataCreate` confirmed `nm -D`).
- **CONFIRMED main-input path:** Java `DSICarplay.postRotaryEvent(int)`/`postTouchEvent`/
  `postButtonEvent` → native dio → `HIDDevicePostReport` (displayUUID-bound) → iOS. dio already
  uses the exact libairplay HID subsystem we call → integration path proven; but `postRotaryEvent`
  has NO display arg (main only) → altScreen needs the separate `CMD_KNOB` path (above).

**Remaining (input):** (c) Java sender — on altScreen focus send `CMD_KNOB` on the bus (Java bus
client already reads; add a writer). On-unit: does dio's AirPlay HID browser forward our virtual
device to iOS (very likely — it does for main).

---

# WORKING ZONE (fullscreen ↔ split) — RESOLVED via MHI3 (450) RE

**Reference:** MHI3 / MPR3 stock `libairplay.so` (SDK **450**, aarch64) has the FULL accessory-side
viewArea implementation. NEITHER stock 210 NOR our from-source 280 has it (both = 0 occurrences of
`viewArea`) — 450 is the only reference. Binary:
`Firmwares/HU/MHI3_ER_AU_P4364_8W0906961DR/_opt_ivi_C3_full_extract/.../lib/libairplay.so`.

## /info viewAreas schema — CONFIRMED (MHI3 `AirPlayScreenDictSetViewAreas`)
Cluster display dict carries:
- `"viewAreas"` = array of `{ originXPixels, originYPixels, widthPixels, heightPixels,
  "safeArea": { originXPixels, originYPixels, widthPixels, heightPixels } }`  (+ type-110 extras:
  `viewAreaSupportsFocusTransfer`, `viewAreaStatusBarEdge`).
- an active-index int + `"adjacentViewAreas"` array of indices.
Our `build_cluster_dict` already emits `viewAreas`[full, split] with these exact keys.
DECISION: predeclare the modes (fullscreen index 0 = default, split index 1) — enough.

## LIVE switch — RESOLVED, stock-210-achievable ✅
It is **NOT** a CoreMedia/FigEndpoint feature — it is a plain COMMAND. From MHI3
`AirPlayReceiverSessionViewAreaUpdate(session, uuid, animationDurationMillis, viewAreaIndex,
adjCount, adjArray, completion, ctx)` (decompiled, key strings confirmed):
```
command = { "type": "updateViewArea",
            "params": { "uuid":                   <cluster display uuid>,
                        "viewAreaIndex":          <target index>,   // 0=full, 1=split
                        "animationDurationMillis": <ms>,            // animated transition
                        "adjacentViewAreas":      [<indices>] } }
AirPlayReceiverSessionSendCommand(session, command, completion, ctx)
```
`AirPlayReceiverSessionSendCommand` is EXPORTED IN STOCK 210 → the hook replicates this 1:1.
The phone receives it → CarKit `CARSession._sessionUpdatesQueue_handleViewAreaChangeWithPayload:`
reads `{uuid, viewAreaIndex, animationDurationMillis}` → `_updateScreenInfo:currentViewAreaToViewArea:
duration:` → animated switch to `viewAreas[index]`.

## Feature gate
`AirPlayReceiverSessionHasFeatureViewAreas` reads `*(u8*)(*(session+24) + 293)` — a per-session
capability byte set during negotiation, driven by `ENABLEDFEATURES_VIEWAREAS`. → iOS must
negotiate viewAreas support; likely add `"viewAreas"` to `extendedFeatures` (like `"altScreen"`).
CONFIRM: does advertising the feature (extendedFeatures / enabledFeatures) flip that byte.

## LIVE switch — IMPLEMENTED ✅ (builds, docker 274K, TLS-clean)
`altscreen_hook.c` `zone_on_cmd` (bus `CMD_ALT_ZONE`, payload `[u8 viewAreaIndex][u16 LE
durationMillis?]`, dur default 250):
- builds `{type:"updateViewArea", params:{uuid:CLUSTER_DISPLAY_UUID, viewAreaIndex, animationDurationMillis}}`
- ships via stock `AirPlayReceiverSessionSendCommand(session, cmd, NULL, NULL)` (resolved lazily,
  non-critical — missing ⇒ zone off, advertise/handle_alt still work).
- session ptr = `g_alt_session` (main AirPlay session), set on `alt_setup` success, cleared on teardown.
- **VERIFIED phone-side (CarKit `_sessionUpdatesQueue_handleViewAreaChangeWithPayload`, CARSession.mm:1250):**
  reads ONLY `uuid` + `viewAreaIndex` (fallback `viewArea`) + `animationDurationMillis`; bounds-checks
  index vs declared viewAreas count (`>=count → reset 0`); `adjacentViewAreas` NOT read on switch.
- **VERIFIED shipper (stock 210 `AirPlayReceiverSessionSendCommand`@0x28460):** generic —
  serializes any dict to binary-plist, `POST /command`, NO command-type whitelist. Ctrl chan = `session+704`.

### CONFIRMATION (implemented)
We pass a completion callback (not NULL) so the switch is confirmed, not fire-and-forget:
- **RE (stock `sub_28CF0`, the `/command` response handler, disasm @0x28d68):** parses the phone's
  reply plist, reads status Int64, then calls `completion(int status, CFDictionaryRef resp|NULL, void *ctx)`
  — `status==0` ⇒ accepted (resp passed); else resp=NULL. `ctx` = the 4th SendCommand arg.
- `zone_cmd_done` relays it to Java as **`CMD_ALT_ZONE_ACK 0x0112`** `[u8 index][i32 LE status]` (requested
  index carried in `ctx`). Runs on libairplay's HTTP-client thread — `bus_send` is mutex-backed/safe.
- SendCommand sync failure (completion won't fire) ⇒ acked inline with the rc.

## Remaining (zone)
1. Advertise the `viewAreas` feature so iOS sets `HasFeatureViewAreas` (confirm the flag name/place) —
   only if on-unit shows the phone ignores `updateViewArea` without it (the accessory sends regardless;
   we bypass the MHI3 `HasFeatureViewAreas` pre-check by calling SendCommand directly).
2. Fill the exact SPLIT_SAFE_* geometry (user provides) — `build_cluster_dict` viewAreas[1].safeArea.
3. Java-side `CMD_ALT_ZONE` sender (bus writer) + on-unit smoke test.

Session ptr capture + SendCommand signature: `int AirPlayReceiverSessionSendCommand(session,
CFDictionaryRef command, completionFunc, ctx)` — command = {type, params}. completion/ctx may be NULL.

---

# FocusTransfer — SKIPPED (decision) — RE kept for reference

**DECISION: skip FocusTransfer.** Knob routing (displayUUID binding) works without it; revisit
only if on-unit proves the cluster ignores input without focus. RE below kept for that day.



**Two INDEPENDENT axes — do not conflate them:**

1. **Input routing = HID device's displayUUID binding, NOT focus.**
   `AirPlayReceiverSessionSendHIDReport(session, deviceID, report, len)` (MHI3, decompiled) →
   ```
   { type: "hidSendReport", params: { "uuid": "<%X deviceID>", "hidReport": <bytes> } }
   ```
   The report is addressed by the **HID device uuid**; which display it targets is fixed at
   `HIDDeviceCreateVirtual` via the device's displayUUID. Our cluster-bound knob therefore
   delivers into the cluster scene **regardless of focus**. Routing is already correct.

2. **Focus = which scene is active/foreground (accessory acquire/give).**
   `accessoryAcquireFocus` / `accessoryGiveFocus` = **no params**, session-level SendCommand
   (`{type:"accessoryAcquireFocus"}`). Per-viewArea permission = `viewAreaSupportsFocusTransfer`
   + `AirPlayScreenDictSetAccessoryGiveFocus`. This is NOT "where input goes" — it's "whose
   scene is active."

**Not in our references:** the `accessoryAcquireFocus` handler lives in the phone's CarPlay-UI
daemon, NOT in CarKit (CARSession.mm has only `takeScreenForClient`). So whether a NON-focused
cluster scene *acts* on delivered input is an iOS-UI behavior we cannot confirm statically —
**on-unit question.** Typical iOS: input events go to the focused/foreground scene, so the knob
likely "arrives but does nothing" until the cluster has focus.

**Design conclusion:** FocusTransfer is the **activation half**, paired with the knob (routing
half) — not an alternative. Model for "Java routes knob → cluster":
1. Java decides knob→cluster → hook sends `accessoryAcquireFocus` (cluster becomes active)
2. knob HID reports (already cluster-bound) → land in the active scene
3. Java routes away → hook sends `accessoryGiveFocus`

**Remaining (focus):** keep it a ready add-on — two param-less `AirPlayReceiverSessionSendCommand`
calls (`accessoryAcquireFocus` / `accessoryGiveFocus`), driven by the same Java bus arbiter that
sends `CMD_KNOB` (Java already owns the focus decision). Wire only if on-unit shows the knob
reaches the cluster but doesn't act without focus. Gate: `ENABLEDFEATURES_FOCUSTRANSFER` — advertise
alongside `altScreen`/`viewAreas` in extendedFeatures if needed.

## Other MHI3 (450) portable session commands — IMPLEMENTED ✅ (builds 276K)
All via `AirPlayReceiverSessionSendCommand` + logging completion (`alt_ship` helper); target the
cluster uuid (except changeUIContext = session-global). Keys VERIFIED by `get_string` on MHI3 450.
| Command | type | params | bus cmd | payload |
|---|---|---|---|---|
| Appearance | `uiAppearanceUpdate`/`mapAppearanceUpdate` | `{uuid, appearanceMode:int, appearanceSetting:int}` | `CMD_ALT_APPEARANCE 0x0114` | `[u8 target(0=ui,1=map)][u8 mode][u8 setting]` |
| UI context | `changeUIContext` | `{url:string}` (no uuid) | `CMD_ALT_UICTX 0x0115` | UTF-8 url |
| Focus | `accessoryAcquireFocus`/`accessoryGiveFocus` | *(none)* | — | SKIPPED (see above) |
| ~~Zoom~~ | ~~`changeMapZoomLevel`~~ | — | ~~`CMD_ALT_ZOOM 0x0113`~~ | **DROPPED** (see note) |

**DECISION (2026-07-26): CMD_ALT_ZOOM DROPPED, all altScreen input is HID.** `changeMapZoomLevel`
was a SEMANTIC command (focus-independent) — kept RE'd for reference, but the map zoom is handled by
the HID knob in-context (`CMD_ALT_HID`, ex-`CMD_KNOB`). 0x0113 retired/reserved. Code + zoom_on_cmd
removed. Rationale + the semantic-vs-HID distinction: memory `altscreen_zoom_vs_hid.md`.

**Enum values (Appearance, Java-supplied, confirm on-unit):** `AirPlayAppearanceMode`/`Setting` are
SDK enums (not in the phone/CarKit decompile — CarPlay-UI daemon). Hook passes the int verbatim; Java
owns the values (likely day/night).

CornerMasks: **SKIPPED** (deprioritized) — accessory-side `ScreenStreamSetCornerMask` is a STUB
even in 450; iOS side is `/info cornerMasks` + `setCornerMaskImageData:` (image data), not worth it.

---

# Java side — cluster-tab integration (design, RE from MU1316 full decompile)

Decompile: `Tools/jxe2jar/out/MU1316-combined-vf/` (30k files — the complete stock HMI).

## Cluster tab state (the gate for everything)
`de/audi/tghu/navi/app/cluster/ClusterViewMode.java`:
- **`ChoiceModel(67)` = ACTIVE cluster tab** — `viewMode.getValue()` ∈ {COMPASS=0, RGI=1, KDK=2, MAP=3}.
  This is the authoritative "which tab is showing" signal.
- **`ChoiceModel(400140)` = user's menu pick** (`ClusterInputListener` ChoiceListener; the on-screen
  "Map / Last destinations / Favorites" menu) → `favoredViewMode`.
- `refreshViewMode()` maps favored→actual by readiness (rgiValid / isKDKReady / isMapReady); MAP(3)→
  `switchDisplayContextKombi(8)`, KDK(2)→`(9)`, `showKombiMap`. Both models live in NavigationEnv/
  `ClusterService` — which we already patch → subscribing to 67 is trivial.

## Input model (rotary/roller)
Audi rotary = `VirtualButtonModel` + `VirtualButtonListener` (`de/audi/atip/hmi/model/`).
`increment/decrement(i, delta, k)` = rotary steps; `keyPressed(i, keyId, k)` = buttons
(DDS_SELECT=17, BACK=15). Template: `de/audi/app/terminalmode/keyevents/TMVirtualButtonListener.java`
(registers on `TERMINAL_MODE_TOUCH_MEDIA_HANDLER_VIRTUAL_BUTTON` → `updateRotary` → `postRotaryEvent`
= the MAIN-screen central-knob path). The steering-wheel MFL roller drives the CLUSTER via a
VirtualButton bound in the cluster screen-bag (`de/audi/tghu/terminalmode/hmi/evohigh/
TerminalModeScreenBag1.java`, `setMflRollerUpIncrementsModel`).

## Focus / ownership
`Resource.SCREEN` ownership (`isResourceOwnerDevice(Resource.SCREEN)`), `ExternalEventsListener.
hmiActivated/deactivated`, per-app SMM focus actions, `MMICombiActionProxy.setMMICombiContext`.

## PLAN (cluster altScreen tab)
Goal: track active tab; **Plan A** = add a dedicated altScreen cluster tab; else **Plan B** = replace
the stock nav MAP stream. In our tab: steering-wheel scroll → altScreen (`CMD_ALT_HID`).

- **Plan A (new tab):** extend the fixed enum {COMPASS,RGI,KDK,MAP} with ALT — patch `ClusterViewMode`
  (mode + refreshViewMode + display context) + `ClusterInputListener` (menu item + validateViewMode)
  + the cluster HMI menu (400140 screen-bag) to render a new entry. Feasible; costliest bit = a new
  visible menu entry in the widget/HMI layer.
- **Plan B (replace MAP stream):** reuse VIEWMODE_MAP(3) — the KOMO/displayable already pipes cluster
  video on MAP; feed altScreen there instead of the stock map, and **hide the stock nav side-handlers**
  (the "Map/Last-dest/Favorites" quick menu + the "Options: Map colors/display/orientation/Automatic
  zoom" popup). Less HMI-menu work; it's a hijack of the existing MAP surface.

- **KDK maneuver_render visibility:** gate on `ChoiceModel(67)` — HIDE our KDK maneuver render when the
  active tab is altScreen (A: ALT mode; B: hijacked MAP), SHOW on RGI/KDK/COMPASS. Do NOT touch BAP —
  projection rides `CombiBAPListener`/`AppConnector`, independent of render visibility.
- **Scroll routing:** when active tab == altScreen → a cluster `VirtualButtonListener` forwards
  increment/decrement + select → `CMD_ALT_HID`.

## Java build blockers to resolve
- `CarplayBus.send()` is STUBBED ("one-way bus") → implement the writer (frame the 16-byte header +
  payload to the hook socket) before any `CMD_ALT_*` can be sent.
- Identify the cluster roller VirtualButton model ID (from `TerminalModeScreenBag1` evohigh) + whether
  to replace or wrap its listener (don't break native cluster nav on non-altScreen tabs).
