# H1: genre-agnostic implementation branch

2026-09-13 closure: all three 256-update arms completed. No extension or promotion.
H1 complex candidate NMSE .660550 and H1+fidelity .661834 versus G9 .504024;
backing .897499/.902845 versus .664428. Mixture-conditioned lead response improves,
so this is a fidelity/protection tradeoff, not a uniform defeat or a quality win.
The new active direction is [direct stereo → 7.1](UPMIX_RETROSPECTIVE_AND_STEREO71.md).
The implementation and former execution notes below are preserved as history.
Final independent binding verification completed: 68 snapshots, 9 checkpoints,
numeric mirrors and historical control checked, separate from the runner's exact
numerical checkpoint/Adam/diagnostic reloads. It did not freshly replay audio.
Final report SHA256
`b0cd3527a7ef63fae19152115da2f75c7283bea52e3157630850f07f3750d9d7`.

2026-09-13. User requested implementation of the new direction, including dense
electronic music/dubstep. Preserve G17 and all earlier evidence. H1 is not a new
name for a vehicle-ready upmix or an automatically approved rear-role model.

## Implementation and boundaries

- `hssr_hierarchical_source.py`: four frequency resolutions 257/129/65/33,
  widths 12/20/32/48; two per-band GRU48 blocks with full coarsest-grid frequency
  mixing; three learned upsampling/skip stages; ten full-bin complex source
  estimates plus exact unknown residual. FFT512/hop128 and reliable 1e-4 spatial
  features stay fixed for the first study. No genre input or added future frames.
- This is structurally different from the failed G7 pooled-global-GRU plus
  shallow band-memory model. It does NOT establish that hierarchy wins: G7,
  G6 and G13 remain negative controls/motivation, not evidence for H1 quality.
- 56,492 parameters; 2,641,424 estimated dense MAC/frame (~990.534M/s at 375fps),
  3,168 recurrent-state floats. This is about 4.36x G9 dense arithmetic, despite
  fewer weights. Counts exclude FFT/features/normalization/nonlinearities/WOLA/
  memory/renderer/OS. No Krait budget approval or SG5 export. Reduce cost only
  after demonstrating a useful quality reference; parameter count is not speed.
- Explicit state ownership, tensor shapes/device/dtype and checkpoint-change
  invalidation. Frame-local normalization must not consume future frames or other
  batch items. Existing QNX source/PCM guards are unchanged, not H1 integration.
- `source_scene_targets.py`: optional five-target scene contract (foreground,
  backing support, instrumental support, texture support, unknown) formed from
  INDIVIDUAL source tracks before family aggregation. Two guitars can have
  different roles. Notes bind source SHA, actual mixture context and interval.
  Partial/crossing/stale/conflicting evidence cannot approve a whole crop.
- Real-music positives need actual human-local evidence; authored notes have a
  separate provenance. Genre/frequency/width/brightness do not grant permission.
  A vocal hook remains foreground; a primary bass/synth retains its harmonics.
  This is reference-side target preparation, NOT a trained inference-time role
  recognizer. No real reviews were fabricated. Existing 11-family caches cannot
  be relabelled as separate instrument instances or role-confirmed scene targets.
- `source_scene_stress.py`: deterministic authored electronic-drop, dense-unison
  and sparse-tonal sources with known primary/support/texture intent. Common gain,
  band-limited harmonic/noise construction, intentionally retained modulation.
  These are contract fixtures, not real dubstep coverage or voice recordings.
  They are not included in the frozen H1 pilot training.

## First controlled learning experiment

`run_source_h1.py` compares G9 reliable control, H1 complex-only and the same H1
with additional waveform/multi-resolution fidelity at coefficient 0.1. H1 pair
starts with identical complete weights. All share two original TRAIN crops,
seed 20260917, 11-family targets, natural-mixture schedule, 1% quiet floor, Adam,
LR .001 through 512, clipping 5 and source/WOLA alignment. Equal updates are NOT
equal compute. No genre-generalization claim from this memorization probe.

The first pilot intentionally does not combine architecture, new role labels,
expanded data, new FFT and a new rendering policy in one uninterpretable run.
Subsequent source-to-scene training still needs supported individual-track roles.

Scores include unchanged per-family/G5 metrics, FLOAT/S16 input sensitivity,
actual lead/backing/guitar/foundation/unknown solos, waveform/MR-STFT errors and
protected-source response under masks computed once on the original mixture.
That response is not a perceptually audible-leakage percentage. Unknown-only
guitar in the existing two-song selection is still not demonstrated vocal bleed.

Completed G9 source/domain evaluation must exactly replay before training.
At 64/128/256, the fresh control must reproduce historical G9 weights, complete
Adam and original source/domain metrics. Every new checkpoint restores complete
weights/Adam and repeats all new diagnostics exactly. Independent verifier checks
code/data/recipe/optimizer/history bindings, numeric mirrors and recomputes the
candidate mean; it explicitly does NOT perform fresh audio rehash/inference.

## Guarded execution

- Immutable local runtime `build/upmix-g-control/source-h1-runtime-a`, 68 files,
  bundle SHA `8d58001197ea8f8853fa4c114715abb60f4c77bdb88881da2025b173b3b65371`.
- Private NAS output `build/reference-audio/v17-source-h1-sequence-a/{smoke,pilot}`;
  local control `build/upmix-g-control/source-h1-sequence-a` and stage subfolders.
- Confirmed local supervisor 57734 / initial smoke worker 57735. Recheck live
  identities before any signals. Initial `nohup` launch 57407 did not survive;
  no stage directories/results existed, empty launch log preserved. The detached
  process-session launch is the confirmed owner; no duplicate worker was started.
- Pure-stdlib supervisor, nice 10/two threads, 4 GiB child RSS, 22 GiB Mac/8 GiB NAS
  disk reserves. Sequence: 4-update smoke -> independent verification -> fresh
  256-update comparison -> independent verification. Failure stops later stages.
  Smoke 1800s/pilot 7200s; each verifier 900s; global 11000s. No auto-promotion/deploy.
- `control/pause.requested` pauses its active child and prevents future stages;
  supervisor keeps watching. Paused time counts against guards. Removing only
  this marker resumes H1. G17 has its separate queue/supervisor/worker pause rules.
  Do not stop only the H1 supervisor while leaving its worker live.
- Mac no-update synthetic 1497-frame forward/backward: 4.731s, peak 2,319,368,192 B.
  Not measured real-audio throughput, convergence or vehicle inference time.
- Initial 16 H1/model/target/harness tests passed in working tree and immutable
  runtime; full source regression 231 tests passed in 60.551s, including existing
  native host/sanitizer fixtures but no new QNX/vehicle timing.
- With the authored scene fixtures, the full 233-test source regression also
  passed in 65.430s. The frozen training runtime remains unchanged.

### Smoke completed; main comparison active

All three four-update smoke arms completed in 454.307s, peak 3,432,808,448 B. Every
checkpoint/Adam/source/domain/fidelity/mixture-response replay passed. Independent
verification checked 68 snapshots, 3 checkpoints and 7 numeric mirrors; subsequent
read-only recheck passed and 7 in-memory policy corruptions were rejected without
editing artifacts. Smoke plan SHA
`524e4ca924c1db4c4bea69a0ae6abc43d479a7cb26aca08fddfc712c60f656df`,
report SHA `41f77769198d87b1ba116b06971a5fb1e097e51714ff24a07b56ba70bf0eb323`.
Four updates are implementation qualification, NOT a source-quality win. The
historical 1024 checkpoint was replayed before training; fresh historical training
trajectory checks begin at 64, for which the original G9 has a saved milestone.

The supervisor automatically started the fresh 256-update comparison as worker
59626. The control G9 milestone 64 has now passed exact historical parameter,
complete Adam, source/domain and checkpoint-reload checks; H1 training is active.
Read local `source-h1-sequence-a/pilot/status.json` for current progress; this note
is not live telemetry. No car files or G17 training parameters were changed.

### Actual available TRAIN timeline, metadata-only audit

`audit_source_timeline.py` completed on the sealed catalog and full-TRAIN cache;
two independent unit cases cover overlapping unions, scored-core exclusions,
partition/duplicate/geometry/boundary rejection. No audio/model was decoded.
Output `build/upmix-g-control/source-h1-timeline-a`, plan SHA
`32fb89b88b4b266d5474879ea850b10e6b761450fb8b68c81ca29a6711c01b80`,
report SHA`30f5a96b1f6d9b17aeefa398e4d1076b67fca26a6bd66b362ed740b597604df5`.

All 181 TRAIN songs/28 artists total 39,107.067s (~10.86h). The 572 cached windows
contain 4,576 distinct seconds (76.27min), 11.7012% of that timeline. The union of
all possible scored crop cores, after 1s burn and 512-sample exclusion, is
3,997.899s (66.63min), 10.2230%. These are AVAILABLE contexts/cores, not the
actually sampled optimizer history, annotated roles or demonstrated genre
coverage. They do not prove that limited coverage alone caused weak fitting.
Future coverage expansion should use bounded raw-source reading/caching on NAS,
not describe repetition of the present windows as full-song training.

## Next acceptance work, not completed claims

1. Finish and interpret H1 source comparison versus protection and source fidelity,
   not only candidate sum. Do not treat a smoke pass as a quality improvement.
2. Expand a successful source candidate to representative TRAIN/DEV conditions;
   audit actual timeline/production-condition coverage. Keep final heldout sealed.
3. Establish supported per-track role intervals and source-to-scene targets;
   implement/train the separate slow-context role/risk path only with honest
   provenance. Current role preparation alone does not recognize arbitrary music.
4. Compare true-source, strong offline and learned-source renders with the same
   roles/renderer/common level. Calibrate temporal admission without erasing
   deliberate ducking/modulation or forcing a fixed rear-energy fraction.
5. Only then target compression/native ABI/deadlines and source/role/renderer
   integration, continuous-song/reset testing, measured cabin transfer and
   level-matched final acceptance. 3D OFF is not bypass; rear drivers share feeds.
