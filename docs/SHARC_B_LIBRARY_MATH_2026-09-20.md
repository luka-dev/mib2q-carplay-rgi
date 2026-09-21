# First-B log/pow library numerical closure — 2026-09-20

Static/offline only. No vehicle, firmware writes, deployment or default preset
changes. New `amp_b_math.py` supplies bounded numerical references for the
actual library paths used by `BandLevelMaskReference`; it is not a SHARC CPU
or bit-exact40-bit floating-point emulator.

## API and explicit activation

```python
from amp_b_math import BMathReference

math = BMathReference(fix_rounding='nearest')  # explicit MODE1 hypothesis
math.log10(positive_values)
math.pow10(exponents)
math.pow10_trace(exponents)  # anchor/sixteenth splits/reconstruction indices
math.metadata()
```

`fix_rounding` is mandatory and accepts `nearest` or `floor`. It does not
claim either is the live processor setting. `BandLevelMaskReference` accepts
`numeric_mode='firmware_library', fix_rounding='nearest'` to opt in. Its
default remains `numeric_mode='host_math'`; irrelevant rounding overrides in
that mode are rejected. The standalone `level_gain` helper accepts an optional
`math_reference` without changing its existing default.

The numerical domain deliberately excludes firmware exception/errno paths:

- Log: positive normal-float32 range, `2^-126 .. float32.max`.
- `pow10`: finite exponent`[-37,37]`, positive base10 only.
- Real finite scalar or array inputs; shapes preserved.
- No silent fallback, normalization or inferred startup state.

Inputs/intermediates/results use float64 rather than forced firmware store
rounding; constants are decoded from actual stored float32 words. Ideal
division represents the mathematical limit of RECIPS plus Newton refinements.
Seed-ROM error, flags, SHARC40-bit operations, intermediate32-bit stores and
exceptional behavior remain outside the reference.

## Logarithm path and recovered rational expression

`12F90A` prepares `1+smoothed_power`; `12D80B` calls natural log `124393`,
then multiplies by literal word`3EDE5BD9` (approximately1/ln10). The subsequent
caller multiplies by10 for its dB value. No20*log amplitude substitution is made.

For `x = m*2^e`, `m` in`[.5,1)`, `124393` uses constants at`B9393..B939A`:

- Threshold float32(sqrt(.5)), literal.5 and2.
- If `m <= threshold`, decrement`e` and use
  `z = (m-.5)/(.5*(m-.5)+.5)`.
- Otherwise `z = (m-1)/(.5*m+.5)`.
- With `q=z²`, reduced log is
  `z + z*q*(c4*q+c5)/(q+c3)`.
- Add `e*c6` and `e*c7` in the recovered order; those are split ln2 terms.

The helper uses the actual table and literal factor, not NumPy logarithm under
a firmware label. `natural_log` is exposed for testing this stage independently.

## Power path is not exp(log(x))

The caller`12F8CC` supplies base10 to`1257E6`. That routine uses three tables:

| DM range | Role |
| --- | --- |
| `B939B..B93AC` | Anchors corresponding to sixteenth-octave powers |
| `B93AD..B93B5` | Anchor correction terms |
| `B93B6..B93BD` | Reduced-log and exp2 polynomial constants |

For base10 the actual search selects anchor index12, coarse log2 part3.3125
and a small corrected remainder. `125875..125899` performs several scaled
FIX/FLOAT operations, splitting exponent products into sixteenths. The model
preserves those operations and their explicit rounding mode rather than
collapsing them into a single mathematical power. A residual adjustment,
lookup index and binary exponent then drive the stored exp2 polynomial and
SCALB reconstruction. The trace exposes these quantities for independent checks.

The primitive `125DB3` performs reciprocal/refinement division for the initial
reduced-log ratio. It is represented by ideal division, not a fabricated
RECIPS seed or a claim of instruction equivalence.

## Decoder additions verified against the local ADI manual

`docs/papers/sharc_programming_rev2.4.txt`, tables12-3/12-4 and§11-37..39,
defines these previously missing compute operations:

- `0x30`: fixed integer ABS.
- `0xD9`: FIX floating to integer with exponent scale.
- `0xDA`: FLOAT integer with exponent scale.
- `0xDD`: TRUNC floating to integer with exponent scale.

The decoder now recognizes them. Crucially, FIX with MODE1.TRUNC=1 rounds
toward minus infinity; it is not the separate TRUNC instruction's rounding
toward zero. Nearest-mode ties use IEEE nearest-even in the bounded reference.
The entire`1257E6..125949` listing now contains zero UNKNOWN operations;
this statement does not imply a whole-firmware emulator.

## Proof and checks

`load_math_tables` validates the complete audited loader SHA256, four code-range
hashes,25 decoded instruction pins, and exposes all four raw coefficient-table
word sets with hashes. CodeFlash identity is recorded from the paired audited
image constant; this module directly reads only the loader.

Loader SHA256:
`9169c24f0337c6bfd3646e2538a567bca839b24aa0db5f67b6984943c2286ca7`.
Code addresses are SHARC shortword PCs; DM addresses are32-bit words.

Nine targeted tests cover opcode decoding, fresh loader corruption rejection,
log branch boundaries, dense normal-domain sweeps, explicit negative/tie
rounding, real retained B01 level coefficients, opt-in/default isolation and
metadata boundaries. Observed comparison errors—not hardware error bounds:

- Log10 vs NumPy: maximum absolute difference approximately`8.971e-7`
  over10001 geometrically spaced positive normal-float32 values.
- Pow10 vs mathematical power: maximum relative difference approximately
  `1.036e-7` over20001 exponents spanning`[-37,37]` in both explicit FIX modes.

No live-mode inference, acoustic acceptance or production tuning recommendation
is implied by these tests.
