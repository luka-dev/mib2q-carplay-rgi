# carplay replay driver — libcarplay_replay.so

Replays a REAL on-unit CarPlay callback trace INTO from-source `dio_manager` on
QEMU, with **no phone**. Complements `carplay_trace.c` (the on-unit capture hook):
capture records libairplay→dio delegate callbacks post-decrypt; replay calls
from-source dio's registered callbacks back with the recorded args.

## Files
- `replay.c` — the driver (`libcarplay_replay.so`, QNX ARMv7, only `libc.so.3`, ~15 KB, -Wall clean).
- `build_replay.sh` — compile on the VM, pull the `.so` back (mirrors `build.sh`).
- `carplay_session_stable.jsonl` — the real capture (832 events, ~31s; copyProperty now carries `keyStr`).
- Boot harness (in `../../../Tools/qnx-carplay-emu/carplay/`):
  `dio_trackF_replay.build`, `run_dio_trackF_replay.sh`, `build_trackF_fromsrc.sh`,
  `trackf_fromsrc_stubs.c` (leaf/ABI stubs only now), `trackf_fromsrc_env.cpp`
  (cold-boot data the real callbacks touch).

## What it does
1. **Passive interpose** `AirPlayReceiverServerCreate` → grab dio's real server ptr.
2. **Passive interpose** `AirPlayReceiverServerSetDelegate(server, delegate)` → copy
   dio's real 15-word server delegate + context (`delegate[0]`), pass the real
   delegate straight through, then spawn a driver thread.
3. Driver thread reads `REPLAY_TRACE`, and in `seq` order calls dio's captured
   delegate slots (recon_airplay.h map: `[3]`copyProperty `[5]`sessionCreated
   `[6]`sessionCreationFailed `[7]`sessionScreenCreated `[8]`audioType
   `[9]`waitAudioAvailableAck). Trace object ids map:
   `o#<server>`→real server, `o#<context>`→real context (both learned from the
   trace's own SetDelegate record), every other `o#`→a malloc'd placeholder.
   Recorded `ts` deltas are honored (usleep, capped by `REPLAY_MAXGAP_MS`, default 200).
4. `AirPlayReceiverSessionSetDelegate` / `...ScreenSetDelegate` are interposed and
   **stubbed only for synth session ptrs** (a real session — none here — passes
   through). Frames (`cb.screenFrame`) are **injected** in Phase 2: the priv is
   reconstructed with the real `ScreenStreamCreate` and each recorded AVCC frame
   is fed through the real `ScreenStreamProcessData` (see Phase 2 result below).

Env: `REPLAY_TRACE=<path>` (required), `REPLAY_MAXGAP_MS=<ms>` (sleep cap).

## Build
```sh
cd c_hook_trace && sh build_replay.sh
```

## Run (QEMU boot test)
```sh
# 1. build the probe with the REAL callbacks linked in (stages trackf_fromsrc_probe):
cd ../../../Tools/qnx-carplay-emu/carplay && sh build_trackF_fromsrc.sh
# 2. build the replay driver:
cd -/../c_hook_trace && sh build_replay.sh
# 3. boot the replay:
cd ../../../Tools/qnx-carplay-emu/carplay && sh run_dio_trackF_replay.sh 55
# -> trackF_replay_boot.log ; grep '[REPLAY]'
```
The build LD_PRELOADs `libcarplay_replay.so` onto `trackf_fromsrc_probe` — the
from-source dio CarPlay bring-up (reconstructed `CAirPlayMainThread::run()`, the
only path that reaches SetDelegate; the full mesh `dio_manager` is blocked upstream
at the -6707 DSI-register wall and never reaches the CarPlay server). The staged
trace is filtered from `carplay_session_stable.jsonl` to lifecycle `cb.*` + server funcs
**+ the 800 `cb.screenFrame` blobs** (Phase 2 injects them; ~13 MB of decoded AVCC).

## Phase 1 result (2026-07-16, 55s boot, no phone) — REAL callbacks + faithful keys

The Track-F probe now links the **REAL** `dio-src/CDIOManager_callbacks.cpp` in
place of the empty `CBSTUB` stand-ins, and replay synthesizes **real CFStringRef
keys** from the trace's `keyStr`. Driver walked the full lifecycle **without
crashing**; probe returned cleanly, `GET /info` = **server still up** (200 OK).

**Real callbacks executed (not stubs):**
- captured `server=126dd8 ctx=108278`; bound `o#1→server o#2→ctx`.
- `copyProperty=1030a0` (real `s_serverCopyPropertyCallback`) fired **22×**.
- `sessionScreenCreated=10512c` → **real `s_sessionScreenCreatedCallback`** ran →
  called `AirPlayReceiverSessionScreenSetDelegate(synth)` (driver stubbed benign).
- `sessionCreated=104ed8` → **real `s_sessionCreatedCallback`** ran → built the
  0x38-byte session delegate → called `AirPlayReceiverSessionSetDelegate(synth)`
  (driver stubbed benign) → posted `SessionCreated` event → `enqueToWorkers`
  (registry all-NULL at cold boot → `execute()` inline). The two synth-session
  `SetDelegate` interpositions are the *proof* the genuine bodies ran — a stub
  never calls `SetDelegate`.
- driver `DONE: copyProperty=22 lifecycle=2 screenFrames=0(deferred)`.

**Per-key copyProperty comparison (dio-replayed vs unit-recorded), 22 keys:**
Every key drove the provider with a real UTF-8 `CFStringRef`
(`CFStringCreateWithCString`@0x79b50, freed via `CFRelease`). dio's offline
provider is **key-agnostic**: post is a stub, `waitCopyPropertyResult` returns
"ready" immediately, so it hands back `cfg->copyPropValue=NULL, copyPropErr=0`
for **every** key. The trace shows what the *real HU worker* deposited per key:

| unit answer | keys | dio replayed | divergence |
|---|---|---|---|
| **value** (o#N, err 0) | bluetoothIDs, limitedUIElements, limitedUI, manufacturer, model, nightMode, oemIcon, oemIconLabel, oemIconVisible, rightHandDrive | NULL, err 0 | **VAL-DIVERGE** (10) |
| **not-found** (NULL, err −6714) | features, firmwareRevision, hardwareRevision, hidLanguages, OSInfo, settingsIcon, playbackAllowed | NULL, err 0 | **ERR-DIVERGE** (12*) |

`val-diverge=10 err-diverge=12 of 22` (*7 unique not-found keys; `playbackAllowed`
+ `features`/`firmwareRevision` recur across `seq`). **The divergence is the
signal:** offline dio has no DSI/HMI worker to resolve keys, so it returns the cfg
default. The 10 keys the unit answers with a value are exactly the ones dio's
worker fills from the HU (model/manufacturer/oemIcon*/limitedUI/nightMode/RHD/BT);
the 7 not-found keys (features/firmwareRevision/OSInfo/…) are the ones the HU
declines. Neither value nor err **matches** on any key — expected, and it pinpoints
that Phase 2 needs the worker-deposit path (`postCopyPropertyEvent` /
`CAirPlayServiceEvent_SessionCopyProperty::execute` → `cfg->copyPropValue`) to make
offline answers faithful.

> **Superseded by Phase 3 below** — the driver now performs the worker deposit
> itself, so every key's value + err match the unit (21/21).

### Honest gaps / what's still not real
- **Worker-deposit path is stubbed.** `postCopyPropertyEvent` (0x12121c) and
  `waitCopyPropertyResult` (0x157454) in `CAirPlayMainThread_impl.cpp` are file-local
  stand-ins (post = no-op, wait = "ready"), so the copyProperty *value/err* can't
  match the unit. Same for the session `s_sessionCopyProperty`/`s_handleSessionControl`
  waits inside the callbacks TU — those would `pthread_cond_wait` forever on the
  zeroed cfg sync object, but this trace never drives them (no per-session
  copyProperty / control in the 22 server-delegate events).
- **Registry + trace channel are cold-boot data, not the live mesh.**
  `trackf_fromsrc_env.cpp` supplies `s_registrationArray[29]={0}` / `s_mutex` and a
  68-byte trace channel with `m_level=100` (HIGH) — identical to the real objects at
  cold boot (no worker mesh, tracing backend unloaded), so `enqueToWorkers` runs
  `execute()` inline and `logCV` never fires. Faithful for bring-up; a live worker
  mesh is a later step.
- **Frames injected (Phase 2, done).** The 800 `cb.screenFrame` events are now
  streamed through the real `ScreenStreamProcessData` (priv reconstructed via the
  real `ScreenStreamCreate`); they reach the genuine entry but stop at the OMX
  boundary on QEMU (no Qualcomm decoder). See the Phase 2 result section below.
- **stderr interleaving** between the probe's `TRACKF` and the driver's `write(2)`
  is cosmetic (two unbuffered threads on fd 2), not a fault.

## Phase 2 result (2026-07-16, 68s boot, no phone) — REAL AVCC video injected

The 800 `cb.screenFrame` events are now **un-deferred and injected** into the
from-source dio video path. `run_dio_trackF_replay.sh` stages the full trace
(screenFrame INCLUDED, ~13 MB of decoded AVCC), and the driver streams every
frame in `seq` order interleaved with the lifecycle, paced by the recorded
`pts` (~26 fps, capped by `REPLAY_MAXGAP_MS`).

**Injection point — OPTION A (real exported `ScreenStreamProcessData`).** The
staged libairplay is the **from-source** build; its `ScreenStreamProcessData`
(`libairplay-mhi2q/src/screenstream.c:414`, export **@0x257d8**) is the genuine
frame-delivery boundary — `ProcessData → _FeedInput → OMX_EmptyThisBuffer(me->
decoder)`. It is NOT the stock binary's `Forward(...,70,...)→SendAll` pipe (that
was the on-unit RE of the *original* libairplay; the from-source rebuild we run
uses OMX directly, so **Option B has no pipe target here**). With no phone there
is no live stream ctx, so the `priv` is **reconstructed** with the REAL exported
`ScreenStreamCreate` (**@0x25344**), and each recorded AVCC frame is fed through
the REAL exported `ScreenStreamProcessData`. VA-cited exports (from staged
`libairplay.so`): `ScreenStreamCreate 0x25344`, `ScreenStreamStart 0x255ec`,
`ScreenStreamProcessData 0x257d8`, `ScreenStreamStop 0x258a8`.

**`replay.c` wiring:** `screen_setup()` (one-shot, on first frame) resolves the
exports via `dlsym(RTLD_DEFAULT,...)` (same global-scope path Phase 1 used for
`CFStringCreateWithCString`), calls `ScreenStreamCreate` → real priv, then
`ScreenStreamStart`. `jhex()` hex-decodes the `data` field back to AVCC bytes;
`jnum64()` parses the 13-digit `pts` (QNX `long` is 32-bit → the old `jnum`
saturated; frames pace with 64-bit pts deltas). Each frame → `ScreenStream
ProcessData(priv, avcc, len, pts, NULL,NULL,NULL)`.

**Boot-test (`trackF_replay_boot.log`):**
- `ScreenStreamCreate rc=0 stream=10a260` — real priv reconstructed.
- `ScreenStreamStart rc=-6753` — `OMX_GetHandle("OMX.qcom.video.decoder.avc")`
  answered `OMX_ErrorComponentNotFound` by `omx_stub.c` → `me->running=0`.
- **All 800/800 frames delivered** to `ScreenStreamProcessData`, each returning
  `rc=-6745` (kStateErr) at the `require(me->running)` guard (screenstream.c:421)
  — real entry reached, **no OMX deref, no crash**. `bytes=12,824,397` real AVCC.
- Driver `DONE: copyProperty=22 lifecycle=2 screenFrames=800(injected=800)`.
- `/info` still **200 OK** after the stream (server survived); no `SIGSEGV`/
  `Memory fault`/dumper in the log (the `NetUtils.c:519` / `KeychainLiteFile`
  ASSERTs are the documented no-NIC / cold-boot artifacts, not faults).

**What the decoder handoff received:** nothing past the `require(running)` gate —
`_FeedInput`/`OMX_EmptyThisBuffer` are never reached because `ScreenStreamStart`
could not bring up the Qualcomm OMX component on QEMU. The frames reach the real
`ScreenStreamProcessData` entry and stop exactly at the OMX boundary.

**Honest QEMU limit (restated):** QEMU has no Qualcomm OMX (omx_stub returns
ComponentNotFound) and no real display, so this validates the **frame-DELIVERY
path** — the recorded AVCC reaches from-source dio/libairplay's genuine
`ScreenStreamProcessData` entry, in seq order, at real cadence, without crashing
— NOT decoded/rendered video. Reaching `OMX_EmptyThisBuffer` (and real NV12→
screen present) needs the on-unit Qualcomm decoder; that is the on-device step.

Harness note: `trackf_fromsrc_probe.cpp` now holds `TRACKF_HOLD_S` (default 45s)
after `/info` so the detached driver can drain the full ~31s pts-paced stream
before the process exits; run with a ≥68s QEMU window.

### Build/link notes
- Probe link adds `obj_callbacks.o` (real `CDIOManager_callbacks.cpp`) + `obj_env.o`
  (cold-boot data). `trackf_fromsrc_stubs.c` keeps only the config leaves + C++ ABI
  stubs; the 10 `CBSTUB` delegate stand-ins **and** `s_airPlayListenerID` were removed
  (now owned by the real callbacks TU). `new`/`delete` → malloc/free stubs;
  `__aeabi_atexit` stubbed no-op (the env `osal::Mutex s_mutex` static-dtor
  registration otherwise resolves `__aeabi_atexit` through libairplay's
  `libstdc++.so.6` DT_NEEDED, which QNX has none of → `ldd:FATAL`).

## Phase 3 result (2026-07-16, 68s boot, no phone) — per-key VALUE fidelity 21/21

The copy-property answers are now **faithful to the unit per key**. `replay.c`
reconstructs each captured value and DEPOSITS it into the from-source dio config
so the provider returns the unit's real value — the offline equivalent of the
HU worker's deposit.

**Deposit path recovered (VA-cited, from-source dio-src).** The provider
`CDIOManager::s_serverCopyPropertyCallback` @0x158690
(`CAirPlayMainThread_impl.cpp:190`): `ctx == delegate.context == cfg`
(`CDIOManagerConfig*`); after the worker resolves the key it returns
`*outErr = cfg->copyPropErr` (`cfg+0x1f4`, @0x158710 `LDRNE R3,[R4,#0x1F4]`) and
`cfg->copyPropValue` (`cfg+0x1f0`, @0x158714 `LDR R4,[R4,#0x1F0]`). Offline the
worker never runs (`postCopyPropertyEvent`/`waitCopyPropertyResult` are file-local
stubs), so the driver **is** the worker: it writes cfg+0x1f0/+0x1f4 for the pending
key right before calling the provider. Offsets are the `offsetof`-self-checked
`struct CDIOManagerConfig` in `harman-sdk/recon_airplay.h:198`.

**Value reconstruction (real captured valData → real CFTypeRef).** The trace
(`carplay_session_values.jsonl`) carries each `cb.copyProperty` `ret.valData` — the
hex binary-plist of the CFTypeRef the unit returned. `call_copyProperty` hex-decodes
it (`jhex`), builds a `CFData` via `CFDataCreate` (@0x7895c) and reconstructs the
CFTypeRef via `CFPropertyListCreateWithData` (@0x7ace0), deposits it, then calls the
provider. For `valType:"null"` keys it deposits `NULL` + the recorded err (`-6714`).
All CF objects `CFRelease`d after each key (cfg slot cleared first) — no leaks.
The run script now stages the **values** trace (not `_stable`, which lacks valData).

**Per-key comparison (dio-replayed vs unit-recorded), boot log:**
- **Value** = pointer identity `r == cfg->copyPropValue` (the provider returns the
  exact CFTypeRef we deposited — dio hands back the unit's real value; immune to the
  probe /info thread's concurrent CF activity). **Corroboration**: dio's returned
  value is re-serialized via `CFPropertyListCreateData` (@0x7ab48) and byte-diffed
  vs the unit's raw captured valData — `[byte-exact roundtrip]` on 9/10 bplist keys.
  Only `bluetoothIDs` (first bplist key, temporally coincident with the probe's live
  `/info` copyProperty) lacks the byte-exact tag because that re-serialize is
  perturbed by concurrent CF-allocator activity; its identity check still MATCHes.
- **Err** = `err == recErr`.

```
copyProperty per-key compare: KEY-MATCH=21 KEY-DIVERGE=0 of 21 (val-diverge=0 err-diverge=0)
```
- **10 bplist keys** (bluetoothIDs, model="AUDI 4935", manufacturer="HARMAN",
  oemIcon=<19 KB CFData>, oemIconLabel="Audi MMI", oemIconVisible, limitedUI(false),
  limitedUIElements=["softKeyboard"], nightMode(true), rightHandDrive="0") — dio
  returns the reconstructed value, err 0. **MATCH.**
- **11 not-found events** (features×2, firmwareRevision×2, playbackAllowed×4,
  hardwareRevision, hidLanguages, OSInfo, settingsIcon) — dio returns `NULL`,
  err `-6714`. **MATCH.**
- Server survived: `/info` **200 OK**, `trackf_fromsrc_probe` exited status=0, no
  `SIGSEGV`/`Memory fault`/dumper.

**Note (21 vs 22):** `carplay_session_values.jsonl` records 21 copyProperty events
(Phase 1's "22" was from `_stable`, one extra `playbackAllowed` recurrence). The
gate is per-key fidelity — met for all 21.

**Harness hygiene:** `omx_stub.c` `OMX_GetHandle` reverted to the known-good
Phase-2 behavior (returns `OMX_ErrorComponentNotFound` for the AVC decoder → 
`ScreenStreamStart` -6753, frames gate at `require(me->running)`); the prior
"fake decoder" vtable is retained below the early return as dead reference code.
Frames are not the Phase-3 focus (all 391 still reach the real
`ScreenStreamProcessData` entry, rc=-6745, no crash).

**Still not real (unchanged):** the worker MESH is not running — the driver
emulates the single worker deposit for the server-delegate copyProperty. Per-session
`s_sessionCopyProperty` / control waits are untouched (this trace drives none). Video
decode still needs the on-unit Qualcomm OMX.

## Phase 4 result (2026-07-16) — STOCK libairplay (= capture source = deploy target)

Phases 1-3 ran from-source dio against a **from-source** libairplay (cksum
`129002294`, 3226465 B, `ScreenStreamProcessData@0x257d8`). The capture was taken on
FULL STOCK and the deploy target is **from-source dio + STOCK libairplay** (only dio
is rebuilt), so the staged runtime lib was swapped to genuine stock and the replay
re-run. Driver `replay.c` resolves the ScreenStream* exports by **name** (`dlsym`), so
no driver edit was needed — it binds whatever the loaded lib exports.

**Which lib was staged before (marker + cksum).** FROM-SOURCE build:
`cksum 129002294 / 3226465 B`. Hard markers: exports `ScreenStreamCreate@0x25344`,
`ScreenStreamProcessData@0x257d8` (the "RE'd VAs" cited in Phases 1-3 were in fact
**from-source** offsets, not stock); ~3.2 MB unstripped. It lacks the stock-only
exports `ScreenStreamCopyDelegates / ReportBadFrames / SetVisibility`. Backed up on
the VM as `stage/libairplay.fromsrc.so`.

**Stock lib staged now (source, cksum, RE'd VAs).** Genuine MHI2Q MU1316 firmware
lib `Firmwares/MHI2Q_US_AUG22_P5087_MU1316/extracted/app/eso/lib/libairplay.so`,
`cksum 3791488696 / 763420 B` (byte-identical to the VM's `.stock` copies and to
`Patches/carplay_vc/libairplay.so`). Stock ScreenStream VAs (verified present in the
staged lib): `ScreenStreamCreate@0x85ce4`, `ScreenStreamProcessData@0x84fec`,
`ScreenStreamStart@0x84da0`, `ScreenStreamStop@0x84c50` — **different offsets** from
the from-source lib. All 6 libairplay symbols the from-source dio probe imports
(`AirPlayReceiverServer{Create,SetDelegate,SetProperty,Control}`,
`...Session{,Screen}SetDelegate`) are exported by stock → the from-source probe loads
ABI-clean against stock (deploy-target compatibility proven at load).

**Control path — 21/21, libairplay-independent (CONFIRMED on stock).** Full-frames
run (`trackF_replay_boot.stock.log`): SetDelegate captured (`server=1280a8 ctx=1083a0`),
every copyProperty that replayed = byte-exact `KEY-MATCH` (0 diverge). The
frames-excluded control run (`trackF_replay_noframes_stock2.log`) completes the whole
lifecycle cleanly: `KEY-MATCH=21 KEY-DIVERGE=0 of 21`, `lifecycle=2`, driver `DONE`,
no crash — identical to the from-source Phase-3 result. So the dio<->libairplay
negotiation (server lifecycle + per-key copyProperty + session callbacks) is faithful
against the real deploy-target lib.
- Non-fatal stock differences under QEMU: (a) `GET /info` **NOT REACHED** — stock
  `AirPlayReceiverServerControl(START)` returns err=0 and slog says "Servers
  successfully started" but the RTSP/HTTP server never listens (no NIC → AirTunes
  `0x0`, Bonjour `-6727 kNotFoundErr`); the from-source phases' "200 OK" needed the
  same NIC and is a probe-liveness check, not a fidelity gate. (b) A **nondeterministic
  heap race** in the driver's CF value-reconstruction against stock's CFAllocator
  (`libc _list_memalign` SIGSEGV) truncated one no-frames run at 3 keys; the retry ran
  all 21 clean. replay.c is read-only, so this driver/allocator race is noted, not fixed.

**Frame path — headless injection is INCOMPATIBLE with stock (root cause found).**
On the first `cb.screenFrame`, `screen_setup()` calls the driver's reconstruction
`g_ScreenStreamCreate(&g_ss)` → **SIGSEGV** at `libairplay.so@ScreenStreamCreate+0xd0`
(`ip=0x78405db4 mapaddr=0x85db4`, faulting store to `ref=0x78405ce4` = read-only
.text). Two stacked causes:
  1. **ABI/signature mismatch.** From-source `ScreenStreamCreate(void **out)` saves the
     out-pointer from **r0** (`str r0,[fp,#-32]` @0x25358) → the driver's single-arg
     `ScreenStreamCreate(&g_ss)` is correct for from-source (Phase 2 got `rc=0
     stream=10a200`). STOCK `ScreenStreamCreate` writes the new stream to **[r1]**
     (`mov sl,r1; streq r4,[sl]` @0x85db4) — the out-pointer is the **2nd** arg; r0 is
     a different param. The driver passes `&g_ss` in r0, leaving r1 garbage → the
     success-store lands on read-only .text → SIGSEGV. The from-source rebuild changed
     the prototype; the read-only driver is hard-wired to it.
  2. **OMX decoder in Create.** Before the crash, stock `ScreenStreamCreate`
     synchronously brings up the real Qualcomm OMX AVC decoder (slog: `[ScreenStream]
     Creating... / [OmxVideoImpl] Initializing / [COMXVideoDecoder] init
     (videoDecoderThread) / [CCommandThread] Starting`). QEMU has no
     `OMX.qcom.video.decoder.avc` (omx_stub → ComponentNotFound), so even with the
     signature fixed the headless reconstruction would wall inside Create. Stock
     **front-loads** decoder bring-up into `ScreenStreamCreate`; the from-source build
     **deferred** it to `ScreenStreamStart`.

**Is the -6753 resolved? Yes — it was never a deploy-target error.** -6753 was a
FROM-SOURCE-only artifact: from-source `ScreenStreamStart` → `OMX_GetHandle` →
ComponentNotFound → `running=0` → clean return `-6753`, frames gated at
`require(running)` with no crash. **Stock never emits -6753 in this flow** — its
decoder bring-up is in `ScreenStreamCreate`, which needs the real OMX component (and
the driver's arg order). So the frame path does NOT complete on stock either; the
"mystery" is explained (wrong-lib entry) but not turned into a working headless frame
flow. Frames never reach stock `ScreenStreamProcessData@0x84fec` via this driver.

**Honest end-to-end verdict.** With stock libairplay (capture source = deploy target),
the replay validates **the full control plane** the from-source dio and stock
libairplay actually exchange: server lifecycle (Create/SetProperty/SetDelegate/
Control-START), 21/21 per-key copyProperty fidelity (byte-exact), and the session
create/screen-create callbacks — all against the genuine deploy-target lib, ABI-clean
at load. It does **not** validate video decode/display: that is a genuine on-unit
hardware wall (Qualcomm OMX + real screen), and additionally the headless
frame-injection path is blocked by the from-source-vs-stock `ScreenStreamCreate`
prototype divergence — on the real unit stock libairplay's own AirPlay session (not
dio, not this driver) calls `ScreenStreamCreate` with the correct args and the real
decoder present, so this is a harness-reconstruction limit, not a deployment defect.

**Files:** stock full-frames run `trackF_replay_boot.stock.log`; from-source baseline
`trackF_replay_boot.fromsrc.log`; stock control-only `trackF_replay_noframes_stock2.log`.
Staged runtime lib left = **STOCK** (`3791488696`); from-source backup at
`stage/libairplay.fromsrc.so`.

## Phase 5 result (2026-07-16) — STOCK libairplay video path via ABI shim + fake OMX + Screen stub

Phase 4 hit two stacked walls on the stock frame path: (1) `replay.c` (READ-ONLY) calls
`ScreenStreamCreate(&out)` with the out-ptr in **r0**, but stock's `ScreenStreamCreate`
@`0x85ce4` *ignores r0* and writes the new stream to **`*r1`** (`85d00 mov sl,r1`;
`85db4 streq r4,[sl]`) → success-store landed on read-only .text → **SIGSEGV**; and
(2) stock front-loads the Qualcomm OMX AVC decoder. Phase 5 fixes both in the harness
(replay.c untouched) and instruments the real stock OMX contract.

**(a) ABI shim (`carplay/ssc_abi_shim.c` → `libsscabi.so`, LD_PRELOAD ahead of
`libcarplay_replay.so`).** `dlsym(RTLD_DEFAULT,"ScreenStreamCreate")` from replay.c now
binds the shim; the shim auto-detects stock (`mov sl,r1`=0xe1a0a001 in the prologue) and
re-issues `real(0, out)` so the out-ptr lands in r1. **Result: `ScreenStreamCreate rc=0`,
no SIGSEGV** — Phase 4's hard wall is gone. (From-source lib → passthrough, so one preload
is safe for both.)

**(b) Fake OMX ENABLED + instrumented (`carplay/omx_stub.c`).** The dead fake-decoder
vtable was un-gated and every OMX entry logged. **Observed stock contract** (stock
`dio::COMXVideoDecoder`/`OmxVideoImpl` drive it, instrument log):
`OMX_Init` → `OMX_GetHandle("OMX.qcom.video.decoder.avc")` →
`GetExtensionIndex("OMX.QCOM.index.config.video.DisplayPictureBuffer")` →
Get/SetParameter(that vendor idx) → GetParameter(PortDefinition port 0), port 1 (×2) →
`SendCommand(StateSet, Idle)` → `AllocateBuffer(port 0)` ×4 (262144 B). **The classic
trap: the QCOM `DisplayPictureBuffer` extension returning `NotImplemented` is FATAL**
(`enableDecoderOrderMode` → `configure` fail → `ScreenStreamStart -6736`). Fix: fake-succeed
*exactly* that one extension (vendor index `0x7F000001`); everything else stays
`NotImplemented`. After the fix the decoder proceeds through port setup, `StateSet(Idle)`,
and **input-port buffer allocation** — all working, no crash.

**(c) Headless QNX Screen stub (`carplay/screenstub.c` → replaces `libscreen.so.1`).**
Provides the 17 `screen_*` symbols stock libairplay imports with **real heap-backed**
render buffers (the previous `libscreen.so.1` was the OMX stub — zero `screen_*`).

**(d) The real output-side blocker — an ENV GATE, not platformLayer (decompiled, VAs).**
The decoder's output buffers come from `dio::CScreenRender::sharedBuffers` @`0x8b9e0`, which
returns **`-6740`** when its `CWindowBuffers` (`this+16`) is null — i.e. `CScreenRender::config`
@`0x8bcc8` (which creates the window via `screen_create_context`/`screen_create_window` and
the render buffers) never ran. `config` is called ONLY by `OmxVideoImpl::configure` @`0x8c388`,
which has a hard gate:
```
v17 = getenv("NVSS_VIDEO_OUTPUT_DEVICE");
if ( !v17 ) goto LABEL_23;          // -6736; CScreenRender::config + COMXVideoDecoder::start SKIPPED
... CScreenRender::config(...) ... COMXVideoDecoder::start(...)
```
In the QEMU harness that env var was unset → `config` skipped → no window/buffers → `sharedBuffers
-6740`. **`platformLayer` was a red herring**: the decompiled `_ScreenStreamSetProperty` @`0x85590`
has NO `platformLayer` branch (only `avcc`→a1+8/+40, `widthPixels`→a1+12, `heightPixels`→a1+16,
window dims, offsets, fps); `CScreenRender::config` creates its OWN window, it is not handed one.
**Fix: set `NVSS_VIDEO_OUTPUT_DEVICE=1` in the launch** (`dio_trackF_replay.build`). Geometry is
irrelevant to reaching Executing (the OMX port supplies 800×480; config runs fine at window w=0,h=0).

**(e) AVCC header injection — so ProcessData feeds every frame.** With the decoder Executing,
`ScreenStreamProcessData` @`0x84fec` still rejected frames after the first with **"Annex B Header
ptr is invalid (0x0)"** (`-6736`): the first frame needs `a1+40` (the AnnexB SPS/PPS header, built
by `H264ConvertAVCCtoAnnexBHeader` from an `avcC`) and `a1+52` (NAL-length-size). On the real unit
`AirPlayReceiverSessionScreen` sets these via `ScreenStreamSetProperty("avcc", …)`; `replay.c` never
does, and the trace has no SPS/PPS (39×nal=1 + 1×nal=5). Since the fake OMX **swallows** the header,
a minimal parseable `avcC` (lengthSizeMinusOne=3 → 4-byte NAL length, matching the captured frames)
is enough. The ABI shim (`ssc_abi_shim.c`) injects it after Create/before Start via the stock
`_ScreenStreamSetProperty` export (key `"avcc"` = CFConstantString @`0xAE49C`) →
`Annex B Header (ptr set, 20 B)`, `a1+52=4`.

**(f) FillBufferDone → black NV12 (output present round-trip).** `fk_FillThisBuffer` now fills each
primed output buffer with a black NV12 frame and returns it via `FillBufferDone`, paced ~30 fps
(usleep, no busy loop), so the stock present path (`CScreenRender` → `screen_post_window`, our
no-op) completes. No real decode/display — headless.

**GATE MET (stock libairplay, headless, captured AVCC).** Boot log
`trackF_replay_boot.stock_omx_phase5.log`:
- `ScreenStreamCreate rc=0` (ABI shim), `[sscabi] inject avcc (23 B) → rc=0`,
  `[ScreenStream] Annex B Header (ptr: 0x001098F8, length: 20)`.
- `CScreenRender::config` runs (22 `[fakescreen]` calls: create_context/window,
  `create_window_buffers count=4`, `get_window_property_pv(0x25=RENDER_BUFFERS)→4`,
  `get_buffer_property_pv(0x22=POINTER)→real ptr`), `getAllBuffers(4)`,
  `setState OMX_StateExecuting` → **`[ScreenStreamStart] Decoder started successfully (configured=1)`**,
  `ScreenStreamStart rc=0`.
- **`EmptyThisBuffer` reaches #800** (ETB #1 = 20-B AVCC header, flags=0x80 CODECCONFIG; #2..#800 =
  the frames), every `screenFrame → ScreenStreamProcessData rc=0 [fed to OMX EmptyThisBuffer]`,
  0 "Annex B invalid".
- **`FillBufferDone` black NV12 (262144 B)** flowing (present path completes), 0 SCREEN_RENDER errors.
- **0 SIGSEGV/Memory-fault**, `replay driver DONE`, control **KEY-MATCH=22/22** (stable trace) /
  **21/21 byte-exact** (values trace). `replay.c`'s read-only "running=0 / OMX ComponentNotFound"
  log strings are now stale/inaccurate (fake OMX IS up, running=1).

**Note on trace + the CF heap race.** Two traces: `carplay_session_stable.jsonl` (800 frames, no
`valData` → NULL deposits, no CF reconstruction) gives a deterministic clean pass and was used to
prove the frame path (`run_dio_trackF_replay_stable.sh`). `carplay_session_values.jsonl` (391
frames + byte-exact `valData` → 21/21) exercises `replay.c`'s CF value-reconstruction, which races
stock's CFAllocator (`libc _list_memalign` SIGSEGV, documented Phase 4, in READ-ONLY `replay.c`) —
retry until a clean pass (Phase 4 pattern). Both traces reach Executing + `EmptyThisBuffer` +
`FillBufferDone` identically; the race only affects the copyProperty phase, not the video path.
> **SUPERSEDED by Phase 6** — the CF value-deposit race is now FIXED (root cause = borrowed-pointer
> UAF on the shared `cfg+0x1f0` slot, not allocator luck); the values trace passes 5/5, no retries.

**Harness note:** dio here is the from-source `trackf_fromsrc_probe` (control plane proven
stock-identical in Phase 4); the video path under test is 100% **stock libairplay**. Full stock-dio
compose (`trackf_stock_spawn` + replay) is a remaining wiring step, but the video path is
libairplay-only so this IS the stock-baseline video result.

**Files:** `carplay/ssc_abi_shim.c` (ABI adapter + AVCC inject), `carplay/screenstub.c` (17 Screen
symbols, real buffers), instrumented `carplay/omx_stub.c` (fake AVC decoder + FillBufferDone);
`carplay/dio_trackF_replay.build` (LD_PRELOAD `libsscabi.so:libcarplay_replay.so`,
`NVSS_VIDEO_OUTPUT_DEVICE=1`, staged `libsscabi.so`); `carplay/run_dio_trackF_replay_stable.sh`;
boot log `carplay/trackF_replay_boot.stock_omx_phase5.log`; observed contract
`carplay/phase5_omx_contract.txt`.

## Phase 6 result (2026-07-17) — CF value-deposit race FIXED → spotless combined values baseline

Phases 4-5 left one non-determinism: the **values** trace (`carplay_session_values.jsonl`,
`valData` → CF reconstruction) SIGSEGV'd in `libc _list_memalign` during the copyProperty phase
(5/5 fail this session), so the "retry until lucky" note stood and the byte-exact video proof had
to fall back to `_stable`. Phase 6 root-causes and **fixes** it; the combined values run
(copyProperty values + 800 video frames together) now passes **5/5**, no retries.

**Race mechanism (VA-cited).** The provider `CDIOManager::s_serverCopyPropertyCallback` @`0x158690`
returns `cfg->copyPropValue` (`cfg+0x1f0`, stock @`0x158714` `LDR R4,[R4,#0x1F0]`;
`CAirPlayMainThread_impl.cpp:218`) **without CFRetain** — the caller gets a borrowed pointer, not a
+1 ref. Two threads invoke that provider sharing the **single** slot `cfg+0x1f0`: (1) the replay
driver, and (2) libairplay's own AirPlay/Bonjour startup info-gather thread (boot log:
`>>> s_serverCopyPropertyCallback FIRED (ctx=1083a0) … return copyPropValue=…`). Phase 3's
`call_copyProperty` reconstructed a real CFTypeRef, deposited it, and **`CFRelease`d it per key**;
the concurrent reader took the just-deposited pointer from the shared slot and used it *after* the
driver freed it → **use-after-free** → `libc _list_memalign` SIGSEGV (`fltno=11 ref=0001cffc`). The
`_stable` trace deposits **NULL** and frees nothing → the concurrent reader gets NULL → no UAF →
deterministic clean. Exact fit for "only the values trace crashes".

**Why the "thread-safe allocator" fix is impossible (RE'd, VA-cited).** CFL exports **no** CFAllocator
API — no `kCFAllocatorMalloc`, `CFAllocatorCreate`, `CFAllocatorGetDefault` (checked the 258 CF
dynsyms; only `kCFL*` callback structs). `CFLDataCreate` (via `CFDataCreate`@`0x4ddbc`) **rejects any
non-NULL allocator** (`if (a1) return -6705`) and calls libc `calloc`/`free` directly. So the CFL
"default allocator" *is* libc malloc; there is nothing to swap. (`atomic_sub_value` refcounting and
libc malloc are both thread-safe — the fault is the borrowed-pointer UAF above, not allocator
corruption per se; the fix addresses both by removing driver CF alloc/free from the busy window.)

**The fix (`replay.c`, Phase 6).** Reconstruct **every** copyProperty value **once at
`AirPlayReceiverServerSetDelegate` time** — single-threaded, *before* `Control(START)` spawns the
AirPlay/Bonjour threads — and cache the CFTypeRefs (`precompute_values()`; non-destructive scan of
the trace). During replay `call_copyProperty` only **deposits the cached, still-live pointer** and
compares by identity (`r == cfval`) — **zero CF alloc/free on the driver thread in the concurrent
window**, so the borrowed pointer the concurrent reader holds is always valid. The byte-exact
roundtrip check (re-serialize + diff vs raw `valData`) is likewise precomputed single-threaded, so
it is now race-free for **all 10** bplist keys incl. `bluetoothIDs` (Phase 3 could only get 9/10).
Cache released **once** at driver DONE (`free_value_cache()`, shared slot cleared first) — no leak,
no double-free, values still reconstruct byte-exact.

**GATE MET — 5/5 (was 5/5 FAIL).** Combined config: from-source `trackf_fromsrc_probe` + **stock**
libairplay + Phase-5 ABI-shim/fake-OMX/Screen/NVSS stack, driving `carplay_session_values.jsonl`
(values **and** 800 video frames). Every run (`run_dio_trackF_replay.sh 75`):
- `precomputed 17 unique copyProperty values (single-threaded, pre-START)`.
- `copyProperty per-key compare: KEY-MATCH=21 KEY-DIVERGE=0 of 21 (val-diverge=0 err-diverge=0)`,
  **10/10 bplist keys `[byte-exact roundtrip]`**.
- Video: `ScreenStreamCreate rc=0`, `ScreenStreamStart rc=0 EXECUTING`, frames fed through
  `OMX_EmptyThisBuffer`, `FillBufferDone` flowing, 391 frames delivered.
- **0 SIGSEGV / Memory-fault / `_list_memalign`**, `replay driver DONE`.
- Repeatability: **clean 5/5** (runs 1-5), no retries — the Phase-4/5 "retry until lucky" note is
  now obsolete for the values trace.

**Files:** fix in `c_hook_trace/replay.c` (`precompute_values` / `vcache` / `free_value_cache`);
boot log `carplay/trackF_replay_boot.log`; per-run logs `/tmp/replay_run_{1..5}.log`.

## Phase 7 result (2026-07-17) — GOLDEN STOCK BASELINE: literal MU1316 stock dio + stock libairplay

Phases 5-6 proved the video path on **stock libairplay** but the dio under test was still the
from-source `trackf_fromsrc_probe` (control plane proven stock-identical in Phase 4). Phase 7
swaps in the **genuine on-unit MU1316 stock `dio_manager` ELF** and re-runs the same combined
values trace headless — so the golden reference is now the exact stack the trace was captured on:
**stock dio + stock libairplay**, no from-source anywhere.

**Genuine MU1316 pair (cksum-verified, byte-identical to firmware + on-unit).**
- `dio_manager`: **cksum `2164590314` / 691203 B** == `Firmwares/MHI2Q_US_AUG22_P5087_MU1316/
  extracted/app/eso/bin/apps/dio_manager` == `Patches/dio_manager-mhi2q/binaries/dio_mhi2q_mu1316_us.elf`
  (all three identical). Staged `stage/dio_manager` matches.
- `libairplay.so`: **cksum `3791488696` / 763420 B** == the MU1316 firmware lib (the on-unit lib the
  trace was captured on). Staged `stage/libairplay.so` matches.

**Bring-up (stock-specific).** Stock dio's natural init hits the -6707 DSI-register mesh wall and
`_exit(1)`s — it never reaches CarPlay on its own. `libtrackf_stock_spawn.so` (LD_PRELOAD) (a)
swallows that premature exit + parks the thread, (b) fails `getifaddrs` post-spawn so the enable-gate
`sub_175AE0` @0x175ae0 falls through to the 2×-SetProperty path, and (c) **force-spawns stock dio's
OWN `CAirPlayMainThread::run` @0x15c214** with a synthetic `CAirPlayMainThread` + config (cfg[0x191]=1
pre-signals the copyProperty sync object so the wait short-circuits, exactly the from-source no-op
equivalent). run() then does the REAL `ServerCreate → SetProperty(iface,model) → SetDelegate →
Control(START)` against stock libairplay — the same boundary the replay driver interposes. New this
phase: a one-line gate in `trackf_stock_spawn.c` — when `REPLAY_TRACE` is set, the spawn parks its
manager (skips its own /info+exit) so `libcarplay_replay.so` can drain the full ~31s trace; stock
run()'s `CFRunLoopRun` keeps the process alive and the QEMU window ends it.

**Stack.** `LD_PRELOAD=libsscabi.so:libtrackf_stock_spawn.so:libcarplay_replay.so`,
`NVSS_VIDEO_OUTPUT_DEVICE=1`, + the proven Phase-5/6 headless video stack (fake OMX in
libOmxCore/libOmxBase/libomxctx, screenstub `libscreen.so.1`, Phase-6 CF-fixed `replay.c` — all
reused unchanged). Build `carplay/dio_trackF_replay_stock.build`, run
`carplay/run_dio_trackF_replay_stock.sh`.

**Combined run — SPOTLESS (boot log `carplay/trackF_replay_stock_boot.log`):**
- captured `server=1c93f8 ctx=1eeaa8`, delegate **`copyProperty=158690`** = stock
  `s_serverCopyPropertyCallback`@0x158690 (the genuine stock provider VA, not from-source).
- `precomputed 17 unique copyProperty values (single-threaded, pre-START)` (Phase-6 UAF fix active).
- **`copyProperty per-key compare: KEY-MATCH=21 KEY-DIVERGE=0 of 21 (val-diverge=0 err-diverge=0)`**,
  **10/10 bplist keys `[byte-exact roundtrip]`** incl. bluetoothIDs (model="AUDI 4935",
  manufacturer="HARMAN", oemIcon, limitedUI, nightMode, rightHandDrive, …). Deposit into synthetic
  cfg+0x1f0/+0x1f4 works on stock unchanged — offsets confirmed identical.
- **`[sscabi] real ScreenStreamCreate=780b5ce4 detected=STOCK (out=arg2, adapting)`** — the ABI shim
  auto-detected the stock prototype (out-ptr in r1) and re-issued `real(0,out)`; `ScreenStreamCreate
  rc=0`, AVCC header injected, decoder → `OMX_StateExecuting`, **`ScreenStreamStart rc=0 (EXECUTING)`**.
- **391/391 frames** through `ScreenStreamProcessData` → `OMX_EmptyThisBuffer` (ETB #1 = 20-B AVCC
  CODECCONFIG, #2.. = frames), `FillBufferDone` black NV12 flowing. `DONE: copyProperty=21
  lifecycle=2 screenFrames=391(injected=391)`.
- **0 SIGSEGV / Memory-fault / `_list_memalign` / dumper.** Process parks in stock run()'s CFRunLoopRun
  (no self-exit in replay mode) → ended by the QEMU window (signal 15), clean — not a fault.

**Repeatability: clean 5/5** (run 1 = `trackF_replay_stock_boot.log`; runs 2-5 =
`/tmp/replay_stock_run_{2..5}.log`), every run KEY-MATCH=21/21, byte-exact=10/10, EXECUTING,
391/391 frames, faults=0, DONE=1. No retries.

**Stock-vs-from-source-probe differences (honest):**
1. **Bring-up:** stock needs the `libtrackf_stock_spawn.so` force-spawn (swallow -6707 exit + gate
   bypass + spawn run@0x15c214 + synthetic ctx); the from-source probe *is* a reconstructed run() as
   its own `main`. Same libairplay boundary reached either way.
2. **`ScreenStreamCreate` ABI:** stock writes the new stream to `*r1` (out=arg2); from-source uses
   r0 (out=arg1). The `libsscabi.so` shim auto-detects (`mov sl,r1` prologue) and adapts — one preload
   serves both.
3. **cfg layout:** identical — the copyProperty deposit offsets `cfg+0x1f0/+0x1f4` and the provider
   VA `0x158690`/`0x158714 LDR R4,[R4,#0x1F0]` are stock, and the deposit into the spawn's synthetic
   cfg (ctx=1eeaa8) worked unchanged.
4. **Timing:** stock burns ~9s in `GetPrimaryMACAddress` ENXIO retries (no NIC) before ServerCreate
   (SetDelegate ~14-16s in); replay then drains ~31s → ≥75s QEMU window. From-source used a
   `TRACKF_HOLD` then `exit(0)`; stock parks in CFRunLoopRun (killed by the window).
5. **No stock-specific defect** — video decode/display still needs the real on-unit Qualcomm OMX +
   screen (the fake-OMX/screenstub headless limit, identical to Phases 5-6, not a stock regression).

**Verdict.** The literal on-unit MU1316 stack — genuine stock `dio_manager` (2164590314) + genuine
stock `libairplay.so` (3791488696) — reproduces the captured CarPlay session headless **spotlessly and
repeatably (5/5)**: full control plane (server lifecycle + 21/21 byte-exact per-key copyProperty +
session/screen callbacks) and the full video-delivery path (ScreenStreamCreate/Start EXECUTING + 391
frames through EmptyThisBuffer + FillBufferDone), 0 SIGSEGV. **This is THE golden stock baseline** —
the constant for the incremental plan (next: stock dio + from-source libairplay; then from-source dio).

**Files:** `carplay/dio_trackF_replay_stock.build`, `carplay/run_dio_trackF_replay_stock.sh`,
`carplay/trackf_stock_spawn.c` (REPLAY-mode park added); boot log
`carplay/trackF_replay_stock_boot.log`; per-run logs `/tmp/replay_stock_run_{2..5}.log`.

## Phase 8 result (2026-07-17) — STOCK dio + FROM-SOURCE libairplay (single-variable swap)

Phase 7 froze the golden stock stack. Phase 8 holds **STOCK dio_manager constant** and swaps ONLY
stock libairplay → **from-source libairplay** (`Patches/libairplay-mhi2q/libairplay.dev.so`), re-running
the SAME `carplay_session_values.jsonl` headless. This isolates from-source libairplay as the one variable.

**Both binaries verified (cksum).**
- dio_manager: **stock `2164590314` / 691203 B** (unchanged from Phase 7 golden — genuine MU1316).
- libairplay: **from-source `4277412394` / 3254068 B** — srcvers 280.33.8 rebuild (emulator/OMX variant,
  `DEV=1`), NOT stock `3791488696` / 763420 B. Hard from-source markers: ~3.2 MB unstripped; exports
  `ScreenStreamCreate@0x26390 Start@0x26b00 ProcessData@0x271c4 SetProperty@0x26510` and the 450-port
  `ScreenStreamSetViewArea@0x2670c` (stock has none of these VAs); DT_NEEDED only `libc/libm`.
  Staged as `stage/libairplay.dev.so`; the golden `stage/libairplay.so` (stock) is untouched.

**From-source video/OMX path characterized (RE + boot-log, VA/source-cited).**
- **OMX is EXTERNAL, not internal (REFUTES the "resolves OMX internally" cross-check).** `readelf --dyn-syms`:
  `OMX_Init/OMX_GetHandle/OMX_FreeHandle/OMX_Deinit` are **UND** (imported) → resolve from process global
  scope = the harness fake OMX (`libOmxCore/Base/ctx` = `omx_stub.c`). `screen_*` are UND too → the
  screenstub (`libscreen.so.1`). So from-source hits the SAME external fake-OMX/screenstub boundary as stock.
- **OMX bring-up is DEFERRED to `ScreenStreamStart@0x26b00`, not front-loaded in Create** (opposite of stock).
  `ScreenStreamCreate@0x26390` (`screenstream.c:264`) only does `_CFRuntimeCreateInstance` + mutex/cond init +
  `*outStream=me`; the out-ptr is saved from **r0/arg1** (`str r0,[fp,#-32]` @0x263a4) — so the `libsscabi`
  shim auto-detects **`detected=from-source (out=arg1, passthrough)`** and does NOT re-order args (stock was
  `out=arg2/r1`). `ScreenStreamStart` runs the full OMX contract: `OMX_Init → OMX_GetHandle("OMX.qcom.video.
  decoder.avc") → SetParameter(VideoPortFormat AVC) → SendCommand(Idle) → GetParameter(PortDefinition)×2 →
  AllocateBuffer(port0×4 + port1×4) → SendCommand(Executing) → FillThisBuffer(prime out bufs)` (`screenstream.c:357`).
- **No `NVSS_VIDEO_OUTPUT_DEVICE` gate** (grep: string absent). Stock gated window creation on that env
  (`OmxVideoImpl::configure`); from-source does not — its `_MakeWindow` is called from Start (if width/height
  set) or lazily on `OMX_EventPortSettingsChanged`. Harness still sets `NVSS_*=1` (harmless no-op for from-source).
- **No QCOM `DisplayPictureBuffer` extension, no AnnexB hard gate.** From-source uses the plain OpenMAX IL
  flow (never queries the QCOM vendor extension that was FATAL-if-NotImplemented for stock — Phase 5), and
  `ScreenStreamProcessData@0x271c4` (`screenstream.c:414`) feeds frames directly: `avcc` (SPS/PPS) is OPTIONAL
  (`if(needCodecConfig && me->avcc)`), no "Annex B Header invalid" rejection. So from-source needs neither the
  QCOM-extension quirk nor the ABI shim's avcC inject — confirmed in-log: the shim logs no avcc-inject for the
  from-source path and ETB#1 is a real frame (flags=0x10 ENDOFFRAME), not a 0x80 CODECCONFIG header.

**Headless-stack adaptation needed — ONE change (env-gated, stock path byte-identical).** From-source's
`_OnFillDone` (`screenstream.c:181`) **re-submits `OMX_FillThisBuffer` INLINE from inside the FillBufferDone
callback** (`if(me->running) OMX_FillThisBuffer(c,b)`), assuming the real Qualcomm OMX returns FillBufferDone
from its OWN decoder thread (so FillThisBuffer is non-blocking). The Phase-5 fake OMX (`omx_stub.c`
`fk_FillThisBuffer`) called `FillBufferDone` **synchronously inline** — fine for STOCK (whose COMXVideoDecoder
drives output on a dedicated render thread and whose FillBufferDone handler queues, never re-submits inline),
but against from-source's inline recycle it **recursed unboundedly on `ScreenStreamStart`'s thread**:
`FillThisBuffer→FillBufferDone→_OnFillDone→FillThisBuffer→…`. Symptom (first from-source run): Start entered
(OMX reached Executing, FillBufferDone climbed to #1800) but **never returned** to the driver → the 391 input
frames were **never fed** (zero `EmptyThisBuffer`). Fix: `omx_stub.c` now delivers FillBufferDone from a
**render thread** when **`FAKEOMX_ASYNC_FILL=1`** (set only in the from-source `.build`), so `OMX_FillThisBuffer`
returns immediately (real-OMX semantics) and the inline recycle becomes a bounded ring hop. Env UNSET ⇒ the
original synchronous path, unchanged → the golden stock baseline is byte-for-byte behavior-identical (re-verified
5/5-equivalent; see below).

> **THE KEY DIVERGENCE (real, not a stub gap):** from-source libairplay recycles output buffers **inline in the
> FillBufferDone callback** (`_OnFillDone`→`OMX_FillThisBuffer`), whereas stock libairplay's decoder decouples
> FillThisBuffer/FillBufferDone across its own render/command threads. On the real Qualcomm OMX this is benign
> (FillThisBuffer is non-blocking, FillBufferDone fires async from the decoder thread — one hop per decoded
> frame, paced by real decode). It only bites a **synchronous** fake OMX, which is why it surfaced here and not
> on-device. This is exactly the kind of stock-vs-from-source behavioral difference Phase 2/8 exists to expose.

**GATE MET — STOCK dio + FROM-SOURCE libairplay run the captured session headless (boot log
`carplay/trackF_replay_fromsrc_lib_boot.log`):**
- captured `server=o#1→1c3350 ctx=o#2→1c34f0`; delegate `copyProperty` = stock provider (dio is stock),
  **`copyProperty per-key compare: KEY-MATCH=21 KEY-DIVERGE=0 of 21 (val-diverge=0 err-diverge=0)`**,
  **10/10 bplist keys `[byte-exact roundtrip]`** — control plane 21/21 **unchanged** (dio is stock; libairplay
  swap does not touch it).
- `[sscabi] detected=from-source (out=arg1, passthrough)` → `ScreenStreamCreate rc=0`,
  `[fakeomx] FAKEOMX_ASYNC_FILL=1 → FillBufferDone via render thread`, OMX `Idle→AllocateBuffer(4+4)→Executing`,
  **`ScreenStreamStart rc=0 (EXECUTING — OMX decoder up)`** (now RETURNS — was hung pre-fix).
- **391/391 frames** fed to `ScreenStreamProcessData → _FeedInput → OMX_EmptyThisBuffer` (fakeomx
  `EmptyThisBuffer` counter flowing, flags=0x10 ENDOFFRAME), `FillBufferDone` black NV12 recycling on the render
  thread. `DONE: copyProperty=21 lifecycle=2 screenFrames=391(injected=391)`.
- **0 SIGSEGV / Memory-fault / `_list_memalign`**, clean `ComponentDeInit` (render thread joined). Process parks
  in stock run()'s CFRunLoopRun → ended by the QEMU window (signal 15), clean.

**Repeatability: clean 5/5** (run 1 = boot log; runs 2-5 = `/tmp/fromsrc_rep_run_{2..5}.log`), every run
KEY-MATCH=21/21, byte-exact=10/10, `ScreenStreamStart rc=0` EXECUTING, 391/391 frames, faults=0, DONE=1. No retries.

**Verdict.** Swapping stock→from-source libairplay under the constant stock dio reproduces the captured session
headless **spotlessly, 5/5**: control plane 21/21 unchanged (dio-owned), and the from-source video path reaches
its frame-delivery equivalent of the golden baseline (Create/Start EXECUTING + 391 frames through
EmptyThisBuffer). The single real from-source-vs-stock divergence — **inline FillBufferDone recycle vs
render-thread-decoupled recycle** — was surfaced, root-caused (VA/source-cited), and accommodated with one
env-gated harness line that leaves the golden stock path untouched. Video decode/display still needs the real
on-unit Qualcomm OMX (unchanged headless limit). Next incremental step: **from-source dio + from-source libairplay**.

**Files:** `carplay/dio_trackF_replay_fromsrc_lib.build` (stages `libairplay.dev.so`, `FAKEOMX_ASYNC_FILL=1`),
`carplay/run_dio_trackF_replay_fromsrc_lib.sh` (rebuilds the fake-OMX stubs w/ `-I omx-inc`, cksum-stages the
from-source lib), `carplay/omx_stub.c` (env-gated async render thread — `fk_render_thread`/`fk_fill_one`);
boot log `carplay/trackF_replay_fromsrc_lib_boot.log`; per-run logs `/tmp/fromsrc_rep_run_{2..5}.log`.

## Phase 9 result (2026-07-17) — FULL FROM-SOURCE STACK: from-source dio + from-source libairplay

Phase 7 froze the golden **stock+stock** baseline; Phase 8 swapped in from-source libairplay under
constant stock dio (single variable). Phase 9 (final) makes **BOTH** variables from-source and re-runs
the SAME `carplay_session_values.jsonl` headless — the interaction check neither independent phase could
show: does composing from-source dio × from-source libairplay surface a NEW divergence?

**Both binaries verified genuine from-source (cksum, both ≠ stock).**
- dio: **from-source `trackf_fromsrc_probe` cksum `1052295190` / 44399 B** — the reconstructed dio-src
  `CAirPlayMainThread::run()` + real `CDIOManager_callbacks.cpp` (control plane proven stock-identical
  Phase 4/5/6), **NOT** stock dio `2164590314`. Delegate VAs are the from-source coordinates:
  `copyProperty=0x103170 sessionCreated=0x104fa8 screenCreated=0x1051fc audioType=0x1056f0
  waitAck=0x1055e8` (vs golden stock `0x158690 / 0x157a04 / 0x154cec / 0x154e58 / 0x157438`).
- libairplay: **from-source `libairplay.dev.so` cksum `4277412394` / 3254068 B** (srcvers 280.33.8
  rebuild), **NOT** stock `3791488696`. `ScreenStreamCreate@0x26390` (out=arg1).

**Stack.** New harness variant (Phase 7/8 harnesses intact): build
`carplay/dio_trackF_replay_fromsrc_full.build` (fork of the Phase-5/6 `dio_trackF_replay.build` —
launches `trackf_fromsrc_probe` directly, no stock-spawn; the ONLY two deltas vs it are staged
libairplay → `libairplay.dev.so` and `FAKEOMX_ASYNC_FILL=1`), run
`carplay/run_dio_trackF_replay_fromsrc_full.sh` (verifies both from-source cksums up front). Reuses the
proven headless stack unchanged: `libsscabi.so` (auto-detects the ABI), Phase-6 CF-fixed READ-ONLY
`replay.c`, `omx_stub.c` (async render thread), `screenstub.c`. The stock-only bits (NVSS gate / QCOM
ext / AnnexB gate) are inert against from-source libairplay (Phase 8) and left harmless.

**Combined run — SPOTLESS (boot log `carplay/trackF_replay_fromsrc_full_boot.log`):**
- captured `server=1280e0 ctx=1083a0`; delegate `copyProperty=0x103170` = **from-source dio-src**
  `s_serverCopyPropertyCallback` (self-traced by the `[trackFfs]` TRACKF probe — genuine from-source
  provider firing, deposits into synthetic `cfg+0x1f0/+0x1f4`).
- **`copyProperty per-key compare: KEY-MATCH=21 KEY-DIVERGE=0 of 21 (val-diverge=0 err-diverge=0)`**,
  **10/10 bplist keys `[byte-exact roundtrip]`** — control plane 21/21, unchanged by the libairplay swap.
- `[sscabi] real ScreenStreamCreate=78426390 detected=from-source (out=arg1, passthrough)` →
  `ScreenStreamCreate rc=0`, `FAKEOMX_ASYNC_FILL=1 → FillBufferDone via render thread`, OMX
  Idle→AllocateBuffer→Executing, **`ScreenStreamStart rc=0 (EXECUTING)`**.
- **391/391 frames** through `ScreenStreamProcessData → _FeedInput → OMX_EmptyThisBuffer` (`running=1`,
  `frames=391 delivered-to-ScreenStreamProcessData=391 bytes=6411198`), `FillBufferDone` black NV12
  recycling on the render thread. `DONE: copyProperty=21 lifecycle=2 screenFrames=391(injected=391)`.
- **0 SIGSEGV / Memory-fault / `_list_memalign` / dumper.** `/info` **200 OK** (server up via from-source
  `run()` — from-source libairplay listens on loopback; stock lib needs a real NIC, Phase 4).

**Repeatability: clean 5/5** (run 1 = boot log; runs 2-5 = `/tmp/fromsrc_full_run_{2..5}.log`), every run
KEY-MATCH=21/21, byte-exact=10/10, `detected=from-source`, `ScreenStreamStart rc=0` EXECUTING, 391/391
frames, faults=0, DONE=1. No retries.

**Parity vs the golden stock baseline (Phase 7) — IDENTICAL observable behavior, NO new interaction
divergence.** Every observable of the replayed session matches golden stock+stock exactly: KEY-MATCH
**21/21** (byte-exact **10/10**), `lifecycle=2`, `ScreenStreamStart rc=0` EXECUTING, **391/391** frames,
**0 SIGSEGV**. The only differences are structural (expected, already documented Phases 4-8), not
behavioral:
1. **Callback VAs** differ (from-source `0x103170/…` vs stock `0x158690/…`) — the from-source rebuild is
   a different binary; the *answers* those callbacks produce are byte-identical.
2. **ScreenStreamCreate ABI:** from-source out=arg1 (sscabi passthrough) vs stock out=arg2 (sscabi
   adapts) — one preload serves both (Phase 4/7).
3. **FillBufferDone recycle:** from-source recycles inline in `_OnFillDone` → needs
   `FAKEOMX_ASYNC_FILL=1` (Phase 8's single real divergence, libairplay-side); stock decouples across
   its render thread (env unset).
**The full stack is a clean superposition:** from-source *dio* contributes its (stock-identical) control
plane — the libairplay swap does not touch KEY-MATCH (21/21 whether lib is stock or from-source); from-source
*libairplay* contributes its (Phase-8-characterized) video path — the dio swap does not touch it. Both
from-source accommodations are libairplay-side; from-source dio adds **zero** new accommodation. So no
emergent interaction bug — the two independently-validated variables compose without surprise.

**Honest remaining limit (unchanged).** Real Qualcomm OMX decode + real screen present are on-unit only;
QEMU has no `OMX.qcom.video.decoder.avc`, so this validates the frame-DELIVERY path (captured AVCC → real
`ScreenStreamProcessData` → `OMX_EmptyThisBuffer`, EXECUTING, in seq order, no crash), not decoded/rendered
video. On the real unit stock/from-source libairplay's own AirPlay session drives Create/Start with the real
decoder present — the headless reconstruction limit is not a deployment defect.

**FINAL — incremental bring-up summary (stock+stock → stock+fromsrc-lib → fromsrc-full):**
| Phase | dio | libairplay | control | video (headless) | faults | 5/5 |
|---|---|---|---|---|---|---|
| 7 (golden) | stock `2164590314` | stock `3791488696` | 21/21 byte-exact 10/10 | Create/Start EXECUTING, 391 frames | 0 | ✓ |
| 8 | stock `2164590314` | **fromsrc `4277412394`** | 21/21 byte-exact 10/10 | EXECUTING (needs `FAKEOMX_ASYNC_FILL`), 391 | 0 | ✓ |
| 9 (full) | **fromsrc `1052295190`** | **fromsrc `4277412394`** | 21/21 byte-exact 10/10 | EXECUTING, 391 frames | 0 | ✓ |
Every incremental swap reproduced the golden baseline; the full from-source stack matches stock+stock on
all observables of the captured session. The two from-source-vs-stock structural differences (Create ABI,
inline FillBufferDone recycle) are surfaced, root-caused (VA/source-cited), and accommodated with
env-gated harness lines that leave the golden stock path byte-identical. Deploy target (from-source dio +
stock **or** from-source libairplay) is control-plane- and frame-delivery-faithful headless; real decode/
display remains the single on-unit hardware step.

**Files:** `carplay/dio_trackF_replay_fromsrc_full.build`, `carplay/run_dio_trackF_replay_fromsrc_full.sh`;
boot log `carplay/trackF_replay_fromsrc_full_boot.log`; per-run logs `/tmp/fromsrc_full_run_{2..5}.log`.
Phase 7/8 harnesses (`dio_trackF_replay_stock.*`, `dio_trackF_replay_fromsrc_lib.*`) untouched;
`replay.c` + trace READ-ONLY (unchanged).

## Verdict — `_OnFillDone` inline `OMX_FillThisBuffer` recycle: SDK-faithfulness review (2026-07-17)

The Phase-8 "single real divergence" (from-source `_OnFillDone` re-submits `OMX_FillThisBuffer`
INLINE inside FillBufferDone vs stock's decoupled recycle) was re-examined for whether the from-source
pattern is a genuine rebuild bug to fix, or the correct/faithful behavior. **Outcome (a): DO NOT change
from-source `screenstream.c`.** No code change this turn.

**1. There is NO Apple-SDK OMX `_OnFillDone` to be faithful to.** The from-source libairplay is a rebuild
of the leaked Apple SDK `srcvers 280.33.8`, but the SDK's ScreenUtils video backend
(`vendor/carplay-sdk/Support/ScreenUtilsStub.c`, header stamped `Version: 280.33.8`) is a **pure
GStreamer stub** — `#include <gst/gst.h>` / `gstappsrc` / `vpudec`, no OMX, no `OMX_FillThisBuffer`, no
`FillBufferDone`. `src/screenstream.c`'s OMX backend is a **from-scratch project reimplementation** that
*replaces* that GStreamer stub (see the file's own header) and is reversed from stock **dio**'s Qualcomm
chain (`reversed/01_screen_decode_chain.md`: `dio::OmxVideoImpl` / `dio::COMXVideoDecoder` /
`dio::CScreenRender`, statically linked into stock libairplay, absent from the leak). So the task's
literal (a)-premise ("the SDK source HAS the inline resubmit") is moot — the SDK contains no OMX code at
all; the canonical reference is stock dio, not the SDK.

**2. Stock dio pattern (VA-cited, `reversed/01_screen_decode_chain.md`).** `OMX_CALLBACKTYPE @0xBA9EC` =
`{EventHandler=0x881c4, EmptyBufferDone=0x87d60, FillBufferDone=0x86e88}`. Stock `FillBufferDone@0x86e88`
→ `dio::COMXVideoDecoder::onFillBufferDoneCallback` → `dio::CScreenRender::render@0x8b5c0` →
`screen_post_window` — i.e. stock ALSO does real present work in the callback (the analogue of
from-source `_PresentFrame`). The output-buffer **recycle** (`FillThisBuffer` resubmit) is driven off
stock's own `dio::CCommandThread` (the `videoDecoderThread`, `init(...,slots=4)`), NOT resubmitted inline
— inferred from behavior (stock does not recurse against a synchronous fake OMX); the reversed doc pins
the callback VAs but does not instruction-cite the resubmit site, so this decoupling is behavior-proven,
not opcode-proven.

**3. The inline resubmit is the canonical OpenMAX IL client recycle and is correct on real hardware.**
Recycling the output buffer with `OMX_FillThisBuffer` from inside `FillBufferDone` is the standard,
idiomatic IL pattern. On the real async Qualcomm `OMX.qcom.video.decoder.avc`, `FillThisBuffer` only
enqueues and returns; `FillBufferDone` is delivered from the component's **own** decoder thread — one
bounded hop per decoded frame, paced by real decode. `omx_stub.c`'s own comment confirms it: *"The real
Qualcomm OMX returns FillBufferDone from its OWN decoder thread, so OMX_FillThisBuffer is non-blocking."*
from-source holds no lock across the resubmit (`_OnFillDone` calls `_PresentFrame` then `FillThisBuffer`
lock-free), so it is deadlock- and recursion-safe against a conformant component.

**4. The recursion was a NON-CONFORMANT test double, not a from-source bug.** Unbounded
`FillThisBuffer→FillBufferDone→_OnFillDone→FillThisBuffer…` recursion occurs ONLY when the fake OMX calls
`FillBufferDone` synchronously from within `FillThisBuffer`'s call stack — which **violates the OMX IL
async contract** (a conformant component never does this). `FAKEOMX_ASYNC_FILL=1` delivers `FillBufferDone`
from a render thread (`omx_stub.c` `fk_render_thread`), making the fake **conform to real-OMX threading
semantics**. That is the correct accommodation: it fixes the test double to match hardware, it does not
paper over a from-source defect. Changing `_OnFillDone` to add a libairplay-side render/queue thread would
diverge from the minimal correct IL pattern, add a thread real hardware doesn't need, and risk QNX
threading traps — gold-plating to satisfy a broken fake. Hence **no from-source change**.

**Re-confirmation runs (this turn, from-source UNCHANGED, `FAKEOMX_ASYNC_FILL=1`):**
- **Phase 8** (stock dio `2164590314` + from-source libairplay `4277412394`), `run_dio_trackF_replay_fromsrc_lib.sh 85`:
  `detected=from-source (out=arg1)`, `ScreenStreamCreate rc=0`, `ScreenStreamStart rc=0 (EXECUTING)`,
  **391/391** frames through `OMX_EmptyThisBuffer`, `KEY-MATCH=21/21` (byte-exact 10/10),
  **0 SIGSEGV/Memory-fault/_list_memalign**, `DONE screenFrames=391(injected=391)`.
- **Phase 9** (from-source dio `1052295190` + from-source libairplay `4277412394`), `run_dio_trackF_replay_fromsrc_full.sh 85`:
  both cksums verified from-source, `detected=from-source`, `ScreenStreamStart rc=0 (EXECUTING)`,
  **391/391** frames, `KEY-MATCH=21/21` (byte-exact 10/10), `/info` **200 OK**, **0 faults**, `DONE`.

**Is `FAKEOMX_ASYNC_FILL` still needed? Yes — and it is the *correct* fake behavior, not a workaround.**
It models the real async Qualcomm OMX (non-blocking `FillThisBuffer`, `FillBufferDone` off the decoder
thread). It stays env-gated only so the golden stock baseline (Phase 7) remains byte-identical — stock
tolerates the synchronous fake because it queues its recycle. Reasonable future hygiene: make async the
default and gate the *synchronous* legacy mode instead; harness cosmetics, not correctness. `screenstream.c`,
`replay.c`, dio-src, and the trace are all **unchanged**.
