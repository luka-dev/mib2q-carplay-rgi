# Whole-upmix offline test — 2026-09-20

> Follow-up: [retained first-B spectral controls](UPMIX_RETAINED_B_2026-09-20.md)
> are now available through `full_retained_zero_aux` / `full_retained_aux_probe`.
> They replace the synthetic spectral controls in a NEW scenario with real
> common-header/config bindings and consistent mode5 pointers. Older `full_*`
> scenarios and results below remain historical neutral diagnostics, not upgraded
> retroactively. Current validation is704 tests (583 model +121 extraction).
>
> Newer [static numerical scenarios](UPMIX_STATIC_COMPLETION_2026-09-20.md)
> use `full_static_nearest_*` or `full_static_floor_*`: actual B log/pow and
> stored internal FFT table; separate aux/adaptation axes. They are explicit
> rounding hypotheses, not SHARC bit-exact or a recovered live preset.

The practical target is a repeatable run from stereo through the **actual host
C upmixer**, its quantized six-channel output, and an explicit electronic DSP
scenario. This is not another claim that static analysis recovered the running
vehicle or a calibrated cabin.

## Two distinct conclusions

1. C/upmix behavior: exercise the current source, HSSR model, guard model, S16
   and float synthesis, rear/bass/LFE/front-retention controls. Measure PCM6
   levels, near-rail samples and digital C output; keep the real generated WAVs.
2. Conditional amplifier response: compare the same candidates in a default-v7
   hypothesis and a composed DSP hypothesis. Record its controls, supplied state,
   retained coefficients and unknowns. Model overs are preserved, not normalized
   away. A completed calculation is **not** an accepted sound-quality verdict.

The host renderer runs native Mac AArch64 NEON and synchronous inference. The
QNX hook can use asynchronous worker control, a different target architecture,
transport and QSA scheduling. A host test does not certify target timing or
bit-identical live PCM. No deployment or vehicle contact is part of this test.

## Reproduce a single-track run

Use an existing 48 kHz signed 16-bit stereo WAV. The output directory must be new.
The runner, not the C executable, understands the colon-separated variants:

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
  --scenario baseline_v7 --scenario full_zero_aux --input-eq-bank 7 \
  --out output/my-upmix-e2e
```

Variant fields are `name:float_or_s16:surround:front_bass_db:lfe_gain_db:retention`.
The runner explicitly selects `--lfe assist` and the guard model. The real C
CLI defaults differ; do not omit these in manual reproductions. Float synthesis
still emits an S16 PCM6 WAV; it is not an unquantized float export.

Repeat `--input` for a corpus. `--save-output-wavs` additionally retains the
16-channel model output as float WAV without peak normalization. These config-
order outputs are not a stereo headphone simulation or a physical speaker map.
For `full_aux_probe`, auxiliary tones are deliberately synthetic, independent
of the stereo programme and identical across candidate/dry comparisons. They
test sensitivity to supplied aux/adaptive state, not music recorded in the car.

For a corpus, replace the script with `audit_upmix_end_to_end_batch.py` and add
`--jobs 7` (or fewer for a smaller machine); all other flags are the same. Each
track runs in an isolated subprocess with numerical-library threads limited to
one. The aggregate preserves the original per-track reports and verifies their
completion, axes, parameters and provenance. It cannot promote an interrupted
partial run to a complete result. Existing output directories are rejected.

## Measurement discipline

The dry reference is L/R-only PCM6 through the **same BANK1 scenario**, delayed
512 frames to match the main C path convention. It is not stock stereo/bank0.
The renderer can use an additional rear-scene delay; 512 is not a claim that
every output shares one delay.

With different A/B/aux delays, per-channel A-only realignment would compare
different mixtures/times. The new metrics instead use one simultaneous programme
window `[0,N)` for all 16 outputs and a separate explicit finite trailing window.
Leading latency stays in that measurement. Reported band powers are arithmetic
means within candidate output groups, **not coherent acoustic sums or SPL**.
Sample peaks are not true peaks or a proof of physical amplifier clipping.
Raw multichannel model WAVs can contain values above full scale. They are
analysis artifacts, not ready-to-play listening files; an explicitly bounded
monitoring/acoustic-render path is needed for auditioning.

Input zero-padding exercises the C and DSP tails within the stated duration;
it does not prove all IIR/adaptive state has settled. Programme and tail peaks
are retained separately. No automatic loudness matching, gain correction,
production preset selection or deployment follows from this report.

## Explicit composed scenario

`amp_test_scenarios.py` is a runtime factory, separate from test fixtures. It
requires an input-EQ bank and exports the choices and unknowns with each row:

- common input: supplied 6-to-8 mapping, zero tone controls and requested delay;
- first B: retained B01 bank1 tails/finish, mode5, supplied object97 gain 0.5,
  neutral **synthetic** spectral controls, successful on-time 1024-frame wrapper;
- second B: recovered input/post EQ, matrix/scalar/delay targets, raw controls
  zero, selected targets **assumed already delivered and settled**, supplied
  loader-count hypothesis 8/4/4 and zero histories;
- C4D17: hash-verified profile0 **initial** FIR, not a learned/current FIR;
- runtime: unity gains and a high diagnostic limiter threshold, not an invented
  attenuating fixture or recovered live protection settings.

`full_zero_aux` supplies zero independent auxiliary inputs and disables adaptive
updates. `full_aux_probe` changes both the auxiliary signals and adaptive-enable
state: it is a combined sensitivity test, not an isolated measurement of either
effect. Its independent tones are 63 Hz at 0.01 for graph11 and 71/113/173 Hz at
0.003 each for second-B inputs8/9/10; all stop at the programme-window boundary.
Never describe either scenario as the verified BANK1 vehicle preset.

## Using the results to tune

Compare candidates **within the same track and scenario**. Inspect the paired
woofer 60–120 Hz minus front-mid 500–2000 Hz change, absolute band levels,
per-output peaks, PCM6 near-rail counts and guard activity together. A smaller
bass/mid difference can mean restored mids, reduced bass, or both. It is not by
itself a better sound, a loudness-matched comparison or a safe hardware preset.

To test a new control choice, add another `--variant`; to test different music,
repeat `--input`. Extend `--seconds` and the finite tail for longer listening
segments. The short validation prefixes establish that the pipeline runs; they
do not cover a whole song, every DSP state or every upmix mode. All supplied
variants here use `hssr-universal` with LFE assist.
The retention candidates also change surround from 0.18 to 0.30; comparison with
the reference does not isolate retention alone. `retain75` versus
`retain75_bass1` isolates the supplied front-bass setting in this grid.

Historical music reports used different windows and, in some cases, path-delay
realignment. Their band numbers are not directly comparable to this short-prefix
simultaneous-window test. The earlier longer tuning/stress artifacts remain
separate evidence, not extra cells credited to the new composed scenario.

## What remains outside this test

Actual external DSP selectors, full spectral-parameter binding, callback timing,
some reset/error/allocation behavior, hardware rounding and acoustic responses
are not established by these scenarios. Full-cabin/subjective readiness must
remain false. The recovered coefficient tables make a conditional electronic
test more useful; they do not turn chosen unknown controls into observed facts.

Fresh build/test provenance is in
`build/upmix-e2e-20260920/build_provenance.json`. It records compiler, binary,
source/model hashes and local sanitizer/NEON results; it is not a QNX release
or a claim about the binary currently installed in the vehicle.

## Validation and interpretation

625 relevant Python tests pass: 504 model/cabin/runner tests and 121 extraction
tests. Fresh S16 and float C tests pass under ASan/UBSan; native NEON tests pass.
These are implementation checks, not a sound-quality certificate.

### Seven-track end-to-end result

Completed aggregate:
`output/upmix-e2e-music-parallel-20260920/report.json`
(SHA256 `77cdb0c849c20895405cc30620f111c1aae5f05370fa4b6ed621d7e0b606933f`).
Seven tracks × (four fresh C variants + dry BANK1) × two DSP scenarios = **70/70
rows**, `complete:true`. Input prefixes are six seconds, followed by 0.25 seconds
of zero padding; input-EQ bank7 is an explicit hypothesis. The aggregate retains
the seven verified shard reports and all 35 PCM6 WAVs/logs. Full 16-channel WAVs
are not saved for this corpus; rounded-output array hashes and metrics are.

Post-run checks independently verified all 169 unique referenced file
fingerprints, exact shard-to-aggregate equality, every PCM6 format/length, all
16×7 band matrices, paired deltas and the recomputed aggregate summary.
All 28 candidate PCM6 WAVs have zero near-rail samples and exactly zero digital
C samples. This does **not** imply silence at the physical centre after mixing.
All 21 float renders have complete guard telemetry; the seven S16 renders do
not emit those float-specific counters (`guard_telemetry_complete:false`, empty
guard map), so no S16 guard-activity duration is inferred.

Mean paired bass/mid tilt below is woofer60–120 minus front-mid500–2000 change
relative to dry BANK1 in the **same scenario**, not stock stereo or cabin SPL:

| Variant | v7 tilt, dB | Composed tilt, dB | v7 max peak, dBFS | Composed max peak, dBFS |
|---|---:|---:|---:|---:|
| S16 reference | +1.371 | +1.422 | +3.485 | +21.182 |
| Float reference | +1.370 | +1.417 | +3.088 | +20.895 |
| Float retention75 / surround0.30 | +0.554 | +0.569 | +3.088 | +20.906 |
| Same, front bass−1 dB | −0.356 | −0.344 | +2.995 | +20.545 |

Thus the test distinguishes candidate spectral balance and also exposes output
headroom risks that PCM6 rail checks alone miss. Ten of 35 v7 rows and 30 of 35
composed rows exceed0 dBFS (both counts include dry). The composed magnitudes
have the supplied-spectral-control caveat explained below; **no preset is
accepted or deployed** from this result. The short prefix is a working-pipeline
test, not whole-track or exhaustive-state acceptance.

The older `output/upmix-e2e-music-20260920/report.json` is an intentionally
interrupted sequential checkpoint (16/70, `complete:false`), superseded by this
complete parallel run. It is retained, not counted as additional coverage.

### Auxiliary/adaptive sensitivity

The completed auxiliary probe is
`output/upmix-e2e-aux-probe-20260920/report.json`: Deep Down Low, six-second prefix
plus 0.25 seconds of padding, input-EQ bank10 hypothesis, dry plus two candidates,
two scenarios, 6/6 rows. All six FLOAT32 16-channel WAVs are retained. The dry
whole-clip peak is +22.0898 dBFS with zero aux/adaptation disabled and +31.9946
dBFS with the independent probe/adaptation enabled. Independent WAV reads verify
the first two peaks as 12.72011185 and 39.78609848 linear full-scale units.
Post-run validation checked all 80 unique file fingerprints referenced by the
report, reread all six output WAVs, matched their FLOAT32 array hashes and
recomputed every stored metric exactly.

These large values are **not a vehicle clipping diagnosis**. A bounded independent
review found no obvious unit/dB or duplicate-gain error: the chosen first-B
six-tail has retained pre-gain +11 dB, final-gain +9 dB and shared finish +1 dB,
but the scenario supplies unity spectral branches rather than the missing real
spectral masks. On an 8192-frame diagnostic prefix, the dry bank10 common-input
peak grows from 0.935 to 1.768 FS, first-B reaches 7.340 FS and second-B stays
zero. Bank10 itself is non-neutral (approximately +5.94 dB at 60 Hz); bank7
common input is neutral. Adaptive cap15 permits up to +15 dB at the five-band
endpoint under the supplied controls. This explains the scale as plausible,
not a proof of the complete adaptive trajectory, hardware protection or actual
listening level. Never choose a compensating −22/−32 dB production gain from
this diagnostic fixture.

The aux test changes signals **and** adaptive enable together; its effect cannot
be attributed to one alone. It uses bank10, so comparing it directly with the
bank7 corpus would additionally confound input EQ.
