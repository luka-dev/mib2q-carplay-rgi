# LVDS Video Pipeline -- Display Manager & Video Encoder Internals

## Display Manager (libdm_modMain.so)

Native display manager bridges Java DSI calls to videoencoderservice via ASI.

### Key Functions

- **`CDSIDisplaymanagement::setUpdateRate(display, rate)`** -- receives DSI from Java, posts event
- **`CDSIDisplaymanagement::evtSetUpdateRate`** -- calls `CASIMostEncoder::setUpdateRate()` ONLY (does NOT call setActiveDisplayable)
- **`CASIMostEncoder::setActiveDisplayable(displayID, displayable)`** -- stores displayable in RB-tree, forwards via ASI proxy to videoencoderservice. No gating.
- **`CASIMostEncoder::setUpdateRate(displayID, rate)`** -- stores rate in RB-tree, forwards via ASI proxy. No gating.
- **`CContextManager::preContextSwitchHook()`** -- called during context switch. Two branches:
  - Display HAS terminal -> hardware displayables path (physical screen)
  - Display has NO terminal -> MOST encoder path -> calls `setActiveDisplayable(displayID, firstDisplayable)`

### Display ID Mapping

- Java terminal 1 -> `getInternalDisplayID(1)` = **4** (cluster)
- videoencoderservice `mapDisplayId(4)` = **remoteDisplayId 0**
- Timer for remoteDisplayId 0 is always pre-created in constructor

## Video Encoder Service (videoencoderservice)

### Key Functions

- **`CASIVideoEncodingSrv::setActiveDisplayable(displayID, displayable)`** (`sub_10A0D0`) -- enqueues event {displayID, displayable, fps=-1}, signals condition variable
- **`CASIVideoEncodingSrv::setUpdateRate(displayID, rate)`** (`sub_1078D8`) -- enqueues event {displayID, -1, rate}, signals condition variable
- **`CASIVideoEncodingSrv::threadLoop`** (`sub_1072FC`) -- pops events from deque, calls processEvent
- **`processEvent`** (`sub_106EC4`) -- dispatches:
  - displayable>0 && fps<0 -> `adjustDisplayable` (stores displayable for capture)
  - displayable<0 && fps>=0 -> `adjustTimer` (starts/stops capture timer)
- **`CServiceController::mapDisplayId`** (`sub_10A638`) -- `displayID 4 -> remoteDisplayId 0` (cluster), others -> 1
- **`CServiceController::adjustTimer`** (`sub_10A694`) -- looks up pre-existing timer for remoteDisplayId, sets interval to 1000/fps ms, stops+modifies+starts timer. fps capped at 60, no other gating.
- **`CDisplayTimer::process`** (`sub_10B478`) -- timer callback:
  1. Gets displayable from array[remoteDisplayId]
  2. If encoder not open && m_encoder_reconnect -> opens encoder (openEncoder)
  3. If m_current_state == 1 (ST_ACTIVE) -> calls encodePicture (ipteCaptureDisplayable -> H.264 -> TS)
  4. Manages CDisplayLink (MOST connection)
- **`CServiceController` constructor** (`sub_10AAE8`) -- creates TimerGroup, creates timer at 1000ms (idle), adds as displayTimer id=0 (cluster). Timer is always pre-created if TimerGroup is valid.

## Context -> Displayable Mapping

| Context | Displayables       | First (sent to encoder)   | Notes                                                             |
|---------|--------------------|---------------------------|-------------------------------------------------------------------|
| 72      | {33}               | **33** (base map)         | FPK map -- always has content                                     |
| 74      | {20, 102, 101, 33} | **20** (KDK intersection) | FPK map + KDK -- displayable 20 only has content during native RG |
| 76      | FPK split          | varies                    |                                                                   |
| 77      | FPK split + KDK    | varies                    |                                                                   |

## ROOT CAUSE: setActiveDisplayable Never Called

1. **`setActiveDisplayable` is ONLY called from `preContextSwitchHook` during context switch** -- never from setUpdateRate
2. Java `DisplayManager.switchContext()` **skips DSI call** if `confirmedActiveContext[terminal] == requestedContext` (context already set)
3. If context 72 is already active from HMI boot/previous call, `switchContext(72, 1, null)` is a no-op -> preContextSwitchHook never fires -> setActiveDisplayable never called
4. videoencoderservice has displayable **0** (initial from constructor) -> IPTE captures nothing
5. When `setKDKVisible(20, 1)` maps 72->74, this IS a new context -> DSI fires -> setActiveDisplayable(4, **20**) -- but displayable 20 (KDK) has no rendered content without native route guidance

## Fix: Force Context Switch (IMPLEMENTED)

```java
// Switch away first to force a real context change
dm.switchContext(0, 1, null);    // any different context
Thread.sleep(250);                // wait for DSI confirmation
dm.switchContext(72, 1, null);   // switch to FPK map -> preContextSwitchHook -> setActiveDisplayable(4, 33)
Thread.sleep(250);
// Do NOT call setKDKVisible -- displayable 33 (map) has content, displayable 20 (KDK) does not
dm.setUpdateRate(1, 10);         // start video encoding at 10fps
```

Implemented in `ClusterService.activateClusterVideoPipeline()`.

## Verified Findings (Mar 2026)

1. **PresentationController renders map to displayable 33 at all times** -- map flickering observed on VC during context switch tests. Displayable 33 has content even during CarPlay.
2. **Displayable 33 is NOT blank during CarPlay** -- confirmed via KOMOProbe testing.
3. **MOST video connection succeeds** -- map flickering on VC confirms video frames reach the cluster via MOST isochronous channel.
4. **Context 0 is safe to switch to temporarily** -- sequence `ctx(0)` -> `ctx(72)` forces preContextSwitchHook.
