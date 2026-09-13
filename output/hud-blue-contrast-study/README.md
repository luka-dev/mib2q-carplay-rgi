# Stronger blue fill contrast — 2026-09-13

> Superseded clarification: full fill must remain the production HUD-like blue. Only the base may change. See ../fixed-hud-fill-study/README.md. The lighter active endpoints here were not selected.

The user found the prior saturated-blue/light-blue pair too similar. Two isolated
proposals preserve blue identity, two endpoints, existing soft fill and blink phases.
A keeps the previously bright base and lifts the fill close to icy white.
B uses a deeper saturated blue base and an icy fill, with a larger brightness range
but reduced distant-arrow luminance. No grey/silver or red base is reintroduced.

The comparison includes the previous proposal. Far (0%), half (50%), U-turn and
blink-on captures expose the tradeoff, on the same neutral #464646 backdrop.
The backdrop is a contrast probe, not a calibrated cluster screenshot.
Shader values and sampled top-face host pixels are in palette-values.json.

All 18 real GL captures passed the existing premultiplied-alpha check and had no
GL errors. Brighter active-state host luminance was checked for both candidates.
These remain proposals: production source, road, geometry, feathering and animation
are unchanged, and no palette is selected or deployed. Temporary sources/build and
raw captures: build/hud-blue-contrast-study/.
