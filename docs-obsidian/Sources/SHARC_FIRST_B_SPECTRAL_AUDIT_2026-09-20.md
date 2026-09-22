# First-B independent spectral numerical audit — 2026-09-20

STATIC/OFFLINE ONLY. No vehicle, deploy, production preset or default transform
changes. This audit supplements the separately recovered spectral and spatial
bindings; it does not establish active controls or acoustic cabin fidelity.

## Corrected level-mask clamp order

`amp_b_adaptive.BandLevelMaskReference` previously interpreted parameter14 as
minimum and15 as maximum. That was wrong: **p14 is maximum, p15 is minimum**.
The retained B01 header's level bank `source[50:66]` ends `[1, 0.5]`; the old
constructor rejected this legitimate bank. Synthetic unity-clamp tests used
equal bounds and therefore did not expose the reversal.

Independent caller/callee evidence:

- `12FA2E..12FA3D` pushes p14, p15, threshold, slope and output pointer.
- `12F8E0` loads `R8=DM(+5,I6)` = p14; `12F8E2` loads
  `R4=DM(+4,I6)` = p15 before calling `12BB49`.
- `12BB6B` computes `MAX(F4,F0)`; `12BB70` computes `MIN(F8,F0)`.

The corrected class validates `0 <= p15 <= p14` and calls the existing
explicit `level_gain(minimum=p15, maximum=p14)` helper. The generic helper's
named-argument API did not change. Tests now use unequal bounds and compare
multiple updates of the actual B01 bank against independent scalar equations.

Do not apply this swap to the transient bank. Its `I3=params+79` at `12F13D`
remains fixed, so `12F215 R4=DM(-3,I3)` is p76/minimum and
`12F217 R8=DM(-2,I3)` is p77/maximum. Actual B01 main/post transient bounds
are approximately `[0.07943282,1]`, consistent with the existing implementation.

## Real map and band semantics

Fresh hash-verified loader extraction confirms:

- `DM[B9097]=B904C`, `DM[B9098]=74`.
- The75 integer boundaries cover `[0,513]` exactly, including Nyquist.
- First39 bands have one FFT bin each. Final band is `[431,513)`,82 bins.
- `1316C1` sums bins; it does not divide by band width. `13165D` fills each
  half-open band with its scalar. Current complete-map contracts accept this
  real map without alteration.

`130321` averages coherence entries9 through59 equally by **band**, not by
frequency-bin width. With the real map that covers FFT bins9 through112,
`421.875 <= f < 5296.875 Hz` at48k/1024. The previous synthetic uniform map
covered bins62 through414 for those same indices. This is a material control
semantics change, not merely a different plotting layout.

The transient kernel explicitly uses74 bands (`12F150`). The magnitude
smoothers process only the first256 bins (`12E6F4`, `12E706`), not all513.
No mismatch was found in existing pre/main/post branch routing, paired
sum/difference scales or shared-versus-independent transient-history handling
in the bounded successful fixed-mode path examined here.

## Explicit firmware-derived window and packing twiddles

New `amp_b_window.load_spectral_tables()` returns:

```python
{'window': numpy_array_1024, 'twiddles': complex_numpy_array_256,
 'metadata': json_serializable_proof_and_boundaries}
```

The binder can pass those arrays explicitly to `BSpectralFrameReference`.
`amp_b_transform` defaults remain unchanged for existing callers.

`130A1C` generates512 values from `(i+.5)*float32(pi)/1024`, with pi word
`40490FDB`; `130AEB` uses those values forward for the first half and reversed
for the second. This is not a Hann window or an amplitude-normalized window.

For all512 constructor angles, `125C2B`'s quadrant index is0 and sign is+1.
Its stored polynomial coefficients are `DM[B93CD:B93D4]`, seven words from
approximately `-7.37066e-13` through `-1/6`. The bounded helper evaluates the
recovered Horner order `theta + theta^3*P(theta^2)` and stores float32 values.
It validates the loader hash, three complete instruction-range hashes and
decoded constructor/polynomial/mirroring instruction pins.

The generated window differs from ideal double-precision sine by at most
`4.2943360645e-8`. Squared overlap lies between `0.9999999175878213` and
`1.0000001081251213`; **no correction or normalization is applied**.

Stored real-transform packing twiddles come from `E4754`: first512 float words
are256 interleaved `cos(theta),-sin(theta)` pairs. Both the used-word hash and
the full1024-word table hash are retained. Tests prove supplying these values
changes the output relative to the analytic default and is not ignored.

Precision boundaries remain explicit: float64 intermediates and ideal division
replace SHARC intermediate40-bit/flags/RECIPS behavior; the final float32 store
is modeled. NumPy still supplies the mathematical complex FFT. The recovered
twiddles bind real pre/postpacking, not instruction-level complex-kernel
execution. These tables are constructor-derived, not live allocated RAM readback.

## Reproducibility

CodeFlash SHA256:
`8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`.
Bit-reversed SHARC loader SHA256:
`9169c24f0337c6bfd3646e2538a567bca839b24aa0db5f67b6984943c2286ca7`.
Code addresses above are SHARC shortword PCs; DM addresses and parameter
offsets are32-bit words. B01 header starts at CodeFlash byte offset`0x12000C`.

Tests: `test_amp_b_adaptive.py` (13),
`test_amp_b_spectral_real_layout.py` (7), `test_amp_b_window.py` (5).
The real-layout tests decode both local firmware files independently and use
manual per-band/temporal equations, rather than only comparing imported constants.
