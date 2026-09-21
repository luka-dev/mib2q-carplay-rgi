# 60 s one-variable grid through the full conditional model, 2026-09-21

Offline only. No vehicle contact, no production/source/default edits, no deployment.
Same hash-pinned native C renderer, models and `full_static_nearest_zero_aux`
scenario as the [6 s check](UPMIX_FULL_MODEL_SOUND_CHECK_2026-09-21.md), so the two
studies chain directly (identical source-file hashes). Still not a cabin measurement,
not stock stereo, not a subjective verdict.

## What was run

Seven local cuts, **60 s + 4 s zero tail**, 48 kHz, EQ-7 input hypothesis only.
Baseline = local startup upmix settings at unity programme gain; four variants change
**exactly one** control each. Plus `dry_bank1` (L/R through the same BANK1 model).
42 rows, 3 parallel track shards, about 11.5 min per row per shard,  10G on disk.

| Variant | Changed vs startup | C mode | Surround | Front bass dB | LFE dB | Retention |
|---|---|---|---:|---:|---:|---:|
| `startup` | baseline | S16 | 0.40 | −3 | 0 | 0 |
| `bass0` | front bass −3 → 0 | S16 | 0.40 | 0 | 0 | 0 |
| `surr30` | surround 0.40 → 0.30 | S16 | 0.30 | −3 | 0 | 0 |
| `retain75` | retention 0 → 0.75 | S16 | 0.40 | −3 | 0 | 0.75 |
| `float` | S16 → float render (linked −1 dBFS rail, guard) | float→S16 | 0.40 | −3 | 0 | 0 |

Pre-declared before the run: tail windows 250 ms, decay threshold −80 dBFS electrical;
a row counts as decayed only if every window from that point to the end of the tail
is silent. Level matching is post hoc on band powers only (two anchors, below); sample
peaks are always reported raw.

## 1. Six-second prefixes were not representative

Per-track bass/mid tilt (front woofers 60–120 Hz minus front mid/treble 500–2000 Hz,
each relative to dry through the same model), 6 s study vs this 60 s run:

| Track | startup 6 s | startup 60 s | bass0 6 s | bass0 60 s | startup peak 6 s → 60 s dBFS |
|---|---:|---:|---:|---:|---:|
| Boost Up | +0.50 | −0.03 | +3.30 | +2.86 | −16.51 → +3.14 |
| Crazy Train | −2.62 | −0.40 | +0.24 | +2.45 | +0.31 → +1.20 |
| Deep Down Low | −2.24 | −1.29 | +0.61 | +1.58 | +3.65 → +4.73 |
| Heart-Shaped Box | −2.58 | +2.26 | +0.36 | +5.18 | −11.71 → −3.64 |
| The Blue-Eyed Devil | −0.41 | −1.77 | +2.46 | +1.12 | +2.13 → +3.71 |
| UP. UP. UP. | −0.12 | +3.16 | +2.78 | +6.03 | −8.35 → +1.23 |
| take your vibes and go | +0.39 | +1.60 | +3.23 | +4.44 | −3.69 → +1.57 |
| **mean** | **−1.01** | **+0.50** | **+1.86** | **+3.38** | |

Four of the seven 6 s prefixes were quiet intros (peaks −4 to −17 dBFS). Over 60 s,
six of seven tracks exceed 0 dBFS in every row including dry. The 6 s numbers
understated the bass/mid tilt by about 1.5 dB on average and by up to 4.8 dB on a track.
Any tuning conclusion drawn only from the 6 s rows is withdrawn.

## 2. One variable at a time (variant minus startup, mean over 7 tracks [min, max], dB)

| Changed | woofer 60–120 | sub 20–60 | sub 60–120 | front mid 500–2000 | rear 500–2000 | rear 2–8 kHz | centre 500–2000 | tilt | peak |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| front bass −3 → 0 | +2.90 [+2.86,+2.94] | +3.43 [+2.95,+3.61] | +3.26 [+3.20,+3.33] | +0.02 | +0.01 | −0.01 | +0.03 | +2.88 | +0.04 |
| surround 0.40 → 0.30 | 0.00 | 0.00 | 0.00 | +0.62 [+0.21,+1.21] | −0.58 [−1.45,+0.05] | −0.51 [−0.96,−0.25] | +0.95 [+0.36,+1.48] | −0.62 | +0.18 [0,+0.61] |
| retention 0 → 0.75 | +0.01 | 0.00 | 0.00 | +2.43 [+0.78,+4.49] | +1.15 [+0.84,+1.41] | +0.73 [+0.37,+1.08] | +3.27 [+1.30,+4.92] | −2.42 | +0.66 [0,+1.82] |
| S16 → float | −0.29 [−1.24,0] | −0.05 | −0.19 [−0.75,0] | −0.10 | −0.16 | −0.19 | −0.10 | −0.18 | −0.08 [−0.47,+0.03] |

Reading:

- **Front bass is the only control that moves bass, and it moves the modelled sub
  too** (+3.3..3.4 dB for a 3 dB shelf change), while leaving mid, rear and centre
  untouched. The old "sub untouched by front bass" comment is wrong for this chain.
- **Retention does not touch bass at all.** Its tilt reduction (−2.42) is entirely
  front-mid restoration (+2.43), and it also raises rear (+1.15), centre (+3.27) and
  peaks (up to +1.82 dB). It is a front/centre level control, not a bass fix.
- **Surround 0.30 vs 0.40** trades about 0.5 dB of rear for +0.6 dB front mid and
  +0.95 dB centre; bass unchanged.
- **Float render** is a small global reduction (−0.1..−0.3 dB) plus lower PCM6 peaks
  (−0.57 dB mean) from the linked −1 dBFS rail. Its guard is active on every track over
  60 s: minimum gain 0.26 on Deep Down Low (293 700 of 3.07 M frames attenuated), 0.41–0.45
  on four others, 0.90 on Heart-Shaped Box. S16 rows have no telemetry for the same
  condition; clean S16 PCM6 (zero near-rail samples in all 35 rows) does not mean no
  internal limiting.
- The earlier float candidate (0.30 / −1 / 0.75 / float) therefore combined a mid
  restore (retention), a rear cut (surround) and a **bass increase** (front bass −3 → −1,
  about +1.9 dB). Its measured 6 s tilt of −0.46 dB was not a bass reduction.

## 3. Absolute levels vs dry and level-matched view (mean over 7 tracks, dB)

| Variant | woofer 60–120 | front mid | sub 20–60 | rear 500–2000 | centre 500–2000 | tilt | broadband offset to match dry | matched woofer | matched mid |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| startup | −3.28 | −3.79 | −4.72 | −0.07 | −4.77 | +0.50 | +3.09 | −0.19 | −0.69 |
| bass0 | −0.39 | −3.77 | −1.30 | −0.06 | −4.74 | +3.38 | +1.09 | +0.70 | −2.68 |
| surr30 | −3.28 | −3.16 | −4.72 | −0.65 | −3.82 | −0.12 | +3.01 | −0.26 | −0.15 |
| retain75 | −3.27 | −1.35 | −4.72 | +1.08 | −1.51 | −1.92 | +2.58 | −0.69 | +1.23 |
| float | −3.57 | −3.89 | −4.77 | −0.23 | −4.88 | +0.32 | +3.23 | −0.34 | −0.66 |

The upmix at startup settings is about 3 dB **quieter** than dry broadband through the
same chain; the front drops (mid −3.8) and the modelled rear-door band stays at the dry
level (−0.07). In this electrical model the rear does not get louder with the upmix;
the front gets quieter. Matched at the front-mid anchor, rear is +3.7 dB and the
modelled sub −0.9 dB relative to dry. Rear/centre/sub group names remain unverified
physical candidates.

## 4. Tail decay and state

All 42 rows: model output below −80 dBFS from the second 250 ms tail window onward,
and exactly zero (−300 dB) from about 1.25 s; PCM6 from the C renderer is exactly zero
at most 49 ms after the programme (11 ms for dry). No accumulation, no long tails, no
residual signal. A 1 s tail is sufficient for future runs. This is the model; SHARC
arithmetic, hardware protection and cabin acoustics are not represented.

## 5. Peaks and headroom

Worst raw sample peaks (Deep Down Low, config 0): dry +6.35, startup +4.73, bass0 +4.88,
surr30 +4.80, retain75 +5.13, float +4.26 dBFS. Six of seven tracks exceed 0 dBFS in
every row. Runtime gains 1 and limiter threshold 100 are hypotheses, so this flags the
assumed level budget, not hardware clipping; dry exceeds too, so it is not upmix-caused.
Dry carries 25 641 near-rail samples inherited from the masters; all upmix rows carry 0.

## What this does and does not settle

- Settled for this model: the one-variable effects above (tight [min,max] bounds for
  bass and float; track-dependent for surround and retention), the 6 s
  non-representativeness, tail/state cleanliness.
- Not settled: EQ10 hypothesis (not rerun; 6 s deltas differed <0.03 dB but peaks were
  higher), actual vehicle EQ/selectors/levels/SoundCheck, the relation between the
  modelled groups and physical speakers, acoustics, subjective preference, S16 internal
  limiting activity. No production preset follows from this document alone.

## Artifacts

- Grid: `output/upmix-long-onevar-eq7-20260921/report.json`, 42/42, SHA256 `4b0f78f56ee408ab618c911ffff2ce6aee77e7013eebe00f56d95ecc974fcd6a`
  (per-track shard reports, PCM6, saved 16-ch FLOAT32 outputs, aux zeros, renderer logs).
- Review: `output/upmix-long-onevar-review-20260921/review.json` SHA256 `e3fea33e1027213c7dd720b2643b50dfae63f5e0c559be4b0722545c16487323`,
  `rows.csv`, `review.py` (reproduce: `python3 review.py ../upmix-long-onevar-eq7-20260921`
  from the review directory; refuses to overwrite).
- Verification: every saved 16-ch array hash and every stored metric recomputed; 232 unique
  referenced file fingerprints checked; shard/aggregate equality; aux files exactly zero;
  digital C exactly zero in all rows; source hashes equal to the 6 s study.
- Reproduce the grid: `audit_upmix_end_to_end_batch.py --seconds 60 --tail-seconds 4
  --input-eq-bank 7 --scenario full_static_nearest_zero_aux --jobs 3 --save-output-wavs`
  with the five `--variant` strings from the table (sources: the 60 s 48 kHz cuts in
  `/private/tmp/upmix-bank1-audit.BR9uxY/*.wav`, hashes in the report).
