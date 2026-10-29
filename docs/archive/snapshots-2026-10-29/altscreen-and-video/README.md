# AltScreen and Video — archived documentation

> Historical snapshot before the current-state documentation restructure. Commands, status and measurements below belong to their recorded revisions.

*Map of content for CarPlay's second screen: the instrument-cluster video (altScreen, stream 111) and the path from decode to panel.*

---

## 📋 What this section covers

CarPlay can drive a **second display**: the Audi instrument cluster (Virtual Cockpit). Apple calls it *altScreen*. On the wire it is AirPlay screen **stream type 111**. The phone offers it only when the head unit advertises the cluster display and negotiates the feature, so the whole feature depends on that gate. Once the phone streams H.264, the hook converts it to Annex-B and forwards it over localhost TCP `:19831` to `altscreen_render`. The renderer OMX-decodes it and draws it into the managed cluster displayable `99` (`altscreen_render/protocol.h:24`, `altscreen_render/screen_egl_present.c:45`).

Read [AltScreen Cluster Video](../../../altscreen-and-video/ALTSCREEN_CLUSTER_VIDEO.md) first for the activation gate and the cluster wiring. Then read [Video Pipeline](../../../altscreen-and-video/VIDEO_PIPELINE.md) for the decode and delivery internals, [Cluster Loader](../../../altscreen-and-video/CLUSTER_LOADER.md) for the PRISM waiting animation, and [Reconnect and Lifecycle](../../../altscreen-and-video/RECONNECT_AND_LIFECYCLE.md) for replug behaviour. [Home](../../../README.md) is the hub and [Landmines](../../../LANDMINES.md) holds the trap list. Cluster route-guidance graphics, the *other* frame source in the same cluster context, are covered in [Cluster and RGI overview](../../../cluster-and-rgi/README.md).

## 📊 The whole path

The 2026-10-06 source candidate adds shared recovery, idle EOF/status observation,
owned in-place conversion, batched I/O, end-to-end compressed age and decoded
mailbox selection. The stock capture clock, `glFinish` and per-frame image
refresh remain in place. **[code]**

First drive with that build (2026-10-09): the phone delivered a steady ~21 fps
and the hook forwarded every frame, but the cluster went black twice for
1.6–1.8 s. Both were the renderer's own steady-backlog rule turning a ≥1 s
stall of its threads (process starvation at default priority) into a
generation restart with a decoder teardown. Since 2026-10-09 a backlog keeps the
picture, reports `STATUS 0` so the hook requests an IDR, and drops the queued
frames when that IDR arrives; `altscreen_render` runs at priority 15 like
`maneuver_render`, the OMX input wait is 3 s on a monotonic clock, and the
hook's greeting window is 250 ms so a late `accept()` no longer degrades the
link to legacy framing. Details: [Video Pipeline](../../../altscreen-and-video/VIDEO_PIPELINE.md),
[Landmines](../../../LANDMINES.md). **[car + code]**

```mermaid
flowchart TB
    accTitle: AltScreen activation and delivery overview
    accDescr: The phone negotiates stream 111, the hook transfers owned frames through a bounded worker queue, and the renderer decodes and selects mailbox pictures before the stock cluster capture.

    read_info["Phone reads car info"] --> negotiate_alt["Control SETUP enables altScreen"]
    negotiate_alt --> setup_stream["SETUP creates stream 111"]
    setup_stream --> prepare_frame["Hook decrypts and prepares owned AU"]
    prepare_frame --> forward_frame["Bounded worker forwards on 19831"]
    forward_frame --> decode_frame["Renderer reader queues OMX input"]
    decode_frame --> select_picture["Mailbox selects decoded picture"]
    select_picture --> present_picture["glFinish and swap to plane 99"]
    present_picture --> capture_plane["Stock capture feeds instrument cluster"]
```

Read [current pipeline and protocol](../../../altscreen-and-video/VIDEO_PIPELINE.md) for ownership,
HELLO/ACK/STATUS and FIFO fallback; read
[all six changes and validation](../../../altscreen-and-video/ALTSCREEN_FRAME_RATE.md#-six-follow-up-improvements-implemented--2026-10-06)
for operation counts and the test evidence.
[Input commands](../../../altscreen-and-video/ALTSCREEN_INPUT_COMMANDS.md) covers the separate native command
worker, roller cadence, immutable selected age, ACK/CF contracts and runtime
knobs. [Current pacing validation](../../../altscreen-and-video/ALTSCREEN_PACING_VALIDATION.md) collects video
and input review resolutions, reproducible commands and latest result limits.

## 🔗 The notes

| Note | What it covers |
|---|---|
| [AltScreen Cluster Video](../../../altscreen-and-video/ALTSCREEN_CLUSTER_VIDEO.md) | The activation gate — control-SETUP response `enabledFeatures` ∋ `altScreen` (the `/info` advert is also injected but does not set the flag) — then the hook's own stream-111 receiver. Also the iOS URL model, error codes, the 1440×542 panel, and the forceKeyFrame `uuid` fix (resolved). |
| [Video Pipeline](../../../altscreen-and-video/VIDEO_PIPELINE.md) | The OMX decode chain (native 32m buffers → EGLImage → RGBA plane 99), the Nagle/`TCP_NODELAY` fix, the load-bearing `glFinish` and per-frame rebind, owned buffers, v2/status protocol, mailbox/FIFO ownership, stream 110 and screen crypto for comparison. |
| [Cluster Loader](../../../altscreen-and-video/CLUSTER_LOADER.md) | The PRISM waiting animation: an embedded glass-Apple material shader in `altscreen_render/loader/`, drawn until the first decoded picture takes over. |
| [Reconnect and Lifecycle](../../../altscreen-and-video/RECONNECT_AND_LIFECYCLE.md) | Lifecycle churn on a fast replug, now absorbed by a 400 ms deactivate debounce. Also covers the advert correction, the main-screen UUID fix, and renderer supervision. |
| [AltScreen Input Commands](../../../altscreen-and-video/ALTSCREEN_INPUT_COMMANDS.md) | Canonical input path, paced zoom, reserved priority, state replacement, pre-commit/TTL cancellation, CF faults and runtime knobs. |
| [AltScreen Pacing Validation](../../../altscreen-and-video/ALTSCREEN_PACING_VALIDATION.md) | Current startup/steady, HELLO/STATUS, clock and input-command contracts; 25 sanitizer scenarios, paced zoom, command priority/coalescing, test commands and hardware limits. |
| [AltScreen Frame Rate](../../../altscreen-and-video/ALTSCREEN_FRAME_RATE.md) | Saved 20–27 fps evidence, sender/decoder/capture analysis, all six current pacing improvements, operation counts and offline validation limits. |
| [Maps 30 FPS Profile](../../../altscreen-and-video/MAPS_30FPS_PROFILE.md) | Always-enabled Identify PPID addition for the verified Maps-30 profile, identity-preserving fallback and native vphone evidence. |
| [Wired 30 FPS Investigation](../../../altscreen-and-video/WIRED_30FPS_INVESTIGATION.md) | Saved HU logs, native vphone source-rate controls, direct Maps capability override and accessory PPID evidence; physical-unit validation limits. |
| [Maps Frame-Rate Profile Mechanism](../../../altscreen-and-video/MAPS_FRAME_RATE_PROFILE_MECHANISM.md) | iAP2 identity through CarExperienceAssets to Maps/VectorKit scheduling, profile selection, cache updates and reconnection. |
| [PPID Profiles and Capability Settings](../../../altscreen-and-video/PPID_PROFILE_COMPARISON.md) | Original HU identity evidence, display/Siri/capability settings, and an inline table of all 90 PPIDs and their 97 exact rules in asset version 40. |
| [AltScreen RE History](../../../altscreen-and-video/archive/ALTSCREEN_RE_HISTORY.md) | Every altScreen framing, gate theory, plan and verdict in order — what held and what was refuted. |
| [Cluster Video Encoder](../../../altscreen-and-video/CLUSTER_VIDEO_ENCODER.md) | DisplayManager, `videoencoderservice` and stock KOMO: how a cluster context becomes the VC's H.264. |
| [Cover Art Pipeline](../../../altscreen-and-video/COVER_ART_PIPELINE.md) | Stock MH2P cover-art design vs our MHI2Q hook, and the size-only Setup regression. |
| [Enhanced Siri Contract](../../../altscreen-and-video/ENHANCED_SIRI_CONTRACT.md) | What iOS 26.6 needs for zero-latency Siri and voice trigger: the negotiation gate, `/info` keys, the `'zll '` parameters message, the timestamped `requestSiri`. Verdict: button-only possible in principle, not built; voice trigger not practical. Probe: the control SETUP logs the phone's feature proposal. |
| [CarPlay Video Playback](../../../altscreen-and-video/CARPLAY_VIDEO_PLAYBACK.md) | iOS 27 "Video in Car" (stream 130, URL playback). **Not planned:** only a partial build is possible (clear H.264); DRM needs Apple's receiver FairPlay module, and camera clips are HEVC. |
| [CarPlay Feature Survey](../../../altscreen-and-video/CARPLAY_FEATURE_SURVEY.md) | Stock baseline, features we added, tiered list of addable ones, the LOD widgets / Live Activities chain. |
| [Partial 360 Camera Preflight](../../../altscreen-and-video/PARTIAL_360_CAMERA_PREFLIGHT.md) | Deferred: CarPlay beside the top-view camera — contexts, crops, camera ownership, coordinator design. |

## 💡 Truth levels

Claims are tagged **[car]** for confirmed on the unit, **[decompile]** for reverse engineering, **[code]** for verified in the current source, and **[open]** for unverified. The earlier stream 111 path from decode to panel is **[car]**: one session ran 13,056 frames in 5 min. The October 9 drive above exercised the earlier pacing build and found two blackouts. Subsequent corrections and later lab profiles have their own validation records; they are not automatically vehicle-accepted. The phone delivered about 21 fps in that drive; a universal 30 fps claim remains unsupported. Provenance is listed in each note's Sources section; merged originals remain in git history.
