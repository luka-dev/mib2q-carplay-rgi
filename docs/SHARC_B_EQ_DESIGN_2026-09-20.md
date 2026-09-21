# First-B nonzero EQ designer12AB1F — static mathematical reference

The previously zero-gain-only boundary is now extended with
`tools/soundamp-virtual-cabin/amp_b_eq_design.py`. It implements all three
successful nonzero design paths and returns the library's stored coefficient
order **[b0,b1,b2,−a1,−a2]**, not ordinary SOS feedback signs.

No vehicle I/O, configuration/default changes, deployment or commit. This is a
mathematical reference with float32-rounded output coefficients, **not bit-exact
SHARC transcendental routines**, live EQ state or an acoustic cabin model.

## Record and call binding

The existing API18/19 call sites at12C2EF/12C376/12C3FE/12C481 and
12C7E8/12C873/12C8FF/12C986 pass five design words, a coefficient destination,
and sample rate48000. Caller12C2D0..12C2EF copies design config305..309, sends
destination295 and rate0x473B8000=48000. The other three pairs are
310→300,389→379,394→384.

| Design word | Recovered meaning |
|---|---|
| 0 | Frequency in Hz, divided by sample rate at12AB47 |
| 1 | Width in percent: bandwidth for peaking, slope for shelves |
| 2 | Gain in dB; amplitude parameter A=10^(gain/40) |
| 3 | Rounded type:0 peaking,1 low shelf,2 high shelf |
| 4 | Not read by the bounded12AB1F body; no invented enable flag |

The fifth word's lack of use here does not mean it is unused elsewhere. All
five words must be finite in this reference. The public API uses positive
frequency below Nyquist, positive width, gain−100..+20dB (the recovered target
clamp), and type0≤value<2.5 rounded by trunc(value+.5). Other firmware conversion
or invalid-input behavior is deliberately not generalized.

## Recovered math

Let w=2πf/fs, s=sin(w), c=cos(w), A=10^(gain/40). The ROM immediates are
float32 approximations:2π at12AB4E,1/40 at12AB67,1/100 at12ABF0/12AC7D,
and ln(2)/200 at12AD02. The gain exponent has compensated float arithmetic
at12AB70..12AB7F; the reference uses its mathematical gain/40 value.

Type0 peaking,12ACF3..12AD6D:

```
alpha = s * sinh((ln(2)/200) * width * w/s)
a0 = 1 + alpha/A
b = [1 + alpha*A, -2*c, 1 - alpha*A] / a0
a = [1, -2*c/a0, (1 - alpha/A)/a0]
```

Thus width100 is **one octave**, not bandwidth100 octaves and not Q=100.

For both shelf paths,12ABDD..12AC65(type2) and12AC68..12ACF0(type1):

```
S = width/100
beta = s * sqrt((A*A + 1)/S - (A - 1)^2)
```

Type1 is low shelf:

```
a0 = (A+1) + (A-1)*c + beta
b0 = A*((A+1) - (A-1)*c + beta) / a0
b1 = 2*A*((A-1) - (A+1)*c) / a0
b2 = A*((A+1) - (A-1)*c - beta) / a0
a1 = -2*((A-1) + (A+1)*c) / a0
a2 = ((A+1) + (A-1)*c - beta) / a0
```

Type2 is high shelf:

```
a0 = (A+1) - (A-1)*c + beta
b0 = A*((A+1) + (A-1)*c + beta) / a0
b1 = -2*A*((A-1) + (A+1)*c) / a0
b2 = A*((A+1) + (A-1)*c - beta) / a0
a1 = 2*((A-1) - (A+1)*c) / a0
a2 = ((A+1) - (A-1)*c - beta) / a0
```

The two output feedback terms are stored negated. Zero gain exits with
the identity initialized by12AB0A. Nonzero success calls13163D for stability;
failed checks rewrite identity and invoke the error logger. The host helper
raises for unsupported/overflow/non-real/unstable cases instead of silently
returning successful identity. It does not emulate the logger's return policy.

Math-call roles in the decoded outer function:124354 divide,125C2B sin,
125C1C cos,1257E6 power,12440B sinh,1244A6 sqrt. The two trig entries share a
range-reduction/polynomial core; cosine adds π/2 at125C4F, while the sine entry
does not. DM tableB93C6 begins.5, maximum angle,1/π and split π terms; its
polynomial ends1/120 and−1/6. The sinh entry has the exponential and reciprocal
combination at12447F..124495; the negative term selected at124419 separates
the sinh entry from the nearby cosh entry124442. These confirm the host
primitive roles without claiming their rounding is bit-identical.

## Integration and proof boundary

Both `replay_output_target_slice` and `replay_bound_scalar_targets` in
`amp_b_bound_scenario.py` accept **`allow_nonzero_eq=True`**. Default remains
the strict zero-only behavior. Result metadata names the opt-in, coefficient
order, bounded domain and `designer_math_reference_not_bitexact`.

This allows explicit retained-slot scenarios with nonzero descriptor55..58
to produce the four EQ coefficient targets. It does not assert those slots
are nonzero in the vehicle. The eight retained loader/B01 calibrations still
have zero gains in these design records. No input map, active mode, pointer
history, coefficient smoothing or common-input EQ is inferred by this work.

## Verification

Eight new tests pass. The independent decoded outer-instruction oracle:

- verifies the machine-word SHA for12AB0A..12AD70;
- executes branches, old-operands parallel clauses, delayed CALL instructions,
  register/stack accesses and coefficient writes, rounding arithmetic to float32;
- substitutes host mathematical primitives and an explicit CJUMP ABI bridge;
- compares270 ordinary nonzero cases (three types, five frequencies, three
  widths, four gains) plus27 deep-cut cases down to−100dB;
- checks zero identity, rounded types, unused fifth word, opt-in replay,
  coefficient signs, expected frequency-response gains and independent SciPy
  filter processing.

Ordinary comparison tolerance: rtol2e−5/atol3e−6; deep-cut tolerance:
rtol3e−5/atol2e−5. Deep low shelves show cancellation sensitivity; these are
coefficient comparisons, not a certified bound on acoustic/dB error. No test
count establishes identity to a physical amplifier.

```sh
python3 -m unittest discover -s tools/soundamp-virtual-cabin -p test_amp_b_eq_design.py
```

Source: raw V850 LE32/base0/size0x180000 SHA256
`8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`;
SHARC loader SHA256
`9169c24f0337c6bfd3646e2538a567bca839b24aa0db5f67b6984943c2286ca7`.
Designer shortword range12AB0A..12AD70 SHA256
`ddb418950c281e25fbd759923641e253b58582764d56887f89c96b40b19b9ea4`.
Inspected shared trig range125C1C..125C9A SHA256
`e370cfef7e4b288e4947d0fd6caf0e27ccf406811c925a1581c331157e0a34c3`;
sinh/cosh range12440B..1244A6 SHA256
`b5cfec9abcc492945713d15515be290a651a5142f72062878a04a448d8a27041`.
Addresses are SHARC shortword PCs or DM words, not V850 byte offsets.
