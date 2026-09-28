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

All six original swinging songs still swing at G 1.2; seven additional songs cross the 0.30 one-second range threshold. Every added crossing is attributed to **guitar or keys** in the stem tracker, the same instruments the higher gain was meant to help. This points to unstable instrument depth in the mask-driven rendering path, rather than a new vocal-only artifact.

One renderer-knob control kept cue weights at 4 and G at 1.0 but removed the default −4 dB rear-body shelf (`--rear-body 0`). It reached −36.7 dBFS rear RMS, close to the G 1.2 point, **without increasing the swing count (6/20)**. Medians were voice 0.017, guitar 0.167, piano 0.128, keys 0.141, strings 0.100, bass 0.055, and drums 0.082. The body change therefore offers a steadier way to raise some rear energy, but loses strings and still misses drums. Bowed-string median band shares rose only at 300–1,000 Hz (0.086 → 0.101) and fell above 1 kHz, so its overall loss is measured rather than inferred from the shelf curve. This is a 20-song oracle-cue probe (`D:\upmix\runs\membership\oracle_fast_444_g10_body0_screen\`), not a deployable setting. Stop manual gain/shelf guessing here; the owner's verdict and a source-specific controller should guide the next bounded search.

## Per-source membership oracle screen

[`oracle_source_membership.py`](../../tools/stereo-upmix-render/oracle_source_membership.py) gives **each isolated eligible stem** its own slow causal activity latch (300–4,000 Hz cue; −42 dB admission, −50 dB release, 0.3/1.5 s dwell, 0.15 s ramp). Vocal, bass and drum powers remain separate **instantaneous** protection cues. The original mix still enters the C renderer. This is non-deployable: it uses true MoisesDB stems for admission and protection. With every latch forced open, its `HSSRMSK0` output matched the original oracle-cue generator on a checked 60 s song to mean absolute difference **4.3×10⁻⁹**, maximum **1.2×10⁻⁷**; the paired result therefore isolates membership rather than a changed mask formula.

On the same 20 DEV songs at G 1.0, the membership removed a median 0.56% of oracle-passing eligible energy (p90 2.29%, worst 5.41%) and switched source states a median three times per song. Guitar, piano and keys membership was almost always open (median open fractions 0.993, 1.000 and 1.000), while bowed strings were open only 0.138 of the time and percussion 0.000. The real C renderer gave:

| Median rear share | Fast oracle alone | With per-source membership |
| --- | ---: | ---: |
| Voice | 0.027 | 0.027 |
| Guitar | 0.188 | 0.186 |
| Piano | 0.112 | 0.112 |
| Keys | 0.164 | 0.164 |
| Strings | 0.121 | 0.121 |
| Drums | 0.106 | 0.101 |
| Percussion (4 paired songs) | 0.329 | 0.109 |
| Any-stem swing songs | **6/20** | **6/20**, none new or resolved |

**Do not promote this oracle activity-latch family.** It does not change the sustained sources whose depth moves, and its dwell rejects transient percussion. This does not rule out a controller with better stream identity or a clean eligible-audio path; the previous full 40-song oracle-audio experiment remains substantially stronger than oracle mask × mixture. Mask logs: `D:\upmix\runs\membership\oracle_source_membership_v1\report.json`; C results: `D:\upmix\runs\membership\oracle_source_membership_v1_screen\stems.csv`. The CPU-only renderer invocation is [`run_source_membership_screen.ps1`](../../tools/stereo-upmix-render/pc/run_source_membership_screen.ps1).

## Reproduce

On the PC, the generated masks and report are under `D:\upmix\runs\membership\oracle_sharedmask4k\`; the C harness output is under `D:\upmix\runs\membership\oracle_sharedmask4k_screen\`. The baseline rows are `D:\upmix\runs\masknet-oracle\stems_amp_1.0\stems.csv` restricted to the same 20 songs. The source code is [`persistent_rear_membership.py`](../../tools/stereo-upmix-render/persistent_rear_membership.py), with [`upmix_stem_tests.py`](../../tools/stereo-upmix-render/upmix_stem_tests.py) accepting `--mask-dir` to feed the precomputed masks through the real renderer.
