# J525 conditional electronic DSP model — v7 default / v8–v9 opt-in (2026-09-20)

Current relevant Python validation: **704 tests (121 extraction +583 model/cabin)**.

> Newest: [static numerical/publication/schedule extensions](UPMIX_STATIC_COMPLETION_2026-09-20.md).
> Source-derived log/pow and internal FFT table are opt-in, with explicit FIX
> hypotheses. Whole829-word publication and the mode5 consumer schedule are
> separately executable. They do not themselves render changing-delay audio
> or close every pointer alias. No production or vehicle state changed.

> Latest: [retained first-B spectral scenario](UPMIX_RETAINED_B_2026-09-20.md)
> removes unity masks/uniform bands/arbitrary spatial initialization in the NEW
> `full_retained_*` tests. Common-header/config/table bindings are explicit;
> level p14MAX/p15MIN corrected, mode5 pointers consistent. Retained coefficients
> and successful fresh init are not current vehicle state or bit-exact SHARC.

> [Whole-upmix offline runner](UPMIX_END_TO_END_2026-09-20.md) now exercises the
> actual current host C upmixer, fresh PCM6, runtime DSP scenarios and paired
> common-window metrics, with optional FLOAT32 16-channel exports and parallel
> track execution. This is a practical end-to-end test, not a live-state or
> acoustic-fidelity claim; supplied neutral first-B spectral controls can expose
> large retained tail gains and must not be interpreted as the factory preset.

> Latest third-pass closure: [effective first-B tail binding and all-stage
> synthetic integration](SHARC_B_TAIL_BINDING_2026-09-20.md),
> [second-B target writers and mode1 gain state](SHARC_SECOND_B_2026-09-20.md),
> [wrapper control/transitions](SHARC_B_WRAPPER_CONTROL_2026-09-20.md),
> [nested active zero delay](SHARC_NESTED_ZERO_DELAY_2026-09-20.md), and
> [bounded pointer/smoothing analysis](SHARC_B_BINDING_2026-09-20.md).
> All electrical scenario-presence gates can now be true in a synthetic composed
> test, but this does not establish live state, full firmware coverage, acoustic
> fidelity or tuned musical quality. Production/defaults unchanged.

> Previous second-pass closure: **536 tests (121 extraction +415 model/cabin)**.
> [C4D17 v3 detector/calibration](SHARC_COMPLEX_CALIBRATION_2026-09-20.md) fixes
> stage12 stride16 compaction; adaptive reports rerun for both profiles. Default
> identity-path music reports are unaffected. [Second-B](SHARC_SECOND_B_2026-09-20.md)
> now has a30-node numerical reference and opt-in AmpChain v9 integration from
> independent aux8/9/10, with supplied effective state and partial stored targets.
> [Nonzero B EQ](SHARC_B_EQ_DESIGN_2026-09-20.md) and conditional
> [1024-sample two-bank wrapper timing](SHARC_B_WRAPPER_TIMING_2026-09-20.md)
> are implemented/tested. Defaults remain unchanged; this is not live-state,
> bit-exact hardware, measured cabin or subjective-optimum validation.

> Previous closure: [B scalar/effective-output bindings](SHARC_B_BINDING_2026-09-20.md)
> cover all15 replay scalar targets incl API21/22 final gain; no smoothing/live-state claim.
> [C4D17 calibration excitation/timing](SHARC_COMPLEX_TIMING_CLOSURE_2026-09-20.md)
> now includes two shaping filters and second-B ordered ancestry from graph8/9/10.
> [Acoustic bridge](UPMIX_CABIN_BRIDGE_2026-09-20.md) accepts explicit6/16-channel
> transfer domains without double DSP; synthetic fixtures are not measurements.
> [Readiness gates](UPMIX_MODEL_READINESS_2026-09-20.md) verify392/392 stress cells
> but correctly reject full fidelity.490 tests(121 extraction +369 all model/cabin).
> Actual calibration/capture/state remain missing; AmpChain defaults unchanged.

> CRITICAL: [V850→SHARC matrix audit](SHARC_MATRIX_CONTROL_2026-09-20.md) proves
> A07/RAM rows are outputs. v5/v6 applied an extra transpose, now removed.
> Old speaker-group bass results and “sub only LFE” claims below are superseded;
> [Corrected music rerender](UPMIX_MATRIX_RERENDER_2026-09-20.md) is complete:
> seven tracks,28 matching PCM hashes, explicit v7/v8 scenarios and two LFE ablations.
> [Midrange attribution](UPMIX_MIDRANGE_ATTRIBUTION_2026-09-20.md) now separates
> upstream loss from changed L/R summation. [C guard fix and fresh retention sweep](UPMIX_GUARD_RETENTION_2026-09-20.md)
> now use corrected topology; input protection still does not bound downstream EQ.
> [105-render tuning sweep](UPMIX_TUNING_SWEEP_2026-09-20.md): rear/retention grid,
> bass/LFE controls, limiter duration and downstream headroom, matched PCM6 comparison.
> Current421 Python tests(121+300),168 SHARC ranges,43 V850 listings. No car-state proof.

Latest **static-only** control audit: [level source](SHARC_LEVEL_CONTROL_2026-09-20.md).
The common-EQ selector is driven by the first smoothed source-level control,
not a PCM measurement; normal enable initializesA5. Explicit control helpers
cover source/trim/override/mute/commit without assuming external values or delivery.
First-B API25 instead uses the unsmoothed selected target. Its curves address
three linear branch gains; loader/B01 give zero level contribution, NOT zero B.
All9 branch-gain slots now bound: B01 differs from loader at APIs18/19/22/27.
Explicit target calculators now also cover all15/12 API19/18 scalar destinations,
two-tail delay selectors and four EQ rebuilds. Nonzero EQ design is now an
explicit opt-in mathematical reference; remaining composition/control writers
remain open. [API17 bank replacement/replay](SHARC_B_CONFIG_2026-09-20.md)
now covers all four calibrations, distinct from input matrix bank0..2.
API16/20 effective-config pointer selection is also bound: shared pointers depend
on apply order, not just final selectors; explicit-state helpers preserve this.
Pointer1161 is post spectral crossfeed (mono sum unchanged at this stage);
1162..1165 pointees are delay curves, but indirect consumers remain unresolved,
not assumed inactive. Extended FIFO indexed stores,
mode-call arguments and source-RAM store now have explicit effect plans, not full
audio emulation or delivery proof. [Outer request audit](SHARC_OUTER_CONTROL_2026-09-20.md)
binds A21 forwarding and a dedicated API18 sender: signed source−12..12 clamps
to−9..9, zero→0. Its additional coefficient-block effects are now bound in the
matrix audit above; no UI mapping or current vehicle state is inferred.
EQ-only bank comparisons are not
total-volume simulations. Physical input/B configuration work remains open.

Implementation: [amp_dsp_chain.py](../../tools/soundamp-virtual-cabin/amp_dsp_chain.py).
Version: `bank1-known-electronics-v7`. This supersedes the earlier table that
omitted the second pair mixer. It is a conditional electronic model, not a
complete emulator or a prediction of cabin SPL.

V5 corrects C4D17 control scales from the erroneous `/10` exponent to the
instruction-proven `/20` law. Earlier adaptive scenario numbers are withdrawn
and recomputed; the static identity-path tables below are unaffected.
Startup/calibration control, noise core and first-B filter kernels now have
separate references/tests; this is still not the complete B processor.
Further B research adds fixed delay/nested-all-pass/single-all-pass tail and
spectral magnitude/normalization/sum-difference references, with explicit
parameters only. The tail's B lanes6/7 route to row1 slots8/9 at −99 dB;
the other six B lanes have 0-dB mixer gains. These helpers do not change the
default model or establish the active vehicle processing. See the
[remaining-block audit](SHARC_REMAINING_BLOCKS_2026-09-19.md) for evidence.
Next follow-up adds six/four-channel fixed tails and their eight-channel
merge/EQ/gain finish (`amp_b_output.py`), plus explicit-history spectral
subtraction, transient/level masks and smoothers (`amp_b_adaptive.py`).
`amp_b_transform.py` now covers real-FFT packing/scales and an ideal
sine-window/overlap neutral reference: unity, delayed 512 samples, not a
live B render. The complex FFT primitive is idealized, not SHARC emulation.
`amp_b_spatial.py` adds `SpatialFrameReference`, composing 130771 statistics,
minimum/mean controls, complex band separator, diffuse mask and residual blend
with explicit parameters, converted selectors and histories. `amp_b_pipeline.py`
now composes spectral wrappers and fixed mode2 from prepared stereo PCM to eight
B outputs: pre→four-tail, post→six-tail, main→two-tail. It requires explicit
parameters/state; other modes/input preparation, lifecycle, hardware execution
and live configuration remain open. The old optional precomputed-power bypass
in normalization was incorrect: that pointer is overwritten scratch storage.
The API and regression test are corrected; the baseline never used the option.
Activation audit additionally separates active graph handler, callback gate
BC9EC (boot0, live unresolved), and selector (init2, V850 mapping2/5/5).
CORRECTION: BC9EC==1 inhibits/clears; !=1 calls the library in callback state3.
Boot0 is therefore not a reason to model B as silent. Host20001 selects process;
20003 selects copy bypass, not mute. Bulk initialization arg1 queues event5
(bypass) then44 (process); live execution and later state remain unverified.
Bank1 linkage is now verified: the matrix-bank byte also indexes the2/5/5
library-mode table; sink0x26 refresh therefore requests mode5, pending delivery
and application. `BMode5FixedReference` adds explicit object+97 gain times the
third boundary input to both stereo inputs, then composes all eight B outputs.
The gain source is now traced: loader0.5 or conditional B01 value0.5011872053
(about−6dB), from source+43 to object+97. Normal initialization registers
interpolation from object+299, so its update helper skips this gain. This is
not proof of the effective live value. Upstream graph preparation still needs
active parameter/source binding; the A07 matrix is downstream of B, not before
it. New `BInputBankEQReference` models the upstream12618C per-channel EQ kernel
with explicit effective gains/coefficients/history; no cross-channel mixing.
Sep20 continuation binds the upstream calibrated writers and adds
`BInputPathFixedReference`: C4A54 delay, prefilter, unity gain targets, shared
tone/graphic EQ and16-bank level-selected EQ. That EQ is COMMON TO A AND B;
its55Hz peak can reach+20.5dB with neutral Bass tone gains. Active bank is not
read back. C4A54's uniform delay subtracts2967samples from enabled millisecond
requests and clamps0..9600; an independent DMA-ring test confirms steady
sample-delay semantics.277 tests pass;127 bounded ranges UNKNOWN0.
See [input controls and delay audit](SHARC_INPUT_PATH_2026-09-20.md).
Boundary graph inputs still need physical MOST mapping. AmpChain now accepts
an explicit `upstream_scenario` and optional `first_b_scenario` (mode5), sharing
the processed input across A/B. Mapping, controls, B parameters/history and
additional wrapper latency must be supplied. Default remains v5; this opt-in
path reports `bank1-electronics-v6-explicit-common-input`. 287 tests pass
(81 extraction +206 model). The two-track bank10-versus7 smoke report shows
about +5.9dB in20–60Hz at the candidate sub/front-woofer groups under B=0;
it is not a calibrated full-B render, a stock comparison or a live diagnosis.
See the input-path audit for API, report and remaining readback prerequisites.

Source: [output_binding_audit.json](../../assets/soundamp/output_binding_audit.json),
freshly extracted from J525 8W0035465 SW0420 CodeFlash, SHA-256
`8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`.
New records retain source addresses and actual firmware gain-table values.
A stale audit without these fields is rejected instead of using old defaults.

V3 follow-up: [remaining-block audit](SHARC_REMAINING_BLOCKS_2026-09-19.md).
Post-EQ channel binding is now decoded (non-unity supported); gain and limiter
kernels are recovered and can run with explicit caller-supplied runtime state.
C4D17's five-band audio kernel is independently reconstructed, but **not** a
replacement for the complete block. Default v2 numeric assumptions remain;
the seven-track table below is the earlier v2 report, not a live-state render.

V4 composes C4D17's fixed full-mode music/detector/control path and exposes it
as an **explicit optional** AmpChain scenario, requiring supplied graph11 audio
and power offsets/FIR/enable/sideband/rounding choices. Startup writes permit
adaptation; it is **not proven off**. Default remains identity: the totals and
musical reports below still omit the conditional 1023-sample delay and adaptive
transfer. See the remaining-block audit for the exact buffer binding, synthetic
scenarios, API, tests and live-readback fields. Calibration audio composition,
hardware equivalence and live auxiliary source/level remain open. The recovered
upstream auxiliary scalar is +6 dB in the loader; API input is after that node.

## Implemented chain

```
6-input matrix (bank 1; -99 dB floor, not mute)
  -> 8-bus delay C4AF7: [1632]*8
  -> 8-bus delay C4B0F: [87,87,125,204,403,403,367,367]
  -> row EQ C4B27: 16 verified-unity records
  -> profile EQ C4B3B: 55 packed records
  -> first pair mixer C4B4F: A gains + explicit B input
  -> complex C4D17: default IDENTITY / explicit full-mode reference scenario
  -> 10->16 selector C4DF1
  -> second pair mixer C4DFB: recovered A/B gains and A polarity
  -> normal selector C4E6A / config-to-DSP permutation
  -> packed output EQ C4E74: 143 records
  -> C4E89 EQ: 8 packed records on cfg12..15
  -> C4E9A limiter (only with explicit runtime scenario)
  -> C4EAA EQ: 8 packed records on cfg12..15
  -> output delay C4EBB -> C4ED3 limiter (explicit runtime scenario)
  -> stored output gain
```

The three added EQ banks are neutral in stored calibration. Non-unity row EQ
is still rejected; post-EQ now uses verified packed binding. Their 32 records
are explicitly present in the model metadata. Optional `runtime` also applies
explicit steady gains at C4B59/C4DDA, bracketing the chosen complex-block model.

Defaults: profile 0, steady row 1, ASAD FC, normal selector mode 0.
Profile 1 works with a profile-1 extraction; the input matrix is read from that
artifact, not hardcoded to profile 0. This does not establish live MMI state.

`process(x6)` resets state at clip start and returns named stages, with
`out` in V850 configuration order. Use `capture_stages=False` to retain only
the final output. Process the full clip before cropping. Optional
`first_b=frames×10` and `second_b=frames×16` inject audio directly at the
respective mixer boundaries; omitted inputs are an explicit zero hypothesis.
They do not reconstruct the unknown upstream B generators.

`response(freqs)` includes the known delays and therefore returns phase
consistent with impulse rendering. `include_delay=False` is available for
filter-only inspection. It rejects nonlinear runtime scenarios; those require
`process()`. No acoustic summation is performed.

## Recovered second-mixer gains

Writer `0xA9FF4` -> SHARC `0xE453A`; A at profile-0 `0x10032C`,
polarity at `0x10031C`, B at global `0x102408`. Signed codes use **0.2 dB**
units and -128 mute, not the first mixer's 1 dB coding.
See [independent audit](UPMIX_BANK1_BASS_AUDIT_2026-09-19.md).

| Output candidates | Config | Added A-path gain | Total known A delay, samples |
|---|---|---:|---:|
| Front mid/treble | 0/1 | +3.8 dB | 1786/1789 |
| Rear doors | 8/9 | 0 dB | 2035/2035 |
| Centre mid | 10 | +0.2 dB | 1757 |
| Centre tweeter | 11 | -8.8 dB | 1774 |
| Front woofers | 12/13 | +9.4 dB | 1719/1719 |
| Sub windings | 14/15 | -0.8 dB | 1836/1836 |

These are existing factory calibration gains, not proposed changes to the
amplifier. Speaker names are inferred candidates. Delays exclude the upmixer,
IIR group delay and other unmodelled processing.

## Biquad math

Builder `0xA96C8` / packer `0xABF7C`, independently traced again on 19.09.
The model uses extracted float gain/ZETA tables with float64 processing;
this is mathematical reconstruction, not SHARC rounding equivalence.

`K=cot(pi*f/48000)`, `G=10^(dB/20)`.
Stored five-word coefficients are `[-a1,-a2,b0,b1,b2]/a0`;
accelerator order is `[b0,b1,-a1,b2,-a2]`.

| Type | Formula / parameter |
|---|---|
| 1 peak | P=10/q=1/Q; boost b=[K²+GKP+1,2(1−K²),K²−GKP+1], a=[K²+KP+1,2(1−K²),K²−KP+1]; cut b uses P, a uses P/G |
| 2/4/6 LP1 | b=[1,1,0], a=[K+1,1−K,0] |
| 14/16/18 HP1 | b=[K,−K,0], a=[K+1,1−K,0] |
| 3/15 LP2/HP2 | P=q/10=ζ; a=[K²+2KP+1,2(1−K²),K²−2KP+1]; b=[1,2,1] or [K²,−2K²,K²] |
| 5,7…13 / 17,19…25 | same second-order formulas, P from firmware ZETA table; HP index=type−12 |
| 27 high shelf | G≥1: b=[GK+1,1−GK,0], a=[K+1,1−K,0]; G<1: b=[K+1,1−K,0], a=[K/G+1,1−K/G,0] |
| 28 all-pass | P=10/q; numerator reverses denominator |
| 0 | explicit bypass; q=0 uses P=10 in applicable nonzero types |

Unsupported filter types fail explicitly.

## Updated static response

Profile 0 / row 1 / B=0 / complex identity, dB re indicated MOST input.
Values include the second mixer; delays alter phase, not the magnitudes below.

| Output candidate ← input | 31 Hz | 63 | 125 | 250 | 500 | 1k |
|---|---:|---:|---:|---:|---:|---:|
| Front woofer ← L | -18.9 | +0.9 | -0.7 | -15.6 | -32.0 | -49.2 |
| Front mid/treble ← L | -35.2 | -26.3 | -22.1 | -2.0 | -2.6 | -1.5 |
| Centre mid ← L only | -69.5 | -45.9 | -33.6 | -8.8 | -8.2 | -16.2 |
| Rear door ← Ls | -20.9 | -4.4 | -6.3 | 0.0 | +4.5 | -2.4 |
| Sub winding ← LFE | +3.8 | +4.9 | -25.3 | -40.2 | -59.8 | -67.1 |

The matrix still routes LFE into front/centre/rear as well as the sub.
The -99 dB input floor produces tiny leakage, so L/R-only bank 1 is no longer
reported as literally zero at sub/rear. The centre crossover attenuates bass;
its upstream bus must not be equated with a full-range physical speaker.

## Recomputed music report

[Seven-track v2 report](../output/upmix-bank1-model-v2-20260919/report.json).
Same three pre-rendered C variants as the independent audit. Analyse source
seconds 5..55 of 60-second clips, processing the whole clip first; align each
output by its known A-path delay plus the 512-frame upmixer latency.

Welch PSD, linear power average within stereo groups, mean paired dB over
tracks. Reference is dry input L. Baseline `dry` is L/R-only PCM6 **in bank 1**,
not stock stereo. Numbers are conditional electrical levels, not cabin SPL.

| Variant | Woofer 60–120, re dry L | Front mid 500–2k, re dry L | Sub 20–60, re dry L | Bass/mid ratio change vs dry bank 1 |
|---|---:|---:|---:|---:|
| dry | -0.7 dB | -2.3 dB | -90.4 dB (leakage) | 0 |
| LFE12288, no shelf | -1.5 dB | -5.9 dB | -5.3 dB | +2.80 dB |
| LFE4096, no shelf | -1.3 dB | -5.9 dB | -14.8 dB | +3.07 dB |
| LFE4096, shelf -3 dB | -4.2 dB | -5.9 dB | -14.8 dB | +0.15 dB |

Bass/mid ratio change uses paired changes in different frequency bands, not
subtraction of the two re-dry-L columns. The reference powers in those bands
differ. Levels above input full scale in this float model do not establish
either real hardware clipping or adequate amplifier headroom.

## Remaining hypotheses, always exported in reports

- Input processing before C4AED is not numerically emulated.
- Complex C4D17 remains identity despite active scheduling.
- First B is a separate 512-frame processor; second B traces to auxiliary
  inputs 8/9/10. B=0 remains the default hypothesis, not measured silence.
- C4B59/C4DDA steady gains and C4E9A/C4ED3 limiter kernels can run only with
  explicit runtime parameters; default reports omit them. Full gain ramps,
  live volume/tone/loudness and coefficient transitions are not emulated.
- Input-bus packed EQ grouping is inferred; physical T38 binding is not proven.
- Speaker sensitivity, passive networks and cabin transfer are absent.

## Validation and reproduction

```sh
python3 tools/audit_amp_output_bindings.py IMAGE --output assets/soundamp/output_binding_audit.json
python3 tools/soundamp-virtual-cabin/amp_dsp_chain.py
python3 tools/soundamp-virtual-cabin/audit_bank1_model.py IMAGE --output OUT/static-audit.json
python3 -m unittest discover -s tests -p 'test_audit_amp_output_bindings.py'
python3 -m unittest discover -s tools/soundamp-virtual-cabin -p 'test_*.py'
```

V2 validation (before the additional v3 tests): 12 extraction tests and
20 model/cabin/report tests passed. Checks cover
one-hot routing, exact delay positions, short clips, both B inputs, matrix
floor versus mute, independent packed-EQ composition, six-input impulse FFT
agreement with the complex response, PSD units and report-window alignment.
A fresh C-renderer/report CLI smoke run also passes. Independent numerical
composition error: 7.30e-15.
