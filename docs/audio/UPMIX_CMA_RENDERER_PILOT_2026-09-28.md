# Bounded Production Renderer Search

_Exploratory six-song DEV pilot, 2026-09-28. This is a static-knob search with Track A's frozen v21a mask, not a port or a source-membership controller._

The [production screen](UPMIX_PRODUCTION_RENDERER_PROBE_2026-09-28.md) showed that rear air and front retention alone did not solve voice, bass or drum leakage. [`cma_production_renderer.py`](../../tools/stereo-upmix-render/cma_production_renderer.py) therefore runs a small bounded **CMA-ES** search against the same real C renderer and stereo mixture, with no oracle injection. It changes ten existing product settings: mask drive, vocal gate, surround, front retention, rear air, rear body, mask gamma, mask treble, front bass and front treble. All values remain within the C parser's permitted ranges. The head-unit hook, Track A source and model binary are unchanged.

Six songs were fixed from the 20-song enriched screen before examining any search result: “Holy Magick — Below,” “Take Another Line,” “Wake Up So,” “James Graydon — Our Love,” “Juniper — God Knows” and “Modified Freq — Real.” They cover keys, guitar, piano, all three bowed-string songs in that screen, several protected-stem failures and exposed voice. This is a **selected training screen**, not holdout evidence. The run uses three generations of six candidates, seed 1610, and caches per-song rows. Its output is `D:\upmix\runs\cma_production_renderer6\search.json`.

The scalar search score penalises rear-share violations for voice ≤0.02, bass ≤0.06, drums ≤0.08, guitar ≥0.15, keys ≥0.15, piano ≥0.10 and strings ≥0.14. It also penalises more than one any-stem swinging song among six, median rear RMS outside ±0.5 dB of the frozen baseline, and guitar/keys/wind depth-versus-level slopes outside ±0.05. A small delivery reward and a level penalty discourage silence, but do not make a low-scoring candidate feasible. The score is an engineering proposal, **not** a perceptual metric or proof that no setting exists.

The PowerShell runner [`run_cma_production_renderer6.ps1`](../../tools/stereo-upmix-render/pc/run_cma_production_renderer6.ps1) refuses to overlap training or another renderer harness and forces CPU-only execution. Any apparent winner must be rerun on the original 20-song production screen, the full 40 DEV songs and an independent set before device or listening consideration.

## Result

All **18 candidates** completed after one interrupted C render was resumed from per-song cache. The stale destination triggered the host renderer's same-file guard on the WinBtrfs D: volume; the stem harness now removes only its own incomplete output WAV before retrying and surfaces the C error. No training or GPU run overlapped the search. The complete configuration, per-stem summary and score of each candidate are in `D:\upmix\runs\cma_production_renderer6\search.json`.

| Six-song condition | Score, lower is better | Any-stem swings | Guitar | Keys | Piano | Strings | Voice | Bass | Drums | Rear RMS dBFS |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Frozen v21a baseline | 25.27 | 4/6 | 0.257 | 0.199 | 0.162 | 0.225 | 0.091 | 0.151 | 0.344 | −32.96 |
| Lowest scalar score (`g2c3`) | **13.65** | 0/6 | **0.089** | **0.072** | **0.059** | **0.082** | **0.029** | **0.127** | **0.141** | **−36.99** |
| Rear-present trade-off (`g2c2`) | 19.19 | 3/6 | 0.206 | 0.164 | 0.125 | 0.185 | **0.062** | **0.149** | **0.296** | −33.93 |

The lowest-score point is **4.03 dB quieter in the rear** than baseline and misses every instrument minimum; even its voice, bass and drums remain above their protection limits. The rear-present point clears the four instrument medians but leaks protected stems and swings in half the six songs. **None of the 18 candidates met the six-song screen constraints**, so none merits a full-20 or full-40 promotion or a live config change. The scorer's preference for a quiet point is itself a warning that a scalar score cannot replace hard admission gates. This bounded search supports a local mask-selectivity trade-off, not an impossibility result for a new controller, different mask, or the full parameter space.
