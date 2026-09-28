# Persistent Rear Membership: First Offline Screen

_2026-09-28. Track B, review 16. This is a non-deployable 20-song mechanism screen through the real C renderer, not an in-car result or a full-harness validation._

## Question and setup

Does a slow membership decision, kept separate from fast protection, reduce rear movement while retaining useful instruments? The first prototype uses **one global membership state**. Its admission cue is the 300–4,000 Hz energy ratio of the 4k shared-mask separator estimate to the mix. The estimate provides **evidence only**; the C renderer routes the original mixture. A ground-truth per-cell oracle mask protects vocals, bass, and drums immediately. This oracle cue cannot ship.

The causal controller uses a 0.55 admission threshold, 0.35 release threshold, 0.8/2.0 s cue attack/release, 0.5/1.0 s dwell, and a 0.2 s membership ramp. Its output is a precomputed `HSSRMSK0` mask consumed by `upmix_render_inj.exe` at G 1.0. It was compared with the original oracle mask on the **same 20 DEV songs × 60 s** and the same renderer settings. The other 20 DEV songs remain untouched by this screen. `persistent_rear_membership.py` logs membership changes and, separately, the loss of true eligible-stem energy caused by membership relative to the same fast mask with membership fully open.

## Paired result

| Metric | Fast oracle mask alone | With global membership | Target |
| --- | ---: | ---: | ---: |
| Vocal rear share | 0.027 | 0.023 | ≤ 0.02 |
| Guitar | 0.188 | 0.183 | ≥ 0.15 |
| Piano | 0.112 | 0.100 | ≥ 0.10 |
| Keys | 0.164 | 0.153 | ≥ 0.12 |
| Bowed strings | 0.121 | 0.121 | ≥ 0.14 |
| Bass (11 finite paired rows) | 0.064 | 0.064 | ≤ 0.06 |
| Drums | 0.106 | 0.094 | ≤ 0.08 |
| Songs with any stem's 1 s rear-share range > 0.30 | 6/20 | **9/20** | ≤ 5/39 on full harness |

The median paired change was −0.005 for guitar, −0.003 for piano, −0.002 for keys and drums, and −0.001 for voice. The controller removed a median **3.5%** of the true eligible audio still passing the fast mask (p90 11.3%); one song lost **88.6%**. Median membership was open 96.4% of the time, and the median song had only one state change: initial admission about 1–3 seconds after playback began. That startup transition contributed new swings. The member condition's rear RMS was −37.1 dBFS; the older baseline CSV did not record rear RMS, so this screen does not establish a level-matched perceptual advantage.

## Decision

**Do not promote this global-membership controller.** It leaves voice and drums above their limits, strings below theirs, and increases the swing count. Its admission cue is almost always high after startup, so it rarely represents a source entering or leaving; in the few songs where it closes, it can erase substantial eligible audio. This rejects this **global cue and threshold family**, not persistent membership tied to individual streams.

Next: the oracle-cue upper bound must keep vocal, drum, and bass protection as separate fast signals and measure source-specific membership. A bounded search against the real C renderer is useful only once that controller changes source behaviour without rewarding silence. The owner's live CarPlay listening test remains the first product decision.

## Separate fast-protection oracle probe

`oracle_cue_mask.py` computes independent vocal, drum, and bass powers from the ground-truth stems and routes the **original mix** through the same C renderer. Its default weights reproduce the existing oracle mask numerically (mean absolute mask difference 0.0000058 on a checked song). This validates the STFT geometry, including the absolute power floor. It is an offline cue upper-bound probe, not a shippable estimator.

With vocal/drum/bass weights all set to 2, exponent 0.5, and G 1.0, the same 20-song paired screen gave:

| Median rear share | Original oracle | Separate weighted cues | Target |
| --- | ---: | ---: | ---: |
| Voice | 0.027 | 0.020 | ≤ 0.02 |
| Guitar | 0.188 | 0.162 | ≥ 0.15 |
| Piano | 0.112 | 0.094 | ≥ 0.10 |
| Keys | 0.164 | 0.143 | ≥ 0.12 |
| Bowed strings | 0.121 | 0.107 | ≥ 0.14 |
| Bass (11 finite paired rows) | 0.064 | 0.055 | ≤ 0.06 |
| Drums | 0.106 | 0.092 | ≤ 0.08 |
| Any-stem swing songs | 6/20 | 6/20 | ≤ 5/39 on the full harness |

The candidate's rear RMS was −37.3 dBFS. This one operating point improved protected stems but lost piano and strings and did not improve the swing count. The mask uses true separated sources, so a deployable model would have additional extraction error. This **does not prove** that all cue weights or a source-specific controller fail; it shows that stronger fast suppression alone is insufficient at this point. Results: `D:\upmix\runs\membership\oracle_fast_222\` and `D:\upmix\runs\membership\oracle_fast_222_screen\`.

A second paired point strengthened each protected cue to weight 4 and raised the renderer drive to G 1.2. On the same 20 songs, medians became voice **0.018**, bass **0.056**, drums **0.089**, guitar **0.190**, piano **0.108**, keys **0.158**, and bowed strings **0.119**. Rear RMS rose to −36.6 dBFS. It recovered guitar and piano while preserving voice and bass protection, but drums still exceeded 0.08, strings remained below 0.14, and any-stem swing songs rose from **6/20 to 13/20**. The gain tradeoff is therefore severe at this operating point. This is still an oracle-cue 20-song screen, not a bounded-search optimum or a full-harness result. The PC result is at `D:\upmix\runs\membership\oracle_fast_444_g12_screen\`; [`run_oracle_tradeoff.ps1`](../../tools/stereo-upmix-render/pc/run_oracle_tradeoff.ps1) records the exact CPU-only procedure.

## Reproduce

On the PC, the generated masks and report are under `D:\upmix\runs\membership\oracle_sharedmask4k\`; the C harness output is under `D:\upmix\runs\membership\oracle_sharedmask4k_screen\`. The baseline rows are `D:\upmix\runs\masknet-oracle\stems_amp_1.0\stems.csv` restricted to the same 20 songs. The source code is [`persistent_rear_membership.py`](../../tools/stereo-upmix-render/persistent_rear_membership.py), with [`upmix_stem_tests.py`](../../tools/stereo-upmix-render/upmix_stem_tests.py) accepting `--mask-dir` to feed the precomputed masks through the real renderer.
