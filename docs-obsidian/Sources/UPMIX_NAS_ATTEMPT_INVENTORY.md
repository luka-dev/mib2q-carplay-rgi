# NAS: inventory of prior upmix attempts

Snapshot: **2026-09-15 02:26–02:35 UTC**, NAS `100.85.38.96`.
Private scope only:

```text
B = /mnt/shared/users/luka/Audi/upmix-training-2026-09-11
R = B/project/build/reference-audio
```

This is a read-only inventory of directory sizes, bounded metadata, statuses,
reports, checkpoint filenames, and three TRAIN NPY headers. No training started,
no audio decoded, no DEV/heldout PCM opened, no source files modified or deleted.
Prior verification reports were inspected; the entire audio corpus was **not
rehashed again**. A status of “complete” means that particular job finished,
not that a model is accepted for the car.

[Machine-readable inventory](UPMIX_NAS_ATTEMPT_INVENTORY.json) includes bounded
runtime counts, the actual TRAIN waveform header checks, and all 181 frozen
Moises TRAIN song IDs. Historical PID fields were not used to decide job completion.

[TRAIN waveform reuse index](UPMIX_NAS_MOISES_TRAIN_REUSE_INDEX.json) provides
all 572 relative cache paths with their expected historical hashes and crop
origins, plus the frozen 181-song/28-artist mapping. Fresh checks verified every
referenced file exists and has the expected byte size, all rows remain TRAIN,
and the catalog/plan/report/verification bindings agree. It explicitly separates
the three fresh NPY header checks from the historical whole-cache SHA verification;
the 18 GiB PCM payloads were not reread. This is a staging index, not automatic
semantic admission or permission to move source material into the rear target.

## Storage

Allocated sizes from `du -k`; GiB means 2³⁰ bytes. These are current disk usage,
not claims about independent unique music or additional training exposure.

| Scope | GiB |
| --- | ---: |
| Entire private project B | 349.17 |
| B/external | 195.07 |
| B/project | 151.78 |
| B/runtime | 2.27 |
| B/migration | 0.04 |
| Free on /mnt/shared | 7041.02 |

The expensive assets are audio and historical teacher/audit outputs, not the
new small neural checkpoints. The depth-limited runtime scan counted **1072
checkpoint file entries, 622.14 MiB**, and 188 report/result/comparison files.
This includes copies, qualification/replay snapshots and intermediate steps;
it is not 1072 independent trained models. It excludes checkpoint files under
R and directories beyond the bounded scan. There is no current space pressure
requiring deletion.

## What is already available and worth reusing

All paths below are relative to B unless prefixed with R.

| Existing asset | Allocated size | Verified metadata / appropriate reuse |
| --- | ---: | --- |
| external/moisesdb | 138.73 GiB | Extracted MoisesDB originals: 2825 files, 148,948,560,641 bytes. Existing extraction report says all CRC32 and JSON parsing passed; original archive already deleted in the earlier authorized task. Reuse originals if fresh controlled crops are needed. |
| R/v16-moises-catalog-sha-a/manifest.json | 2.8 MiB manifest | Frozen 240 songs / 2585 tracks, split 181 TRAIN / 40 validation / 19 heldout; 28 / 9 / 6 artists. Existing catalog reports full source SHA coverage and no cross-partition exact duplicate song groups. Preserve this split. |
| R/v16-moises-source-full-train-a | 18.02 GiB | **Already prepared full-waveform TRAIN cache:** 572 windows, all 181 TRAIN songs, 28 artists. Useful for diverse controlled foreground/source-addition training after explicit new admission, not native 5.1 rear targets. |
| external/p1-protected-sources-48k-a | 657 MiB | Completed nine prepared source files plus manifest. Reuse for existing controlled front-protection probes with original provenance and restrictions. |
| external/p1-native51-48k-a | 1.24 GiB | Four audits, two imported 48 kHz native multichannel tracks. Import complete; `training_ready=false`, `role_admission=false`. Research inputs, not automatically approved rear-role labels. |
| external/albumdb-paper51-48k-a | 1.93 GiB | Nine completed immersive-to-5.1 research conversions. Independent stereo pairing false; role admission false. Independent admission report confirms normal role loader rejected it, research loader verified nine rows. Reuse via the explicit research policy only. |
| external/albumdb-stems-19683001-b | 29.50 GiB | Successful extraction of four archives / 822 WAVs, originals retained. `training_ready=false`; raw source assets, not approved model labels. |
| external/albumdb-19683001-masters-a | 4.38 GiB | Downloaded master assets; keep distinct from derived research targets. |
| external/p1-micro-fixture-a; p1-front-transfer-probes-a | 3.77 MiB; 13.17 MiB | Existing small regression fixtures. Do not relabel repeated evaluations as new independent training evidence. |

### The old Moises cache preserves actual waveform detail

The preparation plan states `full-train-source-float-v1`, 48,000 Hz, eight
seconds per window, common origin and no per-stem gain normalization.
All 572 report entries declare shape **[11, 2, 384000]**. Actual first, middle and
last file headers were independently read: **float32**, C order, same shape,
33,792,128 bytes each; only the 128-byte NPY headers were read.

These are stereo PCM waveforms for eleven source families, **not 32-band
features or already restricted spectra**:

`lead, backing, foundation, high_percussion, drum_body, guitar, pad, keys,
strings_winds, fx_other, unknown`.

They can feed the current full-bin model or fresh spectral analysis without
returning to the old 32-band representation. They are grouped experimental
source families; they do not establish which guitar/synth is foreground,
or that a backing-labelled vocal belongs behind the listener.

The old cache report is complete and technically `training_ready=true`;
its explicit `placement_approved=false` remains binding. The separate
verification report records all cached hashes checked and crop activity
recomputed; 6567 review-pending items remain unapproved. The fresh inventory
confirmed all 572 row partitions are TRAIN and their song set exactly equals
the 181 frozen TRAIN IDs.

The appropriate next expansion is controlled, diverse foreground/source
addition from those TRAIN songs, with artist/song-disjoint assessment and
explicit role constraints. **Do not present these source mixtures as native
5.1 master pairs or silently promote their rear-role labels.**

Bindings:

- Catalog SHA256: `ecac5c4d4477167456da372fb5ff095e45a8a76652cbe9d69e0922e46f44510d`; freshly matches the historical preparation plan.
- Cache plan SHA256: `d75d86b87669596b96402b7cd5a9cc1b683159dd062a2375737b278ed88db0ed`.
- Cache report SHA256: `287f274a62f1c2ff1e8d8aee6be91ee5ec9789cdd7a2121b39d65a4206a463d5`.
- Cache verification SHA256: `c1036b097c216eaa6bcc75a3cc4db544ad2b08a092f49df73bd1efc72c5b0aa3`.

Historical plans still contain `/Volumes/private/...` or Mac path hints.
Use an explicit, verified NAS root relocation for readers; do not rewrite
the sealed historical plans, manifests or checkpoints in place.

## Old teachers and datasets: useful evidence, not clean labels

`R/v15-complete-audit` occupies **78.96 GiB**. Its status/report are
`complete-needs-semantic-review`: 587/587 clips, 50/50 songs, approximately
194.67 unique core minutes, technical audit complete. It explicitly says
`semantic_correctness_guaranteed=false` and `training_approved=false`.
There are unresolved vocal/instrument identities, low-level residuals and
gain-sensitive interpretations. Older 43 audited clips are marked as lacking
retained audit audio; do not infer complete retention from the count alone.

Related retained assets:

- `R/v15-full-float-source`: 4.95 GiB; 587 FLOAT source clips, 50 tracks.
- `R/v15-full-float-teacher`: 9.90 GiB; 587 RoFormer teacher records.
- `project/build/hssr-cabin-v13-a`: 6.22 GiB, mostly its cache.
- Older v12/v13/v14 teacher/render/audit folders remain under R.
- `project/build/hssr-teacher-models` 1.90 GiB,
  `hssr-audio-audit-models` 0.90 GiB,
  `hssr-teacher-env` 1.08 GiB.

Preserve these for provenance, semantic review and comparisons. Do not reuse
the entire teacher output as approved stereo-to-5.1 supervision. Existing
validation/heldout folders were inventoried by metadata only and remain outside
the new optimizer data.

## Completed old model attempts

| Family | Exact retained paths / state | Reuse |
| --- | --- | --- |
| HSSR v13 | `project/build/hssr-cabin-v13-a/{best_so_far.pt,hssr_development.pt,hssr_model.bin,normalization.npz}`; pipeline says pilot complete, not selected/installed. | Historical baseline only; not a P1-compatible warm start. |
| HSSR v15 a/b/c | `project/build/hssr-v15-routing-{a,b,c}/{last.pt,candidate.bin,status.json,result.json}`; training complete, semantic labels unapproved, pending C/attribution/listening acceptance. | Keep old baseline/ablation evidence. v15a improved its old validation objective; b/c did not. That does not establish new P1 acceptance. |
| Source G8 | `R/v16-moises-source-g8-nas-full-a/report.json`, `step-{512,1024,2048,4096}.pt`; report complete, deployment false. | Prior source separation baseline and harness evidence. |
| Source G12 | `R/v16-moises-source-g12-nas-full-a/report.json`, `step-{512,1024,2048,4096}.pt`; report complete, deployment false. | Same distinction; no direct P1 weight compatibility claim. |
| Source G17 | `R/v16-moises-source-g17-budget-a/report.json`, `step-{6144,8192}.pt`; completed 4096 new updates from earlier checkpoint, placement/deployment false. | Preserve exact historical training and failure evidence. |
| H1 | `R/v17-source-h1-sequence-a/{smoke,pilot}`; pilot complete with g9-spatial-1e-4, h1-complex and h1-fidelity arms, each checkpoints64/128/256. | Finished research pilot, not validation or car acceptance. Smoke has three step4 checkpoints. |
| Earlier P1 | `runtime/p1-native51-pilot-a`, quiet/factorial/folded/front orchestrator trees, and micro objective/optimizer/length/frequency trees. | Completed paired diagnostics and replay qualifications. Several older arms reached5014 updates; update count is not independent content coverage. Preserve reports/code along with weights. |
| P1 geometry LR1e-4 | `runtime/p1-front-policy-orchestrator-a/comparison/geometry1024`; latest1024 and completed report. | Sealed baseline, no car approval. |
| P1 geometry LR3e-4 | `runtime/p1-front-rate-orchestrator-b/comparison/geometry1024`; latest1024 and completed report. | Current sealed native diagnostic baseline; musical qualification still failed. |
| P1 prefix pair | `runtime/p1-prefix-probe-owner-a/experiment`; both control/jitter complete64 additional updates; eight checkpoints16/32/48/64; independent postflight complete. | Conditioning experiment, not promoted model or ordinary v6 continuation artifact. |

The two geometry latest checkpoint bindings freshly read from `latest.json`:

- LR1e-4: `checkpoints/step-00001024.pt`, SHA256 `6c1e07af3a21a980f41e8fa458da2fb1186e901e75103ed1aac377510a12551b`.
- LR3e-4: `checkpoints/step-00001024.pt`, SHA256 `2b95062b8bdebdca1163783ba12cca7235fa3d982db052fe9e90dcf4eaf0b9a8`.

These hashes were read from the existing sealed bindings, not produced by
loading or executing any checkpoint. Most G-model artifacts reside under R,
while `runtime/source-*` contains worker reports/tools; an empty checkpoint
count in a runtime folder does not mean its completed model is lost.

The reusable Linux environment remains
`runtime/source-g8-linux-20260912/venv` (1.06 GiB for that runtime root).
New runners should use separate snapshots/output roots and preserve existing
qualification and model-schema contracts.

## Incomplete or failed leftovers identified

- `external/albumdb-stems-19683001-a` (1.58 GiB): incomplete after one archive,
  failed on an AppleDouble `._...wav` sidecar. The completed replacement is
  `...-b`; do not accidentally select the old partial extraction.
- `runtime/source-g8-supplement-1024-a`: failed with
  “Original main-core numerical evaluation changed”; replacement `...-b`
  has completed status and report.
- `runtime/p1-front-rate-orchestrator-a`: only an empty `worker.log`,
  no completed run/model/report; the actual completed rate run is `...-b`.
- Some source run `progress.json` files still contain
  `complete=false` while `status.json` and final `report.json` both confirm
  completion. Treat the final report plus matching status/plan as evidence,
  not an old progress snapshot or a stale PID.
- The v15 semantic-review and P1 musical-admission gaps are **unfinished
  acceptance**, not unfinished extraction or a training process to resume blindly.

No cleanup recommendation is needed for the next bounded preparation:
all prior calculations and original datasets remain intact, with roughly
7 TiB available.
