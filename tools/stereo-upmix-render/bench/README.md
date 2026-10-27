# Scene renderer benches

Two benches measure the C scene renderer the way the hook runs it: host wall
time, or retired ARMv7 instructions on real QNX 6.5 under QEMU `-icount`.
QEMU instructions are a deterministic regression metric, not Krait cycles and
not proof of the vehicle realtime margin; the guard histogram on the unit is
the real number.

| File | What it is |
|---|---|
| `scene_engine_bench.c` | The scene engine (`hook/audio/scene_engine.c`): N seconds (default 60) of a deterministic synthetic stereo signal (two panned decaying plucks over a noise floor) pushed in 768-frame blocks (one QSA period) through `scene_engine_push()`, timed per push. `--mask` installs a synthetic mask row per 512-sample control so the semantic stage and the duck run; `--mask --handoff` routes the row through the scene mask worker's mailboxes (`hook/audio/scene_mask_worker.h`, stepped between pushes with the net stubbed), so its difference against `--mask` is the audio thread's hand-off cost; `--no-push` runs everything but the engine (what to subtract from a whole-run plugin count). Prints frames, the S16 checksum and the front/rear RMS. |
| `scene_mask_bench.c` | The fine mask net runtime (`hook/audio/scene_mask.c`, `scene_mask_control()` per 512-sample control) on the same signal; needs the `SMSKNET1` weight blob (`../port/export_masknet_weights.py`, or `models/scene-masknet-c/scene_masknet_c.bin`). Prints controls and a checksum of the S16-quantised eligibility rows. |
| `scene_build_qnx.sh` | Cross-builds either bench with the hook's flags (`scripts/apac_flags.sh`, `-O2`, `-mfpu=neon` without vfpv4 = no fused multiply-add, softfp, `-Werror`): `--variant latest|4.9|8.5`, `--bench engine|mask`; `SCENE_CFLAGS_EXTRA="-O3 -fno-math-errno"` tries the upmix DSP flags on the engine files. |
| `scene_qnx_qemu_bench.sh` | Runs a bench under QEMU `-icount`: guest `ClockCycles` is calibrated against two exact instruction loops and the program refuses to call the result an instruction count if that calibration disagrees. `--seconds 1..65`, `--mask [--handoff]`, `--no-push`, `--bench mask --weights SMSKNET1.bin` (the blob is shipped into the IFS). |
| `qemu_l1d_plugin.c` | TCG plugin: one L1 data cache (default 16 KiB, 4-way, 64-byte lines, LRU: the APQ8064/Krait geometry), reports accesses and misses (`metric=l1d_cache`). |
| `qemu_fnprof_plugin.c` | TCG plugin: attributes every retired instruction to the function that contains it from an `nm -S` symbol table; `focus=FUNCTION` also counts per address inside one function, which `addr2line` maps to source lines (the QNX build carries `-g`). |
| `results/` | The Universal-era bench results (2026-09-04 … 09-25), kept as history; the scene numbers live in `docs/build-and-deploy/SCENE_BACKEND_DEPLOY.md`. |

## 💻 Host

```sh
cc -O2 -std=gnu99 -ffp-contract=off -Wall -Wextra -Ihook/audio \
   tools/stereo-upmix-render/bench/scene_engine_bench.c hook/audio/scene_engine.c \
   hook/audio/scene_engine_math.c hook/audio/scene_clusters.c \
   hook/audio/scene_mask.c hook/audio/scene_mask_worker.c -lm -lpthread -o build/scene_port/scene_engine_bench
build/scene_port/scene_engine_bench --mask

cc -O2 -std=gnu99 -ffp-contract=off -Wall -Wextra -Ihook/audio \
   tools/stereo-upmix-render/bench/scene_mask_bench.c hook/audio/scene_mask.c -lm -o build/scene_port/scene_mask_bench
build/scene_port/scene_mask_bench --weights models/scene-masknet-c/scene_masknet_c.bin
```

`-ffp-contract=off` because the QNX/ARMv7 build has no FMA.

## ⚙️ QNX under QEMU

```sh
tools/stereo-upmix-render/bench/scene_build_qnx.sh                       # GCC 8.5 (:latest), -O2, NEON, no FMA
tools/stereo-upmix-render/bench/scene_qnx_qemu_bench.sh --mask           # instructions per push
tools/stereo-upmix-render/bench/scene_qnx_qemu_bench.sh --mask --handoff # + the worker hand-off
tools/stereo-upmix-render/bench/scene_qnx_qemu_bench.sh --bench mask \
    --weights models/scene-masknet-c/scene_masknet_c.bin --seconds 30    # the net per control
```

The shipped numbers (engine 0.31–0.34 M instructions per 256-frame hop, net
1.37 M per 512-frame control, hand-off +1.1 k per push) and the checksums to
compare a re-run against are in
`docs/build-and-deploy/SCENE_BACKEND_DEPLOY.md`.

`-icount` counts instructions and models **no cache at all**. For data-cache
questions build the L1D plugin and point the bench at it:

```sh
read -ra GLIB <<< "$(pkg-config --cflags glib-2.0)"
cc -shared -fPIC -O2 -I"$(brew --prefix qemu)/include" "${GLIB[@]}" \
   -Wl,-undefined,dynamic_lookup \
   -o /tmp/libl1d.so tools/stereo-upmix-render/bench/qemu_l1d_plugin.c

BENCH_QEMU_PLUGIN=/tmp/libl1d.so,size=16384,ways=4,line=64 QEMU_KEEP_LOG=/tmp/qemu.log \
tools/stereo-upmix-render/bench/scene_qnx_qemu_bench.sh --seconds 10 --mask
grep metric=l1d_cache /tmp/qemu.log
```

Miss counts are a functional cache model over the whole guest run (boot, signal
generation, accounting), not a timing model and not scoped to one function; the
engine's own accesses are the difference against the same run with `--no-push`.
Use them to compare two builds on the same workload.

**Per-function / per-line profile.** `qemu_fnprof_plugin.c` (built like the
cache plugin) takes the bench binary's symbol table:

```sh
B=build/scene-engine-bench/qnx-latest/scene_engine_bench
nm -S --defined-only $B | grep -E ' [tT] ' > /tmp/syms.txt
BENCH_QEMU_PLUGIN=/tmp/libfnprof.so,symbols=/tmp/syms.txt,focus=scene_engine_frame QEMU_KEEP_LOG=/tmp/qemu.log \
tools/stereo-upmix-render/bench/scene_qnx_qemu_bench.sh --seconds 10 --mask
grep -A 20 metric=instructions_per_function /tmp/qemu.log
grep '^  0x' /tmp/qemu.log | awk '{print $1}' | addr2line -e $B   # per-line counts (binutils addr2line)
```

`scene_build_qnx.sh` writes `build/scene-engine-bench/qnx-<variant>/` or
`build/scene-mask-bench/qnx-<variant>/`. The plugins build against QEMU 11
(plugin API 7) and the older API.

The Universal bench (`stereo_upmix_bench.c`, `build_host.sh`, `build_qnx.sh`,
`qnx_qemu_bench.sh`, the mask self-test) left with the Universal backend on
2026-10-09; git tag `upmix-universal-last-2026-10-09` has it.
