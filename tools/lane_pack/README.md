# lane_pack — procedural Audi-style lane-guidance sprites

Generates the 18 single-lane direction sprites used by `maneuver_render` to draw
the lane-guidance strip below the maneuver arrow.

Pure geometry — no firmware/SVG assets are read at runtime.  Pillow only.

## Run

```sh
python3 tools/lane_pack/draw_lanes.py
```

Produces:

| Path | What |
|---|---|
| `out/lg_dr_<HEX>[_recommended].png` | 18 RGBA sprites, 41×39 |
| `out/_preview_strip.png` | All 18 in one row for quick visual check |
| `out/_compare.png` | 4-row side-by-side vs reference (if present) |

`reference/` holds the Audi olymp1 800x246 originals (41×39 each) used
as a visual target — drop the firmware files there to enable the
compare grid.

## Direction codes

BAP convention from `c_hook/routeguidance/rgd_tlv.h`:

| Code | Meaning |
|---|---|
| `0x00` | straight |
| `0x20` | slight right |
| `0x40` | right |
| `0x60` | sharp right |
| `0x72` | u-turn (right-side curl) |
| `0x92` | u-turn (left-side curl) |
| `0xa0` | sharp left |
| `0xc0` | left |
| `0xe0` | slight left |

Each code has a `_recommended` variant rendered in our blue (`#59ABE6`,
matches the `maneuver_render` bargraph), used when BAP `gi=2 PREFERRED`.

## Style choices

- `41×39` tile size, `#323232` background — matches stock `olymp1/800x246`.
- Stem origin offsets to the side OPPOSITE the arrow direction
  (right turn → stem on left, left turn → stem on right, straight → centered).
- Sharp turns (60°, 90°) use a high stem with a quick L-shaped bend
  (J-hook profile).  Slight turns (20°, 30°) use a gentler S-curve from
  a lower stem.  U-turns (`0x72`/`0x92`) draw a tight Π-loop with a
  downward arrowhead on the return side.
- Render is at 8× via Pillow primitives, then LANCZOS-downsampled for
  antialiasing — no cairo / rsvg dependency.

## Next step

Port `draw_lanes.py` geometry into native GLES2 polygons in
`maneuver_render/lane.c` so the sprites are generated at runtime (no shipping
assets).  Bezier polylines from `_bezier()` already produce vertex
arrays compatible with `GL_TRIANGLE_STRIP`-style stroke rendering.
