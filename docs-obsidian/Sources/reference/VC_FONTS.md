# VC fonts (0379) — extracted fonts, glyph tables & symbol selection

*`assets/vc-fonts/` holds the extracted fonts, their glyph catalogs, the rendered symbol candidates, the VC lookup resources and the render/selection logic, kept together so the cluster text work is reproducible.*

---

## 📚 What it is

A static extraction of the **Virtual Cockpit (VC) 0379** font set and everything
needed to reason about *which characters the cluster can actually draw*. Source
firmware: `Firmwares/VC/AU_C1_AU491_0379_1310_prod_8S0906961AL_A4-A5-Q5_2018/`
(A4/A5/Q5 2018 VC). The firmware was **not modified**; this is read-only RE plus
host renders. Provenance (source paths, SHA-256, font table offsets) is in
`assets/vc-fonts/manifest.json`; the original audit note is
`assets/vc-fonts/README.md`.

## 🎯 What it's for

The cluster nav display renders text with **these specific VC fonts** — you cannot
show a glyph a font doesn't contain, and there is no system font fallback we
control. So before choosing any special symbol for the CarPlay cluster HUD
(SignPost/Exit brackets, an ETA/clock marker, direction arrows), we have to know
which code points exist in the shipped fonts and how they render. This extraction
is that ground truth; it feeds the current-position / RouteInfo text design:

- [`VC_FONT_GLYPHS.md`](VC_FONT_GLYPHS.md) — the glyph-coverage summary.
- [`VC_CURRENT_POSITION_TEXT_FIT.md`](VC_CURRENT_POSITION_TEXT_FIT.md) — text-fit
  using the real `AudiTypeDisplay-Normal.ttf` metrics.
- [`VC_CURRENT_POSITION_SCROLL_DESIGN.md`](VC_CURRENT_POSITION_SCROLL_DESIGN.md) —
  scroll design (`--fonts`/`--lookups` point at `assets/vc-fonts/`).

**Chosen selection** (from the audit): SignPost/Exit `‹…›`; time marker =
U+25CC DOTTED CIRCLE `◌` (not `ETA`, not combining U+20DD) — small scene
`◌ 37 min`, big scene `◌ HH:MM | 37 min`. `◌` exists in the Arabic/JP/KR fonts,
absent from the five Audi faces; VC-side fallback for it is **not yet verified**.
[open] The Java JAR was built and its tests pass; **not deployed to the HU.** (Status claim
carried over from the audit; not re-checked against the current build.)

## 📦 What each part is

| Path | What | Keep reason |
|---|---|---|
| `fonts/` | The 9 source outline fonts (5 Audi `AudiTypeDisplay*`, `LT_Univers440` Arabic, SourceHanSans JP/KR, ZYHei GB18030) + `arial.font`/`.ftgz` (a separate `FONT` bitmap format) | the assets |
| `lookup-resources/*.lur` | The VC's own per-locale font lookup resources (China/Japan/Korea/EU-Arabic) | extracted VC data |
| `lookup-ranges.json` | Decoded lookup ranges across the `.lur` set | derived index |
| `catalogs/` | Unicode → glyph-ID tables (TXT + JSON) per font | machine glyph tables |
| `previews/` | 59 PNG: every encoded glyph of the five Audi faces + the SignPost/ETA candidate renders (FreeType/Pillow, **not** VC screenshots — real layout/hinting differs) | the visual evidence |
| `*.py` | The render/selection logic: `render_icon_candidates.py`, `render_circle_candidates.py`, `clock_followup.py` | the logic |
| `{circle,icon,clock}-*.json` | Candidate sets those scripts produced | selection record |
| `manifest.json`, `README.md` | Provenance (source paths, SHA-256, offsets) + audit note | ground truth |

**Font glyph counts** (best-cmap Unicode entries): the five Audi faces carry
669–674 each; `LT_Univers440` Arabic 854; SourceHanSansJP 16 697; SourceHanSansKR
22 888; ZYHei GB18030 28 522 (full table in `README.md`; re-counted with
`fontTools` `getBestCmap()` over `assets/vc-fonts/fonts/`).

## 💡 Note on the dropped CJK catalogs

The three large CJK glyph catalogs
(`SourceHanSans{JP,KR}-Regular.json`, `ZYHei_GB18030_c.json`, and the KR/ZYHei
`.txt`) — ~24 MB of derived Unicode→glyph tables — were **removed** as regenerable
derived data that nothing cites. The **fonts themselves are kept** in `fonts/`, so
the tables can be regenerated with `fontTools` if ever needed, e.g.:

```bash
python3 -c "from fontTools.ttLib import TTFont; import json; \
f=TTFont('assets/vc-fonts/fonts/SourceHanSansKR-Regular.otf'); \
cmap=f.getBestCmap(); \
json.dump({hex(k):v for k,v in cmap.items()}, open('/tmp/khr.json','w'))"
```

The small Audi/Arabic catalogs are kept (they are tiny and directly cited).
