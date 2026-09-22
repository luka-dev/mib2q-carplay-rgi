# HSSR dataset, supervision and model audit

2026-09-08. Review and local diagnostics only. No training, model replacement,
production-source changes or vehicle deployment in this audit.

## Conclusion

The next experiment should correct teacher preparation, temporal training and
evaluation before increasing network capacity. There are verified data-pipeline
problems; insufficient hidden width has not been demonstrated. Keep V11 as the
reference, not as a claim that its spatial reconstruction is ground truth.

Scope: all 645 current pilot caches, all 57 current teacher pairs, source/target
generation, training/export, C feature extraction/inference and routing evaluation.
Historical V11 raw training data/checkpoint were not reconstructed: its retained
directory contains the exported model only. Historical composition below comes
from the V11 selection report, not a fresh audit of the old corpus.

## 1. Actual data coverage

| Current pilot | Optimization | Validation | Repeated-use regression |
|---|---:|---:|---:|
| Songs | 34 | 9 | 7 |
| Unique excerpts | 34 × 10 s | 9 × 10 s | 14 × 10 s |
| Unique audio | 340 s | 90 s | 140 s |
| Feature caches | 510 | 135 | separate render evaluation |

The 43 local training sources contain approximately 169.84 minutes of audio,
but the current pilot uses one 10-second crop at 35% of each source's duration:
7 min 10 s total, approximately 4.2% of available source duration. Five variants
(original, vocal-only, instrumental-only, vocal-heavy, instrumental-heavy) and
three gains (0, -6, -12 dB) do not create new performances or musical passages.
Overlapping 64-step chunks likewise do not create independent examples.

All 645 caches were read: 477,360 training and 126,360 validation control frames;
no NaN/Inf; 34/9 unique source hashes with no intersection. MP3 artist tags also
have no exact artist-string intersection between the 43-song and 7-song sets.
However the split enforces file identity, not artist/composition/mastering identity.
The manifest replaces artist metadata with `local file`, and none of the 50 MP3s
has a genre tag. The selection appears rock-heavy, but no audited genre count is
claimed. Example: the heldout `Heart-Shaped Box` is tagged Imad Royal, not Nirvana.

The seven formerly blind songs have now been inspected across repeated model
decisions. They should be called a development regression set; a fresh final test
must be reserved before further model selection. Avoid crop-level confidence
intervals that treat variants or two excerpts from one song as independent songs.

Historical V11 had MUSDB-derived ancestry and additional supervision: its report
records 384/91 extra train/validation caches, including a 340/90 separator subset.
Do not describe V11 as originally trained only on the current seven-minute pilot,
or compare old 30-second and new 10-second evaluations as identical experiments.

Sources: `build/reference-audio/v12-training-source/manifest.json`,
`build/reference-audio/v12-separator-cache/`, private MP3 metadata, and
`tools/stereo-upmix-render/bench/results/2026-09-07-hssr-selection-v11.md`.

## 2. Verified teacher-preparation defects

### Independent gain changes distort the supervision

`prepare_hssr_roformer_teacher.py` leaves the separator's normalization threshold
at its default 0.9. Installed audio-separator 0.47.0 normalizes the input and each
output stem separately. Thus saved vocal/instrument energy ratios need not match
the corresponding original mixture.

Measured all 43 training and 14 heldout pairs, fitting
`mixture ≈ a × saved_vocal + b × saved_instrumental`:

- 30/57 pairs have more than 0.5 dB difference between the two fitted gains.
- Among nonzero training pairs, the largest relative imbalance is about 3.04 dB.
- Median reconstruction SNR is 14.95 dB without correction, 32.14 dB with one
  common fitted gain, and 82.87 dB with separate fitted gains.
- Example `red-orange-yellow-p1`: vocal factor 1.11124, instrumental factor
  1.57601; raw reconstruction SNR 9.57 dB, corrected 81.01 dB.
- `xoxo-p1` has an exactly zero saved vocal stem. This may be a genuine
  instrumental passage; it needs coverage/label review, not automatic rejection.

High corrected reconstruction SNR verifies gain consistency, NOT source-separation
accuracy: this teacher can construct the complementary stem by subtraction.
The changed ratios affect original-mixture labels, synthetic mixing proportions
and mixture-relative evaluation weights. They do not establish how much each
previous candidate's regression was caused by this problem.

Next implementation: retain one documented mixture gain, use floating-point
teacher outputs without independent stem normalization, restore common amplitude,
check alignment/length/reconstruction before caching. Existing pairs may be
recoverable using validated fitted gains, but should not be silently overwritten.

### Teacher sample-rate contract is inconsistent

The wrapper requests 48,000 Hz; the actual checkpoint YAML specifies 44,100 Hz.
Installed `CommonSeparator.prepare_mix` loads at the requested sample rate;
the inspected MDXC path does not convert to the YAML rate before inference.
Therefore the teacher sees audio at a different rate from its configuration.
The magnitude of the resulting separation-quality loss has NOT been measured.

Next implementation: run this teacher at its native 44.1 kHz; jointly resample
aligned stems back to the student's 48 kHz grid, with explicit length handling.
Compare corrected/native-rate separation on fixed QA excerpts before a full run.

### Cache validity is too weak

`valid_pair` checks file readability, 48 kHz, stereo and matching nonzero lengths.
It does not bind existing stems to input excerpt hash, actual model/config hash,
normalization settings or expected source length. A stale but readable pair can
be reused after changing those inputs. Store and verify this full provenance,
including output hashes and schema version, before reuse.

Local evidence: teacher wrapper lines 56/85; checkpoint
`build/hssr-teacher-models/model_bs_roformer_ep_317_sdr_12.9755.yaml`; installed
`audio_separator/separator/{common_separator.py,architectures/mdxc_separator.py}`.

## 3. Targets are not six independent source labels

The current RoFormer supplies vocals and instrumental. `selection_v11_targets`
then invents the other classes using frequency, flux, tonality, coherence and pan.
For example, bass is a low-frequency curve over instrumental energy, and drums
are inferred from transients. These are useful priors, not isolated bass/drum
truth. Fine-tuning every class against them can overwrite semantic knowledge from
earlier real-stem training and affect both rear selection and LFE decisions.

The target is conservative instrumental routability, not authored 5.1 placement
or an acoustic model of the car. It also treats the teacher's entire vocal stem
as protected, without independently labeling lead, backing vocals, choir or vocal
reverberation. Any future change in that policy must be explicit.

The actual Universal renderer also retains a bounded phase-rotated, frequency-
steered mid/high bed when explicit objects are weak. Its content floor and
percussion rescue mean the rear signal is not a pure isolated instrumental stem.
Audible vocal remnants remain possible even when the intended policy protects
lead vocals. Include these fallback paths in mixture-conditioned evaluation.

Class mass is zero in 21.96%/22.30% of training/validation band-time cells.
Zero mass means unlabelled, not a known negative class. The corrected V12-D
auxiliary loss respects that; the earlier V12-C version did not. Rear BCE still
averages over every band, including silent/unresolvable bands. Use separate
activity/confidence masks and source-balanced sampling; retain explicit silence
safety tests without letting silence dominate semantic optimization.

Restore real-stem replay alongside corrected pseudo-labels. MUSDB18 provides
vocals, drums, bass and other for 150 tracks (~10 h, 100/50 split); HQ avoids the
compressed edition's high-frequency bandwidth restriction. Its access/use terms
and documented label errors still matter. See [official dataset documentation](https://sigsep.github.io/datasets/musdb.html).
Do not use official test songs to train or tune this controller.

## 4. Training/runtime mismatch, measured rather than assumed

Current model: 368 features → GRU160 → GRU160 → Linear264; 451,464 trainable
parameters. Outputs: 32 bands × (six class logits, rearability, pan) + eight globals.
It is a causal routing controller, not a waveform source separator.

Training uses ordinary float PyTorch GRUs; export quantizes weights afterwards.
C also quantizes matrix inputs/recurrent activations and uses rational sigmoid/
tanh approximations. Loading the exported weights into PyTorch is not C inference.

Diagnostic measurements with the unchanged V11 exported model:

- Feature parity on one 10-second original excerpt: both implementations produce
  936 frames; raw feature MAE 0.0000425, normalized MAE 0.000182. This is good
  agreement, not evidence of a broad feature-extraction failure. Other excerpts,
  silence and transition edge cases still require a permanent parity suite.
- C versus dequantized-weight PyTorch on 54 isolated validation caches (nine songs,
  two stem types, three levels): mean absolute vocal-probability difference
  0.00561 and rear-probability difference 0.00323, using the same exact sigmoid/
  softmax to compare logits. This isolates network differences, not final DSP.
- Resetting PyTorch hidden state every 64 controls versus continuous inference
  changes rear probability by 0.01224 on average; on instrumental-only caches,
  0.02185, versus 0.00621 for C/float inference differences.

64 controls at 93.75 Hz are only 0.683 s. Each training chunk starts from zero;
runtime is continuously stateful and applies async controls one control period
later. Long-context training with warm-up/state carry should precede a third GRU.
Candidate recipe: 2–4 s supervised chunks, 0.5–1 s causal burn-in, truncated
backpropagation with detached state, explicit resets at real track boundaries.

Implement a differentiable emulator of this exact custom runtime: row-wise weight
quantization, input/hidden rounding and saturation, activation approximations,
normalization and delayed control application. Verify it against the host C oracle.
Generic PyTorch QAT is a concept, not a drop-in backend for our custom QNX GRU;
see [PyTorch quantization guidance](https://pytorch.org/blog/quantization-in-practice/).

AdamW, gradient clipping and warm-start baseline retention are reasonable. The
larger issue is selecting the best epoch only by teacher loss on short reset
chunks. V12-A lowered that loss while worsening isolated-vocal heldout transfer
by approximately 49%; this demonstrates objective/evaluation disagreement, not
proof that the network needs additional capacity.

## 5. Evaluation must follow the actual mixture and renderer

`evaluate_hssr_teacher_stem_routing.py` separately renders each isolated stem.
Since the network and DSP adapt to the input, those runs do not use the controls
that the original mixture would produce. Their weighted ratio is an isolated-stem
stress measure, NOT measured vocal leakage in the rendered mixture. Historical
numbers remain useful regressions but should be labelled accordingly.

Add mixture-conditioned attribution: run the controller on the mixture, record
its actual masks/routing/gain trajectory, then apply that same frozen trajectory
to the aligned stems. Include delay, object routing and mix-derived linked gain.
Check reconstruction/additivity and report any residual rather than assigning it
silently to an instrument. Keep separate isolated-stem tests as safety cases.

Optimize/evaluate rear vocal energy, useful instrumental transfer, front damage,
fold-back error, stereo/rear stability and gain pumping together. Calibrate through
the deterministic DSP because class vetoes/object-rescue routes can bypass the
learned rearability scalar. Use source-balanced aggregates and per-song worst
cases; prohibit the trivial all-front solution with a useful-instrument floor.
Select candidates on fixed development folds, then evaluate once on new songs.
Listening in the car is still necessary for placement and timbre judgments.

## 6. Architecture decision

There is a real representation limitation: FFT512 at 48 kHz has 93.75 Hz bins.
Three configured bands contain no bins: 60–90, 125–170 and 220–280 Hz. Those empty
bands have zero rear targets in all current caches. Wider GRUs cannot recover
frequency detail discarded by the frontend.

Keep the current graph as corrected-data baseline. Then test a frequency-aware
causal alternative: shared per-band encoder (e.g. width 32), a small adjacent-band
mixer, compact temporal GRU/global context, and per-band control heads. Test an
additional past-only 1024/2048-sample analysis branch for low-frequency/harmonic
context while retaining the short synthesis transform. A longer analysis history
does not require future lookahead, but changes response smoothing and compute.
Exact dimensions and budget need a prototype; this is a design hypothesis.

Band/temporal structure is motivated by [Band-split RNN](https://arxiv.org/abs/2209.15174),
not a claim that its full separator or reported accuracy transfers to this car.
Do not deploy an offline RoFormer or bidirectional network in the realtime hook.

Controlled width ablation for the existing two-GRU graph:

| Hidden width | Parameters | Dense weight MAC/s at 93.75 controls/s | Relative weight work |
|---|---:|---:|---:|
| 160, current | 451,464 | 42.12 M | 1.00× |
| 192 | 597,000 | 55.73 M | 1.32× |
| 256 | 943,368 | 88.13 M | 2.09× |

These are arithmetic counts, excluding FFT/routing/activation/quantization work,
not measured ARM cycles. Formula for weights is `3HF + 9H² + HO`, with F=368,
O=264; biases add `12H+O`. A third 160-wide GRU adds 154,560 parameters and is
not the first experiment. Width/layer/frontend changes require a versioned model
ABI and runtime changes: today's binary loader and state arrays are fixed-size.

## 7. Virtual cabin and Sound Check remain separate constraints

Use the agreed 3D OFF operating assumption, but not a fictitious DSP bypass.
The recovered electrical/input mapping is not full measured cabin acoustics.
There are 19 drivers and 16 electrical speaker circuits; rear-door woofer/tweeter
sharing a feed does not create independently steerable student outputs. We feed
logical PCM channels through the OEM amplifier, not one neural channel per driver.

Rear hardware distinction: R15/R17 are the lower rear-door drivers; R14/R16 are
their shared-feed tweeters. Separate R209/R210 are surround/effect midrange drivers
in the Sportback C-pillar trim, not the rear-door tweeters. The student does not
choose instruments independently for those three drivers on either side. Within
each door, electrical/acoustic filtering distributes one signal by frequency;
the exact passive crossover is not established. The rear +3 dB high shelf has a
4 kHz midpoint and acts on the logical Ls/Rs signal, not a tweeter-only output.
The documentary door/effect equal-power fan-out is an evaluation assumption, not
a recovered factory gain ratio. See `SOUNDAMP_9VS_SPEAKER_PARTS_RESEARCH.md` and
`SHARC_OUTPUT_BINDING_AUDIT.md` for confidence levels and candidate output filters.

Evaluate the known input matrix/headroom as a digital constraint. Do not train
an inverse of unknown OEM EQ, delays or seat impulse responses. Focus All/minimum
Surround assumptions still need live confirmation before physical validation.

Sound Check in the new float path is uniform post-inference/post-synthesis gain.
Input gain augmentation tests source-level robustness, not Sound Check itself.
Test the two axes independently: input master level versus negotiated post-gain
and limiter action, including transitions. Do not hardcode the -16 dB fixture as
a universal Apple target or expose it as a semantic model class.

## Recommended experiment order

1. Fix teacher native rate, shared gain, float outputs and cache provenance;
   validate old/new pairs without overwriting the evidence from this audit.
2. Expand unique passages across the available tracks (initially 3–5 disjoint
   20–30 s sections where duration permits), restore real-stem replay, include
   transitions, silence, sparse vocals, dense guitars, bass/drum transients,
   mono/near-mono and spacious mixes. Generate augmentations within source splits.
3. Add faithful stateful/quantized training and mixture-conditioned C evaluation.
   Freeze a fresh final test split before further selection.
4. Retrain the unchanged 2×160 baseline first; compare identical-data/compute
   ablations against 2×192 and the frequency-aware prototype. Multiple seeds,
   per-song confidence intervals and real target CPU measurements are required
   before calling an architecture better.
5. Only promote after mixture safety/quality, legacy regressions, target realtime
   headroom and listening checks pass. No current candidate supersedes V11 here.

Reproducible diagnostic helper: `build/hssr_audit_diagnostics.py`; raw results:
`build/hssr-dataset-runtime-audit.json`. The helper composes the existing
`tools/stereo-upmix-render/bench/hssr_validate.c`; all generated diagnostics remain
local/ignored. Private audio was not uploaded or redistributed.
