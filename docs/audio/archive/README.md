# Upmix Research Archive

*Archived 2026-10-09 with the Universal retirement: the dated 2026-09-25 → 10-04 research notes (separator, oracle-renderer and Track B joint-scene work, one probe, trial or snapshot each) with their `.json` / `.csv` / `.log.txt` measurement sidecars.*
*The code and tools these notes link to exist only at git tag `upmix-universal-last-2026-10-09` (commit 1501fc7d); their results are summarised in [Upmix History and Results](UPMIX_HISTORY_AND_RESULTS_2026-10-09.md), the inventory of the retirement is [Upmix Code Retirement](UPMIX_CODE_RETIREMENT_2026-10-09.md), the live map is [Audio](../README.md).*

## 🗂️ Notes

| Date | Note | One-line verdict |
|---|---|---|
| 2026-09-25 | [Upmix Source Separation Plan](UPMIX_SOURCE_SEPARATION_PLAN.md) | Running plan and log of the per-source rear scene (Phase 0 mask students v2–v29 → v21a chosen; the separator and joint-scene lines), updated through 10-04; terminal decision: history16 fails its 8192 joint comparison and strict fitting gates, MLP12 preserved. |
| 2026-09-27 | [Upmix Mask Net Phase 1 Report](UPMIX_MASK_PHASE1_REPORT_2026-09-27.md) | v21a per-cell mask net in C shipped 27.09 (3.92 → 1.46 M instr/control, Krait 1.33 ms per 10.67 ms control); the reset bug meant the net had never run in the car before the 27.09 evening; drive 1.5 / air −9 / gate 0.5 shipped sighted, [car] "something behind, still less than 22.09". |
| 2026-09-28 | [Upmix Separator Handoff](UPMIX_SEPARATOR_HANDOFF.md) | Separator-track decision record (rules, PC tooling, iteration ledger, stopped queue), updated through 10-04; last result: history16 fails the ≤ .10 fitting and the MLP12 comparison, keep MLP12, no continuation queued. |
| 2026-09-28 | [Upmix Architecture Gate](UPMIX_ARCHITECTURE_GATE_2026-09-28.md) | Ideal-mask audio full-40 bound and rear-depth decomposition: the object route itself swings (+6.14 dB rear energy per source energy between depth extremes in 10/10 traces), independent of extraction; nothing deployed or heard. |
| 2026-09-28 | [Causal Confidence Map](UPMIX_CAUSAL_CONFIDENCE_MAP_2026-09-28.md) | Review 22: the static confidence map passes 4/4 source gates (vocal/E .3231) but fails entry .826/.473, the 80 ms EMA 2/4; not promoted. |
| 2026-09-28 | [Causal Deep-Filter Probe](UPMIX_CAUSAL_DEEP_FILTER_PROBE_2026-09-28.md) | Review 24: two-tap causal complex filter, eligible .3984, vocal/E .4755, 0/4 gates; rejected. |
| 2026-09-28 | [Causal Matrix Probe](UPMIX_CAUSAL_MATRIX_PROBE_2026-09-28.md) | Review 27: learned causal 2×2 matrix student, eligible .4043, vocal/E .5381, 0/4 gates; failed promotion. |
| 2026-09-28 | [Causal Role-Focus Probe](UPMIX_CAUSAL_ROLE_FOCUS_2026-09-28.md) | Review 21: rare confident-cell focus, eligible .4624 → .3372, entry .786/.520; rejected. |
| 2026-09-28 | [Causal Four-Role Probe](UPMIX_CAUSAL_ROLE_PROBE_2026-09-28.md) | Review 20: four-role softmax student, eligible .4624 → .3505, entry .788/.539; the true-power target passes but its labels are unavailable live; rejected. |
| 2026-09-28 | [Causal Shared-Mask Source-Loss Pair](UPMIX_CAUSAL_SOURCE_PAIR_2026-09-28.md) | Review 19: shared-mask source loss raises eligible .46 → .49 but vocal .156 → .195 and rear +.668 dB by opening pure-vocal cells; rejected. |
| 2026-09-28 | [CMA Renderer Pilot](UPMIX_CMA_RENDERER_PILOT_2026-09-28.md) | CMA-ES over ten static knobs with the frozen v21a mask, 18 candidates on 6 songs: 0 feasible, the best score silenced the rear by 4.03 dB; rejected, a scalar score prefers silence. |
| 2026-09-28 | [Continuous-Context Audit](UPMIX_CONTINUOUS_CONTEXT_AUDIT_2026-09-28.md) | 60 s uninterrupted vs 8 s-reset inference of the causal complex models: −.001 / −.004 dB median, range −.05…+.05 dB; resets are not the cause. |
| 2026-09-28 | [Direct-Object Renderer Probe](UPMIX_DIRECT_OBJECT_RENDERER_PROBE_2026-09-28.md) | Object straight to Ls/Rs without bed (surround .3235): true audio 8 → 0/40 swings, IRM audio 0/40, TEST20 4 → 0/20, sep9 6 → 1/20 but guitar .105 / bass .070 [oracle]; mechanism pass in-sample, never ported to the hook. |
| 2026-09-28 | [Dual-Head Causal Probe](UPMIX_DUAL_HEAD_CAUSAL_PROBE_2026-09-28.md) | Review 23: auxiliary four-role head on the causal complex control, eligible .3623, vocal/E .4497, 0/4 gates; rejected. |
| 2026-09-28 | [Dual-State Spatial Probe](UPMIX_DUAL_STATE_PROBE_2026-09-28.md) | Review 35: the learned 8 s slow spatial state changes the output but fails the protected-source ratios and the entry p10 gate; rejected. |
| 2026-09-28 | [Fine-Cell Role Probe](UPMIX_FINE_ROLE_PROBE_2026-09-28.md) | Review 37: fine-cell role and direct matrix route (146 M MAC/s) uses the cue for 7 % of the output, entry tail worse, overlap and protected-source gates fail; rejected. |
| 2026-09-28 | [Matrix Cell-Attribution Audit](UPMIX_MATRIX_CELL_ATTRIBUTION_2026-09-28.md) | Review 30: the frozen matrix comparison locates the added vocal leakage 99.45 % in pure-vocal cells. |
| 2026-09-28 | [Matrix Cross Ablation](UPMIX_MATRIX_CROSS_ABLATION_2026-09-28.md) | Review 28: cross terms are .308 of the frozen matrix student's output; the 4k matrix student had already failed promotion. |
| 2026-09-28 | [Matrix Source-Loss Probe](UPMIX_MATRIX_SOURCE_LOSS_PROBE_2026-09-28.md) | Review 29: full-operator source-retention/protection loss, eligible .462 but vocal/E .506; rejected at the frozen source/entry gate. |
| 2026-09-28 | [Oracle Renderer Probe](UPMIX_ORACLE_RENDERER_PROBE_2026-09-28.md) | True eligible audio through Track A's renderer: full40 baseline 8/40 swings; air −40 → 5/40 but rear −0.575 dB; retention .75 → 1/40 but strings .134; retention .25 → 14/20 [oracle]; all fail, retention moves the share denominator, not the rear. |
| 2026-09-28 | [Persistent Membership Screen](UPMIX_PERSISTENT_MEMBERSHIP_SCREEN_2026-09-28.md) | Review 16: global, per-source and protected-cue membership latches on the mixture, global 6 → 9/20 swings, per-source 6 → 6, cues 13/20 at G 1.2; rejected. |
| 2026-09-28 | [Production Renderer Probe](UPMIX_PRODUCTION_RENDERER_PROBE_2026-09-28.md) | v21a with air −40 / retention .75 on enriched DEV20: baseline 15/20, air off 14 (rear −0.524 dB), retention .75 → 7 but guitar .132 / keys .129; neither promoted, mask selectivity dominates. |
| 2026-09-28 | [Protected-Cell Veto Upper Bound](UPMIX_PROTECTED_VETO_UPPER_BOUND_2026-09-28.md) | Review 31: privileged true-stem protected-cell veto, eligible .4255 (< .432), vocal/E .1642, entry .828/.493; failed the joint gate. |
| 2026-09-28 | [Role-Cue Audit](UPMIX_ROLE_CUE_AUDIT_2026-09-28.md) | Review 36: the frozen dual-state student ranks pure cells (AUC .941) but tracks mixed vocal+eligible cells poorly (song-median correlation .192), and its direct decoder path changes the output only 2.53 % RMS. |
| 2026-09-28 | [Separator Night Report](UPMIX_SEPARATOR_NIGHT_REPORT_2026-09-28.md) | Night of 27→28.09: sep5–sep8 (width, supervision, structure, loss) rejected on the same frontier, the sharedmask 4k signal did not survive 10k, review 16 paused broad separator training; the 09-29 correction narrows "ruled out" to the sep5/sep7 interventions. |
| 2026-09-28 | [Spatial-Cue Upper Bound](UPMIX_SPATIAL_CUE_UPPER_BOUND_2026-09-28.md) | Review 26: privileged song-specific full 2×2 L/R operator, eligible .3189 → .4617, vocal/E .5314 → .3183, better in 11/12; the bound passes, true-stem fit only, no candidate. |
| 2026-09-28 | [Spatial-Memory Horizon](UPMIX_SPATIAL_MEMORY_HORIZON_2026-09-28.md) | Review 34: fixed 2 s / 8 s window fits fail (+4.29 / +5.62 dB error vs 30 s), recency-weighted 2–8 s fits pass the medians; short decay passes once evidence accumulates. |
| 2026-09-28 | [Anchored Vocal-Entry Matrix Probe](UPMIX_VOCAL_ENTRY_TARGET_PROBE_2026-09-28.md) | Review 33: anchored vocal-onset reconstruction target, vocal/E .5422; rejected at the frozen source/entry gate. |
| 2026-09-28 | [Vocal-Invariance Matrix Probe](UPMIX_VOCAL_INVARIANCE_PROBE_2026-09-28.md) | Review 32: paired-mixture vocal-invariance training collapses to silence (eligible .0713); rejected at the frozen source gate. |
| 2026-09-29 | [Band-Allocation Probe](UPMIX_BAND_ALLOCATION_PROBE_2026-09-29.md) | Same-budget 48-band redistribution at 10k: TRAIN SI-SDR 7.797 → 7.951 dB (+.155), loss .76 % lower; fails the frozen gain gate, original partition kept. |
| 2026-09-29 | [BSRNN Pretrained Inference](UPMIX_BSRNN_PRETRAINED_INFERENCE_2026-09-29.md) | Pretrained BSRNN on the four TRAIN crops: optimized SIMO 11.013 dB median `other` SI-SDR vs large 7.463 dB (student 7.797); reference only, not an Audi candidate. |
| 2026-09-29 | [BSRNN Real-C Check](UPMIX_BSRNN_REAL_C_CHECK_2026-09-29.md) | Pretrained SIMO full40 through the real C renderer: 0/40 swings, entry .876/.746, 14/14 gates, V/B/D .0022/.0070/.0033; the offline existence proof, non-causal at 30× the budget. |
| 2026-09-29 | [BSRNN Reference Intake](UPMIX_BSRNN_REFERENCE_INTAKE_2026-09-29.md) | Pinned BSRNN pipeline and verified large/SIMO weights on the PC, loader and gain traps recorded; neither model is an Audi candidate. |
| 2026-09-29 | [Cambridge Multitrack Data Trial](UPMIX_CAMBRIDGE_TRIAL_2026-09-29.md) | Three-song guitar/piano/strings TRAIN addition with matched control: guitar −.0124 paired at 4k; negative, rejected. |
| 2026-09-29 | [Causal Reference Generalization](UPMIX_CAUSAL_GENERALIZATION_2026-09-29.md) | Frozen 40k matched true-stem continuation on all 176 TRAIN songs: full40 paired +1.30 dB, 39/40 non-worse, relative gates pass; absolute gates fail (guitar .121, strings .109, entry .804/.420), original-route swings 10 → 15/40; rejected. |
| 2026-09-29 | [Causal Reference Encoder](UPMIX_CAUSAL_REFERENCE_ENCODER_2026-09-29.md) | Largest causal encoder (residual96x4, 2.168 G MAC/s, 14× the budget) with the fixed decoder: TRAIN4 fitting 7.754 → 10.500 dB (+2.746 dB, −20.584 % objective); CPU audit passed, protected-source and compute limits remain. |
| 2026-09-29 | [Cross-Corpus Remix Probe](UPMIX_CROSS_CORPUS_REMIX_PROBE_2026-09-29.md) | MoisesDB–OnAir cross-corpus remix, matched 4k pair: same 4/20, no joint source, entry and real-C improvement; rejected. |
| 2026-09-29 | [Cross-Song Augmentation Probe](UPMIX_CROSSSONG_AUGMENT_PROBE_2026-09-29.md) | Review 43: cross-song true-stem four-song remix, eligible .4921 → .4912, entry p10 .5814 → .5893; source separation unchanged, rejected. |
| 2026-09-29 | [Cue Attribution Bound](UPMIX_CUE_ATTRIBUTION_BOUND_2026-09-29.md) | Review 41: eligible transmission rides on protected audio (eligible → .31 when it is subtracted); protected subtraction fixes the source ratios at the cost of eligible transmission. |
| 2026-09-29 | [Decoder-State Supervision](UPMIX_DECODER_STATE_SUPERVISION_2026-09-29.md) | Matched 2k decoder-state teaching: state error −5.86 % but audio loss .05530 → .06616 and SI-SDR 7.79 → 7.54 dB; all frozen gates failed. |
| 2026-09-29 | [DSD100 Independent Corpus](UPMIX_DSD100_INDEPENDENT_CORPUS_2026-09-29.md) | DSD100 acquired and validated (15,989,815,305 B, ZIP CRC and SHA-256 checked, 50 Dev / 50 Test, Test sealed); never trained on. |
| 2026-09-29 | [Experiment Effectiveness Audit](UPMIX_EXPERIMENT_EFFECTIVENESS_AUDIT_2026-09-29.md) | Audit of the short-run decisions (control distributions, vocal-owned metrics); revised the next-experiment policy toward the causal larger-corpus experiment; no model result of its own. |
| 2026-09-29 | [Invariant-Input Probe](UPMIX_INVARIANT_INPUT_PROBE_2026-09-29.md) | Polarity-invariant input representation at 10k: exact polarity invariance but TRAIN SI-SDR 7.797 → 7.630 dB; rejected. |
| 2026-09-29 | [MoisesDB Instrument Sidecar](UPMIX_MOISES_INSTRUMENT_SIDECAR_2026-09-29.md) | Six-family instrument sidecar for the MoisesDB TRAIN caches (176 built); the sidecar remix screen went eligible .44 → .42 at 4k; rejected with the other data screens. |
| 2026-09-29 | [MoisesDB Remix Probe](UPMIX_MOISES_REMIX_PROBE_2026-09-29.md) | MoisesDB cross-song remix as a 4k data change: eligible .42 → .44, voice .197 → .208, 3/20 swings; rejected. |
| 2026-09-29 | [OnAir Data Probe](UPMIX_ONAIR_DATA_PROBE_2026-09-29.md) | Curated OnAir five-song truth pool as a matched 4k data intervention; rejected at 4k with the other data screens. |
| 2026-09-29 | [Operator Anchor Probe](UPMIX_OPERATOR_ANCHOR_PROBE_2026-09-29.md) | Review 42: slow spatial operator anchored to a true-stem fit, eligible .478 → .431, entry p10 .710 → .676; slightly better operator fit, worse transmission; rejected before C. |
| 2026-09-29 | [Vocal-Overlap Bounded-Mask Upper Bound](UPMIX_OVERLAP_ORACLE_BOUND_2026-09-29.md) | Review 38: the true-stem bounded IRM passes the vocal, drum and vocal-entry gates but misses the bass source ratio (bass/E .0589 > .0339). |
| 2026-09-29 | [Separator Research Review](UPMIX_RESEARCH_REVIEW_2026-09-29.md) | Literature and source-code review of 22 lines against the plateau; proposed the causal reference fitting → generalization → compression sequence; launched no training. |
| 2026-09-29 | [Separator Status Snapshot](UPMIX_SEPARATOR_STATUS_2026-09-29.md) | Stop snapshot of the separator track: band allocation (.055586 → .055163, 0.76 %) and decoder-state teaching (SI-SDR 7.79 → 7.54 dB) both failed their frozen gates; original partition kept. |
| 2026-09-29 | [Spatial Operator Source and Entry Bound](UPMIX_SPATIAL_OVERLAP_BOUND_2026-09-29.md) | Review 39: the privileged 2×2 stereo operator fitted from true eligible audio passes the median source and entry gates (vocal/E .300, bass/E .0094, entry 1.04/.78) with severe per-song outliers (one song at 6e-6 eligible). |
| 2026-09-29 | [Upmix Status and Problems](UPMIX_STATUS_AND_PROBLEMS_2026-09-29.md) | Owner summary with dated updates up to the spectral40k failure and the 09-30 wrap; the data hypotheses were tested, joint quality / device / owner acceptance stayed open. |
| 2026-09-29 | [Student Spatial Cue](UPMIX_STUDENT_SPATIAL_CUE_2026-09-29.md) | Review 40: a frozen causal fine-role cue fitting the slow operator improves the bass ratio but fails the vocal and drum source gates (vocal/E .545, drums/E .071). |
| 2026-09-29 | [Training-Path Diagnosis](UPMIX_TRAINING_PATH_DIAGNOSIS_2026-09-29.md) | Fixed-TRAIN fitting diagnostics: 10k closes 73.2 % of the oracle gap (SI-SDR 7.80 dB), the same decoder with privileged free states reaches 20.59 dB, polarity flip 7.80 → −0.97 dB; encoder/state inference is the limit, not the decoder. |
| 2026-09-30 | [Separator Attempt Chronicle 09-25 → 09-30](UPMIX_SEPARATOR_CHRONICLE_2026-09-25_TO_2026-09-30.md) | Attempt ledger 09-25 → 09-30 (final PC readback 2026-10-01 03:28 UTC): named mask/sep recipes, reviews 16–43, data and BSRNN tests, unified-model iterations, failed/stopped/unexecuted attempts with evidence links. |
| 2026-09-30 | [Track B Session Wrap](UPMIX_SESSION_WRAP_2026-09-30.md) | Owner-requested pause of Track B: successes/failures, the complete attempt chronology, the final spectral40k failure, PC/archive state and the finite restart sequence. |
| 2026-09-30 | [Unified Rear Scene Architecture](UPMIX_UNIFIED_SCENE_ARCHITECTURE_2026-09-30.md) | One causal network for front and rear: the broad/capacity pairs fail, spectral 40k full40 .225/.217/.219/.191 with 31/40 swings and entry .799/.451, paired +.138 dB < +.3; rejected 2026-10-01 03:28 Z, learned rears sit 3.1–3.3 dB below Track A at equal gain. |
| 2026-10-03 | [Joint Scene 8192 Continuation](UPMIX_JOINT_SCENE_8192_CONTINUATION_2026-10-03.md) | Fixed TRAIN crop continued 2048 → 8192: Ls/Rs NMSE .4485/.4695 → .3189/.3489, neither reaches the ≤ .10 criterion; rear fitting stays partial. |
| 2026-10-03 | [Joint Scene Fixed Fit](UPMIX_JOINT_SCENE_FIXED_FIT_2026-10-03.md) | One TRAIN excerpt, 2048 updates: Ls/Rs NMSE ≈ 1.0 → .4485/.4695, strict fitting gate not reached; still improving at termination, no ceiling claim. |
| 2026-10-03 | [Joint Scene MLP12 Head](UPMIX_JOINT_SCENE_MLP12_HEAD_2026-10-03.md) | MLP12 output head at matched counts: rear NMSE .2848/.3148 (−10.7 % / −9.8 % vs linear) at 111.11 M MAC/s; the ≤ .10 gate never met, MLP12 kept as the anchor. |
| 2026-10-03 | [Joint Scene NAS Pilot](UPMIX_JOINT_SCENE_NAS_PILOT_2026-10-03.md) | Five-output shared causal encoder on 7 AlbumDB self-downmixes, 4096 updates: rear NMSE TRAIN .971 / DEV 1.136 (1.0 = silent rear); fails on TRAIN as well, a reproducible negative. |
| 2026-10-03 | [NAS Scene-Training Recheck](UPMIX_NAS_SCENE_RECHECK_2026-10-03.md) | Saved-report inspection on the NAS (direct 5.1 P1, analytical front, multisong loss/startup failures, bounded target witness, gain/correlation diagnosis); nine report hashes, no new training or listening. |
| 2026-10-03 | [Rear Scene Research](UPMIX_REAR_SCENE_RESEARCH_2026-10-03.md) | 16 primary-source literature entries (stable objects, separate ambience) and the joint front/rear proposal that became the NAS pilot; planning only, no hook deployment. |
| 2026-10-04 | [FlyWire Topology Screen](UPMIX_FLYWIRE_TOPOLOGY_SCREEN_2026-10-04.md) | Auditory-seeded 64-node FlyWire reservoir vs its degree/sign/weight-preserving rewired null vs GRU at 4096: .4107/.4422 vs .4022/.4308 vs .3594/.4021; loses to its own null, fails the predeclared rule. |
| 2026-10-04 | [Scene Encoded History16](UPMIX_SCENE_ENCODED_HISTORY_2026-10-04.md) | Current plus three past encoded frames before the GRU (history16) at 8192: rear .3026/.3431, 6.27 % / 8.99 % worse than MLP12, front/centre also worse; fails, MLP12 kept. |
| 2026-10-04 | [Scene Interband Context](UPMIX_SCENE_INTERBAND_CONTEXT_2026-10-04.md) | One frequency GRU12 residual branch on the frozen MLP12 anchor at 8192: rear .2898/.3118 (+1.78 % / −0.95 %) at 122.06 M MAC/s; fails its predeclared rule. |
| 2026-10-04 | [Scene Output Reachability](UPMIX_SCENE_OUTPUT_REACHABILITY_2026-10-04.md) | Target-informed bounded operator through the stock forward reaches rear NMSE .000265/.000317 on the fixed crop; the output family can reproduce the target, so the gap is learned inference, not reachability. |

## 🗃️ Recovery and documentation snapshots

- [Retired tools: verified Git recovery paths](RETIRED_TOOLS.md).
- [Pre-audit stereo pipeline](STEREO_TO_5_1_UPMIX_PRE_AUDIT_2026-10-29.md): earlier diagrams and configuration.
- [October 9–10 SCENE deployment](SCENE_BACKEND_DEPLOY_2026-10-09.md): original artifact hashes and measurements.
- [Unavailable source references](../../archive/LEGACY_REFERENCES.md): explicit gaps in the original journals.

## 🗃️ Retired runtime and integration notes

- [HSSR Model](HSSR_MODEL.md)
- [Upmix Calibration and Tuning](UPMIX_CALIBRATION_AND_TUNING.md)
- [Upmix Code Retirement Map](UPMIX_CODE_RETIREMENT_2026-10-09.md)
- [Upmix History and Results](UPMIX_HISTORY_AND_RESULTS_2026-10-09.md)
- [UPMIX rework baseline (2026-10-07)](UPMIX_REWORK_BASELINE_2026-10-07.md)
- [UPMIX rework proposal (2026-10-07)](UPMIX_REWORK_PROPOSAL_2026-10-07.md)
- [UPMIX radical directions (2026-10-07)](UPMIX_RADICAL_DIRECTIONS_2026-10-07.md)
- [Upmix Stability: "the sound goes back and front" (2026-09-25)](UPMIX_STABILITY_2026-09-25.md)
- [Current Track A on the matched full40 harness](UPMIX_TRACKA_MATCHED40_2026-09-30.md)
- [Upmix Training History](UPMIX_TRAINING_HISTORY.md)
- [Upmix domain model and qualification map](UPMIX_DOMAIN_MODEL.md)
- [CarPlay Upmix Listening Protocol](UPMIX_CARPLAY_LISTENING_PROTOCOL.md)
- [Upmix Listening and Deploy Log](UPMIX_LISTENING_AND_DEPLOY_LOG.md)
- [Upmix Release and Acceptance](UPMIX_RELEASE_AND_ACCEPTANCE.md)
- [iOS 26 type-103 connection triage](IOS26_TYPE103_CONNECTION_TRIAGE_2026-10-03.md)
