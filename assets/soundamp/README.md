# J525 SHARC extraction artifacts

- `J525_8W0035465_SW0420_SHARC_SPI_raw.ldr.bin` is the canonical `0x27764`-byte
  slice transmitted by the V850 to the ADSP-2148x.
- `J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin` is a derived analysis copy
  with the bits of every byte reversed. Do not treat it as an independently
  flashable image.

Exact source offsets, SHA-256 values, loader validation and the reproduction
command are in [`docs-obsidian/Sources/SHARC_APPLICATION_EXTRACTION.md`](../../docs-obsidian/Sources/SHARC_APPLICATION_EXTRACTION.md).
