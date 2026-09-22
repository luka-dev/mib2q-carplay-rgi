# Whole-chain reassessment and G5 learnability gate

2026-09-12. Scope: repository source, current cached development audio/metadata,
completed G2/G3/G4 reports, native rendering contracts and static cabin evidence.
No new vehicle readback, acoustic measurement, entire-corpus re-listening,
source rehash/re-download, firmware changes or deployment. Preserve all results.

Implementation follow-up: [G6 matched past-context experiment](UPMIX_V16_G6_PAST_CONTEXT.md)
tests the next representation branch. G5's historical measurements and frozen
code remain unchanged.

## Decision

Do not keep extending the G3/G4 protection-weight experiments. First establish
that an estimator can reconstruct a tiny real-music TRAIN subset; separate
learnability from role supervision, generalization and downstream rendering.
The new G5 test is not a trained upmix candidate or an automatic architecture
selector. The full corpus still needs a qualified learning pipeline before a
larger run is justified.

## Chain audit

| Stage | Verified now / boundary | Action |
|---|---|---|
| Source provenance | Existing FLOAT reader uses common native origin/resampling/gain; hashes, cached activity and artist partitions are bound. This is construction integrity, not proof of every musical label. | Reuse verified caches; no wholesale teacher rebuild or per-stem loudness normalization. |
| Source family vs role | G2 pending inventory has 502 interval/source items and zero `role_confirmed` entries. Metadata `background_vocals` is experimental identity, not a human placement vote. An aggregated guitar head can contain both primary and secondary parts. | Keep source reconstruction and musical placement separate; all contributors need supported interval evidence before family-level rear permission. Inventory counts do not inspect any separate new review files. |
| Effective training exposure | G4 natural-mixture scored-core union is 231.584 seconds from 33 songs, despite 40 available TRAIN cache windows. Repeated crops/solos do not add independent songs. | Stop describing these pilots as full-corpus training. Expand only after learnability and label gates. |
| Input/architecture | FFT512 at 48 kHz = 10.67 ms and 93.75 Hz bin spacing; stride-4 encoder compresses six features into width4/8 with one GRU32/64. Subpixel output does not remove input compression. | Test capacity and representation separately. Longer past-only spectral context remains an untested next branch, not a presumed cure. |
| Learning objective | G3 reduces actual protected-solo output while attenuating useful sources. G4 solo positives raise output but worsen natural-mixture fidelity and protected-solo rejection. | Suspend those loss-weight changes; test reconstruction-only music memorization before further curriculum design. |
| Evaluation | Reference/WOLA calibration and checkpoint replays pass. Fixed-mask contribution energy is not uniquely identifiable audible leakage. Repeated use of eight validation songs is development, not an unbiased final score. | Keep actual-output, gain/shape, per-family/quiet-source, polarity/level and solo tests separate. G5 evaluates TRAIN only; heldout stays closed. |
| Runtime integration | G2–G5 are Python source-estimator prototypes, not the shipping 32-band controller ABI. Host C veto is not a source estimator or learned role/risk detector. | C/operator/export parity, state/reset/FLUSH and measured QNX deadlines are still required after an estimator qualifies. |
| Rendering | Current `audio_hook.c` has opt-in float synthesis → one Sound Check → linked matrix/sample rails → S16. Cabin/quiet-LFE config forces float mode; flags in the live car were not read. Cabin rendering separates source-aligned subtraction and rear signal. | Preserve these paths and test them; do not repeat the old S16-order diagnosis as current universal behavior. Legacy mode still exists. |
| Cabin/hardware | OFF maps to HU level0, not a whole-DSP bypass. Six PCM channels are not 19 independent drivers; rear-door woofer/tweeter share one circuit. Recovered input matrix is not a measured seat transfer. | Keep C initially0, quiet48-Hz bass assist/no new bass boost, no inverse acoustic EQ or independent tweeter routing. |

Current native tests pass: Sound Check, float render and stereo upmix. A stale
`scripts/test_hook_audio.sh` invocation lacked `SPA_STEREO_UPMIX_TESTING` required
by the shelf test; added the same macro already used by `scripts/run_tests.sh`.
This is a harness compile fix, not a vehicle audio change. The reviewed native
paths do not establish intersample-peak/excursion protection or perceived quality.

### Coupled front-removal / rear-veto correction

Integration hazard: a final rear-only veto cannot restore a source already
subtracted upstream from FL/FR. Closing rear after that subtraction could remove
useful music from both destinations. This was not reproduced as an installed
vehicle defect: the G source-estimator/guard chain is not deployed.

Added host-only `g2_guard_apply_objects` / `g2_guard_postprocess_objects` APIs.
They require dry/source-preserved fronts and a separate source-aligned stereo
removal buffer (same S16-scaled float units, non-aliasing). The same admitted
gate gain controls rear output and front removal. Missing/stale decisions,
expired intervals, missing/nonfinite removal keep dry fronts and close rear.
Ambience/steered rear audio must never be supplied as the removal buffer.
Sound Check and linked matrix/sample limiting still follow once; those stages
can apply a common gain, so preservation here refers to the pre-gain dry signal.

Existing rear-only APIs keep their behavior for compatibility; callers that
subtract beforehand do not gain this protection automatically. New tests cover
matched reopening, interval expiry, missing/invalid removal, reset, and exact
whole/chunked output including real Sound Check and known-matrix limiting.
Native tests and AddressSanitizer/UndefinedBehaviorSanitizer pass. No production
hook call site or vehicle configuration was changed.

## Completed G4 disposition

G4 completed 300 updates in 1406.76 s; all three checkpoint hashes and local/NAS
report mirrors verified. Final candidate NMSE 1.12027 versus G3-1x 0.71225;
actual isolated-lead candidate/input energy 26.770% versus 4.286%. Backing
material NMSE 0.89249 does not compensate for overall failure. Reject this
bounded intervention for promotion; do not infer that positive examples in
general are useless or that one architecture can never learn.

## G5: two widths × two input representations

`run_source_g5_learnability.py` selects the first two distinct TRAIN artists with
material lead and backing in a scanned four-second crop. Selection is based on
metadata/activity, not model failure or validation. It alternates the exact two
mixtures, with one-second burn-in and final512 samples excluded. No new audio
cache, remix, solo training, or validation/heldout evaluation.

Four arms: width4/GRU32 and width8/GRU64, each with:

1. Historical six-channel input: normalized real/imaginary stereo plus log power.
2. Optional `input_features='invariant'`: log L/R relative powers, normalized
   L/R power difference, weighted real/imaginary cross-spectrum, log mean power.
   It keeps relative stereo phase but discards common phase. The six-channel
   dimensions and parameter schema are identical; no magnitude-only output is
   imposed, and output masks remain complex at every bin.

The default remains `complex`; an existing G2 checkpoint's validation record
reproduced exactly after the optional input branch was added. Historical runners
that require source-code hashes must use their own frozen code snapshots; no
old manifest is updated to impersonate the new source revision.

On one diagnostic TRAIN mixture, joint L/R polarity reversal produced relative
output-equivariance MSE 0.004944 in G2 and 0.008407 in G4. This is a robustness
weakness, not the established main cause of their failure. The invariant branch
enforces this symmetry in its input representation; tests also check common
STFT-phase rotations, gain, silence, endpoints, causality and state chunking.
Removing common phase may discard useful information: quality is an experiment.

Each arm starts fresh, deterministic seed 20260917; identical full initial state
within each width. Adam LR 0.001, clip 5, 256 updates per arm, balanced complex
reconstruction only with the existing 1% quiet-family energy floor. Checkpoints
64/128/256. These changes versus G4 define a memorization diagnostic, not a
fair full-corpus G4 successor benchmark. A finite-budget failure is not proof of
mathematical incapacity; a success is not proof of unseen-song separation.

### Calibration and acceptance of the experiment

- Perfect references must give zero complex loss.
- Box-clipped ratio oracle measures available output-format headroom, not a
  realizable causal separator. On the two selected TRAIN crops, complex losses
  are 0.00305 and 0.00536.
- A TRAIN-fitted time-constant per-frequency complex mask is a stronger
  no-temporal-separation reference than just a scalar mixture split. Its losses
  are 0.33746 and 0.28907; fitting it uses only these two TRAIN mixtures.
- Zero-candidate/all-residual baseline, exact reconstruction, natural candidate
  fidelity, protected/useful solos, polarity and -12 dB robustness are reported.
- Compare learning curves and per-song/family results, not just aggregate loss.
  No fixed checkpoint is automatically chosen or exported.

If even music memorization stays near static filters, inspect optimization and
time/frequency representation before larger corpus training. If it succeeds,
next test generalization on broader TRAIN coverage and sealed development checks,
then revisit protected hard examples and musical-role supervision. Only after
those pass: role-aware front/removal/rear integration, target C parity/timing,
frozen heldout, level-matched listening, and authorized in-car acceptance.

## External cross-check

Modern low-latency separation is not evidence that any small model fits our head
unit. RT-STT reports a 383k-parameter causal model, but its 1.01 ms result uses
FP16/TensorRT on an RTX 3080 Ti; those numbers are not QNX/Krait timing. Its
four-stem task also does not solve lead/backing or primary/secondary placement.
It is a useful later architecture reference, not a drop-in solution or a reason
to discard useful high-frequency content to copy a benchmark configuration.
[Wu et al., RT-STT, §§III–V](https://arxiv.org/html/2511.13146v1).

Band-SCNet provides another causal multiscale/bandwise architecture reference;
its published task/quality claims do not establish our device runtime or musical
role policy. No external code or weights were downloaded for this iteration.
[Yang et al., Band-SCNet](https://www.isca-archive.org/interspeech_2025/yang25d_interspeech.html).

## Verification and artifacts

### G5 terminal outcome

All four 256-update arms completed in 461.32 s (7 min 41 s). All twelve
checkpoint hashes, plan binding and every local/NAS numeric JSON mirror passed.
PID 80672 has exited; no next training run was launched automatically.

| TRAIN-only final arm | Complex reconstruction loss ↓ | Combined candidate waveform NMSE ↓ |
|---|---:|---:|
| width4 / GRU32 / complex input | 0.29097 | 0.60008 |
| width4 / GRU32 / invariant input | 0.27290 | 0.58317 |
| width8 / GRU64 / complex input | 0.25724 | 0.56290 |
| width8 / GRU64 / invariant input | 0.23841 | 0.50028 |
| TRAIN-fitted frequency-constant reference | 0.31327 | 0.59147 |

This run supports a limited conclusion: capacity and input representation both
help these two TRAIN mixtures. Invariant inputs give zero measured polarity
equivariance error here (the legacy inputs are around 0.004). Even the strongest
TRAIN result has backing waveform NMSE 0.53946 / 0.73513, and remains far from
the box-ratio output-format calibration. No architecture is qualified for broad
music, musical placement or deployment from this experiment.

Next direction: retain both complex/invariant controls, test a richer past-only
time/frequency representation (for example an additional longer-resolution
analysis branch) with an explicit compute budget, then broaden TRAIN coverage
and independent development evaluation. Do not replace this with another G4
gain/negative-weight sweep. Any final model still needs source/role separation,
coupled front/rear rendering, C parity/QNX timing, frozen heldout and listening.

108 focused model/data/loss/guard tests, 11 virtual-cabin tests and 4 static
HU-control tests passed; three additional native audio test binaries passed.
G5 four-step integration completed all four arms in 36.79 s, with checkpoint
hashes and numeric mirrors verified. Independent reload of all four checkpoints
reproduced every numerical evaluation exactly. These are correctness checks,
not quality.

Integration: `build/reference-audio/v16-moises-source-g5-smoke-a` and local
`build/upmix-g-control/source-g5-smoke-a`.
Main launched 2026-09-12: `v16-moises-source-g5-learnability-a` /
`source-g5-learnability-a`, PID 80672 (historical launch identifier only).
Actual optimizer progress was verified at first arm step 69/256; log is
`build/upmix-g-control/source-g5-learnability-a.log`.
Weights/code on private NAS, local numeric mirror; 22 GiB Mac / 8 GiB NAS reserves,
7200 s main budget. No cleanup/retry or automatic promotion. Consult actual
`status.json`, process and terminal `complete: true` before restarting anything.
