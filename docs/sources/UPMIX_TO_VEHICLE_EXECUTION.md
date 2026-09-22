# Continuous execution toward a vehicle candidate

2026-09-13 latest scope correction: **direct stereo → 5.1**, per user.
[P1-51 implementation and gates](UPMIX_P1_STEREO51.md) supersede the eight-output
proposal below. Model/loader/trainer/offline reference are synthetic-tested;
real-music target admission, role protection, native/QNX and vehicle approval
remain open. Do not mistake synthetic optimizer updates for music training.
Native 5.1 integrity/SRC import and actual optimizer/iterator resume are now
qualified (58 Mac tests; 33+5 NAS tests and full-size exact continuation check).
Two real 48 kHz targets are on private NAS with roles=pending; no admission bypass,
real-music training, export or vehicle write was performed.

2026-09-13 active direction override:
[G/H1 retrospective and direct stereo → 7.1 transition](../audio/UPMIX_TRAINING_HISTORY.md).
Do not launch further G/H1 extensions. G17 and the three H1 256-update arms have
completed; retain all artifacts. The historical gates/results below are evidence,
not an instruction to resume superseded experiments. P1 data preparation is active;
actual music training requires verified layout/target admission. The six-channel
vehicle transport and existing safety boundaries are unchanged.

Owner: Luka. Updated 2026-09-13. The user explicitly requested continuing through normal
research/build/test iterations without asking for approval at every milestone.
Do not stop merely because one bounded experiment completed. Continue the next
safe implementation step; stop for genuinely missing authority/access/evidence,
not to ask whether another ordinary experiment should be run.

This is an execution plan, not a statement that a model is ready. No installed
vehicle file has been changed by the G-series experiments. Preserve all existing
audio, teachers, calculated caches, checkpoints and reports. No paid compute,
new accounts or destructive cleanup is implicitly authorized.

User requirement 2026-09-13: maximum genre-agnostic operation, explicitly even
dubstep. Apply the [genre-agnostic acceptance contract](../audio/UPMIX_TRAINING_HISTORY.md)
to subsequent design, data-coverage audits and comparisons. Foreground bass/synth
hooks and their upper harmonics are not automatic rear content; useful secondary
parts must still survive. This requirement is not evidence of current corpus
coverage or model acceptance, and does not mutate the frozen G17 continuation.

New H1 implementation/execution is tracked separately in
[UPMIX_H1_GENRE_AGNOSTIC.md](../audio/UPMIX_TRAINING_HISTORY.md): multiscale source model,
individual-track role-target contract, and guarded Mac smoke/pilot sequence.
Do not apply G17's PID or pause mechanism to the separate H1 supervisor.

## Gates, in dependency order

1. Source learnability and optimization: finish matched G7 architecture and
   longer-budget G5 controls. Inspect individual heads as well as their sum.
   A lower summed error cannot excuse poorer backing or foreground protection.
2. Full TRAIN-family coverage: prepare the new aligned source cache from every
   eligible TRAIN song; inspect actual scored material, not metadata counts.
   Fill demonstrated sampling gaps before claiming support for hats/pads/etc.
3. Broader learning/generalization: train on balanced actual-source coverage,
   then evaluate frozen development sets; preserve final heldout independence.
   Revisit protected/positive objectives with reconstruction and anti-collapse
   controls. A two-song memorization probe is not the finished dataset/training.
4. Musical role and abstention: lead/co-lead/doubles/foreground hooks/reverb stay
   front; backing/secondary parts are optional. Family identity alone cannot
   approve placement, especially mixed primary/secondary guitars in one head.
   Never manufacture a human-local review from automated analysis. Any weaker
   supervision must retain its real provenance and explicit uncertainty.
5. Native implementation: exact source operators, model/state/feature/export
   parity, streaming/reset/FLUSH/rate generation handling and warmup. Coupled
   dry-front removal and rear permission must not lose music on a veto.
6. Renderer/cabin: PCM order FL FR FC LFE SL SR; initial C=0, quiet bass assist,
   Sound Check once after FLOAT synthesis, then linked matrix/sample rails and
   S16. Rear-door woofer/tweeter share a feed. 3D OFF is not a DSP bypass; known
   J525 input matrix is not a complete acoustic transfer or inverse-EQ license.
7. Candidate acceptance: calibrated source/role/protection regressions, measured
   target QNX/ARM deadlines/memory, frozen heldout, level-matched listening and
   a reversible package. Do not call host-only tests or zeroed rear "ready for
   the car". Physical audibility and user preference require real evidence.

Continue implementation/preparation where safe if a later gate lacks target
access or listening evidence. Report a real blocker precisely when it becomes
the remaining dependency; do not silently weaken the acceptance contract.

## G7 completed configuration and result

Files: `hssr_band_source.py`, `run_source_g7_bandwise.py`, `test_source_g7.py`.
The 512/128 output and full Nyquist coverage stay unchanged. Six invariant
stereo features retain each of two adjacent bins as separate channels in129
groups. Local width24 features have a residual depthwise/pointwise stage. A
33-group pooled global GRU64 supplies context, while a shared GRU8 has an
independent state for each frequency group. A rank12 factorized decoder emits
the same ten complex source estimates plus exact `unknown` residual.

The two G7 arms have identical complete initial states,96,508 learned parameters
and198.261million dense MACs/s. The `framewise` arm resets its band GRU to the
same learned initial value every frame; `bandwise` carries1032 band-state floats
in addition to the64 global-state floats. Both retain the global causal GRU.
These counts exclude FFT/features/nonlinearities/memory and are not target timing.

The third arm is the existing G5 width8/GRU64 invariant model, trained to the
same longer budget. Each gets1024 updates on the exact two G5 TRAIN mixtures:
LR0.001 through512 and0.0003 afterward, clip5, same reconstruction-only objective
and scored core. Milestones64/128/256/512/1024 are interleaved across all arms.
G5 step256 must reproduce its historical parameters/evaluation exactly, so a
longer training budget is not misattributed to a new architecture. All saved
checkpoints are numerically reloaded; there is no automatic promotion/export.

Main: `build/reference-audio/v16-moises-source-g7-bandwise-a` (private NAS),
local `build/upmix-g-control/source-g7-bandwise-a`; former PID9092 completed all
three1024-update arms in8385.89 reported work seconds. Four-step smoke completed all three arms in78.53s,
with3 SHA/exact reload checks,9 numeric mirrors and20 code snapshots verified.
41 focused G5/G6/G7 tests passed before the main run.

At1024, candidate sum NMSE is0.330300 G5,0.552090 framewise G7,0.514215 bandwise
G7. Backing NMSE on the two fixed TRAIN crops is0.354306/0.634725 for G5,
0.814567/0.844862 framewise and0.822248/0.850212 bandwise. G7 is not promoted.
These remain two-song learnability results, not a generalization comparison.
All15 checkpoint SHAs/metadata/finite weights and20 snapshots were independently
checked after completion; the runner had already replayed every evaluation exactly.
`verify_source_records.py` distinguishes a binding check from independent numerical
re-evaluation. State-tensor counts include fixed buffers: G7 has96508 learned
parameters plus258 buffer values, not96766 learned parameters. The initial
verification count-label record is preserved beside the corrected v2 proof.

## Full TRAIN source preparation completed and independently verified

`prepare_source_full_train.py` reuses the sealed source catalog and TRAIN vocal
activity scan. It selects572 eight-second windows from all181 TRAIN songs /
28 TRAIN artists: two fixed duration-relative positions, then nonoverlapping
existing activity-selected positions. Native start/alignment and common FLOAT
gain are shared by every source. Only the existing private-NAS audio is read.
No validation/heldout audio, new download or source-separation teacher is used.

Expected new raw cache is18.002GiB on NAS; hard cache budget32GiB. The Mac holds
numeric status/report mirrors only. Reserves remain22GiB Mac /8GiB data/output.
Cache identities, source provenance, SHA and actual scored-family activity are
recorded. Completion is not `training_ready` if material-family coverage gates
fail; those deficiencies must be addressed explicitly, not overwritten.

Main: `build/reference-audio/v16-moises-source-full-train-a`, local
`build/upmix-g-control/source-full-train-a`; launch PID11356,21600-second work
guard. `--resume` requires identical plan/code/input binding, verifies completed
cache bytes/activity, and preserves incomplete files in a recoverable folder.
Completed datasets are immutable. Do not rerun a completed job or duplicate a
live owner. Four real-window smoke hashes passed;4 preparation unit tests cover
partition/source binding, complete selection, actual material coverage and an
interrupted/resumed cache without deleting the interrupted artifact.

Completed572 windows /181 songs /28 artists in1750.50s. All572 cache SHAs and
all five scored-crop activity records per cache were independently rechecked;
10 preparation snapshots and local plan/report mirrors matched. Verification
took398.01s and is recorded in `verification.json` in both roots.

Actual material coverage (windows / songs / artists): lead391/106/25,
backing270/57/19, high-percussion161/63/16, guitar479/159/27, pad81/28/10,
keys310/106/16, strings-winds93/37/9, FX-other43/17/5. No declared TRAIN-family
coverage deficiency remains. This is source activity, not musical placement.

The old48-window loader retained one mmap/descriptor per cache and failed with
`Too many open files` on the expanded corpus. No corrupt audio was found.
`source_dataset.py` now validates SHA/activity one file at a time, closes every
mapping, and supplies independently owned slices through `ClosedWave`. It rejects
changed file identity/size/mtime on later reads. A600-proxy test exercises this
ownership boundary. The original loader is unchanged while G7 uses its snapshot.

## Fixed broader development set and G8 curriculum

`prepare_source_development.py` completed80 fixed eight-second windows from all
40 validation songs /9 artists in472.42s. Two duration-relative positions per
song were chosen before model scoring. Artifacts:
`build/reference-audio/v16-moises-source-all-dev-a`, local `source-all-dev-a`.
No final-heldout audio was decoded. This is a reusable development set and must
not later be called an untouched final test. Rare-family development coverage is
still thin: pad5 windows/2 artists, strings-winds4/2, FX-other1/1. Do not hide this
behind an all-family mean or claim broad FX generalization.

`train_source_full.py` starts a fresh width8/GRU64, invariant-input, subpixel
SourceHead; no two-song overfit weights initialize it. Every six updates include
three complete-coverage natural examples, one family/artist-balanced natural
example, one true protected-only input and one true positive-only input. All
eight candidate source families occur in both balanced/positive-solo schedules;
lead gets half the protected-only slots, foundation/unknown share the rest.
Complete coverage cycles every window and changes its crop offset each pass.
The loss is balanced complex reconstruction of all11 true source families,
with the same1% quiet floor; no new aggressive leakage penalty or frozen-mask
attribution. Source identity remains separate from rear suitability.

Original Mac G8: `build/reference-audio/v16-moises-source-g8-full-a`, local
`build/upmix-g-control/source-g8-full-a`;4096 updates,21600s work guard,
32-step LR warmup to0.001,0.0003 after2048, clip5,2 CPU threads. All80 development
windows are evaluated at512/1024/2048/4096 with independent-head metrics, every
material source solo, gain/polarity checks and exact checkpoint reload/replay.
Each development window uses its fixed last4s input,1s burn and final512-sample
exclusion. Report equal-window and equal-artist aggregates, not just pooled loss.

Four-step smoke on the real full TRAIN cache plus two development windows
completed615.82s (mostly input validation/baseline fitting), with20 snapshots,
4 numeric mirrors, checkpoint SHA and exact numerical replay verified. Main
captures code before input preflight and reports baseline-fitting progress.
It must not be duplicated or have its frozen source edited while active.

### Qualified CPU/Linux relocation, 2026-09-12 late evening

Both paused Mac jobs were resumed at23:13:54 EDT with unchanged code and readable
NAS data. G7 finished. G8 stayed in baseline fitting: a32.23MiB sequential cache
read took39.89s and a warm12MiB mapped crop3.18s over the current NAS connection.
The original Mac PID28515 was interrupted with SIGINT inside `fit_constant` /
`ClosedWave._read`, before optimizer creation (last progress544/572). No trained
state was discarded; its files and original status remain preserved.

The NAS is Ryzen3 2300X/4 CPU cores/8GiB RAM. A private1.1GiB virtual environment
under `Audi/upmix-training-2026-09-11/runtime/source-g8-linux-20260912` uses Python
3.12.3, torch2.10.0+cpu, NumPy2.5.1, SciPy1.17.0, soundfile0.13.1. No global system
packages, account permissions or services were changed. Setup uses the official
[pip bootstrap](https://pip.pypa.io/en/stable/installation/) and
[pinned PyTorch CPU distribution](https://pytorch.org/get-started/previous-versions/).

An immutable27-file bundle captures the transitive local Python imports in
addition to the20 training snapshots. Nine Linux tests passed. Eight bounded
real-cache updates took12.11s including selected-cache validation: about1.23s
compute /0.03s read per update, peak RSS1.07GB. Full572 TRAIN +80 development
cache preflight, four updates and two-window evaluation/exact checkpoint replay
then completed278.95s. Independent SHA/metadata/mirror checks passed. The smoke
does not claim full-development quality (its two windows have no material backing).

Current main: `v16-moises-source-g8-nas-full-a`, started2026-09-13T03:50:59Z
(23:50:59 EDT). Former NAS supervisor PID95510/training PID95511 completed;
do not resume these old PIDs. There was no duplicate Mac training job.
Data-relative output is unchanged in meaning; curriculum, initialization, losses,
all80 development windows and4096 steps are unchanged. CPU/Linux is a separately
qualified runtime, not a claim of a bit-identical Mac optimization trajectory.
All4096 optimizer updates and all80-window evaluations/exact replays completed
in9328.289s; supervisor exited cleanly. Independent verification checked20 run
snapshots (27 files in the runtime bundle),4 checkpoints and7 numeric mirrors.
Plan SHA`c7a2464b677238656894979a7feb0a7f8f25ec61e3713c5e32e7d917380d3c80`;
final checkpoint`42f283f98830194d184e06b215edfcce3b1059b51870668340663f191523a55e`.

Remote base: `/mnt/shared/users/luka/Audi/upmix-training-2026-09-11`.
Read `runtime/source-g8-nas-full-a/status.json` and
`runtime/source-g8-nas-supervision-a/status.json`. The detached supervisor owns a
single-run lock, two CPU threads/nice10,4GiB RSS /1GiB available-memory guards,
8GiB data-space reserve and21600s work budget, and checks the27 source hashes.
If the user pauses, stop BOTH supervisor and child; retain in-memory state and
distinguish it from checkpoints. Never restart from this plan without checking
actual processes and artifact ownership. No final heldout or vehicle is involved.

### Early G8 development results and scored-core coverage audit

At512/1024 updates, all80 original development windows and exact checkpoint
replay completed. Candidate sum NMSE improves0.858557 ->0.782900 (constant
baseline0.890633), but backing NMSE worsens0.912823 ->0.923870 and fitted backing
gain falls0.06325 ->0.05300. On actual lead-only input, combined candidate-output
energy/input is0.45912 ->0.42274. This is a protected-solo robustness test, not
uniquely identified contamination in a full mixture. These results do not qualify
the separator; aggregate improvement must not hide weak individual heads.
At2048, exact all80-window replay also passes, but candidate NMSE regresses to
0.879034, backing NMSE0.926575 / gain0.05603, and lead-only combined candidate
energy/input0.474714. The bounded4096-step run continues as recorded; there is
no checkpoint promotion or assumption that more epochs fix representation.
Final4096 candidate NMSE0.697478 is better in aggregate, but backing NMSE0.927283
and fitted target gain0.045657 remain weak. Lead-only combined candidate energy
is0.429186. No checkpoint is qualified. Foundation natural NMSE1.043714 despite
good positive-solo NMSE0.055981 further shows why solo success cannot substitute
for correct mixture separation. Sparse DEV-family coverage remains as documented.

`audit_source_development.py` binds both saved evaluations to the original
last4s input /1s burn /512-sample exclusion and the sealed activity metadata.
Actual main-core backing coverage is18 windows/6 artists, high-percussion14/3,
pad3/1 and FX0/0. The earlier cache-wide counts were any of five scored crops,
not the core actually evaluated by G8.

`build/upmix-g-control/source-development-core-audit-a` selects19 supplemental
crops: for every family absent from a window's main core, choose its earliest
sealed material crop, then deduplicate window/offset. No model error/prediction
enters selection and the80 main crops stay unchanged. Combined distinct-window
coverage becomes backing24/7 artists, high-percussion22/5, pad5/2 and FX1/1.
Overlapping crop counts are not independent songs/artists. This metadata audit
does not re-read current audio bytes; supplemental evaluation must reverify them.
Three audit unit tests pass. Supplements are development-only, not final heldout,
optimizer examples or placement approval; FX evidence remains very limited.

The19 supplements were actually evaluated for G8 step1024 in
`v16-moises-source-g8-supplement-1024-b`: selected audio SHA/activity reverified,
all their original main-core records exactly replayed, then supplementary scores
exactly repeated after reloading the checkpoint. Supplemental backing NMSE0.930511,
FX0.966462 with fitted FX gain0.02062; this confirms weakness, not new approval.
The independent binding verifier checks30 snapshots, parent checkpoint and three
numeric mirrors; it is not an additional independent audio/numerical evaluation.

Initial supplement run `...-1024-a` failed original-record exact replay because
the separate SSH launch did not inherit the supervisor's BLAS/OMP/MKL thread
environment. Setting only `torch.set_num_threads(2)` was insufficient. The new
runner requires OPENBLAS_NUM_THREADS/OMP_NUM_THREADS/MKL_NUM_THREADS=2 **before
imports** on Linux and records them; old failed artifacts remain preserved.
No numeric tolerance was weakened. `train_source_full.evaluate` now offers
explicit supplemental offsets; its default last4s result/format is unchanged,
tested against explicit default offsets and the actual NAS original records.
Never edit the already-running G8 bundle to acquire this optional helper.

## G9 spatial-conditioning comparison

The real native FFT exposed a numerical feature problem: near-empty spectral
bins retained full balance/coherence confidence with the legacy1e-16 denominator
floor. Feeding native FFT values into PyTorch reproduces the~1.08e-4 state drift;
native/PyTorch recurrent operators on the same FFT differ only~2e-7..6e-7.
FFT relative MSE itself is~1.55e-14. Do not loosen state tolerance to mask this.

`invariant_reliable` explicitly adds a declared relative-power floor only to
balance/coherence denominators; log-energy inputs are unchanged. It adds no
parameters or future samples. Applying it to old weights changes their learned
contract, so it must be trained and exported separately, not silently enabled.

`v16-moises-source-g9-conditioning-a` completed three256-update TRAIN-only arms
in621.46s: identical100192 parameters/initialization, two fixed G5 TRAIN crops,
objective, learning rate and schedule; floors0,1e-6,1e-4 are the sole intervention.
All9 saved original/domain evaluations replay exactly;31 snapshots,9 checkpoints
and12 numeric mirrors were independently binding-checked. G5 step256 reproduces
its historical parameters/evaluation exactly. No validation/heldout was scored.

| Step256 metric | Legacy | Floor1e-6 | Floor1e-4 |
|---|---:|---:|---:|
| Candidate sum NMSE |0.500276|0.499203|0.504024|
| Backing NMSE, crop1 |0.539459|0.557583|0.577529|
| Backing NMSE, crop2 |0.735132|0.738043|0.751327|
| S16 output perturbation NMSE, -12dB crop1 |2.912e-3|4.737e-4|1.211e-6|
| S16 output perturbation NMSE, -12dB crop2 |1.419e-4|3.961e-5|4.847e-7|

Floor1e-4 greatly improves ingress stability but loses some backing reconstruction
in this short probe; it is not promoted as a separation-quality winner. The
long-budget extension uses the same three arms through1024, LR0.001 through512
then0.0003, and requires exact historical G5 parameter/evaluation reproduction
at256 and the G7 extended-control512/1024 milestones. Run in a fresh immutable
bundle; never modify the completed short experiment or active G8 runtime.

Extended run `v16-moises-source-g9-conditioning-long-a` completed all three1024
arms in1713.10s (former Mac PID83646). Its private38-file runtime is
`build/upmix-g-control/source-g9-long-runtime-a`. All15 checkpoints replay; legacy
G5 parameters/evaluation exactly reproduce historical256 and512/1024 controls.
At1024: candidate NMSE0.330300 /0.336880 /0.334680 for legacy/1e-6/1e-4.
Backing NMSE is0.354306/0.634725 legacy,0.364483/0.635837 floor1e-6,
0.361853/0.641848 floor1e-4. The1e-4 reconstruction gap shrinks to~1–2% relative
backing NMSE in these two TRAIN crops; S16 output perturbation at-12dB falls from
3.936e-3/3.507e-4 to5.839e-6/1.933e-6 (~674x/181x lower).
Choose1e-4 as the explicit numerically reliable research baseline for subsequent
representation experiments, NOT a qualified separator or vehicle promotion.

## G10 paired inter-frame phase experiment

`hssr_phase_source.py` and `run_source_g10_phase.py` retain the reliable1e-4
six-channel base and append four L/R temporal-coherence channels. Compare
`g10-zero` and `g10-advance`: identical100352 learned parameters, all initial
states, source targets, optimizer and two TRAIN crops. Added input weights start
at0. The only intervention is exposing versus zeroing the four new features.

Features use current and previous frame-normalized complex bins; remove the
known bin-center128/512-hop phase rotation with exact quadrant constants. The
same bounded coherence denominator downweights weak bins and large amplitude
mismatches. Previous complex spectrum is explicit4112B state, alongside GRU64;
there is no longer analysis window or additional future sample. Dense arithmetic
adds10400 MAC/frame (+160 weights), excluding feature/FFT/WOLA/renderer costs.
This is not a claim of better source separation or measured target feasibility.

Six tests cover known phase advancement, matched initialization/zero control,
gradient reachability, weak-bin confidence, streaming/reset, future independence,
gain/polarity/static common-phase behavior, closure and rejection by SG5 exporters.
The new phase-state/input layout is NOT SG5F3201 or SG5R3201 compatible; if useful,
it needs separate native implementation/ABI qualification before routing use.

The runner first exactly replays completed G9 reliable step1024 source and actual
FLOAT/S16 diagnostics. Main comparison is bounded at1024 steps, LR0.001 through512
then0.0003, interleaved64/128/256/512/1024 milestones and exact checkpoint replays.
No validation/heldout/role approval or new audio generation is involved. The
ten-channel zero control need not follow the historical six-channel convolution's
bit-identical optimization trajectory; it is the matched control for this pair.
Private runtime`build/upmix-g-control/source-g10-runtime-a` has41 files, SHA
`bbc419d89dcb5b8a9db55dfca15378e7a42da3cbd1702358256e54fe02a4f43e`.
Initial4-step smoke: `v16-moises-source-g10-phase-smoke-a`, local control
`build/upmix-g-control/source-g10-phase-smoke-a`, former PID95683 completed both
arms in193.495s. Independent binding verification checked41 snapshots,2 saved
checkpoints and6 numeric mirrors; original source and FLOAT/S16 replay were exact.
Main `v16-moises-source-g10-phase-a`, local `source-g10-phase-a`, former PID97469
completed both1024-step arms in1228.556s. Independent binding verification checked
41 snapshots,10 checkpoints and14 numeric mirrors; original source and input-domain
replays were exact. Plan SHA`65f46d15d473dc877498dbab3e51f751cd18fbfa91acde19831d722d7f2116f9`.
Final phase candidate NMSE0.330445 versus zero-control0.333858 (1.02% lower);
backing0.354644/0.639156 versus0.361153/0.640702. Lead-only candidate energy falls
0.0217133->0.0117240 on the first crop but rises0.0404169->0.0420065 on the second.
S16 perturbation at-12dB is7.1614e-6/2.0602e-6 (zero-control5.9486e-6/1.9518e-6).
This modest TRAIN gain is not a qualified separator or universal robustness win.

Post-launch repository-only hardening makes the returned previous spectrum own
its4112B storage under `no_grad` instead of keeping a view of an entire input
chunk; malformed/incomplete phase states also fail explicitly. The live runtime
snapshot is intentionally unchanged. Numerical replay is required before using
this changed implementation with saved experimental checkpoints. Both completed
G10 source and actual FLOAT/S16 evaluations exactly replayed in the decoder-rank
diagnosis below. The six phase tests plus two source-route tests passed after
the storage-ownership change. Optional width/hidden constructor arguments added
later preserve the default initialization and are not changes to a live bundle.

## Decoder cost diagnosis and G11 capacity redistribution

The G9 direct mask convolution costs499200/605936 dense MACs per frame (82.4%).
`hssr_factor_source.py` factors its linear kernel3 map into kernel3-to-rank and
rank-to-output kernel1 maps, retaining bias, edges, full257 output bins and source
closure. No nonlinear operation is inserted between factors. Fixed ranks8/16/24/
32/48 are post-hoc diagnostics on saved G9 and G10 phase weights, not new training.
Full rank48 controls rounding and is deliberately more expensive than the original.

`analyze_source_decoder_rank.py`: `v16-moises-source-decoder-rank-a`, local control
`source-decoder-rank-a`, former PID7233 completed in428.522s with zero optimizer
updates;45 snapshots,10 checkpoints and13 numeric mirrors independently checked. Immutable
45-file runtime SHA`ff0fb9959c1032ef376e91db780e0dc83c8125d5562aa1c1ed26fe739f036340`.
Before testing ranks, it exactly replayed G9 reliable and both completed G10 source
and actual FLOAT/S16 evaluations using the state-ownership fix. Weight residual
alone is not source quality: G9 rank16 reduces arithmetic47% but backing NMSE
worsens0.361853/0.641848->0.409213/0.664010. Rank24 is less damaging but still not
an automatic post-hoc replacement. Full-rank source drift stays below7.81e-14
and recurrent/phase states remain exact. No compressed checkpoint is promoted.
Results/checkpoints are preserved separately; verification plan SHA
`1e4d5591e18047c58b3649447a54c7cddb9180794227898488cda80b63116d7f`.

G11 `run_source_g11_capacity.py` compares three FRESH1024-update arms on the same
two TRAIN crops and unchanged balanced complex loss/LR/phase inputs. G10 baseline
must reproduce historical parameters, Adam state and source/FLOAT/S16 scores at
every matching milestone. Small rank24 retains identical initial front/context;
wide rank24 changes width8/GRU64 to width12/GRU96. Fresh first-factor weights are
random and the final factor is zero, avoiding two-zero-factor dead gradients.

| Arm | Learned parameters | Dense MAC/frame | GRU state |
|---|---:|---:|---:|
| G10 phase baseline |100352|616336|64 floats|
| G11 small rank24 |97664|441616|64 floats|
| G11 wide rank24 |212848|605976|96 floats|

All retain4112B previous-spectrum state and add no future samples. Arithmetic is
not measured target speed; factor weights/state need a separate native ABI. This
is representation/capacity learnability, not full-TRAIN generalization or routing.
Thirteen focused phase/factor/capacity tests pass. Immutable47-file runtime
`source-g11-runtime-a`, SHA`679a17b55694217b1b16fff54f1e6080b9114368819175c5b6cf92f5e2bd7716`.
Four-step smoke `v16-moises-source-g11-capacity-smoke-a`, former PID9138 completed
in176.185s;47 snapshots,3 checkpoints and7 numeric mirrors independently verified.
Main `v16-moises-source-g11-capacity-a`, local `source-g11-capacity-a`, former
PID10937 completed all three1024-step arms in1934.795s. Historical baseline
parameters, Adam state and source/FLOAT/S16 evaluations exactly replayed at all
five milestones. Independent binding verification checked47 snapshots,
15 checkpoints and19 numeric mirrors; plan SHA
`538a2ac1e9aef4eeca75abe9b1698f8bb6e0892a57b48f417cc89164e1c2b99b`.
Candidate NMSE is0.330445 baseline,0.366995 small and0.324821 wide. Wide backing
improves modestly to0.344655/0.627138, but its second lead-only candidate energy
rises0.042006->0.080455 (first improves0.011724->0.007637). Neither factorized
variant is promoted; a1.7% summed-error gain does not excuse that regression.

## G12 reliable-input full-TRAIN intervention, completed, not promoted

G8 completion does not close the generalization gate. G9 established why a
relative-power floor is required for numerical stability, but its two TRAIN crops
do not establish broad learning. The next full-TRAIN run keeps G8 width8/GRU64,
100192 parameters,572 TRAIN windows/181 songs, actual curriculum/gain/targets,
loss and4096-update LR schedule, changing only spatial confidence to reliable1e-4.
It starts from the same fresh initialization, NOT G8 or two-song learned weights.

`train_source_full.py` preserves the explicit legacy default and adds a distinct
`g12-full-train-reliable-source-v1` plan/SG5R3201 input contract. A completed G8
reference is required: source/data/report/checkpoint bindings, exact schedule
prefix, constant calibration and DEV selection must agree before updates begin.
Main cannot silently use a limited DEV subset or a different update budget.
The4-step/two-DEV-window smoke is plumbing qualification, not source-quality proof.

`supervise_source_nas.py` now admits only the prior4096-step/21600s full mode or
explicit4-step/1800s/two-window smoke. Both variants retain the SAME legacy lock
name,2 threads/nice10,4GiB RSS/1GiB available-memory and disk guards. Pause both
supervisor/child; do not mutate an old supervisor file or restart an old PID.
Five reliable-full tests and one supervisor-policy test pass, including bit-exact
legacy factory/bytes, same parameter initialization and invalid reference/config.
Prepared51-file runtime`source-g12-runtime-b`, SHA
`d74ffc30fd71f58754d63c437173f1ab786a23f7e763c1d18ca86e98795c6801`.
NAS runtime is `runtime/source-g12-linux-bundle-20260913-b`; all51 hashes and
14 focused Linux tests passed. Smoke `v16-moises-source-g12-nas-smoke-a` completed
full572 TRAIN/all80 DEV preflight,4 updates and two-window scoring in265.983s.
Independent verification checked21 run snapshots,1 checkpoint and4 numeric
mirrors, with exact original numerical reload. Its two DEV windows contain no
material backing; this is plumbing qualification only.

Main `v16-moises-source-g12-nas-full-a` started2026-09-13T06:53:45Z, NAS supervisor
PID98178 / child98179, control `runtime/source-g12-nas-full-a`, supervision
`runtime/source-g12-nas-supervision-a`. At07:27UTC it had reached727/4096 updates
and completed the first all80-window512-step evaluation/exact replay. Check live
process identity/status before any pause/resume. Stop both actual processes on
user pause. Wall-budget guards currently include paused wall time; SIGSTOP is
not a saved optimizer checkpoint or unlimited-duration resumability promise.

At1024, all80 DEV evaluations/exact checkpoint replay completed. Candidate NMSE
is0.770835 versus G8-at1024 .782900, but backing .924717/gain .052574 versus
.923870/.053002 and lead-only candidate energy .423238 versus .422742 do not
establish improved individual-source quality/protection. The reliable floor
addresses numerical conditioning; it is not a demonstrated generalization cure.
Main was back to training at1079/4096 at07:50UTC.

At08:28UTC it reached2048 and was repeating the checkpoint across all80 DEV
windows. The sealed2048 evaluation gives candidate NMSE .918731, backing
.930327/gain .057250 and lead-only candidate energy .476128. These do not improve
on matched G8-at2048 (.879034/.926575/.474714 respectively); do not promote an
intermediate checkpoint because the reliable feature floor is numerically safer.

Main finished all4096 updates and all four80-window DEV evaluations/exact Linux
reloads in9440.929s (2026-09-13). Supervisor98178/child98179 are completed launch
identities; do not signal them without fresh process verification. Independent
Mac/SMB binding verification checked21 code snapshots,
4 checkpoints and7 mirrors; it did not substitute a Mac numerical replay for
the qualified Linux evaluation. Plan SHA
`e4a9ee5c2a6dcc9168246a4e3e833a4de44cb72a4ab10ebfcd5d4fbd0b4393cd`;
final checkpoint `661faf4188c38517333bea888c2b54033c605836476e790c3f9df1b4ec041e51`.
Final candidate NMSE .689604 versus G8 .697478 is only a1.13% improvement.
Backing NMSE .928129/gain .044515 (G8 .927283/.045657) remains weak; actual
lead-only candidate/input energy .412811 versus .429186 is still unacceptable
as a placement qualification. No separator/rear/deployment approval. Final
rare-family supplement and seen-TRAIN fit initially awaited restored SSH;
both completed in the qualified Linux environment later on2026-09-13 (below).

The supplemental evaluator now explicitly supports both legacy G8 and reliable
G12 input contracts, including the checkpoint's exact floor. It cannot relabel
legacy weights as reliable. Two contract tests plus three DEV-audit tests pass
on both Mac and NAS. The immutable56-file `runtime/source-supplement-runtime-c`
has SHA`8e591fac7f541237ab050e5ab1f2aea7395f9ba08b9a213a3243407a9dd189bc`.
G8 final4096 supplemental evaluation completed as former NAS PID99435, output
`v16-moises-source-g8-supplement-4096-a`, control
`runtime/source-g8-supplement-4096-a`, nice15/two BLAS threads/1800s/2GiB peak
guard. All19 cache SHA/activity checks, their original main-core numerical replays,
supplemental scores and checkpoint reload/replays passed in406.541s. Independent
binding verification checked35 run snapshots, the original G8 final checkpoint
and3 numeric mirrors; plan SHA
`457952cae8f00bae2854d91b0754e5dc55577b762531b6c650e3204bd2126e91`.
Candidate NMSE0.638905 does not hide weak backing0.926921/gain0.05008, pad0.952892
and FX0.988579/gain0.007345 (only1 window/1 artist). Lead-only combined candidate
energy0.433017 remains unqualified. It reuses the same19 activity-selected
overlapping crops, not19 new independent DEV examples, with no updates. Later G12 matching
checkpoints must use this same selection for a comparable rare-family check.

G12 step1024 same-selection supplement completed in403.965s, former NAS PID99967,
output `v16-moises-source-g12-supplement-1024-a`, using runtime-c above. All19
selected audio SHA/activity checks, original-main and reloaded-supplement exact
replays passed; independent binding verification checked35 snapshots, the parent
checkpoint and3 numeric mirrors. Plan SHA
`ea4bda4cf05b62c7af6178b6b464bce37d04431fffdf83e4e46a6c3bca3a7e02`.
Candidate NMSE .724490, backing .930013/gain .051686, pad .946970, FX .966329 and
lead-only candidate energy .414618. Matched G8-at1024 supplemental candidate
.742323 is slightly worse, but individual backing remains weak. FX still has
only1 crop/1 artist; this is not proof of broad rare-family or vocal-role quality.

After SSH access returned, G12 final4096 supplement completed394.528s, former
NAS PID106069, `v16-moises-source-g12-supplement-4096-a`. Same19 overlapping
selections, exact original-main/reloaded-supplement scores;35 snapshots/parent
checkpoint/3 numeric mirrors independently bound on NAS. Plan SHA
`5455e229f10b7c0132cfe15f4e0ab6ff91a4e337d78e97df3ddac03a21c571f3`.
Candidate NMSE .646519, backing .927703/gain .049097, pad .950626,
FX .991387 and lead-only candidate energy .417636. No source or role approval.

## G13 matched full-frequency encoder experiment, completed

`hssr_fullbin_source.py` retains257 frequency positions in a width4/GRU64 phase
model with a direct40-channel kernel3 complex-mask decoder. Its matched control
uses the same complete initial state and158840 learned parameters, but strides
the input convolution by4 (65 positions) and linearly interpolates its SiLU
features back to257 with `align_corners=True`. Thus the paired difference is
frequency decimation/interpolation, not additional parameters or labels. Both
retain10 reliable phase features,4112B previous-spectrum state and no future
samples. Estimated dense MAC/frame:415880 control,454280 full-frequency;
interpolation/FFT/nonlinearities are excluded, and these are not target timing.

`run_source_g13_full_frequency.py` reuses the strict G11 harness with a distinct
study/plan. Three fresh1024-update arms include historical G10 phase as a bridge;
the G10 parameters/Adam/source/FLOAT/S16 results must reproduce exactly at every
matching milestone. G10 has different geometry/parameter count from the matched
pair; do not call it an equal-parameter control. Same two TRAIN crops, balanced
complex loss, schedule and LR are retained. No DEV/heldout, native export or
role approval is implied. The width6/GRU48 alternative is unit-tested only,
not an additional trained arm.

Six G13 tests plus two G11 and six phase tests passed. Four-step smoke
`v16-moises-source-g13-frequency-smoke-a` completed all three arms in202.181s,
with51 run snapshots,3 checkpoints and7 numeric mirrors independently verified.
Main `v16-moises-source-g13-frequency-a`, local control `source-g13-frequency-a`,
former Mac PID26460, started around07:18UTC from immutable52-file `source-g13-runtime-a`,
SHA`0011b1299703cfd2663c86debd46a58f871237a7e86cdb8e92847e2e7051ba48`.
All three1024-step arms completed in1718.823s. Exact historical G10 parameters,
Adam state and source/FLOAT/S16 evaluations reproduced at all five milestones.
Independent verification checked51 snapshots,15 checkpoints and19 numeric
mirrors; plan SHA`59ce0177f627be4bcd8d4a04a5599d438aa2f870f86e1fafab05a72297136099`.

Candidate NMSE: G10 .330445, stride4 control .379037, full-frequency .362408.
Full-frequency is4.39% better than its paired control in the sum, but backing
is worse: .505203/.656199 versus .384971/.623625. Lead-only candidate energy
is .016715/.051261 versus .015463/.051184 (G10 .011724/.042006). Neither G13
arm is promoted. This result does not imply frequency preservation is generally
useless: the complete narrow architecture failed to beat the historical bridge.

### Actual recurrent-history diagnosis

`analyze_source_history.py` measures trained GRU dependence at three fixed TRAIN
frames375/749/1124. Original local and phase features remain fixed; only the GRU
history is restarted from zero for the last1/4/16/64/128/375 encoded frames.
Actual full-forward decoder replay and complete-prefix GRU controls isolate
diagnostic rounding/indexing errors. Report per-head and summed-candidate spectral
changes, not just hidden-state differences. Scattered spectral anchors are not a
rendered waveform, musical memory upper bound, role decision or new reset policy.

Four history tests pass across all five phase/factor/full-bin geometries, including
a local-only decoder whose output correctly ignores changed GRU state, complete
prefix controls, immutable parameters, hook cleanup and malformed inputs. The
combined phase/G11/G13/history run passed18 tests. Immutable54-file
`source-history-runtime-a` SHA
`fc5cc5cfd86c28e4d6db95ce9f3482c915502d6caf2aab27fd844c750b16beb8` ran diagnosis
`v16-moises-source-history-g11-a`, local control `source-history-g11-a`, former
Mac PID31553. All three arms completed in165.519s (status166.401s), below
1800s/3GiB guard. Original G11 source/FLOAT/S16 results exactly replayed, followed
by the entire diagnosis after checkpoint reload. Independent verification checked
53 snapshots,3 parent checkpoints and5 numeric mirrors; plan SHA
`5c30464c21879485738918e6643f238e06f940f33ca35d21022b780f05261c5a`.
No optimizer updates, new data or live-bundle edits were involved.

The hypothesis of effectively unused GRU history is NOT supported by these
anchors. For G10, retaining16 frames (42.67ms hop duration) changes summed
candidate spectra by0.009607/0.030455 energy relative to input;128 frames
(341.33ms) still changes0.001468/0.002809, and375 frames(1s)0.000507/0.000180.
Complete-prefix state replays are exact. G11 variants also retain meaningful
history; this is not proof of perceptual benefit, a global memory limit or a
reason to change recurrence cadence blindly. Next diagnose full-run TRAIN fit
separately from the fixed DEV generalization scores.

The same unchanged history audit also completed on G13, output
`v16-moises-source-history-g13-a` / local `source-history-g13-a`, former Mac
PID42900,149.312s. Original source/FLOAT/S16 and every history reload replay
passed. Independent verification checked53 snapshots,3 parent checkpoints and
5 numeric mirrors, plan SHA
`bf92c157d1c598c614052590188d26fa9799ba1ed10c65ceb31886dc5ac4f973`.
The historical G10 diagnostic reproduces its earlier numbers exactly. G13's
full-frequency candidate change at16 frames is .009378/.043170 relative input
energy; at375 frames .000267/.000791. It also uses past history; this does not
reverse the poorer backing reconstruction in the actual G13 training comparison.

Primary-source recheck: RT-STT uses1024/512 STFT at44.1kHz and reports GPU/FP16
timing, not Krait timing. Its six temporal LSTM blocks are not equivalent to our
single GRU; it also truncates frequency bins to384, so it is not a full-band
drop-in reference. These architectural differences motivate controlled studies,
not copying its published speed/quality into vehicle acceptance.
[Wu et al., §§III–V](https://arxiv.org/html/2511.13146v1).

### Full-run seen-TRAIN fit audit, completed

`evaluate_source_train_fit.py` selects only naturally exposed cores from the
completed full training schedule: earliest natural core per TRAIN artist, then
earliest missing material coverage up to three artists per family. No model
scores choose these examples. G8 selection is29 cores/29 existing caches from
all28 TRAIN artists. Material artists: lead16, backing13, foundation28, hats9,
drum body25, guitar25, pad4, keys13, strings3, FX3, unknown9. These are training-fit
diagnostics, not an independent29-example validation set.

The scorer now has distinct `evaluate` (validation-only) and
`evaluate_training_fit` (TRAIN-only) entry points sharing the unchanged numerical
body. Heldout/mixed partitions fail explicitly; no real data is relabelled.
Original cached common FLOAT gain is retained, without per-stem normalization;
recorded natural training levels can differ. All selected cache SHA/activity is
reverified. Two original DEV controls must exactly replay before TRAIN scoring;
then every TRAIN result is independently repeated after checkpoint reload.
Bad fit here would support underfitting of these seen cores, not by itself prove
whether data, capacity, objective or finite training exposure is the root cause.

G12 final seen-TRAIN audit also completed428.542s, former NAS PID106070,
`v16-moises-source-g12-train-fit-a`. Same29 exposed cores/all28 TRAIN artists,
exact original DEV controls and repeated TRAIN scores. Independent NAS binding
verified37 snapshots/parent checkpoint/3 mirrors, plan SHA
`ccdb2df6ac28e3f96032aab6efeb71c61008c2a109c8168c52702d275bec761b`.
Candidate NMSE .523931, backing .930061/gain .050255, pad .965579,
FX .968065; actual lead-only candidate energy .394429. Like G8, poor fit is
already present on seen training material, not only new songs. More exposure
remains a testable hypothesis, not a demonstrated cure or reason to promote G12.

Sixteen selected fit/full-training/reliable/supplement tests passed on Mac and
NAS. Immutable60-file NAS `runtime/source-train-fit-runtime-a` SHA
`fd2cdfcb07402dd8c6f13cfa88ffda226a536f9270105d4a9719df43556fc92a` passed full hash
verification before launch. Former NAS PID99833, output
`v16-moises-source-g8-train-fit-a`, control `runtime/source-g8-train-fit-a`,
started around07:51UTC, nice15/two BLAS threads/1800s/2GiB peak guard. Completed
all29 TRAIN cores and exact reload/replay in453.387s. Independent verification
checked37 run snapshots, parent checkpoint, recorded natural-exposure selection
and3 numeric mirrors; plan SHA
`ee13f41cff7c5c1ceb50155b181c49dbc2e669fba61719d6848f0cb02a96ea43`.
Candidate NMSE .539221, backing .930436/gain .050733, lead .846381, guitar .849114,
pad .964015 and FX .965216. Lead-only combined candidate energy .409082.
Foundation solo NMSE .030336 but natural-mixture .683134. This establishes poor
fit on these already seen training examples: failure is not only unseen-song
generalization. It does not yet distinguish optimization/finite exposure from
representation or labeling limits. G12 remains in its unchanged frozen runtime.
No new audio cache or optimizer update is made by this audit.

### Residual-output loss geometry, completed

The historical `unknown` residual couples all ten predicted source outputs, even
when true unknown is absent. `source_residual_geometry.py` computes the exact
quadratic curvature in those ten **source-output** coordinates:
`diag(1/scale_explicit) + (1/scale_residual) * ones(10,10)`, with the unchanged
actual spectral-core family energies and1% floor. A common positive factor is
irrelevant to its condition number. This is not the neural-parameter Hessian or
a measured learning-speed ratio. Five tests cover autograd equivalence, closure,
orthonormal zero-sum control, common gain and actual initialization/core scales.

`v16-moises-source-residual-geometry-a` completed on NAS in97.496s, former PID100060,
with no model loaded or optimizer update. All29 actually seen TRAIN cores were
SHA/activity verified, then reread/recomputed exactly. Immutable63-file runtime
`source-residual-geometry-runtime-b` SHA
`541637cad94845554392f1b63fc079d0639983adfc1b8ddc7ff98b4054b0d06e`
passed NAS source hashes and8 geometry/TRAIN-fit tests before launch. Independent
verification checked40 run snapshots,29 core bindings,2 mirrors and recomputed
analytic curvature/summary. Plan SHA
`5cc7aa7cefab7c4259d91746f3c7b943e5cd27e1e959a3be5d9d1c32e66e0ecc`.

Median condition number: unknown residual327.667, foundation25.875, lead54.333,
orthonormal zero-sum32.232. The median **paired** unknown/foundation ratio is
14.355 (range .851–31.546); foundation is not universally better. Mean initial
zero-explicit-head balanced loss is7.15356 for unknown residual versus .56457
for foundation. This motivates a controlled training test, not an automatic
architecture/quality approval.

### G14 residual-family comparison, completed, not promoted

`ResidualPhaseSourceHead` retains G10 phase inputs,100352 learned parameters,
616336 dense MACs/frame and the entire initial raw state. It predicts ten bounded
complex source heads with foundation as the unbounded residual, then maps every
output back to the original eleven-family public order. Unknown becomes a real
bounded head. Initial **outputs** differ (all-unknown versus all-foundation), as
does which source is unbounded. Per-arm box-ratio oracles record that difference.
The experiment does not safely relabel old weights; native export is forbidden.

`run_source_g14_residual.py` uses the same two TRAIN crops, unchanged balanced
eleven-family loss/1% floor, seed, alternating schedule, LR and clipping. Both
arms start fresh. The repeated G10 arm must reproduce historical weights, Adam
state and source/FLOAT/S16 results at each matching milestone. Actual foundation
and unknown solos supplement the unchanged lead/backing/guitar tests; every new
checkpoint must exactly replay all these results after reload. No DEV/heldout
selection, role approval or deployment. The dense budget excludes remapping and
is not measured target timing. Disk, wall and3GiB peak-RSS guards apply.

Sixteen focused G14/phase/geometry tests passed. Immutable66-file Mac runtime
`build/upmix-g-control/source-g14-runtime-a` SHA
`b3b318ff5b09ac76dcc868869d754f31a850fa22ec8fbe9afa73595c7f34780e`
was source-hash checked before four-step smoke launch, former PID61352, output
`v16-moises-source-g14-residual-smoke-a`, local control
`build/upmix-g-control/source-g14-residual-smoke-a`, nice10/two BLAS threads/1800s.
It completed173.914s; independent verification checked54 source snapshots,
2 checkpoints and6 numeric mirrors, plan SHA
`e5a8143923ade955f474f8aa9fd3499f7b89969728c7d609c97a41d535c96f58`.

An oracle-only follow-up preserves the exact public-order mixture reduction for
both arms, avoiding tiny FLOAT changes from summing reordered targets. New
regression checks exact common oracle input and unchanged G10 oracle. Immutable
66-file `source-g14-runtime-b` SHA
`dd9c4b19aef5ebd9a19c62258f90f9ccbfc92bb8030f26ee4c01ba84a5e66286`
passed12 focused tests in its packaged runtime. Its fresh smoke-b, former PID62434,
completed183.525s and54 snapshots/2 checkpoints/6 mirrors verified; plan SHA
`f25f05a9c0c30fd314fc6f5e8fb211c427852dcff58a7c385ebfb8e9ba00f9a1`.
Across smoke-a/b both arms have identical weights and source/FLOAT/S16/additional
solo scores; only oracle bookkeeping changed. Neither smoke is quality evidence.
Read-only mutation checks reject8 corruptions: public order, deployment flag,
duplicate arm, schedule, residual name, raw order, budget and checkpoint residual
label, without writing or replacing any artifact.

Main `v16-moises-source-g14-residual-a` started2026-09-13T08:38:45Z as Mac PID65490,
local control `build/upmix-g-control/source-g14-residual-a`, same immutable runtime-b,
two BLAS threads/nice10/10800s/3GiB peak guard. Both arms receive1024 updates, with
64/128/256/512/1024 exact reloads and matching historical G10 weight/Adam/source/
domain replay. These are now historical launch identities, not live pause targets.

At512 both arms have sealed/reloaded evaluations and historical G10 exact replay.
Candidate NMSE improves .368935 -> .278830 and backing .379435/.667959 ->
.328847/.603341, but lead-only candidate energy .013333/.051668 becomes
.012257/.332308; the first unknown-only case .109158 becomes .361306. This is
not a qualified protection improvement. Complete the fixed budget without
changing its runtime; do not select this arm on candidate sum alone.

G14 finished1254.340s; all five G10 historical parameter/Adam/source/domain checks
and both arms' reloads passed. Independent binding verification checked54 source
snapshots,10 checkpoints and14 mirrors; plan SHA
`8f035df82c1fc9e062895a5969351c5730236f55d7a5cbd7392b1eaa04e480d5`.
Final candidate NMSE .330445 -> .256138, backing .354644/.639156 ->
.325889/.600266, but lead-only candidate .011724/.042006 -> .010365/.438965,
foundation-only .024471/.047850 -> .084484/.098200, unknown-only first case
.106459 -> .339464. Do not promote this tradeoff on summed fidelity alone.

### G15 orthogonal-output comparison, completed, not promoted

`hssr_orthogonal_source.py` implements a fixed Helmert zero-sum basis for the ten
raw decoder coordinates. It keeps G10's complete initial saved state AND initial
all-unknown outputs. It has the same100352 learned parameters/616336 dense MACs,
with an additional fixed O(ten) basis transform explicitly excluded from that
dense count. Unknown remains the mixture remainder after physical outputs are
formed. Raw bounded coordinates are not named source masks; the feasible output
set differs and needs its own clipped-projection reference, not a claim of a
joint weighted-loss optimal oracle. No compatible native export exists.

Motivation: for a single active family with unchanged1% floors, a residual in a
different, absent family has source-output condition number108.19; an active
residual1.1, whereas the orthonormal basis gives10 regardless of which family is
active. This is exact output geometry, not proof about learned gradients,
source quality or a reason to remove clipping. G14's protected-solo regression
makes preserving the all-unknown start useful as a matched control.

The G15 entry point reuses the G14 paired harness with distinct policy/output
semantics, same source loss/data/seed/LR, G10 historical exact controls and all
protected-solo/domain/reload checks. Unit tests cover the analytic matrix,
gradient/isometry, identical initial outputs/state, closure, chunking, gain,
polarity, endpoints, future causality, feasible projection and export rejection.
The prototype passed18 focused G14/G15/phase tests, including in its immutable
71-file `source-g15-runtime-a`, SHA
`dabc17584278eab66a50fc4441fbdf4e568b6476bf73edfc47a297e69364f9db`.
Four-step `v16-moises-source-g15-orthogonal-smoke-a`, former Mac PID81293,
completed192.581s. Exact model reloads and independent58-snapshot/2-checkpoint/
6-mirror binding checks passed; plan SHA
`4f7cb7d6e15ac8f63182c8cd686737b8427d4b5cb85be394c801c2e604b1e6ff`.
Main `v16-moises-source-g15-orthogonal-a` started09:05:12UTC as former Mac PID82307,
control `build/upmix-g-control/source-g15-orthogonal-a`, same immutable runtime,
1024 steps per arm/two threads/nice10/10800s/3GiB peak guard. It completed1393.136s;
all five historical G10 weight/Adam/source/domain controls and both arms' saved
reloads passed. Independent binding verification checked58 snapshots,10 checkpoints
and14 mirrors; plan SHA
`46f106849e13be204c40ab013b0391fe0ba4ba624945268f116729fa0683a604`.
Final candidate NMSE .330445 -> .358389 (8.46% worse); backing .354644/.639156 ->
.351558/.642718, lead-only candidate energy .011724/.042006 -> .012344/.046980.
Foundation-only .024471/.047850 -> .027795/.046036 is mixed. Unknown-only guitar
case .106459 -> .429214 is not proof of vocal leakage (see caveat below).
S16 output sensitivity is small, but unchanged source architecture is not a
source-quality win from better analytic coordinate conditioning. No promotion.
Full201-test source host discovery passed
65.812s before the later listening-UI additions; this does not establish target
timing or learned quality.

### Unknown-test interpretation caveat, metadata verified

The sole active unknown-only input in the two-song micro-study comes from
Battlestar / Fixed, track `c82db6e6-c96e-435d-8721-9dacaa69d256`: a curated
`clean_electric_guitar` with `has_bleed:null`. The conservative mapper assigns it
to unknown because isolation was not declared, not because lead contamination
was acoustically demonstrated. Unknown-only candidate energy must therefore not
be described as direct vocal leakage. This does not approve its musical role,
purity or rear placement; the real lead-only regressions remain separate.

The SHA-bound catalog `ecac5c4d4477167456da372fb5ff095e45a8a76652cbe9d69e0922e46f44510d`
contains1954 tracks across181 eligible TRAIN songs:1781 declare no bleed,167
declare bleed,6 lack the flag (four clean-electric-guitar, one background-vocal,
one lead-male-singer track, across4 songs). Thus missing flags are not a proven
corpus-wide root cause. No labels/caches or active experiment were changed.

### Matched learned-source listening implementation

`render_source_listening.py` binds completed G14/G15 studies, reloads exact source/
FLOAT/S16/protected-solo evaluations, and writes PCM24 stereo excerpts with one
nonincreasing common gain across every output and input mode of each crop.
Every encoded WAV is reread and checked against the scaled FLOAT waveform, then
SHA-bound. No per-output normalization, cabin routing, SoundCheck, human votes or
placement approval. Song titles/intervals come from the same bound TRAIN catalog.
The ambiguous Fixed unknown-guitar test is explicitly identified in its label.

`source_review_server.py` is read-only and loopback-only; allowlisted media is
checked by SHA on every request, with bounded HTTP range support and no arbitrary
filesystem endpoint. The shared oracle HTTP handler accepts custom HTML without
changing the old page by default. Nine new-listening/existing-oracle tests pass.
Immutable63-file `source-listening-runtime-a` SHA
`c11ce4f942ec70b3cbb55a6e716ae7cc9c7fa67a2fd63d63f96d0cef85a52e9c`
was hash-checked before former Mac render PID85795, output
`v16-source-listening-g14-a`, local control `build/upmix-g-control/source-listening-g14-a`.
It completed191.034s,34 encoded/reread/SHA-bound WAVs, and serves loopback8872
as PID87127. A fresh combined render `v16-source-listening-g14-g15-a` completed
214.368s as former PID89635:43 WAVs with exact source/domain/solo replay of both
verified parents and weight-identical G10 deduplication. Both clips use common
gain1 across every model and input mode. Fixed/Battlestar61.00–63.989333s and
Enouement/Calculus185.00–187.989333s are TRAIN memorization cores, not unseen music.
Combined server PID90937 uses the same immutable runtime at
`http://127.0.0.1:8873`; Chrome shows all three variants. UI/media loading was
checked without starting playback; no listening/role/purity approval is implied.

At09:18UTC the existing NAS SSH password helper was denied authentication.
No credential/account configuration was changed. One separate key-only attempt
also failed; no repeated password probing. SMB remains readable/writable, and
G12 completed normally. At17:12UTC, after the user confirmed100.85.38.96, the
unchanged helper again authenticated as oem and authorized private execution
as luka succeeded. No credential/account changes were made; the transient cause
was not diagnosed. Linux3.12.3/Torch2.10.0+cpu/NumPy2.5.1, frozen hashes and
relevant tests were rechecked before new NAS jobs. No Mac numerical evaluation
was substituted for the original Linux trajectory.

### Full-training exposure/gradient-log audit, completed

`analyze_source_exposure.py` replays the exact4096-step G8 metadata schedule and
cross-checks each recorded recipe, gradient/clipping flag and learning rate.
No audio or checkpoint/model is loaded. Unique overlapping cached cores,
natural mixtures and actual solo updates are counted separately. Three tests
cover duplicate/missing/reordered logs, corrupted activity/LR/gradients, partition
and identity. Immutable68-file `source-exposure-runtime-a` SHA
`534f4aa26e92c8dd1bfd3312ddf15d86c8aa093c4a4172ae50b6851261b61c3a`
passed packaged tests and source hashes. Output `v16-moises-source-exposure-g8-a`
completed34.895s; independent verification replayed the metadata/event summary,
checked39 code snapshots and2 mirrors. Plan SHA
`ce436ffb1a7d6e63b52c202a3aab6c7d00fc965e5f5a2ddd73803ffb5ead5055`.

All572 windows/181 TRAIN songs/28 artists participated. There were2731 natural
updates over2197 of2860 eligible cached core offsets;1819 of those2197 appeared
once as a natural mixture (cores overlap, so these are not independent audio
samples or full-song coverage). Backing had1241 material natural updates across
993 cores/57 songs/19 artists plus86 explicit solo updates. FX216 natural updates
across110 cores/17 songs/5 artists plus85 solos. Lead had342 protected solos.
Global clipping occurred636/4096 times (15.53%),33.59% in the first quarter versus
8.30% in the last. Unknown solos clipped74.12%, drum-body solos67.06%; this does
not prove that a higher threshold would help. Finite exposure/optimization remain
open causes of weak TRAIN fit; do not call this a converged exhaustive training.
The event log is SHA-bound now, not a historically authenticated gradient trace;
waveform activity is not the exact spectral loss scale or per-parameter gradient.

The same immutable audit completed for G12 in35.436s through SMB, output
`v16-moises-source-exposure-g12-a`. Independent metadata/event-summary replay
checked39 snapshots and2 mirrors; plan SHA
`5f3f1eefcc965d755c7a07fe497729a0f7fef11f2d32d52b3c49212b76494efa`.
Natural/source exposure is identical to G8 by construction. G12 clipped649/4096
updates (15.84%),33.40% in the first quarter versus8.30% in the last; unknown
solos clipped135/170 (79.41%). This does not justify silently changing clipping,
labels or claiming convergence after roughly one visit to most cached cores.

### G16 unchanged-model training-budget continuation, completed, not promoted

`run_source_g16_budget.py` isolates additional optimizer updates on unchanged
G10 phase architecture, same two natural TRAIN crops, loss/1% floors, alternating
selection, clipping5 and absolute LR (.001 through512/.0003 afterward). It restores
complete model state and Adam moments/counters/options; no fresh optimizer, new
warmup, new seed, label change, loss penalty or recurrent-state carry across crops.
CPU Adam moment storage is deep-copied on restoration so separate restored
optimizers cannot accidentally share mutable tensors. Fixed phase rotation,
all tensor shapes/dtypes/finiteness and exact per-parameter Adam steps are checked.

Mandatory historical64->128 resume-smoke must exactly reproduce G10 weights,
Adam and source/FLOAT/S16 metrics, including initial64 replay; its checkpoint
also exactly reloads added foundation/unknown solos. Main1024->8192 is prohibited
without a completed independently verified smoke using the same code/parent.
Main checkpoints2048/4096/8192 preserve the original absolute learning-rate
schedule, and each must reload model/optimizer/source/domain/solo results exactly.
No DEV/heldout audio or optimizer-based placement permission; continued TRAIN
memorization does not establish full-corpus convergence or useful rear purity.

Immutable73-file `build/upmix-g-control/source-g16-runtime-a` SHA
`459dbaba7d2160f6641e6e0c0396abff81538f5567f54fc27459aaf10edbd722`
passed all hashes and10 packaged continuation/phase tests. Full source discovery
passed209 tests in61.895s, including native host/sanitized fixtures but no new
QNX guest/target timing. Resume-smoke `v16-moises-source-g16-resume-smoke-a`,
former Mac PID91660, completed186.253s, control
`build/upmix-g-control/source-g16-resume-smoke-a`, nice10/two threads/1800s/3GiB.
Initial64 and resumed128 exactly reproduced historical G10 model/Adam/source/
domain. Independent verification checked60 code snapshots,1 checkpoint and5
numeric mirrors and directly rechecked historical parameter/Adam equality;
plan SHA `ecd273cfe40b202760f9a7f9664350ae56f5642e41d4550fbc02805c110aaaa0`.
Eight read-only policy mutations were rejected (arm, spatial floor, deployment,
threads, clipping, stop step, sample order, parent identity); no artifacts changed.

Main `v16-moises-source-g16-budget-a` started2026-09-13T09:51:02Z as Mac PID92669,
same immutable runtime, control `build/upmix-g-control/source-g16-budget-a`, log
`build/upmix-g-control/source-g16-budget-launch-a.log`. It restores original G10
step1024, with7168 new updates through8192, two threads/nice10/10800s/3GiB peak
guard, rather than starting over or extending the LR half-point. It completed
3240.292s; PID92669 is historical, not a live pause target. All three model/Adam/
source/FLOAT/S16/additional-solo reloads passed. Independent binding verified60
snapshots,3 checkpoints and7 numeric mirrors; plan SHA
`a62aff499f1b7324bd2b6a3e467e4425faa0ec46288d9ff3e8931aa4c097e353`.
Final8192 checkpoint SHA
`b588c4f01ff381568674c82d47f2b796d649d759f1610217d485eaa8f5724d05`.
Candidate NMSE .330445(at1024) -> .296001(2048) -> .269897(4096) -> .236855(8192),
28.32% lower than1024. Backing improves .354644/.639156 -> .304876/.582141,
but fitted gains remain .619970/.369692 and lead-only candidate worsens
.011724/.042006 -> .017983/.049381. Foundation .024471/.047850 -> .019234/.073070
is mixed; unknown-only guitar .106459 -> .080179 is not a vocal-purity result.
Longer training helps this two-crop fit, but does not qualify source/protection
or establish full-corpus convergence. No main checkpoint is promoted.

### G17 full-TRAIN unchanged-model continuation, staged sequence active

`train_source_g17_continue.py` isolates training budget on completed G12, not
G16's two-song phase model. Same572 windows/181 TRAIN songs, labels,11-family
balanced complex loss/1% floor, width8/GRU64 reliable1e-4/subpixel geometry,
original source-order/common gains/solos/curriculum and Adam/clipping5. The full
extended schedule must preserve the entire original4096-step recipe prefix.
Absolute LR retains the original32-step warmup and2048 transition to .0003;
changing the stop to8192 cannot move that transition or reset optimizer moments.
Model/Adam shapes/dtypes/finiteness/counters/options and independent moment
storage are checked; the original TRAIN constant calibration must reproduce.

Historical qualification restores G12 step512, exactly replays all80 original
DEV scores, performs the unchanged512 next updates, and requires exact historical
step1024 weights/Adam/all80 DEV scores plus independent checkpoint reload.
It has a7200s wall bound, not a new quality experiment. Main cannot begin without
a completed independently verified qualification with identical code/parent.
Main restores original G12 step4096 (not qualification weights), adds4096 updates
through8192, and scores/reloads all80 fixed DEV windows at6144/8192. No heldout
or role selection/approval. A new verifier checks parent/code/checkpoint/mirrors,
the complete schedule against TRAIN metadata and historical weights/Adam/DEV
bindings, without misrepresenting binding checks as fresh numerical replay.

`supervise_source_nas.py` preserves its old contracts and shared `.source-g8-nas.lock`,
adding only explicit G17 qualification/main configurations. Owner-only Linux,
two BLAS/OMP/MKL threads, nice10,4GiB RSS/1GiB available-memory and8GiB data-space
guards remain; main wall bound21600s. Existing G8/G12 runtimes remain untouched.
`sequence_source_g17.py` runs exactly four stages: qualification, independent
verification, main, independent verification. Nonzero exit/hash mismatch prevents
the next stage; no automatic model selection, parameter deployment or cleanup.
The sequence has an independent owner lock, per-stage logs and32400s global bound.

Final immutable79-file Mac `build/upmix-g-control/source-g17-runtime-b` and NAS
`runtime/source-g17-linux-bundle-b` SHA
`3efbf8e42ca9fa5827a9273ebfaf7977055bbe8e54a0276315b68813d04fa674`
were source-hash checked. Sixteen focused tests passed on Mac and NAS; full215
source tests passed59.295s on Mac. Earlier unused runtime-a is preserved.
Sequence launched2026-09-13 around17:30UTC as NAS queue PID106182, control
`runtime/source-g17-sequence-a`, outer log `runtime/source-g17-sequence-a-launch.log`.
Its current stage/process identities must be read from queue status and the
active stage's supervision plan, not inferred from old PIDs.
Initial supervision started17:30:26UTC as PID106183 / training child106184.
At17:32UTC direct SSH status confirmed481/572 TRAIN windows verified and normal
resource guards. No qualification optimizer result was yet available. Some live
SMB status reads returned stale content/trailing parse errors while direct NAS
JSON reads were valid; prefer direct SSH for rapidly replaced status files.

Qualification output `v16-moises-source-g17-resume-smoke-a`, control
`runtime/source-g17-resume-smoke-a`, supervision
`runtime/source-g17-resume-smoke-a-supervision`. Main uses `v16-moises-source-g17-budget-a`,
`runtime/source-g17-budget-a` and `runtime/source-g17-budget-a-supervision`.
On user pause, create `runtime/source-g17-sequence-a/pause.requested` to forbid
later stage starts, then identity-check and stop queue first, current supervisor
and its training child. The marker does not pause an already active worker.
Before resume inspect live identities, guards and elapsed wall budgets; paused
time is not excluded. Do not imply indefinite resumability or remove data.

### Synthetic full-length CPU/MPS training cost probe

`benchmark_source_training.py` performs forward/backward without optimizer updates
on synthetic1497-frame/four-second inputs. No actual dataset was loaded or staged
onto the Mac. `source-training-throughput-a` records CPU2-thread per-item warm
times .392s(batch1), .377s(batch2), .360s(batch4). Batch4 reached4.216GB peak RSS,
exceeding the3GiB guard, so that process stopped before MPS; its partial records
remain, and it is not a completed cross-device benchmark. The initial status
file did not record the exception; terminal output and missing final report
establish failure. Subsequent code writes failure status and guards after each
pass, with batch sizes limited to1/2.

Separate fresh `source-training-throughput-mps-b` completed: .842s per item at
batch1 and .429s at batch2, MPS driver allocations1.132GB/2.246GB. All weights
remained unchanged. These tested small GPU batches do not beat CPU; this is not
a claim about every GPU/batch/model, real-data I/O, optimizer throughput or backend
numerical parity. Do not enlarge local dataset storage on that evidence.

## Native source ABI implementation and current evidence

`source_ops.c/.h`, `source_g5_native.c/.h` implement the fixed G5/G8 FLOAT frame
path: six invariant stereo features, strided frequency encoder, dense layers,
PyTorch reset-after GRU, full-bin subpixel complex masks and exact `unknown`
residual. `export_source_g5.py` emits a separate400896-byte `SG5F3201` research
blob, with exact dimensions/length/CRC/finite checks; never reuse HSSRNEO1.
Work storage is58392 bytes plus explicit state/source outputs. Frame sequence and
stream generation must match; any numeric failure clears source outputs and does
not commit state. A caller still must retain dry fronts and close rear on error.
No production callsite or source-aware renderer is claimed here.

Sixteen Python tests exercise scalar/NEON/fast-tanh operator variants, full
network/state/reset parity, missing frames/epochs, malformed weights, NaN/Inf,
subpixel endpoints, gain/polarity and a dense490006-point tanh test. Fast compiler
math remains forbidden. The optional NEON tanh approximation is separately
bounded/tested and is not a quality/promotion decision.

`verify_source_g5_native.py` binds the real G5 step1024 checkpoint and128 real
music STFT frames. Host ASan/UBSan (LeakSanitizer unavailable on this Mac) and an
isolated QNX/ARM guest both passed. First exact implementation:6.967 million
calibrated QEMU instructions/frame. IEEE bitwise finite checks, NEON dense dots
and register-resident convolution accumulation reduced this to3.791 million,
with QNX relative source MSE7.44e-14 and maximum state error5.96e-7.
The next same-source exact/fast-tanh pair measured3.913/3.310 million respectively;
fast source MSE1.26e-13, state error unchanged. Its exact variant regressed versus
the previous3.791M version, so keep profiling rather than calling every edit an
improvement. Artifacts live in local `source-g5-native-1024-*` directories;
failed initial sanitizer configuration and prior versions remain preserved.
Further safe kernel3 NEON unrolling (`fast3-a`) reaches2.778 million calibrated
instructions/frame, source MSE1.256e-13, max source error1.049e-5 / state5.96e-7.

These are **instruction counts**, not Krait cycles or measured audio deadlines;
FFT/WOLA/renderer/callback cost is excluded. More runtime optimization and full
streaming tests remain. Read-only TCP check of the known head-unit address on
2026-09-12 timed out; nothing was installed or restarted. Continue local work until
actual target access is the remaining dependency, not as a reason to stop early.

### Raw-audio streaming implementation

`source_stft.c/.h` adds packed-stereo FFT512, full257 bins and incremental
11-source WOLA. `source_g5_audio.c/.h` accepts interleaved FLOAT chunks of1..128
samples (split larger callbacks), and emits0 or128 planar source samples without
chunk-boundary loss. First output requires512 observed samples; metadata retains
the original input timeline. Playback delay/dry alignment are not silently inferred.
Finite-file end returns only384 covered tail samples and explicitly reports any
uncovered0..127 trailing input (or the whole input when shorter than512).

All source/recurrent/WOLA/input state rolls back on numeric failure. Wrong rate,
sample index or generation is rejected; live FLUSH resets the entire source state
instead of emitting an old tail. Caller must preserve dry fronts/close rear on
error and during source warmup. Family outputs are not speaker channels.

Twenty-six host tests pass:11 operators,5 frame-model,4 FFT/WOLA and6 integrated
streaming. They cover exact replay under1-sample/irregular chunks, missing epochs,
44100Hz rejection, short files, silence/gain/polarity, invalid aliases, NaN and
late-WOLA failures. A trained-checkpoint whole-waveform host/QNX verifier is being
qualified separately. Its first host pass met waveform/closure bounds but exposed
~1.07e-4 hidden-state drift once the real native FFT was included; diagnosis is
recorded above. The failed legacy whole-chain artifact remains preserved and
must not be described as a QNX/ARM success.

### Explicit reliable ABI and whole-stream QNX/ARM check

Legacy `SG5F3201` export/math/bytes remain unchanged. Reliable models require
`SG5R3201`, featurekind2 and their positive FLOAT spatial floor at byte68. Its
CRC binds header and payload (excluding the CRC field); reserved bytes must be
zero. Both ABIs retain400896-byte models. Recurrent state binds the model CRC,
so a midstream model/feature change requires reset even before the next FFT.
CRC prevents accidental mixups, not malicious substitution or hash collisions.

`build/upmix-g-control/source-g9-audio-64-fast-a/report.json` binds the actual
G9 floor1e-4 step64 checkpoint. Host ASan/UBSan and isolated QNX/ARM both pass
384 raw music frames: QNX waveform relative MSE3.3465e-13, maximum waveform
error5.96e-6, maximum state error3.455e-7, exact chunk/reset replay and atomic
failure rollback. Existing bounds were not relaxed. State/work/tables219972B,
model400896B; mean4.1535M estimated instructions/frame includes FFT/model/WOLA,
but excludes renderer/SoundCheck/PCM callback. These are not measured target
cycles/deadlines, a quality approval or a production integration.

The combined host regression now passes38 tests:11 operators,5 frame models,
4 FFT/WOLA,6 integrated streaming,4 conditioning,3 reliable ABI/model switching,
2 final-guard native/sanitized chains and3 development-core audits.

### Coupled front/rear veto hardening

`source_g2_guard.c` now rejects whole/partial front-removal/PCM overlap, invalid
gain configuration/state and nonfinite subtraction results. It computes both
front results before committing either: finite operands can overflow. Failure
preserves supplied dry fronts, closes rear immediately and restarts only the
reopen ramp. Already nonfinite dry input is not repaired here. Adjacent disjoint
buffers remain legal. Tests cover both overflow channels, reverse/trailing
overlap, stale/expired decisions, exhausted counters and chunk invariance through
the real once-only SoundCheck/matrix/S16 chain. No production callsite is added.

### Source-aligned PCM adapter, host and QNX/ARM fixture-qualified

`source_g5_route.c/.h` joins the real source stream to interleaved stereo S16
ingress and FL/FR/FC/LFE/SL/SR S16 egress, with explicit512-sample delay and a
48000-original-sample source warmup. Fixed generation-bound family weights may
request only the eight candidate families; lead/foundation/unknown weights are
hard forbidden. Requested family identity is NOT placement/purity evidence.
The final decision must approve every requested family on the ORIGINAL input
timeline; missing/stale evidence keeps delayed dry fronts and zero rear.

The same selected source is removed at its original front position and added
to rear with the same gate; no steering, ambience, EQ or bass assist is introduced
here. C/LFE remain0. Numeric, queue or model-CRC failure closes rear for the whole
callback, preserves delayed dry music, and latches source failure until reset.
Finite-end drain emits the remaining512 delayed samples dry-only without feeding
invented future frames into the model. Live FLUSH discards queues via generation
reset; caller must also handle metadata/discontinuity and the512 media-clock delay.

`build/upmix-g-control/source-route-native-d/report.json`: host ASan/UBSan and
isolated QNX/ARM pass the complete FFT/model/WOLA/alignment/final-guard/real
SoundCheck/known-matrix/S16 test. Fixtures have authored constant quarter-mixture
backing masks, NOT natural-music purity labels. Tests cover exact128/irregular/
single-sample chunk equivalence, warmup, abstention/positive utility, forbidden
families, aliasing, failure/CRC changes, FLUSH, short streams and full dry drain.
The first guest compile failed on QNX header include order; `<stddef.h>` before
`<stdint.h>` in the guard header resolves it. Failed `source-route-native-a` stays.

The adapter requires the48kHz SoundCheck gain stage and sample-peak limiter to
remain enabled; absent normalization metadata means unity gain with protection,
not disabling the stage. Full-scale input with+6dB metadata passes linked PCM
and all eight known input-matrix rails. This does not establish acoustic,
excursion or true-peak safety. `sg5_route_init` initializes fresh storage once;
live `sg5_route_reset` requires a strictly newer nonexhausted generation, so an
old permission interval cannot revive after FLUSH. Same/older generations are
rejected without mutation. The `native-c` limiter proof and earlier results remain.

All native compilation uses private snapshots including actual production
SoundCheck/matrix code. Latest binary SHA starts`89f557d073f2eddc`; full proof
binds plan`837d36e949d06b533dd850b45f385fbda3e6176b9f27bb2c1b08e72273eeaa1f`.
This check does NOT
measure timing or integrate the production media callback. The current combined
regression passed68 tests (previous62 +5 reliable-full +1 supervisor policy).
Later focused G13/phase/capacity and supplemental/audit runs also passed;
overlapping runs are not added together as new unique tests.
Full `test_source_*.py` host discovery subsequently passed181 tests in61.104s,
including the native/sanitized fixtures. This predates the new residual-geometry
and G14 tests; it did not launch a new QNX guest or establish car timing/quality.
Actual target deadlines, learned source/role quality, final heldout and listening
remain open. A default-closed prototype is not a useful finished upmixer.

## Current decision boundary

G6 completed and was not promoted: the long invariant window improved candidate
sum NMSE only0.73% versus its same-size short control, while both backing crops
worsened. See [G6](UPMIX_V16_G6_PAST_CONTEXT.md) and
[whole-chain G5 audit](../audio/UPMIX_TRAINING_HISTORY.md). At1024 updates the extended
G5 control reaches complex loss0.178388 / candidate waveform NMSE0.330300,
backing NMSE0.354306 and0.634725 on its two TRAIN crops. It is still not a qualified
separator. Completed G7 did not beat G5 at matched milestones and is not promoted.
G8 expands actual-source coverage/optimization
and development evidence while native work proceeds, without automatic promotion.
