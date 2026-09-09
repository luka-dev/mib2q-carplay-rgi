# HSSR Universal host renderer

This directory exercises the exact stereo upmixer shipped in the hook.  There
is one production algorithm, `hssr-universal`, plus `off`, the bit-exact
`L,R,0,0,0,0` safety bypass.  Historical comparison profiles were removed from
the working tree after Universal won the objective and listening evaluation;
their history remains in Git.

An opt-in `--float-render` candidate exists for host evaluation and the QNX
hook behind `AUDIO_FLOAT_RENDER=1` (default off; not deployed).
The production description below refers to the default S16 path; see
[stage-one results](../../docs/UPMIX_FLOAT_STAGE1.md) for its differences.
See [integration and fine-tuning](../../docs/UPMIX_CABIN_V12.md) for subsequent work.

Universal accepts 48 kHz, signed 16-bit stereo WAV and writes extensible 5.1
WAV in `L,R,C,LFE,Ls,Rs` order.  It has 512 frames of matched front/rear/LFE
latency, keeps digital C at zero, applies a fixed +3 dB high shelf with a 4-kHz
midpoint only to separated Ls/Rs, caps each rear at Q15 `8192` (`0.25`), and can
add Cabin-Aware LFE V2. The rear shelf precedes the linked limiter and does not
alter the object-transfer signal subtracted from dry L/R. V2 uses one phase-safe 48-Hz LR4 sub-band per side,
the existing HSSR bass/drum posteriors and a limiter over all six recovered
J525 logical buses. Vehicle listening showed that the recovered SoundAmp
matrix distributes LFE into the front, centre and rear buses, so its calibrated
cap is Q15 `4096` (`0.125`, -6 dB from the first V2 build) and its paired front
gain is Q15 `31392` (about `0.958`). There is no legacy LFE mode or fallback. The
semantic controller requires a
validated HSSR model; the selected V11 separation-supervised model is:

```text
build/hssr-selection-v11/hssr_model.bin
SHA-256 ab01463e9359f013c4b40f311d630bd20af48d761ad361563cb5246f6bd686ae
```

## Build and render

```sh
cc -std=c99 -O2 -Wall -Wextra -Werror -Ihook/audio \
  tools/stereo-upmix-render/stereo_upmix_render.c \
  hook/audio/soundcheck.c \
  hook/audio/stereo_upmix.c hook/audio/hssr_dsp.c hook/audio/hssr_neon.c \
  -lm -o build/stereo_upmix_render

build/stereo_upmix_render \
  --mode hssr-universal \
  --model build/hssr-selection-v11/hssr_model.bin \
  --lfe assist --surround 0.25 input-stereo.wav output-5.1.wav
```

Use `--mode off` for the exact bypass; it does not require a model.  Input and
output may not resolve to the same file.

## Experimental float synthesis and Sound Check

```sh
build/stereo_upmix_render --float-render \
  --model build/hssr-selection-v11/hssr_model.bin \
  --lfe assist --surround 0.25 \
  --soundcheck-target -16 --soundcheck-params -6.5 -1 0.96875 \
  input-stereo.wav candidate-5.1.wav
```

This explicit protocol fixture yields -9 dB using the existing Sound Check
calculation. It is not a universal Apple target or measured metadata for your
input WAV. Omit both Sound Check options for unity programme gain.
`--surround` means **our synthesis rear gain**, not the MMI Surround position.

The candidate retains V11 inference and source observation semantics, but
renders front transfer, rear shelf and LFE in S16-scaled float units. It
replaces the legacy per-rear limiter/front clamp/LFE-only cabin clamp with one
post-gain detector watching all six PCM channels and eight recovered J525
input-matrix buses. One common gain preserves simultaneous channel ratios.
The matrix is used only for detection, not applied to the actual PCM output.
Output is quantized once. The -1 dBFS guard is a sample-peak policy, not a
true-peak limiter or protection against every downstream EQ/acoustic peak.
The float synthesis OFF API is exact; optional post-gain/protection can of
course change OFF samples too. Default S16 CLI behavior remains unchanged.

`evaluate_float_candidate.py` compares the default and float variants under
unity/-9/+1 dB fixtures, keeps every render local, and reports PCM plus
pre-clipping J525 matrix metrics. Its output directory must be new. Use
`--legacy-reference` with a pre-change binary to check exact compatibility.

## Objective evaluation

The reproducible public-authored corpus is prepared by
`prepare_authored_corpus.py`; a local stereo stress set can be made with
`prepare_local_stereo_stress_corpus.py`.  The evaluators accept only the one
shipping renderer:

```sh
python3 tools/stereo-upmix-render/evaluate_reference_corpus.py \
  --renderer build/stereo_upmix_render \
  --manifest build/reference-audio/public-authored-corpus/manifest.json \
  --output build/reference-audio/hssr-universal-eval \
  --lfe assist --surround 0.25 \
  --hssr-model build/hssr-selection-v11/hssr_model.bin

python3 tools/stereo-upmix-render/evaluate_stereo_stress_corpus.py \
  --renderer build/stereo_upmix_render \
  --manifest build/reference-audio/local-stereo-stress/manifest.json \
  --output build/reference-audio/hssr-universal-stress \
  --hssr-model build/hssr-selection-v11/hssr_model.bin
```

Teacher-stem selectivity is measured separately by
`evaluate_hssr_teacher_stem_routing.py`.  The final model and performance
decision is recorded in
[`bench/results/2026-09-07-hssr-selection-v11.md`](bench/results/2026-09-07-hssr-selection-v11.md).
The rear-presence shelf response, held-out A/B and ARM/QNX cost are recorded in
[`bench/results/2026-09-08-rear-high-shelf.md`](bench/results/2026-09-08-rear-high-shelf.md).

These measurements can reject regressions, clipping, dead rears and excessive
vocal/bass leakage.  They cannot prove that stereo inversion recovered the
original studio objects; mastered stereo is non-invertible, so vehicle
listening remains the final perceptual gate.

## Tests

```sh
scripts/run_tests.sh
tools/stereo-upmix-render/bench/build_host.sh
build/stereo-upmix-bench/host/stereo_upmix_bench \
  --backend hssr-universal \
  --model build/hssr-selection-v11/hssr_model.bin
```
