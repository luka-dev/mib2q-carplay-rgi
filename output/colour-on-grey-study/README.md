# Colour on grey backdrop — 2026-09-13

> Later feedback: the user questioned red as suggesting a wrong manoeuvre and wants visual consistency with the blue HUD. See ../hud-blue-study/README.md. These red proposals were not selected.

User rejected grey/aluminium arrow bases, including a lighter version, because
both the road and the cluster background are grey. Distant manoeuvres must remain
chromatically distinct at zero fill. Do not return to grey-on-grey palette proposals.

Two isolated actual-renderer proposals were captured on a neutral #464646 backdrop:
red → white and white → red. The backdrop is a contrast probe, not a calibrated
measurement or screenshot of the user's cluster. It is composed using the actual
framebuffer's premultiplied RGB and alpha, not by recolouring previous screenshots.

Red → white is recommended: the distant arrow is coloured and active/blink-on is
brighter. White → red was captured for comparison but lowers active luminance;
its white base also provides less colour separation from the grey environment.
Both use the prior compensated red/white endpoints. Production sources, geometry,
lighting, road materials, feathering and animation are unchanged. No palette was
selected or deployed. All 18 GL captures passed the existing premultiplied-alpha
check without GL errors. Temporary build/captures: build/colour-on-grey-study/.
