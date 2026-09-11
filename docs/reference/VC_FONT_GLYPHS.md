# VC 0379 route-text glyphs

Source firmware: `AU_C1_AU491_0379_1310_prod_8S0906961AL_A4-A5-Q5_2018`.
Audited 2026-09-10 from its existing `extracted/` tree, without changing firmware.

## Selected presentation

| Purpose | Text | Unicode | Coverage in extracted fonts |
| --- | --- | --- | --- |
| SignPost/Exit | `‹Exit 12›` | U+2039 / U+203A | All five Audi faces |
| Small-view time | `◌ 37 min` | U+25CC DOTTED CIRCLE | Arabic and JP/KR; absent from Audi faces |
| Full-view time | `◌ 22:42 \| 37 min` | U+25CC DOTTED CIRCLE | Same; arrival respects HU 12/24-hour setting |

U+25CC is a standalone dotted circle, **not** U+20DD COMBINING ENCLOSING CIRCLE
and not an emoji sequence. Its presence in supplementary font files does not
prove the navigation widget selects that font: physical VC rendering/fallback
is still an acceptance test. The user reports no route text on HUD in their
tests; this presentation targets VC and leaves HUD maneuvers/distances unchanged.
Do not generalize that observation into a claim about all HUD text modes.

All text stays in CurrentPositionInfo/FctID 19. TurnToInfo/FctID 20 is cleared;
TimeToDestination/FctID 22 stays absolute ETA. See [the field matrix](NAVSD_FCTID_MATRIX.md).
The 96-byte UTF-8 text budget reserves **6 bytes** for `‹›`, leaving 90 for content.
Host tests verify exact U+25CC bytes `E2 97 8C`, both views/phases, input fallbacks,
closing quotes, and UTF-8 boundaries including supplementary characters.

## Font evidence

Nine outline fonts were found. Five Audi faces have 674/674/671/671/669 Unicode
cmap entries, with 668 codepoints common to all five. The other files are
`LT_Univers440_88perc_Arabic.ttf`, `SourceHanSansJP-Regular.otf`,
`SourceHanSansKR-Regular.otf`, and `ZYHei_GB18030_c.ttf`.

The main fonts are under `stage2_fs/STAGE2.1/data/gtf/` and
`stage2_fs/STAGE2.2/data/gtf/`; `AudiTypeDisplayHigh-Normal_08.ttf` is under
`stage3_fs/data/gem/fonts/`.

For `AudiTypeDisplayHigh-Normal.ttf`: SHA-256
`dc859c10b86b8629fbc1d426ecd2b7cade58a11deb2e015d17a23df8ca518c19`;
sfnt cmap offset 2456, length 4314. No standard clock/watch/hourglass codepoints
were found in any of the nine outline fonts. Private-use sheets were inspected:
U+F8FF in Audi draws four Audi rings, despite glyph name `apple`; it is not a clock.

The separate gzip-wrapped Arial bitmap was also decoded: 557 ordered codepoints,
9206×34 pixels at 4 bits/pixel; no clock glyph found in the complete bitmap.
Its decompressed SHA-256 is
`8308e59073430a99899f5d14bb8028a31fc3fe4c14295b2112391501e7a62273`.

Actual stopwatch **images**, not text glyphs, were extracted from `atlas_full_LA.atlas`:
`lap_timer/intervalWatch.png` (UID 221) and `lap_timer/stopwatch.png` (UID 223),
both 32×34. Atlas SHA-256:
`36dcaf425079dbd1a10e32b8e01b50b847d0c2d0f62168afa97678be706ebeb5`.
No text-code mapping to these sprites has been established. Atlas UIDs are not
Unicode values; sending a UID as a character is not a way to select its image.

## Reproduce the outline-font audit

`tools/vc_font_audit.py` requires Python, fontTools and Pillow (tested with
fontTools 4.63.0 and Pillow 12.1.0). Supply the firmware root and a new output path:

```sh
python3 tools/vc_font_audit.py /path/to/firmware /path/to/new-audit-output
```

It copies font resources, records source paths/SHA-256/table offsets, exports full
Unicode and platform cmap catalogs, and renders exact-font Audi specimens without
OS fallback. Eight `.lur` files are structurally checked as sorted range-to-slot
tables. Range membership is **not glyph coverage**, nor proof of UI font binding.
`--refresh` regenerates a previous audit output. The separate deep bitmap/atlas
diagnostics are recorded in the local evidence directory, not reproduced by this tool.

Full local evidence: `output/r2/vc-fonts-0379-20260910/` (README, JSON catalogs,
PNG previews and follow-up scripts). Extracted proprietary fonts/images and bulky
generated catalogs are intentionally not part of this source commit.
