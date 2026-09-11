# V15 experimental streaming routing training

2026-09-11. User authorized continuing and launching training after the complete
587-core, 50-song FLOAT audit. This is **routing/pan-head distillation only**, not
semantic approval or a deployable replacement. No vehicle writes or promotion.

## Scope and admission

Baseline: `build/hssr-selection-v11/hssr_model.bin`, 368 inputs, two 160-unit GRUs,
264 outputs, 451,464 parameters. Both recurrent layers, six semantic class rows
per band and all global/LFE rows stay frozen. Only rear-selection and pan rows
are trainable. Two-stem pseudo-targets do not justify learning backing, hi-hat or
piano classes. This experiment does not redesign the network architecture.

The complete audit contains 136 backing-dominates-lead warnings, 420 quiet-piano
identity warnings and 31 karaoke input-path disagreements. Neither a separator
filename nor AST/CLAP similarity establishes a clean source or its musical role.
Original source/audit `training_eligible` / approval flags remain unchanged.

| Candidate mode | Train cases | Validation cases | Train windows | Validation windows |
|---|---:|---:|---:|---:|
| Original mixture, two-stem rear/pan target | 70 | 18 | 131 | 31 |
| Vocal-only input, zero rear target | 303 | 62 | 473 | 103 |

Cases can overlap by clip. There are 383 distinct admitted development clips,
128 development clips without supported windows, and 76 heldout clips excluded
from fitting and checkpoint selection. These are pre-mask window counts, not
guaranteed effective supervised duration. All 587 clips were considered.

Admission is an **uncalibrated experimental gate**, not ground truth:

- Require both shared-level and diagnostic-level content views, finite scores,
  at least one-second context and mono retained energy fraction >=0.1.
- Instrumental positive windows: maximum selected AST voice score <0.1, strongest
  nonvoice CLAP descriptor exceeds strongest voice descriptor by >0.05, and
  clip-level vocal-teacher disagreement <=-12 dB.
- Vocal-negative windows: AST voice score >0.1 and CLAP voice exceeds nonvoice
  by >0.05. No inference that vocals are lead rather than backing is required.
- Supervise only complete analysis/next-control windows inside accepted core
  intervals, excluding the first 64 controls and the final control of each clip.

## Training and persistence

`tools/stereo-upmix-render/train_v15_streaming.py` reads one contextual clip at a
time without a feature cache. Native FLOAT teacher identity/output hashes are
verified before use. Input mixture/vocals share headroom; input gains cycle through
0/-6/-12 dB and input features see rounded S16 PCM. FLOAT teachers remain intact.
Sound Check gain is not a network input and remains a post-synthesis operation.

The frozen runtime-aware recurrence stays continuous across 256-control chunks
within each contextual clip. Quantized hidden embeddings feed the trainable head;
prediction at control t supervises the target at t+1. AdamW LR=1e-5, no weight
decay, four epochs. Routing BCE is weighted by (1+4*target rear); pan MSE is weighted
by target rear and 0.5. Vocal-negative optimizer loss is additionally scaled 0.25.

Validation averages the two case-family means equally; it is not an audible
quality score. Epoch zero can win if no trained epoch improves validation.
Frozen parameters are checked exactly at each checkpoint. This does not establish
bit-exact round-trip equality with the original exported binary.

Fresh launch (do not duplicate a live process):

```sh
build/hssr-teacher-env/bin/python tools/stereo-upmix-render/train_v15_streaming.py --output build/hssr-v15-routing-a --epochs 4
```

Outputs: `admission-plan.json`, `status.json`, `disk-status.json`, `last.pt`,
`history.json`, then `candidate.bin` and `result.json`. Checkpoint stores optimizer,
best model, original scores, epoch and next case. Source manifest, final audit and
baseline hashes bind the run; per-case audit hashes are in the admission plan.
After confirming the previous process exited, resume with the same command plus
`--resume`; it requires an existing `last.pt`. No checkpoint exists until initial
validation has completed. Preserve failed/smoke outputs for inspection.

Disk pauses at <=22 GiB and resumes at >=27 GiB, checking between clips/chunks
and before saves. No automatic deletion, no new separator run, no multi-GB cache.
This is a polling guard, not a filesystem quota. At launch free space was 23.20 GiB.

## Verification and acceptance still needed

Seven streaming tests pass: dtype conversion, two-view gates, short/NaN rejection,
heldout exclusion, context mask, continuous embeddings, frozen-row optimizer.
Five existing cabin-training tests also pass, including exported-C emulator
regression (logit MAE 0.00113901). A one-case smoke run completes and exports a
469,760-byte binary; its validation did not improve, so its baseline was retained.
These checks verify mechanics, not a new candidate's acoustic quality.

After training: exported-C checks of the chosen candidate, continuous full-song
attribution/vocal leakage and rear-collapse checks, multi-level comparisons and
heldout regression evaluation. Existing heldout songs are a development regression
set, not newly blind material. Then level-matched listening against baseline.

Cabin assumptions remain 3D OFF, Focus All and minimum Surround; the latter two
have not been read back live. 3D OFF is not amplifier bypass. The renderer retains
the documented shared rear-door woofer/tweeter feed and bass safeguards; this
trainer has no measured cabin transfer loss and cannot claim full cabin optimization.
Further semantic training needs stronger labels or explicit review. Do not install
the experiment or promote it solely because pseudo-target validation improves.
