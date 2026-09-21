# First-B spatial coefficients and fresh-constructor binding

Static/offline only. This closes the synthetic spatial settings in a **retained
effective-configuration scenario**, not the identity of the active car preset.
No vehicle access, deployment or production-default change was made.

Implementation: `tools/soundamp-virtual-cabin/amp_b_spatial_binding.py`.
Existing spatial numerical equations remain unchanged. Runtime factory/pipeline
integration belongs to the parent task.

## API and boundary

```python
spatial, binding = build_effective_spatial(
    effective_config=explicit_829_word_snapshot,
    pointers=explicit_all_18_config_relative_pointers,
    edges=retained_75_integer_edges,
    assume_successful_fresh_initialization=True,
)
# binding['constructor'], binding['enabled'], binding['metadata']
```

`bind_effective_spatial` returns the same binding without the constructed
reference. Inputs must be real finite float32-representable values; coefficients
are converted to their float32 stored values, never gain-normalized or replaced.
The entire loader is hash-checked by `load_shortwords`, then bounded instruction
range fingerprints and exact decoded pins are checked. Invalid pointers,
unavailable fresh state, wrong retained map, nonfinite coefficients and invalid
converted ramp bounds fail closed.

The caller remains responsible for selecting an effective snapshot and the
ordering of mode commands. Supplying a retained bank does not prove that it was
delivered to the car, selected, settled, or unmodified afterward.

## Exact static bindings

`12E6A7` loads the 28-word parameter pointer from object+1166 before calling
`130771` at `12E6AA`. Existing verified API20 pointer maps choose config+3 for
the default case and config+439 for value3. API16 does not overwrite1166.

Band map: DM[B9097]=B904C, DM[B9098]=74, 75 integer edges from0 through513.
The constructor caller `12E109..12E10F` supplies object+15 and74 to `12FD24`.
This is the same map as the common spectral binding, not uniform bands.

Parameter meanings and B01 bank1 **default** values (decimal display rounded;
the binder retains all stored float32 bits):

| Words | Meaning | B01 bank1 default |
|---|---|---|
| 0..3 | enable, eigen selector, balance selector, gate selector | 1,1,1,0 |
| 4..5 | ramp start/end | .94,.98 |
| 6..7 | rounded low/high band indices | 0,73 |
| 8 | supplied ramp step | .001 |
| 9..10 | regularization, fallback scale | .1,1 |
| 11..12 | power/cross current and prior weights | .2522313595,.7477686405 |
| 13..14 | minimum/mean current and prior weights | .07860797644,.9213920236 |
| 15..16 | high frequency smoothing | 1,0 |
| 17..18 | low frequency smoothing | 1,0 |
| 19..20 | diffuse temporal current/prior weights | .2959247828,.7040752172 |
| 21..22 | diffuse clamp min/max | .04,.2 |
| 23..24 | rising gate current/prior weights | .005923000164,.9940770268 |
| 25..26 | falling gate current/prior weights | .09357196093,.9064280391 |
| 27 | diffuse mix | .2 |

All selector/index conversions above use float32 addition of .5 followed by
TRUNC in the bounded nonnegative domain. p2 is converted inside separator
`130001..130014`, not in the top-level dispatcher. Balance and enable test
nonzero; eigen/gate paths test equality to1, not generic nonzero.

Ramp indices0 become1 only after bounds validation. `130409..130411` reads the
previous value and adds p8 **before** the first store. B01 bank1 default ramp is
approximately .94,.941,...,1.012,.98: the supplied step is NOT recomputed from
endpoint difference. The discontinuity is preserved, not normalized away.

B01 banks1..3 default spatial banks are identical. API20 value3 uses ramp
.96/.96 and mix1 in those banks; it is not an identity/bypass. B01 bank4 uses
eigen selector0 and default mix.3, alternate mix.4. Loader defaults are a
different retained coefficient source and are not substituted for B01.

## Fresh state: not an all-zero substitute

`12FD24` allocates the13-word state and history buffers via132D56. Successful
0x2000 allocation calls the fill routine1257D0 with R8=M5=0;1257DD fills every
allocated word. The constructor then explicitly fills the scalar gate history
with float1 at `12FDF6..12FDFC`.

| Spatial state field | Successful fresh value |
|---|---|
| +3/+4 power history, +5 complex cross history | 74/74/148 zero words |
| +6 diffuse history | 74 zero words |
| +7 gate history | **1**, not0 |
| +8 fixed-mean history, +9 minimum history | 0,0 |
| +10 balance slope | raw40A00001 = 5.000000476837158 |
| +11 balance offset | rawC0600001 = -3.500000238418579 |

The state constants are written at12FE33/12FE35 and are not p10/p11 of the
parameter bank. Thus the former diagnostic slope1/offset0 was not the retained
constructor. Working band-power and cross buffers are separately allocated at
state+0/+1/+2, also initially cleared; the reference computes those per frame.

## Verification and remaining uncertainty

Six new tests plus35 existing spatial tests pass:

```sh
python3 -m unittest discover -s tools/soundamp-virtual-cabin -p 'test_amp_b_spatial*.py'
```

Tests compare extracted parameters against independent raw retained bank words,
pin raw constructor constants, check all16 source/bank/pointer combinations,
preserve the non-interpolated ramp, test repeated-frame state and reproducible
fresh initialization, float32 half-boundary conversion, bypass and fail-closed
behavior. These are numerical/static proofs, not execution traces from hardware.

Not established statically: live bank/command/pointer state, current learned
histories, later state writes, publication/interpolation timing, allocation
success on the running device, exact SHARC floating-point instruction rounding,
or acoustic transfer through the cabin. This binding intentionally states a
successful fresh constructor premise; it does not label it current car state.
