# V15 full-corpus audit: source clipping correction

2026-09-09. This supersedes the V14 PCM16 corpus for new teacher generation.
No new student training, checkpoint promotion or car writes.

## Verified preparation defect

V14's full-song preparation decoded to PCM16 without headroom. Independently
decoding original files to float showed samples outside the integer range:

| Original song | Float peak | Peak dBFS | Samples outside ±1 |
|---|---:|---:|---:|
| Archers | 1.41943 | +3.04 | 1.16885% |
| Cities In Dust | 1.27274 | +2.09 | 0.01872% |
| Us V Them | 1.35651 | +2.65 | 0.04786% |

Thus exact reconstruction of the PCM16 decode did not establish preservation of
the floating-point decoder output. The integer conversion was clipping peaks.
The FLOAT rebuild preserves values above ±1; these are not treated as invalid
float audio. Native teachers apply one common inference gain and restore it into
FLOAT outputs. Listening previews retain their separate shared headroom gain.

Across the rebuilt corpus, 44/50 songs and 456/587 cores contain samples above
integer full scale. The maximum decoded float amplitude is 1.68043.
This does NOT quantify pre-existing mastering distortion or promise its removal;
the fix avoids additional clipping introduced by our preprocessing.

## Completed source checks

`build/reference-audio/v15-full-float-source/manifest.json`

- All 50 songs, all 587 non-overlapping cores, 194.6726 unique minutes.
- 411 train, 100 validation, 76 heldout cores; source boundaries unchanged.
- All source/core hashes verified, 48 kHz stereo FLOAT, finite samples and exact lengths.
- Concatenated core sample bytes match an independent full-original FLOAT decode.
- No exact decoded-audio duplicates; no source-hash partition leakage.
- Coarse spectral/time similarity checked across partitions for review only; this
  is not a comprehensive near-duplicate detector. No semantic correctness claim.

Complete report: `build/reference-audio/v15-full-float-source/integrity-audit.json`.
The earlier V14 fingerprint attempt contained empty spectral bands; its v2 report
fixed that numerical check. Neither legacy integer report should admit V14 to new training.

## Active all-core workflow

Owner script: `tools/stereo-upmix-render/complete_float_dataset.py`.
It owns native base separation and the bounded complete-audit runner, captures
logs to disk and writes terminal success/failure state. The initial run is ongoing;
source verification is complete, full teacher/content verification is not yet complete.

For EVERY one of the 587 cores:

1. Native RoFormer all-vocal/instrumental pair with source/model/output fingerprints.
2. Demucs6 → DrumSep6; residuals retained in the diagnostic record.
3. Karaoke on all-vocals AND independently on the original mix.
4. AST and CLAP content analysis of every stem and original-minus-stem controls,
   covering the entire core with declared 10-second windows/short-tail treatment.
5. Shared-level and diagnostic-level views; uncalibrated rankings are not converted
   into purity probabilities or automatic training approval.
6. Per-stem/window triage: near-silence, weak residual, short context, supported
   candidate or unresolved content/role. Heldout remains evaluation-only.

The runner uses at most five clips per batch. **Resumed preservation policy
(2026-09-09): `--preserve-all-audio` retains every new batch directory, including
incomplete stages.** Every newly audited clip has its 17 specialist/direct-input
WAVs exposed in `retained/` using hard links to the preserved batch files, avoiding
a second physical copy. No automatic deletion is performed under this policy.
The plan refuses a subsequent downgrade to the old scratch-cleanup policy.

Before the user requested preservation, the initial run retained specialist WAVs
only for predefined middle clips. At the pause, 55 cores were audited, 12 had
retained specialist audio and **43 had reports but no retained specialist WAVs**.
Those historical WAVs are not retroactively restored by resume; reconstruct them
from sources/models/recipes if needed. Their existing reports are unchanged.
All source audio, base stems, teacher provenance and diagnostics remain preserved.
No pre-existing user audio, old teacher files or human review notes were deleted.

The owner now checks disk availability every 5 seconds. At <=22 GiB it sends
SIGSTOP to both owned worker process groups; after free space reaches >=27 GiB
it sends SIGCONT. It never frees space automatically. Lower-level preflight
checks retain their 15 GiB reserve plus stage working headroom. This polling
guard is not a filesystem quota: unrelated applications can still consume space
between checks. State is written to `v15-workflow/disk-status.json` and the owner
status; the per-stage audit status alone does not indicate a disk pause.

AST now supports MPS automatically; CLAP stays CPU. On a four-item real HH test,
AST's MPS/CPU maximum sigmoid-score difference was 6.2e-7; warmed MPS batches took
0.36–0.43 s versus 1.74 s CPU in that test. Actual devices are recorded per result.
Use `--ast-device cpu` to reproduce earlier CPU diagnostic runs.

## Status, logs and final report

- Workflow: `build/reference-audio/v15-workflow/status.json`
- Disk guard: `build/reference-audio/v15-workflow/disk-status.json`
- Logs: `build/reference-audio/v15-workflow/{integrity,teacher,audit}.log`
- Per-stage progress: `build/reference-audio/v15-complete-audit/status.json`
- Per-clip evidence: `build/reference-audio/v15-complete-audit/clips/<id>.json`
- Updating/final report: `build/reference-audio/v15-complete-audit/dataset-report.{json,md}`
- Retained audition stems: `build/reference-audio/v15-complete-audit/retained/<id>/`
- Local progress endpoint: http://127.0.0.1:8766/api/pipeline
- Listening room: http://localhost:5178/ ; new entries have a FLOAT title suffix.

`complete-needs-semantic-review` means every planned diagnostic ran, NOT that all
teachers are correct. `failed-needs-inspection` means inspect the named log and
resume; never label a partial run complete. The final report separates numerical
integrity from content/routing uncertainty. No automatic student training follows.

## Restart only after confirming no existing workflow owns the lock

```sh
build/hssr-teacher-env/bin/python tools/stereo-upmix-render/complete_float_dataset.py --resume --preserve-all-audio
```

The base separator validates completed pairs before reusing them. A partially
written/stale pair is refused for inspection, not silently overwritten. Interrupted
scratch may remain for inspection; never recursively delete a workspace or old corpus.

V14 base/specialist/50-clip queues were stopped and marked superseded. Their data
remain comparison-only. Prior V13/V14 semantic findings describe those historical
PCM16 inputs and must not be automatically transferred to new FLOAT revisions.
