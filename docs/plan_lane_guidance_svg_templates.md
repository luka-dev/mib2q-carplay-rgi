# Lane Guidance SVG Template Plan

## Goal

Create SVG lane-guidance templates that match the OEM Audi HUD/cluster style from the cropped references in `logs/lane_guidance_crops/user_crops/`.

The SVGs are design masters first: precise geometry, color tokens, glow/blur treatment, and lane variants. Runtime rendering can later consume the same geometry by rasterizing SVGs or porting the paths to `maneuver_render`.

## References

Use these crops as the visual source of truth:

- `crop_01_day_wide_original.png` - wider daytime lane guidance crop.
- `crop_02_day_tight_original.png` - tighter daytime crop with clearer lane arrows.
- `crop_03_night_original.png` - nighttime crop with strongest HUD separation.
- `*_clean_bg_dim.png` and `*_clean_dark_panel.png` - only as separation aids, not as shape truth.

## Visual Style Targets

- No hard background in the final OEM-style SVG; HUD elements should float.
- Active or recommended lane arrows are light Audi HUD blue.
- Inactive lane arrows and separators are soft white/silver, not pure white.
- Shapes are softly bloomed and slightly translucent, not crisp UI icons.
- Lane separators are vertical dashed pills: usually three short rounded dashes.
- Lane arrows are compact road-sign glyphs: vertical stem plus a curved or angled branch.
- The original looks optically blurred through glass; SVG should include a subtle filter layer for glow/softness.

## Initial Color Tokens

Sampled from the current reference crops:

| Token | Day target | Night target | Notes |
| --- | --- | --- | --- |
| `--hud-blue-core` | `#9fc7df` | `#5b7fb4` | Use brighter day value for active SVG master; night can be a preview theme. |
| `--hud-blue-glow` | `#c8ecff` | `#6fb7ff` | Low opacity outer glow. |
| `--hud-white-core` | `#b3b8b0` | `#80797a` | Photo samples are dimmed by glass; SVG master can lift this slightly. |
| `--hud-white-master` | `#d8ddd8` | `#c7c2c0` | Practical master color before opacity/glow. |
| `--hud-muted` | `#8f958e` | `#5e5a58` | For not-good / unavailable arrows. |
| `--hud-preview-bg` | `#222927` | `#17110f` | Preview only, not part of floating HUD asset. |

Start opacity:

- Active blue core: `0.82`
- Active blue glow: `0.28`
- White/silver core: `0.72`
- Muted inactive arrows: `0.42`
- Separator dashes: `0.68`

## Geometry Plan

Build one normalized SVG coordinate system:

- `viewBox="0 0 772 252"` for the tight day crop aspect, then scale variants from it.
- Baseline around `y=164`.
- Lane stem height around `70`.
- Separator dash width `9-11`, dash height `28-34`, radius `5`.
- Lane slot pitch around `110-118`.
- Arrow stroke width around `22-26`, with rounded caps and joins.

Create reusable symbols:

- `lane-separator`: three stacked rounded pills.
- `arrow-straight-muted`: straight/forward lane arrow.
- `arrow-right-active`: right branch arrow matching the blue turn shape.
- `arrow-right-muted`: same path as active, muted/silver.
- `arrow-slight-right-active`: optional second variant if iOS angle is less sharp.

Prefer filled paths over stroked paths for the final master if the runtime rasterization exposes stroke inconsistencies. Use stroked paths during sketching because they are faster to tune.

## SVG Layering

Each glyph should render as three layers:

1. `glow`: same path, wider stroke/fill, Gaussian blur `stdDeviation=5-8`, low opacity.
2. `soft-core`: slightly blurred copy, `stdDeviation=0.7-1.2`.
3. `core`: main path, mostly sharp but still rounded.

Keep filters small with explicit bounds to avoid clipping:

```svg
<filter id="hudGlow" x="-35%" y="-35%" width="170%" height="170%">
  <feGaussianBlur stdDeviation="6"/>
</filter>
```

## Template Variants

Minimum SVG outputs:

- `lane_guidance_5_right.svg` - five lanes, right lane active.
- `lane_guidance_5_right_alt.svg` - same layout, tighter right arrow based on night crop.
- `lane_guidance_6_double_right.svg` - six lanes with two active right lanes, based on night crop.
- `lane_guidance_debug_palette.svg` - contact-sheet style preview with day/night backgrounds.

Potential runtime variants:

- 3-8 lane counts.
- Active statuses: not-good, good, preferred.
- Directions: straight, slight right, right, sharp right, slight left, left.

## Validation Loop

1. Draw the first SVG over `crop_02_day_tight_original.png` at matching size.
2. Compare silhouette: lane pitch, dash height, arrow branch angle, arrow thickness.
3. Compare color on dark and gray backgrounds using `clean_bg_dim` previews.
4. Rasterize SVG to PNG at 1x, 2x, and the renderer target size `328x181`.
5. Check that blur/glow survives downscaling without becoming muddy.
6. If it will be ported into GLES C code, freeze final path coordinates and convert curves to simple path commands or procedural primitives.

## Open Questions

- Should the SVG master represent the HUD photo exactly, including blur and dimmed glass color, or the cleaner idealized OEM source that would be rendered before windshield projection?
- Do we need only the current right-lane guidance shape, or a full lane-direction library for live iOS lane data?
- Is the final consumer SVG/raster assets, or should SVG only guide a procedural `maneuver_render/render.c` implementation?
