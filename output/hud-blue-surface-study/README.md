# Further blue-only base treatments — 2026-09-13

User requested more variants after the dark-blue rim proposal. These are three
isolated VISUAL prototypes, not selected themes or production implementations:

A / stripes: alternating blue tones in the unfilled region. Static world-XZ
procedural bands; no image texture. Filled region becomes uniform HUD-like blue.
B / glow: broader softer rim, using 8 offset silhouette passes at two strengths.
C / beam: a soft brighter stripe along the route centre with darker blue sides.
A temporary vertex attribute estimates transverse distance using the existing
centreline points and each vertex's path distance; the half-width is fixed to .07.

All use the existing active endpoint vec3(0.32,0.77,1.0), with blends of the dark
base #0A4D87 and that active endpoint. There is no white active endpoint or third
semantic colour. Decorations blend away under the existing soft progress front
and tint weights. All 9 full-fill/blink-on frames are PIXEL IDENTICAL to the fixed
blue baseline. The 27 GL captures passed existing premultiplied-alpha checks and
had no GL errors; checks.json records parity. Camera/geometry/road are unchanged.
Grey #464646 backdrop is a contrast probe, not a calibrated cluster screenshot.

Do not deploy these copied-source binaries. Before any chosen treatment ships:
- A needs moving-scene pattern anchoring and aliasing review on the source resolution.
- B still lacks proper multi-mesh occlusion and transient alpha handling; draw cost
  is unprofiled (extra silhouette draws disable depth testing/writes).
- C needs general width/head/join coordinate validation and a cached production
  implementation of the new attribute. Animation and target performance are untested.

Main source files and production palette have not changed. No commit or deployment.
Temporary code/build/raw captures: build/hud-blue-surface-study/.
