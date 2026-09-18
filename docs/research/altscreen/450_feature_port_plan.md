# Porting 450.14.2 CarPlay features into our 280.33.8 from-source libairplay

RE of MHI3 `libairplay.so` srcvers **450.14.2** (aarch64), 3 focused IDA passes (2026-07-13).
Goal: port the newer CarPlay features 450 has that our 280 lacks (95 Apple-engine funcs total).
Addresses are 450 VAs (image base 0). Our 280 = clean SOURCE; 450 = aarch64 binary (oracle for the delta).

## THE UNIFYING MECHANISM (read first)
Almost everything below rides two things our 280 ALREADY HAS:
1. **SETUP feature-latch:** the RTSP SETUP handler (`sub_50990`, our `_requestProcessSetup`-style path) reads a `"features"` sub-dict from the SETUP body and latches each capability as a `uint8` into the parsed screen/features context (`session+24` on aarch64). The `HasFeature*` getters just read these bytes. Keys → aarch64 offsets:
   | features key | ctx off | getter |
   |---|---|---|
   | `viewAreas` | +0x125 | HasFeatureViewAreas |
   | `enhancedSiri` | +0x126 | HasFeatureEnhancedSiri |
   | `altScreen` | +0x127 | HasFeatureAltScreen (matches our altScreen work) |
   | `uiContext` | +0x130 | HasFeatureUIContext |
   | `cornerMasks` | +0x148 | HasFeatureCornerMasks |
   | `focusTransfer` | +0x149 | HasFeatureFocusTransfer |
   | `h.264Level5.1` | +0x14A | (HasFeatureH264Level51) |
   **PORT:** do NOT copy the aarch64 offsets — our ARMv7 struct differs. Add Boolean fields to our parsed-features struct + latch them in our SETUP parser from the SAME `features` keys (exactly like altScreen is already handled). `CopyUIContextURLs` reads a CFArray at ctx+0x138 (+312).
2. **Command channel:** every `Request*/Change*/Update*/ShowUI/StopUI` builds `{ "type":<name>, "params":{...} }` and calls **`AirPlayReceiverSessionSendCommand`** (our 280 has it, `AirPlayReceiverSession.c:985` → POST `/command` binary-plist over the event RTSP conn). So all command fns are ~15-line typed wrappers — copy-paste with the exact keys below.
3. **Dict builders** (`AirPlayScreenDict*`, `AirPlayInfoArrayAdd*`, `MainScreenDictCreate`, `AltScreenDictCreate`) = pure `CFDictionarySetInt64/Value` into a caller dict. Leaf, no state.

---

## 1. INPUT (HID) — the joystick/MMI + multitouch
Drop into `src/hid_2q.c` exactly like the existing Buttons/Knob/SingleTouch reversals (malloc + fixed byte template + Create/Fill pair). None need session state except SendHIDReport/AddHIDDevice.

- **`HIDDPadCreateDescriptor` @0x10dd50** — 39-byte fixed Consumer-page descriptor; report=2B. Usages: AC Home(0x223)/AC Back(0x224) + Menu Up/Down/Left/Right/Pick/Escape (0x40-0x46). **← the MMI rotary/joystick d-pad.**
  bytes: `05 0C 09 01 A1 01 15 00 25 01 75 01 0A 23 02 0A 24 02 95 02 81 02 95 06 81 01 19 40 29 46 95 07 81 02 95 01 81 01 C0`
- **`HIDDPadFillReport` @0x10de30** — 2B: byte0 bit0=Home,bit1=Back; byte1 bits0-6=the 7 Menu usages.
- **`HIDPointingCreateDescriptor` @0x10e4a0** — 62B fixed; report=5B `[buttons(1), X u16LE, Y u16LE]`, X/Y logical 0..0x7FFF. **← free cursor.**
- **`HIDPointingFillReport` @0x10e530** — `(report, buttons, double xNorm0..1, double yNorm0..1)`; scales to 0..32767: report[0]=buttons, u16@+1=X, u16@+3=Y.
- **`HIDMultiTouchScreenCreateDescriptor` @0x10d700** — `(out,outLen,cfg)`, cfg={u16 xMax,u16 yMax,u32 fingerCount}; malloc(55*fingers+7); 6B header `05 0D 09 04 A1 01` + per-finger 55B Finger block (X/Y logmax patched) + `C0`. report=5B/finger `[tip+7pad, X u16, Y u16]`.
- **`HIDMultiTouchScreenFillReport` @0x10f4b0** — C++ vector→rewrite to buffer; per touch 5B: tip=(b?0:event!=0), X lo/hi, Y lo/hi. st_touch_event={u16 x@0,u16 y@2,u8 event@4}.
- **`HIDMultiTrackPadCreateDescriptor` @0x10f210** — 142B fixed blob `@0x14CD80` (memcpy); Digitizer TouchPad, logmax 0x0400, +two 0x24 device-config collections. report=19B.
- **`HIDMultiTrackPadFillReport` @0x10f280** — 19B, two finger records (id blob 4B + valid/count/flag each) — verbatim layout in agent report.
- **`AirPlayReceiverSessionSendHIDReport` @0x5eb80** — HU→iOS input send: `SendCommand({type:"hidSendReport", uuid:"%X"(deviceUUID), hidReport:CFData(bytes,len)})`. Needs our SendCommand (have it).
- **`AirPlayInfoArrayAddHIDDevice` @0x5ecd0** — advertise device in /info: `{uuid:"%X", name, displayUUID, hidProductID, hidVendorID, hidCountryCode, hidDescriptor:CFData}`.

Input flow to iOS: build descriptor (Create*) → advertise via AddHIDDevice in /info → on physical input, Fill*Report → SendHIDReport → iOS drives CarPlay UI. For altScreen/cluster input, the HID device's `displayUUID` targets the cluster display.

## 2. VIEW AREAS — the cluster-shape mechanism (load-bearing) — ✅ PORTED (2026-07-16)
**DONE in from-source 280.** All 4 items ported + verified against the 450 binary (VA-cited below).
Files: `AirPlayReceiverSessionScreen.c` (VideoConfig parse), `src/screenstream.c` + `src/screenstream_sw.c`
(SetViewArea + store/apply/forward), `AirPlayReceiverPOSIX.c` (`_CopyDisplayDescriptions` viewAreas /info),
`AirPlayReceiverSession.c` (SETUP features-latch + HasFeatureViewAreas getter + updateViewArea cmd),
`AirPlayReceiverSessionPriv.h` + both `ScreenUtils.h` (decls).
- **Per-frame header layout (450 ProcessFrames@0x674e4 → ScreenStreamSetViewArea@0x102b20):** rect arrives as
  floats in the **VideoConfig** (opcode 1) header: `params[3].f32={x,y}`, `params[4].f32={w,h}` (header bytes
  +32/+36/+40/+44). SetViewArea stores w/h/x/y at struct +56/+60/+64/+68 (arg order **w,h,x,y**), log-only in 450.
- **/info keys (AirPlayScreenDictSetViewAreas@0x5f990, all read from the binary):** `viewAreas` = array of
  `{widthPixels←idx1, heightPixels←idx0, originXPixels←idx2, originYPixels←idx3, safeArea:{same 4 keys}}`;
  `initialViewArea`; `adjacentViewAreas`. **idx0=height, idx1=width swap confirmed.** type-110-only extras
  (`viewAreaTransitionControl`, `viewAreaStatusBarEdge`, `viewAreaSupportsFocusTransfer`, safeArea
  `drawUIOutsideSafeArea`) gated `if type==110` — our alt is 111 so omitted. Builder is UNGATED (advertisement).
- **updateViewArea (0x5f0e0):** `{type:"updateViewArea", params:{uuid, animationDurationMillis, viewAreaIndex,
  adjacentViewAreas[]}}` via SendCommand. **Fire-and-forget — 450 does NO follow-up** (no ForceKeyFrame / no
  local re-config; the phone re-sends a VideoConfig header at the new geometry → ProcessFrames applies it).
  Both 0x5f0e0 and 0x5f990 have ONLY export-table xrefs (no internal caller — dio/platform drives them).
- **HasFeatureViewAreas (0x58ca0):** reads `*(session+24)+0x125`; capability key `"viewAreas"` in SETUP
  `features` sub-dict. Ported as a `Boolean hasFeatureViewAreas` on our session (latched in SETUP) + getter.

### original notes
- **`ScreenStreamSetViewArea` @0x102b20 — THE important one:** iOS sends the live view-area rect as **floats in the per-frame screen header**; `ProcessFrames`@0x674e4 parses `w/h/x/y` (after SetWidthHeight) → ScreenStream +56/+60/+64/+68 → crop/position. **PORT: add w/h/x/y uint32 to our ScreenStream + parse in our ProcessFrames + crop in decode.** This is what makes a non-rectangular cluster render correctly.
- **`AirPlayScreenDictSetViewAreas` @0x5f990** — /info builder: `viewAreas`(array of {widthPixels,heightPixels,originXPixels,originYPixels, +if type110: viewAreaTransitionControl,viewAreaStatusBarEdge,viewAreaSupportsFocusTransfer; safeArea sub-dict}), `initialViewArea`, `adjacentViewAreas`. AirPlayViewArea struct = 7×uint32 (idx0=height,idx1=width! swapped — match exactly).
- **`AirPlayReceiverSessionViewAreaUpdate` @0x5f0e0** — cmd `updateViewArea` `{uuid, animationDurationMillis, viewAreaIndex, adjacentViewAreas[]}`.
- **`AirPlayReceiverServerSetViewArea` @0x508e0** — low priority; server default area stash (off 172/180).
- HasFeatureViewAreas: `viewAreas` feature key.

## 3. CORNER MASKS — advertise only, NOT applied
- **`ScreenStreamSetCornerMask` @0x102db0 = `return 0` STUB** in 450 too. Corner-rounding must be done in `maneuver_render`/compositor, NOT libairplay.
- `AirPlayScreenDictSetCornerMasksSupport` @0x5fde0 — sets `cornerMasks`:bool in /info.
- `AirPlayReceiverSessionScreen_SetCornerMask` @0x66930 — accepts (mask,len) but **drops them** (only passes screenStream ptr). PORT as advertise+no-op; don't consume the mask.

## 4. UI CONTEXT (dashboard/cluster UI) — all SendCommand wrappers
- `ShowUI` @0x5f2e0 — cmd `showUI` `{uuid, url?}`.
- `StopUI` @0x5f430 — cmd `stopUI` `{uuid}`.
- `ChangeUIContext` @0x5f610 — cmd `changeUIContext` `{url}`.
- `UIAppearanceUpdate` @0x5e310 — cmd `uiAppearanceUpdate` `{uuid, appearanceMode, appearanceSetting}` (day/night).
- `AirPlayScreenDictAddUIAppearance` @0x5fd20 — /info builder: `uiAppearanceMode`,`uiAppearanceSetting`.
- `CopyUIContextURLs` @0x58d70 — returns retained CFArray at ctx+0x138. HasFeatureUIContext: `uiContext` key.

## 5. MAP / FOCUS / SIRI — SendCommand wrappers (9 of 11 trivial)
- **Map:** `ChangeMapZoomLevel`@0x5e670 cmd `changeMapZoomLevel` `{uuid, zoomDirection}`; `MapAppearanceUpdate`@0x5e4c0 cmd `mapAppearanceUpdate` `{uuid, appearanceMode, appearanceSetting}`; `AirPlayScreenDictAddMapAppearance`@0x5fd80 /info `mapAppearanceMode`,`mapAppearanceSetting`.
- **Focus:** `AccessoryAcquireFocus`@0x5dff0 cmd `accessoryAcquireFocus` (no params); `AccessoryGiveFocus`@0x5e090 cmd `accessoryGiveFocus` `{widthPixels,heightPixels,originXPixels,originYPixels,focusHeading}` (rect 5×u32); `AirPlayScreenDictSetAccessoryGiveFocus`@0x5fe50 /info `accessoryGiveFocus`:bool.
- **Siri:** all 3 wrap internal `sub_593A0` → cmd `requestSiri` `{siriAction, siriTriggerTimestamp?, siriTriggerZone?}`. AirPlaySiriAction enum: 0=n/a,1=prewarm,2=buttondown,3=buttonup,4=voiceactivation. `siriTriggerTimestamp` = AirTunesClock-synchronized host time (latency: UpTicks-ms; sample: audio-timing-node lookup). **Simplest port: button/prewarm Siri with latency=0 skips the timestamp math entirely.**

---

## UNIFIED PORT PLAN (by type + priority)

**Tier 1 — high value, low risk (do first):**
- HID block (§1): 8 descriptor/fill fns → verbatim into `hid_2q.c` (exact bytes given) + `AddHIDDevice` + `SendHIDReport`. **Gives the joystick/MMI + multitouch.** Proven pattern (we did 10 HID fns already).
- ViewArea `ScreenStream` w/h/x/y + ProcessFrames parse (§2). **Load-bearing for cluster shape.**
- `AltScreenDictCreate`/`MainScreenDictCreate`/`InfoArrayAddScreen` (§Info) → clean altScreen /info primitives (replaces our hand-built dict).

**Tier 2 — cheap command wrappers (copy-paste, reuse SendCommand):**
ShowUI/StopUI/ChangeUIContext/UIAppearanceUpdate/ViewAreaUpdate/ChangeMapZoomLevel/MapAppearanceUpdate/AccessoryAcquire/GiveFocus + the ScreenDict* /info helpers. ~15 lines each. Add as needed.

**Tier 3 — needs care:**
- SETUP feature-latch: add Boolean fields + latch from `features` sub-dict (extend our existing altScreen-latch). Enables all HasFeature*.
- Siri timestamp math (only if voice-activation needed; button/prewarm is trivial).
- `CopyPairingIdentity` (needs PairingSession — we have it; keychain consts given).

**Do NOT port:** corner-mask consumption (stub in 450 too → compositor).

**Note:** all command wrappers + dict builders are leaf/trivial. The only structural work is (a) ScreenStream view-area fields, (b) SETUP feature-latch fields — both small extensions of code we already have.
