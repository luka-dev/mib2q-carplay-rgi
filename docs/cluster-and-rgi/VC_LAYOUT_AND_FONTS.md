# VC Layout and Fonts

*What the Virtual Cockpit (VC 0379, AU491 A4/A5/Q5 2018) can draw as text and where: the extracted fonts and their glyph coverage, the CurrentPosition label budget (359 px, AudiTypeDisplay-Normal 27), our FctID 19 scrolling, the chosen route-text symbols, the Classic/Sport layout seam, and the layout options file.*

---

## 📋 Why this matters

During CarPlay all route text goes through **FctID 19 CurrentPositionInfo**; FctID 20 TurnToInfo is cleared and FctID 22 stays the absolute ETA (field matrix in [RGI and BAP Bridge](RGI_AND_BAP_BRIDGE.md)). The VC renders that string with its own fonts and its own layout. We control neither a pixel offset nor a system font fallback, so every symbol and every length decision has to be checked against the shipped fonts and the widget's grid. Nothing on the VC was modified; all of this is static RE plus host renders. `[decompile]`

NavSD FctIDs in play: 18 distance/bargraph, 19 main text, 20 next text (we clear it), 21 distance to destination, 22 absolute ETA, 23 maneuver descriptors, 24 lanes, 46 destination, 49 ExitView (EU/NAR number 0, kept for the stock 23/18/49 FSG sync). The OK button toggles text/time; FctID 22 stays ETA. `[code]`

## 📦 The extracted font set

Source firmware `Firmwares/VC/AU_C1_AU491_0379_1310_prod_8S0906961AL_A4-A5-Q5_2018/`. Everything lives in `assets/vc-fonts/` (provenance, SHA-256 and table offsets in `assets/vc-fonts/manifest.json`, audit note `assets/vc-fonts/README.md`). `[code]`

| Path | What |
|---|---|
| `fonts/` | 9 outline fonts: five `AudiTypeDisplay*` faces, `LT_Univers440_88perc_Arabic.ttf`, `SourceHanSansJP-Regular.otf`, `SourceHanSansKR-Regular.otf`, `ZYHei_GB18030_c.ttf`; plus `arial.font`/`.ftgz` (a separate FONT bitmap format) |
| `lookup-resources/*.lur` | The VC's per-locale font lookup tables (China, Japan, Korea, EU-Arabic) |
| `lookup-ranges.json` | Decoded `.lur` ranges |
| `catalogs/` | Unicode → glyph tables for the Audi and Arabic faces (the ~24 MB CJK catalogs were dropped as regenerable with `fontTools getBestCmap()`) |
| `previews/` | 59 PNGs: every encoded glyph of the five Audi faces + symbol candidates (FreeType/Pillow, **not** VC screenshots) |
| `render_icon_candidates.py`, `render_circle_candidates.py`, `clock_followup.py` + `{icon,circle,clock}-*.json` | Candidate renders and the selection record |

Coverage (best-cmap entries): Audi faces 674/674/671/671/669 with 668 code points common to all five; `LT_Univers440` Arabic 854; SourceHanSansJP 16,697; SourceHanSansKR 22,888; ZYHei GB18030 28,522. Main fonts sit in `stage2_fs/STAGE2.1/data/gtf/` and `STAGE2.2/data/gtf/`; `AudiTypeDisplayHigh-Normal_08.ttf` in `stage3_fs/data/gem/fonts/`. `AudiTypeDisplayHigh-Normal.ttf` SHA-256 `dc859c10b86b8629fbc1d426ecd2b7cade58a11deb2e015d17a23df8ca518c19` (cmap offset 2456, length 4314). `[decompile]`

Dead ends worth remembering:

- No clock/watch/hourglass code point exists in any of the nine outline fonts. `U+F8FF` in the Audi face draws the **four Audi rings** (glyph name `apple`), not a clock.
- The gzip-wrapped Arial bitmap (557 code points, 9206×34 px at 4 bpp, decompressed SHA-256 `8308e59073430a99899f5d14bb8028a31fc3fe4c14295b2112391501e7a62273`) has no clock either.
- Real stopwatch **images** exist in `atlas_full_LA.atlas` (`lap_timer/intervalWatch.png` UID 221, `lap_timer/stopwatch.png` UID 223, 32×34; atlas SHA-256 `36dcaf425079dbd1a10e32b8e01b50b847d0c2d0f62168afa97678be706ebeb5`). Atlas UIDs are not Unicode; sending a UID as a character does not select the sprite.
- `.lur` range membership is **not** glyph coverage and not proof of UI font binding.

Reproduce: `python3 tools/vc_font_audit.py <firmware-root> <new-output>` (fontTools 4.63.0, Pillow 12.1.0); it copies fonts, records hashes/offsets, exports cmaps and renders exact-font specimens without OS fallback.

Sources: `VC_FONTS`, `VC_FONT_GLYPHS`

## 📌 Chosen route-text symbols

| Purpose | Text | Code point | In the fonts |
|---|---|---|---|
| SignPost / ExitInfo | `‹Exit 12›` | U+2039 / U+203A | All five Audi faces |
| Next street or maneuver description when ExitInfo is absent | `● Main Street` | U+25CF BLACK CIRCLE | JP/KR and Chinese only |
| Small-view time | `◌ 37 min` | U+25CC DOTTED CIRCLE | Arabic and JP/KR only |
| Full-view time | `◌ 22:42 \| 37 min` | U+25CC | same; arrival follows the HU 12/24 h setting |

- `◌` is the standalone dotted circle (bytes `E2 97 8C`), not U+20DD combining enclosing circle and not an emoji sequence. `●` was chosen by Luka on 2026-09-13 for the next-street and maneuver-description fallbacks only; the current road stays unprefixed. `[code]` (`java_patch/com/luka/carplay/rgd/BAPBridge.java:68-74`)
- Byte budget of the 96-byte FctID 19 text: `‹›` costs 6 bytes (90 left), `● ` costs 4 (92 left), plain current road gets all 96; the placeholder `ROUTE_TEXT_PENDING` is a lone `…`. `[code]`
- Whether the VC navigation widget actually falls back to the JP/KR/Chinese face for `●`/`◌` is **not verified**; measuring them with the Audi font's missing-glyph box would be misleading. `[open]`
- Luka saw no route text on the HUD in his tests; this presentation targets the VC only and leaves HUD maneuvers/distances unchanged. Do not generalize that to every HUD text mode. `[car]`

Sources: `VC_FONT_GLYPHS`, `VC_FONTS`

## 📊 The CurrentPosition label budget

In our MAP/LVDS presentation, `SV_LVDS_FPK_NavMap.NavStatus.Street` gives CurrentPosition **at most 359 px** of content width, **AudiTypeDisplay-Normal.ttf at size 27**, one line with truncation on, in **both fullscreen and smallscreen**. Short text gets its preferred width; longer text is cut at 359 px by the VC itself. There is no useful character-count limit. `[decompile]`

Analysed ELF: `<workspace>/Patches/UiModVC/A5_2018_unpacked/hmi_app.elf`, SHA-256 `d2df7d51e7dc6e9d16f0eb063142cb39212e32148b729041d318489bbbe9dc9b` (read through PT_LOAD mappings; data-segment file offsets differ from VAs).

| Finding | Evidence |
|---|---|
| CurrentPosition reaches the map street text | `sub_178218` publishes it to DPID `0x04000059` when TurnToInfo is absent; no char-count cut |
| Street widget | `sub_4A2A18`, `0x4a3fec–0x4a40e0`: `…PriorityLayoutContainer.TurnToInfo` uses `Nav_Map_Streetname` |
| Grid column | `0x4a3868` binds the **centre** column to `Nav_FPK_NavStatusBar_width` DPID `0x04000051`; descriptor `0xa9016c` → `0xa86188` = signed16 **359** |
| Font size | label model at view + `0x2190`; size DPID at model +244 = `0x06400d47`; `sub_751898` initializes invariant indices 2800…3541 from VA 10795256; index `0xd47` → `0xa4bda6` = **27** |
| Font style | model +252 = `0x06400af3`, invariant 0; `sub_63CCFC` has no stage-dependent substitution |
| Face | `sub_147138` (`VC_FontGroup_GetAudiFilename`) resolves style 0 in list `0x06400ec9` (descriptor `0x8b66ac`, list at `0xab1e38`, 9 refs) → `P0640050f` = `AudiTypeDisplay-Normal.ttf`; index 1 Bold, index 6 High Normal |
| Truncation | defaults at `0x878af8`: maximumLineCount (+24) = 1, truncation (+56) = 1, useAltTextIfTooLong (+57) = 0; `sub_74579C` (`VC_AbstractLabel_InitTextLayout`) calls EAL `setMaximumLineCount`, `setTruncation`, `setBasicFontGroup`, `setLineExtender` |

Why both View modes are equal: the Street grid (view + `0x19f0`, column descriptor view + `0x1a28`) has three columns sized `(-1,0), (359, 0x04000051), (-1,0)` (`0xd96180`) with growth and shrink weights `[1.0, 0.0, 1.0]` (`0xd94b78`, `0xd975a8`), so the centre column never grows or shrinks. `sub_749860`/`sub_749218` fix preferred = min = max for a non-negative column; `sub_74A330` redistributes by weights. The single child (`0x4a3c00–0x4a3cf0`) sits in column 1 with zero width adjustment. `PriorityLayoutContainer` flags `[2,1,2,1]` (distance, street, separator, signpost): `sub_6B6498` keeps flag-2 widths and shares the rest among visible flag-1 children, no gap; both container and label have zero margins (`sub_6CAA04`), and `sub_746570` passes the width to EAL `setMaximumSize`. A wider video area does not widen this label. The KZB `navStatus_anchor_center` is only a position/opacity anchor. `[decompile]`

No competing label: the distance label's visibility behaviour tests `Nav_Map_TurnToInfo_available` (`0x0400005a`, chain `sub_6107EC → sub_611A84 → sub_654830`, behaviour object at `0xd5d488` = view + `0x1fa0`, attached at `0x4a3fb4–0x4a3fd8`); with our empty TurnToInfo it is hidden. The separator/SignPost behaviour (`sub_610718`/`sub_611900`, attached at `0x4a42a0–0x4a42cc`) needs both TurnToInfo and SignPost, so it is hidden too, even if an old SignPost string stays in the data pool. `[decompile]`

Host widths with Pillow at size 27 (font SHA-256 `4e0f6fb1d522df648c7430b3a345f24c7f85af1e3b7ba82a07b55c316f036b5e`), estimates only: `[model]`

| Text | Chars | Advance px |
|---|---:|---:|
| `W` / `i` / `Ш` | 1 | 27.922 / 7.391 / 29.641 |
| `Main Street` | 11 | 146.83 |
| `North Pennsylvania Avenue` | 25 | 343.81 |
| `‹North Pennsylvania Avenue›` | 27 | 364.56 |
| `Ленинградский проспект` | 22 | 326.92 |
| `улица Большая Дмитровка` | 23 | 347.86 |
| `‹улица Большая Дмитровка›` | 25 | 368.61 |
| `улица Большая Дмитровка, 1` | 26 | 380.30 |

359 px ≈ 12 × `W` or 48 × `i`; ordinary names fit around 20–26 characters; `‹›` alone costs ≈ 20.75 px. Measure the whole formatted string and keep the independent 96-byte guard. The earlier "High Normal" measurements used the wrong face and are superseded.

The source audit reasoned that `startCustomRenderer()` selects the MAP/LVDS path by `forceGfxAvailable(true)`. That call is now a no-op on this FPK cluster (it returns unless the cluster map is MOST), so the label budget rests on the firmware grid alone. `[code]` (`BAPBridge.java:2266-2276`)

Sources: `VC_CURRENT_POSITION_TEXT_FIT`

## 🔄 FctID 19 scrolling

`updateCurrentPositionInfo(String)` has no pixel offset, so long text is published as **overlapping fragments** (text steps, each still centred by the VC), not a continuous marquee. Implemented 2026-09-17; vehicle calibration pending. `[code]`, `[open]`

| Rule | Value |
|---|---|
| Frame budget | 353 px (`(359 − 6) × 64` in 1/64 px, 6 px uncalibrated EAL rounding reserve) and 96 UTF-8 bytes (`java_patch/com/luka/carplay/rgd/CurrentPositionScroll.java:6-7`) |
| Latin/Cyrillic/CJK | advance at grapheme boundaries, ~24 px/s (`:148`), at most 4 changed publications/s (`MIN_STEP_MS = 250`) |
| Holds | first/last frame ≥ 1800 ms (`HOLD_MS`); no blank frame at the loop point |
| RTL / contextual scripts | overlapping pages, word boundaries when ≥ 2/3 of the fragment is kept, ≥ 1800 ms per page; RLM/LRM (3 bytes, budgeted) keeps the source paragraph direction; source never reversed |
| Fits already | publish once, no deadline; View refresh or recovery may republish |
| One cluster too wide | static decorated `…` + a warning, no split, no spin |
| Missing glyphs | measured with the largest `.notdef` advance, sent intact (VC may show boxes), logged |
| Send failure | fragment kept, retry after 500 ms; late wakeups never replay a backlog; clock jumps back or > 60 s forward rebase the hold |

- Input keeps the complete received string (the old early 96-byte cut is gone); `‹›` stays on every signpost fragment; prefixes are budgeted separately. Text is normalized to **NFC with embedded Unicode 17.0.0 data** (independent of the old JRE), lone surrogates → U+FFFD, and segmented into extended grapheme clusters (combining, Hangul, Indic conjuncts, ZWJ emoji, regional-indicator pairs), never cut inside a cluster.
- **Scheduling:** an O(n) plan (offsets + dwell, prefix sums, no per-frame strings; NFC reordering by stable counting sort) is built only when source or decorations change. A tick builds at most one ≤ 96-byte fragment and sends FctID 19 only if it differs; no segmentation or measurement on ticks; no FctID 20/22, icon, renderer packet or full BAP sync from a tick. The serialized `RouteGuidance.presentationLoop` owns the timer (no new thread); `RouteGuidance.this` is never taken while holding `presentationLock`.
- **Restarts:** new route/maneuver identity, changed text/decorations, explicit View refresh or info-phase change restart at the first fragment; distance changes and duplicate text keep the plan and its hold. Stop/disconnect cancels; renderer loss suspends and cached replay restarts on recovery; retired worker generations (including one blocked on the route monitor) cannot publish; queued View/OK and readiness work runs before a due tick; the first fragment always gets its full hold even if FRAME_READY arrives early.
- **OK = temporary Time mode:** 20 s after a successful publication the worker restores the latest exit/next/current text and restarts the first hold, without any new RGI packet (`INFO_TIME_HOLD_MS = 20000`, `java_patch/com/luka/carplay/rgd/RouteGuidance.java:64`, `:659-661`). A second OK returns immediately; View, route end, disconnect and stop cancel it; route deltas and replays do not extend it. View/OK retries the complete FctID 20/19/22 refresh after 500 ms on any failure, reading the latest requested phase. `[code]`

### Metric data inside the JAR

`tools/generate_vc_text_data.py` reads five faces offline (AudiTypeDisplay-Normal at 27 px, LT_Univers440 Arabic, ZYHei GB18030, SourceHanSans KR/JP). Characters that the four Normal `.lur` profiles consistently map to Audi slot 0 use Audi's advance; others use the largest bound among possible faces. Positive kerning, contextual single substitutions (Arabic forms) and ligature expansion are included; negative kerning is ignored so a frame is never under-budgeted; an orphan combining mark reserves a dotted-circle base. These are **conservative advance estimates**, not a shaper. `[model]`

Output `java_resources/com/luka/carplay/rgd/vc-text.bin`, format **VCT2** (column-wise delta/varint), 33,928 bytes raw / 12,859 in the JAR (was 87,360 / 34,487): 1,531 advance ranges, 2,705 Unicode property ranges, 2,081 decompositions, 961 compositions; no font outlines. Runtime arrays ≈ 58,224 bytes, three 21-bit fields packed per `long`, decoded once lazily. Inputs and hashes in `tools/vc-text-data/manifest.json`; Unicode licence in `META-INF/UNICODE-LICENSE.txt`. No fonts, AWT, ICU or HarfBuzz on the HU; only the JAR changes. `[code]`

Validation (host): all 766 GraphemeBreakTest cases, 20,034 NormalizationTest rows, packed-format round trip of 7,278 records (canonical SHA-256 `7f11283d92a1ec869641553789e78acf8e55f08dea778c009513719fedbee1b4`, corrupt inputs fail explicitly), 191 multilingual fragments through the real MU1316 `AppConnectorNavi` + `CurrentPositionInfo_Status` serializer within the 97-byte wire limit (96 + length), 5,000 RAQM-shaped strings bounded by the generated advances (1/64 px quantization), `RouteInfoTimeoutTest` for the 20 s deadline. The integration audit reproduced and fixed three bugs: lost View/OK retry, publication by a retired blocked worker, and a queued OK phase surviving route end. Regenerate with `python3 tools/generate_vc_text_data.py --download --check`, `python3 tests/test_vc_text_metrics.py`, `VC_UNICODE_TEST_DIR=… bash scripts/test_route_info.sh`, `bash scripts/audit_java_stock.sh`. `[model]`

Next on-car checks: cutoff near 353/359 px, Latin/Cyrillic accents, CJK fallback, Arabic and mixed-direction page order, centring jitter, holds and update rate. Tune the reserve and timing only from that evidence. Devanagari names mostly lack glyphs in the extracted set; scrolling cannot add fonts. `[open]`

Sources: `VC_CURRENT_POSITION_SCROLL_DESIGN`, `VC_CURRENT_POSITION_TEXT_FIT`

## ⚙️ Classic/Sport layout seam

The VC design (Classic or Sport skin) and the CarPlay fullscreen/compact View state are **independent axes**; never infer one from the other. `[decompile]`

- **VC side** (`…/stage2_fs/STAGE2.1/bin/gtf2`): `0x18F4B4` handles data-pool message `0x0100004E` (`INTERN_Designated_LayoutVariant_Choice`) and passes the raw variant to `sub_12D208`, which maps raw 0 → design 0 and raw 256 → design 1 (other values → unknown fallback); `0x190124` publishes the confirmed change; labels Classic `0xA5104F`, Sport `0xA51082`; `isSportDesign@0x176740` reads GTF condition values 4, 5, 14, 15, 16 = Sport (a VC template condition, not an HU API).
- **HU side:** `DSICarKombi.DCDISPLAYPRESETSTYPE_CLASSIC = 2` is real, but `DCDISPLAYPRESETSTYPE_DRIVINGPROFILE = 3` is a drive-profile preset class, **not** Sport. The authoritative value is `LastmodeStorage.getSkin()`: `DisplayManagerEvo.isSportSkin(skin)` is true only for 1, KDK setup picks Sport assets for 1 and Classic for 0, and `DSICarTimeUnitsLanguageManager.updateSkin()` writes live changes there.
- **Our code:** `AltScreenModule` reads `getSkin()` on start and in its 250 ms connected-context reconcile (1 → Sport, 0 → Classic, anything else keeps the geometry), without a second DSI activator or a CarKombi listener override. `compact sport|classic` in the layout options file overrides the result (below). `[code]` (`java_patch/com/luka/carplay/core/AltScreenModule.java`, `refreshClusterStyleFromStock` / `applyClusterStyle`)

The View button stays a logical fullscreen/compact choice; the skin picks the compact placement. All three view areas keep the full encoded/visible frame `(0,0) 1440×542`, and the same resolved area goes to iOS (`updateViewArea`) and to the local loader. `[code]` (`hook/altscreen/altscreen_hook.c:260-270`, `:454-470`)

| Index | Name | Safe area in code |
|---:|---|---|
| 0 | fullscreen | `(270,90) 900×290` |
| 1 | sportscreen | `(90,90) 360×300` |
| 2 | classicscreen | `(540,90) 360×300` |

The source note listed `900×362`, `(90,135) 360×317` and `(540,135) 360×317`. Those were cut back after the 2026-09-19 calibration-grid photos showed Maps anchoring bottom text under the VC's own translucent map bar (≈ y 390 down) and clock bar. The code values are current. See [Cluster Contexts](CLUSTER_CONTEXTS.md) and [AltScreen Cluster Video](../altscreen-and-video/ALTSCREEN_CLUSTER_VIDEO.md) for how the areas are used.

Sources: `VC_LAYOUT_STYLE`

<a id="layout-options"></a>

## 🎛️ Layout options (`/mnt/app/carplay_layout.cfg`)

One small text file changes the layout without a rebuild. No file means the built-in behaviour above, byte for byte (the hook's URLs and areas, and the Java decisions, are covered by tests for that case). It is written by the `layout_MoreIncredibleBash` card from the card's own `mod/carplay_layout.cfg`, which ships with every line commented and explains every key; running the card with that template removes the file again. The uninstall card removes it, the logging card copies it into `config/`. `[code]`

| Key | Values | Read by | Applies |
|---|---|---|---|
| `full x y w h` | safe area of view area 0 | hook (`hook/altscreen/cluster_layout.h`) | next phone connection |
| `sport x y w h` | safe area of view area 1 (compact, Sport) | hook | next phone connection |
| `classic x y w h` | safe area of view area 2 (compact, Classic) | hook | next phone connection |
| `cards right\|left\|center` | the phone's turn card + ETA column (`maneuverLayout` rightAligned / leftAligned / topAligned) when RGI is off | hook | next phone connection |
| `eta yes\|no` | `showETA` in that scene | hook | next phone connection |
| `car_profile auto\|off` | use the per-car safe areas (below) or ignore them | hook | next phone connection |
| `speed_limit yes\|no` | `showSpeedLimit` in both scenes (whether Maps draws the sign here is [open], see [CarPlay speed limit](CARPLAY_SPEED_LIMIT.md)) | hook | next phone connection |
| `compact auto\|sport\|classic` | which compact area the phone gets: the skin, or forced | Java (`ClusterLayoutConfig`) | within 2 s |
| `view_source auto\|choice\|vc` | where full/compact comes from: see below | Java | within 2 s |

**Why only at connection.** CarPlay has no live safe-area command: the receiver declares `viewAreas[]` with nested safe areas during display negotiation and `updateViewArea` only selects one of those indices (confirmed independently by harman-f, Allemon, joeyQuery TRACE-013 and the aberbic issue #5 review). The hook re-reads the file (and the car profile) on every cluster display build (each `/info`) and logs the result once per change: `layout cfg …, car profile …: full=(…)[car|cfg] … rejected=N first_bad_line=L` in `/tmp/carplay_hook.log`. The visible frame stays the full 1440×542 panel; nothing in the file can change it (CarKit rejects the stream otherwise). `[code]`

**Validation.** Each line stands alone; a bad line is ignored and that setting keeps its built-in value. A rectangle must lie inside 1440×542, be at least 165 wide (CarPlaySupport's card platter is `clamp(safeW, 165, 300)`) and 203 high (ETA needs > 202), and at least 290 high when narrower than 498 px (otherwise CarPlaySupport picks "Undefined layout (0)" and third-party apps show no cluster content, see [Third-party Nav Apps](THIRD_PARTY_NAV_APPS.md)). The card refuses unknown keys, wrong field counts, non-numbers and unknown words before installing anything. `[code]` (`tests/altscreen_cluster_layout_test.c`, `scripts/test_layout_mib.sh`)

**`view_source`.** The View button state used to come only from `NAV_VIEW_SIZE_CHOICE`. During CarPlay that model can stay silent or frozen ([KDK Geometry and Animation](KDK_GEOMETRY_AND_ANIMATION.md), reported on a US P5087 B9 and on a Q7 4M), while the cockpit still sends FctID 54 `largeMapView` at the midpoint of every stage change. `auto` (default) uses both and the one that reported last wins; at the connect-time seed a known FctID 54 stage beats the possibly stale choice model. `choice` is the old choice-only behaviour; `vc` uses FctID 54 once it has arrived. The decision line is `viewArea send mode=… reason=VC_FCT54 | NAV_VIEW_SIZE_CHOICE | NAV_VIEW_SIZE_CHOICE-initial | LAYOUT_CFG` in `/tmp/carplay_java.log`. `[code]` (`tests/ClusterLayoutOptionsTest.java`)

**Car profile (Q7).** The Q7 firmware has its own `LayoutMIB2HighQ7` / `Q7Sport` classes ([KDK Geometry and Animation](KDK_GEOMETRY_AND_ANIMATION.md)), so the A5-measured areas above may not fit its gauges. `VehicleLayoutProfile` (`java_patch/com/luka/carplay/cluster/`) sees the live Layout class through `ClusterLayerController.updateLayout()` and, for the two Q7 classes, writes `/tmp/carplay_layout.vehicle` with the Q7 rectangles below; for every other class (B9 = the A5 this was measured on) it removes the file. The hook applies built-in < car profile < `/mnt/app/carplay_layout.cfg` per area, and `car_profile off` in that file skips the profile so both sets can be compared on one car (`cl_layout_build`; log tags `[car]` / `[cfg]`). Java logs `car profile=q7 layout=…` once. `[code]` (`tests/ClusterLayoutOptionsTest.java`, `tests/altscreen_cluster_layout_test.c`)

Where the Q7 values come from: ganm-afk's review in aberbic issue #5 uses them on a Q7 4M with a retrofitted SQ7 cockpit (1440×540 canvas). The closed commercial package for A4/A5/Q5/Q7 on P5092 declares the identical three rectangles for every car (checked in its compiled library), so treat them as one source rather than an independent Q7 measurement; which car they were measured on is not stated. `[open]` on our car

| Area | aberbic #5 review (Q7 4M) | Built-in (A5 B9) |
|---|---|---|
| full | `360 87 720 297` | `270 90 900 290` |
| sport | `114 88 350 296` | `90 90 360 300` |
| classic | `520 87 400 297` | `540 90 360 300` |

**Testing the hypothesis: the car's own map layout.** The stock navigation places its own cockpit map with list model 176 (row 1, read by `LayoutProviderG22Kombi` and `GUIKombiFPK`): column 0/1 screen width/height, 8 info line at the bottom, 9 top (tab) line, 11/12 left/right gauge-tube distance, 13/14 the same for the "Kb" variant, 24/25 map offset, 26/27 map size. `getVisibleArea()` is the screen minus those margins: x = tubeLeft − mapOffsetLeft, y = topLine − mapOffsetTop, w = width − tubeLeft − tubeRight, h = height − topLine − infoLine. That table is filled at run time, per car and skin, so it cannot be read from the firmware files. `AltScreenCombiBAPListener.logScreenLayout()` writes the whole table, with both derived rectangles, to `/tmp/carplay_java.log` whenever it changes (at start, on FctID 44 and on FctID 54), as a warning line, so no verbose marker is needed: `screen layout list176 reason=… largeMap=… style=… audiVisible normal=(x,y wxh) kb=(x,y wxh) rows=… row0=[…] row1=[…]`. Read only. If the derived rectangles match what is clear on the cockpit, the car profile can be computed from this table for every car instead of hard-coded. `[code]` (`tests/ScreenLayoutLogTest.java`), `[open]` until the first car log

Not ported from that review: its `adjacentViewAreas` naming the other two indices inside every view area. luka's display-level `adjacentViewAreas` plus `[0]` on compact switches works on the A5, and the commercial package sends an empty `adjacentViewAreas` with every `updateViewArea` and none in the view areas, so adjacency is evidently not required on these cockpits; if the Q7 ignores View switches even with `view_source auto`, it is still the next thing to try. The startup loader animation (`altscreen_render/loader/loader_layout.h`) keeps its own built-in placement.

**Licensing.** Allemon's `cluster_viewarea.cfg` / `cluster_shift.cfg` / cluster-layout GEM entries inspired the idea (his repository has no licence file), so nothing was copied: this is a separate design on luka's three predeclared view areas. The Q7 numbers are quoted from a GPL-3.0 source (the aberbic issue #5 review).

## 🔗 Related

- [RGI and BAP Bridge](RGI_AND_BAP_BRIDGE.md): FctID 19/20/22 ownership and the text builder.
- [Distance Bargraph](DISTANCE_BARGRAPH.md): the distance label in the same NavStatus container.
- [Third-party Nav Apps](THIRD_PARTY_NAV_APPS.md): `updateViewArea` and zone switching.

## 📚 Sources

Merged originals were removed from the tree after this note absorbed them; they remain in git history (commit 23972aa).

- `VC_FONTS`: map of `assets/vc-fonts/` and the chosen symbols.
- `VC_FONT_GLYPHS`: glyph coverage, clock dead ends, audit tool.
- `VC_CURRENT_POSITION_TEXT_FIT`: 359 px / size 27 widget RE and host widths.
- `VC_CURRENT_POSITION_SCROLL_DESIGN`: fragment scrolling, VCT2 metrics, OK time mode.
- `VC_LAYOUT_STYLE`: Classic/Sport seam and view areas.
