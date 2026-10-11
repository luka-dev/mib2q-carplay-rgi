---
title: VC route text - FctID 19 content, ETA toggle & scrolling
tags: [rgd, bap, cluster, text, verified]
status: verified-source
sources:
  - code: java_patch/com/luka/carplay/rgd/BAPBridge.java
  - code: java_patch/com/luka/carplay/rgd/CurrentPositionScroll.java
  - code: java_patch/com/luka/carplay/rgd/VCTextData.java
  - code: java_patch/com/luka/carplay/rgd/VCUnicode.java
  - code: java_resources/com/luka/carplay/rgd/vc-text.bin
  - code: scripts/build_java.sh
  - test: tests/VCTextScrollTest.java
  - test: tests/RouteInfoPresentationTest.java
reconciles:
  - mib2q-carplay-rgi-next docs/cluster-and-rgi/RGI_AND_BAP_BRIDGE.md (route text section)
---

# VC route text - FctID 19 content, ETA toggle & scrolling

All CarPlay route text on the VC goes into **FctID 19 CurrentPositionInfo** - one UTF-8 field of at
most 96 bytes. FctID 20 TurnToInfo is sent as `("", "")` during RGI and stock writes to it are gated
([bap-fctids](bap-fctids.md)). FctID 22 always carries the absolute arrival clock (`timeInfoType = 1`; type 0 blanks
the VC's distance/arrival block).

## 📋 Context

> [rgd-activation](rgd-activation.md) - active -> **vc-route-text** - FctID 19 fragments -> VC lower bar.
> Phase toggle: [steering-wheel](../input/steering-wheel.md).

## 🔍 What is shown

| View | Phase 0 (default) | Phase 1 (after roller OK) |
|---|---|---|
| Fullscreen | `‹exit/signpost›`, else `● next road`, else current road | `◌ 10:42 \| 37 min` (`1 h 05 min` over an hour) |
| Smallscreen | same as fullscreen | `◌ 37 min` (arrival clock if no remaining time) |

- **Input priority** - `mExitInfo` (signpost, wrapped in U+2039/U+203A) -> `mAfterRoad` (last `:`-part)
  -> `mName` -> `currentRoad` (no prefix), after whitespace normalisation.
- **Prefixes** - U+25CF + space before the next road; U+25CC DOTTED CIRCLE + space before time. The
  decorations are budgeted on **every** fragment.
- **Never `""`** - with no text and no estimate, active RGI publishes `…` (U+2026); an empty string makes
  the VC show its native placeholder.
- **Phase 1 returns by itself** - `RouteGuidance` arms a 20 s hold (`INFO_TIME_HOLD_MS`) after the
  summary is published, then drops back to phase 0. A View change resets to phase 0 and republishes the
  cached state at once.
- The 12/24 h clock follows the HU setting; the arrival epoch is shifted from the JVM's UTC to HU local
  time before `AppConnectorNavi` formats it.

## 🔄 Scrolling (`CurrentPositionScroll`)

Text that does not fit is split into fragments, and only FctID 19 is rewritten on each tick:

| Constant | Value | Meaning |
|---|---:|---|
| `MAX_BYTES` | 96 | UTF-8 budget per fragment (incl. prefix/suffix/direction mark) |
| `WIDTH_64` | (359 - 6) x 64 | width budget in 1/64 px (uncalibrated rounding reserve) |
| `HOLD_MS` | 1800 ms | minimum dwell on the first and last fragment (and each page) |
| `MIN_STEP_MS` | 250 ms | minimum step between scroll fragments |

- Fragments break only at **extended grapheme** boundaries, after NFC; surrogate pairs and combining
  sequences are never split. An orphan combining mark is measured with the shaper's U+25CC base.
- RTL and Indic/contextual scripts use overlapping **pages** in logical order (advance 3/4 of a page)
  instead of a per-grapheme scroll; a leading U+200E/U+200F keeps the paragraph's first-strong
  direction. The VC still performs bidi and shaping itself.
- A single grapheme wider than the budget cannot be split: the source is kept and one static fallback
  frame is shown. Missing glyphs / fallback are logged once per text (`CurrentPosition font coverage=`).
- The scroll is suspended while the presentation is not confirmed, and restarts on a new maneuver,
  maneuver version or `route_generation`.

## 📊 Glyph metrics in the JAR

`VCTextData` loads `vc-text.bin` once (magic `VCT2`) from the classpath: per-code-point glyph advances
of the VC fonts plus pinned **Unicode 17** property, decomposition and composition tables, packed as
three 21-bit fields per `long`. `VCUnicode` implements NFC and grapheme segmentation on those tables,
independent of the HU's old `Character`/`BreakIterator` data - no fonts, AWT, ICU or native shaper on the
unit. `scripts/build_java.sh` copies `java_resources/` into the class tree before assembling the JAR, so the table
(and `META-INF/UNICODE-LICENSE.txt`) ship inside `carplay_hook.jar`.

## 🤔 Open

- (!) U+25CC is taken from the VC's supplementary fonts; how the unit renders it when a face lacks it is untested on-car.
