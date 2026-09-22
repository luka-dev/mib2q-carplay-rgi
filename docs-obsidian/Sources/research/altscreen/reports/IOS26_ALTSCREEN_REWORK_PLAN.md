# AltScreen (iOS 26.1) — hook rework plan for MHI2Q stock libairplay 210

**Date:** 2026-08-01
**Target:** MHI2Q head unit, **STOCK `libairplay.so` srcvers 210.81** (QNX/ARMv7),
CarPlay via LD_PRELOAD hook into `dio_manager`. We add altScreen by **interposing
stock 210 exports** — we do NOT ship a from-source libairplay in prod.
**Status:** analysis complete, rework NOT started. RGI + coverart are independent
(BAP + TCP bus) and unaffected.

---

## 0. TL;DR — the core finding

> **CORRECTION (2026-08-01, verified against MORE iOS 26.1 code — supersedes the pure-URL
> reading below).** altScreen on iOS 26.1 is **TWO things, not one**:
> 1. **`altScreenURLs` (control channel) = a HANDSHAKE / metadata** — which app's cluster
>    *scene* provides the content (Apple Maps etc.). The HU must **store + ACK** it. It is
>    **NOT an HTTP video URL** the HU downloads — opening it with an HTTP GET is almost
>    certainly the wrong path.
> 2. **The pixels ride a type-111 AirPlay screen stream (the SETUP flow), same as before.**
>    Verified: `AirPlaySender/AirPlaySender_01.mm:2453` `apsession_setupStreamCreatingResponseInternal`
>    whitelists stream types **{111, 120, 130}** (mask `0x80201` over base 111) as valid
>    "special" setup streams; the phone renders the cluster as a scene
>    (`CARTemplateUIInstrumentClusterSceneViewController` → `setInstrumentClusterScene:`) →
>    encodes → streams it. So **our KEPT push-111 path (`alt_find_111`+`alt_setup`+the
>    `ScreenStreamProcessData` tap → altscreen_render) is LIKELY the real video transport.**
>
> **Codec (verified offline):** `Activate.mm:147` gates HEVC behind
> `enableCarPlayEncodingStreamTypeHEVC` → **H.264/AVC by default, HEVC only if negotiated.**
> Our altscreen_render OMX is AVC → matches the default. Risk: a HEVC-forced cluster would
> need a HEVC decoder.
>
> **Decoder:** always **altscreen_render (OMX)** — libairplay/hook only transport NALs.
>
> **2×OMX concurrency (RESOLVED, verified in firmware):** altScreen runs 2 concurrent
> `OMX.qcom.video.decoder.avc` (dio main-110 + altscreen_render cluster-111). Firmware
> `libmmkiVdec_asic_AF.so` `sub_99F8` (`.../decoder/mmki/src/MMKIVdecIo.cpp:153`, APQ8064)
> caps at **4 concurrent decode clients** (`gMMKIVdecInstances<=3` → create; 5th → "too many
> clients opened" → NULL). Our 2 ≤ 4 → SAFE; throughput (vidcfw.bin) also fine for 2 sub-1080p
> streams. Checked offline via `Tools/mhi2q-qemu/out/unpacked/.../lib/` — SDP base irrelevant.
>
> **⇒ Revised plan:** the SetProperty `altScreenURLs` **ACK** (Step 1, done) is the handshake
> that unlocks the **111 stream SETUP** (push path, already coded + kept). The HTTP
> `url_open_thread` (Step 2) is probably NOT how pixels arrive — keep it only as a
> format-probe. **On-unit dump decides**: with `ALTSCREEN_HANDLE_ALT`, does the log show
> `111 stream offered — bringing up altScreen` (push fired) or only `altScreenURLs`?

**Original (pure-URL) reading — kept for history, now known incomplete:** our hook implemented
the OLD push-video model; a first pass of only the *activation* path (Activate.mm) showed
`altScreenURLs` sent over control with no stream construction there, and concluded "iOS 26.1
doesn't push 111". That missed the *separate* stream-SETUP path (§0 correction) which DOES
handle 111. The `-6740` teardown (looks like a dio restart-loop; NOT a crash) still stands as
what happens when altScreen is advertised but left unhandled.

---

## 1. Verified protocol (iOS 26.1 / build 23B85)

```
1. HU GET /info → displays=[ main, alt{Name:"…AltScreen", Type:AltScreen(111), showsInstruments} ]
2. iOS parses displays → hasGaugeClusterScreen = TRUE   (mandatory; extendedFeatures alone is NOT enough)
3. iOS sets altScreen activation bit
4. iOS: CRFetchInstrumentClusterURLs → CFArray of per-app cluster-scene URLs
        (Apple Maps / Music have cluster scenes). EMPTY → error -16721 (needs a nav app active).
5. iOS: SetProperty("altScreenURLs", [URLs])  ← over the AirPlay HTTP/RTSP CONTROL channel
        (NOT in SessionSetup.streams — that's the key difference from push-video)
6. HU must: store the URLs + ACK within ~250 ms, else iOS → -6740 teardown
7. HU must: OPEN one URL (HTTP+H.264 to a phone-hosted endpoint) → decode → cluster display
            + report "scene active" via iAP2, else iOS gives up too.
```

### iOS-side code references (decompiled 26.1)
Base: `/Users/luka/Desktop/AUDI_2/Refferences/iPhone18-3_26.1_23B85_Restore`
- `…/CarKit.framework/CarKit/CARSessionConfiguration.mm:915` parses `@"displays"`; `:956`
  `_hasGaugeClusterScreen = [displays bs_containsObjectPassingTest:&__block_literal_global_620]`
- `…/CarKit.framework/CarKit/CARDisplayInfo.mm:200,409,433,640` — a display is a cluster iff
  name `"Instrument Cluster"` + `showsInstruments == YES` (that's what block_620 tests).
- `…/AirPlaySender.framework/AirPlaySender/Activate.mm:627` `APCarPlay_CRFetchInstrumentClusterURLs`;
  `:649` empty→`-16721`; `:660` `CFDictionarySetValue(theDict,@"altScreenURLs",…)`. **No stream SETUP here.**
- `…/carkitd/CRCarKitServiceAgent.mm:1274` `fetchInstrumentClusterURLsWithReply` (this agent stub
  returns empty; real URLs come from app cluster scenes).

---

## 2. Reference materials (all paths absolute)

| What | Path | Use |
|---|---|---|
| **iOS 26.1 decompile** (sender/CarKit ground truth) | `Refferences/iPhone18-3_26.1_23B85_Restore` | verify what iOS actually does |
| **Public CarPlay SDK 280** (base AirPlay mechanics; **NO altScreen**) | `Refferences/Carplay/` | RTSP/SETUP/screen-stream base — how 210 is structured |
| **Leaked SDK 280.33.8** (HAS altScreen, **push-video** model) | `Patches/libairplay-mhi2q/vendor/carplay-sdk/Sources/` | shows OLD altScreen impl (`AirPlayReceiverSessionScreen.c:370` SetAltScreen, `:734` stream tag; `AirPlayReceiverPOSIX.c:1391` alt display dict) |
| **from-source libairplay-mhi2q** (our build, push-video, unproven) | `Patches/libairplay-mhi2q/` (`src/screenstream.c`) | shows our current 111-decode path |
| **MHI3 altScreen RE** (aarch64 Linux, libairplay 450.14.2) | `Patches/mhi3-altscreen/findings/` | `MASTER_KNOWLEDGE.md`, `firmware_ceiling.md`, `ios_altscreen_protocol_decoded.md`, `altscreen_true_fails.md` — **treat as hypotheses to verify** |
| MHI3 libairplay binary (+IDA) | `Patches/mhi3-altscreen/work/libairplay.device.so`(`.i64`) | newer libairplay w/ altScreen remnants |
| Stock 210 libairplay (+IDA) | `Patches/carplay_vc/libairplay.so`(`.orig`,`.i64`) | **the actual target** — find SetProperty/getter addrs here |
| Our hook (altScreen) | `Patches/mhi2-carplay/hook/altscreen/altscreen_hook.c`, `altscreen_av.c` | code to rework |

### Key libairplay symbols (MHI3 450.14.2 addrs — **must re-find for MHI2 210** in `carplay_vc/libairplay.so.i64`)
- `AirPlayReceiverSessionCopyAltScreenURLs` @ 0x58D00 — **getter** (present in stock, exposes stored URLs to platform)
- `AirPlayReceiverSessionSetProperty` @ 0x59150 — control-channel property dispatcher (where `"altScreenURLs"` would arrive/be stored)
- Negative evidence (MHI3): `"altScreenURLs"`/`"uiContextURLs"` CFString constants have **ZERO xrefs** → the SETTER/handler was gutted → we must supply it.

---

## 3. What the hook does now (`altscreen_hook.c`) vs what's needed

| current (push-video, wrong for 26.1) | needed (URL/scene) |
|---|---|
| `AirPlayCopyServerInfo` (293) advertises extendedFeatures + displays | **keep** — this is what triggers iOS to send URLs |
| `AirPlayReceiverSessionSetup` (772) → `alt_find_111` (749) finds a **pushed** type-111 stream in `request.streams` → `alt_setup` (663) | **remove** — iOS 26.1 never puts a 111 stream in SETUP |
| `ScreenStreamProcessData` (562) taps 111 NALs → `altav_forward` → altscreen_render | keep as an optional sink; real input will come from the URL-opener |
| `AirPlayReceiverSessionTearDown` (794) cleanup | keep |
| **no SetProperty interpose** | **ADD** interpose `AirPlayReceiverSessionSetProperty`: catch `"altScreenURLs"` → store + ACK |
| **no URL opener** | **ADD** HTTP+H.264 client: open one URL → decode → `altav_forward` → altscreen_render |
| **no iAP2 scene-active report** | **ADD** if iOS needs it after opening |

Frame forwarding to altscreen_render (`altscreen_av.c` `altav_forward_frame` :81 / `_avcc_frame` :117,
→ altscreen_render `:19831`) is **reusable as-is** — altscreen_render (OMX sink) is NOT the problem.

---

## 3b. Codex review corrections (2026-08-01) — what is FACT vs HYPOTHESIS

A skeptical second-pass review against the ground-truth code. Keep these honest:

**Verified (fact):**
- iOS 26.1 altScreen activation is **URL-list based**. `Activate.mm:314` sets the alt-screen
  flag; `:615` fetches `CRFetchInstrumentClusterURLs` and inserts `@"altScreenURLs"`; `:738`
  passes the **whole activation dictionary** to an opaque vtable method and checks its status.
  **No `type==111` stream construction / SETUP anywhere in this path.** → our
  `alt_find_111`-on-SessionSetup hook is genuinely ineffective for this activation.
- `-16721` fires locally in iOS when the URL fetch fails / returns no array (`:626`) — before
  anything is sent to the HU. (Not a "count==0" check; null/err from the fetch helper.)

**NOT established (downgrade to hypothesis — do NOT code as if true):**
- **The interception seam.** `Activate.mm` sends a *whole activation dictionary* through an
  UNNAMED vtable method — it does NOT show a discrete `SetProperty(session,"altScreenURLs",arr)`.
  So it is UNPROVEN that: (a) stock 210 exports an interposable `AirPlayReceiverSessionSetProperty`
  with the assumed ABI; (b) the activation request even crosses that exported symbol/PLT vs an
  internal parser or platform callback; (c) it receives key/value separately vs the whole dict;
  (d) returning 0 produces the wire response iOS waits for; (e) MHI3's `platformData+0x128` slot
  applies to MHI2 210; (f) retaining the array hook-side == what stock/its getter expects.
- **"URL = HTTP+H.264"** — the element format is UNKNOWN (must dump on-unit).
- **"HU must OPEN a URL" vs "store+ACK is enough"** — UNRESOLVED. `:738` proves iOS waits for a
  successful activation-request status; it does NOT prove the HU must persist the array or open a
  URL. MHI3 notes contradict each other (`ios_altscreen_protocol_decoded.md:38` ~250ms-ACK vs
  `firmware_ceiling.md:45,65` must-open); no 250ms constant is in the reviewed code.
- **"-6740 = teardown, not crash"** — HYPOTHESIS. Our own hook comment (`altscreen_hook.c:301`)
  calls the observed loop a crash. Reconcile with evidence, don't assume.

**⇒ Prerequisite before any code: prove the stock-210 INGRESS PATH.** On a real activation that
carries `altScreenURLs`, find WHICH interposable function is actually called, its exact
argument/ownership ABI, and which return value becomes the control-channel wire response. Source
of truth = `Patches/carplay_vc/libairplay.so.i64` + a live on-unit trace, NOT the MHI3 addresses.

---

## 4. Incremental rework plan (each step testable on-unit)

**Step 0 — ✅ RESOLVED (2026-08-01, IDA on `Patches/carplay_vc/libairplay.so`).** The ingress
seam + ABI are PROVEN by static RE (all four codex doubts closed). Facts:

- **Seam = `AirPlayReceiverSessionSetProperty` @ `0x28e20`** — an exported, interposable symbol.
  ABI (ARM AAPCS): `int AirPlayReceiverSessionSetProperty(session, /*a2=*/1, key/*CFStringRef, r2*/,
  qualifier/*a4, r3*/, value/*CFTypeRef, stack a5*/)`. Returns int: **0 ⇒ HTTP 200 (ACK); nonzero ⇒ HTTP 422.**
  (Note arg order in the decompiler view: key=a3, qualifier=a4, value=a5.)
- **Stock 210 has NO altScreen anywhere** (`*AltScreen*`/`*ClusterURL*` = 0 funcs; older than MHI3
  450.14.2 which had the getter). `AirPlayReceiverSessionSetProperty` handles exactly ONE built-in
  key (`unk_9EB54`, int16 range-checked → `dword_BABC0`); **every other key — including
  `altScreenURLs` — falls through to `AirPlayReceiverSessionPlatformSetProperty` (dio_manager).**
  So the whole altScreen handler is ours to add; nothing to reuse.
- **Ingress proven end-to-end:** iOS sends an RTSP/HTTP **`SET_PARAMETER`** with a binary-plist body
  `{type:<propName>, value:<val>, qualifier:<q>}` → `AirTunesServerHTTP_ProcessRequest` @ `0x302e0`
  (`_requestProcessSetProperty` branch @ `0x30cd8`) parses the plist, extracts key/value/qualifier,
  and calls `AirPlayReceiverSessionSetProperty(session, 1, key, val, q)` **via the PLT thunk
  `j_…SetProperty` @ `0x194e0` (loads from GOT `…_ptr` @ `0xb9174`) ⇒ LD_PRELOAD interposable.**
  libairplay does the plist parsing; our hook receives key/value **already separated** — no plist
  parsing in the hook.

**⇒ Hook seam (Step 1) is now concrete:** interpose exported `AirPlayReceiverSessionSetProperty`;
`if (CFEqual(key, CFSTR("altScreenURLs"))) { store value (CFArray, CFRetain it); return 0; }`
else tail-call the real one. `return 0` is the HTTP-200 ACK on the wire. Codex doubts (a)(b)(c)(d)
all closed. What remains UNPROVEN is purely dynamic (below): does ACK alone hold the session, the
URL element format, the -6740/250ms timing — settle with the on-unit A/B experiment in §5.


**Step 1 — SetProperty ACK + URL dump — ✅ IMPLEMENTED + BUILDS (2026-08-01).**
- `altscreen_hook.c`: interpose exported `AirPlayReceiverSessionSetProperty(session,1,property,
  qualifier,value)`. When `property=="altScreenURLs"`: log count + **dump each URL** (CFStringGetCString),
  and `return 0` (⇒ HTTP 200 ACK). Everything else tail-calls the real one. Gated `ALTSCREEN_HANDLE_ALT`.
- No storing in platformData@+0x128 — that MHI3 slot is N/A (stock 210 has no getter); the hook keeps
  its own copy (strdup of url[0]) only for the opener. Built: `build/libcarplay_hook.so` (emutls=0).
- **On-unit test (Mode A of §5):** advert ON + `ALTSCREEN_HANDLE_ALT`, watch `/tmp/*altscreen*`:
  does the `altScreenURLs` line appear (proves ingress on a real phone) and does the **-6740
  teardown-loop stop**? The dumped URLs also settle the format question.

**Step 2 — URL opener — ✅ IMPLEMENTED (gated probe arm; forward best-effort).**
- `altscreen_hook.c` `url_open_thread`: gated `ALTSCREEN_OPEN_URL` (Mode B). Parses `http://host:port/path`,
  HTTP GET url[0], **logs status line + first body bytes** (so we learn the exact format), and if the
  body is raw Annex-B (`00 00 00 01`) forwards it via `altav_forward_frame` → altscreen_render.
- HONEST LIMIT: if the body is NOT raw Annex-B (TS/HLS/fMP4 → likely), we log-only; the demux/codec
  glue is finalized AFTER the on-unit dump tells us what the phone actually serves. Writing it blind
  would be a guess. https not handled (log+skip).

**Step 3 — iAP2 scene-active report** — TODO, only if iOS still tears down after a real Mode-B open.

**Interim calibration — ✅ `altscreen_render` chessboard (`ALTSCREEN_TESTPATTERN`).**
- `testpattern_qnx.c`: 16×6 semi-transparent labeled chessboard (A1..P6) on plane 99, instead of the
  black plane, so the **usable cluster area** can be measured on-unit. Needs only ctx 80 live — NO phone.
  Built into `build/altscreen_render` (ARM, emutls=0).

### Env-flag matrix
| flag | effect |
|---|---|
| `ALTSCREEN_HANDLE_ALT` | advertise altScreen (/info) + arm the SetProperty ACK + (dormant) push-111 path |
| `ALTSCREEN_ADVERT_DISPLAYS` | also inject `displays=[main,cluster]` (needed for hasGaugeClusterScreen) |
| `ALTSCREEN_OPEN_URL` | Mode B — actually open url[0] over HTTP + forward Annex-B |
| `ALTSCREEN_TESTPATTERN` | altscreen_render draws the calibration chessboard instead of video |

---

## 5. Open questions to verify (do NOT trust findings blindly)
1. **The ingress seam + ABI** (Step 0) — the #1 blocker; everything else depends on it.
2. Does store+ACK alone keep the session alive, or must the HU actually OPEN a URL? UNRESOLVED.
3. Exact `altScreenURLs` element format (HTTP(S)? RTSP? AirPlay stream ref?) — dump on-unit.
4. What "scene active" iAP2/control ack (if any) iOS blocks on after a URL is opened.
5. Is the observed dio loop a `-6740` teardown or an actual crash? (hook comment says crash; verify.)

### Fastest experiment to settle Q2 (after Step 0 locates the seam)
One controlled A/B hook at the seam:
- **Mode A:** retain the received URL array + emit a verified successful control-channel response,
  but make NO connection to any URL.
- **Mode B:** same, then immediately open/drain the FIRST URL.
Capture: control traffic, URL connection attempts, monotonic timestamps, iAP2 teardown, and
dio_manager PID continuity.
- A dies, B survives → opening a URL is required.
- A survives → store+ACK is enough.
- Both die → URL-open alone is insufficient; a scene-active/report step is missing.

---

## 6. Bottom line
- **RGI + coverart** do not depend on any of this (BAP + TCP bus); fixed by the no-dup bus hook.
- **altScreen on iOS 26.1** = build a small **cluster-scene receiver** in the hook (SetProperty
  handler → URL opener → iAP2 report). Our from-source's push-video path and the current
  `alt_find_111`/`alt_setup` are the wrong protocol and should be retired.
- Start with **Step 1** — it's a small hook change and decides how big the rest is.
