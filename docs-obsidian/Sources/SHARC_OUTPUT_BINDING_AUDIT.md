# J525 downstream output-binding audit

2026-09-08. Read-only analysis of `8W0035465`, SW 0420, HW H11 dump.
No firmware or production virtual-cabin configuration was changed.

Follow-up: [static readiness and Sound Check](UPMIX_STATIC_READINESS_AND_SOUNDCHECK.md)
adds the verified HU 3D enum conversion, complex-block controls and a reproduced
intermediate-S16 headroom limitation. Physical output labels remain inferred.

## Result and proof boundary

The extracted SHARC loader contains a **79-entry processing schedule**, module
descriptors, signal-buffer IDs, and pointers to parameter arrays written by V850.
Two A07 routing tables and inverse V850/DSP channel permutations can be followed
into this structure. Output EQ has 144 record slots and sixteen counts summing
to **143**. Profile-0 eight-bus EQ counts sum to **55** within an 80-slot bank.

Verified: bytes, copy loops, parameter destinations, channel permutations,
table selection, selector semantics, split-half mixer layout, and sequential
output EQ packing. See the follow-up [VISA/state audit](SHARC_VISA_3D_OFF_AUDIT.md).
Candidate speaker names remain inference. Unproven: complete complex-processor
semantics, direct output-hardware-to-T38 binding, and the exact ten-bus transfer
under the user's MMI 3D OFF setting.

## Reproduce

```sh
python3 tools/audit_amp_output_bindings.py \
  /Users/luka/Desktop/AUDI_2/Firmwares/SoundAmp/uPD70f3479_8W0035465_0420_H11/CodeFlash_24082026_172027.bin \
  --output assets/soundamp/output_binding_audit.json
python3 -m unittest discover -s tests -p 'test_audit_amp_output_bindings.py'
```

- [Machine-readable report](../../assets/soundamp/output_binding_audit.json).
- [V850 decompiler evidence](../../assets/soundamp/output_binding_v850_evidence.md).
- [Image extraction and fingerprint](SHARC_APPLICATION_EXTRACTION.md).

Twenty-one extraction/audit/VISA tests passed after the follow-up. These are
static regression checks, not hardware execution or speaker-pinout proof.
Source: raw V850 little-endian, load/file base 0, size `0x180000`, SHA-256
`8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`.
SHARC addresses below are word addresses, not V850 offsets or byte addresses.

## Exact V850 parameter routing

`0xC23C0` copies A07 profile data from `0x100034 + profile*0x370` to
`gp-0x13900`; fixed data from `0x1021B4` to `gp-0x13590`.
`0xB64E0` copies `0x102438..0x103237` to `gp-0x1330C`.

```text
A07[0x102DAD + mode*16] -> 0xA91C6 -> SHARC 0xC20D0 (16 selectors)
A07[0x102DCD]          -> 0xA9328 -> SHARC 0xC2114 (16 selectors, permuted)
```

Both stored modes at `0x102DAD` are identical:
`[8,0,8,0,9,1,9,1,4,5,6,7,2,2,3,3]`.
Normal routing at `0x102DCD` in V850 configuration order:
`[1,5,4,0,2,6,11,10,8,9,13,12,3,7,14,15]`.

`0xA9328` also handles channel-test overrides: diagnostic inputs 16/17 or mute
(`0xFFFFFFFF`). Composition below assumes normal routing, not those overrides.

V850-to-DSP permutation at `0x68B40`:
`[11,10,9,8,4,5,6,7,15,14,13,12,0,1,3,2]`.
`0x68B30` is its exact inverse. Multiple writers/readers use them: `0xA9328`
(routing), `0xA9C94` (delay-like integer vector), `0xAC464` (gain), `0xACAA0`
and `0xA8548` (dynamics), `0xA8238` (monitoring). They are not the ordering of
the previously inferred service-test groups.

## Candidate physical assignments

Indices are **zero-based**. Bus10 is the downstream ten-channel stage, not raw
MOST input. Physical assignments combine numeric routing, candidate passbands
and Audi's 19-driver inventory: **not directly recovered pinout**.

| V850 config | DSP slot | Bus10 source | Candidate speaker(s) | Candidate T38 pins | EQ count |
|---:|---:|---:|---|---|---:|
| 0 | 11 | 0 | R20 + R103, front-left shared mid/treble | 11,12 | 12 |
| 1 | 10 | 1 | R22 + R104, front-right shared mid/treble | 13,14 | 12 |
| 2 | 9 | 9 | R221, upper A-pillar right | 24,32 | 5 |
| 3 | 8 | 8 | R220, upper A-pillar left | 23,31 | 5 |
| 4 | 4 | 8 | R276, dashboard left | 25,33 | 4 |
| 5 | 5 | 9 | R277, dashboard right | 26,34 | 4 |
| 6 | 6 | 7 | R210, right effect | 28,36 | 10 |
| 7 | 7 | 6 | R209, left effect | 27,35 | 10 |
| 8 | 15 | 4 | R14 + R15, rear-left door | 4,5 | 12 |
| 9 | 14 | 5 | R16 + R17, rear-right door | 16,17 | 12 |
| 10 | 13 | 2 | R208, center mid | 6,18 | 17 |
| 11 | 12 | 2 | R219, center tweeter | 3,15 | 6 |
| 12 | 0 | 0 | R21, front-left bass | 7,8 | 17 |
| 13 | 1 | 1 | R23, front-right bass | 9,10 | 17 |
| 14 | 3 | 3 | R211, one winding | 19,20 or 21,22 | 0 |
| 15 | 2 | 3 | R211, other winding | 21,22 or 19,20 | 0 |

The two subwoofer windings cannot be ordered from this evidence. Bus10 8/9 feed
candidate elevated front branches; 6/7 feed candidate effect branches. Neither
pair should be equated with raw MOST channels 7/8. The former service-table-based
physical numbering in the working virtual-cabin JSON remains unmodified and
must not be combined with this new permutation as if both were proven.

## Filter records now have destinations

`0xA96C8` reads eight-byte design records:
`u16 frequency_selector, s16 gain_tenths_db, u8 Q_code, u8 type, u16 reserved`.
It generates coefficients; `0xABF7C` packages five coefficient words.
The lookup in `0xAB7F6` numerically matches `cot(pi*f/48000)`. Inverting it
verifies 62, 100, 240, 255, 870, 1100, 4500 and 6000 Hz to <0.001 Hz.

| Bank | Design records in CodeFlash | SHARC coefficients | Writer | Counts |
|---|---|---|---|---|
| Eight-bus profile EQ | `0x10003C + profile*0x370` | `0xE0264` | `0xA985C` | profile-0 55 / capacity 80 |
| Output EQ | `0x102448` | `0xC2AB0` | `0xA9B04` | 143 / capacity 144 |
| Auxiliary 16-channel EQ | `0x102288` | `0xE069C` | `0xA9A2C` | capacity 48 |

Output counts at `0x102438` go through `0xA95C8` to `0xC2124`, which also sets
descriptor control `0xC4E7D` to 1. Do not assume nine filters on each channel.

Sequential output packing is now validated in VISA `0x1284D6` and converter
`0x125D17`; see the follow-up for IIR accelerator details. Under that packing,
candidate rear-door outputs have HP-family records at **100 Hz**, the center
separates mid/tweeter at **6000 Hz**, and front bass has HP **62 Hz** and LP
**240 Hz** records. These are digital design parameters, not acoustic crossovers.

The rear input buses also have EQ records at 113/138/174 Hz (each stored gain
-8 dB), plus mid/treble correction. One cutoff cannot describe the full rear
response. The LFE input-bus candidate has LP 66 Hz and HP 33 Hz records; zero
output-EQ count does not mean an unfiltered subwoofer.

## SHARC processing structure

The schedule at `0xC5794` contains 79 descriptor pointers and a zero terminator.
Descriptors carry class pointers, port-ID arrays and parameter pointers. Buffer
IDs are reused: tracing must respect schedule order, not globally join equal IDs.

| Descriptor | Structural role / numeric evidence | Candidate VISA process |
|---|---|---|
| `0xC4AED` | 8-in/8-out matrix, `0xC022A` | `0x1269BC` |
| `0xC4B3B` | 8-channel EQ, capacity 80, `0xE0264` | `0x126BAE` |
| `0xC4B4F` | 20-in/10-out, gains `0xE0584` | `0x12694C` |
| `0xC4D17` | complex multichannel stage, meaning not fully decoded | `0x126DB2` |
| `0xC4DF1` | 10-in/16-out selector, `0xC20D0` | `0x128910` |
| `0xC4DFB` | 32-in/16-out mixing, `0xE453A` | `0x12694C` |
| `0xC4E6A` | 25-in/16-out selector, auxiliary inputs, `0xC2114` | `0x128910` |
| `0xC4E74` | 16-channel output EQ, capacity 144, `0xC2AB0` | `0x1284D6` |
| `0xC4EBB` | 16-channel delay structure, `0xE4674` | `0x124517` |
| `0xC4EE3` | 16-channel gain structure, `0xC393A` | `0x12879C` |

V850 parameter writes corroborate these roles; this is not a claim that all
listed VISA functions have been disassembled. B01 is copied by `0xACFB4`:
**3562 words from `0x12000C` to `0xC3C6A`**. The old flat-float view beginning
`0x120018` is not the complete DSP object layout. Its 23-filter scan is not an
exhaustive output-EQ catalog and does not establish speaker routing.

## Remaining decisive work

1. The three targeted VISA roles are now validated; other DSP handlers and
   the complete complex-processor transfer remain to be decoded.
2. Confirm MMI OFF maps to level 0 with the effective gate enabled. The five-row
   selection is decoded: enabled level 0 selects row 1, while row 0 is bypass.
   Do not conflate this with live UI/readback proof.
3. Bind actual serial/power-stage outputs to T38 with hardware/diagnostic
   evidence. Use the physical candidates as research leads, not OEM truth.
4. Add verified electronic paths to virtual cabin. Speaker and seat acoustic
   responses remain separate unknowns.

Architecture reference: Analog Devices,
[SHARC programming reference](https://www.analog.com/media/en/dsp-documentation/processor-manuals/adsp-2136x_2137x_214xx_pgr_rev2.4.pdf).
It distinguishes normal-word ISA and short-word VISA operation; blindly decoding
short-word payloads as ordinary 48-bit instructions is not a valid shortcut.
