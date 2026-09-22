# CurrentPosition text fit on VC 0379

Audited 2026-09-17. No production formatting or byte limits changed.

## Result and limits

There is no useful single character-count limit before visual ellipsis. Our
sender limits the whole string to **96 UTF-8 bytes**, without adding ellipsis.
The VC's native label separately enables single-line truncation in EAL.

The follow-up traced the grid allocator and font-group resolver. In our active
MAP/LVDS presentation, `SV_LVDS_FPK_NavMap.NavStatus.Street` gives CurrentPosition
a maximum **359 px** of content width, using **AudiTypeDisplay-Normal.ttf at
size 27**, in **both fullscreen and smallscreen**. It is one line with truncation
enabled. Short text receives its preferred width; overflowing text is limited
to 359 px.

The earlier High Normal measurements were illustrative and used the wrong face
for this widget. They are superseded by the Normal-face examples below.

This is a result from the firmware's layout and font-selection code, not a live
vehicle capture. The exact rasterizer rounding at a borderline fit still needs
EAL/on-VC verification; host Pillow measurements are estimates. Other native
compass/maneuver presentations are outside this result.

## Sender

`java_patch/com/luka/carplay/rgd/BAPBridge.java`:

- `MAX_CURRENT_POSITION_UTF8_BYTES = 96`.
- `limitUtf8` preserves UTF-8 and surrogate boundaries; it does not append `…`.
- `‹…›` reserves six bytes, leaving 90 bytes of signpost content.
- `● ` reserves four bytes, leaving 92 bytes of next-road content.
- Plain current-road fallback has all 96 bytes.
- The neutral `ROUTE_TEXT_PENDING = "…"` is a separate no-data placeholder.
- Our route presentation clears FctID 20 and publishes the text through FctID 19.

96 bytes permits 96 ASCII characters or 48 ordinary Cyrillic letters without
prefixes. This says nothing about whether the resulting string fits visually.

## Firmware evidence

Analyzed ELF:
`/Users/luka/Desktop/AUDI_2/Patches/UiModVC/A5_2018_unpacked/hmi_app.elf`

SHA-256: `d2df7d51e7dc6e9d16f0eb063142cb39212e32148b729041d318489bbbe9dc9b`.
Use ELF PT_LOAD mappings for reads; data-segment file offsets differ from VAs.

| Finding | Evidence |
| --- | --- |
| CurrentPosition fallback reaches map street text | `sub_178218` publishes the current-position string to DPID `0x04000059` when TurnToInfo is absent; no character-count truncation in this path |
| Street text widget | `sub_4A2A18`, especially `0x4a3fec–0x4a40e0`: `SV_LVDS_FPK_NavMap.NavStatus.Street.PriorityLayoutContainer.TurnToInfo` uses `Nav_Map_Streetname` |
| Grid column width | `0x4a3868` binds the **center column** to `Nav_FPK_NavStatusBar_width`, DPID `0x04000051`; descriptor `0xa9016c` points to `0xa86188`, whose signed 16-bit value is 359. This is not the outer Street widget's width |
| Font-size binding | Label model at view base + `0x2190`; font-size DPID at model +244 is `0x06400d47` |
| Font-size value | `sub_751898` initializes invariant integer indices 2800..3541 from signed 16-bit values starting at VA 10795256. Index `0xd47` resolves to `0xa4bda6`, value 27 |
| Font-style binding | Model +252 uses `0x06400af3`, invariant value 0; `sub_63CCFC` reads that style without a stage-dependent substitution |
| Primary font | `sub_147138` resolves style index 0 in list DPID `0x06400ec9` to `P0640050f`, the invariant string `AudiTypeDisplay-Normal.ttf` |
| Single-line truncation | Defaults at `0x878af8`: maximumLineCount at +24 = 1; truncation byte at +56 = 1; useAltTextIfTooLong at +57 = 0. Property mapping verified through `sub_65ECB0`, `sub_644654`, `sub_654428`, `sub_6543A8` |
| Renderer configuration | `sub_74579C` calls EAL `setMaximumLineCount`, `setTruncation`, `setBasicFontGroup` and `setLineExtender`; IDB renamed `VC_AbstractLabel_InitTextLayout` |

The container also contains distance-to-maneuver, separator and signpost labels.
The distance label has a visibility behavior testing `Nav_Map_TurnToInfo_available`
(`0x0400005a`): `sub_6107EC` -> `sub_611A84` -> model `visibleByModel` setter
`sub_654830`. Its behavior object is constructed at `0xd5d488` (view + `0x1fa0`)
by `sub_2D6964`/`sub_610F24`; `0x4a3fb4–0x4a3fd8` attaches it to the distance
widget. With our empty TurnToInfo, the map-status updater sets this flag to zero,
so that distance label is hidden in this branch. The separator behavior
`sub_610718` / `sub_611900` requires both TurnToInfo and SignPost availability.
The same behavior is attached to SignPostLabel at `0x4a42a0–0x4a42cc`, so it is
hidden too. Thus our CurrentPosition has no competing visible label in this
container, even if an older SignPost string remains in the data pool.

## Why the budget is the same in both View modes

`BAPBridge.startCustomRenderer()` sets `forceGfxAvailable(true)` after the custom
renderer reports a ready frame, explicitly selecting the VC MAP/LVDS path.
Fullscreen/smallscreen changes its stage/viewport, not the CurrentPosition
widget or the following fixed grid-column definition.

`sub_4A2A18` builds the Street grid at view base + `0x19f0`:

- Column descriptor at view + `0x1a28`: three columns. Sizes at `0xd96180` are
  `(-1, 0), (359, 0x04000051), (-1, 0)`; each pair is `(value, DPID)`.
- Growth weights at `0xd94b78` and shrink weights at `0xd975a8` are both
  `[1.0, 0.0, 1.0]`. The center column has zero weight in either direction.
- `sub_749860` reads the size/DP pairs. `sub_749218` explicitly sets preferred,
  minimum and maximum to the resolved size for a nonnegative fixed column.
  `sub_74A330` redistributes space according to the growth/shrink weights.
- The single 104-byte child constraint assembled at `0x4a3c00–0x4a3cf0` places
  PriorityLayoutContainer in column 1, row 0, with a one-column span. Its width
  adjustment is zero. `sub_7494EC`, `sub_7495AC` and `sub_6B4E34` assign its bounds.
- PriorityLayoutContainer's flags are `[2, 1, 2, 1]` for distance, street,
  separator and signpost. `sub_6B6498` preserves flag-2 widths and shares the
  remainder among visible flag-1 children. With only the street text present,
  its width is `min(preferredWidth, 359)`. The layout adds no inter-child gap.
- Both PriorityLayoutContainer and the text label have zero horizontal margins.
  `sub_6CAA04` calculates `max(0, widgetWidth - leftMargin - rightMargin)`.
  `sub_746570` passes that content width to EAL `setMaximumSize`.

The corresponding KZB `navStatus_anchor_center` is a position/opacity anchor;
the C++ grid above independently fixes the text budget. Making the video area
wider does not grant this native label additional columns or a larger font.

## Font resolution

`sub_147138` (`VC_FontGroup_GetAudiFilename` in the annotated IDB) reads list
`0x06400ec9`. Its invariant descriptor at `0x8b66ac` has the serialized list at
`0xab1e38`: nine references, starting with `P0640050f`, `P06400509`. Index 0
therefore selects Normal; index 1 selects Bold. High Normal is at index 6.
`sub_14721C` prepares the font/fallback job, `sub_1477B4` adds those font files,
and `sub_147BA0` returns the font group by the unchanged style index.

`sub_746570` activates this group before setting text. The font-size DP resolves
to 27; the language-specific integer override ranges do not include either
the size or style index. Latin, Cyrillic and `‹›` are present in this Audi face.
The supplementary-font choice for `●` and `◌` is not established here; measuring
them as the Audi font's missing-glyph box would give a misleading width.

## Offline width examples, not a vehicle acceptance result

Pillow `ImageFont.truetype(..., 27).getlength(text)` with the extracted
`assets/vc-fonts/fonts/AudiTypeDisplay-Normal.ttf` gives:

Font SHA-256:
`4e0f6fb1d522df648c7430b3a345f24c7f85af1e3b7ba82a07b55c316f036b5e`.

| Text | Characters | Advance, px |
| --- | ---: | ---: |
| `W` | 1 | 27.922 |
| `i` | 1 | 7.391 |
| `Ш` | 1 | 29.641 |
| `Main Street` | 11 | 146.83 |
| `North Pennsylvania Avenue` | 25 | 343.81 |
| `‹North Pennsylvania Avenue›` | 27 | 364.56 |
| `Ленинградский проспект` | 22 | 326.92 |
| `улица Большая Дмитровка` | 23 | 347.86 |
| `‹улица Большая Дмитровка›` | 25 | 368.61 |
| `улица Большая Дмитровка, 1` | 26 | 380.30 |

By these host metrics, 359 px fits 12 repeated `W` or 48 repeated `i`. Ordinary
street names often fit around 20–26 characters, but there is no reliable count
limit. The `‹›` pair alone uses about **20.75 px**, leaving about **338.25 px**
for the signpost content. Measure the complete formatted string, not just its
payload, and keep the independent 96-byte UTF-8 guard.

Before changing production formatting, validate just-fitting/just-overflowing
strings with the real EAL rasterizer or on VC, including the actual fallback
font for `●`/`◌`. No HU connection, restart, process termination or deployment
was performed during this audit.
