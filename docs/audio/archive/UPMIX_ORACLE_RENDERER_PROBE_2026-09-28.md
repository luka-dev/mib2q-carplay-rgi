# Oracle-Audio Renderer Stability Probe

_Prepared before reading the variant results, 2026-09-28. This is a non-deployable mechanism screen through the real host C renderer, not a car result._

## Question

The original full-40 oracle experiment supplied **true rear-eligible instrument audio** to the C object path at a constant mask drive of 1.0. It met the listed rear-share, protection and median depth-versus-level limits, yet **8/40 songs** still had at least one stem's one-second rear-share range above 0.30. This tests whether simple, shipped renderer knobs remove those residual swings without weakening the rear or exposing protected stems. Since the injected audio is perfect and the mask drive constant, extraction and frame-wise mask decisions cannot explain these eight particular failures.

## Frozen design

[`prepare_oracle_renderer_probe.py`](../../../tools/stereo-upmix-render/prepare_oracle_renderer_probe.py) chose all eight original swinging songs plus 12 deterministic nonswinging controls from the same 40 DEV songs. The fixed selection, source track IDs and original 115 stem rows are in `D:\upmix\runs\oracle_renderer_probe\{manifest.json,split.json,baseline_stems.csv}`. Selecting on the failure endpoint makes this an enriched diagnostic set, **not an unbiased validation set**.

[`oracle_renderer_sweep.py`](../../../tools/stereo-upmix-render/oracle_renderer_sweep.py) loads each selected MoisesDB song once, then runs the same real C renderer and stable gain for these five conditions:

| Condition | Change from shipped renderer, oracle audio injected at G 1.0 |
| --- | --- |
| `baseline` | Fresh render of the original condition |
| `rear_body0` | Remove the default −4 dB rear-body shelf |
| `rear_air_off` | Set rear air to −40 dB instead of −12 dB |
| `front_retention025` | Set front retention to 0.25 instead of 0.50 |
| `front_retention075` | Set front retention to 0.75 instead of 0.50 |

Before scoring variants, every baseline stem's median rear share and one-second range must reproduce the original CSV within **0.005**. Each result is cached per song; [`run_oracle_renderer_probe.ps1`](../../../tools/stereo-upmix-render/pc/run_oracle_renderer_probe.ps1) refuses to start while the rapid queue, training or another stem harness is active. `compare_rapid_pair.py` pairs by song and bootstraps songs, not stem rows.

## Decision rule fixed before results

A knob deserves a full-40 rerender only if it resolves **at least three of the eight** original any-stem swing songs, adds **no new swinging control song**, and meets the goal's median rear-share limits on this screen: guitar ≥ 0.15, keys ≥ 0.15, piano ≥ 0.10, bowed strings ≥ 0.14, voice ≤ 0.02, bass ≤ 0.06 and drums ≤ 0.08. The median rear RMS must remain within 0.5 dB of the baseline, and the median guitar/keys/wind depth-versus-level slopes must stay within ±0.05. Failing any gate rejects the knob as a simple stability remedy; it does not rule out a new controller. These screen thresholds are promotion filters, not proof of the full-40 or untouched-20 goal.

If one passes, rerender the original 40 and a separately frozen independent set with exactly the selected setting. All original DEV songs have already influenced selection, so their remaining subgroup is not an untouched holdout. The full goal still requires causal ≤ 150 M MAC/s int8 measured on the unit, ≤ 85 ms extra latency, source-attributed vocal-entry ducking, and the owner's blind live-CarPlay preference. An oracle-audio knob screen alone proves none of those deployment or listening gates.

[`evaluate_oracle_renderer_probe.py`](../../../tools/stereo-upmix-render/evaluate_oracle_renderer_probe.py) applies this gate to complete CSVs and writes `decision.json`. A self-comparison using the frozen baseline as every condition correctly reported 8 → 8 swings and rejected all four variants for resolving none; the actual variants were still running at that check.

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

The exact two settings, plus a fresh baseline, finished on the original 40 DEV songs through [`run_oracle_renderer_full40.ps1`](../../../tools/stereo-upmix-render/pc/run_oracle_renderer_full40.ps1). Every fresh baseline stem again reproduced the original CSV within 0.005. The 231-row results and `decision.json` are at `D:\upmix\runs\oracle_renderer_full40\`.

| Condition | Any-stem swing songs / 40 | Guitar | Keys | Piano | Strings | Voice | Bass | Drums | Rear RMS dBFS | Full-40 gate |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| Fresh baseline | 8 | 0.255 | 0.230 | 0.171 | 0.180 | 0.009 | 0.013 | 0.069 | −37.619 | Reference |
| Rear air −40 dB | **5** (3 resolved, 0 new) | 0.237 | 0.226 | 0.171 | 0.172 | 0.008 | 0.012 | 0.032 | −38.194 | **Fail:** rear −0.575 dB vs ±0.5 dB gate |
| Front retention 0.75 | **1** (7 resolved, 0 new) | 0.170 | 0.170 | 0.120 | **0.134** | 0.009 | 0.013 | 0.068 | −37.619 | **Fail:** strings below 0.14 |

All median guitar, keys and wind level-dependence slopes remain within ±0.05. The 20-song screen overestimated string share under front retention: it had only three string songs, and the full set has six. The effect of front retention is partly a change in the front denominator while the rear RMS stays fixed; a 1/40 swing count is therefore not sufficient evidence of audible rear stability. Removing rear air reduces drum share strongly, but also lowers the overall rear level by just over the precommitted tolerance.

[`diag_oracle_audio_swings.py`](../../../tools/stereo-upmix-render/diag_oracle_audio_swings.py) subsequently located the one-second low/high windows of all **10 swinging stem traces across the eight baseline songs** using the saved C outputs and true stems (`D:\upmix\runs\oracle_renderer_probe\swing_timing.json`). Most do not coincide with a qualifying vocal entry: six of the eight songs have no entry under the fixed detector in the 60-second excerpt. One guitar swing in “Even Though” spans rear share 0.144 → 0.497 while its vocal stem is below −88 dBFS in both extrema. Other swings have vocal level changing in opposite directions. These observations show that the baseline's residual movement cannot be attributed solely to vocal-entry ducking; this timing check is not the goal's source-attributed transition score.

Both failures were close to a boundary. An explicitly **posthoc, exploratory** follow-up in [`run_oracle_renderer_full40_tune.ps1`](../../../tools/stereo-upmix-render/pc/run_oracle_renderer_full40_tune.ps1) checked rear air −40 dB with surround 0.427 (approximately restoring 0.575 dB) and front retention 0.70 on the same 40 songs. Any apparent success needs the separately frozen test split. No source-attribution, device or in-car verdict follows from perfect injected audio.

## Exploratory full-40 follow-up

The follow-up completed on the same original 40 DEV songs and real C renderer; `D:\upmix\runs\oracle_renderer_full40\tune_decision.json` preserves the earlier decision file. It uses the **previously inspected** full harness, so its passing condition is selected evidence, not an independent holdout result.

| Condition | Any-stem swings / 40 | Guitar | Keys | Piano | Strings | Voice | Bass | Drums | Rear RMS dBFS | Render gates |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| Baseline | 8 | 0.255 | 0.230 | 0.171 | 0.180 | 0.009 | 0.013 | 0.069 | −37.619 | Reference |
| Rear air −40, surround 0.427 | 9 (1 resolved, 2 new) | 0.274 | 0.256 | 0.200 | 0.201 | 0.009 | 0.013 | 0.037 | −37.627 | **Fail:** swing count |
| Front retention 0.70 | **4** (4 resolved, 0 new) | 0.185 | 0.187 | 0.128 | **0.140166** | 0.009 | 0.013 | 0.068 | −37.619 | **Pass**, with strings only 0.000166 above the limit |

Median guitar, keys and wind level-dependence slopes are 0.0025, −0.0042 and 0.0037 for front retention 0.70, within the ±0.05 limits. Its rear RMS is unchanged because the knob keeps more energy in the front; the instrument rear shares are lower than baseline. The bowed-string margin is too small to treat as robust. The gain-restored rear-air condition shows that matching rear RMS alone can create new swing songs.

[`run_oracle_renderer_test20_tune.ps1`](../../../tools/stereo-upmix-render/pc/run_oracle_renderer_test20_tune.ps1) checked **only** front retention 0.70 on the separately frozen 20-song MoisesDB TEST split, selected before this follow-up. The 101-row paired C render and `tune_decision.json` are in `D:\upmix\runs\oracle_renderer_test20\`.

| TEST condition | Any-stem swings / 20 | Guitar | Keys | Piano | Strings | Voice | Bass | Drums | Rear RMS dBFS |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Oracle baseline | 5 | 0.269 | 0.174 | 0.184 | 0.202 | 0.0065 | 0.0057 | 0.0585 | −37.171 |
| Front retention 0.70 | **2** (3 resolved, 0 new) | 0.194 | **0.128** | 0.140 | 0.160 | 0.0065 | 0.0057 | 0.0584 | −37.171 |

**Independent TEST gate fails:** keys are 0.128, below the formal 0.15 goal. The user offered 0.12 as a possible revised keys target, but that was not adopted before this result, so it cannot rescue this check retroactively. This TEST subset has only **five** keys songs, **one** bowed-string song and **no** wind stem with a valid slope; the evaluator correctly reports `slope_wind` as unverified, not as a measured slope violation. The guitar and keys slopes that can be measured are +0.0063 and −0.0080, both within ±0.05. TEST therefore supports a stability improvement with perfect source audio but does not establish the full instrument or slope goal. Track A's actual v21a mask failed the related [production screen](UPMIX_PRODUCTION_RENDERER_PROBE_2026-09-28.md). Source-attributed entry ducking, a causal implementation, device measurements and the owner's in-car preference remain unmet.

**Holdout caveat:** this enriched screen used knowledge of all eight failures in the original 40 DEV songs. The other 20 songs in that set were not rendered under the initial knob variants, but they are not a strict untouched holdout for choosing the knobs. The independently frozen songs therefore come from MoisesDB's separate 24-song `test` split and must be reported as TEST, not as a DEV subgroup.

That independent set was frozen before the full-40 result: [`prepare_oracle_test_holdout.py`](../../../tools/stereo-upmix-render/prepare_oracle_test_holdout.py), seed 1610, `D:\upmix\runs\oracle_renderer_test20\manifest.json`. It contains 20 of the 24 test-split tracks with **zero title or track-ID overlap** with the 40 DEV songs. The tuned retention-0.70 result is being checked there through the C renderer. It remains a separate **TEST** holdout, not the goal's literally named untouched DEV subgroup.
