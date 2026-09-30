# Upmix Status and Problems 2026-09-29

**Current owner direction — one network and an expressive rear, 2026-09-30:** Replace q6d+guard+v21a with one shared causal audio/scene model. The [architecture and expressiveness plan](UPMIX_UNIFIED_SCENE_ARCHITECTURE_2026-09-30.md) and isolated `unified_upmix_net.py` CPU prototype are implemented: four joint source operators, six instrument memberships/pans/update rates, carried state;483,086 parameters,110.0055M estimated denseMAC/s including all heads. Future-prefix/chunk/reset/attribution/gradient/silence structural checks passed; this is untrained, with no acoustic, device or hook-ABI claim. Owner wants full instruments clearly behind; historical15–20% floors do not establish expressiveness. Explore frozen TRAIN-calibrated20/30/40% transfer points with source protection, body/attack/band/level and entry evidence, then owner ears. Next complete copied-host replacement contract and current Track A matched full40 baseline, freeze real-TRAIN fitting/source/scene supervision, then one-GPU execution. No job queued or GPU/package/unit/Track A change from this update. Earlier proposed next scalar mechanism is superseded by this explicit unified direction; preserve its measured failures and both reference checkpoints.

**Completed Track B — broad causal pair and full C test, 2026-09-30:** The [completed comparison](UPMIX_CAUSAL_GENERALIZATION_2026-09-29.md#-completed-full40-result-2026-09-30) finished both40k horizons, strict CPU checkpoint audit,40 DEV×60s,231 stem rows per C route and265 source-attributed events per arm. All11 relative quality gates pass, **absolute product gate fails**. Median paired DEV20s SI-SDR gain **+1.302827dB**, bootstrap90%[1.051355,1.443251],39/40 nonworse. Fixed direct-C control→reference guitar/keys/piano/strings **.103/.105/.091/.103→.121/.159/.135/.109**, voice/bass/drums **.0152/.0536/.0096→.0059/.0521/.0085**,0/40 swings both; rear+.365834dB at unchanged gain. Keys/piano/protected stems/slopes pass; guitar/strings and vocal-entry **.803796/.419774 median/p10** fail. Original-C swings worsen10→15/40, so direct-C stability is not an encoder-only achievement. Reference passes10/14 observed direct gates;2.168G estimated neuralMAC/s remains over≤150M allowance. PC idle: evaluator/supervisor returned0, no Python or GPU lease; no next job queued. Next predeclare one matched source/loss mechanism study on measured guitar/strings/entry failures before compression. Exact raw results/CSV/transition rows are committed with the report. Test24/independent20/device/ownerCarPlay remain open. No Track A/unit/shared-package change. Earlier active/pause snapshots below are history.

**Completed causal fitting result — causal reference pair, 2026-09-29:** The [matched normalized residual causal encoder experiment](UPMIX_CAUSAL_REFERENCE_ENCODER_2026-09-29.md) completed both 10k arms and independent CPU checkpoint audit. Same four frozen TRAIN crops, unchanged true-audio objective and bit-identical frozen trained decoder: fresh control → reference median SI-SDR **7.754→10.500 dB** (+2.746 dB), objective **.057067→.045320** (−20.584%); all four crops improved and all eight predeclared fitting comparisons passed. Fixed-STFT future-prefix NSE **0**, decoder/source/checkpoint parity verified. This demonstrates better causal fitting for this encoder package, not generalization or a product pass: vocal/bass frozen-mask transmissions rose **.0633→.0767 / .0748→.0901**, treble remains weak, and cost is **2,167.95 M estimated neural MAC/s** versus the ≤150 M measured-device allowance. No new causal DEV/source-entry/C, device or owner listening result. PC root `D:\upmix\runs\separator_causal_reference_v1`; terminal readback zero Python, lease absent, rapid queue empty. Next prepare broader true-stem training plus source-specific TRAIN/DEV and natural-song entry/C gates before compression; no next job queued. No Track A/unit/package edit. Earlier live execution and prerequisite notes below are history; SIMO remains the separate noncausal quality reference.

**Latest Track B result — full40 pretrained SIMO check, 2026-09-29:** [Completed source/entry/real-C evaluation](UPMIX_BSRNN_REAL_C_CHECK_2026-09-29.md) passes all 14 predeclared observed offline gates with the frozen host-only direct route, original gain 1.0, 40 DEV×60 s and 231 stem rows. Guitar/keys/piano/strings rear shares **.200/.211/.219/.204**, voice/bass/drums **.0022/.0070/.0033**, **0/40** any-stem swings versus **11/40** on the original C route; rear RMS difference **+.101 dB**. Vocal-entry retention **.875674/.745983** median/p10, 84 entries across 36 songs (83 valid ratios), within 265 total events across all 40 songs. On the identical enriched DEV20/direct-C subset, sep9→SIMO guitar **.105→.204**, keys **.144→.228**, voice **.019→.002**, at fixed gain but +1.149 dB rear RMS; no level-matched architecture claim. Aggregate passes retain individual bass/entry/instrument failures and upper-band attenuation, documented in the report. This is a large noncausal pretrained reference, not a device-qualified or owner-heard candidate; untouched-20, causal int8 ≤150 M/≤85 ms and live CarPlay gates remain open. Quiet-stem coverage and native/SSH execution failures were corrected with verified saved-output/event resume; final CSV/statistics/hash readback passed. PC idle: zero active Python, GPU lease absent, rapid queue empty, upstream clean. Next proposed matched causal encoder diagnostic with the same fixed trained decoder; no training queued, no Track A/unit edit. Earlier intake/inference/execution notes below are history.

**Latest Track B result — pretrained BSRNN item 1, 2026-09-29:** [Strict inference](UPMIX_BSRNN_PRETRAINED_INFERENCE_2026-09-29.md) completed on the exact four frozen TRAIN crops: all five public checkpoints loaded with zero missing/unexpected keys and exact tensor parity; 32 predicted stems saved at original gain in 48 kHz float WAV. Optimized SIMO `other` median SI-SDR **11.013 dB**, original-style large **7.463 dB**, versus historical small causal fitting baseline **7.797 dB**. SIMO's +3.216 dB is an offline pretrained reference improvement, not a matched causal architecture result or source/entry/C/device/listening pass. Dependencies were installed only in a separate venv, sharing unchanged torch/torchaudio 2.11.0+cu128. Three setup failures and their recovery are documented. PC idle: zero Python, lease absent, queue empty; upstream source clean. Next predeclare the fixed-gain source/entry/real-C comparison using SIMO as the stronger reference, then test causal mechanisms with a matched control. No fine-tuning, new encoder, Track A or head-unit change; earlier intake-only statements below are historical.

**Latest prerequisite completed, 2026-09-29:** The owner requested full BSRNN before custom architecture work. [Pinned source and verified BSRNN-large/SIMO weights](UPMIX_BSRNN_REFERENCE_INTAKE_2026-09-29.md) are now on PC, with five extracted checkpoints and 2.84 GB of archives. No new audio result: models have not run, dependencies were not installed, and the references are noncausal. Next validate strict loading and original gain before causal adaptation. Current read-back: no Python, GPU lease absent, rapid queue empty.

**Latest research update, 2026-09-29:** The [review of 22 research lines](UPMIX_RESEARCH_REVIEW_2026-09-29.md) found substantial differences between our simplified BandSepNet and published normalized residual separators. Proposed next step: demonstrate an acoustic gain with one stronger **causal encoder and the same fixed decoder**, then check generalization and compress. This takes priority over the DSD 40k data pair proposed below. It is a hypothesis, not a new success: no candidate, code, training, device measurement or listening result was added. Completed two-tap/context negatives remain valid; the old width experiment does not exclude every capacity/optimization improvement. Documentation only this turn; last recorded PC state is idle, not freshly polled.

**Latest Track B result — band allocation, 2026-09-29:** The [same-budget 48-band test](UPMIX_BAND_ALLOCATION_PROBE_2026-09-29.md#completed-10k-result-and-decision) completed 10k on the exact four TRAIN windows. Reducing maximum band width **89→41** kept **586,916 parameters / 125.139 M estimated neural MAC/s**, but improved objective only **.055586→.055163 (0.76%)** and median SI-SDR **7.797→7.951 dB**. Three windows were nonworse, one lost .338 dB; the frozen joint gate failed. CPU checkpoint/budget/window parity passed; no DEV/C/device promotion or long extension. Keep the original partition. Next prepare the previously frozen full-horizon causal original baseline versus DSD Dev addition, rather than extend these failed shortcuts; its cache/runner remain pending and no long run is queued. GPU lease/rapid queue are clear, no Python/daemon. Earlier execution entries below are history.

**Latest Track B result — direct decoder-state teaching, 2026-09-29:** The [matched 2k continuation](UPMIX_DECODER_STATE_SUPERVISION_2026-09-29.md#completed-matched-2k-result) completed with the original encoder and a bit-identical fixed decoder. State MSE fell only **5.86%**, while audio loss worsened **.05530→.06616** versus control and median SI-SDR **7.79→7.54 dB**. All frozen gates failed; no continuation/C/device promotion. CPU target/checkpoint parity passed. About 45% of initial concatenated state-error energy is invisible to the individual band decoders, under the documented per-band projection; this is not a full-operator or universal capacity diagnosis. The cuDNN eval-mode startup error was fixed before either student ran, with saved target reuse. GPU lease/rapid queue are clear, no Python/daemon remains. Next predeclare one same-budget 48-band redistribution test, reducing the widest high-frequency groups without discarding complex phase. Nothing new is queued; earlier execution entries are history.

**Latest Track B result — invariant input, 2026-09-29:** The [fixed-TRAIN feature comparison](UPMIX_INVARIANT_INPUT_PROBE_2026-09-29.md#completed-10k-result-and-decision) completed 10k. With the same original encoder/trunk/decoder and exact TRAIN windows, magnitude/relative-phase inputs achieved polarity NSE **0** but reconstruction worsened: objective **.05559→.05747**, median SI-SDR **7.80→7.63 dB**, all four windows slightly worse. Frozen diagnostic promotion **NO**. Checkpoint semantics and feature formula passed an independent CPU audit; no DEV/C/device/car claim. This whole feature replacement also discards cross-frequency phase information, so it does not reject all polarity-invariant representations. The GPU lease/rapid queue are clear and no Python/daemon remains. Next predeclare one direct decoder-state learning diagnostic versus matched audio-loss continuation on the original encoder; no follow-up is queued. Earlier state entries below are history.

**Latest verified diagnosis, 2026-09-29:** The [TRAIN-path investigation](UPMIX_TRAINING_PATH_DIAGNOSIS_2026-09-29.md) completed four fixed-window fitting, raw/cache consistency, decoder-state relaxation, physical polarity and real-C oracle-parity checks. At 10k, the causal baseline closed 73.2% of its numerical oracle loss gap, median SI-SDR 7.80 dB. Its unchanged trained decoder reached 20.59 dB with privileged optimized states (69.9% less remaining loss), demonstrating a substantial encoder/state-inference or optimization gap rather than a decoder-only ceiling. Exact eligible reconstruction can still have protected frozen-filter contributions; real C output parity confirms these contributions are not audible contamination in the summed waveform. Treat that metric as sensitivity evidence, jointly with reconstruction, entry and C tests; no historical student is promoted. DSD100 is now fully CRC/SHA validated (50 Dev/50 Test), with Test audio undecoded/unscored and Dev conversion pending. GPU diagnostics are terminal and the rapid queue/lease clear. All audited encoder blocks receive gradients; saturation is localized, not a proven treble-failure cause. Next freeze one representation/state-learning intervention before another broad efficacy run. Device and ear gates remain open.

One page for the owner: what the stereo→5.1 upmix has achieved, what is still broken, and the order of the next steps. Details live in the linked records; this note only summarises them.

**Later Track B update, 2026-09-29:** The old GPU and "untested causes" rows below describe the original stop snapshot. Execution resumed afterward. Three matched 4k data screens failed their joint source/entry/real-C gates: MoisesDB four-family remix, a five-song curated OnAir supplement, and MoisesDB six-family instrument remix. None earned 10k. The [MoisesDB result](UPMIX_MOISES_REMIX_PROBE_2026-09-29.md#completed-4k-result-and-decision), [OnAir result](UPMIX_ONAIR_DATA_PROBE_2026-09-29.md#completed-4k-result-and-decision), [instrument-family result](UPMIX_MOISES_INSTRUMENT_SIDECAR_2026-09-29.md#completed-matched-4k-result-and-decision) and [current handoff](UPMIX_SEPARATOR_HANDOFF.md) carry the data. A previously frozen [causal slow-operator architecture pair](UPMIX_OPERATOR_ANCHOR_PROBE_2026-09-29.md#resume-after-the-data-screens-2026-09-29) now holds the PC GPU lease, with no DEV result yet. The three negatives narrow the tested data recipes, not all data or architecture choices; no Track A hook change or car listening result follows.

**Current Track B update, 2026-09-29 14:28 PC local time:** The preceding live-GPU note is historical. The causal slow-operator and fine-role cross-song probes both failed their preselected source/entry gates, and a fourth rapid data comparison—[MoisesDB–OnAir cross-corpus remix](UPMIX_CROSS_CORPUS_REMIX_PROBE_2026-09-29.md#completed-4k-pair-and-decision)—also failed at 4k and real C. That candidate left guitar/keys/piano near **.10/.09/.06**, voice near **.05**, and vocal-entry p10 worsened **.412→.384**; no 10k promotion. The GPU is free. An official DSD100 download is in progress, with an integrity and overlap audit prepared; it cannot alone prove the separate instrument-family thresholds. There is still no Track B model in the head unit, device measurement or blind live-CarPlay verdict. Preserve the full product gates and choose a larger audited real-stem pool or a targeted causal architecture intervention.

---

## 🎯 Goal

Instruments audibly and stably in the rear, the voice never, no "back and forth". The car plays ordinary stereo from the phone over **live CarPlay**; the head-unit hook builds 5.1 (`AUDIO_REALTIME_UPMIX=on`). A candidate is therefore a head-unit build/config, not a file.

---

## ✅ Achieved

### Track A — what ships in the car

| Step | Result | Commit |
| --- | --- | --- |
| Float reference of the protect-mask net | Parity with PyTorch 3.6e-7 | cc5ec5d |
| Mask net in the production path | Features on the audio thread, inference on the control worker, 3-miss fallback to the heuristic | 518fba0 |
| Install package | v21a blob, startup env, per-model settings drift check | efa42d9, 81a836e |
| int8 + NEON port | 3.92 → **1.46 M instructions/control**; QNX `expf/logf` were software double — replaced | 6bbb02e → 6d667c7 |
| Krait measurement | Mask **1.33 ms/control** of ~10.7 ms, 839/839 on time, audio thread +1 % | — |
| Reset bug | `spa_stereo_upmix_reset()` dropped the guard/mask pointers — the mask never ran in the car before this | c43a17c |
| Car tuning | Mask drive **1.5**, rear air **−9 dB**: guitars 0.17 / voice 0.08 (4 songs >15 % vs 8 in the 22.09 build); guitar swing range a quarter lower, voice a third lower | 04fb06d |
| Best build | `build/install_protected_WAUZZZF47HA098927.zip` (SHA 710342a1…075f), three models, VIN-locked | 800de9d |

### Track B — separator feasibility

| Result | Meaning |
| --- | --- |
| Bounds measured | With **true** eligible audio the renderer gives **0/40** swinging songs and instruments ~0.21–0.23: the goal is reachable in principle |
| Renderer route (review 18) | Host-only object/bed change: 8→0/40 swings on truth; even the weak sep9 estimate 6→1/20 |
| Tooling | `sep_net.py` with all ablation flags, diagnostics (`diag_gap`, `diag_transitions`, `diag_ceiling`, `diag_lossfloor`, `diag_auxgrad`), rapid loop (queue, `GPU_LEASE`, 20-song screen, `LEADERBOARD.md`) |
| Fixes | Scheduler resume (10.5× LR), log-loss NaN, attribution 0/0 NaN, UTF-16 board, deterministic prefetch seeds |
| Proxy validation | sharedmask's 4k stability gain did **not** survive to 10k — 4k ducking/swings do not predict 10k |

---

## ❌ Problems

```mermaid
flowchart TD
    accTitle: Open Problems and Their Dependencies
    accDescr: The separator frontier, the three-model complexity and the missing listening test all block a confident rear; the listening test gates what to change next.

    listen["👂 No owner listening test yet<br/>front-only vs Track A"]
    shelf["🧱 Separator shelf<br/>guitar 0.09–0.12, voice 0.02–0.04"]
    swings["🔁 Track A still swings<br/>15/20 songs on the enriched screen"]
    three["🧩 Three networks in the hook<br/>q6d + guard + mask"]
    renderer["🛠️ Renderer route fix<br/>host-only, not in the hook"]
    next["❓ What to change next"]

    listen --> next
    shelf --> next
    swings --> renderer
    three --> next

    classDef bad fill:#fee2e2,stroke:#dc2626,stroke-width:2px,color:#7f1d1d
    classDef open fill:#fef9c3,stroke:#ca8a04,stroke-width:2px,color:#713f12
    class shelf,swings,three bad
    class listen,renderer,next open
```

| Problem | Evidence | Status |
| --- | --- | --- |
| **The shelf** | ~38 variants (width, supervision, band RNN, loss, lookahead, complex/matrix/role masks, 8 s memory) land on one recall/leak frontier: guitar 0.09–0.12, voice 0.02–0.04. Target ~0.20 | Open |
| **Track A still swings** | Enriched 20-song real-C screen: 15/20 any-stem swing songs; instruments .176/.166/.118/.225, voice/bass/drums .075/.150/.285 | Open |
| **CMA-ES on the frozen mask failed** | 10 params, 18 candidates, 6 songs: best score silenced the rear by 4.03 dB | Closed (negative) |
| **Three networks** | q6d (routing) + guard (vocal safety, same graph) + mask v21a, run in series on one worker; three blobs, three settings | Open |
| **No listening test** | Owner blind test front-only vs Track A over CarPlay never done | Blocking |
| **Renderer fix not in the car** | The strongest swing lever exists only in the host C renderer | Open |
| **GPU contention** | One GPU training at a time; Codex's `operator_anchor4k` holds it now | Scheduling |

### Two untested causes of the shelf

1. **No cross-song stem remix.** `windows()` in `tools/stereo-upmix-render/sep_net.py` builds every training window from the stems of *one* song (only ±6 dB per-slot gains). Standard separator training mixes stems from different songs and offsets; with ~240 true-stem songs this is the cheapest untried data lever.
2. **Under- vs over-fitting was never measured for the separators.** The Track A mask lineage was flat on TRAIN as on DEV (AUC 0.81/0.76), i.e. underfit. For the sep students the TRAIN-vs-DEV screen has not been run. A TRAIN gap → data (remix, pseudo-labels on the 3,367-song `D:\music`); no gap → capacity/training length (the Krait has 2–4× headroom).

---

## 🧭 Next steps

```mermaid
flowchart LR
    accTitle: Planned Order of Next Steps
    accDescr: Cross-song remix first, then the train versus dev fitting check, then the renderer route in the hook, with the listening test and the one-network merge after.

    remix["1️⃣ Cross-song remix<br/>flag in windows(), 4k rapid run"]
    fit["2️⃣ TRAIN vs DEV<br/>CPU, 30 min"]
    route["3️⃣ Renderer route<br/>into the hook over v21a"]
    listen["👂 CarPlay listening<br/>front-only vs Track A"]
    merge["🧩 One network<br/>distil q6d+guard+mask, parity gate"]

    remix --> fit --> route --> listen --> merge

    classDef step fill:#dbeafe,stroke:#2563eb,stroke-width:2px,color:#1e3a5f
    class remix,fit,route,listen,merge step
```

| # | Step | Gate |
| --- | --- | --- |
| 1 | Cross-song remix flag in `windows()`; one 4k rapid run after the GPU lease frees | +0.03 eligible at ≤ +0.01 vocal vs the 4k anchor |
| 2 | TRAIN vs DEV screen of sep3 and sharedmask_10k | Decides data vs capacity |
| 3 | Review-18 renderer route in the hook, v21a unchanged | Fewer swings on the 20-song screen at equal rear level |
| 4 | Owner listening over CarPlay | Owner verdict |
| 5 | One multi-head network distilled from the three | Parity with Track A, cost ≤ today |

---

## 🔗 Records

- [Separator handoff](UPMIX_SEPARATOR_HANDOFF.md) — full ledger and rules
- [Separator night report 2026-09-28](UPMIX_SEPARATOR_NIGHT_REPORT_2026-09-28.md)
- [Source separation plan](UPMIX_SOURCE_SEPARATION_PLAN.md)
- [Listening and deploy log](UPMIX_LISTENING_AND_DEPLOY_LOG.md)
- [HSSR model](HSSR_MODEL.md)
