# MHI2Q CarPlay AltScreen — Master Knowledge Base (A→Z)

**Last updated:** 2026-05-25
**Goal:** drive a second CarPlay "AltScreen" surface onto the Audi MHI2Q (MU1316)
instrument cluster, by **reusing libairplay's own screen primitives** (2nd screen
session) + tapping the decrypted H.264 → a separate decoder process that owns the
cluster surface (via `maneuver_render`). Wired CarPlay.

Single source of truth for the engineering re-open. Companions in this folder:
- [`feasibility.md`](feasibility.md) — primitive inventory + iOS contract + phases.
- [`impl_plan.md`](impl_plan.md) — module design, interposer table, RE-resolved bodies.
- [`re_findings.md`](re_findings.md) — raw addresses/signatures/struct offsets.
- [`gate_analysis.md`](gate_analysis.md) — the original 6-gate "wireless-altScreen-as-
  Apple-intended is impossible" writeup (still valid for *that* framing).
- [`reference/`](reference/) — MHI3 working hook (`mhi3_libaltscreen_hook_ultra.c`)
  + MHI3 master knowledge — Cosmin's proven recipe (got the 111 stream on MHI3).

Live hook code: `../c_hook/altscreen/` (stays there — the build needs it).
Cluster renderer: `../maneuver_render/` (separate process; owns displayable 20 / ctx 74).

---

## 0. Status (2026-05-25)
Two framings of "AltScreen on MHI2Q":
1. **Apple-intended wireless altScreen** — still **impossible** (gate_analysis.md):
   Cinemo pre-2020 iAP2 can't emit subparam-17; libairplay has no native altScreen
   advertise. Not pursued.
2. **Engineering path (this KB)** — **feasible, no architectural walls left.** All
   data-plane primitives exist and are per-stream; the cluster display is solved by
   `maneuver_render`. Remaining = implementation (coding `handle_alt` + the decoder process)
   + on-device validation. No live MHI2Q target during this work → everything is
   compile-verifiable + self-logging; injection gated (`ALTSCREEN_INJECT=0`).

## 1. Why the three feared walls fell
| Feared wall | Verified reality |
|---|---|
| Replicate AES/crypto | ❌ key is **per-stream by `streamConnectionID`** (`AirPlay_DeriveAESKeySHA512ForScreen`, `"AirPlayStreamKey%llu"`) → 2nd stream just re-calls it |
| No 111 receiver in 210.81 | ❌ full screen stack present (main mirroring uses it); `ScreenStreamProcessData` + `ProcessFrames` are **per-object** |
| No cluster display sink | ❌ `maneuver_render` already composites to the cluster (Display Manager + MOST) |
| Need Ferrite control events (`setScreenRecordingState`) | ❌ Cosmin's working MHI3 hook sends NONE — correct 2-display `/info` + non-failing sink is enough; native pipeline + iOS bring the stream up |

## 2. Architecture (LOCKED)
```
libcarplay_hook.so  (altscreen module, pure LD_PRELOAD interposition)
  • AirPlayGetFeatures            -> real() | altScreen-bit          (advertise)
  • …PlatformCopyProperty         -> real() + append alt display dict (2 displays)
  • AirPlayReceiverSessionSetup   -> real() (100/101/110) + bring up 111 ourselves
       Screen_Create → UpdateState → Setup → DeriveAESKey(streamConnectionID)
       → SetSecurityInfo (AES ctx @ screen+0xC8) → ServerSocketOpen
       → append {type:111,dataPort:ourPort} to respDict["streams"]
       → spawn AltScreenReceiver thread (mirrors sub_24220):
            SocketAccept→NetSocket→Set{UUID,DeviceID,TimeSync,*EndTime}
            → StartSession (sets g_alt_stream) → ProcessFrames(screen,sock,key)
  • ScreenStreamProcessData       -> if stream==g_alt_stream: forward NALs +
                                     skip real decode (no 2nd HW decoder); else real
                          │  AVCC-framed NALs over TCP :19830 (altscreen_av wire fmt)
                          ▼
altscreen_render  (separate process)  — recv NALs → OMX H.264 decode → cluster
   reuse maneuver_render/platform_qnx.c (display_create_window(20), dmdt ctx 74, MOST)
```
Why interposition (not patching): libairplay 210.81 calls its own exports via
preemptible PLT/GOT thunks (verified) → same mechanism as the existing NmeTransport
hook. No mprotect/byte-poke/ARM-Thumb risk.

Why a 2nd screen *object* (not de-singletoning `sub_26F9C`): `ProcessFrames` is
parameterized `(screenObj, netSocket, key)`, so a self-owned object + listen socket
+ receiver thread gives libairplay everything to recv/decrypt/decode the alt stream.

## 3. iOS-side contract (keep altScreen alive)
"Inactive" destroy after 1-2 s = `screenstream_virtualDisplaySourceTerminated`
("display source terminated") — the FigVirtualDisplay source is the cluster-scene
app. **Apple Maps drives it (works); Waze doesn't (inherent, app-side.)**
HU must: advertise altScreen → `altScreenURLs` non-empty (else -16721) → 2-display
`/info` → screen port → consume frames. Ferrite control events are a **fallback**
(generic sender exists: `AirPlayReceiverSessionSendCommand @0x28460`).

## 4. Implementation status (tasks)
- ✅ `altscreen_av.{c,h}` — AVCC parse + Annex-B NAL framing + TCP forward (wire fmt).
- ✅ `altscreen_hook.{c,h}` — pure interposition; recon always-on; `inject_display`
  wired (append alt dict to "displays" array); `handle_alt` chain fully RE-resolved
  (approach A) — coding pending.
- ⏳ `handle_alt` body — Screen_Create chain + AltScreenReceiver thread (recipe known;
  exact arg orders for DeriveAESKey out-buffers + SetTimeSynchronizer cbs to nail).
- ⏳ `AirPlayGetFeatures` advertise bit value (confirm altScreen bit / 32-vs-64-bit).
- ⏳ `altscreen_render` — separate decoder process (offline-testable vs captured H.264
  in `reference/`-adjacent dumps / mhi3-altscreen dumps).

## 5. Hard constraints
- **No `__thread`** in the .so (QNX 6.5 ARMv7 gcc 4.4 emutls) — globals + mutex.
  (Cosmin's MHI3 hook uses `__thread`/AArch64 patcher — do NOT copy those parts.)
- Build via `../compile_hook.sh` (ntoarmv7-gcc 4.4, `-std=gnu99`); module is in
  `ALTSCREEN_SRCS`. Build VM is the user's local QNX VM (not reachable from CI).
- Symbols resolved via `dlsym` — version-robust; addresses in re_findings.md are
  for verification only.

## 6. Open items before "done"
1. Code `handle_alt` + AltScreenReceiver (offline) → confirm exact arg orders in IDA.
2. Build `altscreen_render`; validate decode offline against captured frames.
3. Confirm advertise feature bit (IDA `AirPlayGetFeatures` / MHI3 mask).
4. ON DEVICE (when available): does iOS open type-111 with our 2-display /info; does
   the 2nd session deliver frames; "inactive" stability; wired-USB altScreen init.
</content>
