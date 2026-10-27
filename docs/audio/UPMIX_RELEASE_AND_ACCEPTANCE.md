# Upmix Release and Acceptance

> **Retired 2026-10-09 with the Universal backend.** The renderer, models and knobs this note describes left the tree; the last tree that has them is git tag `upmix-universal-last-2026-10-09`.
> What runs now: [Stereo to 5.1 Upmix](STEREO_TO_5_1_UPMIX.md). Why, and with which numbers: [Upmix History and Results](UPMIX_HISTORY_AND_RESULTS_2026-10-09.md), [Upmix Code Retirement](UPMIX_CODE_RETIREMENT_2026-10-09.md).
> The LFE, amp and measurement facts below remain valid.

*The release bar the upmix was meant to clear — the frozen acceptance protocol, the runtime audit, the cabin constraints — and the research candidates (G-series, P1, Q2, cabin-first) that were built against it and never shipped. Records 2026-09-08 → 2026-09-17.*

---

## 📋 Where the shipped upmix stands against the bar

Three release statuses were defined and kept distinct: **research candidate** → **installation-ready candidate for controlled-car evaluation** (all non-CABIN gates) → **acoustically validated in-car release** (CABIN too). Missing evidence never becomes a pass; lack of hardware or listener evidence is UNVERIFIED / BLOCKED, not a waived gate.

The HSSR scene upmix in the car ([Stereo to 5.1 Upmix](STEREO_TO_5_1_UPMIX.md)) did **not** get there through these gates. It shipped on Luka's owner decision after sighted listening ([Upmix Listening and Deploy Log](UPMIX_LISTENING_AND_DEPLOY_LOG.md)). Measured against the protocol it is still a research candidate: no frozen-TEST scoring, no Krait full-chain loaded trace, no blind 24-song listening, no cabin recording.

```mermaid
flowchart LR
  accTitle: Upmix release statuses and gates
  accDescr: A research candidate becomes installation-ready only after the data, quality, native, target and package gates pass, and acoustically validated only after blind listening and cabin evidence; the shipped HSSR upmix bypassed this ladder by owner decision.
  research["🧪 Research candidate"]
  quality_gates["📊 Coverage, recovery, protection, bass"]
  native_gates["⚙️ Native parity and streaming"]
  target_gates["⏰ Krait P99 and 2 h load"]
  package_gate["📦 Package and rollback"]
  install_ready["✅ Installation-ready"]
  listening_gate["👤 Blind 24-song listening"]
  cabin_gate["🔍 Car recordings with MMI state"]
  validated["🏁 Acoustically validated"]
  shipped["🚀 HSSR scene in the car"]
  research --> quality_gates --> native_gates --> target_gates --> package_gate --> install_ready
  install_ready --> listening_gate --> cabin_gate --> validated
  research -.->|owner decision| shipped
  classDef primary fill:#dbeafe,stroke:#2563eb,stroke-width:2px,color:#1e3a5f
  classDef success fill:#dcfce7,stroke:#16a34a,stroke-width:2px,color:#14532d
  classDef warning fill:#fef9c3,stroke:#ca8a04,stroke-width:2px,color:#713f12
  class research primary
  class install_ready,validated success
  class shipped warning
```

---

## 🎯 The frozen acceptance protocol

Declared **2026-09-16** before any new release-candidate scoring, as a prospective engineering spec, not a claim that anything passes. v1 is sealed and preserved; v2 is the self-contained successor with **the same 38 gate IDs, thresholds, directions and scopes**. Machine sources: `tools/stereo-upmix-render/release_acceptance_v1.json` / `_v2.json`; the sealed v2 text is kept verbatim in [UPMIX_RELEASE_ACCEPTANCE_V2](../sources/UPMIX_RELEASE_ACCEPTANCE_V2.md); v2 freeze `output/upmix-release-protocol-v2/` (protocol SHA `214acf8d…`, metric spec `39f452e6…`). Any change creates a new revision and an all-candidate rerun; it can never rescue an already exposed final TEST. Thresholds are conservative product budgets, not psychoacoustic standards.

| Gate family | Pass condition |
|---|---|
| Data / exposure | artist, song-group, file-hash and PCM-duplicate disjoint TRAIN/DEV/TEST including crops and augmentations; historical TRAIN stays TRAIN; inspected DEV never becomes untouched TEST |
| Coverage | ≥ 12 songs / 4 artists; each source family and protected role ≥ 3 songs / 2 artists; sparse, dense acoustic/electronic incl. dubstep, mono/narrow/hard-pan, overlap, quiet, rapid role changes; uncovered cells reported, never filled by an overall count |
| Eligibility floors | relative metrics need target core RMS ≥ −60 dBFS and target/mix ≥ −40 dB; below that, absolute error and quiet status only |
| Training | full-timeline sampling with ≥ 1 complete coverage pass; ≥ 5 checkpoints of TRAIN/DEV curves after coverage; DEV-only selection; matched-budget architecture comparisons |
| Source recovery | NMSE (no SI rescaling); recovery `10·log10(control/candidate)` median ≥ 3 dB, song-cluster bootstrap 95 % lower bound ≥ 0 (10 000 resamples, seed 20260916), worst active song NMSE ≤ 0.8 |
| Protection | isolated protected source → rear ≤ −30 dB worst; mixed response `rear(X) − rear(X−P)` P95 ≤ −24 dB, worst ≤ −18 dB |
| Useful support | per-channel LS coefficient 10th pct ≥ −6 dB, 90th ≤ +1 dB; ≥ 80 % of eligible supporting intervals routed; an always-abstain rear fails |
| Front transfer | worst target-coherent gain error ≤ 0.5 dB in PCM and J525 input proxy; P95 interchannel phase error ≤ 5° |
| Attacks / sustains | P95 RMS-envelope error ≤ 3 dB (50 ms onset windows, 100 ms sustain windows) |
| Bass | 20–120 Hz energy change ≤ 1 dB worst vs aligned stereo proxy; no new LFE boost; one SoundCheck gain; probes at −9 / 0 / +1 dB |
| Native / streaming | reference vs native ≤ 5e-5 normalized and ≤ 2 LSB S16; reset silence exact zero; arbitrary chunks, seeks, generations, invalid model; ≥ 2 h continuous stress; no allocation, locks or blocking I/O in the callback |
| TARGET | ≤ 40 ms algorithmic latency; ≤ 32 MiB state + weights + work; loaded full-path P99 ≤ 80 % and max ≤ 100 % of the **actual** callback period; zero misses/underruns over ≥ 2 h of competing load; only real APQ8064/Krait/QNX evidence counts — QEMU or host MACs never do |
| Package | exact weights/config/hashes, licences, reproducible commands, six-slot ABI, SHA-bound manifest (integrity, not a signature), preflight/backup/rollback; installation stays a separate user authorisation |
| Listening | coded randomized A/B from a committed seed, one full-render level match per pair (≤ 0.2 dB), rear-only and full renders; **24 independent verdicts on 24 distinct song clusters**, ties = non-preference, lower 95 % Wilson bound > 0.5, no severe lead/front/bass/artifact regression; headphone preview is not car evidence |
| CABIN | authorised car recordings/listening with recorded seat, levels and MMI state, front/rear/source diagnostics; no matrix, wiring drawing or HRTF substitutes |

For a 1024-sample 48 kHz callback the TARGET budget is P99 ≤ 17.067 ms and max ≤ 21.333 ms; the shipped realtime path runs 768-frame / 16 ms periods, which needs its own cohort.

### What the v2 errata fixed

Read-only synthetic counterexamples made the v1 evaluator PASS a negative silence peak, a negative native error, −2.5 missed deadlines, a negative period ratio, a Wilson bound of 2.0, a non-timestamp string and an unmaterialised domain hash. v2 (evaluator `upmix-release-acceptance-evaluator-v2`, schemas `upmix-release-*-v2`) repairs the **evidence ledger**, not the thresholds:

- explicit numeric domains per gate (errors, ratios, time, memory, counts ≥ 0; deadlines and LSB integers; coverage/bounds in [0, 1]);
- every report binds a hash-verified domain manifest whose eligible IDs it must reproduce exactly (units `song_cluster`, `song_verdict`, `reference_native_run`, `target_cohort`);
- all TARGET reports share one cohort (same build, rate, period, workload, `emulated:false`); both LISTENING gates share one manifest;
- UTC timestamps parsed, never future-dated, never before the freeze except re-verified AUDIT;
- exact dB boundaries via `exact_log_ratio {numerator, denominator, multiplier}`: 0/positive = −∞ (zero leakage passes, zero support fails), positive/0 = +∞, 0/0 = UNVERIFIED except recovery between two perfect results = 0 dB; absolute-error gates take `abs` after the log; `coherent_sign ∈ {−1, 0, 1}` so negative transfer cannot hide behind a magnitude.

The ledger checks binding and arithmetic; it does not compute metrics, authenticate hardware or conduct listening. v1 evaluator bytes are preserved (SHA `9041f1ff…`).

---

## ⚙️ The runtime audit (2026-09-16)

Result: **runtime infrastructure available, no model or complete car path approved** (`release_ready:false`, `car_approved:false`). A fresh QNX GCC 8.5 ARM32 build (`-O2 -mfpu=neon -mfloat-abi=softfp -ffp-contract=off`, no fast-math) succeeded; the previously packaged P1 core and callback re-ran in an isolated QNX guest (max normalized PCM error 1.19e-7) — but P1 had already failed its musical qualification, so passing runtime tests does not re-admit it.

| Boundary | Rate / layout / amplitude | Status |
|---|---|---|
| Frozen P1 core | 48 kHz normalized FLOAT stereo → `FL FR C LFE SL SR`, C/LFE 0 | native + guest parity |
| P1 callback | same FLOAT, exactly N in/out, fixed 1024-sample delay | diagnostic only |
| Hook FLOAT route (since deleted) | 48 kHz, six slots, FLOAT in **S16 amplitude units** | source only |
| QSA sink | interleaved `S16_LE`, 48 kHz | device setup |
| APAC 5.1 decoder | `C L R Ls Rs LFE`, remapped `[1,2,0,5,3,4]`, never re-upmixed | `[code]` |

Traps named there: never feed normalized output into the S16-scaled guard or apply the 32768 conversion twice; the hook's error branch (immediate stereo widening) is not a latency-aligned fallback for a 1024-sample model; the hook's outer renderer mutex means an allocation-free core does not make a lock-free callback.

**Historical Krait measurements** (standalone upmix, 1024 samples = 21.333 ms period, timer resolution ≈ 1.007 ms) `[car]`: HSSR-NEON mean 4.283 / P95 6.043 / P99 8.057 / max 9.065 ms; the Object renderer mean 5.022 / max 13.093 ms, loaded P99 21.15 ms and later above the period. They exclude decode/transport/QSA, so they do **not** establish 75–80 % spare CPU. QEMU instruction counts are per 1024 samples for V12 float (6.61 M) but per 256-sample hop for P1 (7.74 M = 30.95 M per 1024); never convert either to ARM ms.

**Cabin constraints that every candidate inherits:** 19 drivers on 16 circuits; each rear door's woofer and tweeter share one feed (left T38 4/5, right 16/17); C-pillar R209/R210 do not become extra PCM channels; 3D OFF is not a DSP bypass; C/LFE = 0 in PCM does not mute the physical centre or sub; Focus All / Surround minimum are assumptions pending readback. Amp detail: [Virtual Cabin and Amp RE](VIRTUAL_CABIN_AND_AMP_RE.md).

**MMI 3D numbering, verified statically** (MU1316 Java + native MIBRoot `requestSet3DSound` at VA `0x1EB0FC`, table `0xDDEE50`, dispatch of SoundSettings `0x22` at `0x1EB1F0`) `[decompile]`:

| MMI label | UI index | DSI mode | HU SoundSettings 0x22 |
|---|---:|---:|---:|
| OFF | 0 | 1 | 0 |
| LOW | 1 | 2 | 1 |
| MID | 2 | 4 | 2 |
| HIGH | 3 | 8 | 3 |

OFF → HU level 0 is proven; HU level 0 → J525 field 4 is a strongly supported link, not traced end to end through RCC/MOST. OFF does not prove the complex block C4D17 or the auxiliary mix are off (its descriptor+40 bit 0 gates the entry and V850 `0xAA89A` sends 1; descriptor+39 selects stage mask 4 or 2 by `gp−0x3D62`; the two full masks `0x0FFF7FE5` / `0x0DFF75E5` differ only in stage bits 9, 11, 25; 1133 instructions decode cleanly, helper `0x126CEC` keeps ten UNKNOWN operations behind stage bit 4): that block is stateful and signal-dependent (five paths, recursive filters, power history, gain-change clamps), so no constant gain row or `symphoria_3d_enabled=false` can stand in for it.

---

## 🔍 Research candidates that never shipped

Each was built against the gates above and stopped short. Production remained HSSR throughout.

### The G-series source separators (2026-09-12/13)

Every run, plan SHA, checkpoint hash and PID is in the execution journal [UPMIX_TO_VEHICLE_EXECUTION](../sources/UPMIX_TO_VEHICLE_EXECUTION.md).

Small source-reconstruction networks on MoisesDB (TRAIN 181 songs / 28 artists → 572 eight-second windows; DEV 40 songs / 9 artists → 80 windows; 19 held out). Every one failed on **backing** and **lead-only leakage** even where the summed NMSE improved; none was promoted `[model]`.

| Gen | Idea | Outcome |
|---|---|---|
| G5 | width 8 / GRU 64 invariant, 1024 updates | two-crop sum NMSE 0.330, backing 0.354 / 0.635 — the bridge control |
| G6 | long invariant window | +0.73 % sum, both backing crops worse |
| G7 | band-wise GRU8 + global GRU64, 96 508 params | sum 0.514–0.552, backing ≈ 0.82–0.85 — worse than G5 |
| G8 | full-TRAIN curriculum, 4096 updates (NAS, 9328 s) | DEV sum 0.697, backing 0.927 / gain 0.046, lead-only energy 0.43 |
| G9 | relative-power floor 1e-4 on balance/coherence | fixes a real numeric bug (native FFT drift ≈ 1.08e-4), S16 perturbation −674× — adopted as baseline, not a quality win |
| G10 | inter-frame phase features | −1 % sum, lead-only mixed |
| G11 | factorized rank-24 decoder | small worse; wide +1.7 % but second lead-only energy doubled |
| G12 | G8 with the reliable floor | DEV 0.690 vs 0.697 — conditioning, not generalization |
| G13 | full-frequency vs stride-4 encoder | full-bin −4.4 % sum, backing worse |
| G14 | foundation as residual head | sum 0.256, lead-only second crop 0.042 → 0.439 |
| G15 | Helmert orthogonal outputs | sum 8.5 % worse |
| G16 | same model, 8192 updates | sum −28 % on the two crops; lead-only worse; memorisation |
| G17 | G12 continued to 8192 (staged qualification → main) | completed; not promoted — the direction switched to direct stereo → 5.1 |

Durable lessons: poor fit already existed on **seen** TRAIN cores (G8/G12 seen-TRAIN sum ≈ 0.52–0.54), so it was not only generalization; 15.5 % of updates clipped (33.6 % in the first quarter); the historical `unknown` residual had a median condition number 327.7 vs 25.9 for foundation. Native C for G5/G8 reached 2.78 M QEMU instructions per frame and passed QNX guest parity — never wired into production. The `unknown`-only test track (Battlestar / Fixed) is an undeclared-bleed clean electric guitar, so its rear energy is not vocal leakage.

### P1 direct stereo → 5.1 and the stereo → 7.1 detour (2026-09-13)

The scope flipped from eight-output proposals to direct stereo → 5.1 (P1): synthetic-tested model/trainer, 58 Mac + 38 NAS tests, two real 48 kHz targets staged with `roles=pending`, no real-music admission. The relative-energy P1 comparison (960 updates per arm) failed reconstruction and active-band criteria on both arms. Learning an authored 5.1 layout is not the product: authored rears can legitimately carry lead-vocal effects our front-only policy forbids.

### The cabin-first redesign and routing review (2026-09-08, 2026-09-15)

Recommendation (never implemented as a replacement): change the **target** before another architecture sweep — stereo → source-aligned components + role evidence + uncertainty → permission to relocate supporting material → a bounded J525-aware renderer → one SoundCheck gain + linked safety → PCM6. Principles that survive into the shipped design:

- keep three signals apart: the estimate `e`, the part actually removed from the front `m`, and the rear rendering `r`; only a source-aligned relocated component may be subtracted from the front — never synthetic ambience (the shipped scene path subtracts the source-aligned object, 2026-09-17);
- no minimum rear quota; safe front fallback and useful rear are different outcomes;
- unknown / uncertain / disagreeing material stays front; bass body of a foreground source never goes rear;
- roles, not instrument lookups: lead, co-lead, doubles, wordless hooks, principal guitar/synth/growl stay front;
- rear high shelf and ±0.72–0.95 steering were flagged as A/B options, not truths.

Routing-review specifics still worth knowing: the pre-scene rear subtracted the whole synthetic rear (object + side ambience + phase-rotated universal bed) from the front; universal makeup was `sqrt(0.32 / (rear/input power))` clamped 1…3.5 (a forced ≈ 32 % rear, later capped 1.5); object pan was forced to ±0.78…0.95 and fallback pan alternated ±0.72 across broad bands; the +3 dB / 4 kHz shelf is +2.865 dB at 8 kHz with ≈ 13.94° phase at 4 kHz; enabling LFE assist trims the whole front by −0.347 dB (32670 → 31392). Proposed frequency policy: below ~120 Hz no semantic relocation; 120–250 Hz very conservative; 250–800 Hz confident supporting instruments with their harmonics; 800 Hz–4 kHz the main useful rear-object band with the strongest vocal protection; 4–8 kHz controlled continuation with sibilance rejection; above 8 kHz modest ambience, no minimum. The cabin-first reference ladder: (1) target proof with role-confirmed real stems, (2) strong offline separation through the same policy, (3) causal student on a small varied set, (4) coverage and deployment — each with oracle-role and predicted-role tests. Its R1 conditioned-reconstruction line reached only +0.11 dB SDR over scalar attenuation (context X+E NMSE 0.746 vs scalar 0.894; a one-song full-bin linear control 0.531). External evidence cited: rear-moved vocal reverb shifts the perceived foreground (Paulus & Torcoli, EUSIPCO 2022); ADI's 131k-param, 1.89 GMAC/s separator on SHARC-FX (2026-09-10) with a streaming/training mismatch of its own.

Its matrix algebra (`BL = .794 L + .708 rL`, "generic downmix closure cannot preserve the front") used the transposed amp matrix and is **withdrawn** — see [Upmix Calibration and Tuning](UPMIX_CALIBRATION_AND_TUNING.md#the-corrected-matrix-changed-the-diagnosis). The routing review's reading of the B9 S4 speaker workbook stands: hobby mic sweeps, rear door woofer ≈ 160–5000 Hz, rear tweeter 2–20 kHz, a broken `#REF!` summary cell, S4 sub data not applicable to the S5 enclosure — constraints, not inverse-EQ targets.

### V12 / V13 cabin render experiments (2026-09-08)

`AUDIO_FLOAT_RENDER`, `AUDIO_CABIN_RENDER`, `AUDIO_QUIET_LFE`: float synthesis → one SoundCheck gain → linked −1 dBFS matrix/rail guard → one S16 conversion; the cabin policy subtracted 1.0 × the original-position source estimate (instead of 2.5 × the rear image), disabled the synthetic rear floor, capped the mask at 1.0 and bypassed the rear shelf; quiet LFE halved the assist (0.0625) and removed the −0.35 dB broadband front trim. Cost ≈ +8 % (float) and +17 % (cabin) QEMU instructions. The cabin renderer's mixture-conditioned attribution showed a **negative** instrumental-vs-vocal transfer advantage on 6 of 12 excerpts. V12 fine-tunes A–E were all rejected (V12-A: vocal rear leakage +18 % validation / +49 % held-out). V12 learned candidates had to meet a pre-declared screen — ≥ 90 % of V11 weighted instrumental rear transfer, ≤ 110 % of its vocal leakage, ≥ 95 % of its selectivity on validation and held-out stems, ≤ 1 % validation-loss increase per input level, ≤ 5 % mean C-render level variation, zero matrix over-range — and every one failed (B: selectivity 0.937; C: vocal transfer 6.78×, from reading zero class mass as a negative label; D: held-out vocal +22 %, selectivity 0.80; E, an 87.5/12.5 V11/D blend, no gain). V13 on 14 excerpts vs float + old assist: auxiliary LFE energy −6.46 dB (quiet only) / −6.51 (cabin), front 250–4000 Hz +0.36 / +2.52 dB, rear −0.02 / −6.81 dB, max bus 0.8913 FS; its training set grew to 43 songs × 3 × 20 s with native-rate BS-RoFormer teachers, rearability/pan rows only trainable. Mac async float timing: mean 1.079 ms, max 2.424 ms per 1024-frame call (host, not the car). All three paths were **deleted on 2026-09-21** (`9255485`); the S16 output is byte-identical.

### The Q2 release execution (2026-09-16/17)

The hash-bound, timestamped journal of this push is [UPMIX_RELEASE_EXECUTION](../sources/UPMIX_RELEASE_EXECUTION.md).

A full "product" push with source-local guidance, 81 TRAIN / 19 DEV songs (813 / 178 sources), matched predicted vs guidance-disabled arms (24 290 jobs each on the NAS, both completed 2026-09-17 ≈ 02:50 UTC), native six-unit C chain with QNX guest parity (S16 difference ≤ 1 LSB) and a complete gain-session / S16 adapter. Findings that ended it `[model]`:

- predicted backing NMSE worse than **mute** on every active backing aggregate (Rivum 1.071 / 1.049, Firefly 1.049 / 1.034, Halos 1.046 / 1.026); drums uneven (Rivum 0.23 vs Iain Kerr 1.62);
- swapping in true carriers helped backing but broke other heads (quiet bass 0.161 → 2.781) — the decoder had co-adapted cancellation;
- zero placement-confirmed role labels among 813 TRAIN / 178 DEV sources; 12 identity heads merge same-family sources, so principal vs supporting guitar cannot be told apart.

The planned insertion point, had it passed, was `audio_hook.c:spa103_render_frame` after `decodeS16` and trim, in the 48 k stereo → PCM6 branch before the ring — with its own 1024-frame delayed packet tags and no second legacy gain/limiter. It was never linked.

**MPS vs CPU:** MPS was not bit-exact even between two identical uninterrupted runs (≤ 2.98e-8), so real comparisons ran on CPU; see [HSSR Model](HSSR_MODEL.md) for why that refusal became the bottleneck.

### The acoustic bridge (2026-09-20)

`tools/soundamp-virtual-cabin/amp_cabin_render.py` joins a PCM6 candidate to measured impulse responses, in one of two exclusive domains: `most_inputs` (6 stereo IRs L … Rs, end to end, amp model forbidden) or `post_amp_config_outputs` (16 stereo IRs cfg00…cfg15 after an explicit AmpChain). Manifests pin 48 kHz, SHA-256 per response, per-response `gain_db` (±120 dB file bound), provenance `measured`/`synthetic` and capture seat / state / mic calibration / level reference; ear signals sum coherently, overs are preserved, nothing is normalized. Only synthetic smoke fixtures exist — this is the tool the CABIN gate needs once real IRs are recorded. The old `cabin_model.py` six-bus tool (transposed matrix) now refuses to run without `--allow-obsolete-transposed-model`.

---

## 🔐 Data and licence boundaries

- **MoisesDB** is CC BY-NC-SA 4.0, non-commercial research; keep attribution and per-source provenance; never bundle dataset audio with a car package. Trained-weight redistribution consequences are undecided; both shipped weights are flagged development-only.
- **SCNet** code is MIT, but checkpoint redistribution rights were an open issue; a code licence is not a weight licence.
- **Cambridge-MT** multitracks need contributor permission for research and individual licensing for AI training — not assumed usable. **MedleyDB** full access is by request, non-commercial.
- Architectural references (band-split RNN, SCNet, Band-SCNet: 2.59 M params, 92 ms latency, 17.59 GFLOP/s on an i7 — not a Krait admission) motivate experiments; none is a drop-in under the 40 ms / Krait budget.
- Teacher outputs are estimates, never isolated-source or role ground truth.

---

## 📚 Sources

Merged originals were removed from the tree after this note absorbed them; they remain in git history (commit 23972aa).

- `UPMIX_RELEASE_ACCEPTANCE` — frozen protocol v1 (all gates and thresholds).
- [UPMIX_RELEASE_ACCEPTANCE_V2](../sources/UPMIX_RELEASE_ACCEPTANCE_V2.md) — the self-contained v2 spec, evidence representation and cohort integrity.
- `UPMIX_RELEASE_ACCEPTANCE_V2_ERRATA` — the demonstrated v1 ledger defects and exact dB boundary rules.
- `UPMIX_RELEASE_RUNTIME_AUDIT` — build/guest proof, unit boundaries, historical Krait timing, budgets, cabin constraints, target access.
- `UPMIX_RELEASE_REFERENCE_OPTIONS` — architecture references and data/weight licence boundaries.
- [UPMIX_RELEASE_EXECUTION](../sources/UPMIX_RELEASE_EXECUTION.md) — the Q2 release push, native chain, final DEV/TRAIN diagnostics, insertion point.
- [UPMIX_TO_VEHICLE_EXECUTION](../sources/UPMIX_TO_VEHICLE_EXECUTION.md) — G5–G17, native source ABI, P1 scope change.
- `UPMIX_CABIN_FIRST_REDESIGN` — target-first redesign, role policy, reference ladder.
- `UPMIX_CABIN_ROUTING_REVIEW` — workbook evidence, subtraction-source critique, frequency-region policy.
- `UPMIX_CABIN_V12` — float render integration and the rejected V12 fine-tunes.
- `UPMIX_CABIN_V13` — quiet LFE and the cabin render policy, mixture attribution.
- `UPMIX_CABIN_BRIDGE_2026-09-20` — the IR bridge and the legacy transpose repair.
- `UPMIX_STATIC_READINESS_AND_SOUNDCHECK` — the next-upmix contract and acceptance matrix (MMI 3D enum and C4D17 detail live in [Virtual Cabin and Amp RE](VIRTUAL_CABIN_AND_AMP_RE.md)).
- Long-form: [UPMIX](../research/UPMIX.md), [HSSR_UPMIX_MODEL](../research/HSSR_UPMIX_MODEL.md), [VIRTUAL_CABIN](../research/VIRTUAL_CABIN.md).
