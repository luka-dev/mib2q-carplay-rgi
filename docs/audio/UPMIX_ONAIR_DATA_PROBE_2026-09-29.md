# Curated OnAir Truth-Pool Probe

_Track B, 2026-09-29. Corpus audit and matched comparison frozen before GPU training._

## Why this is a separate experiment

The [MoisesDB-only 50% remix screen](UPMIX_MOISES_REMIX_PROBE_2026-09-29.md) did not improve the joint source/entry/real-C result. This experiment asks whether a second **real-stem corpus** improves the unchanged BandSepNet recipe when given a visible, fixed share of training windows. It does not combine a new corpus with cross-song remix in the same comparison. A later cross-corpus remix can be tested against this result as its own intervention.

The [OnAir V4 release](https://github.com/sevagh/OnAir-Music-Dataset) provides multitracks, but its [official four-source loader map](https://github.com/sevagh/onair-py/blob/main/onair/config.py) covers only some tracks and even assigns instrumental ambience to a `vocals` target on some LoFi songs. [`onair_map.json`](../../tools/stereo-upmix-render/onair_map.json) therefore includes only explicit vocal, drum, bass and instrument stems with clear roles; it omits ambience, FX, combined kick+bass stems and duplicate submixes. The included set has **five songs**: Under My Skin, Set Me Free, Yaku, Ramen Rain and Bouncin Around. The first two have an explicit `Vox` source; the other three contribute instrumental windows without a fabricated vocal target. Ronin was attempted using the official four-role sources, with and without Main Loop, but selected-stem/master correlation stayed **.219/.217** within ±0.5 s, so it was excluded pending alignment/version audit. Action, Aurora Borealis, Reflections, Autumn in NY and Vibin Vibin remain excluded for the reasons in the mapping file.

[`cache_onair.py`](../../tools/stereo-upmix-render/cache_onair.py) checked the selected WAVs: all are stereo 44.1 kHz and all selected stems in each song have the same length. It resampled each to 48 kHz, assigned the seven MoisesDB-compatible source slots, and normalized the **selected-stem sum** to a .7 peak before int16 caching. Training mixtures and targets are recomputed from those seven cached slots, so their closure is exact by construction. The mastered mix is used only for a best-lag/correlation audit; it is **not** forced into the training mixture. All five cached files are TRAIN only in `D:\upmix\datasets\onair_cache7`; no OnAir song enters DEV/TEST. Cache results:

| Track | Seconds | Master lag | Correlation |
| --- | ---: | ---: | ---: |
| Under My Skin | 169.2 | 0.0 ms | .476 |
| Set Me Free | 152.5 | 0.0 ms | .469 |
| Yaku | 209.5 | −298.0 ms | .725 |
| Ramen Rain | 174.6 | −282.3 ms | .866 |
| Bouncin Around | 223.7 | −254.3 ms | .950 |

The LoFi master offsets are reported, not applied to the stems. Equal stem durations and a strong shared-sum correlation support the synthetic mix, but the master check alone cannot prove that every individual stem is free of overlap or vocal bleed. These five songs are a small domain supplement, not an independent validation set.

## Frozen matched 4k pair

The code path in `sep_net.py` adds `--extra-truth DIR --extra-truth-frac 0.25`. With the existing half/half MoisesDB-truth/teacher-pseudo sampler, **25% of true-stem windows** come from the curated OnAir cache: about **12.5% of all training windows**. The other 37.5% are MoisesDB truth and 50% remain unchanged teacher pseudo. The existing `--remix` stays at zero in both arms. A synthetic sampler smoke check produced 510 pseudo, 351 MoisesDB truth and 139 extra-truth windows out of 1000 and verified mixture/eligible closure.

Run `onair_ctrl_s1` with no extra corpus and `onair25_s1` with `--extra-truth D:\upmix\datasets\onair_cache7 --extra-truth-frac 0.25`, both under the same new code hash, BandSepNet architecture, seed 1, 4k steps and rapid recipe as `anchor_s1`. The control should reproduce the saved anchor within ordinary GPU variability. The only candidate change is the 25% OnAir share conditional on a truth window. Record code/cache/renderer hashes, process and lease state before launching; one GPU tenant, no package installs.

The same DEV12 attribution, 43 vocal entries and 20-song fixed-gain real-C screen decide the result. Compare eligible and individual instrument rear shares at the same rear level, with voice/bass/drums, absolute entry transmission, median/p10 ducking and any-stem swings jointly guarded. A +.03 eligible or +.02 instrument signal is only useful if protected sources and motion do not regress. Inspect paired songs and the anchor seed range; repeat a borderline result with seed 2. Only a joint 4k signal earns a matched 10k pair, causal `norm=stream` replication and later full40/device/car gates. No result is claimed yet.

**Preflight and short smoke, 03:05 PC local time.** The repository code/map/cache script are committed at `3991e21`. The PC `sep_net.py` SHA-256 is `05a9f71e6bafef69d00393a836cff412bbf418cae9f83108441a03408393bd10`; rapid daemon `be2094dd0c2bc43b3ffaab88b71b6f432438775f35f90b5569929f685a884fb3`; real-C executable `730a2ca9af8bf84db8b8ac9b4abacadeff70190abb1fc7b78f0536012bb74520`; OnAir split `4b3955d211eeaed745eb5ef4de461cd02b173489e719d742f4737b706d42cc18`. The five `.npy` cache SHA-256 prefixes, in the table's order, are `9ca7b059`, `7a48b875`, `1d3d39a1`, `697288ef`, `4282a9d4` (full hashes were read from the PC before the pair). A 20-step GPU smoke completed for both default and extra-truth paths, saving step-20 checkpoints; a separate 1000-window synthetic sampler test confirmed the selected pool share and target closure. These checks establish the data path runs, not any separation improvement. The smoke lease was released and no Python/GPU job remained at preflight.

**Pair launched, 03:06 PC local time.** The owner had authorized data experiments. The rapid queue received `onair_ctrl_s1|` followed by `onair25_s1|--extra-truth D:\upmix\datasets\onair_cache7 --extra-truth-frac 0.25`, and the detached daemon started as PID 29096. A read-back showed one `sep_net.py train` process, rapid's GPU lease, `onair_ctrl_s1` start record and the expected 176-truth/774-pseudo header with the control's log-magnitude statistics matching the original anchor (−2.82/2.81). The OnAir arm remains queued behind the control and its C screen. The ad hoc launch command printed a PowerShell syntax error in a duplicate-folder preflight expression (`Test-Path` with `-or`); the tag folders did not previously exist, and the subsequent lease/process/job read-back confirmed exactly one control run. No result is inferred from launch.

## Completed 4k result and decision

Both arms finished 4k and the full rapid screen on 2026-09-29 PC local time (`onair25_s1` at 03:41:13). The new-code control reproduced the saved `anchor_s1` within run variation. The table uses the same fixed DEV12 attribution, 43 entry events, 20 DEV songs × 60 s and real C renderer with `--mask-drive 1.0`; rear level is the actual Ls/Rs RMS, not a post-hoc gain match.

| Measure | `onair_ctrl_s1` | `onair25_s1` |
| --- | ---: | ---: |
| Last DEV SI-SDR, dB | −1.15 | −1.44 |
| Eligible transmission, all / eligible-dominant | .43 / .47 | .39 / .43 |
| Vocal / protected transmission | .195 / .086 | .200 / .091 |
| 8–14 kHz eligible transmission, dB | −23.1 | −23.5 |
| Vocal-entry retention median / p10 | .756 / .392 | .752 / .387 |
| Entry 0–2 s vocal-leak peak | .29 | .35 |
| Rear level, dBFS | −37.6 | −37.5 |
| Guitar / keys / piano / strings rear share | .11 / .09 / .06 / .13 | .09 / .09 / .06 / .11 |
| Voice / bass / drums rear share | .04 / .07 / .10 | .05 / .06 / .09 |
| Any-stem swing songs | 3/20 | 3/20 |

The fixed OnAir share made the data intervention visible, but it **reduced eligible recall and guitar/strings rear share** at essentially the same rear level. Vocal transmission and entry leakage increased. The predeclared joint gate rejects this candidate; no second seed or 10k continuation is warranted for this recipe. This result tests five curated OnAir songs at 12.5% of *all* windows and the unchanged same-song model recipe; it does not rule out a larger, better audited corpus or instrument-family remix. After completion the queue was empty, the GPU lease absent and no Python job remained; the rapid daemon was stopped with a process-ID check. No head-unit or Track A file changed.

A paired-song read of the saved `stems.csv` files (`compare_rapid_pair.py`, 5,000 bootstrap resamples with the **song** as unit) found vocal rear share **.039→.047**, paired median delta **+.005** (90% CI **+.0007..+.0083**). Guitar was **.108→.092**, paired delta **−.013** (CI **−.017..+.0007**); bowed strings **.131→.113**, paired delta **−.018** (CI **−.0508..−.0002**, only five songs). All three original swinging songs remained, with none added or resolved. These intervals describe this repeatedly inspected 20-song DEV screen; they are not independent test-set confidence intervals.
