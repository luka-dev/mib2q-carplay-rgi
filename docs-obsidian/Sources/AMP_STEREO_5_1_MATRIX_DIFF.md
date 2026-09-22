# Amplifier stereo/5.1 matrix and live-ASAD audit

Date: 2026-09-04

Target image:

```text
/Users/luka/Desktop/AUDI_2/Firmwares/SoundAmp/
  uPD70f3479_8W0035465_0420_H11/CodeFlash_24082026_172027.bin
size:   0x180000 / 1,572,864 bytes
SHA256: 8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b
ISA:    V850 little-endian, image base 0
```

This audit answers whether the amplifier recognizes stereo versus 5.1 from the
SAD/ASAD sideband, whether that distinction changes its input matrix, and what
the recovered stereo and 5.1 matrices actually do. No firmware binary was
modified.

## Result

The amplifier **does receive ASAD dynamically**. It polls the INIC/MOST side,
detects a changed speaker-allocation byte and schedules a complete re-evaluation
of the active 8x8 input matrix.

That does **not** switch the current CarPlay sink `0x26` between stereo and 5.1
banks. Sink `0x26` has raw bank `1`, and raw bank `1` remains the 5.1 bank for
both relevant allocations:

```text
Sink 0x26 + ASAD 0xc0 -> bank 1
Sink 0x26 + ASAD 0xfc -> bank 1
```

ASAD-dependent bank switching exists only for sink records whose raw bank is
`2`. For example, the eight-position sink `0x32` selects bank 0 for stereo
ASAD, bank 1 for six-position ASAD, and bank 2 when the low two ASAD bits make
the eight-position layout active.

Therefore the earlier shorthand “the matrix bank depends only on `SinkNr`” was
too broad globally, but it is correct for the target sink `0x26`.

The bank also selects a separate SHARC control: bank 0 writes IEEE-754 `2.0`,
while banks 1 and 2 write `5.0`. This proves that the amplifier has a DSP mode
difference in addition to the 8x8 coefficients. The image contains no semantic
name for that control, so it cannot honestly be labelled “Symphoria” or
“upmix enable”. On `0x26`, `ASAD=0xc0` and `ASAD=0xfc` both retain the bank-1
value `5.0`.

## The two independent selectors

The current sink is stored as `SinkNr - 0x21` at `gp-0x3D7C`. Its four-byte
record is indexed from `0x95314`:

```text
[raw_matrix_bank, SAD_capability_flag, sample_width, 0]
```

For `SinkNr 0x26`, the record is:

```text
0x95328: 01 a5 10 00
          |  |  |
          |  |  +-- 16-bit profile
          |  +----- SAD-capable; retain allocation received from INIC
          +-------- raw matrix bank 1
```

The complete normalization performed by `SelectA07ProfileAndMatrix @ 0xC23C0`
and the live selector `FUN_000C2774` is:

```text
raw bank 0                         -> bank 0
raw bank 1                         -> bank 1
raw bank 2 + ASAD == 0xc0          -> bank 0
raw bank 2 + ASAD != 0xc0
             and (ASAD & 3) == 0   -> bank 1
raw bank 2 + (ASAD & 3) != 0       -> bank 2
raw bank >= 3                      -> bank 0
```

This yields the intended width-adaptive behavior only on raw-bank-2 profiles:

```text
Sink 0x32 + ASAD 0xc0 -> bank 0 (stereo/fold-down)
Sink 0x32 + ASAD 0xfc -> bank 1 (5.1)
Sink 0x32 + ASAD 0xff -> bank 2 (7.1)
```

The selectable `0x32` profile is eight-channel, 24-bit and has a different
MOST payload contract. It is not a drop-in way to obtain dynamic stereo/5.1
selection on the existing six-channel, 16-bit `0x26` connection.

## Bank-dependent SHARC mode control

`FUN_000C2774` stores the normalized bank in `gp-0x3D90` and schedules parameter
groups `0x2F`, `0x30`, `0x16`, and `0x0F` at `0xC287E..0xC2896`. In the
`FUN_000A76B4` dispatcher (case at `0xA7754`), group `0x30` reaches
`FUN_000AA38C`. Its `0xAA3CC..0xAA3EE` path calls:

```text
FUN_000AC648(0x10, table_0x6EAA0[selected_bank])
```

The exact table is:

```text
0x6EAA0  00 00 00 40    bank 0 -> float 2.0
0x6EAA4  00 00 A0 40    bank 1 -> float 5.0
0x6EAA8  00 00 A0 40    bank 2 -> float 5.0
```

This is the strongest static evidence for a real stereo-versus-multichannel
DSP operating mode. It is still only a numeric SHARC control `0x10`; the
now-extracted SHARC program must be disassembled to give values 2 and 5 their
meaning. See [`SHARC_APPLICATION_EXTRACTION.md`](SHARC_APPLICATION_EXTRACTION.md).

The other groups scheduled with a bank refresh do not reveal a named upmixer:

- `0x2F` writes SHARC control `0x13` from a four-value table selected by a
  sound-system state rather than ASAD or the sink;
- `0x16` updates the 20-entry gain/ramp vector at `0xE0584` and the 8x8
  matrix-related state;
- `0x0F` sends the 8x8 coefficients and 16-output enable/diagnostic vectors at
  `0xC20D0` and `0xC2114`.

The A07 `0x370` configuration profile is not selected separately by the sink,
and B01 has one global 33-frequency coefficient set with no recovered
sink-indexed dispatch. Crossovers and per-output EQ therefore do not visibly
swap as a second stereo/5.1 matrix in the V850-controlled data.

## Proof that ASAD reaches the amplifier live

The only three writers of the saved allocation byte `gp-0x3E21` in the complete
CodeFlash are initialization, Connect handling, and the INIC poll:

1. `FUN_000B64E0 @ 0xB6B82` initializes it to `0xc0`.
2. `FUN_000C4B34 @ 0xC4DB8` forces `0xc0` only when the sink record's SAD flag
   is `0x5a`. The target `0x26` record uses `0xa5`, so Connect does not replace
   its live allocation.
3. `FUN_000D1D94` polls the INIC with request
   `00 07 01 2c d0 71 00 01 00` and consumes the returned allocation:

```asm
D1E4C  ld.bu -0x3c1c[gp],r12     ; returned allocation byte
D1ECA  ld.bu -0x3e21[gp],r18     ; previous ASAD
D1ECE  cmp   r18,r12
D1ED2  st.b  r12,-0x3e21[gp]     ; save changed ASAD
D1ED6  mov   9,r6
D1ED8  mov   15,r7
D1EDA  jarl  0xDD2D4,lp          ; task 9, event 15
```

Task-9 event 15 reaches `FUN_000C2774`, which reads the sink record and ASAD
again, copies the newly selected 64-byte matrix into RAM, and schedules SHARC
parameter group `0x0F`. `WriteSharcInputMatrix8x8 @ 0xA9040` converts its
signed-dB cells to linear gains and writes SHARC parameter words
`0xC022A..0xC0269`.

Thus the amplifier side is already capable of reacting to an allocation change
without another MOST Connect. The present limitation is on the head unit:
deva's metadata devctl only changes persistent state, and stock code emits the
new SAD header on the next CSD playback start.

## Which A07 profile is reachable

The profile selector is the 2x21 signed-byte table at `0x100008`:

```text
profile = int8(A07[0x100008 + state_a + 21*state_b])
state_a = gp-0x456A
state_b = gp-0x4592
```

Both rows are identical:

```text
[-1, 0, 1, -1, -1, -1, -1, -1, -1, -1, -1,
 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1]
```

`state_a=1` selects profile 0, `state_a=2` selects profile 1, and the other
values select the built-in fallback. Profiles 2..7 exist in A07 but are not
reachable through the shipped selector table. The two rows being identical
also means `state_b` does not change the selected profile in this image.

## Stereo bank 0

For the fallback and every reachable A07 profile, bank 0 is the same two-bus
fold-down. Omitted cells are `-100 dB` and are clamped to approximately `-99 dB`
by the writer:

```text
L    -> Bus L   0 dB
R    -> Bus R   0 dB
C    -> Bus L  -6 dB, Bus R -6 dB
LFE  -> Bus L  -6 dB, Bus R -6 dB
Ls   -> Bus L   0 dB
Rs   -> Bus R   0 dB
Ch7  -> Bus L   0 dB
Ch8  -> Bus R   0 dB
```

This is not an L/R-to-surround upmixer. With only L and R present, it produces
only logical Bus L and Bus R. Its purpose is also to fold any non-front input
positions into those two buses.

## 5.1 bank 1

The normal startup selection is profile 0. Its bank 1 routes:

```text
L    -> Bus L  -2 dB, Bus C  -6 dB
R    -> Bus R  -2 dB, Bus C  -6 dB
C    -> Bus L  -8 dB, Bus R  -8 dB, Bus C  -3 dB
LFE  -> Bus L  -8 dB, Bus R  -8 dB, Bus C -10 dB,
        Bus LFE +3 dB, Bus Ls -14 dB, Bus Rs -14 dB
Ls   -> Bus L  -3 dB, Bus Ls +6 dB
Rs   -> Bus R  -3 dB, Bus Rs +6 dB
Ch7  -> Bus L  -3 dB, Bus Ls +9 dB
Ch8  -> Bus R  -3 dB, Bus Rs +9 dB
```

Profile 1 changes only these gains:

```text
C -> Bus C          -2 dB instead of -3 dB
LFE -> Bus LFE      +4 dB instead of +3 dB
Ls/Rs direct        +5 dB instead of +6 dB
Ch7/Ch8 surround   +10 dB instead of +9 dB
```

There are 21 changed cells between bank 0 and bank 1 in each reachable profile.
The full fallback plus all eight stored profiles and all three banks remain in
[`SHARC_INPUT_OUTPUT_MATRICES.csv`](SHARC_INPUT_OUTPUT_MATRICES.csv).

For the hook's exact compatibility/off output `L,R,0,0,0,0`, profile-0 bank 1
therefore produces:

```text
L -> Bus L -2 dB and Bus C -6 dB
R -> Bus R -2 dB and Bus C -6 dB
```

It creates no direct L/R route to the surround buses. Forcing bank 0 would not
restore a recovered surround upmix; it would instead remove the known center
feed and leave only Bus L/R.

## ASAD-dependent coefficient trims

A complete instruction search found 13 accesses to `gp-0x3E21`. Besides the
writers, selectors and status replies, two SHARC writers consume it:

- `FUN_000A9040` conditionally adjusts cells of the 8x8 matrix.
- `FUN_000AA648` conditionally adjusts odd cells of a 20-element gain/ramp
  vector written at SHARC parameter `0xE0584`.

The relevant arrays live in the constant A07 block copied from `0x102438`:

| A07 address | Use | shipped values over its valid `-9..+9` control range |
|---:|---|---|
| `0x102CB0` | multichannel adjustment for output columns 5..8 | all `0.0 dB` |
| `0x102CD6` | exact-`0xc0` adjustment for input rows 5..8 under its additional mode flag | `-9..+9 dB` |
| `0x102C8A` | exact-`0xc0` adjustment for odd cells of the 20-element vector | all `0.0 dB` |

These branches prove ASAD-aware calibration, but not a direct Symphoria enable.
For the compatibility/off stereo PCM, input rows 5..8 are digital zero, so the
only non-zero ASAD-dependent table cannot create surround content from L/R. The
checked-in head-unit canary now supplies its own bounded LFE/Ls/Rs before this
matrix; those channels are renderer output, not content created by ASAD. No other
direct ASAD consumer exists in the V850 image. The separate numeric SHARC control
`0x10` follows the normalized bank, not ASAD independently.

## Consequence for APAC/AAC switching

The fixed six-slot PCM container remains correct:

```text
APAC 5.1              -> measured L,R,C,LFE,Ls,Rs reorder
AAC stereo, upmix off -> L,R,0,0,0,0
AAC stereo, canary    -> 0.958L,0.958R,0,LFE-V2(phase-safe LP48),bounded Ls,bounded Rs
```

What is not semantically equivalent is announcing `ASAD=0xfc` for either stereo
line. It tells the far side that all six positions are active, so the amplifier
cannot learn from the header that the decoded source was stereo. With the canary,
LFE/Ls/Rs really are populated while C is intentionally zero because bank 1
derives the mid/centre bus. The hook preserves valid audio and the known-good
six-channel transport, but it deliberately does not preserve stock stereo source
metadata.

Changing to `ASAD=0xc0` for index 23 would reach the amplifier's live ASAD
logic if the head unit actually emitted it. On sink `0x26` it would still use
bank 1 and SHARC control value `5.0`; it could only exercise the ASAD-aware
calibration or an unobserved downstream SHARC behavior. Static analysis does
not prove that this enables the car's advertised Symphoria upmix. The adopted
shipping policy therefore keeps one `PCM6/48 kHz/S16` stream and `ASAD=0xfc`
for the complete buffered session; only the APAC/AAC decoder changes at an
in-session format boundary.

An allocation-only A/B remains available as a diagnostic using separate playback starts:

1. index 23, six PCM slots, `ASAD=0xfc`;
2. index 23, the same six PCM slots, `ASAD=0xc0`.

This A/B test does **not** enter the separately recovered bank-0/control-`2.0`
mode; it isolates only the effect of the allocation header while retaining the
current connection. It does not change the shipping decision: per-track
stop/reconfigure/start is intentionally avoided, and calling `createMetadata`
while playback continues does not update the head unit's live header.

A test of the true bank-0/control-`2.0` path instead requires a stereo sink such
as `0x21` or `0x2A`, or a raw-bank-2 connection whose bank can follow ASAD.
That changes the MOST sink/width contract and is not a safe codec-hook-only
change. It should not be conflated with updating the SAD header.

## Static boundary

The recovered 8x8 table maps source positions to eight logical SHARC buses. It
does not contain the final bus-to-16-amplifier-output mapping, and no second
8x16 routing matrix is present in A07/B01. Any richer stereo expansion after
Bus L/R, including Symphoria, remains inside the extracted but not yet
semantically disassembled SHARC application or runtime state. It must not be
claimed from the fold-down matrix above.

## Related records

- [`SHARC_INPUT_OUTPUT_MATRIX.md`](SHARC_INPUT_OUTPUT_MATRIX.md) — original
  8x8 extraction and orientation proof.
- [`DYNAMIC_BUFFERED_CODEC_SWITCH.md`](DYNAMIC_BUFFERED_CODEC_SWITCH.md) —
  cookie-driven APAP codec switching and head-unit SAD update lifetime.
- [`SAD_HEADER.md`](SAD_HEADER.md) — the repaired HU-side speaker allocation.
- [`MC_SELECTION_AUDIT.md`](MC_SELECTION_AUDIT.md) — why connection 162 selects
  the external-amplifier multichannel pair.
