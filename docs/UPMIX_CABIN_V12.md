# Cabin-aware float integration and V12 fine-tuning pilot

2026-09-08. Local development only; no vehicle deployment or live settings
changes. This follows the historical `UPMIX_FLOAT_STAGE1.md` results.

**Outcome: retain V11 weights.** Four fine-tuning runs and one conservative
weight blend were evaluated. No learned candidate demonstrated a sufficiently
useful, non-regressing replacement. The review bundle contains the NEW float
hook with the ORIGINAL V11 model, not a claimed better-trained V12 network.

## Scope and invariants

Assume MMI 3D OFF, Focus All and minimum Surround; the latter two are not live
readbacks. MMI OFF is not a physical DSP bypass. The recovered input matrix
is modelled, not the complete complex DSP, output EQ or acoustic cabin. Two
drivers in a rear door share an electrical circuit; no independent tweeter
channel or duplicated full-band feed has been invented.

The new hook option `AUDIO_FLOAT_RENDER=1` is off by default. Stereo inference
uses original decoded PCM; front transfer, rear shelf and LFE stay float until
one uniform Sound Check gain, one linked matrix/rail sample-peak guard, and
final S16 conversion. Native APAC 5.1 is reordered, not upmixed, then uses the
same post-gain guard. Experimental APAC 5.1.2 retains its old gated path.
Explicit upmix OFF retains old widening and existing Sound Check semantics.

The float guard is independent of `AUDIO_SOUNDCHECK` and the legacy
`AUDIO_SOUNDCHECK_LIMITER` switch. Disabling normalization must not accidentally
disable candidate headroom protection. Prefix FLUSH, source/cookie changes,
and stopping playback clear the guard's release state with the upmix state.
One linked gain preserves simultaneous channel ratios; it can still alter
the programme envelope. No true-peak, downstream EQ or acoustic safety claim.

## Training design

- Recover trainable row-dequantized weights from the validated V11 binary;
  original full-precision checkpoints/stems had previously been removed.
  Preserve its embedded feature normalization. This is a fine-tune, not a
  reproduction of the original full MUSDB training run.
- Pilot corpus: 43 private training songs, one 10-second excerpt at fraction
  0.35 each. Deterministic source-hash train/validation split; all variants of
  one song stay together. The separate seven-song regression corpus has zero
  source-hash overlap. Fine-tuning validation songs may have been seen during
  original V11 training; do not call this entirely unseen pretraining data.
- Local BS-RoFormer vocal/instrumental teacher, original mixture plus isolated
  vocal negatives, instrumental positives and vocal-/instrumental-heavy mixes.
- Recompute all source features at 0/-6/-12 dB with unchanged semantic targets.
  This tests source-level robustness; Sound Check remains AFTER inference.
- Six epochs, AdamW, initial learning rate 3e-5, balanced loss, batch 32.
  Compare validation loss before training too, retain epoch zero if no epoch
  improves, and checkpoint improvements immediately. Export CRC-checked int8
  model, then evaluate the exported weights per input level and in C renders.

Cabin knowledge currently constrains rendering/headroom and evaluation, not
a fabricated acoustic teacher loss. Greater rear energy alone is not an
improvement: vocal leakage and instrumental transfer must be compared together.
Separate stem inference is a diagnostic, not exact joint-mixture attribution.

## Reproducibility

Teacher environment: `build/hssr-teacher-env`, Python 3.14.6,
audio-separator 0.47.0, Torch 2.14.0, Apple MPS. The package's pinned beartype
0.18.5 failed on Python 3.14 union type annotations; only this isolated env
overrides it with beartype 0.22.9. System Torch/training environment is unchanged.

Teacher weights: `model_bs_roformer_ep_317_sdr_12.9755.ckpt`, 639331213 bytes,
SHA256 `5b84f37e8d444c8cb30c79d77f613a41c05868ff9c9ac6c7049c00aefae115aa`.
Download was resumed after interruption; complete-file MD5 matches the release
server's `Jh6JiGRuxJSY5G/a+X8HAw==`. Model and package come from the upstream
[audio-separator project](https://github.com/nomadkaraoke/python-audio-separator)
and [UVR model release](https://github.com/TRvlvr/model_repo/releases/tag/all_public_uvr_models).
Audio stays local. Model licensing remains private/development/MUSDB-derived;
no commercial release approval is implied.

Re-run with a NEW candidate output directory:

```sh
bash tools/stereo-upmix-render/train_cabin_candidate.sh build/hssr-cabin-v12-next
```

Teacher stems are resumable. The script trains from the existing private
43-song manifest, builds augmented caches and produces per-level validation.
It does not perform vehicle deployment or automatically promote the model.

## Integration verification

- Full `scripts/run_tests.sh` passed after hook integration, including real
  authenticated APAC reference, AAC/APAC routing and flush tests.
- Eleven selection-training tests pass: binary warm start/CRC, source-disjoint
  level augmentation with identical targets, routing locks and vocal vetoes,
  including an explicit missed-vocal-class penalty and unlabelled/partially
  labelled band semantics.
- QNX GCC 8.5 hook and codec build passed export/runtime/constructor guards.
  Final candidate build uses `LOG=0 AUDIO_STATE_TRACE=1`: full logging/captures
  disabled, compact lifecycle trace includes `float_render`.
- QNX/ARM32 benchmark executed successfully with 32 measured 1024-frame calls:
  same V11, rear synthesis gain 0.25, LFE assist. Legacy mean 6,100,282 estimated
  instructions; complete float path with +1 dB post-gain and matrix guard
  6,612,581 (+8.4%). QEMU instruction estimates are NOT measured Krait cycles.
- Mac async float benchmark: 128 calls, mean 1.079 ms, maximum 2.424 ms versus
  21.333-ms audio period; 271/271 controller jobs completed, zero busy drops.
  Host timing is not proof of live head-unit deadlines.

## Artifacts and promotion

The offline screen, declared before examining V12 C-render results, requires
at least 90% of V11 weighted instrumental rear transfer, at most 110% of its
weighted vocal leakage, and at least 95% of selectivity on both validation
and heldout stems. Each input-level validation loss may increase by at most
1%; mean C-render level variation by at most 5%; float matrix over-range count
must be zero. These practical regression tolerances are not confidence
intervals or acoustic scores. Passing never authorizes vehicle promotion.

`scripts/package_cabin_candidate.py` assembles local ARM libraries, model,
candidate startup, reports and local V11/build rollback copies. It validates
model CRC/header, ARM ELF headers and rollback model SHA; refuses overwrite.
The bundle is explicitly unapproved for vehicle use and has no installer.
The existing SSH deploy script remains pinned to V11.

## Completed pilot attempts

V12-A: balanced loss, LR 3e-5, six epochs, best epoch 4. Validation total
2.740774 -> 2.621521 (-4.35%); exported balanced loss improves at all three
source levels. C-render mean rear-transfer variation across 0/-6/-12 dB
0.894178 -> 0.445833 dB. Nevertheless REJECTED: weighted vocal rear leakage
increases 18.3% on fine-tune validation and 48.9% on heldout, while instrumental
transfer rises only 3.6%/4.0%. No matrix over-range samples. Training loss and
level consistency alone would have selected a separation regression.

V12-A SHA256: `16e65422a672cb19ce1b646a2f88160a0dc060e88e4158489bdd2de2b014540c`,
CRC32 `ff24b6b5`. Kept as a rejected development artifact, not a deployment model.

V12-B: existing selection-v11 loss, LR 1e-5, best epoch 5 of six. REJECTED on
fine-tune validation before running its heldout renders: vocal transfer +9.4%,
instrumental +2.6%, selectivity ratio 0.93747 < fixed 0.95 floor. Mean level
variation is 0.454748 dB. SHA256
`05324c46b94cb80f7c9bf2ffc044f8616faf69fd2cfcf1635662516e763fa0de`, CRC32 `89779bf6`.

V12-C added a vocal one-vs-rest classification loss, since the runtime's
object-rescue/safety paths depend on vocal class probability as well as the
rearability output. It preserves all runtime coefficients and binary layout.
It was REJECTED on validation (weighted vocal transfer 6.78x V11). Its new
auxiliary loss incorrectly interpreted zero class mass in the old
vocal-negative targets as negative vocal labels. Existing CE correctly treats
that mass as activity/confidence; a zero row is unlabelled. Thus lower teacher
loss concealed a real routing regression. No C heldout renders were run.

V12-D corrects that newly introduced loss: normalize class posterior by its
label mass, then weight the auxiliary loss by that mass. Zero mass contributes
no classification lesson. Regression tests cover zero and partial class mass.
Same six epochs/LR 1e-5 from V11, same split. Reproduce the CORRECTED profile:

```sh
bash tools/stereo-upmix-render/train_cabin_candidate.sh build/hssr-cabin-v12-d-repeat cabin-v12 0.00001
```

D passed fine-tune validation transfer checks (+2.13% vocal, +0.04%
instrumental, selectivity 0.97949x) but was REJECTED on heldout: +22.23% vocal,
-2.35% instrumental, selectivity 0.79889x. Mean C-render source-level variation
fell to 0.507582 dB, but this does not outweigh the vocal regression. All 42
float heldout renders remained below full scale in the known input matrix.
D model SHA256 `429f2b29554a2c5c7fe7f5d046d68e22af44504b3b8eaa21fce20bd120046d8e`,
CRC32 `93c90a5c`; QNX execution also passed (mean 6,611,795 estimated instructions).

V12-E: a single network formed from 87.5% V11 and 12.5% D checkpoint weights,
then quantized. Validation behavior almost returns to V11, but mean level
variation is 0.895824 dB versus V11's 0.894178 dB and balanced loss changes are
negligible. NOT SELECTED: no demonstrated useful improvement; no E heldout
test was needed. SHA256
`b8dbc6623b369d559867ae1753cabcd0589dfae4de58c33d4acd14b264ddd377`.

The final reference screen is deliberately separate from the learned-model
screen. It requires exact V11 SHA, all 14 x 4 renders, byte-exact legacy output
against the pre-change renderer and no float known-matrix over-range samples.
It makes no training-gain or acoustic-quality claim. Original thresholds for
learned candidates remain unchanged; no failed model is packaged as a winner.

Final render-only report: `build/reference-audio/v12-final-v11-float/report.json`.
Reference screen: `build/hssr-cabin-v11-float-reference/offline-review.json`.
Private review bundle: `build/packages/audi-float-v11-reference/` (and ZIP).
It includes candidate libraries/startup, V11, local rollback copies and reports.
The master startup/deployment defaults remain untouched.

All reported heldout results are a development regression set, not a new blind
listening trial. Candidate thresholds have not been relaxed after failures.

Next: loudness-matched listening of float+V11 versus legacy+V11 and live
Focus/Surround/deadline/level confirmation before installing anything. More
training needs stronger preservation of original semantic priors and a larger
source-diverse supervision pilot, not just extra epochs or lower teacher loss.
The regenerated teacher/caches and full-precision checkpoints now make that
work resumable. Do not silently promote A/B/C/D/E or discard their reports.
