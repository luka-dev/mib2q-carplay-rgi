# HSSR Universal host renderer

This directory exercises the exact stereo upmixer shipped in the hook.  There
is one production algorithm, `hssr-universal`, plus `off`, the bit-exact
`L,R,0,0,0,0` safety bypass.  Historical comparison profiles were removed from
the working tree after Universal won the objective and listening evaluation;
their history remains in Git.

Universal accepts 48 kHz, signed 16-bit stereo WAV and writes extensible 5.1
WAV in `L,R,C,LFE,Ls,Rs` order.  It has 512 frames of matched front/rear/LFE
latency, keeps digital C at zero, caps each rear at Q15 `8192` (`0.25`), and can
add Cabin-Aware LFE V2. V2 combines phase-safe 55/95 Hz LR4 body/punch paths,
the existing HSSR bass/drum posteriors and a limiter over all six recovered
J525 logical buses. Its cap is Q15 `8192` (`0.25`) and its paired front gain is
Q15 `30100` (about `0.919`). There is no legacy LFE mode or fallback. The
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
  hook/audio/stereo_upmix.c hook/audio/hssr_dsp.c hook/audio/hssr_neon.c \
  -lm -o build/stereo_upmix_render

build/stereo_upmix_render \
  --mode hssr-universal \
  --model build/hssr-selection-v11/hssr_model.bin \
  --lfe assist --surround 0.25 input-stereo.wav output-5.1.wav
```

Use `--mode off` for the exact bypass; it does not require a model.  Input and
output may not resolve to the same file.

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
