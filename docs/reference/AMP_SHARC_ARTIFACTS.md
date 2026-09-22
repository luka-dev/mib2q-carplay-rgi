# J525 amp DSP — reverse-engineering artifacts

**Home:** `assets/soundamp/` — the extracted SHARC boot streams, the VISA/V850
disassembly, and the per-block evidence behind the amp reverse-engineering.

## What it is

The static RE working set for the **Audi B&O J525 amplifier** DSP (a V850 MOST/FBlock
control processor + an ADSP-2148x **SHARC** audio DSP). It is the raw material the
Virtual Cabin analysis rests on. Source: amplifier `8W0035465` SW `0420` HW `H11`,
CodeFlash image SHA-256 `8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`
— **not modified**; this is decoding + mathematical reconstruction only.

## What it's for

This is the **provenance** for the amp side of the upmix — the matrix, biquad math,
the full DSP chain, the two "B" processors, VISA/3D-off, level/matrix control. The
readable conclusions live in the white paper; keep this so those claims can be
re-checked and the SHARC work continued:

- [`../research/VIRTUAL_CABIN.md`](../research/VIRTUAL_CABIN.md) §4 — the amp DSP model.
- `docs/SOUNDAMP_FULL_DSP_MODEL.md`, the `docs/SHARC_*` set — the detailed audits
  (all repointed here).
- `assets/soundamp/README.md` — the extraction's own detailed index.
- Model implementation: `tools/soundamp-virtual-cabin/amp_dsp_chain.py`.

## What each part is

| Path | What |
|---|---|
| `J525_…_SHARC_SPI_raw.ldr.bin` | the ADSP-2148x SPI slave-boot stream extracted from the V850 CodeFlash (authoritative) |
| `J525_…_SHARC_SPI_bitrev.ldr.bin` | the byte-bit-reversed loader used for all VISA disassembly |
| `*.asm` (395) | VISA / V850 disassembly, per DSP block (matrix, mixers, EQ, limiters, delays, B processors) |
| `*_evidence.md` (9) | per-block V850/VISA evidence write-ups (complex, VISA, MMI 3D, focus/surround, output binding) |
| `output_binding_audit.json` + `*/…json` | machine audit outputs |
| `complex-*`, `remaining-blocks-*`, `second-b-*`, `wrapper-control-*` (subdirs) | the dated per-topic RE working sets |
| `README.md` | the extraction's own index |

## Caveats

Static RE only: **no live amp state was read back, no arithmetic is bit-exact, no
cabin acoustics are modelled.** The bus→physical-speaker binding, per-driver EQ, and
the MMI "3D off" mapping are inferences. The input-matrix orientation correction
(rows = outputs) is baked into the audits here — see `docs/SHARC_MATRIX_CONTROL_2026-09-20.md`
and `docs/UPMIX_MATRIX_RERENDER_2026-09-20.md`; older per-speaker bass summaries from
the transposed reading are withdrawn.
