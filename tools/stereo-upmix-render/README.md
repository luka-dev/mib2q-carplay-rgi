# Scene upmix renderer tools

The reference, the tests and the fixtures of the stereo-to-5.1 scene renderer
that ships in the hook (`hook/audio/scene_*.c`, the `SCENE` backend of
`hook/audio/stereo_upmix.c`; spec `docs/audio/UPMIX_SCENE_ALGORITHM_2026-10-08.md`,
sections 14-18). Everything here is offline: numpy in float64 is the truth, the
C port is checked against it, and the car decides the rest.

## 📁 Files

| File | What it is |
|---|---|
| `scene_upmix.py` | The numpy reference renderer (`render(x[n,2], SceneConfig) -> y[n,6]`, `decompose()` for tests) and `SceneStream` / `stream_render`, the block-exact streaming form the C port is diffed against. CLI compatible with the stem harness: unknown renderer flags are ignored, the last two positionals are the stereo input and the 6-ch output (48 kHz int16 WAV); `--alpha --surround --ambient-gain --air-db --rear-delay-ms --sigma-min --no-cluster --no-unmix --wide-pair --duck-mask --dump`. |
| `test_scene_upmix.py` | The verification suite: synthetic scenes (20 s, 48 kHz, mix peak 0.7), every number attributed by stem with the harness's `rear_share`; 51 checks, exit 1 on any failure; prints the module md5. |
| `scene_albumdb_eval.py` | The ten AlbumDB songs with real stems (sum of the stored heads, loudest 60 s) against the passive matrix and the PAD baseline; per-head rear share, swing and behind-ness; writes a Markdown table and JSON into `--out-dir`. The spec's section 14 table. |
| `scene_oracle_master.py` | One AlbumDB stem hard-panned, everything else dead-centre: the exact-decomposition test offline and the in-car oracle file of spec section 13.5. |
| `scene_car_kit.py` | The blind listening kit: stereo sources × renderer conditions → headerless 6-ch S16 files for `qsa_play` on the head unit (plus the 6-ch WAVs), names blinded per source, key in `KEY.txt`, a `rearcheck.s16` tone to prove the SAD is 0xfc. |
| `scene_dev40_dump_stems.py`, `scene_dev40_remeasure_steady.py`, `scene_dev40_summarise.py` | The DEV40 pipeline on the training PC and the Mac (spec section 15): dump the harness's per-stem 60 s excerpts as one npz per song; re-measure a harness run with the steady-range columns; summarise `stems.csv` files the way the Track A matched40 did. |
| `scene_swing_audit.py` | Swing audit on dumped DEV40 stems: one stem's 1 s rear-share medians next to the renderer's internals (admitted places, member weight, pooled pan, Wiener w). |
| `scene_port_fixtures.py` | Golden fixtures for the C port: `functions.npz` (the pure per-frame functions, the tables, an STFT round trip) and `stream_spatial.npz` / `stream_mask.npz` (block-exact `SceneStream` goldens: analysis frames, per-frame engine outputs and stats, the final [n,6]). |
| `scene_mask_fixtures.py` | Goldens for the fine mask net c runtime, made on the training PC with torch and `mask_net.py`: weights, standardize constants, features, every layer for the first 64 controls, posteriors, eligibility and protect for the whole file (`masknet_oracle.npz`). |
| `scene_render_hook.c` | The `SCENE` backend of `hook/audio/stereo_upmix.c` on a stereo WAV, through `spa_stereo_upmix_process_s16()` in 768-frame calls (one QSA period), the output 1024 frames late exactly as in the hook. |
| `port/` | The ctypes tests of the C port against the fixtures (below) and `export_masknet_weights.py`, which turns `masknet_oracle.npz` into the `SMSKNET1` blob the hook loads (`models/scene-masknet-c/scene_masknet_c.bin`). |
| `bench/scene_*` | The engine and mask-net benches, their QNX cross-build and the QEMU `-icount` runner ([bench/README.md](bench/README.md)). |

## ✅ Reference tests

```sh
python3 tools/stereo-upmix-render/test_scene_upmix.py            # 51 checks, ~30 s
python3 -m pytest tools/stereo-upmix-render/test_scene_upmix.py  # the same as test_* functions
```

Scratch WAVs (only the PAD baseline needs files) go to `$SCENE_TEST_TMP` or the
system temp dir.

## 🔬 Port tests

Each script builds what it needs into `build/scene_port/` with the host
compiler and `-ffp-contract=off`, loads it with ctypes and prints per check the
error and PASS/FAIL (exit 1 on any FAIL). The fixtures are the output of
`scene_port_fixtures.py OUT_DIR` and `scene_mask_fixtures.py`; pass `--fixtures DIR`
(the default in the scripts is a scratch path of the session that made them).

```sh
python3 tools/stereo-upmix-render/port/test_math.py --fixtures DIR      # helpers, tables, FFT, config defaults
python3 tools/stereo-upmix-render/port/test_clusters.py --fixtures DIR  # scene_clusters.c, fixture replay + synthetic runs
python3 tools/stereo-upmix-render/port/test_engine.py --fixtures DIR    # frame level and block level, -60 dB gate, 768 vs 1000 bit-identical
python3 tools/stereo-upmix-render/port/test_mask.py --fixtures DIR      # scene_mask.c vs torch on the C features and vs the goldens
python3 tools/stereo-upmix-render/port/test_hook_render.py             # scene_render_hook vs stream_render on the car-kit album01.wav
```

Measured on the shipped sources: engine −125.8 / −131.3 dB of the output RMS
(`test_engine.py`), net 4.2e-4 vs torch on the C features and 5.2e-3 end to
end (`test_mask.py`; the reference's own float32 noise is 4.2e-3), the hook
−71.7 dB on the net's own rows (`test_hook_render.py`).

## ⚙️ The hook renderer

`scene_render_hook.c` is built by `port/test_hook_render.py`; by hand it is the
`run_tests.sh` link line plus the tool:

```sh
cc -std=gnu99 -O2 -ffp-contract=off -Wall -Wextra -Werror -Ihook -Ihook/audio \
   tools/stereo-upmix-render/scene_render_hook.c hook/audio/stereo_upmix.c \
   hook/audio/scene_engine_math.c hook/audio/scene_clusters.c hook/audio/scene_engine.c \
   hook/audio/scene_mask.c hook/audio/scene_mask_worker.c \
   -lm -lpthread -o build/scene_port/scene_render_hook

build/scene_port/scene_render_hook [--mode scene|off] [--lfe off|assist] [--block 1..1024] \
    [--alpha A] [--inst-alpha A] [--inst-gamma G] [--duck-depth D] [--rear-delay-ms MS] \
    [--sigma-min S] [--side-lp-hz HZ] [--bleed-cancel K] \
    [--mask-blob masknet.bin] [--mask-rows rows.f32] in.wav out.wav
```

The knobs are the environment's `AUDIO_SCENE_*` in the same units and default
to the engine's variant-J values, which are what `carplay_startup.sh` exports;
`--mask-blob` installs the fine mask net as `AUDIO_SCENE_MASK_BLOB` does on the
unit (without it: spatial only); `--mask-rows` (a `-DSPA_STEREO_UPMIX_TESTING`
build) dumps the rows the backend installed at every 512-frame control boundary.
`--mode off` is the exact `L,R,0,0,0,0` bypass. The build line in the file's
header still names the retired `hssr_*.c` files; use the one above.

`-ffp-contract=off` on every host build: the QNX/ARMv7 build has no FMA, so a
host render with fused multiply-adds is not the arithmetic the car performs.
Every link line that names `stereo_upmix.c` needs the four scene files
(`scene_engine_math.c`, `scene_clusters.c`, `scene_engine.c`, `scene_mask.c`)
and `scene_mask_worker.c` when the worker is referenced.

## 📊 The stem harness

`upmix_behavior_tests.py`, `upmix_stem_tests.py`, `upmix_music_tests.py`,
`baseline_upmix.py` and `mask_net.py` are the Universal-era harness whose
metric functions (`rear_share`, the per-stem attribution, the swing columns,
the synthetic scene generators, the passive-matrix and PAD baselines, the
ConvNet definition the fixtures and the exporter read) the scene evaluators
import. They stay for that reason; their old `--render` / `--model` CLI paths
that drove the Universal host renderer no longer work.

## 📜 What left on 2026-10-09

The Universal host renderer (`stereo_upmix_render.c`), its bench
(`bench/stereo_upmix_bench.c`, `build_host.sh`, `build_qnx.sh`,
`qnx_qemu_bench.sh`, `mask_selftest_compare.py`), the mask-net parity tool
(`mask_infer_test.c`), the corpus and evaluation scripts of the HSSR era and the
09-25..10-04 separator research were removed with the Universal backend. The
last tree that has them is git tag `upmix-universal-last-2026-10-09`; the story
with the numbers is `docs/audio/UPMIX_HISTORY_AND_RESULTS_2026-10-09.md`, the
inventory `docs/audio/UPMIX_CODE_RETIREMENT_2026-10-09.md`.
