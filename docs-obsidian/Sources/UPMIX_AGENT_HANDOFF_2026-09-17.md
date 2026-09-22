# Audi Virtual Cabin upmix — independent review / takeover prompt

Prepared for Luka, 2026-09-17. Live NAS snapshot: **2026-09-17 12:37:09 UTC**.
This document contains private project paths, but **no passwords or signed download tokens**.
It is a handoff, not approval of the sound, a release, or a new experiment launch.

## Your assignment

You are a new agent reviewing an ongoing stereo → **5.1**, not 7.1, upmix project
for Luka's Audi Virtual Cabin. Read this handoff and the linked evidence, verify
live state, and independently assess whether the current approach can achieve
the musical product. Do not assume that previous implementation decisions are right.
First inspect read-only. Present concrete findings and a bounded next step.
If Luka asks you to take over, continue the authorized development goal below;
do not start duplicate jobs, mutate an active experiment, or silently extend its budget.
Coordinate write ownership if another agent is still working in this shared repository.

The goal is a complete trained model + native runtime + cabin renderer + recoverable
installation package, with an independent, natural rear supporting musical scene
and preserved front fidelity. Not merely lower loss, unit-test success, or a demo.
Ordinary development decisions can be autonomous. Vehicle installation, factory
parameterization, paid services, deletion of accumulated data, and external pushes
require separate authorization. Finish unblocked work; report real hardware,
listening, data or access blockers without claiming success.

## 1. Workspace, access, and safe status check

Local repository (all relative paths below resolve here):

```text
/Users/luka/Desktop/AUDI_2/Patches/mhi2-carplay
```

NAS: **100.85.38.96**, through the existing Tailscale connection.
Historical LAN address: 192.168.1.161; do not substitute it without checking.

```text
NAS private base B:
/mnt/shared/users/luka/Audi/upmix-training-2026-09-11

NAS runtime R:
/mnt/shared/users/luka/Audi/upmix-training-2026-09-11/runtime

NAS existing CPU Python:
/mnt/shared/users/luka/Audi/upmix-training-2026-09-11/runtime/c2-teacher-env-a/venv-cpu/bin/python

Existing local credential/SSH helper:
/Users/luka/Desktop/AUDI_2/office_nas_ssh.sh

Existing programmatic SSH wrapper:
output/q2-carrier-continuation-qualification-a/stage_launch.py :: ssh(code, payload=b'')

Local Python used for orchestration:
/opt/homebrew/opt/python@3.14/bin/python3.14

Historical SMB mount (not present in this handoff's local directory check):
/Volumes/private/Audi/upmix-training-2026-09-11
```

SSH wrapper uses `sshpass -e`, `oem@100.85.38.96`, then `sudo -S -u luka`, with
strict existing host-key validation, no agent forwarding and a 45-second timeout.
Use the existing local `SSHPASS` assignment; do not print the helper, password,
environment or process command lines indiscriminately, and do not enable shell tracing.
Do not weaken SSH host-key checking. Credentials are already available in the
authorized local helper; no password needs to be copied into a chat or this file.
If another agent runs outside this Mac, these paths do not grant it access: Luka
must provide an authorized environment or transfer the handoff/evidence securely.

This command is read-only on NAS and does **not** start training:

```sh
cd /Users/luka/Desktop/AUDI_2/Patches/mhi2-carplay
eval "$(rg '^export SSHPASS=' /Users/luka/Desktop/AUDI_2/office_nas_ssh.sh)"
env PYTHONDONTWRITEBYTECODE=1 /opt/homebrew/opt/python@3.14/bin/python3.14 - <<'PY'
import json, sys
sys.path.insert(0, 'output/q2-carrier-continuation-qualification-a')
from stage_launch import ssh
code = r'''
from pathlib import Path
import datetime, json, shutil
R = Path('/mnt/shared/users/luka/Audi/upmix-training-2026-09-11/runtime')
def alive(pid, birth):
    try:
        raw = (Path('/proc') / str(pid) / 'stat').read_text()
        fields = raw[raw.rfind(')') + 2:].split()
        return fields[19] == str(birth) and fields[0] != 'Z'
    except (FileNotFoundError, ProcessLookupError):
        return False
out = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
       'free_disk_bytes': shutil.disk_usage(R).free,
       'available_ram_bytes': next(int(x.split()[1])*1024
           for x in Path('/proc/meminfo').read_text().splitlines()
           if x.startswith('MemAvailable:')), 'owners': {}}
for name in ['q2-objective-pair-a', 'q2-carrier-continued-control-a',
             'q2-carrier-continued-candidate-a', 'q2-carrier-dev-control-a',
             'q2-carrier-dev-candidate-a', 'q2-full-posttrain-a']:
    p = R / name
    d = {'exists': p.exists()}
    f = p / 'status.json'
    if f.is_file():
        s = json.loads(f.read_text())
        d['status'] = s
        if s.get('pid') and s.get('birth'):
            d['controller_alive'] = alive(s['pid'], s['birth'])
        for child in s.get('children', {}).values():
            if child.get('pid') and child.get('birth'):
                child['identity_alive'] = alive(child['pid'], child['birth'])
    f = p / 'latest.json'
    if f.is_file():
        d['latest'] = json.loads(f.read_text())
    out['owners'][name] = d
print(json.dumps(out, indent=2))
'''
r = ssh(code)
if r.returncode:
    print(json.dumps({'returncode': r.returncode, 'stderr_bytes': len(r.stderr)}))
    raise SystemExit(1)
print(r.stdout.decode())
PY
```

Alternative existing helper, after loading `SSHPASS`:
`python output/q2-objective-pair-stage-a/stage.py readback`.
That command reads NAS and writes a **new local evidence snapshot**.
Do not run that script's `build`, `qualify` or `arm` commands again for the current owner.
SSH failure does not prove that detached NAS training stopped.

## 2. Read these first, and understand chronology

1. [Master plan](UPMIX_MASTER_PLAN.md).
2. [Current improvement plan](UPMIX_Q2_IMPROVEMENT_PLAN.md).
3. [Execution record](UPMIX_RELEASE_EXECUTION.md).
4. [Frozen acceptance v2](UPMIX_RELEASE_ACCEPTANCE_V2.md),
   [errata](UPMIX_RELEASE_ACCEPTANCE_V2_ERRATA.md), and
   [machine protocol](../tools/stereo-upmix-render/release_acceptance_v2.json).
5. [Q2 local guidance](UPMIX_Q2_LOCAL_GUIDANCE_MODEL.md),
   [direct-power interface](UPMIX_Q2_POWER_GUIDANCE.md),
   [full-training comparison](UPMIX_Q2_FULL_TRAIN_COMPARISON.md),
   [full-source trainer](UPMIX_Q2_FULL_SOURCE_TRAINER.md).
6. [Data/exposure audit](UPMIX_RELEASE_DATA_AUDIT.md),
   [machine audit](UPMIX_RELEASE_DATA_AUDIT.json),
   [old attempt inventory](UPMIX_NAS_ATTEMPT_INVENTORY.md),
   [machine inventory](UPMIX_NAS_ATTEMPT_INVENTORY.json).

**Important stale-status trap:** master/improvement/execution documents contain
04:45 UTC entries saying the new pair is waiting for baseline DEV. That was true
then, not now. Baseline DEV finished at 05:36:18 UTC; the new pair is training.
Older documents likewise contain historical “not fitted”, “running” and “NAS idle”
statements. Match timestamps and exact owner/artifact identities. Live files are
authoritative for liveness, sealed reports for results. Do not erase failed history.
The product goal tool was paused at handoff; the independently detached NAS jobs
were nevertheless verified running. These are separate states.

## 3. Current live work — do not duplicate or reset

At **12:37:09 UTC**, controller and both workers were checked against Linux
PID **and start ticks** and were alive, with phase `training` and no reported failure:

| Item | Owner beneath R | Snapshot |
|---|---|---|
| Controller | `q2-objective-pair-a` | PID162682 / birth47426499 |
| Control | `q2-carrier-continued-control-a` | 13,238 /24,288 updates; PID163722 / birth47776173 |
| Candidate | `q2-carrier-continued-candidate-a` | 13,237 /24,288 updates; PID163723 / birth47776174 |
| Future DEV control | `q2-carrier-dev-control-a` | Not created yet |
| Future DEV candidate | `q2-carrier-dev-candidate-a` | Not created yet |

About **54.5% of this additional pass**, not 54.5% of product completion.
Both latest pointers referred to `checkpoint-00013184.pt`.
Disk free7,544,171,552,768B; available RAM4,715,671,552B.
Read fresh status before estimating completion; timestamps/PIDs above are not live guarantees.

Exact experiment plan: [local pair-plan.json](../output/q2-objective-pair-stage-a/pair-plan.json).
Its NAS copy is `R/q2-objective-pair-stage-a/pair-plan.json`.

```text
Plan SHA256:
cb1a05a8be3a7acadb60334117905e48f6bbacc8f82fcd40f792af95785406c8
Stage manifest SHA256:
1ad180eeb9ea7c055a2598ba65de77b8ed0b50c1eacd4e9763d6a08887664e8c
Control plan SHA256:
4b5d478d10161df0779af569657660d79dd270d5311e86015d29527f34b11f63
Candidate plan SHA256:
6e5348f752d689bb1919ff2d30558bb8ed5347dd2dd1bef1e3df0ef129e41dac
```

Both arms inherit the **same final predicted model, Adam and RNG**. They reset
song state at the epoch boundary. The only planned training difference is actual
carrier-supervision coefficient **0 versus0.1**. Both use predicted guidance;
the new control is NOT the old separately trained disabled-guidance model.
All model parameters remain trainable, including the separator/decoder.

Coefficient0.1 was selected before the comparison by a TRAIN-only loss/gradient
scale calibration, not by DEV tuning. Calibration report:
`R/q2-carrier-coefficient-a/report.json`, SHA
`614aee726e7cc27df44ada163752a00377a1fad3d5088e7f73cd88195bd342a0`.
Local verified bodies: `output/q2-carrier-coefficient-stage-a/remote-proof/`.

The first32 real updates in each arm completed technical qualification. The same
owners resumed to **one additional complete epoch:24,290 forward jobs /24,288
optimizer updates**. Do not confuse the job cursor with optimizer count.
After both finish: five fixed TRAIN curve bins, all19 DEV songs per arm, then
`R/q2-objective-pair-a/paired-summary.json`. These five TRAIN bins alone do not
prove convergence or supply five independent checkpoint DEV evaluations.
Final scheduled checkpoints only; no cherry-picking, automatic winner, extra epoch or retry.

Bounds:16h cumulative TRAIN/arm,12h DEV/arm,120,000s controller overall;
100GiB free disk and2GiB available RAM reserves;3GiB top-PID RSS/worker;
4GiB TRAIN output/arm and1GiB DEV output/arm. Current frozen workers do not
spawn subprocesses; the RSS accounting is not a universal process-tree proof.
Checkpoints every64jobs. NAS is4-core Ryzen3 2300X, Torch2.10.0+cpu, two threads
per worker; do not add a third expensive job casually. Keep large data on NAS.

The controller recognizes `R/q2-objective-pair-a/STOP` for its owned work.
Do not create it during a review or kill historical numeric PIDs. If a real stop
is requested/needed, inspect the controller's exact cooperative-stop and
PID/birth cleanup contract first. A resumed trainer may spend minutes rehashing
original source bodies before status advances; a stale paused status alone is
not evidence that its live resume worker is stuck.

## 4. What has actually been learned so far

The **original** predicted-versus-disabled full pair completed one epoch each:
81 TRAIN songs /25 artists /813 original sources /4.493575h,24,288updates each.
Both then finished all19 historical DEV songs with no evaluator execution errors
at05:36:18 UTC. This establishes finite exposure and diagnostics, not convergence
or release acceptance.

NAS original owners:

```text
R/q2-full-source-direct-predicted-a
R/q2-full-source-direct-disabled-a
R/q2-full-source-dev-predicted-a
R/q2-full-source-dev-disabled-a
R/q2-full-posttrain-a/paired-summary.json
```

Original paired summary SHA (rechecked12:37UTC):
`315c42acc460ea30cc5bf7604571c19ce7d984d5aad51d28cce8d50ce3ad189a`.
Original predicted parent:
`R/q2-full-source-direct-predicted-a/checkpoints/checkpoint-00024290.pt`, SHA
`726bf73abffad465ad2a4bce93c9f96d4f16c783591761d1fc29465ae8a74dc4`.
Do not overwrite these or the old disabled checkpoint.

Descriptive results on **reused DEV**,19songs/8artist clusters/206eligible
whole-song family-channel cells, equal-song medians:

- Predicted vs disabled: **+0.451dB** source-error recovery gain; artist-cluster
  bootstrap95% interval approximately +0.156…+0.605dB.
- Predicted vs TRAIN-fitted raw scalar: **−0.099dB**, interval−0.232…+0.046.
- Predicted vs source-relative TRAIN scalar: **+0.070dB**, interval−0.037…+0.167.
- Family gains vs raw scalar: drums+1.895dB; high percussion+0.560;
  lead vocal+0.216; backing vocal**−0.464**; guitar−0.459; bass−0.637;
  keys**−2.645dB**, with worst song-median−10.992dB.
- Backing vocal had11eligible songs and all song-median gains vs raw scalar
  were negative. `other_plucked` and `other_vocal` had no eligible cells: uncovered,
  not successful. Inspect full per-song/channel/quiet distributions, not only this summary.

Therefore guidance modestly helps against the trained disabled arm, but the
original model does **not** clearly outperform simple scalar controls overall.
The problem is **not only backing vocal**. These scores are source-recovery
measurements, not listening percentages or proof of damage to the independent dry front.

Fixed four-TRAIN final-checkpoint diagnosis:
[report](../output/q2-final-train-hint-audit-readback-c/remote-proof/result/report.json),
SHA `55d304c399d14eecf6754c9fe9118019e5a809780cfe68683702850ee0dce2fe`.
All three active backing examples had PCM NMSE>1 (worse than mute's target error).
True source-local hints helped some recovery but worsened absent/quiet heads by
disturbing learned cancellation between carrier/extraction/refinement terms.
That motivates the current **joint** supervised-carrier experiment, not a claim
that better detector loss must solve the problem.

If hints improve but PCM/quiet leakage/useful transfer do not, redesign decoder
coupling with a controlled comparison; do not blindly add epochs or tune only the detector.

## 5. Model, training/data, and unresolved role supervision

Current model is **Q2 direct_power**, not the Q1 pilot and not an installed baseline.
Shared original L/R spectral analysis → source-local recurrent guidance →
full-resolution original-conditioned separator/restorer → separate role/routing
policy → cabin-aware safety/rendering.

Reference geometry:48kHz,FFT1024/hop256,513complex bins,32guidance bands,
width24/GRU48,12aggregate identity heads. Causal context covers31frames;
four-frame held controls. Approximately111,132parameters and485.1M dense MAC/s
in the documented reference graph, **excluding** FFT/features/synthesis/safety.
This is not an actual ARM performance measurement.

Heads: lead_vocal, backing_vocal, other_vocal, bass, drums, high_percussion,
percussion, guitar, keys, strings_winds, other_plucked, other_unknown.
These are dataset identities, **not twelve rear-approved objects**. Two guitars
in the same aggregate head may play opposite musical roles.

Core code under `tools/stereo-upmix-render/`:

```text
q2_local_guidance_model.py       q2_power_guidance_model.py
q2_source_data.py                q2_training_core.py
q2_stream_training.py            q2_carrier_supervision.py
train_q2_full_source.py          train_q2_continue.py
calibrate_q2_carrier_coefficient.py
evaluate_q2_full_source.py       evaluate_q2_continued_source.py
q2_full_song_score.py            summarize_q2_full_source.py
summarize_q2_objective_ablation.py
run_q2_objective_pair.py         q2_job_supervisor.py
```

Every running study uses its frozen NAS source snapshot, not mutable local files.
All source pins are in the pair plan/stage manifest. New code requires a new
versioned owner and qualification; do not edit the running snapshot.

Data authorities:

```text
B/external/moisesdb/moisesdb_v0.1/   # original stems, preserve unchanged
B/project/build/reference-audio/v16-moises-catalog-sha-a/manifest.json
  SHA ecac5c4d4477167456da372fb5ff095e45a8a76652cbe9d69e0922e46f44510d
R/q2-source-data-a/index.json
  SHA f0f8f562d21ea26af9908518045423835ad109173ada2abdb906d5e0fe067ccf
R/q2-train-calibration-a/study/calibration.json
  SHA 8fc7465c6da9268a6f108c04c19ad5f21191543c559d02e8b6b927060147bca6
output/q2-train-calibration-a/       # small local metadata/proof copy, not audio
output/q2-source-fit-input-a/proofs/index.json  # verified local index copy
output/q2-source-body-audit-a/report.json       # 991 TRAIN+DEV original-WAV audit
  SHA 547b50818fb6b2512f459dbb64e265a5c9dc77bc3e3059b57463c3d226302ef0
docs-obsidian/Sources/UPMIX_RELEASE_DATA_Q2_COVERAGE.json
```

Catalog-wide historical split:181TRAIN/40DEV/19reserved songs. The current
admitted no-bleed/identity-mapped subset is81TRAIN/19DEV, not the whole catalog.
Original split seed: `upmix-v16-moises-artist-split-20260911-v1`.
The historical `B/project/build/reference-audio/v16-moises-source-full-train-a`
cache has572eight-second windows, not today's full81-song training corpus.
Likewise `output/q2-source-fit-input-a/manifest.json` is a four-TRAIN-excerpt
qualification bundle, not full training. Preserve old plans containing SMB paths;
use the reader's declared relocation rather than rewriting sealed plans/checkpoints.
Native originals44.1kHz are read with frozen unit-gain160:147 SRC to48kHz;
full declared maximum-tail timelines, no independent source normalization.
Source hashes/paths and known-label masks come from the index and proof files.
Missing/ambiguous targets are UNKNOWN, not clean negatives. Metadata no-bleed
declarations are not equivalent to fresh perceptual purity approval.
The heldout partition is only a **candidate** for untouched confirmation until
the full historical exposure/duplicate/alternate-version audit is reconciled.
Do not decode heldout to mine examples, choose architecture or prepare development demos.

Read [role targets](UPMIX_Q2_ROLE_TARGETS.md),
[active contexts](UPMIX_Q2_ROLE_ACTIVE_CONTEXTS.md),
[active packets v2](UPMIX_Q2_ACTIVE_PACKETS_V2.md), and
[packet readiness](../output/q2-role-active-packet-readiness-a/READY.md).
Existing quiet review packets cover five historical TRAIN songs/59instances;
four additional active contexts are selected/metadata-ready, **not generated or reviewed**.
Their32distinct sources/41context observations do not contain real role judgments.
The existing v1 review server and role-target compiler do **not** accept v2 active
packets without separately qualified compatibility work. Do not merely relabel schemas.
No actual role listener votes or learned rear-safe instance policy are established.

## 6. Musical and Virtual Cabin constraints

- Preserve lead/foreground vocals, wordless vocal hooks, principal guitars,
  dominant synth/growl hooks in front. Instrument class alone grants no rear permission.
- Rear scene should contain justified backing, percussion/hats/shakers, textures,
  secondary guitars/synths, preserving attacks, overtones, tails and phrasing.
- Dense electronic/dubstep, sparse acoustic, mono/narrow/hard-pan, overlapping
  sources, quiet/silence and rapid role transitions all matter. Coverage is incomplete.
- No fake “wow” through louder rear, full-mix copying, added bass, reverb or phase tricks.
- Restoration must stay grounded in original stereo and supervised sources, not
  invent unrelated notes. A reconstructed rear signal is not automatically safe
  to subtract from the front. Current new chain preserves an independent dry front.
- Actual transport is six slots:FL,FR,C,LFE,RL,RR (rear pair also calledSL/SR).
  Rear-door lower drivers and tweeters share feeds; they are not independent PCM destinations.
- 3D OFF is the operating assumption, **not proven factory DSP bypass**.
  Sound Focus All and minimum Surround are user-reported assumptions, not live
  verified control readback. Document/measure them for car qualification.
- Zero C/LFE transport samples do not physically mute those speakers. Factory
  matrix mixing remains; preserved dry PCM alone does not prove seat/front-bus fidelity.
- Keep coherent common SoundCheck gain and linked protection, no independent
  stem normalization or unvalidated LFE/bass boost. Prior excessive bass was a real complaint.

Relevant evidence (electrical/static models are **not** measured seat acoustics):

- [Virtual Cabin evidence](SOUNDAMP_9VS_VIRTUAL_CABIN_EVIDENCE.md).
- [3D OFF/VISA audit](SHARC_VISA_3D_OFF_AUDIT.md).
- [Input/output matrix](SHARC_INPUT_OUTPUT_MATRIX.md).
- [Output binding](SHARC_OUTPUT_BINDING_AUDIT.md).
- [Static readiness and SoundCheck](UPMIX_STATIC_READINESS_AND_SOUNDCHECK.md).
- [Routing and speaker workbook review](UPMIX_CABIN_ROUTING_REVIEW.md).
- [Cabin-first redesign](UPMIX_CABIN_FIRST_REDESIGN.md).
- [Speaker parts research](SOUNDAMP_9VS_SPEAKER_PARTS_RESEARCH.md).
- [Q2 common-output safety](UPMIX_Q2_COMMON_OUTPUT.md).

Electrical-proxy implementation: `tools/soundamp-virtual-cabin/README.md`,
`soundamp_virtual_cabin.py`, `digital_stages.py` in that directory.
Configuration `tools/soundamp-virtual-cabin/audi_a5_f5_9vs_3d_off.json`, SHA
`36e4185a52a192316602020f566e117797840a98efa3b2e0d6b89f0717bd076a`.
Historical reference owners: `R/c1-cabin-reference-b`, `R/c2-scene-reference-a`;
contracts [C1](UPMIX_C1_CABIN_REFERENCE.md) and [C2](UPMIX_C2_SCENE_REFERENCE.md).

Additional user-supplied local evidence roots:

```text
/Users/luka/Documents/Audi
/Users/luka/Desktop/AUDI_2/Firmwares/SoundAmp
/Users/luka/Desktop/AUDI_2/Tools/jxe2jar/out/MU1316-combined-vf
/Users/luka/Desktop/AUDI_2/Patches/mhi2-carplay/docs/B9 S4 B&O speaker data.xlsx
```

The S4 workbook is useful supporting evidence, not calibrated S5 Sportback seat
response or proof of its subwoofer enclosure response. Use appropriate skills if
opening spreadsheets, PDFs, reverse-engineering binaries or Memora.

## 7. Native/QNX work: what passes and what is missing

Original final trained weights passed authored reference/native parity on the
host and **ARM32/QNX emulator**. This is not real loaded Krait hardware evidence.

```text
output/q2-final-checkpoint-host-a/report.json
  SHA 204d9302cd94af77388dfa3480ab181c9349a863f958bff944e8034110f363e4
output/q2-final-checkpoint-qnx-a/report.json
  SHA d5a15ee578687454aff2aef3b9eca33a763bc9b450678cf045493fc3821beb7f
```

Eight named guest checks exited0; float max errors about2.09e-8/1.86e-8,
S16difference1/0LSB. These bind the **old original final weights**, not future
continued checkpoints. New selected weights require fresh export/parity qualification.

New standalone adapter:

```text
tools/stereo-upmix-render/q2_s16_session_adapter.c
tools/stereo-upmix-render/q2_s16_session_adapter.h
tools/stereo-upmix-render/test_q2_s16_session_adapter.py
output/q2-s16-session-adapter-host-a/report.json
  SHA 0c763e4694472294aef378029043d77d01276fbbac8da1e966b91e5756ecc6e7
```

[Adapter contract](UPMIX_Q2_S16_SESSION_ADAPTER.md):14authored host tests passed,
including optimized seeded wet comparisons; standalone ASan/UBSan control is dry/no-model.
Decoded48k stereo S16 → original float → existing chain → six-slot S16LE.
Caller-owned capacity reservation precedes model/metadata changes;1024sample
delayed packet tags include explicit validity; reset/carry/EOF and backpressure
are tested in this isolated adapter. No adapter allocation/I/O/locks.
Host state~1.68MB/workspace~9.45MB are not complete target memory qualification.

**Production hook is not wired.** Existing ring push can drop/overwrite and
stamps current packet tags. Need reserve-before-DSP, delayed-tag/validity publish,
correct partial sink retries, FLUSH/reset/seek semantics, real EOF, and click-free
old/new-path handoff. Do not apply legacy gain/limiter twice. Preserve existing
native APAC5.1 handling. Integration seam is `hook/audio/audio_hook.c`,
`spa103_render_frame`, after decoded-S16 trim and before ring publication, not raw AAC.

Read [runtime audit](UPMIX_RELEASE_RUNTIME_AUDIT.md),
[final-checkpoint native](UPMIX_Q2_FINAL_CHECKPOINT_NATIVE.md),
[native-chain runbook](UPMIX_Q2_NATIVE_CHAIN_RUNBOOK.md),
[session chain](UPMIX_Q2_SESSION_CHAIN.md),
[gain-session runbook](UPMIX_Q2_GAIN_SESSION_RUNBOOK.md),
[hook integration](UPMIX_Q2_HOOK_INTEGRATION.md), and
[audio-session policy](UPMIX_Q2_AUDIO_SESSION_POLICY.md).
Build/run commands and exact dependencies live in those runbooks and sealed manifests.
Do not guess a QNX toolchain path or ABI from the host compiler.

## 8. Acceptance — no test gaming

Use the exact frozen v2 specification and machine protocol, not this abbreviated list:
the local sealed owner is `output/upmix-release-protocol-v2/`; retain the old v1
owner unchanged and do not reinterpret its old results as v2 passes.

- Recovery median≥+3dB over best declared matched deployable control,
  bootstrap95% lower≥0; worst active song NMSE≤0.8.
- Protected solo rear≤−30dB; mixed conditional responseP95≤−24dB,worst≤−18dB.
- Support transferp10≥−6dB,p90≤+1dB;≥80%eligible supporting intervals retained.
- Front coherent gain error≤0.5dB; phase errorP95≤5°.
- Attack50ms/sustain100ms envelope errorP95≤3dB; bass20–120Hz change≤1dB.
- Native max error≤5e-5,S16≤2LSB; chunk/reload/reset/continuous parity.
- Actual APQ8064/Krait/QNX **complete path**,≤40ms latency,≤32MiB,
  P99≤80%callback period,max≤period,zero misses for≥7200s with competing load.
- Genuine randomized blind level-matched listening:≥24distinct song-cluster
  verdicts, real listener judgments, Wilson lower>0.5. No invented listening.
- Frozen song/artist/source disjointness, exposure and provenance audit,
  coverage/unknown/quiet controls; no final-TEST tuning or easy-case selection.

Compare fairly with stereo-preserving/simple DSP, MUTE, both TRAIN-fitted scalar
controls, relevant HSSR/P1/R1/Q1, guided-vs-disabled models, stronger offline
references where available and clearly labeled. Current objective-only pair is
one comparison, not the whole acceptance suite. Reused DEV is not final TEST.
Report per-song/family/role/genre distributions and worst cases; uncovered cells
remain UNVERIFIED. Valid test bugs require transparent versioned all-affected reruns.

Current status: **1. Research candidate**.
Not yet **2. Verified installation-ready candidate for controlled car evaluation**.
Not **3. Acoustically validated in-car release**.
Target hardware and listening gates are required for status2; cabin adds status3.

## 9. Recommended first actions / takeover order

1. Read the essential documents and inspect dirty worktree; do not revert unrelated
   audio-hook/teacher/UI/maneuver changes. Many new evidence/code files are not a
   committed baseline; inspect actual files/hashes, not only `git log`.
2. Run the safe live-status read. Do not launch, stop, duplicate or change the
   active pair just because a document contains an old pending/paused timestamp.
3. Independently inspect the original19DEV summary and worst song/family cases,
   including quiet output, coherent useful transfer and both scalar controls.
4. Let the bounded pair finish, verify checkpoint/report hashes and all scheduled
   exposure, then assess both full19DEV results. Inspect actual PCM behavior,
   not just auxiliary loss. Collect immutable small evidence locally; leave large data on NAS.
5. If carrier supervision helps only hints, propose and test a justified decoder
   redesign with a matched baseline/budget. If it helps recovery, still check
   interference, role safety, naturalness, convergence and generalization.
6. Parallel unblocked work: qualify v2 role-packet compatibility and prepare
   actual role-review material; complete production adapter integration in an
   isolated new branch/study without mutating frozen components or the active experiment.
7. Obtain real role/listener judgments and untouched confirmation data; do not
   fabricate votes or substitute class labels for role truth. Prepare concrete
   target-hardware/cabin procedures and ask only for missing physical access/action.
8. Release only after all mandatory gates: exact trained config/weights, hashes,
   reproducible training/export/build, dependency/licenses, QNX binaries,
   preflight/backup/install/rollback/recovery, machine-readable results,
   level-matched listening material and explicit remaining limitations.

Give Luka concise Russian updates: what completed, what failed, what it means,
and what's actually running next. He wants progress on sound quality, not more
paperwork, dozens of uncontrolled training variants, or repeated requests for
routine decisions. Be candid if the current architecture is fundamentally weak.

## 10. Additional historical source links (not freshly revalidated here)

- [Research review and primary-paper links](UPMIX_RESEARCH_2026_09.md).
- [AlbumDB paper audit](UPMIX_ALBUMDB_PAPER_AUDIT.md).
- [Original Q1 cycle](UPMIX_Q1_GUIDED_CYCLE.md) — unapproved pilot, global activity
  hints; not source-local guidance or a production baseline.
- [Retrospective](UPMIX_RETROSPECTIVE_AND_STEREO71.md) — historical7.1 discussion;
  current target remains5.1.
- [Zenodo DOI supplied by Luka](https://doi.org/10.5281/zenodo.19683000).
- [Other supplied Zenodo record](https://zenodo.org/records/18333249).
- [Amvizo showcase](https://amvizo.com/showcase/).
- [HiRez Emphasis4.0](http://www.hirez.one/streaming/odyssey/2-emphasis-4.0.flac),
  [5.1](http://www.hirez.one/streaming/odyssey/2-emphasis-5.1.flac),
  [7.1](http://www.hirez.one/streaming/odyssey/2-emphasis-7.1.flac),
  [Singularity](https://www.hirez.one/streaming/the-amnis-initiative/01-Singularity.flac).
- [VIN-based parts catalog supplied by Luka](https://superetka.com/etka/?lang=EN&marke=AU&market=USA&model=3720&year=3731&vin=WAUC4CF55JA099137).

Historical local attachments:
`/Users/luka/Downloads/19683001.json` and
`/Users/luka/Downloads/2026_McK_Dataset_AES_Europe_2026_2_.pdf`.
Check availability before using; do not redownload an existing corpus or reuse
an expired signed MoisesDB URL. Public authored multichannel masters are distinct
from source-isolation truth and foreground/support musical-role truth.
Availability or private use is not a substitute for dependency/license records.

Memora, if available: existing concise handoff is memory **1120**, section Audi /
Virtual Cabin upmix release. Follow the user's selective lookup/write limits;
this file and live evidence should avoid broad history retrieval. Do not store credentials.
