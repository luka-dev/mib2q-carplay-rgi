# Arrow palette study — 2026-09-13

The user reports that the current grey-blue arrow looks poor on the actual cluster.
These are isolated host-preview proposals, not an accepted or deployed palette.

- A: neutral silver → saturated turquoise (recommended starting point).
- B: saturated blue → pale ice.
- C: warm silver → amber.

Only the two arrow shader endpoints differ. Production source, lighting,
geometry, asphalt, contact shadow, feathering and animation are unchanged.
Base also serves blink-off; the filled endpoint serves blink-on, with no third colour.
An initial bright-white base was revised because its luminance exceeded the
filled colour. In the final proposals the sampled top face is brighter when active.
This checks host sRGB pixels, not the physical cluster's colour or luminance.

`palettes.png`: current palette and three proposals, base / 50% fill / blink-on.
`forms.png`: U-turn and roundabout comparisons.
`palette-values.json`: shader inputs and sampled host output values.

36 actual GL captures (4 palettes × 3 manoeuvres × 3 states) passed the existing
premultiplied-alpha check and completed without GL errors. Temporary source copies,
preview executables and capture scripts are in `build/palette-study/`.
