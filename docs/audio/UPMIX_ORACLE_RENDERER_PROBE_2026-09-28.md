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

A knob deserves a full-40 rerender only if it resolves **at least three of the eight** original any-stem swing songs, adds **no new swinging control song**, and meets the goal's median rear-share limits on this screen: guitar ≥ 0.15, keys ≥ 0.15, piano ≥ 0.10, bowed strings ≥ 0.14, voice ≤ 0.02, bass ≤ 0.06 and drums ≤ 0.08. The median rear RMS must remain within 0.5 dB of the baseline, and the median guitar/keys/wind depth-versus-level slopes must stay within ±0.05. Failing any gate rejects the knob as a simple stability remedy; it does not rule out a new controller. These screen thresholds are promotion filters, not proof of the full-40 or untouched-20 goal.

If one passes, rerender the original 40 and the untouched 20 DEV songs with exactly the selected setting. The full goal still requires causal ≤ 150 M MAC/s int8 measured on the unit, ≤ 85 ms extra latency, source-attributed vocal-entry ducking, and the owner's blind live-CarPlay preference. An oracle-audio knob screen alone proves none of those deployment or listening gates.

[`evaluate_oracle_renderer_probe.py`](../../tools/stereo-upmix-render/evaluate_oracle_renderer_probe.py) applies this gate to complete CSVs and writes `decision.json`. A self-comparison using the frozen baseline as every condition correctly reported 8 → 8 swings and rejected all four variants for resolving none; the actual variants were still running at that check.

## Result

The CPU screen finished cleanly on the same 20 songs: **115 stem rows in every condition**, and the fresh baseline reproduced each original song/stem rear share and one-second range within 0.005. The evaluator wrote `D:\upmix\runs\oracle_renderer_probe\decision.json`; the paired JSON files and C renders are alongside it.

| Condition | Any-stem swing songs / 20 | Resolved / new | Guitar | Keys | Piano | Strings | Voice | Bass | Drums | Rear RMS dBFS | Screen gate |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| Fresh baseline | 8 | — | 0.256 | 0.230 | 0.171 | 0.272 | 0.009 | 0.012 | 0.069 | −36.64 | Reference |
| Rear body 0 dB | 6 | 2 / 0 | 0.335 | 0.261 | 0.309 | 0.304 | 0.011 | 0.019 | 0.070 | −35.17 | Fail: only two swings resolved; +1.47 dB rear |
| Rear air −40 dB | **5** | **3 / 0** | 0.242 | 0.226 | 0.171 | 0.262 | 0.008 | 0.010 | **0.032** | −36.97 | **Pass to full 40** |
| Front retention 0.25 | 14 | 0 / 6 | 0.369 | 0.282 | 0.241 | 0.383 | 0.009 | 0.012 | 0.070 | −36.64 | Fail: six new swings |
| Front retention 0.75 | **1** | **7 / 0** | **0.176** | **0.179** | **0.122** | **0.187** | 0.009 | 0.012 | 0.068 | −36.64 | **Pass to full 40** |

Both promoted variants also keep median guitar, keys and wind depth-versus-level slopes within ±0.05. The bowed-string median is based on only **three** songs in this enriched screen, so it needs the six-string-song full harness. Front retention changes the amount subtracted from the front while leaving the rear RMS identical here: its 8 → 1 stability result comes with lower instrument rear shares. Whether 0.176 guitar share sounds clearly behind in the owner's cabin is unresolved. Removing the air bed sacrifices a small amount of instrument share and 0.33 dB rear level while reducing drum rear share substantially; its timbre also needs listening.

The exact two settings, plus a fresh baseline, are running on the original 40 DEV songs through [`run_oracle_renderer_full40.ps1`](../../tools/stereo-upmix-render/pc/run_oracle_renderer_full40.ps1). No full-harness, untouched-song, source-attribution, device or in-car verdict is claimed from this 20-song oracle screen.

**Holdout caveat:** this enriched screen used knowledge of all eight failures in the original 40 DEV songs. The other 20 songs in that set were not rendered under these knob variants, but they are not a strict untouched holdout for choosing the knobs. If the full-40 check passes, freeze and render 20 songs from MoisesDB's separate 24-song `test` split before inspecting their outcomes; report those separately rather than calling a DEV subgroup untouched.

That independent set has now been frozen, before the full-40 result: [`prepare_oracle_test_holdout.py`](../../tools/stereo-upmix-render/prepare_oracle_test_holdout.py), seed 1610, `D:\upmix\runs\oracle_renderer_test20\manifest.json`. It contains 20 of the 24 test-split tracks with **zero title or track-ID overlap** with the 40 DEV songs. [`run_oracle_renderer_test20.ps1`](../../tools/stereo-upmix-render/pc/run_oracle_renderer_test20.ps1) will compare the same fresh baseline, rear-air-off and retention-0.75 C conditions if the full-40 result warrants it. It remains a separate **TEST** holdout, not the goal's literally named untouched DEV subgroup.
