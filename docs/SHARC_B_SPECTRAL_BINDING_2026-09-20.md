# First-B retained spectral binding — static/offline only

The first-B reference can now consume extracted common spectral controls rather
than the earlier synthetic unity masks. This is an explicit retained-initialized
scenario, not a claim about current vehicle state, acoustics or chosen sound.
No vehicle access, firmware patch, production default, deployment or commit.

## Source and address domains

The whole B source contains3562 words. Its common prefix0..244 is independent
of the selected829-word calibration bank1..4. Do not take spectral controls
from a bank payload or interpret bank1 as proof of MOST5.1/current state.

- SHARC loader DM32 source base`C3C6A`; loader SHA256
  `9169c24f0337c6bfd3646e2538a567bca839b24aa0db5f67b6984943c2286ca7`.
- B01 common prefix starts at V850 byte/file offset`12000C`; CodeFlash rawLE32,
  base0,size`180000`, SHA256
  `8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`.
- SHARC code addresses below are16-bit shortword PCs; object and DM offsets
  are32-bit words. Fresh decoding uses the existing validated VISA decoder;
  the loader hash and instruction pins are checked on each extraction.

Constructor`130F8E..130F93` calls`12B4D0` with object+59 and source base.
`12B4E6` selects source+38; `12B2BF` copies207 words into local+33:
103 SIMD pairs followed by one scalar, therefore source38..244 becomes
object92..298. Independent decoded copy execution checks the complete interval.
`1315AE..1315B5` passes object+59 as the spectral parameter-base argument.
The distinction between object-base and parameter-base matters: subtraction's
local+39 is object98, and level's local+45 is object104.

| Consumer | Common source words, half-open | Object words, half-open |
|---|---|---|
| Mode5 third-input gain |43|97|
| Pre subtraction |44:50|98:104|
| Sum/difference level masks, shared coefficients |50:66|104:120|
| Two256-bin temporal smoothers |66:69|120:123|
| Post transient |69:157|123:211|
| Main transient |157:245|211:299|

Pre caller`12E1FB..12E201`; main`12E334..12E33F`; level
`12E588..12E592` and`12E615`; smoothers`12E6EC..12E70A`; post
`12E780..12E787`. Tests execute these decoded caller paths with success-return
callee stand-ins, independently deriving passed coefficient/state addresses.

Crossfeed is different: `12E683` loads parameter-base+1102, i.e. pointercell1161,
and`12E48E` dereferences it. It is NOT inline object1102 or config803.
The explicit effective pointer map selects config31(default),426(API20=3),
or493(API16=5). The last applied API owns the shared pointer cells. Applying
API20 then API16=5 therefore selects493 while preserving API20's spatial suffix.
No choice is inferred from the library audio mode.

## Actual controls, not fitted masks

For retained B01 common controls:

- object97 is`0.5011872053146362`; loader has`0.5`.
- Main transient enable0; post enable1; both stereo selectors0 (independent).
- Pre subtraction enabled, ramp start≈0.92/end≈0.97, indices50..512,
  negative residual multiplier≈0.1. Loader starts its ramp at bin12.
- Level enabled, its16-word bank includes **p14 maximum1,p15 minimum0.5**.
  The prior reference treated these in reverse; the independently decoded
  `12FA2E..12F8E2→12BB49` argument binding established the correction.
  Serialized firmware bank order is preserved, never silently swapped.
- Smoothing enabled; history/input multipliers≈0.19448626/0.80551374.
- Post74 factors vary roughly0.016..0.251, not all1; output clamps≈0.07943..1.
  The transient mask is dynamic, so these factors are not direct constant gains.

`B9097=B904C`, `B9098=74` supplies75 nonuniform integer band boundaries,
beginning0..39 and ending311,347,387,431,513. The map covers all513 bins.
It is not the previous uniform `linspace` diagnostic map. The retained loader
map and any real live replacement remain distinct concepts.

## Successful fresh histories

`12F0B8` allocates296 words for each main/post transient state. `12F793`
allocates three independent74-word arrays for each level state. The successful
allocator path`132DEC..132DF5` passes zero to`1257D0`, whose`1257DD` loop fills
the allocation; these constructor histories really begin at zero.
`12D3F6→12D3E0` separately initializes smoother first-call flag1 and zero data,
matching the existing first-enabled-call priming behavior. Spatial state is
bound by the separate spatial constructor helper, including its nonzero gate
history; it is not replaced by blanket zero histories here.

The builder requires explicit`zero_histories=True` to select this successful
fresh-initialization scenario. It does not prove a running amplifier has reset,
or that publication, interpolation, transitions and callback timing succeeded.

## API and verification

`tools/soundamp-virtual-cabin/amp_b_spectral_binding.py` exports:

```python
calibration = load_spectral_calibration(source='b01')
binding = bind_spectral_parameters(calibration,
    effective_config=cfg, pointers=explicit_effective_pointers)
spectral = build_spectral_reference(binding, spatial=spatial_reference,
    spatial_enabled=spatial_binding['enabled'], zero_histories=True)
input_mix_gain = binding['input_mix_gain']
```

Image paths are overrideable but bytes must match the validated images.
Whole-source CRC, raw header SHA, band-map SHA, effective-config SHA and source
fingerprints accompany the binding. Spatial state and enable are supplied
separately; no synthetic spatial parameters are inserted by this helper.

The builder also calls`amp_b_window.load_spectral_tables`: actual256 complex
twiddles from loader DM`E4754` and the statically reconstructed float32-stored
constructor window are passed explicitly. No analytic-default tables or overlap
renormalization are substituted. Window generation uses decoded polynomial
constants with host float64 intermediates and final float32 storage, not a
bit-exact SHARC40-bit/FPU execution claim. Its evidence accompanies
`reference.binding_metadata['transform_tables']`.

Artifact`assets/soundamp/remaining-blocks-20260920-v3/b_binding_spectral_calibration.json`
contains both complete raw common prefixes, both resulting bindings, the
74-band map and source/instruction/transform/spatial evidence. Its representative
calibration bank1 example explicitly applies API20default then API16=5.
The B01 common-prefix SHA256 is
`88d89e08bb90b179d05c18f2bbd4250e9b74ed0ed7786c2343c7f70a4d2913f0`.

Nine tests cover decoded copy/caller paths, actual retained controls, all four banks
and both source images, mode-pointer precedence, corruption rejection, fresh
states, integrated spatial/transform construction and genuinely non-neutral
mask output. Reproduce:

```sh
python3 -m unittest discover -s tools/soundamp-virtual-cabin -p test_amp_b_spectral_binding.py
```

Still not established: live common-header/indexed mutations, current config or
pointer map, live histories, ramps, allocation/error lifecycle, physical graph
mapping, real callback scheduling, SHARC bit-exact transcendental/FFT rounding
or subjective/cabin optimum. Retained spectral controls remove a major
synthetic-coefficient hypothesis; they do not remove these other boundaries.
