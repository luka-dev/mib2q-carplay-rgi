# C4D17 calibration composition and detector correction — 2026-09-20

Static/offline only. No vehicle connection, deployment, default music settings,
firmware modification or claim about active amplifier state.

## Important correction: full-mode detector samples

The previous `c4d17-fixed-full-mode-reference-v2` incorrectly selected the first
four original filtered frames. This omitted **stage12's in-place compaction**,
which runs before stage13's sequential unpack. The correction is in
`amp_complex_path.py`, now **v3**. Old opt-in adaptive/full-mode synthetic results
must not be used as current model predictions. Identity-default music/tuning
reports do not execute this code and are unaffected.

Exact static evidence:

- Loader descriptor `C4D17+12=16`, `+13=4`, root `C4F92+9=64`.
  These are shared SHARC loader fields, not separate profile0/profile1 values.
- `1273AD` loads descriptor+12; `1273AF` doubles it; `1273B3` sets `M1=32`.
- `1273BF` loads a SIMD pair through `DM(I2,M1)`; `1273C2` stores through
  `DM(I3,M0)`, with `M0=2`. The loop count is four.
- Initializer `126ED7..126EE8` binds the input and output halves of the six-pair
  compaction table to the same six scratch buffers. Consequently this is an
  in-place copy, not a different unseen resampler.

| Detector frame | Source interleaved word offsets | Destination word offsets | Original audio sample |
|---|---|---|---|
|0|0,1|0,1|0|
|1|32,33|2,3|16|
|2|64,65|4,5|32|
|3|96,97|6,7|48|

Earlier writes cannot overwrite later source reads. Stage13's `127A80`
sequentially unpacks the **compacted** first four pairs. Effective sampling is
therefore `[0,16,32,48]`, not `[0,1,2,3]`. A literal scalar memory-copy regression
checks this independently of NumPy slicing, and pins the loader fields and
decoded instruction statements. The corrected full-mode test compares against
an independently filtered waveform at those sample indices.

## Newly composed calibration path

[amp_complex_calibration.py](../../tools/soundamp-virtual-cabin/amp_complex_calibration.py)
composes fixed mode2 mask `0xF4BA`, after enable/buffer binding:

1. Stage1 clears ten music-output lanes.
2. Stage3 advances configure/wait/measure. Configure installs gated FIR
   reference weights, auxiliary weight field29 and clears FIR **history**.
   It does not clear coefficients, residual power history, filter states,
   generator state or graph96. Configure/wait/completed calls return early.
3. Stage4 generates the explicitly configured noise, shapes it, applies
   explicit descriptor+30 gain and channel gates.
4. Stage5 filters supplied auxiliary graph11 through two biquads or the
   field36 bypass. Input is **after C4D11**; no additional +6dB is applied.
5. Stage7 packs five excitation pairs and a sixth auxiliary/scratch pair.
6. Stage10 copies excitation to output before detector filtering mutates scratch.
7. Stage12 runs the eight-section prefilter on all12 lanes and compacts at
   stride16; stage13 unpacks four detector frames.
8. Stage14 runs the existing ten-reference512-tap FIR residual/LMS kernel.
9. Stage15 adds64 to the measurement counter.

Stages2,6,8,9,11 and16 onward are absent. There is no normal music input in
this API, no1023-sample full-mode delay, no five-band processing, no power-window
target controller and no inferred microphone or acoustic feedback signal.
The preserved completion fall-through produces **and learns from** one final
block using the previous channel gates; the next call clears outputs.

### Persistent graph96 is not a new music sum

Stage6 normally creates a music sum in graph96, but mode2 does not run it.
Initializer `126E3F..126E46` binds the sixth packed pair to filtered auxiliary
and this existing graph96 buffer. Supplying zeros implicitly here would erase
state the firmware preserves. The new model therefore requires an explicit
64-word graph96 snapshot and retains it across calls.

The decoder/LMS writes the same buffer as follows:

| Graph96 words | End-of-learning-block contents |
|---|---|
|0..3|auxiliary detector samples|
|4..43|ten reference lanes, four samples per lane|
|44..47|FIR residual|
|48..51|prefiltered/decimated stale sixth-pair second lane|
|52..63|unchanged retained words|

The stale lane is not used by mode2 LMS. A regression supplies opposite
nonzero scratch snapshots and checks that this unused lane differs while
excitation, residual and learned FIR coefficients remain identical.

## Explicit-state and numerical contract

Caller supplies profile, generator/sequence, excitation gain, initial FIR bank
and canonical history, residual power state, initial reference/auxiliary gains,
two auxiliary-filter states, eight twelve-lane prefilter states, graph96,
FIX rounding choice and the audited detector stride16. This permits explicit
resume scenarios without silently inventing neutral histories. Profile values
are a supplied parameter snapshot, not live state. All inputs must be finite.

The implementation uses float64 mathematical filter/LMS references. It does
not emulate SHARC SIMD/extended-precision rounding, real-time scheduling,
physical microphone response, calibration acoustics or mode transitions.

## Verification and corrected artifacts

Nine new tests cover loader/alias/stride evidence; exact stage list and unchanged
excitation output; configure-only resets; independent scalar LMS and planar
scratch mapping; stale scratch independence; scalar prefilter recurrences;
completion learning; auxiliary bypass history; and fail-closed inputs. All59
`test_amp_complex*.py` tests passed after the correction.

[Mode2 smoke report](../../assets/soundamp/complex-calibration-20260920/calibration_mode2.json)
uses actual profile0 initial FIR, explicit synthetic type1 generator state,
0.1 excitation gain, .01-amplitude63Hz post-C4D11 auxiliary tone and explicit
zero other histories. Shortened synthetic wait64/measure128 counters exercise
all ten channels in42 calls:21 learning blocks including final fall-through,
then completion. FIR delta L2 is `1.930677909194678e-6`. This is a composition
test, **not a calibration proposal or cabin identification**.

The previous five-second full-mode sensitivity tests have been rerun into new
artifacts, without overwriting old v2 results:

- [Profile0 v3](../../assets/soundamp/complex-calibration-20260920/complex_scenarios_profile0_v3.json)
- [Profile1 v3](../../assets/soundamp/complex-calibration-20260920/complex_scenarios_profile1_v3.json)

Each scenario produces234 target updates. Selected **endpoint**, not acoustic
or output-spectrum, dB values:

| Profile / sideband | Synthetic auxiliary amplitude | Final five endpoint dB |
|---|---|---|
|0 /0|0|4.19,3.58,3.43,2.15,2.06|
|0 /0|.01|6.36,5.36,3.37,2.43,2.33|
|0 /0|.1|9.12,7.45,4.19,.34,.29|
|1 /0|0|0,0,0,0,0|
|1 /0|.01|4.82,3.01,1.58,0,0|
|1 /0|.1|8.82,6.80,4.27,0,0|
|either /1|.1|0,0,0,0,0|

Old `remaining-blocks-20260919/complex_scenarios_profile*.json` numbers and
documentation claims of literal original-prefix sampling are superseded.
They must not be silently mixed with v3. No active vehicle scenario is selected
by these synthetic comparisons.

The static extractor now emits `decimation_stride:16` and
`sample_indices:[0,16,32,48]` in each `complex_control_profiles` detector layout.
Its fresh complete output is
`assets/soundamp/remaining-blocks-20260920-v3/` (168 ranges, UNKNOWN0).
Only `complex_control_profiles` differs from the retained Sep19 audit. The old
artifact is deliberately not overwritten because completed tuning reports pin
its hash; its original-prefix selection description is superseded here.
The v3 numerical reference reports the corrected indices explicitly even when
loading the unchanged coefficient banks from that historical artifact.

Provenance: source CodeFlash SHA256
`8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`;
derived loader SHA256
`9169c24f0337c6bfd3646e2538a567bca839b24aa0db5f67b6984943c2286ca7`.
Reports include parameter/code hashes and decoded instruction fingerprints.
SHARC instruction addresses are shortword addresses, not CodeFlash offsets.

```sh
python3 -m unittest discover -s tools/soundamp-virtual-cabin -p 'test_amp_complex*.py'
python3 tools/soundamp-virtual-cabin/amp_complex_calibration.py IMAGE \
  --output assets/soundamp/complex-calibration-20260920/calibration_mode2.json
python3 tools/soundamp-virtual-cabin/audit_amp_complex_scenarios.py IMAGE \
  --profile 0 --seconds 5 \
  --output assets/soundamp/complex-calibration-20260920/complex_scenarios_profile0_v3.json
# Repeat the last command for profile1 and the corresponding output name.
```
