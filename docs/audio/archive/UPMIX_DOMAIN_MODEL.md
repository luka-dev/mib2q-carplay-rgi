# Upmix domain model and qualification map

> **Retired 2026-10-09 with the Universal backend.** The renderer, models and knobs this note describes left the tree; the last tree that has them is git tag `upmix-universal-last-2026-10-09`.
> What runs now: [Stereo to 5.1 Upmix](../STEREO_TO_5_1_UPMIX.md). Why, and with which numbers: [Upmix History and Results](UPMIX_HISTORY_AND_RESULTS_2026-10-09.md), [Upmix Code Retirement](UPMIX_CODE_RETIREMENT_2026-10-09.md).
> The LFE, amp and measurement facts below remain valid.

<!-- HISTORY16_DOMAIN_START -->
**Latest October4 decision:** [Explicit encoded history16](UPMIX_SCENE_ENCODED_HISTORY_2026-10-04.md) completed8192 and failed joint comparison/strict fitting, with worse rear and front/centre than MLP12. Retain MLP12; NAS is idle and no efficacy extension is queued. A read-only retained-head gradient audit rejects simple local front domination/conflict. A privileged trained-head/context feasibility diagnostic is proposed but unrun. These remain one-crop TRAIN mechanisms, without new full40/device/cabin qualification.
<!-- HISTORY16_DOMAIN_END -->

_Documentation snapshot, 2026-10-04. Runtime boundaries, experimental evidence and release decisions for CarPlay stereo to 5.1._

<!-- SCENE_DOMAIN_STATUS_START -->
**October4 research update:** The owner resumed NAS CPU joint front/centre/rear research after the September30 source-model study closed. [MLP12](UPMIX_JOINT_SCENE_MLP12_HEAD_2026-10-03.md) improves fixed-crop reconstruction but fails the strict rear fitting gate. The [FlyWire reduction](UPMIX_FLYWIRE_TOPOLOGY_SCREEN_2026-10-04.md) is closed. A [target-informed output witness](UPMIX_SCENE_OUTPUT_REACHABILITY_2026-10-04.md) proves near-exact reconstruction is representable on this crop. The completed [interband8192 trial](UPMIX_SCENE_INTERBAND_CONTEXT_2026-10-04.md) fails its matched comparison and strict fitting gates, so that variant is closed and MLP12 retained. Explicit16ms past encoded-frame memory is an unrun next hypothesis. These are short TRAIN diagnostics, with no new full40/device/live-CarPlay qualification. No follow-up efficacy run is queued; earlier running/paused entries are historical.
<!-- SCENE_DOMAIN_STATUS_END -->

## 🧭 Domain boundaries and current state

| Area | Responsibility | Current evidence |
| --- | --- | --- |
| Track A | Deployed hook, three-model routing/protection, int8 implementation and unit integration | q6d + V11 guard + v21a mask; [phase 1 car report](UPMIX_MASK_PHASE1_REPORT_2026-09-27.md), [matched40 host baseline](UPMIX_TRACKA_MATCHED40_2026-09-30.md) |
| Track B | Eligible extraction and joint authored front/centre/rear research, isolated from deployment | September30 spectral40k fails acoustic promotion; October3–4 joint TRAIN diagnostics test inference/representation, not deployment |
| Reference models | Offline extraction target and feasibility comparison | Noncausal pretrained BSRNN SIMO passes its observed offline gates; [real-C reference check](UPMIX_BSRNN_REAL_C_CHECK_2026-09-29.md); no device deployment |
| Qualification | Stable-gain source attribution, transitions, independent generalization, device timing and owner listening | Original full40 complete; genuinely untouched20, loaded-device and blind live CarPlay acceptance remain open |
| J525 amplifier | Render supplied PCM6 across the cabin speakers | [Virtual Cabin and amp RE](../VIRTUAL_CABIN_AND_AMP_RE.md); cabin behavior cannot be inferred from host arithmetic alone |

The September30 PC source study ended at **2026-10-01 03:28 UTC**, released its lease and was paused by the owner. That result remains archived in the [session wrap](UPMIX_SESSION_WRAP_2026-09-30.md) and [complete attempt chronicle](UPMIX_SEPARATOR_CHRONICLE_2026-09-25_TO_2026-09-30.md). Later NAS authorization resumes a separate joint-scene recipe. This page is a documentation snapshot, not a live queue monitor.

```mermaid
flowchart TB
    accTitle: Upmix runtime and research boundaries
    accDescr: Live CarPlay uses Track A and the cabin amplifier. The archived Track B spectral candidate feeds offline evaluation. Separate joint-scene NAS experiments feed TRAIN fitting diagnostics; neither research branch connects to the car.
    phone_stereo["Phone stereo over live CarPlay"] --> track_a["Track A hook<br/>q6d + V11 + v21a"]
    track_a --> car_pcm["PCM6 to J525"] --> cabin["Owner hears cabin speakers"]
    evaluation_mix["Frozen evaluation stereo"] --> track_b["Track B single spectral network<br/>isolated causal C chain"]
    evaluation_mix --> reference_model["Offline noncausal BSRNN reference"]
    track_b --> real_c_harness["Real C harness<br/>source and transition attribution"]
    reference_model --> real_c_harness
    real_c_harness --> final_verdict["spectral40k: joint gates fail<br/>no deployment promotion"]
    authored_target["Authored five-channel TRAIN target"] --> known_downmix["Known same-target stereo downmix"]
    known_downmix --> joint_scene["Joint front/centre/rear research"]
    authored_target --> fitting_diagnostic["Fixed TRAIN reconstruction<br/>and dry conservation"]
    joint_scene --> fitting_diagnostic
```

## 📦 Evidence entities

This is a conceptual domain model of the existing files, not a new database or a proposed artifact format. A candidate is meaningful only with its exact terminal checkpoint, runtime/build and fixed configuration. A saved score must retain the evaluated source/split, gain, coverage and protocol identity.

```mermaid
erDiagram
    accTitle: Upmix experiment evidence relationships
    accDescr: Datasets provide split-bound manifests. Manifests identify terminal checkpoints and candidates; protocols bind evaluations to candidates and manifests, and evaluations support release decisions.+
    dataset }|--o{ manifest : supplies
    manifest ||--o{ checkpoint : constrains_training
    checkpoint ||--o{ candidate : instantiates
    candidate ||--o{ evaluation : produces
    protocol ||--o{ evaluation : freezes_rules
    manifest ||--o{ evaluation : fixes_coverage
    evaluation }|--o{ decision : supports

    dataset {
        string source_identity
        string role_mapping
        string overlap_audit
    }
    manifest {
        string sha256
        string train_dev_membership
        string crop_and_draw_identity
    }
    checkpoint {
        string sha256
        int terminal_step
        string architecture_identity
    }
    candidate {
        string checkpoint_identity
        string runtime_and_build_identity
        string fixed_config
    }
    protocol {
        string sha256
        string predeclared_gates
        string stable_gain
    }
    evaluation {
        string source_and_event_keys
        string saved_audio_and_metrics
        string observed_scope
    }
    decision {
        string failed_and_passed_gates
        string promotion_status
        string remaining_requirements
    }
```

| Evidence layer | Canonical entry point | Interpretation boundary |
| --- | --- | --- |
| Corpora and stem roles | [Datasets and teachers](../UPMIX_DATASETS_AND_TEACHERS.md) | Real stems, pseudo stems and inspected DEV have different evidential value |
| Recipes and failures | [Attempt chronicle](UPMIX_SEPARATOR_CHRONICLE_2026-09-25_TO_2026-09-30.md) | Stopped, prepared and executed attempts have distinct recorded outcomes |
| Candidate implementation | [Unified architecture ledger](UPMIX_UNIFIED_SCENE_ARCHITECTURE_2026-09-30.md) | Numerical parity and operation counts do not establish acoustic or device acceptance |
| Final acoustic comparison | [Spectral full40 decision](UPMIX_UNIFIED_SCENE_ARCHITECTURE_2026-09-30_spectral_broad_full40_complete.json) | 40 songs × 60 s, 231 stem rows, fixed gains and 265 attributed events |
| Archive identity | [Final inventory](UPMIX_UNIFIED_SCENE_ARCHITECTURE_2026-09-30_spectral_broad_final_inventory.json), [aggregation checks](UPMIX_UNIFIED_SCENE_ARCHITECTURE_2026-09-30_spectral_broad_archive_checks.json) | Hashes preserve the original results; recomputation checks their summaries |
| Car evidence | [Listening protocol](UPMIX_CARPLAY_LISTENING_PROTOCOL.md), [deploy log](UPMIX_LISTENING_AND_DEPLOY_LOG.md) | Only live CarPlay and the owner can establish the requested cabin verdict |

## ⚙️ September30 source architecture, archived

```mermaid
flowchart TB
    accTitle: Isolated single network audio architecture
    accDescr: A past-only STFT feeds one shared spectral encoder. Its eligible decoder supplies causal audio to a neural-free renderer; protected reconstruction heads are training-only and the scene branch stays dormant.

    stereo_pcm["Stereo PCM"] --> past_stft["Past-only STFT<br/>2048 / 512"]
    past_stft --> shared_encoder["Shared spectral encoder<br/>width48, two layers"]
    shared_encoder --> eligible_mask["Eligible complex mask<br/>decoder24"]
    shared_encoder -.-> protected_training["Vocal / drums / bass<br/>auxiliary TRAIN reconstruction"]
    shared_encoder --> dormant_scene["Scene branch dormant<br/>frozen parameters"]
    eligible_mask --> causal_audio["Complex multiply + causal WOLA"]
    stereo_pcm --> nn_free_renderer["NN-free six-channel renderer"]
    causal_audio --> nn_free_renderer
    nn_free_renderer --> research_pcm["Research PCM6<br/>center zero"]
```

Selected eligible + scene dense arithmetic is **116.712 M MAC/s**; the full four-source graph is 144.387 M. These exclude FFT/WOLA, normalization, nonlinearities, DSP, memory and scheduling. They are not measured device int8 throughput. Native float/int8 C and ARM/QNX numerical checks are archived in the ledger; full40 native int8 acoustic qualification has not run.

The reserved upper64 variant has 23 Python structural checks and 129.327 M selected dense MAC/s. It has no efficacy result or compatible new C/int8 port. Float-preserving initialization does not guarantee preserved int8 activation scaling. The [session wrap architecture section](UPMIX_SESSION_WRAP_2026-09-30.md#-architecture-and-implementation-boundary) retains runtime state and latency details.

## 🚦 Qualification lifecycle and stopping point

```mermaid
stateDiagram-v2
    accTitle: Upmix candidate qualification lifecycle
    accDescr: A frozen experiment proceeds through terminal and acoustic checks before independent, device and owner qualification. Any failed acoustic decision is archived; the September30 spectral candidate stopped there.

    state "Freeze paired experiment" as frozen_pair
    state "Audit fixed terminal" as terminal_audit
    state "Original full40 acoustic gates" as full40_gates
    state "Archive rejected candidate" as archived_failure
    state "Untouched20 and native int8 acoustics" as independent_native
    state "Loaded-device budget and latency" as device_qualification
    state "Owner blind live CarPlay test" as owner_listening
    state "Qualified release candidate" as qualified_release

    [*] --> frozen_pair
    frozen_pair --> terminal_audit: Complete fixed training
    terminal_audit --> full40_gates: Numerical audit passes
    terminal_audit --> archived_failure: Audit fails
    full40_gates --> archived_failure: Joint gates fail
    full40_gates --> independent_native: Joint gates pass
    independent_native --> device_qualification: Generalization and int8 acoustics pass
    independent_native --> archived_failure: Required gate fails
    device_qualification --> owner_listening: Causal, budget and latency pass
    device_qualification --> archived_failure: Required gate fails
    owner_listening --> qualified_release: Owner acceptance passes
    owner_listening --> archived_failure: Owner rejects
    qualified_release --> [*]
    archived_failure --> [*]

    note right of archived_failure
        September30 spectral40k candidate:
        bass, movement and entry gates fail.
        Owner-requested research pause.
    end note
```

At nominal .40, the spectral candidate has G/K/P/S rear shares .225/.217/.219/.191, but bass .1585 exceeds .06, swings are 31/40 and entry median/p10 is .7991/.4509. Both arms pass 9/13 absolute gates; the spectral arm also fails relative promotion on paired source SDR and string regression. Higher overall guitar share coexists with weak 6–16 kHz rear transmission. These source-dominance measurements do not mean that 22.5% of the whole song is sent behind the listener or that the rear sounds bright.

Required release conditions remain all instrument/protection/stability gates on full40 **and genuinely untouched20**, causal execution, ≤150 M MAC/s int8 measured on the device, ≤85 ms extra loaded-chain latency, and the owner's level-matched blind live CarPlay verdict. Previously inspected reserved20 songs cannot satisfy the untouched requirement.

The September30 source-model stopping plan reserved at most one matched upper64-versus24 hypothesis after explicit resumption, retaining bass, movement and entry gates. A further joint failure was to close that finite source search. The owner subsequently authorized the separate October3–4 joint-scene fitting experiments linked above; that does not promote the archived spectral candidate or remove any release gate. Each new mechanism uses a bounded trial and records failed attempts rather than silently extending architecture enumeration.
