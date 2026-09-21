# Virtual Cabin tuning sweep — 2026-09-20

Local/offline experiments only: no vehicle contact, deployment, startup changes,
production-default selection or commit. This follows the
[corrected guard/retention implementation](UPMIX_GUARD_RETENTION_2026-09-20.md).

## What is being compared

Seven original60-second stereo clips, SHA256 checked against the corrected v7
baseline. C renderer uses the same HSSR main/guard blobs and source algorithm;
only host limiter diagnostics were expanded. The three overlapping0.40-rear
settings are checked against the previous final PCM hashes.

First sweep: retention0.50/0.75/1.00 × rear0.30/0.35/0.40, front bass0dB,
LFE assist0dB re4096, unity programme gain, linked input guard−1dBFS.
Second sweep around0.75/0.30: front bass0/−1/−2dB; LFE assist0/−6/+6dB or
off. `LFE off` also selects the existing higher front gain, so a separate
post-render channel3-only zeroing control isolates LFE summation itself.

Band statistics use the source5..55s window,512-frame upmix delay and known
A-output delay alignment. **Headroom checks use the whole60-second render**,
not just the band-analysis window. No IIR tail beyond the file is flushed.
All levels are electronic model levels, not cabin SPL. Bass/mid tilt means
woofer60–120Hz change minus front-mid500–2000Hz change versus dry BANK1.
The reference is dry L/R-only PCM6 through BANK1, never stock stereo/bank0.

The host reports observed contiguous attenuation runs, mean dB reduction over
all frames and fractions reduced more than3/6/10dB. Silent PCM makes reduction
unobservable and breaks a run; reported longest runs are not proofs of the
limiter's complete internal-state recovery time. This telemetry does not change
the soundcheck gain/limiter implementation.

## Result tables

Grid:63 fresh C float renders. Bass/LFE:42 fresh C float renders plus7 pure
LFE-zero controls. All input matrix buses obey the−1dBFS bound with stated
S16 quantization tolerance; no saved PCM rail hits. All21 overlapping grid
PCM hashes exactly match the previous final sweep: telemetry is audio-neutral.

Values below are means over7 tracks except the full-clip maximum peak and
the frame-weighted guard fraction. Rear delta is500–2000Hz versus dry BANK1;
it is actual modeled rear-door output, not the authored surround gain alone.

| Front retention | Rear gain | Bass/mid tilt, dB | Rear delta, dB | Max post-EQ sample peak, dBFS | Frames attenuated |
|---|---|---|---|---|---|
|50%|0.30|+1.553|0.000|+4.292|9.585%|
|50%|0.35|+1.803|+0.274|+4.268|10.491%|
|50%|0.40|+2.047|+0.567|+4.242|11.707%|
|75%|0.30|+0.679|+0.469|+4.481|11.344%|
|75%|0.35|+0.781|+0.757|+4.383|12.193%|
|75%|0.40|+0.880|+1.051|+4.330|13.355%|
|100%|0.30|−0.134|+0.933|+4.964|14.298%|
|100%|0.35|−0.169|+1.247|+4.964|15.357%|
|100%|0.40|−0.207|+1.557|+4.964|16.652%|

Retain three comparison candidates, **not production-approved presets**:

| Candidate | Tilt mean [track min,max], dB | Front mid delta, dB | Woofer60–120 delta, dB | Guard frames |
|---|---|---|---|---|
|75% / rear0.30 / bass0|+0.679 [−0.258,+1.117]|−1.359|−0.680|11.344%|
|75% / rear0.30 / bass−1|−0.215 [−1.155,+0.327]|−1.329|−1.544|5.028%|
|100% / rear0.30 / bass0|−0.134 [−0.835,+0.150]|−0.569|−0.703|14.298%|

75%/0.30 is the conservative candidate for rear output; bass−1 is the
bass-lean alternative.100%/0.30 retains more front mid, but raises rear output
and guard activity. This does not evaluate spaciousness, vocal image or taste.
Relative to the front-mid change, rear-mid rises1.828dB for75%/bass0,
1.819dB for75%/bass−1 and1.502dB for100%/bass0. Thus lower absolute rear
output at75% does **not** establish a more front-focused image after matching.
Centre-mid500–2000Hz changes track front-mid closely: centre/front differential
is only+0.015/+0.011/+0.017dB for these candidates. This band-power check does
not isolate vocals or prove a stable centre image at an actual listening seat.
At75%/0.30, bass−2 gives tilt−1.166dB, range[−2.081,−0.610]: it is consistently
bass-lean against this reference, so was not selected for the compact comparison.

### LFE is not a monotonic sub-volume control in the corrected model

At75%/0.30, mean sub20–60Hz change versus dry BANK1:

| LFE mode | Sub delta, dB |
|---|---|
|assist0dB|−1.381|
|assist−6dB|−0.933|
|assist+6dB|−2.191|
|CLI off (also higher front gain)|−0.198|

Pure channel3 zeroing after the flat render changes sub power by **+0.917dB
on average**, range[−0.054,+1.268] across tracks. This isolates cancellation
with other matrix inputs; it is not evidence of the car's current mode.
Do not interpret LFE_GAIN=0dB as LFE mute, or reduce LFE expecting the sub to
necessarily get quieter. Bass−1 reduces sub delta to−2.443dB; bass−2 to−3.578dB.

### Limiter duration and downstream headroom

For75%/0.30 bass0, attenuation occurs in11.344% of frames, but reduction
over3dB is only0.393% and over6dB0.0225%. Mean reduction over **all** frames
is0.069dB; the longest observed run is133.438ms and minimum observed gain
0.31979 (about9.90dB reduction). Short strong events can coexist with a small
whole-corpus average; these statistics are not a listening verdict.

Bass−1 reduces attenuation occupancy to5.028% and mean reduction to0.046dB;
longest run132.125ms. Nevertheless its post-EQ maximum rises from+4.481 to
**+4.701dBFS**: weaker input limiting can increase downstream peaks.
100%/0.30 reaches+4.964dBFS. These electronic sample levels are not proof of
physical amp clipping, but the input guard alone does not bound the full chain.
For this corpus and v7, post-guard fixed attenuation of at least5.681/5.901/
6.164dB respectively would put these maxima at−1.2dBFS, before final rounding.
This is not a universal safe gain for arbitrary songs or different DSP states.

## EQ and B sensitivity boundary

Two retained common-input hypotheses: bank7 and bank10, tone gains0, delay0,
explicit mapping[0,1,2,3,4,5,None,None]. They are EQ banks, not matrix-bank
numbers. Neither is asserted active in the car.

The first12s of each selected PCM6 file are processed; source2..10s measured.
For each EQ, compare dry BANK1 and all three candidates under seven cases:
B0; first B ±0.25×donor or +0.25 delayed96 samples; second B ±0.25×donor or
+0.25 delayed96 samples. First donor is the A ten-bus boundary with slots2/3
zero (slots8/9 already zero); second donor is A's split16 boundary before its
pair-mixer A weight. Known B weights/routes are then applied by AmpChain.
The two B paths are stressed separately; the other stays zero.

These are **arbitrary amplitude/phase sensitivity tests**, not reconstructed B
music, calibrated bounds, a complete B processor, or verified operating modes.
They do not cover every possible B lane/input, noise, adaptation, lifecycle,
wrapper timing, C4D17 mode or unknown physical mapping. Superposition is used
only for the fixed linear scenario and tested against direct time rendering.
Candidate deltas reference dry through the SAME EQ/B scenario.

All392 measurements completed:7 tracks ×2 EQ banks ×4 inputs ×7 B cases.
The table averages49 track/case values per EQ; this arbitrary ensemble is
not a probability distribution of real driving states. Ranges span the49
values, and peaks cover each complete12-second clip.

| EQ hypothesis | Candidate | Tilt mean [min,max], dB | Max post-EQ peak, dBFS |
|---|---|---|---|
|7|75% / bass0|+0.714 [+0.100,+1.307]|+5.026|
|7|100% / bass0|−0.050 [−0.175,0.000]|+5.026|
|7|75% / bass−1|−0.205 [−0.760,+0.344]|+4.933|
|10|75% / bass0|+0.716 [+0.146,+1.305]|+6.180|
|10|100% / bass0|−0.046 [−0.128,0.000]|+6.180|
|10|75% / bass−1|−0.205 [−0.720,+0.339]|+5.986|

The two lower-tilt candidates do **not** have a universal ordering:100%/bass0
has higher tilt than75%/bass−1 in56 of98 track/EQ/B comparisons, not all98.
Within these chosen B donors, changing B changes a candidate's paired tilt
relative to B0 by at most0.03550dB. This small result is not a bound for a real
unresolved B processor or differently excited lanes.

Absolute input-EQ sensitivity is much larger. On dry BANK1 with B0, bank10
versus7 raises modeled woofer60–120Hz by a mean **4.979dB** (track range
4.705..5.430), sub20–60Hz by **5.719dB** (5.157..5.955), and front-mid500–2000
by0.360dB (0.275..0.464). Flat user tone controls therefore do not by
themselves establish a neutral input EQ in these explicit scenarios. This
is **not** proof that bank10, or either tested bank, is active in the car.
The matched files'−1.2dBFS guarantee applies only to v7 B0/no-common-input
rendering; it must not be transferred to these other hypotheses.

## Level-matched comparison files

For each track, dry BANK1 and the three shortlisted PCM6 variants share a single
target500–2000Hz front-mid output power. Only constant attenuation is permitted,
applied **after** the already-rendered input guard. Target selection also bounds
every full-file v7 output sample peak at−1.2dBFS before final S16 conversion.
Saved WAVs are read again and rerendered: allowed matching error≤0.02dB and
maximum output sample peak≤−1.19dBFS. Dry input is delayed512 frames to align
its source with the authored candidates.

This is a front-mid electrical comparison, **not LUFS matching, equal perceived
loudness or a headphone/binaural cabin render**. Files contain6ch MOST-order
PCM(L,R,C,LFE,Ls,Rs); they are not16/19 physical speaker recordings. The gain
is per track/candidate for comparison, not a proposed fixed production setting.
No real listening quality verdict is made from these files.

All28 saved files passed reread/rerender verification. Maximum matching error
is0.00000456dB; maximum full-clip sample peak is−1.199895dBFS. Applied gains
range from−7.057dB to0. PCM directory:
`output/upmix-tuning-matched-final-20260920/pcm/`.
The final28 files were moved out of temporary storage without changing their
bytes; report paths were updated and hashes rechecked.

## Reproduction / artifacts

`tools/soundamp-virtual-cabin/audit_amp_tuning.py` has four explicit phases.
All output directories must be new. Reports retain commands, parameters,
hashes and proof boundaries; partial files have `complete:false`.
The grid/bass `analysis` object is inherited from the baseline, including its
`render_mode` description of reused baseline WAVs. Actual new rendering is
identified by each row's recorded `--float-render` command and renderer hash.

Authoritative reports:

- `output/upmix-tuning-grid-20260920/report.json` —63 renders.
- `output/upmix-tuning-bass75-20260920/report.json` —42 renders +7 LFE-zero controls.
- `output/upmix-tuning-stress-verified-20260920/report.json` —final EQ/B sensitivity.
- `output/upmix-tuning-matched-final-20260920/report.json` —28 verified comparison WAVs.

The earlier `upmix-tuning-stress-20260920` and `upmix-tuning-stress-final-20260920`
runs were intentionally stopped and remain incomplete. Final stress avoids
redundantly filtering zero input for isolated downstream B contributions;
both B injection paths are regression-tested against the upstream-enabled
chain. Completed overlapping tracks exactly match the earlier calculation.
`upmix-tuning-matched-20260920` is the earlier two-candidate comparison, not
the final three-candidate set.

Validation:421 Python tests (121 extraction/control +300 model/tools) pass;
float C tests pass under ASan/UBSan. No ARM/QNX target build or deployment.
Known unrelated installer failures in the full repository suite remain as
documented in the preceding guard/retention report; they were not fixed here.

```sh
OPENBLAS_NUM_THREADS=1 python3 tools/soundamp-virtual-cabin/audit_amp_tuning.py \
  grid --out output/upmix-tuning-grid-NEW

OPENBLAS_NUM_THREADS=1 python3 tools/soundamp-virtual-cabin/audit_amp_tuning.py \
  bass --retention .75 --surround .3 --out output/upmix-tuning-bass75-NEW

OPENBLAS_NUM_THREADS=1 python3 tools/soundamp-virtual-cabin/audit_amp_tuning.py \
  stress --grid output/upmix-tuning-grid-20260920/report.json \
  --names r75_s30 r100_s30 \
  --extra output/upmix-tuning-bass75-20260920/report.json --extra-names bass1 \
  --out output/upmix-tuning-stress-NEW

OPENBLAS_NUM_THREADS=1 python3 tools/soundamp-virtual-cabin/audit_amp_tuning.py \
  match --grid output/upmix-tuning-grid-20260920/report.json \
  --names r75_s30 r100_s30 \
  --extra output/upmix-tuning-bass75-20260920/report.json --extra-names bass1 \
  --out output/upmix-tuning-matched-NEW
```

The default host binary is `build/upmix-grid-20260920/renderer`; compile the
normal host renderer with the updated telemetry source. Intermediate sweep PCM
lives in private temporary directories referenced by report hashes; final
matched PCM lives in the chosen match output directory's `pcm/` subdirectory.

## What remains outside this completed numeric sweep

- No subjective winner, vocal-image or spatial-quality verdict: band powers
  alone cannot establish these. Matched files are the comparison material.
- No acoustic/seat/headphone prediction without measured transfer functions;
  electronic group power is not the coherent19-speaker sound field.
- No proof of current B, input EQ bank, C4D17 mode or physical routing in the
  car. More static recovery can refine explicit alternatives, not reveal
  externally selected runtime values by assumption.
- No universal downstream guard or final production gain has been selected.
  A fixed attenuation that passes this corpus and scenario is not a proof for
  arbitrary audio or all DSP states. Any later deployment/listening in the
  car requires lifting the user's current static-only constraint.

No production defaults, vehicle files, startup settings or commits were
changed by this sweep. Local host telemetry, analysis tools, tests and reports
are the additions in this step; prior C upmix changes remain as documented.
