# SHARC input-to-output matrix

Date: 2026-09-02  
Scope: static analysis of amplifier `8W0035465`, SW 0420, V850 CodeFlash. No binary was modified.

> **Critical correction Sep20:** [SHARC consumer audit](SHARC_MATRIX_CONTROL_2026-09-20.md)
> proves RAM/A07 rows are OUTPUTS, columns INPUTS. The input/output interpretation
> and routing conclusions in this historical text are superseded. The numeric table
> below is raw RAM order (read row as output, column as input); the linked CSV and
> extractor have been corrected. Cold-start selection is not proof of live state.

> **Application correction, 2026-09-08:** the same CodeFlash also embeds the
> complete SHARC SPI boot/application stream at `0x041014..0x068777`. The
> checked extraction, hashes, loader proof and reproduction command are in
> [`SHARC_APPLICATION_EXTRACTION.md`](SHARC_APPLICATION_EXTRACTION.md).

## Result

The amplifier image does contain the SHARC input mixer matrix. It is not SHARC
instruction code: the V850 selects an 8×8 signed-dB table from `AD301A07`, converts
every entry to a linear gain, and writes the resulting 64 coefficients to SHARC
parameter addresses `0xC022A..0xC0269`.

For the cold-start default state and six-channel MOST sink `0x26`, the selected matrix
is profile 0, bank 1:

| input → output | Bus L | Bus R | Bus C | Bus LFE | Bus Ls | Bus Rs | Bus 7 | Bus 8 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| L | -2 | -100 | -6 | -100 | -100 | -100 | -100 | -100 |
| R | -100 | -2 | -6 | -100 | -100 | -100 | -100 | -100 |
| C | -8 | -8 | -3 | -100 | -100 | -100 | -100 | -100 |
| LFE | -8 | -8 | -10 | +3 | -14 | -14 | -100 | -100 |
| Ls | -3 | -100 | -100 | -100 | +6 | -100 | -100 | -100 |
| Rs | -100 | -3 | -100 | -100 | -100 | +6 | -100 | -100 |
| Ch7 | -3 | -100 | -100 | -100 | +9 | -100 | -100 | -100 |
| Ch8 | -100 | -3 | -100 | -100 | -100 | +9 | -100 | -100 |

All values are dB. Stored `-100` is passed through a clamp in `FUN_000a9040`, so its
effective value is approximately -99 dB rather than a literal zero coefficient. The
logical bus names are inferred from the diagonal and the 7.1 bank; the numeric input,
output and SHARC parameter positions are proven by code.

This table is already enough to reject one proposed explanation of the car measurement.
It does **not** mute C, LFE, Ls or Rs:

- C reaches Bus L/R/C at -8/-8/-3 dB;
- LFE reaches Bus L/R/C/LFE/Ls/Rs at -8/-8/-10/+3/-14/-14 dB;
- Ls reaches Bus L and Bus Ls at -3/+6 dB;
- Rs reaches Bus R and Bus Rs at -3/+6 dB.

Even if only the physical paths fed by Bus L and Bus R worked, one-hot C and LFE would
still be audible at -8 dB, and Ls/Rs at -3 dB. Therefore the static active matrix does
not resemble the vehicle result in which only source L/R were audible. This is a clean
negative, not proof of where the loss occurs.

## Exact storage and selection

Source image:

```text
file:   CodeFlash_24082026_172027.bin
size:   1,572,864 bytes
SHA256: 8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b
ECU:    8W0035465, SW 0420, HW H11, EV_AMPMst16C2Gen2ALPI
```

Relevant `AD301A07` layout:

| File offset | Size | Meaning |
|---:|---:|---|
| `0x100000` | 8 | signature `AD301A07` |
| `0x100008` | 42 | 2×21 signed profile-selector table |
| `0x100034` | `8 * 0x370` | eight non-matrix configuration profiles |
| `0x101BB4` | `8 * 0xC0` | eight profile matrix sets |
| profile set + `0x00` | `0x40` | bank 0, stereo |
| profile set + `0x40` | `0x40` | bank 1, 5.1 |
| profile set + `0x80` | `0x40` | bank 2, 7.1 |

`FUN_000C23C0` performs the selection and copies the chosen matrix to RAM
`gp-0x3E20`:

```text
profile  = int8(A07[0x100008 + state_a + 21*state_b])
raw_bank = table_0x95314[(SinkNr - 0x21) * 4]
bank     = normalize(raw_bank, ASAD)
matrix   = A07[0x101BB4 + profile*0xC0 + bank*0x40]
```

The ASAD term was recovered completely on 2026-09-04. Raw banks 0 and 1 remain
0 and 1. Raw bank 2 becomes bank 0 for `ASAD=0xc0`, bank 1 when ASAD is not
`0xc0` but `(ASAD & 3)==0` (including `0xfc`), and otherwise remains bank 2.
Raw values 3 and above become bank 0. This qualification does not change the
target result: sink `0x26` has raw bank 1, so both stereo and 5.1 ASAD select
bank 1. The live poll/reload proof is in
[`AMP_STEREO_5_1_MATRIX_DIFF.md`](AMP_STEREO_5_1_MATRIX_DIFF.md).

The two selector rows are identical:

```text
[-1, 0, 1, -1, -1, -1, -1, -1, -1, -1, -1,
 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1]
```

Startup initializes `state_b` (`gp-0x4592`) to 0 and `state_a` (`gp-0x456A`) to 1,
selecting profile 0. State value 2 selects profile 1; other listed values select the
built-in fallback. Profiles 2..7 are present in A07 but cannot be selected by this
two-row table as shipped.

The fallback 5.1 bank is direct for the first six inputs (`L→Bus L`, `R→Bus R`,
`C→Bus C`, `LFE→Bus LFE`, `Ls→Bus Ls`, `Rs→Bus Rs`, all 0 dB). Profile 1 also
routes every one of those inputs. Consequently every valid selector outcome available
to the shipped table preserves source positions 3..6; the clean negative does not depend
on assuming that the runtime state remains at its cold-start value.

The current sink index is stored as `SinkNr - 0x21` at `gp-0x3D7C`. The four-byte
records at `0x95314` select these banks:

| SinkNr | Declared role | raw bank | effective matrix bank |
|---:|---|---:|---:|
| `0x21` | stereo | 0 | 0 |
| `0x22..0x24` | mono/announcement family | 3 | 0 |
| `0x25` | six-position family | 1 | 1 |
| `0x26` | protected 5.1 / `DTCP51` | 1 | 1 |
| `0x27` | stereo family | 0 | 0 |
| `0x28..0x29` | eight-position family | 2 | 0, 1 or 2, ASAD-dependent |
| `0x2A` | stereo family | 0 | 0 |
| `0x2B..0x2C` | six-position family | 1 | 1 |
| `0x2D..0x30` | mono/announcement family | 3 | 0 |
| `0x31..0x32` | eight-position family | 2 | 0, 1 or 2, ASAD-dependent |

Raw bank values 3 and above fall back to bank 0. Bank 2 falls back to bank 1
for six-position ASAD, or to bank 0 for stereo ASAD. Bank 1 is not remapped, so
sink `0x26` selects the 5.1 matrix unconditionally once it is the active sink.

## Proof of matrix orientation and SHARC load

Parameter ID `0x0F` dispatches through `FUN_000A76B4` to `FUN_000A9040`.
`FUN_000A9040` emits four packets, two output columns per packet. For each output it
reads:

```text
RAM[gp-0x3E20 + input*8 + output]
```

It multiplies the signed byte by ten (tenths of a dB), clamps the range to approximately
`[-99 dB, +20 dB]`, converts dB to a linear coefficient through the lookup table at
`0x69399`, and sends eight consecutive input coefficients for that output. This proves
that A07/RAM rows are inputs and columns are outputs.

| Output column | SHARC parameter words |
|---:|---:|
| 1 | `0xC022A..0xC0231` |
| 2 | `0xC0232..0xC0239` |
| 3 | `0xC023A..0xC0241` |
| 4 | `0xC0242..0xC0249` |
| 5 | `0xC024A..0xC0251` |
| 6 | `0xC0252..0xC0259` |
| 7 | `0xC025A..0xC0261` |
| 8 | `0xC0262..0xC0269` |

A bank refresh also schedules parameter group `0x30`. Its handler writes SHARC
control `0x10` from the three-float table at `0x6EAA0`: bank 0 writes `2.0`,
while banks 1 and 2 write `5.0`. This is a proven additional DSP-mode difference,
but its semantic name is absent from the image; it must not be labelled a
Symphoria/upmix switch without runtime or SHARC-code evidence.

The matrix also has an existing internal message-handler read/write surface. `FUN_000D394E` writes
eight bytes to row `(row_number-1)` and queues parameter `0x0F`; `FUN_000D39B6` reads
the same eight-byte row. The outer MOST/diagnostic identifier that reaches these handlers
has not yet been recovered, so this proves the operation but not a ready-to-send service
request. Once that registration is mapped, it is a better runtime confirmation route than
patching the SHARC image.

## Other useful banks

The profile-0 7.1 matrix (bank 2) is identical to the 5.1 matrix except for two direct
routes:

```text
Ch7 -> Bus 7 = 0 dB
Ch8 -> Bus 8 = 0 dB
```

Profile 1 has the same topology with small gain changes: C→Bus C is -2 dB,
LFE→Bus LFE is +4 dB, Ls/Rs direct gains are +5 dB, and Ch7/Ch8 folds are +10 dB.
The built-in fallback and complete 8-profile × 3-bank × 8×8 coefficient set are generated in
[`SHARC_INPUT_OUTPUT_MATRICES.csv`](SHARC_INPUT_OUTPUT_MATRICES.csv). Reproduce it with:

```sh
python3 tools/extract_amp_sharc_matrix.py /path/to/CodeFlash_24082026_172027.bin \
  > docs-obsidian/Sources/SHARC_INPUT_OUTPUT_MATRICES.csv
```

## What remains unknown

This is the logical source-to-eight-bus mixer, not the final mapping to the amplifier's
16 power-stage channels. The V850 code separately sends:

- 16-channel output/filter and limiter parameter vectors;
- a 20-entry gain/ramp vector at SHARC parameter `0xE0584`;
- ten 512-coefficient FIR blocks from A07/B01-related tuning data.

None is an 8×16 source-routing matrix. No second matrix-shaped coefficient region exists
in A07. The final bus-to-speaker split (crossovers, duplicated drivers and physical
channel binding) is therefore expected inside the now-extracted SHARC application or
must be observed through its runtime control/readback path. The application is available;
its SHARC ISA/VISA payload has not yet been semantically disassembled.

The next static step is to map the extracted loader blocks to their declared SHARC
addresses and recover the eight-bus-to-16-output graph. The decisive runtime measurement
remains invoking the existing row-read handler or capturing V850→SHARC writes to
`0xC022A..0xC0269` during an active `0x26` connection. If they match profile 0/bank 1
(or the equally non-muting profile 1/fallback), the one-hot silence occurs before this
matrix receives source positions 3..6 or after its logical buses, not in this matrix.

## Corrected earlier assumptions

`FUN_000A334C -> FUN_000A705A -> FUN_000A6FF2 -> 0xFFFF0008` is the UDS
RequestDownload/reflash path. It does not DMA A07 or B01 wholesale into SHARC. Runtime
audio code reads A07 directly and sends individual parameter groups to SHARC. Earlier
notes calling that chain a monolithic DSP-image DMA, claiming the profile selector is
not consumed by DSP service code, or claiming A07 contains no routing matrix are
superseded by this audit. Separately, `FUN_000A75CA -> FUN_000E33AC` sends the embedded
SHARC loader at `0x041014` with exact length `0x27764`; this application path must not be
conflated with either A07/B01 or the UDS reflash chain.

## Provenance

- Amplifier binary and prior notes:
  `/Users/luka/Desktop/AUDI_2/Firmwares/SoundAmp/`
- Ghidra project created for the V850 image:
  `/Users/luka/Desktop/AUDI_2/Firmwares/SoundAmp/AMP_SHARC_MATRIX.gpr`
  Key functions are saved there as `SelectA07ProfileAndMatrix` (`0xC23C0`),
  `WriteSharcInputMatrix8x8` (`0xA9040`),
  `WriteInputMatrixRowFromMessage` (`0xD394E`),
  `ReadInputMatrixRowToMessage` (`0xD39B6`) and
  `HandleUdsRequestDownloadProgramming` (`0xA334C`).
- Head-unit selection audit: [`MC_SELECTION_AUDIT.md`](MC_SELECTION_AUDIT.md)
- Vehicle and rendering state: [`AUDIO_5_1_STATE.md`](AUDIO_5_1_STATE.md)
- SHARC loader/application extraction:
  [`SHARC_APPLICATION_EXTRACTION.md`](SHARC_APPLICATION_EXTRACTION.md)
