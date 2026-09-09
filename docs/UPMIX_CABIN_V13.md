# V13: restrained bass assist and source-aware cabin renderer

2026-09-08. Local implementation and review bundles only. Nothing installed in
the vehicle. Luka reports that the current vehicle bass overwhelms the song;
an older assist was subjectively better. The last recorded vehicle snapshot is
LP48/V11, not a new live readback. We have not established the acoustic cause.

## Immediate bass correction, independent of new spatial processing

`AUDIO_QUIET_LFE=1` selects the quiet assist and implies float rendering. Keep
`AUDIO_CABIN_RENDER=0` to retain the existing spatial policy while comparing bass.
The offline equivalent is `--quiet-lfe --lfe assist`.

- Keep the existing 48 Hz fourth-order Linkwitz–Riley filters, then use a fixed
  mono sum. No anti-phase recovery, semantic/dynamic bass boost or new crossover.
- At the default LFE gain, an equal-phase low-frequency signal has a 0.0625
  pre-filter asymptotic gain, versus the old nominal cap of 0.125. At 40 Hz the
  measured settled amplitude is approximately halved. Opposite-phase L/R bass
  does not generate extra mono LFE.
- Remove the assist-associated broadband front trim: front gain becomes
  32670/32768 instead of 31392/32768. Do not turn down the entire song to control
  the auxiliary bass feed. Linked matrix/rail limiting still applies.
- `AUDIO_STEREO_LFE=off` continues to make our LFE channel exactly zero. It does
  **not** promise that the physical sub is silent: factory bass routing remains.

This is not a byte-for-byte restoration of the oldest assist. The older V11
assist used a mono 80 Hz filter and a higher nominal gain (~0.178), so the user's
feedback cannot be explained simply as “the new number is larger.” Bandwidth,
phase recovery, dynamics, front subtraction and downstream DSP all matter.

## Experimental full cabin policy

`AUDIO_CABIN_RENDER=1` also implies float rendering and quiet LFE. It is a
separate listening candidate, not a promoted separation model.

| Operation | Legacy policy | Cabin policy |
|---|---|---|
| Front subtraction | 2.5 × the rear image, including steering/bed | 1.0 × the original-position source estimate |
| Synthetic rear floor | Phase-rotated fallback plus adaptive makeup up to 3.5× | Disabled |
| Spatial rescue | Can bypass low learned rearability | Cannot exceed the learned band mask; vocal/bass safety also applies |
| Output steering | Strong left/right steering | Moderate, confidence-dependent; zero pan stays centred |
| Spectral mask ceiling | 1.45 | 1.0 |
| Rear high shelf | +3 dB around 4 kHz | Bypassed |

Extraction and placement now have separate WOLA buffers. We subtract only the
estimated source in its original stereo image, never the decorrelated ambience
or the moved rear image. This is a conservative partial transfer, **not** an
inverse of the factory matrix or proof of exact preservation of every front bus.
Two extra inverse FFTs are needed per hop. Analysis still consumes original S16
PCM; Sound Check is uniformly applied after synthesis, followed by the linked
−1 dBFS sample-peak rail/known-matrix guard and one S16 conversion.

Default flags remain off to preserve the existing installation and allow an
exact reference comparison. Setting only `AUDIO_FLOAT_RENDER=0` does not disable
an explicitly selected quiet/cabin profile. To return to legacy rendering, set
all three flags to zero. Upmix OFF and native APAC routing remain separate.

## Speaker and Virtual Cabin constraints

Assume MMI 3D OFF, Focus All, Surround minimum; the latter settings have not
been read live. 3D OFF is not a physical bypass of the amplifier's DSP.

The workbook and wiring evidence do not provide independent rear-door tweeter
PCM channels. R14/R15 share the left rear-door feed; R16/R17 share the right.
R209/R210 are separate rear surround midranges. Our Ls/Rs are two logical inputs
to the factory amplifier, not six separately addressable rear drivers. Existing
factory crossovers decide the low/mid versus tweeter content. No new software
crossover or duplicated tweeter feed has been invented.

The spreadsheet's approximate 160–5000 Hz rear-lower and 2–20 kHz tweeter plots
are not inverse EQ targets. Its aftermarket capacitor and S4 rear-deck sub data
are not assumed to describe the S5's stock sub enclosure. The recovered input
matrix is modelled; complex output DSP, EQ and seat acoustics remain unknown.

## Verified rendering results

Final renderer: `build/stereo_upmix_render_cabin_v13_gated`.
Report: `build/reference-audio/v13-gated-final-review/report.json`.
All results below retain the exact V11 model, isolating renderer changes.

14 existing excerpts / seven songs, eight candidate/reference float variants
(112 renders), plus 28 legacy comparison renders:

| Against V11 + float + old assist | Quiet bass only | Full cabin |
|---|---:|---:|
| Auxiliary LFE energy change, expressed in dB | −6.46 | −6.51 |
| Front 250–4000 Hz energy change | +0.36 dB | +2.52 dB |
| Rear 250–4000 Hz energy change | −0.02 dB | −6.81 dB |

These are summed digital energy comparisons, not perceived loudness or a
prediction of the cabin's bass response. A quieter rear is not automatically
better separation. The ±Sound Check fixtures are protocol stress inputs, not
a claim that Apple universally mandates a −16 dB target.

- All 14 legacy outputs are byte-identical to the preserved pre-change float
  renderer's legacy path.
- No known-matrix over-range samples. Maximum across PCM and matrix buses is
  0.891255239 FS, consistent with the −1 dBFS guard plus S16 rounding tolerance.
- Full `scripts/run_tests.sh` passes, including source-buffer/reset/chunking,
  tracker-veto, quiet bass, metadata/FLUSH and authenticated APAC regressions.
- 16 training tests pass: shared teacher gain/provenance, frozen-head isolation,
  runtime parity, burn-in gradients, WOLA delay/additivity and existing labels.
- QNX GCC 8.5, LOG=0, state trace enabled: 26 exact exports; ABI/TLS/runtime
  guards pass. Startup trace records `cabin_render` and `quiet_lfe` separately.
- Final cabin QEMU synchronous benchmark: mean ~7.801 million estimated ARM
  instructions per 1024 frames, versus ~6.675 million for legacy float (~+17%).
  These are not vehicle CPU cycles or proof of real-time deadline margin.

## Mixture-conditioned attribution: a real limitation remains

`capture_cabin_transfers.c` captures the actual time-varying 2×2 transfer
matrices from a complete mixture run. This instrumentation is compiled only
into a host diagnostic, not the vehicle hook. `evaluate_cabin_attribution.py`
applies those frozen matrices to aligned teacher stems using the captured
windows and the same 512/128 WOLA timing. Stem inference is not rerun.

On the first 12 training excerpts, reconstruction matches the actual C mixture
output within 2.23e−7 FS. Cross-energy is reported explicitly: stem energies
cannot be presented as additive percentages. Attribution is pre final guard,
with synchronous control and an estimated two-stem teacher, not ground truth.

The instrumental-versus-vocal transfer advantage is negative on 6 of those 12
excerpts. Therefore **V11 + cabin is not approved as a clean vocal-excluding
separator**. These are training diagnostics, not a blind test. Bass correction
can be evaluated independently; the spatial policy needs further training and
joint-mixture/listening review before any quality-promotion claim.

## Dataset and training corrections

The new source manifest is `build/reference-audio/v13-training-source/manifest.json`:
43 songs, three different 20-second crops each (129 clips, 43 minutes), versus
the previous 43 × 10 seconds (7m10s). All crops/levels of one source retain the
same hash-based split. These 43 source tracks also have distinct artist tags;
the tags are now preserved. Augmentation is not counted as new independent music.

`prepare_hssr_native_teacher.py` runs BS-RoFormer at its native 44.1 kHz,
applies/restores one shared input gain and writes aligned 48 kHz FLOAT stems.
It bypasses independent stem normalization and PCM16 output. The instrumental
complement retains the original mixture; additivity does not establish source
quality. Source/model/config/package/output hashes are recorded. Stale pairs
are rejected. Strict cache preparation requires every teacher and a fresh root,
checks provenance/alignment and binds each cache to its teacher record.

The model remains 368 → GRU160 → GRU160 → 264, 451,464 trainable parameters in
the full architecture / 469,760-byte export. Increasing size now would confound
teacher and runtime fixes and increase an already measured rendering cost.
Two-stem pseudo-labels do not justify retraining all six semantic classes.

The pilot freezes both GRUs and semantic/global/LFE head rows; only rearability
and pan rows are trainable, with zero weight decay on the frozen-row matrix.
Runtime-aware training uses row-int8 weights, rounded/clipped activations and
the C rational nonlinearities. Host C versus emulated logits: MAE 0.001139 on
the regression fixture, not bit-exact equality. Temporal training uses 64 burn-in
steps plus 192 supervised steps (~0.683 + 2.048 seconds), with one control-period
target shift. This does not simulate arbitrary asynchronous deadline failures.

Pilot recipe: four epochs, at most 128 shuffled batches per epoch, batch 32,
learning rate 1e−5, CPU/two threads. This is an explicitly budgeted pilot, not
four exhaustive passes. Full validation is retained; epoch zero wins if no
epoch improves. Exported C then runs continuously across unmodified validation
mixtures at all three input levels. No model is automatically promoted.

## Current job and handoff

Native teacher generation is running. The dependent pipeline is already
started and waits for all 129 validated pairs before cache preparation and
training. **No new trained weights are selected at this handoff.**

- Teacher root: `build/reference-audio/v13-expanded-teacher`.
- Pipeline: `tools/stereo-upmix-render/train_cabin_v13.py`.
- Live phase/result: `build/hssr-cabin-v13-a/pipeline-status.json`.
- Training history, when produced: `build/hssr-cabin-v13-a/manifest.json`.
- Continuous C check, when produced: `build/hssr-cabin-v13-a/continuous-c-validation.json`.

Next: inspect the completed pilot, then compare its exported model with V11
using the same cabin renderer and mixture-conditioned attribution, including
corrected teachers for the existing regression set. Fresh blind music and real
multistem replay are still missing. Do not increase rear gain to hide missed
objects, and do not call lower total rear energy improved separation.

The local quiet-bass and cabin bundles keep V11. Their screens bind the exact
renderer, ARM libraries, profile and evidence report hashes; stale screens are
rejected. They contain no automatic installer and are not acoustic/quality
approval. In-car listening and live callback/deadline checks remain required;
no vehicle transfer, mixer setting change, commit or push was performed.
