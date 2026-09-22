# SoundAmp Virtual Cabin

Current relevant Python validation: **704 tests (121 extraction +583 model/cabin)**.

Newest [static full-path extensions and limits](../../docs-obsidian/Sources/UPMIX_STATIC_COMPLETION_2026-09-20.md):
opt-in source-derived log/pow + internal FFT table, full target publication and
mode5 coefficient access schedule. `full_static_nearest_zero_aux` selects the new
numerical reference; `floor` is a separate explicit FIX hypothesis. Dynamic
audio transitions and complete pointer alias closure are NOT established.

Previous: [retained first-B spectral binding](../../docs-obsidian/Sources/UPMIX_RETAINED_B_2026-09-20.md).
`full_retained_zero_aux` and `full_retained_aux_probe` use real common-header
coefficients, spatial controls,74-band map and firmware-derived transform tables;
the level-mask min/max binding bug is fixed. Live state/hardware rounding/cabin
acoustics remain unverified. Old neutral diagnostic scenarios are preserved.

Practical whole-upmix entry point:
[C → PCM6 → explicit DSP → paired metrics](../../docs-obsidian/Sources/UPMIX_END_TO_END_2026-09-20.md).
`audit_upmix_end_to_end.py` performs fresh C renders; the `_batch.py` companion
adds isolated parallel track runs and verified aggregation. Runtime scenarios
are separate from test fixtures. Positive model peaks are preserved; unknown
spectral/live controls remain explicit, not a production preset recommendation.

Latest third-pass closure: [effective first-B tail binding and complete-stage
synthetic wiring test](../../docs-obsidian/Sources/SHARC_B_TAIL_BINDING_2026-09-20.md),
[second-B control targets/mode1](../../docs-obsidian/Sources/SHARC_SECOND_B_2026-09-20.md),
[callback/transition control](../../docs-obsidian/Sources/SHARC_B_WRAPPER_CONTROL_2026-09-20.md),
and [pointer/smoothing proof boundaries](../../docs-obsidian/Sources/SHARC_B_BINDING_2026-09-20.md).
See `output/upmix-composed-static-20260920/report.json`: explicit scenarios for
all electrical stages, not a vehicle preset or measurement. Default v7 unchanged.

Previous static closure: **536 tests pass (121 extraction +415 model/cabin)**.
[C4D17 v3 correction and calibration composition](../../docs-obsidian/Sources/SHARC_COMPLEX_CALIBRATION_2026-09-20.md)
fix detector stride16 sampling; corrected adaptive reports exist for both profiles.
[Second-B aux reference](../../docs-obsidian/Sources/SHARC_SECOND_B_2026-09-20.md) is integrated
into AmpChain opt-in v9 and the cabin bridge. [Nonzero first-B EQ designer](../../docs-obsidian/Sources/SHARC_B_EQ_DESIGN_2026-09-20.md)
and [conditional two-bank timing](../../docs-obsidian/Sources/SHARC_B_WRAPPER_TIMING_2026-09-20.md)
also have executable references. Default v7 metadata/rendering is unchanged.
No live-state or measured acoustic fidelity claim.

Previous static closure: [B effective-output/scalar bindings](../../docs-obsidian/Sources/SHARC_B_BINDING_2026-09-20.md),
[C4D17 calibration timing and second-B ancestry](../../docs-obsidian/Sources/SHARC_COMPLEX_TIMING_CLOSURE_2026-09-20.md),
[electronic/acoustic bridge and legacy matrix repair](../../docs-obsidian/Sources/UPMIX_CABIN_BRIDGE_2026-09-20.md),
and [machine-readable readiness gates](../../docs-obsidian/Sources/UPMIX_MODEL_READINESS_2026-09-20.md).
490 relevant Python tests pass (121 extraction +369 all model/cabin tests).
No complete/live cabin fidelity claim: actual acoustic responses and selected
state are still absent. The legacy documentary matrix is now corrected too;
the obsolete six-bus `cabin_model.py` requires historical-only opt-in.

The current conditional electronic model is
[`amp_dsp_chain.py`](amp_dsp_chain.py), version `bank1-known-electronics-v7`.
**v7/v8 correct a matrix transpose error in v5/v6**: A07/RAM rows are outputs,
not inputs. Old speaker-group bass numbers and “sub only LFE” calculations are
superseded by the [seven-track rerender and controls](../../docs-obsidian/Sources/UPMIX_MATRIX_RERENDER_2026-09-20.md).
See [proof and coefficient side paths](../../docs-obsidian/Sources/SHARC_MATRIX_CONTROL_2026-09-20.md).
It includes the recovered second pair mixer, both pre-EQ delay vectors,
packed post-EQ binding and the input-matrix floor. Explicit runtime scenarios
can include the recovered gain/limiter stages; defaults do not guess their
live state. [`amp_complex_path.py`](amp_complex_path.py) composes C4D17's fixed
full-mode reference as an optional AmpChain scenario with required explicit
auxiliary audio. Default remains identity; mode2 calibration has a separate
explicit-state reference in `amp_complex_calibration.py`, not a music-mode
preset. Live/hardware equivalence is unverified. `audit_amp_complex_scenarios.py` runs synthetic sensitivity
checks using hash-verified initial FIR banks, not a live bass diagnosis.
V5 corrects C4D17's dB conversion to `/20` from actual instruction constants;
v4 adaptive reports are superseded. `amp_complex_modes.py` covers lifecycle/
calibration control and noise core; `amp_b_filters.py` covers isolated B-library
filter math. `amp_b_decorrelator.py` adds the fixed two-channel delay/EQ/all-pass
tail; `amp_b_spectral.py` adds isolated magnitude/normalization and sum/difference
helpers; the composed reference is described below. These B helpers require explicit
inputs/parameters and do not replace AmpChain's default B=0 hypothesis. None
claims the full remaining audio graph. See
[`SOUNDAMP_FULL_DSP_MODEL.md`](../../docs-obsidian/Sources/SOUNDAMP_FULL_DSP_MODEL.md) for the
current chain, proof boundaries and tests. `amp_music_report.py` exports the
model version and assumptions beside every report and aligns analysis windows
by known path delays. The documentary fan-out and measured-BRIR tools described
below remain separate; they do not represent this newer electronic chain.

`amp_b_output.py` extends fixed B output math to six/four-channel branches and
the mode2/5 merge/eight-channel finish, accepting supplied boundary audio.
`amp_b_adaptive.py` covers two adaptive band masks, spectral subtraction and
temporal/frequency smoothing, with explicit band maps, parameters and histories.
`amp_b_transform.py` adds real-FFT wrapper packing/scales and ideal sine-window
overlap: neutral identity processing is unity delayed 512 samples. The complex
FFT is a numpy primitive, not instruction emulation. `amp_b_spatial.py` composes
130771's successful spatial path, with explicit state/parameters/selectors:
statistics, coherence, separator, diffuse mask and residual blend.
`amp_b_pipeline.py` now joins the spectral wrappers and fixed mode2 tails/finish
from two prepared PCM inputs to eight B outputs, with explicit parameters and
state. Pre→four-tail, post→six-tail, main→two-tail; post uses original spectra,
not main-filtered audio. Other modes/input preparation, lifecycle, live state
and hardware validation remain open. Normalization's old `supplied_power`
bypass was wrong and is removed: `power_workspace` is overwritten scratch.
These references do not turn the default music report into a full-B render.
Activation audit correction: BC9EC==1 clears, !=1 permits the library call in
state3; the earlier polarity was reversed. Boot0 is not a B-silence proof.
Host20001 selects process124DF3;20003 selects124D64 copying stereo to four pairs.
Bulk queue arg1 includes event5 bypass then44 process, not proven live completion.
Library selector init2/V8502/5/5 is separate from callback inhibit and graph
handler. See remaining-block audit before choosing any vehicle scenario.
`BMode5FixedReference` now adds mode5 preparation (S0+g*S2,S1+g*S2), with
explicit effective object+97 linear gain, before the same spectral/tail/finish
path. Inputs are graph24/23/22 boundary audio, not raw MOST L/R/C. Bank1→mode5
is now linked by the shared V850 bank byte; live delivery/state still unknown.
Gain loading is traced: source+43→object+97, loader0.5 / conditional B01
0.5011872053. Normal interpolation starts at object+299 and skips this gain.
`amp_b_input_eq.py` adds the fixed upstream12618C kernel: per-channel gain then
three-section bank EQ, records [-a2,-a1,b2,b1] with b0=1, no channel mixing.
Effective parameters/history are explicit; no automatic loader/live preset.
The A07 matrix is AFTER B. `amp_b_upstream.py` now binds an explicit settled
input scenario: delay/prefilter/gain/shared EQ/bank EQ. Calibrated writer sources
and selector rule are exported as `input_path_profiles`. This path is common
to A and B:16 banks include a55Hz peak up to+20.5dB even with tone gains zero.
No live-bank inference. C4A54's delay/TCB path has an independent ring-DMA test.
`audit_amp_input_scenarios.py AUDIT --output REPORT` renders32 fixed transfer
scenarios. See [input-path findings](../../docs-obsidian/Sources/SHARC_INPUT_PATH_2026-09-20.md).
AmpChain now joins common input to BOTH A/B through explicit `upstream_scenario`
and optional `first_b_scenario` (mode5 reference, supplied histories and wrapper
delay). Input mapping is mandatory and labelled unproven. The opt-in model
reports `bank1-electronics-v8-explicit-common-input`; defaults remain v7/B=0.
`amp_music_report.py --upstream-scenario SCENARIO.json` includes common input
with B still zero. `audit_amp_common_input_music.py` compares existing PCM6
clips across explicit scenarios; first scenario is a reference, never stock.
API and two-track conditional results are in the input-path findings above.
Static-only continuation adds `amp_input_control.py`: normal source level,
override, mute gates and cached EQ-selector commit. gp−77F8 is a smoothed
control word, not PCM energy; normal EQ enable initializesA5. A bounded
decoded-V850 interpreter verifies196 target-calculation cases. See
[level source audit](../../docs-obsidian/Sources/SHARC_LEVEL_CONTROL_2026-09-20.md).
Trim writers and override-mode dispatcher are now bound. First-B API25 uses
the unsmoothed target, unlike common EQ; pending/effective B controls have an
explicit defer branch. `amp_b_control.py` adds signed-side curve interpolation
and nine-slot aggregation: slot6 controls four/six/two branch linear gains.
Loader/B01 zero API25 curves yield zero level contribution, NOT zero B.
All9 branch-gain slots are now bound; B01 has nonzero curves for APIs18/19/22/27.
`branch_gain_targets` requires all helper coordinates and prior slot banks;
no current preset is inferred.123 synthetic +260 B01 decoded-SHARC cases
verify curve math (host divide/sqrt stand-ins).
`api18_19_parameter_targets` additionally binds all15/12 scalar destinations,
including two-tail delays and four EQ design records, with explicit shadow
selectors and prior slots. Extracted zero-gain EQ stays identity only under
explicit zero-other-slot assumptions; nonzero designer remains unsupported.
434 additional extracted-curve comparisons cover this extension.
All four B calibration banks and API17 replacement/rebase/effective replay are
now bound. B calibration bank1..4 is NOT input matrix bank0..2. See
[configuration audit](../../docs-obsidian/Sources/SHARC_B_CONFIG_2026-09-20.md) and generated
`b_config_control.json`. Explicit helpers cover normal API17 candidates and
branch-gain replay; slots are retained and replay25 uses raw effective value.
`amp_b_mode_control.py` now binds API16/20 pointer writes into effective config.
Shared pointers are last-applied-wins; explicit prior state is required even with
known final selectors. AUTO20=6 resolves before deferred shadow storage.
Independent decoded execution checks all five non-AUTO pointer paths.
Pointer1161 now resolves to post spectral crossfeed: sum/identical mono unchanged
by this stage. All eight image banks and decoded crossmix caller math tested.
Four consumers1162..1165 remain unresolved despite a no-direct-read literal census;
they are not silently modelled as inactive filters.
`amp_b_indexed_control.py` adds canonical extended-write effect plans: explicit
raw state, both direct stores and ordered call arguments. Indexed controls can
rebind both modes using effective20; source-store mutates source RAM with CRC.
No complete FIFO/callee/audio emulator or real-command-delivery claim.
The [outer request audit](../../docs-obsidian/Sources/SHARC_OUTER_CONTROL_2026-09-20.md) binds A21
raw forwarding and API18's dedicated writer. `api18_request_coordinate` adds only
the accepted signed−12..12→clamped−9..9 projection, checked against decoded
instructions for all256 bytes. No full packet/event model or UI label is implied.
`amp_matrix_control.py` now covers AA644 row/ramp state and both coefficient
writers with explicit inputs, verified against bounded decoded execution.
`audit_amp_matrix_music_control.py` verifies cached report comparability, reinserts
only the old transpose and zeroes only LFE in memory. No fresh upmix or vehicle I/O.
`audit_amp_midrange.py` decomposes fixed-path per-input powers and signed L/R
cross terms, checks closure against the v7 music report, and renders float-only
diagnostic front blends. See [results and stale C guard warning](../../docs-obsidian/Sources/UPMIX_MIDRANGE_ATTRIBUTION_2026-09-20.md).
`audit_amp_retention_candidates.py` runs fresh S16/float C candidates on the
hash-verified seven-track source set. [Corrected C guards and35-render results](../../docs-obsidian/Sources/UPMIX_GUARD_RETENTION_2026-09-20.md)
include downstream peak limits and full-suite installer failures.
`audit_amp_tuning.py` adds a9-point retention/rear grid, bass/LFE alternatives,
whole-clip downstream headroom, limiter telemetry, explicit EQ/B sensitivity
and reread-verified level-matched PCM6 files. See the
[105-render tuning sweep](../../docs-obsidian/Sources/UPMIX_TUNING_SWEEP_2026-09-20.md).
Next: candidate listening/selectivity evaluation (not a model verdict);
unbound pointer consumers,
outermost request semantics, API29, nonzero EQ designer,
B-target composition, wrapper timing and physical mapping; static only.
The preceding tuning step had421 Python tests (121 extraction +300 model/tools),43 V850 listings. No automatic replacement of fixed-bank
AmpChain scenarios, no inference of current vehicle state.

Host-side documentary/static model of the known Audi J525/B&O 9VS signal path
for evaluating HSSR and authored 5.1 without changing or recording the car.

Newer research is recorded in
[`UPMIX_STATIC_READINESS_AND_SOUNDCHECK.md`](../../docs-obsidian/Sources/UPMIX_STATIC_READINESS_AND_SOUNDCHECK.md).
The new [`digital_stages.py`](digital_stages.py) implements isolated verified
selectors, split-half pair mixers, explicit gain-row selection and packed
output-EQ math with caller-supplied coefficients. Ordered execution respects
reused buffer IDs and rejects unknown stages. These kernels are **not yet
connected into a complete end-to-end graph**. No identity transfer is invented
for the unknown complex processor. The equal-power documentary fan-out is
unchanged; it does not gain OEM-calibration status from these additions.

The checked-in development target is **stock MMI 3D OFF, Focus All, Surround
minimum**; Focus/Surround remain assumptions, not confirmed live values. The
legacy `symphoria_3d_enabled=false` JSON field denotes MMI OFF only, not full
DSP bypass. Reports now carry the full state/evidence annotation. The model
deliberately separates facts from the parts that still require a
different missing static artifact or an acoustic measurement:

- proven: 48 kHz, six input positions, V850-selected SHARC profile-0/bank-1
  8x8 input matrix, gain-floor behaviour, all 19 service-manual speakers and
  all 16 J525/T38 amplified output circuits;
- inferred for the documentary render: which semantic logical bus fans out to
  which front/centre/sub/rear circuit;
- unknown: the internal SHARC output index, OEM gain/delay/crossover/EQ per
  circuit, speaker aim/polarity, cabin transfer functions, and the current
  Surround/Sound Focus control values.

The `circuits` mode makes the inferred boundary explicit. It uses equal-power
fan-out only as a neutral test normalization; it never labels that fan-out as
recovered Audi calibration. The result is useful for electrical topology and
rear/front energy auditing, but is not a speaker-perfect acoustic simulation.

## Requirements

Python 3 and NumPy. Input and response files are 48 kHz signed 16-bit PCM WAV.
The input order is `L R C LFE Ls Rs`, matching `stereo_upmix_render`.

## Inspect the target

```sh
python3 tools/soundamp-virtual-cabin/soundamp_virtual_cabin.py \
  inspect --speakers --circuits
```

The JSON target is
[`audi_a5_f5_9vs_3d_off.json`](audi_a5_f5_9vs_3d_off.json). Its normalized
speaker coordinates are schematic drawing coordinates only. The service
manual proves the named locations, but it does not provide acoustic distances
or driver aim angles.

## Run an HSSR file through the proven J525 matrix

First create a six-channel render, for example:

```sh
cc -std=c99 -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -Ihook/audio \
  tools/stereo-upmix-render/stereo_upmix_render.c \
  hook/audio/soundcheck.c \
  hook/audio/stereo_upmix.c hook/audio/hssr_dsp.c hook/audio/hssr_neon.c \
  -lm -o build/stereo_upmix_render

build/stereo_upmix_render \
  --mode hssr-universal --model models/hssr-selection-v11/hssr_guard.bin \
  --lfe assist --surround 0.25 \
  input-stereo.wav build/hssr-universal.wav
```

Then apply the exact amplifier input matrix:

```sh
python3 tools/soundamp-virtual-cabin/soundamp_virtual_cabin.py matrix \
  build/hssr-universal.wav build/hssr-universal-j525-buses.wav
```

This creates:

- an eight-channel PCM file ordered as
  `Bus_L Bus_R Bus_C Bus_LFE Bus_Ls Bus_Rs Bus_7 Bus_8`;
- `build/hssr-universal-j525-buses.wav.report.json`, containing pre-clipping peak,
  RMS, rear energy fraction, rear/front ratio and per-bus clipping counts.

The eight-channel WAV has channel mask zero on purpose: Bus 7/8 have no honest
standard speaker label in the current 5.1 target.

## Expand to the documented 16 J525/T38 output circuits

```sh
python3 tools/soundamp-virtual-cabin/soundamp_virtual_cabin.py circuits \
  build/hssr-universal.wav build/hssr-universal-j525-circuits.wav
```

This applies two deliberately separate layers:

1. The exact 8x8 matrix recovered from the V850 firmware.
2. A labelled architectural inference from each logical bus to the speaker
   circuits in its matching front/centre/sub/rear zone.

The 16 output channels follow the `output_circuits` order in the JSON config.
Every T38 pin pair and connected `Rxxx` loudspeaker is taken from Audi Current
Flow Diagram No. 45. Shared electrical pairs and the two R211 subwoofer pairs
are preserved. The report includes per-circuit RMS/peak, grouped front/rear
energy and every evidence limitation.

Each circuit also carries an `inferred_amplifier_channel` from 1 through 16.
That numbering combines V850 service-table groups with Audi's published
later-generation J525 channel convention. It is a useful static-analysis
hypothesis, explicitly not a direct proof for this MIB2 amplifier.

The inferred fan-out divides each logical bus by `sqrt(number_of_destinations)`.
That preserves aggregate electrical test energy and prevents a four-way front
fan-out from looking 6 dB louder simply because it was duplicated. It is not
an estimate of Audi's individual loudspeaker gains.

## Optional measured acoustic extension

Generate the sequential six-channel measurement file:

```sh
python3 tools/soundamp-virtual-cabin/soundamp_virtual_cabin.py probe \
  build/cabin-driver/probe-6ch.wav
```

The default is deliberately restrained at -30 dBFS and the command refuses
anything above -18 dBFS. Play it only while parked, starting with low vehicle
volume. Record the driver's-head position as unprocessed 48 kHz/16-bit stereo
with a binaural microphone or calibrated two-microphone fixture. Keep every
car setting fixed. Most importantly:

- Symphoria/3D Off;
- fixed Surround value;
- fixed Sound Focus;
- neutral Bass/Mid/Treble;
- fixed volume and microphone geometry.

Convert the unedited recording into six end-to-end responses:

```sh
python3 tools/soundamp-virtual-cabin/soundamp_virtual_cabin.py deconvolve \
  build/cabin-driver/driver-capture.wav build/cabin-driver/responses \
  --probe-manifest build/cabin-driver/probe-6ch.wav.probe.json
```

The sync chirp removes arbitrary recording start offset. All responses share
one storage scale, so their relative channel gains and path delays are kept;
the generated `brir.json` restores that common scale. Do not individually
normalize the response WAVs. Fill the actual seat, Surround, Sound Focus and
microphone calibration into the generated manifest before treating it as a
repeatable measurement.

For `domain: "most_inputs"`, each stereo response is end-to-end and already
contains the J525 matrix and every downstream speaker path. Therefore the
renderer does **not** apply the matrix a second time:

```sh
python3 tools/soundamp-virtual-cabin/soundamp_virtual_cabin.py binaural \
  build/hssr-universal.wav build/hssr-universal-driver-headphones.wav \
  --brir-manifest build/cabin-driver/responses/brir.json \
  --gain-db -6
```

The convolution is block FFT overlap-add. It works on full tracks without
loading the programme audio into memory. The output report records the IR
domain, FFT size, peak and clipping count.

`domain: "logical_buses"` is also supported for future direct bus-response
measurements. In that mode the tool first applies the recovered 8x8 matrix and
then convolves all eight logical buses.

## What the documentary model can and cannot validate

It can inspect `hssr-universal` under the exact input matrix, expose rear-bus level and
headroom, and produce a repeatable headphone audition once measured responses
exist.

It cannot claim speaker-perfect acoustics from documentation alone. Static
evidence still does not bind every recovered crossover/EQ/gain value to a T38
circuit, and public documents do not contain the speaker directivity or cabin
impulse responses. Consequently no synthetic headphone preview is presented
as the sound of the real cabin.

## Regression tests

```sh
python3 -m unittest \
  tools/soundamp-virtual-cabin/test_soundamp_virtual_cabin.py
python3 -m unittest discover -s tools/soundamp-virtual-cabin -p 'test_*.py'
```

The tests lock the 3D-Off constraint, 19-speaker inventory, all 16 unique T38
pin pairs, recovered matrix orientation, 6-to-8 and 6-to-16 renders,
equal-power documentary fan-out, and the rule that an end-to-end MOST-input
BRIR must not receive the OEM matrix twice.

Five additional digital-stage tests cover mute versus the matrix floor,
split-half mixing, explicit OFF-row/gate hypotheses, packed EQ sign/order and
state continuity, and schedule-local buffer reuse. Supplied five-word EQ
coefficients are required; OEM design-record conversion and coefficient
transition timing are still unimplemented. See the
[float candidate results](../../docs-obsidian/Sources/UPMIX_FLOAT_STAGE1.md) for the first
upmix integration: a detector using only the proven input matrix.
