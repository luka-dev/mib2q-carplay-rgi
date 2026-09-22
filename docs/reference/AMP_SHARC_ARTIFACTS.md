# J525 amp DSP — reverse-engineering artifacts

*`assets/soundamp/` holds the extracted SHARC boot streams, the VISA/V850 disassembly and the per-block evidence behind the amp reverse-engineering.*

---

## 📚 What it is

The static RE working set for the **Audi B&O J525 amplifier** DSP (a V850 MOST/FBlock
control processor + an ADSP-2148x **SHARC** audio DSP). It is the raw material the
Virtual Cabin analysis rests on. Source: amplifier `8W0035465` SW `0420` HW `H11`,
CodeFlash image SHA-256 `8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`
— **not modified**; this is decoding + mathematical reconstruction only. The
hash is the one recorded inside the set (`output_binding_audit.json`,
`complex_v850_evidence.md`); the CodeFlash image itself is not in this repo.

## 🎯 What it's for

This is the **provenance** for the amp side of the upmix — the matrix, biquad math,
the full DSP chain, the two "B" processors, VISA/3D-off, level/matrix control. The
readable conclusions live in the white paper; keep this so those claims can be
re-checked and the SHARC work continued:

- [`../research/VIRTUAL_CABIN.md`](../research/VIRTUAL_CABIN.md) §4 — the amp DSP model.
- [`../SOUNDAMP_FULL_DSP_MODEL.md`](../SOUNDAMP_FULL_DSP_MODEL.md) and the `docs/SHARC_*`
  set — the detailed audits. 16 of those 28 files (the SHARC set plus
  the SOUNDAMP model) cite `assets/soundamp`; the other 12 do not mention it
  (`grep -l assets/soundamp`), so "all repointed here" no longer holds [open].
- `assets/soundamp/README.md` — the extraction's own detailed index.
- Model implementation: `tools/soundamp-virtual-cabin/amp_dsp_chain.py`.

## 📦 What each part is

| Path | What |
|---|---|
| `J525_…_SHARC_SPI_raw.ldr.bin` | the ADSP-2148x SPI slave-boot stream extracted from the V850 CodeFlash (authoritative) |
| `J525_…_SHARC_SPI_bitrev.ldr.bin` | the byte-bit-reversed loader used for all VISA disassembly |
| `*.asm` (395: 215 in `remaining-blocks-20260919/`, 172 in `remaining-blocks-20260920-v3/`, 8 in `second-b-control-20260920/`) | VISA / V850 disassembly, per DSP block (matrix, mixers, EQ, limiters, delays, B processors) |
| `*_evidence.md` (8, top level) | per-block V850/VISA evidence write-ups (complex, VISA, MMI 3D, focus/surround, output binding) |
| `output_binding_audit.json` + `*/…json` | machine audit outputs |
| `complex-*`, `remaining-blocks-*`, `second-b-*`, `wrapper-control-*` (7 subdirs) | the dated per-topic RE working sets |
| `README.md` | the extraction's own index |

## ⚠️ Caveats

Static RE only: **no live amp state was read back, no arithmetic is bit-exact, no
cabin acoustics are modelled.** The bus→physical-speaker binding, per-driver EQ, and
the MMI "3D off" mapping are inferences. The input-matrix orientation correction
(rows = outputs) is baked into the audits here — see [`../SHARC_MATRIX_CONTROL_2026-09-20.md`](../SHARC_MATRIX_CONTROL_2026-09-20.md)
and [`../UPMIX_MATRIX_RERENDER_2026-09-20.md`](../UPMIX_MATRIX_RERENDER_2026-09-20.md); older per-speaker bass summaries from
the transposed reading are withdrawn.
