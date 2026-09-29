# MoisesDB Cross-Song Remix: Rapid Control

_Track B, 2026-09-29. Frozen before the first GPU run. This tests a data change, not a new architecture or a deployable candidate._

## Control and intervention

The saved same-song control is `D:\upmix\runs\rapid\anchor_s1\a\step4000.pt` (SHA-256 `8902f4c87e9bfa4f63cf65854194b2f694dc241d580cce859c35508f4f458ad3`). `anchor_s2` (seed 2; SHA-256 `195424be98bab52a12c7b09a86651933f93109700dda70ce977b5b76f2ebe03b`) estimates 4k seed variability. Both used the rapid recipe: `sep_net.py` BandSepNet, complex stereo mask, `norm=window`, seed 1 or 2, 4k steps, batch 12 × 8 s, 50% true MoisesDB TRAIN / 50% unremixed teacher pseudo, learning rate 3e-4, 2k warmup and SI-SDR switch after 20k. The seed-1 model has 586,916 parameters and 125 M **estimated neural** MAC/s; whole-window normalization makes it noncausal.

The candidate job is `remix_moises_s1|--remix 0.5`. It uses the same rapid recipe and seed 1, changing only the sampler on true-stem windows: when selected, the vocal slots stay with the base TRAIN song and drum, bass and `other` slots receive independently cropped TRAIN songs. The 774 pseudo-labelled windows remain same-song. Thus the intended exposure is about 25% of all sampled windows, subject to actual truth-pool selection and eligible track lengths. The current `--remix` path does **not** require four distinct songs, so report it as probabilistic cross-song remix; do not conflate it with review 43's distinct-song, causal fine-role experiment. Mixture and eligible targets are summed from the selected stems after gain augmentation. The PC and repository copies of `sep_net.py` and `rapid_daemon.ps1` matched SHA-256 `2abf35339e17e455152d15e4c6cdd5ac5d3b21c645fe23cf8aa28fc3bb3e4ad5` and `be2094dd0c2bc43b3ffaab88b71b6f432438775f35f90b5569929f685a884fb3` at the preflight check.

## Frozen readout and decision

The rapid daemon saves step 4000, runs `diag_gap.py` on 12 natural DEV excerpts and `diag_transitions.py` on 20 natural DEV songs, then the same 20-song, 60-second real-C renderer screen at fixed gain 1.0. Compare against both saved 4k anchors with paired song rows, including eligible and vocal transmission, bass/drum transmission, 8–14 kHz eligible transmission, guitar/keys/piano/strings rear share, rear level, vocal-entry median/p10 and absolute pre/post eligible energy, plus songs with **any** stem swinging. The diagnostic TRAIN-vs-DEV run on a saved anchor is interpretive context, not a gate by itself. Its uncommitted `diag_gap.py` selector is owned by another agent and must not be edited for this run.

At 4k, call a data signal only if useful instrument/eligible transmission rises at comparable rear level without worsening voice, bass, drums, entry retention or any-stem swings. Use +.03 eligible with ≤+.01 vocal or +.02 instrument rear share as *screening effect sizes*, not separate passes; inspect paired-song spread and the anchor seed range. A borderline result gets a second seed. A promising result gets a matched 10k same-song/remix comparison; the 4k shared-mask gain previously disappeared at 10k. No 4k result can pass the product goal. A winning data recipe must be retested with `norm=stream`, zero future context and streaming parity; full40 real-C, new untouched real-stem songs, measured device int8/latency and the owner's blind live-CarPlay test remain required.

## Execution state

Preflight found no PC GPU lease, active Python trainer or rapid queue line; the aborted earlier `remix` folder has no checkpoint. Keep one GPU tenant and do not install packages into the PC Python environment while runs are active.

**Launch, 2026-09-29 02:24 PC local time.** The owner authorized execution. `remix_moises_s1|--remix 0.5` was queued and the rapid daemon launched detached (PID 29840). Read-back showed `GPU_LEASE = rapid`, the new job record, an active `sep_net.py train` process (PID 12052) and the expected 176-truth/774-pseudo training header. No checkpoint or DEV/C result existed at that read. The daemon's own `job.txt`, checkpoint, diagnostics, screen and board must establish completion; process launch alone does not.

## Completed 4k result and decision

The run finished at 02:50 PC local time. Step 4000, DEV attribution, 43 vocal-entry diagnostics, the 20-song real-C screen and the rapid board all completed. Candidate checkpoint SHA-256: `b509031d93b322419124ffe9578db42d0caa43ffcc5bf9f5923e9907b5951e3f`. The queue and GPU lease were empty; with no Python job active, the rapid daemon was stopped. No second GPU job was queued.

| Fixed screen | Same-song `anchor_s1` | `--remix 0.5`, seed 1 | Reading |
| --- | ---: | ---: | --- |
| DEV12 eligible / vocal / all protected transmission | .42 / .197 / .084 | .44 / .208 / .085 | Small recall rise with vocal rise; below the +.03 / ≤+.01 screening signal. |
| Eligible 8–14 kHz | −23.0 dB | −22.4 dB | Small air change, not a solution to high-frequency suppression. |
| Entry post/pre median / p10, 43 entries | .758 / .390 | .719 / .395 | Median worsened; both miss the product .85 / .60 target. Absolute median eligible pre/post changed .535/.404 → .551/.392: the post-entry level fell. |
| Real-C rear level, 20 songs, gain 1.0 | −37.6 dBFS | −37.6 dBFS | Level is matched at board precision. |
| Real-C guitar / keys / piano / strings rear share | .11 / .09 / .06 / .13 | .10 / .10 / .07 / .13 | No meaningful instrument gain; strings cover only five songs in this screen. |
| Real-C voice / bass / drums rear share | .04 / .07 / .09 | .04 / .07 / .09 | Protected stems remain above product voice/drum targets. |
| Songs with any-stem swing | 3/20 | 3/20 | Exactly the same three songs. |

The second same-song anchor (`anchor_s2`) already spans eligible .47, vocal .236, guitar .12, keys .10 and entry .747/.431 at 4k. Paired rows from the two seed-1 `stems.csv` files show guitar median change **−.0011** across 20 songs (9 positive), keys **+.0017** across 15 (8 positive), piano **+.0039** across 8 (7 positive) and strings **−.0077** across 5 (1 positive). These are small compared with the desired instrument movement and mixed in sign; the three swinging songs are unchanged. The fixed joint gate **rejects promotion to 10k**. This bounds this 50%-truth-window remix recipe at 4k; it does not rule out a different remix rate, distinct-song sampler or other training data.

A CPU-only TRAIN12 screen using another agent's existing `DIAG_SPLIT=train` selector completed after the C screen. Same-song versus remix TRAIN SI-SDR was **−1.53→−1.70 dB**, eligible transmission **.38→.39**, vocal transmission **.122→.123**. On DEV12 the corresponding values were **−1.14→−1.19 dB**, **.42→.44**, **.197→.208**. TRAIN12 and DEV12 contain different songs/source balances; these absolute gaps do not by themselves identify underfitting or overfitting. The 4k learning curves and per-source readout agree only that this intervention did not improve the frozen product screen. The CPU script used no GPU and did not edit `diag_gap.py`.

Next data comparison: start from the better same-song control and add curated OnAir at a specified window share, with its own matched no-OnAir control and stem-role/alignment audit. Keep the review-43 fine-role stream experiment separate. No Track A/head-unit change or listening claim follows from this failed screen.
