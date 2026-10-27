# Production Mask Renderer Probe

_2026-09-28. Offline real-C mechanism screen, not an in-car listening result._

## Question and frozen inputs

Do the two renderer settings that stabilised perfect oracle-injected audio also help the **deployable Track A mask**? The 20-song set is the enriched DEV selection from the [oracle renderer probe](UPMIX_ORACLE_RENDERER_PROBE_2026-09-28.md): all eight songs that swung with oracle audio plus 12 deterministic controls. It is useful for a paired mechanism comparison, but is selected on a failure endpoint and is not an independent validation set.

[`run_production_renderer_probe20.ps1`](../../../tools/stereo-upmix-render/pc/run_production_renderer_probe20.ps1) runs three conditions, 60 seconds per song, stable input gain and the real C renderer:

| Condition | Track A v21a mask, drive 1.5, vocal gate 0.5 |
| --- | --- |
| `baseline` | Rear air −9 dB, front retention 0.50, surround 0.40 |
| `rear_air_off` | Rear air −40 dB |
| `front_retention075` | Front retention 0.75 |

The renderer receives the **stereo mixture only**. No oracle stem, mask, cue or rear-audio injection is passed. The frozen v21a mask binary is `D:\upmix\tools\hssr_mask_tracka_v21a.bin`, SHA-256 `4170a4131e1017d631d7e1faa5b1c017152fef8678b3f0fa54b4524f54f569d0`, copied from `models/mask-full-probe-v21a/hssr_mask.bin`. Track A's source and shipped binary were not edited.

The PC's older `upmix_render_inj.exe` lacked `--mask-model`, so the first attempt stopped before producing a baseline row. A separate host binary, `D:\upmix\tools\upmix_render_mask_v21a.exe`, was cross-built from the current `stereo_upmix_render.c`, `soundcheck.c`, `stereo_upmix.c`, `hssr_dsp.c`, `hssr_neon.c` and `hssr_mask.c` with mingw `-std=c99 -O2 -ffp-contract=off -DSPA_HSSR_TEST_MASK`; SHA-256 `cd1025fc080ed066f6c6174c66d39b9622ad95f1e5d3a7e095aab5c8b914236a`. Both copied hashes were read back on the PC. The new binary has its own name, leaving the established oracle harness untouched.

The runner refuses to start with a rapid job, training or another stem harness active. Results will be in `D:\upmix\runs\production_renderer_probe20\` with paired song results and `decision.json`. The heuristic 20-song screen reports instrument and protected-stem limits, depth-following slopes, rear RMS and a ≤2 swing-song filter. No 20-song screen by itself meets the goal's full-40, independent-test, device or in-car gates.

## Result

All three 20-song C renders finished with **115 stem rows each**. Paired song results and `decision.json` are in `D:\upmix\runs\production_renderer_probe20\`. The baseline is specific to this oracle-failure-enriched selection; it must not be compared as though it were Track A's full-40 aggregate.

| Condition | Any-stem swing songs / 20 | Guitar | Keys | Piano | Strings | Voice | Bass | Drums | Rear RMS dBFS |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Track A v21a baseline | 15 | 0.176 | 0.166 | 0.118 | 0.225 | 0.075 | 0.150 | 0.285 | −33.534 |
| Rear air −40 dB | 14 (1 resolved, 0 new) | 0.164 | 0.165 | **0.090** | 0.210 | **0.064** | **0.136** | **0.168** | −34.058 |
| Front retention 0.75 | 7 (8 resolved, 0 new) | **0.132** | **0.129** | 0.114 | 0.173 | **0.058** | **0.128** | **0.230** | −33.534 |

Bold values in variant rows miss at least one goal direction. Both variants miss the voice ≤0.02, bass ≤0.06 and drums ≤0.08 limits by large margins. Rear air off also lowers rear RMS by 0.524 dB, outside the fixed ±0.5 dB screen tolerance, and piano drops below 0.10. Front retention reduces the swing count substantially, but guitar and keys fall below 0.15 while protected leakage remains high. Median guitar, keys and wind depth-versus-level slopes stay within ±0.05 in all three conditions.

**Decision:** neither variant merits promotion to a 40-song production-mask render or live deployment. Relative to the true-eligible-audio oracle results, the remaining protected-stem leakage with v21a is evidence that mask selectivity is a major limit for these simple knob changes. The oracle tests also show a separate renderer stability contribution. These data do not establish a general impossibility for a new controller or extractor. The owner's listening verdict is still pending.
