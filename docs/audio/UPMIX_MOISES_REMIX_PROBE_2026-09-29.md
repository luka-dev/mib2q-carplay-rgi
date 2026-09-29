# MoisesDB Cross-Song Remix: Rapid Control

_Track B, 2026-09-29. Frozen before the first GPU run. This tests a data change, not a new architecture or a deployable candidate._

## Control and intervention

The saved same-song control is `D:\upmix\runs\rapid\anchor_s1\a\step4000.pt` (SHA-256 `8902f4c87e9bfa4f63cf65854194b2f694dc241d580cce859c35508f4f458ad3`). `anchor_s2` (seed 2; SHA-256 `195424be98bab52a12c7b09a86651933f93109700dda70ce977b5b76f2ebe03b`) estimates 4k seed variability. Both used the rapid recipe: `sep_net.py` BandSepNet, complex stereo mask, `norm=window`, seed 1 or 2, 4k steps, batch 12 × 8 s, 50% true MoisesDB TRAIN / 50% unremixed teacher pseudo, learning rate 3e-4, 2k warmup and SI-SDR switch after 20k. The seed-1 model has 586,916 parameters and 125 M **estimated neural** MAC/s; whole-window normalization makes it noncausal.

The candidate job is `remix_moises_s1|--remix 0.5`. It uses the same rapid recipe and seed 1, changing only the sampler on true-stem windows: when selected, the vocal slots stay with the base TRAIN song and drum, bass and `other` slots receive independently cropped TRAIN songs. The 774 pseudo-labelled windows remain same-song. Thus the intended exposure is about 25% of all sampled windows, subject to actual truth-pool selection and eligible track lengths. The current `--remix` path does **not** require four distinct songs, so report it as probabilistic cross-song remix; do not conflate it with review 43's distinct-song, causal fine-role experiment. Mixture and eligible targets are summed from the selected stems after gain augmentation. The PC and repository copies of `sep_net.py` and `rapid_daemon.ps1` matched SHA-256 `2abf35339e17e455152d15e4c6cdd5ac5d3b21c645fe23cf8aa28fc3bb3e4ad5` and `be2094dd0c2bc43b3ffaab88b71b6f432438775f35f90b5569929f685a884fb3` at the preflight check.

## Frozen readout and decision

The rapid daemon saves step 4000, runs `diag_gap.py` on 12 natural DEV excerpts and `diag_transitions.py` on 20 natural DEV songs, then the same 20-song, 60-second real-C renderer screen at fixed gain 1.0. Compare against both saved 4k anchors with paired song rows, including eligible and vocal transmission, bass/drum transmission, 8–14 kHz eligible transmission, guitar/keys/piano/strings rear share, rear level, vocal-entry median/p10 and absolute pre/post eligible energy, plus songs with **any** stem swinging. The diagnostic TRAIN-vs-DEV run on a saved anchor is interpretive context, not a gate by itself. Its uncommitted `diag_gap.py` selector is owned by another agent and must not be edited for this run.

At 4k, call a data signal only if useful instrument/eligible transmission rises at comparable rear level without worsening voice, bass, drums, entry retention or any-stem swings. Use +.03 eligible with ≤+.01 vocal or +.02 instrument rear share as *screening effect sizes*, not separate passes; inspect paired-song spread and the anchor seed range. A borderline result gets a second seed. A promising result gets a matched 10k same-song/remix comparison; the 4k shared-mask gain previously disappeared at 10k. No 4k result can pass the product goal. A winning data recipe must be retested with `norm=stream`, zero future context and streaming parity; full40 real-C, new untouched real-stem songs, measured device int8/latency and the owner's blind live-CarPlay test remain required.

## Execution state

Preflight found no PC GPU lease, active Python trainer or rapid queue line; the aborted earlier `remix` folder has no checkpoint. This document does not claim the new job has started or produced results. Keep one GPU tenant and do not install packages into the PC Python environment while runs are active.
