# Blue rim visual experiment — 2026-09-13

User found none of the material-inspired base colours harmonious with fixed blue
fill. This assistant-proposed experiment uses one dark blue base (#0A4D87) with
a thin rim in the SAME active blue. It is NOT selected or production code.

The existing active endpoint remains vec3(0.32,0.77,1.0). The rim is present only
around unfilled regions and fades with the existing feathered fill and tint weights.
It disappears at full fill/blink-on; all three full-state captures are PIXEL IDENTICAL
to the earlier fixed-blue baseline. Existing road, geometry and contact shadow are
unchanged. Views compare the identical base with and without the rim at 0% and 50%.

Prototype implementation lives ONLY in build/hud-blue-rim-study/ copied sources:
four offset mesh silhouette draws at 1.05 source pixels, forced active colour,
no depth writes/testing for those silhouette draws, followed by the normal mesh.
Rim shading uses an upward normal so side-face lighting does not darken the contour.
This is an isolated STATIC visual prototype, not a production implementation:
extra draw cost, multi-mesh occlusion, fade overlap and moving-scene behaviour
have NOT been validated on the target or approved. Do not deploy this build.
If the look is selected, implement/validate a proper mask-based or otherwise
occlusion-correct efficient outline and exercise the native transition suite.

18 actual GL captures passed the existing premultiplied-alpha check without GL
errors. Static full-fill parity was verified against the production-colour baseline.
No new colour endpoint, production source edit, commit, or deployment was made.
Grey #464646 backdrop is a contrast probe, not a calibrated cluster screenshot.
