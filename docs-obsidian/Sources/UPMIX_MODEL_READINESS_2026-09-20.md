# What is ready for simulation — evidence contract, 2026-09-20

Static/offline only. No vehicle contact, deployment, preset/default changes, or
new measurement is implied by this work.

The practical [whole-upmix test runner](UPMIX_END_TO_END_2026-09-20.md) now joins
fresh C rendering, PCM6, explicit composed DSP scenarios and simultaneous-window
metrics. This is distinct from the older392-cell evidence contract below:
neither report inherits coverage from the other. The original `full_zero_aux`
music scenario contains neutral supplied spectral controls; the newer
`full_retained_*` scenarios replace them with the retained source bindings.
Both still assume effective state and are not a recovered vehicle preset or
measured acoustic response.

`tools/soundamp-virtual-cabin/amp_model_readiness.py` produces a machine-readable
readiness report instead of promoting a test count or a `measured` label into a
claim that the virtual cabin is the real car. It checks artifacts that exist;
it does not fabricate measurements or certify their truth.

## Reproduce the current audit

```sh
python3 tools/soundamp-virtual-cabin/amp_model_readiness.py \
  --report output/upmix-tuning-grid-20260920/report.json \
  --coverage output/upmix-tuning-stress-verified-20260920/report.json \
  --output output/upmix-model-readiness-20260920/report.json \
  --require stress-coverage

python3 -m unittest discover -s tools/soundamp-virtual-cabin \
  -p test_amp_model_readiness.py -v
```

The completed sensitivity artifact is **stress-verified**, not the older
`stress-final` checkpoint. The audit found all **392/392** cells: seven tracks,
two EQ scenarios, four variants including dry BANK1, seven synthetic B probes.
It checks the source-grid SHA/completion, common-input audit SHA, exactly one
matching hash-pinned file per declared EQ scenario, and a compatible hash-pinned
extra report when a candidate comes from outside the grid. Candidate names must
be unique; extra tracks, banks, variants and B probes fail. Each cell requires
16 output peaks, a 16×7 band matrix, exactly the six expected groups with finite
seven-band vectors/peaks, and whole-clip peak/frame counters. Candidate source
variants and source-track identity must agree across the grid and extra report.
Missing cells fail even if the report says `complete:true`. These are arbitrary
B-boundary probes, not recovered live B audio, an exhaustive DSP state space or
calibrated worst-case bounds. The original rendered length/window stays in the
readiness artifact.

Current results:

| Claim | Result |
|---|---|
| Conditional electrical comparison | available |
| Declared EQ/B sensitivity grid | complete, 392/392 |
| All electrical stages explicitly scenarized | no, default v7 still has omissions/hypotheses |
| Measurement evidence package | absent |
| Actual vehicle state verified | no |
| Full cabin fidelity or subjective optimum established | no |

## Electrical boundary

The contract examines the model metadata actually attached to the report. It
separately flags absent common-input, first-B, second-B, C4D17 and runtime
scenarios. It preserves all model warnings. An explicit scenario closes an
omission in the *chosen calculation*, not uncertainty about the running car.
The second-B flag is now reachable through a real30-node numerical reference:
AmpChain opt-in v9 takes `second_b_scenario` and independent
`second_b_auxiliary` PCM. See [integration contract](SHARC_SECOND_B_2026-09-20.md#ampchain-and-cabin-bridge-integration).
This does not promote default v7 or historical tuning reports to all-stage
coverage, and does not change the full-fidelity result. Current relevant test
count is704 (121 extraction +583 model/cabin), not a fidelity certificate.

The [new static extensions](UPMIX_STATIC_COMPLETION_2026-09-20.md) add actual
log/pow algorithms, a stored-table internal FFT option and separate publication/
mode5 coefficient scheduling. Static unknowns, dynamic transition audio and
hardware-exact arithmetic remain distinct gates, not implied by all-stage presence.

The newer [retained spectral scenario](UPMIX_RETAINED_B_2026-09-20.md) binds
common-header masks, real band map, spatial state and transform tables instead
of synthetic unity controls. It still cannot establish current selector/history,
hardware arithmetic or measured cabin fidelity. The external speaker workbook
does not fill those missing evidence categories.

The [all-stage synthetic composition](SHARC_B_TAIL_BINDING_2026-09-20.md)
now supplies every electrical scenario, including both internal B references,
mode1 auxiliary gain entry, C4D17 and limiters. Its presence gates are all true;
actual vehicle state and full fidelity are still false. Its zero boundary-
injection error verifies wiring only. The392-cell historical grid uses different
default-v7 scenarios and cannot be attributed to this new composed fixture.
Candidate speaker labels do not establish the physical pinout. A mathematical
float reference is not bit-equivalence with SHARC or a model of amplifier and
speaker distortion/protection.

The legacy `soundamp_virtual_cabin.py` documentary fan-out remains a distinct
model, not the v7/v8 electronics path. Its physical labels and equal-power
fan-out are not factory gain/delay/EQ calibration. This audit also exposed its
stale input-row matrix convention; the companion change corrects it to output
rows and cross-checks against AmpChain. Correcting that matrix
does not turn the documentary fan-out into the complete electrical/acoustic
chain.

## Acoustic bridge and evidence

The new `amp_cabin_render.py` bridge has two explicit domains:

- `most_inputs`: six end-to-end transfer responses; AmpChain must **not** be
  applied again. A measured response includes OEM DSP, transducers and cabin
  at the capture state, within the validity of a fixed linear IR model.
- `post_amp_config_outputs`: sixteen `cfg00`…`cfg15` transfer responses;
  AmpChain must precede them. Responses must be tied to actual config-order
  outputs, not inferred from candidate speaker names.

To audit a bridge report, pass `--acoustic-report FILE.report.json` and optional
`--evidence evidence.json`. The readiness tool reopens the manifest through the
bridge validator: every response SHA, 48 kHz stereo finite waveform and exact
channel order is checked again. A historical `responses_validated:true` is not
enough. The manifest hash and report metadata must match; MOST plus a repeated
AmpChain fails the stage-order gate. The report must also have `model:null` for
MOST and an explicit model object for post-amp, regardless of its stage-order
label. Synthetic responses never pass the
declared-measurement gate.

An evidence manifest has the following shape. Paths are relative to this
evidence JSON; every `sha256` is the SHA-256 of the actual referenced file.
These are **schema examples, not measurements**:

```json
{
  "seat": "driver",
  "capture_state": {"bank": 1, "bass": 0},
  "capture_recording": {"path": "capture.wav", "sha256": "REPLACE_WITH_ACTUAL_SHA256"},
  "state_record": {"path": "capture-state.json", "sha256": "REPLACE_WITH_ACTUAL_SHA256"},
  "microphone_calibration": {"path": "microphone-calibration.json", "sha256": "REPLACE_WITH_ACTUAL_SHA256"},
  "config_order_mapping": {"path": "config-output-mapping.json", "sha256": "REPLACE_WITH_ACTUAL_SHA256"}
}
```

`config_order_mapping` is required only for post-amp responses. Seat and full
capture-state object must match the bridge manifest exactly. The evidence
package is ready **for review** only when all gates pass. Hashes guarantee
identity, not that the capture, mapping, calibration or recorded state is true.
The contract therefore never silently promotes this to independently verified
hardware fidelity. This prevents synthetic unit-test fixtures or copied target
settings from masquerading as actual measurements.

`--require cabin-evidence` exits 2 until the review package is complete;
`--require full-fidelity` deliberately exits 2: this tool cannot establish that
claim. Without `--require`, exit 0 means the audit itself completed, not that all
claims are true. JSON `claims`/`gates` are authoritative.

## What still needs evidence, rather than more static arithmetic

Static work can close graph bindings, coefficient selectors, kernel behavior,
and conditional timing. It cannot prove which externally selected state ran in
the car, recover cabin/ear transfer functions from firmware, or choose the
listener's preferred tonal balance. Multi-seat response, capture level and
linearity, long dynamic histories, nonlinear loudspeaker/amp behavior, timing
alignment and headphone reproduction remain separate validation topics. The
bridge preserves FIR tails; finite electronic zero padding does not prove all
IIR/adaptive state has settled. No current readiness claim covers those gaps.
