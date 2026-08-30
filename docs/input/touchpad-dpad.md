---
title: MMI touchpad -> DPAD bridge
tags: [input, touchpad, verified]
status: verified-source
sources:
  - code: java_patch/com/luka/carplay/cursor/CursorController.java
  - code: java_patch/de/audi/app/terminalmode/dsi/carplay/CarplayDSILifecycleController.java
---

# MMI touchpad -> DPAD bridge

Stock forwards the rotary, knob press, back and softkeys to CarPlay natively. The **MMI touchpad is
the only input device stock leaves unbridged** - this patch adds the missing leg.

## Context

> touchpad `updateTouchEvents` -> `TerminalModeDSIKeyEventsController` (class-replaced) ->
> **CursorController** -> stock DSI `postDpad` -> CarPlay session.

## Model

`CursorController` (legacy name - it once drove an on-screen cursor, abandoned because the H.264
encoder ghosted the overlay through motion compensation). A single-finger drag accumulates signed
`deltax / deltay` since the last emit; whenever `|deltax|` or `|deltay|` crosses a **speed-adaptive threshold** it
emits a `KEY_DPAD_*` press+release pair and subtracts the threshold from that accumulator.

- Thresholds: fast finger -> **150**, slow -> **200** units (`DPAD_THRESHOLD_FAST/SLOW`; instantaneous
  per-sample speed, units/100 ms with the sample `dtMs` clamped to 200). A quick flick advances one
  item; a slow drag needs more travel.
- A long drag emits **multiple ticks** (traverse several list items in one gesture); a diagonal drag
  emits both X and Y ticks when both accumulators cross.
- Entering single-finger anchors without emitting and resets the accumulators so prior drift doesn't
  leak in.

## Wiring

`TerminalModeDSIKeyEventsController` (class-replacement) calls `installCursorTouchSink()` on CarPlay
start and routes `CursorController`'s `postDpad(KEY_DPAD_*)` back through the stock DSI bridge
(`KEY_DPAD_LEFT/RIGHT/UP/DOWN` are the module's own 1/2/3/4 codes, re-emitted as DSI directional keys)
into the CarPlay session. The sink is removed on disconnect (`setTouchSink(null)`).

Rotary, knob press and softkeys already reach CarPlay through stock DSI - only the touchpad needed
bridging.
