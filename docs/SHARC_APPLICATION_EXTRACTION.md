# J525 embedded SHARC application extraction

Date: 2026-09-08
Scope: amplifier `8W0035465`, SW 0420, HW H11. No firmware was modified.

## Result

The V850 CodeFlash **does contain the complete ADSP-2148x SPI
boot/application stream**. It is embedded at file offsets
`0x041014..0x068777`, inclusive, and is `0x27764` (161,636) bytes long.

Two checked artifacts are stored in [`assets/soundamp`](../assets/soundamp):

| File | Meaning | SHA-256 |
|---|---|---|
| `J525_8W0035465_SW0420_SHARC_SPI_raw.ldr.bin` | canonical byte-for-byte slice sent by V850 over SPI | `659f55e3b57f7cde8ddeb170a5a58e7a985fa4e43ea6898838d4393753e89154` |
| `J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin` | derived copy with the bits in every byte reversed for loader-header analysis | `9169c24f0337c6bfd3646e2538a567bca839b24aa0db5f67b6984943c2286ca7` |

The **raw** file is the authoritative extraction. The `bitrev` file is not a
second firmware and must not be flashed in place of the raw stream. It only
normalizes the SPI slave-boot representation used by this build.

This corrects the earlier conclusion that the SHARC program had to reside in
an undumped external boot flash. `AD301A07` at `0x100000` and `AD301B01` at
`0x120000` are still V850-consumed parameter/tuning datasets; neither should be
disassembled as the application. The application is the separate embedded
loader stream at `0x041014`.

## V850 loader proof

The boot path is visible directly in the V850 program:

- `FUN_000A75CA` passes source `0x00041014` and length `0x00027764` to
  `FUN_000E33AC`;
- `FUN_000E33AC` transfers the source through `FUN_000AD156` in `0x180`-byte
  chunks;
- after the first `0x600` bytes it performs the boot-kernel delay, then sends
  the remaining loader blocks and final IVT;
- `0x41014 + 0x27764 = 0x68778`, so the last included byte is `0x68777`.

The data at `0x68778` and later is a separate V850 parameter/pointer table and
is deliberately excluded.

## Loader-format validation

ADSP-2148x SPI slave boot uses a `0x600`-byte boot kernel followed by blocks
whose headers are three 32-bit fields: tag, count, and destination address.
For this stream the bytes on the V850/SPI side are bit-reversed within each
byte. Reversing them makes the official header structure parse without gaps or
trailing ambiguity:

- initial boot kernel: `+0x000000..+0x0005FF`;
- first header at `+0x600`: `INIT_L48`, count `5`, address `0x0008C100`;
- second header at `+0x630`: `ZERO_L48`, count `0x17`, address `0x0008C105`;
- third header at `+0x63C`: `INIT_L48`, count `0xC6`, address `0x00091090`;
- 560 blocks total: 1 `FINAL_INIT`, 279 `ZERO_LDATA`, 1 `ZERO_L48`,
  37 `INIT_L16`, 240 `INIT_L32`, and 2 `INIT_L48`;
- terminal `FINAL_INIT` header at `+0x27158`, followed by exactly the final
  `0x600`-byte interrupt-vector table;
- the parse ends exactly at `+0x27764`.

The block tags and final-IVT layout follow the Analog Devices
[CrossCore Embedded Studio Loader and Utilities Manual](https://www.analog.com/media/en/dsp-documentation/software-manuals/cces-loaderutilities-manual.pdf).
Analog Devices also documents the `0x600`-byte kernel and per-byte bit reversal
for ADSP-2148x SPI slave boot in its
[ADSP-21489 SPI boot guidance](https://ez.analog.com/dsp/sharc-processors/f/q-a/61485/sharc-21489-spi-master-boot-problem).

## Reproduce and verify

From the repository root:

```sh
python3 tools/extract_amp_sharc_app.py \
  /Users/luka/Desktop/AUDI_2/Firmwares/SoundAmp/uPD70f3479_8W0035465_0420_H11/CodeFlash_24082026_172027.bin \
  --output-dir assets/soundamp

python3 -m unittest discover -s tests -p 'test_extract_amp_sharc_app.py'
```

The extractor refuses a source with the wrong size or SHA-256, validates the
raw slice hash, reverses it, parses every block, checks the exact tag inventory,
and only then writes the two files.

## Remaining analysis boundary

Extraction is complete; semantic reverse engineering is not. A SHARC
ADSP-2148x ISA/VISA processor module is not currently installed in the local
IDA/Ghidra/radare2 setup. The next static task is to load the normalized block
payloads at their declared SHARC addresses and recover the downstream
eight-bus-to-16-output graph, Symphoria state, crossovers, and physical channel
binding. Hardware chip-off is no longer a prerequisite for that analysis.
