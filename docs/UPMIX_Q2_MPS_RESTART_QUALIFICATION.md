# Q2 backend qualification — 2026-09-16

This is a synthetic execution/restart test, not a music-quality experiment.
No native corpus or reserved TEST PCM was opened. No acceptance threshold changed.

After the manifest-only vocabulary integration fix, the frozen v2 model passed
13 model tests and the runner passed 7 CPU tests: 20/20 in 17.164 seconds.
The actual separate-process CPU interrupted/resumed trajectory was exact for
model parameters, Adam, retained TBPTT history/state, RNG and PCM metrics.
The synthetic bundle now matches real portable data: `classes` appears only in
the manifest; normalization rejects contradictory per-row vocabulary.

The exact MPS restart test had failed, so a separate retained control study ran
two uninterrupted processes A/B and a third process C interrupted after one
committed update. Each used the same authored four-song fixture, student profile,
seed, schedule and 12 updates for each of predicted/disabled/oracle-TRAIN arms.
It completed in 41.923 seconds; artifacts occupied approximately 68.1 MB.

Evidence: `output/q2-mps-restart-diagnostic.QhYvJi/report.json`, SHA256
`fc9137aca5753460a831d60410d22386880b14db5edd8bbb51be176dd9ce9b47`.
The owner preserves executable diagnostic, protocol, source copies, all three
studies/checkpoints and every unequal tensor/scalar comparison.

Two uninterrupted runs already differed: first logged difference was predicted
update 11 activity loss, 5.96046448e-8. Across all arms, maximum final parameter
difference was 2.98023224e-8; maximum Adam difference was 3.72529030e-9.
A/B final PCM-derived metric differences reached 1.3564e-9. Comparing C against
A or B produced the same parameter scale and PCM-metric differences up to
2.1763e-9. These are descriptive numbers, not newly admitted tolerances.
The report compares PCM-derived metrics, not independently retained PCM bytes.

Initial weights, initial PCM metrics, plans, schedule cursors and CPU/MPS RNG
were exact. Source originals/copies and previously committed checkpoints stayed
unchanged; the dataset was independently revalidated against its original SHA.
Different final state weight-binding hashes correctly reflect different weights.

Conclusion: process/backend nondeterminism exists even without interruption.
This does **not** isolate a resume-specific defect, nor prove that none exists.
MPS exact-restart qualification remains failed; no tolerance waiver, architecture
quality inference or vehicle approval follows. The qualified CPU backend is the
current exact-reference execution path. Investigating deterministic MPS kernels
requires a separate execution contract rather than silently changing this study.

Frozen model SHA: `f24391f779e6deb08611937c535b0f0831c69f550555265fd76d247c879a260f`.
Trainer SHA: `8b46f413f768a7bad2be90cfebcd8f8b9a2336021d2f09ca1c331d53b70d58ef`.
Runner-test SHA: `23582b32c5cd531639c5ce255eae9bdabf4fd48223fe8303b742f8b20154d232`.
