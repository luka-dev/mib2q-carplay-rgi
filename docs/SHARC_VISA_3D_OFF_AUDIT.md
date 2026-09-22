# J525 VISA handlers and the level-zero / 3D OFF boundary

2026-09-08. Read-only audit of `8W0035465`, SW 0420, HW H11.
No amplifier firmware or production virtual-cabin configuration changed.

Follow-up: [static readiness and Sound Check audit](UPMIX_STATIC_READINESS_AND_SOUNDCHECK.md)
now verifies the MU1316 Java/native mapping OFF=DSI 1 -> HU level 0. The
RCC/MOST-to-J525 field link and actual runtime gates remain separate checks.
It also expands complex-block decoding and supersedes the MMI-enum uncertainty
below, without claiming full end-to-end processing equivalence.

## Result

Three handler roles are now supported by instruction-level evidence, not just
the shapes of their descriptors:

| VISA entry | Validated role | Important consequence |
|---|---|---|
| `0x128910` | Integer-index selector with invalid-index suppression | Selectors are zero-based input indices, not gains |
| `0x12694C` | Two-input-per-output mixer | Inputs are split into two halves; gains are interleaved pairs |
| `0x1284D6` | Variable-count output EQ using the hardware IIR accelerator | Filter records are packed sequentially across channels |

**Control level 0 is not full processing bypass.** With processing enabled and
B01 valid, level 0 selects gain row **1**. Row **0** is selected when either gate
is off. The normal command handler accepts four levels, 0..3, independently of
the processing-enable field. There are **five** gain rows, not three.

The identification **MMI “3D OFF” = control level 0** is a strong inference from
the four-level control, its scalar table and height-branch attenuation. It is
**not yet a directly observed MMI command/readback mapping**. Do not turn that
inference into a claim that the entire Symphoria processor is disabled.

## Evidence and reproducibility

- [Bounded VISA decoder](../tools/inspect_amp_visa.py).
- [VISA listings with byte provenance](../assets/soundamp/visa_handler_evidence.md).
- [V850 control-handler decompilation](../assets/soundamp/visa_state_v850_evidence.md).
- [V850 parameter writers](../assets/soundamp/output_binding_v850_evidence.md).
- [Updated machine-readable audit](../assets/soundamp/output_binding_audit.json).

CodeFlash is raw V850 little-endian, file/load base 0, size `0x180000`, SHA-256
`8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`.
The extracted, per-byte bit-reversed SHARC loader has SHA-256
`9169c24f0337c6bfd3646e2538a567bca839b24aa0db5f67b6984943c2286ca7`.
VISA PCs below count **16-bit short words**, not bytes. Successive instruction
halfwords are MSB-first; each halfword in the normalized loader is little-endian.

```sh
python3 tools/inspect_amp_visa.py \
  assets/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin \
  0x12694c 0x1269bc
python3 -m unittest discover -s tests -p 'test_*amp*.py'
```

There are 54 / 48 / 289 decoded instructions in the mixer / selector / EQ
ranges respectively. All internal direct branch and loop targets land on
instruction boundaries; none of those ranges contains an UNKNOWN operation.
The mixer's loop endpoint `0x12699A` is a live 48-bit NOP from a zero-fill loader
block, not missing code. Runtime initialization at `0x125A2A` establishes
`M5=M13=0`, `M6=M14=1`, `M7=M15=-1`.

The 21 tests cover decoding vectors, range boundaries, routing, coefficient
layout and state-table extraction. They do **not** execute the firmware on an
amplifier or constitute a sample-accurate emulator of its entire audio graph.
The decoder is a bounded research tool, not a complete vendor disassembler.

## 1. Selector `0x128910`

The function receives a descriptor, resolved buffer-pointer vector and block
length. It reads input count at descriptor `+5`, output count at `+7`, and the
selector-array pointer at `+9`.

At `0x128939..0x128943`, a negative index or index >= input count sets the
gain to zero and substitutes index zero before dereferencing the input vector.
A valid index retains unity gain. The sample loop uses SIMD for adjacent
samples, with two-word increments. For normal finite-valued audio:

```text
out[j] = in[selector[j]]       if 0 <= selector[j] < input_count
out[j] = 0                    otherwise
```

More exactly, the invalid case multiplies input zero by zero; it is not a
literal buffer clear. This distinction matters if a buffer contains NaNs.

This confirms both descriptor instances:

- `0xC4DF1`: 10 inputs -> 16 outputs, selector words at `0xC20D0`.
- `0xC4E6A`: 25 inputs -> 16 outputs, selector words at `0xC2114`.

Thus `0xFFFFFFFF` is suppressed as a negative selector; diagnostic indices
16/17 in the second selector select actual auxiliary inputs, not channel 16/17
of the preceding ten-bus stage. The two identical A07 split-mode tables really
do select the same sources, even though other processing can change with level.

## 2. Pair mixer `0x12694C`

`N = descriptor[7]` and `gains = descriptor[9]`. Instructions `0x12696C` and
`0x12696F` establish pointer-vector offsets N and 2*N. At `0x126977..0x12697D`
the function reads input B at offset N, output at offset 2*N, and input A at
offset zero; it advances the vector by one for each output. At
`0x126980..0x126982`, it reads **two successive gains** per output.

```text
pointer vector = [A[0..N-1], B[0..N-1], output[0..N-1]]
output[j][t] = gains[2*j] * A[j][t] + gains[2*j+1] * B[j][t]
```

`0x126971` enables SIMD plus I9 coefficient broadcast. The floating multiplies,
sum and pipelined store are visible at `0x126986..0x126997`. Parallel stores
read the previous result: reading the listing as sequential Python assignments
would give the wrong sample pipeline. No division by two or other normalization
is performed by this kernel; any scaling is in the gains or surrounding stages.

This applies to both `0xC4B4F` (20 -> 10) and `0xC4DFB` (32 -> 16).
It rejects the alternative interpretation `in[2*j]` / `in[2*j+1]`.

At the first mixer, the port IDs are:

| Local output j | A buffer ID | B buffer ID | Output ID | Row 1 A/B stored dB |
|---:|---:|---:|---:|---|
| 0 | 38 | 20 | 24 | 0 / 0 |
| 1 | 39 | 19 | 23 | 0 / 0 |
| 2 | 52 | 97 | 22 | +9 / -100 |
| 3 | 53 | 97 | 21 | +9 / -100 |
| 4 | 54 | 18 | 36 | +4 / 0 |
| 5 | 55 | 17 | 37 | +4 / 0 |
| 6 | 56 | 34 | 50 | -6 / 0 |
| 7 | 57 | 35 | 51 | -6 / 0 |
| 8 | 97 | 40 | 58 | -100 / -99 |
| 9 | 97 | 41 | 59 | -100 / -99 |

These are profile-0 targets **before** the ASAD-dependent correction discussed
below. Buffer IDs are local to this schedule position and can be reused later.
The table does not assign them directly to MOST channels or physical speakers.

## 3. Packed output EQ `0x1284D6`

This is an IIR-accelerator wrapper, not just a scalar software biquad loop.
Its writes to `0x5200` and `0x521C`, and polling of `0x5201`, target the IIR
control, chain-pointer and DMA-status registers. Their identities and the
coefficient format are specified in the [ADI hardware reference](https://www.analog.com/media/en/dsp-documentation/processor-manuals/adsp-214xx_hwr_rev1.1.pdf),
sections 7-66 and B-24/B-25 (PDF pages 430, 1240/1241).

Descriptor fields used by this instance:

| Offset | Meaning established by accesses | Value |
|---:|---|---|
| +5 | Channel count | 16 |
| +12 | Coefficient-record capacity | 144 |
| +13 | Per-channel stage counts | `0xC2124` |
| +14 | Per-channel accelerator subtask counts | `0xC2134` |
| +15 | Transfer-control blocks | `0xC2144` |
| +16 | Packed accelerator audio scratch | `0xC22B0` |
| +17 | Five-word coefficient bank | `0xC2AB0` |
| +18 | Fourteen-word accelerator coefficient/state bank | `0xC2D80` |
| +19 | Five-word working coefficient bank | `0xC3560` |

`0x128523..0x12852F` sums the channel counts and compares against capacity;
an excessive sum takes the bypass path. For a valid layout, setup iterates
channel counts in order. Each count is divided into groups of 12 plus a
remainder. The coefficient pointer advances by 168 words per full group at
`0x1285B8`, and by 14*remainder at `0x1285DB..0x1285E6`.

For count zero, `0x128732` builds a separate one-stage identity task using
`0xB95CE`; it does **not** advance the packed coefficient pointer. That identity
record starts with `0x80000000, 0x3F`, representing 1.0 in the split 40-bit
format, and its remaining coefficient/state words are zero.

`0x125D17` converts each five-word record in source order **2,3,0,4,1**,
placing each float in two words of the accelerator format and reserving four
more words for state. This establishes the correspondence:

```text
V850 five-word record:       [-a1, -a2, b0, b1, b2]    (normalized)
accelerator seven values:    [b0, b1, -a1, b2, -a2, d2, d1]
storage per accelerator stage: 14 words
```

The counts `[17,17,0,0,4,4,10,10,5,5,12,12,6,17,12,12]` therefore consume
records 0..142 sequentially, leaving capacity record 143 unused by these
channel chains. This confirms the output-grouping interpretation in the
[binding audit](SHARC_OUTPUT_BINDING_AUDIT.md). It does not independently
validate the different input-EQ handler, physical T38 labels or cabin acoustics.
Coefficient updates use a working bank and a smoothing helper; exact transition
timing and accelerator numerical equivalence have not been emulated.

## 4. Control state: why row 0 is not the normal OFF candidate

The normal V850 handler `0xCA4B8` handles independent fields:

- Field 3 changes the processing-enable request at `gp-0x4568`.
- Field 4 accepts 0..3 and copies the level to `gp-0x418F` when changed.
  Raw instructions `0xCA684..0xCA6C0` independently verify that range/write.
- Field 5 changes surround adjustment; `gp-0x3DE0` is clamped to -9..+9.

The effective processing gate `gp-0x456B` is derived separately; source/profile
conditions can disable it even when a level is selected. The B01-valid gate is
`gp-0x455C`. `0xA711C`, `0xAA648`, and `0xAC9C4` agree on the selection rule:

```text
if effective_processing_gate == 0xA5 and B01_valid == 0xA5:
    row = level + 1
else:
    row = 0
```

The development/tuning handler `0xD3A38` corroborates the separate numbering:
its row 0 command clears the enable request; row k>0 enables processing and
sets level=k-1. It also writes row gains, so it is **not** a safe read-only
method to discover the user's state. No such commands were sent.

`0xAA38C` sends level-dependent parameter `0x13` through `0xAC64A` using the
float table at CodeFlash `0x6EA68`: **[-100, -50, 0, +100]**. These are control
parameter values; their units must not be assumed to be output dB.

| Condition | Gain row | Profile-0 row address | Level parameter |
|---|---:|---|---:|
| Processing gate off or B01 invalid | 0 | `0x10033C` | Separate mechanism |
| Enabled, level 0; strongest 3D OFF candidate | 1 | `0x100350` | -100 |
| Enabled, level 1 | 2 | `0x100364` | -50 |
| Enabled, level 2 | 3 | `0x100378` | 0 |
| Enabled, level 3 | 4 | `0x10038C` | +100 |

Rows 2/3/4 share these mixer gains, but the level parameter differs. Equal gain
rows therefore do **not** imply equal spatial processing. Profile 1 uses the
same structure, with -4 dB instead of -6 dB in the A gains of local outputs 6/7.

The gain lookup has another non-obvious distinction:

- Stored -100 dB -> table address `0x6A338` -> **exact float zero**.
- Stored -99 dB -> `0x6A310` -> **0.000011220185115234926**, not zero.

`0xAA648` can further change odd-index/B gains: with an enabled row and
ASAD `0xC0`, it adds the surround correction and clamps to -990..+200 tenths
of a dB. This can promote a -100 dB sentinel to the finite -99 dB floor.
Its transition state can also ramp current gain bytes toward the target or
temporarily suppress odd gains. A static row is a **steady target**, not a
complete snapshot of a transition. `0xAC9C4` also selects an EQ bank by the same
row, so reproducing just the 20 gains would omit another state-dependent path.

## What virtual cabin should take from this

For a future implementation, represent **level**, **effective enable gate**,
**B01 validity**, **surround adjustment**, **ASAD** and **transition state**
separately. Do not encode all of them as one `symphoria_3d_enabled=false` flag.
For the normal OFF candidate use level 0 with the gates active, not row 0;
retain the base B-path mix and distinguish the mute sentinel from the -99 dB
floor. Apply the two-half mixer and the verified packed output EQ in schedule
order. Keep the two physical rear-door drivers on their shared electrical path.

Before calling this an exact model of the user's **MMI 3D OFF** state, the
remaining decisive check is an existing MMI-side enum/command definition or a
read-only capture showing the field-4 value and gates when OFF is selected.
The complete complex-processor transfer and physical output-to-T38 numbering
also remain unresolved. No firmware flashing or tuning command is required
to establish that correspondence.

Instruction encoding was checked against the [ADI programming reference](https://www.analog.com/media/en/dsp-documentation/processor-manuals/adsp-2136x_2137x_214xx_pgr_rev2.4.pdf),
chapters 10/12 and MODE1 description A-6. Bitfield diagrams were rendered and
inspected; legacy ADSP-2106x disassembly alone is insufficient for these VISA
encodings and newer register-transfer fields.
