# RE Artifacts

*Where the reverse-engineering working sets live now: the J525 amp SHARC extraction under `assets/soundamp`, and the Virtual Cockpit font extraction under `assets/vc-fonts` — kept so the claims that rest on them can be re-checked.*

---

## 📚 What lives where

Two static reverse-engineering working sets are kept in the repo as provenance. Both are read-only decodes of unmodified firmware — no live state was read back — and both underpin conclusions documented elsewhere in the vault.

| Home | Subject | Source firmware | Feeds |
|---|---|---|---|
| `assets/soundamp/` | Audi B&O **J525 amplifier** DSP RE | amp `8W0035465` SW `0420` HW `H11` CodeFlash | [[Virtual Cabin and Amp RE]], the upmix model |
| `assets/vc-fonts/` | **Virtual Cockpit (VC) 0379** font set | A4/A5/Q5 2018 VC (`8S0906961AL`) | cluster nav HUD text design |

## 🔍 assets/soundamp — the amp DSP RE

The static RE working set for the J525 amplifier: a **V850** MOST/FBlock control processor plus an ADSP-2148x **SHARC** audio DSP. It is the raw material the Virtual Cabin analysis rests on — the matrix, the biquad math, the full DSP chain, the two "B" processors, and VISA/3D-off level/matrix control. Source CodeFlash image SHA-256 `8776fdb8…41fe4b`, **not modified**; this is decoding and mathematical reconstruction only.

| Path | What |
|---|---|
| `J525_…_SHARC_SPI_raw.ldr.bin` | the ADSP-2148x SPI slave-boot stream extracted from the V850 CodeFlash (authoritative) |
| `J525_…_SHARC_SPI_bitrev.ldr.bin` | the byte-bit-reversed loader used for all VISA disassembly |
| `*.asm` (395) | VISA / V850 disassembly, per DSP block (matrix, mixers, EQ, limiters, delays, B processors) |
| `*_evidence.md` (9) | per-block V850/VISA evidence write-ups |
| `output_binding_audit.json` + subdir JSON | machine audit outputs |
| `complex-*`, `remaining-blocks-*`, `second-b-*`, `wrapper-control-*` | dated per-topic RE working sets |
| `README.md` | the extraction's own index |

**Caveat:** static RE only — no live amp state, no bit-exact arithmetic, no cabin acoustics modelled. The bus→speaker binding, per-driver EQ, and the MMI "3D off" mapping are inferences. The input-matrix orientation correction (rows = outputs) is baked into the audits here; older per-speaker bass summaries from the transposed reading are withdrawn.

## 🔍 assets/vc-fonts — the VC font extraction

A static extraction of the VC 0379 font set and everything needed to reason about **which characters the cluster can actually draw**. The cluster nav display renders text with these specific fonts — you cannot show a glyph a font lacks, and there is no system-font fallback we control. So before choosing any special symbol for the CarPlay cluster HUD (SignPost/Exit brackets, an ETA/clock marker, arrows), this extraction is the ground truth. Firmware **not modified**; read-only RE plus host renders. Provenance (source paths, SHA-256, table offsets) is `assets/vc-fonts/manifest.json`.

| Path | What |
|---|---|
| `fonts/` | the 9 source outline fonts (5 Audi `AudiTypeDisplay*`, `LT_Univers440` Arabic, SourceHanSans JP/KR, ZYHei GB18030) + `arial.font`/`.ftgz` (a separate `FONT` bitmap format) |
| `lookup-resources/*.lur` | the VC's own per-locale font lookup resources (China/Japan/Korea/EU-Arabic) |
| `lookup-ranges.json` | decoded lookup ranges across the `.lur` set |
| `catalogs/` | Unicode → glyph-ID tables (TXT + JSON) per font |
| `previews/` | 48 PNG: every encoded glyph of the five Audi faces + SignPost/ETA candidate renders (FreeType/Pillow, **not** VC screenshots) |
| `*.py` | render/selection logic (`render_icon_candidates.py`, `render_circle_candidates.py`, `clock_followup.py`) |
| `manifest.json`, `README.md` | provenance + audit note |

**Chosen selection** (from the audit): SignPost/Exit `‹…›`; time marker = U+25CC DOTTED CIRCLE `◌`. `◌` exists in the Arabic/JP/KR fonts but is absent from the five Audi faces, and the VC-side fallback for it is **not yet verified**.

<details>
<summary>Note on the dropped CJK catalogs</summary>

The three large CJK glyph catalogs (`SourceHanSans{JP,KR}`, `ZYHei_GB18030`) — ~24 MB of derived Unicode→glyph tables that nothing cites — were removed as regenerable derived data. The **fonts themselves are kept** in `fonts/`, so the tables regenerate with `fontTools` if needed. The small Audi/Arabic catalogs are kept (tiny and directly cited).

</details>

---

Provenance: `docs/reference/AMP_SHARC_ARTIFACTS.md`, `docs/reference/VC_FONTS.md`, and the `README.md` inside each `assets/` folder.
