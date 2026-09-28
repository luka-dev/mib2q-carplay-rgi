# Oracle-Audio Renderer Stability Probe

_Prepared before reading the variant results, 2026-09-28. This is a non-deployable mechanism screen through the real host C renderer, not a car result._

## Question

The original full-40 oracle experiment supplied **true rear-eligible instrument audio** to the C object path at a constant mask drive of 1.0. It met the listed rear-share, protection and median depth-versus-level limits, yet **8/40 songs** still had at least one stem's one-second rear-share range above 0.30. This tests whether simple, shipped renderer knobs remove those residual swings without weakening the rear or exposing protected stems. Since the injected audio is perfect and the mask drive constant, extraction and frame-wise mask decisions cannot explain these eight particular failures.

## Frozen design

[`prepare_oracle_renderer_probe.py`](../../tools/stereo-upmix-render/prepare_oracle_renderer_probe.py) chose all eight original swinging songs plus 12 deterministic nonswinging controls from the same 40 DEV songs. The fixed selection, source track IDs and original 115 stem rows are in `D:\upmix\runs\oracle_renderer_probe\{manifest.json,split.json,baseline_stems.csv}`. Selecting on the failure endpoint makes this an enriched diagnostic set, **not an unbiased validation set**.

[`oracle_renderer_sweep.py`](../../tools/stereo-upmix-render/oracle_renderer_sweep.py) loads each selected MoisesDB song once, then runs the same real C renderer and stable gain for these five conditions:

| Condition | Change from shipped renderer, oracle audio injected at G 1.0 |
| --- | --- |
| `baseline` | Fresh render of the original condition |
| `rear_body0` | Remove the default −4 dB rear-body shelf |
| `rear_air_off` | Set rear air to −40 dB instead of −12 dB |
| `front_retention025` | Set front retention to 0.25 instead of 0.50 |
| `front_retention075` | Set front retention to 0.75 instead of 0.50 |

Before scoring variants, every baseline stem's median rear share and one-second range must reproduce the original CSV within **0.005**. Each result is cached per song; [`run_oracle_renderer_probe.ps1`](../../tools/stereo-upmix-render/pc/run_oracle_renderer_probe.ps1) refuses to start while the rapid queue, training or another stem harness is active. `compare_rapid_pair.py` pairs by song and bootstraps songs, not stem rows.

## Decision rule fixed before results

A knob deserves a full-40 rerender only if it resolves **at least three of the eight** original any-stem swing songs, adds **no new swinging control song**, and keeps the median rear share of guitar, keys, piano and bowed strings within 0.01 of the paired baseline while keeping voice, bass and drums at or below their goal limits. The median rear RMS must remain within 0.5 dB of the baseline, and the median guitar/keys/wind depth-versus-level slopes must stay within ±0.05. Failing any gate rejects the knob as a simple stability remedy; it does not rule out a new controller.

If one passes, rerender the original 40 and the untouched 20 DEV songs with exactly the selected setting. The full goal still requires causal ≤ 150 M MAC/s int8 measured on the unit, ≤ 85 ms extra latency, source-attributed vocal-entry ducking, and the owner's blind live-CarPlay preference. An oracle-audio knob screen alone proves none of those deployment or listening gates.

## Result

Pending. The CPU screen started only after the 10k rapid queue had drained.
