# HUD-blue palette example — 2026-09-13

> Superseded clarification: full fill must remain the production HUD-like blue. Only the base may change. See ../fixed-hud-fill-study/README.md. The lighter active endpoints here were not selected.

User rejected grey/silver on the grey road and cluster backdrop. Red was then
questioned because it could suggest a wrong manoeuvre, while the user's HUD
always uses blue. The current design direction keeps the arrow blue in all states.
The specific shades in this example are still a proposal, not selected/deployed.

Shader endpoints: #0798D6 (saturated visible base) → #73D6F4 (lighter blue).
Existing lighting and saturation affect the output; sampled output values are
recorded in palette-values.json. This is not a measured colour match to the HUD.
Base = no progress / blink off; lighter blue = filled area / blink on. Two endpoints,
existing feathering and phase smoothing, no new bloom shader or animation.

The #464646 background is a contrast probe, not a calibrated cluster screenshot.
Actual framebuffer premultiplied RGB and alpha are composed over this backdrop.
Nine real GL captures passed the existing premultiplied-alpha check without
GL errors. Sampled active-state luminance is higher than the base.
Production sources, geometry, road materials, binary and deployment are unchanged.
Temporary source copies, executables and raw captures: build/hud-blue-study/.
