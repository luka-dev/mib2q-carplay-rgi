# Upmix with retained first-B spectral controls

> Follow-up: [static numerical/publication/schedule work](UPMIX_STATIC_COMPLETION_2026-09-20.md)
> adds new `full_static_*` scenarios. The76 musical rows below remain the
> original retained/host-math results, not a rerun of that new numerical mode.

Static/offline only. This pass replaces synthetic spectral settings **in the new
explicit retained scenario**, without declaring a recovered live vehicle state.
The old neutral diagnostic remains separately available and is not relabelled.

## What is now bound

- [Common spectral header](SHARC_B_SPECTRAL_BINDING_2026-09-20.md): pre subtraction,
  main/post transient, shared sum/difference level coefficients, smoothing and
  mode5 third-input gain. These live in the common245-word source prefix, not
  the selected829-word B calibration bank.
- [Spatial controls and initialization](SHARC_B_SPATIAL_BINDING_2026-09-20.md):
  effective pointer1166 selects the real28-word bank; constructor-derived
  balance constants and gate history1 replace arbitrary test values.
- [Independent numerical audit](SHARC_FIRST_B_SPECTRAL_AUDIT_2026-09-20.md): the
  real74-band nonuniform map, stored real-FFT packing table and polynomial-derived
  analysis/synthesis window are supplied explicitly. Internal complex FFT and
  arithmetic remain host mathematical references, not SHARC bit-exact execution.
- **Corrected bug:** the level regulator's firmware fields are p14=maximum,
  p15=minimum. The old model reversed them; unity test bounds hid the defect.
  The transient regulator uses a different order and was independently checked.

Retained B01 selected here has main-transient enable0, post enable1, subtraction,
level and smoothing enabled. Disabling main-transient is a recovered control,
not an omitted block or a user-selected neutralization. The pre/post branches
are no longer synthetic unity. Source CRC, full image hashes, parameter/band-map
hashes and decoded instruction evidence accompany each binding.

## Consistent mode/pointer scenario

`make_test_scenario(first_b_spectral='retained_calibration', input_eq_bank=7)`
selects B01 common controls and retained calibration bank1 as an explicit
effective snapshot. It applies the pointer maps in order API20=1, then API16=5.
`first_b_pointer_variant='20:3'` instead chooses API20=3, still followed by16=5.
Thus mode5 and its pointer map agree; crossfeed comes from config493, spatial
from config3 or439, and the six-tail pair gains from506/507/508.

The earlier neutral wiring fixture used mode5 with16:default pointers. It remains
a labelled synthetic diagnostic, not a delivered mode-control history. A change
between that fixture and the new scenario includes **pointer mapping and
object97 as well as spectral controls**; do not attribute it to one mask alone.

This binds a supplied effective snapshot, not the complete control-request
replay/publication/interpolation lifecycle. Successful fresh spectral allocation
and initialization, zero tail histories and on-time wrapper callbacks are
explicit. It does not prove current controls or already-evolved histories in a
running amplifier. Other conditional stages (input mapping/EQ selector,
independent auxiliaries, C4D17 state and runtime gains/limiters) keep their prior
explicit assumptions.

## Run it

```sh
OPENBLAS_NUM_THREADS=1 python3 tools/soundamp-virtual-cabin/audit_upmix_end_to_end.py \
  --renderer build/upmix-e2e-20260920/stereo_upmix_render \
  --model build/hssr_model.bin --guard build/hssr_guard.bin \
  --build-provenance build/upmix-e2e-20260920/build_provenance.json \
  --input '/path/to/track.wav' --seconds 6 --tail-seconds 0.25 \
  --variant s16_reference:s16:0.18:0:0:0 \
  --variant float_reference:float:0.18:0:0:0 \
  --variant retain75:float:0.30:0:0:0.75 \
  --variant retain75_bass1:float:0.30:-1:0:0.75 \
  --scenario baseline_v7 --scenario full_retained_zero_aux \
  --input-eq-bank 7 --out output/my-retained-b-test
```

Use `_batch.py --jobs N` for repeated `--input` tracks. The corresponding probe
scenario is `full_retained_aux_probe`; it changes both independent auxiliary
signals and C4D17 adaptive-enable, not just one variable. `--save-output-wavs`
saves16-channel FLOAT32 electrical output with overs preserved. These are not
normalized listening files or a headphone simulation of the cabin.

537 model/cabin/runner tests and121 extraction tests pass (**658 total**). Fresh
runs of the existing hash-pinned C S16/float sanitizer binaries and NEON test pass.
Neither C production source nor startup/default settings changed in this pass.

## Completed musical validation

`output/upmix-e2e-retained-spectral-20260920/report.json` is complete70/70:
seven tracks × (four variants + dry BANK1) × baseline_v7/retained B. All use
six-second prefixes plus0.25 seconds of zero padding, not full-track coverage.
Report SHA256:
`f33d9451ec0f6042d5a1ffdc4a873cf682bc1612415d00eb0fd129dbe9054e00`.

All174 unique referenced file fingerprints, shard equality, PCM formats,
16×7 band matrices, paired deltas and aggregate summary were verified. All35
PCM6 files match the previous run's hashes exactly; every baseline_v7 metric
also matches exactly. The28 candidate PCM6 WAVs have zero near-rail samples and
zero digital C samples. Physical centre sound after mixing is a different claim.

Retained-B results, paired to dry through the same scenario and time window:

| Variant | Mean bass60–120 minus mid500–2000 tilt, dB | Maximum16-output sample peak, dBFS |
|---|---:|---:|
| S16 reference |+1.077|+3.768|
| Float reference |+1.069|+3.471|
| Float retention75 / surround0.30 |+0.453|+3.471|
| Same, front bass−1 dB |−0.461|+3.282|

Fifteen of35 retained-B rows exceed0 dBFS (including dry). This remains an
electrical-model warning, not proof of vehicle clipping or a safe production
gain. Do not equate improved relative tilt with matched loudness or subjective
acceptance. All candidate LFE gains here are0 dB in assist mode.

`output/upmix-e2e-retained-aux-20260920/report.json` is complete6/6: Deep Down
Low, input-EQ bank10, dry/float-reference/retain75_bass1, zero-aux versus synthetic
aux+adaptive enable. Report SHA256:
`ee742de64fe08744724de68cbf58727417c3c1c4782c422402a660b4005ea138`.
All85 unique referenced file fingerprints were checked; all six saved FLOAT32
16-channel WAVs were reread and every stored metric recomputed exactly.

Zero-aux peaks are +5.872/+4.405/+4.139 dBFS respectively; aux+adaptive peaks
are +16.781/+15.300/+14.223 dBFS. Both remain conditional supplied-state results.
The earlier synthetic dry/zero-aux bank10 peak was +22.090 dBFS. Its reduction
to +5.872 is caused by the changed DSP binding bundle, not by changing PCM6,
normalizing output or fixing a production upmix setting. Independent auxiliary
levels and C4D17 controls are still hypotheses, not observed in-car signals.

## Speaker workbook

[The supplied XLSX](B9_SPEAKER_WORKBOOK_EVIDENCE_2026-09-20.md) was inspected
read-only, including its embedded pinout image. It supplies separately attributed
speaker ranges/topology evidence, not B coefficients. Its broken reference and
conflicting tweeter ranges remain documented. No synthetic acoustic filter or
DSP-output permutation was fitted to those values; measured speaker-to-ear IRs
and actual cfg-index→pin correspondence remain absent.
