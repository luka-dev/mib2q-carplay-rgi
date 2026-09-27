# HSSR Universal benchmark

The benchmark calls the same `spa_stereo_upmix_process_s16()` used by the hook
with 1024-frame blocks, LFE assist enabled, and the single Universal backend.
It reports caller-owned state size, host wall time and a rolling checksum.
Pass `--async-controller` to measure the production split where only the GRU
runs on its persistent worker.  That mode paces calls at the real 1024-frame
period outside the timed region so controller-deadline statistics are valid.
Use `--realtime-pacing` on the synchronous run for a cache/scheduling-fair
latency comparison.

```sh
tools/stereo-upmix-render/bench/build_host.sh
build/stereo-upmix-bench/host/stereo_upmix_bench \
  --backend hssr-universal \
  --model models/hssr-selection-v11/hssr_guard.bin \
  --async-controller
```

Cross-build with the same ARMv7/QNX flags as the hook:

```sh
tools/stereo-upmix-render/bench/build_qnx.sh --variant 8.5
```

When QEMU assets are installed, the calibrated instruction-retirement run is:

```sh
tools/stereo-upmix-render/bench/qnx_qemu_bench.sh \
  --variant 8.5 --backend hssr-universal \
  --model models/hssr-selection-v11/hssr_guard.bin
```

QEMU retired instructions are a deterministic regression metric, not Krait
cycles or proof of the vehicle realtime margin.

`-icount` counts instructions and models **no cache at all**.  For data-cache
questions build the TCG plugin in [`qemu_l1d_plugin.c`](qemu_l1d_plugin.c) and
point the bench at it; it models one L1 data cache (default 16 KiB, 4-way,
64-byte lines, LRU: the APQ8064/Krait geometry) and reports accesses and misses.

```sh
read -ra GLIB <<< "$(pkg-config --cflags glib-2.0)"
cc -shared -fPIC -O2 -I"$(brew --prefix qemu)/include" "${GLIB[@]}" \
   -Wl,-undefined,dynamic_lookup \
   -o /tmp/libl1d.so tools/stereo-upmix-render/bench/qemu_l1d_plugin.c

BENCH_QEMU_PLUGIN=/tmp/libl1d.so,size=16384,ways=4,line=64 \
QEMU_KEEP_LOG=/tmp/qemu.log \
tools/stereo-upmix-render/bench/qnx_qemu_bench.sh \
  --variant 8.5 --backend hssr-universal --model build/hssr_model.bin \
  --blocks 8 --warmup 2 --async-controller
grep metric=l1d_cache /tmp/qemu.log
```

Miss counts are a functional cache model over the whole guest run, not a timing
model and not scoped to one function; use them to compare two builds on the same
workload.

**Per-function / per-line instruction profile.**  [`qemu_fnprof_plugin.c`](qemu_fnprof_plugin.c)
(built like the cache plugin) attributes every retired instruction to the
function that contains it, from an `nm -S` symbol table of the bench binary
(the 4-column `addr size type name` form), and with `focus=FUNCTION` also
counts per address inside one function, which `addr2line` maps to source
lines (the QNX build carries `-g`):

```sh
B=build/stereo-upmix-bench/qnx-latest/stereo_upmix_bench
nm -S --defined-only $B | grep -E ' [tT] ' > /tmp/syms.txt
BENCH_QEMU_PLUGIN=/tmp/libfnprof.so,symbols=/tmp/syms.txt,focus=spa_mask_infer QEMU_KEEP_LOG=/tmp/qemu.log tools/stereo-upmix-render/bench/qnx_qemu_bench.sh --variant latest   --backend hssr-universal --model models/hssr-q6d-moises-a/hssr_model.bin   --mask-model build/hssr_mask.bin --blocks 8 --warmup 2 --async-controller
grep -A 20 metric=instructions_per_function /tmp/qemu.log
grep '^  0x' /tmp/qemu.log | awk '{print $1}' | addr2line -e $B   # (binutils addr2line) per-line counts
```

Compare against the same run without `--mask-model`: the difference is the
mask net's cost, including what lands in libc/libm ("unattributed").

**NEON parity of the mask net** (`--mask-selftest`): the bench runs
`spa_mask_infer` alone on deterministic synthetic features for 12 controls and
prints every eligibility and presence value; the host build takes the scalar
path, the QEMU build the NEON one, and
[`mask_selftest_compare.py`](mask_selftest_compare.py) reports the largest
difference (expected ~1e-6: the int8 decoder is bit-exact, the float stages
differ by the lane order of the reductions and the reciprocal-estimate divide):

```sh
tools/stereo-upmix-render/bench/build_host.sh
build/stereo-upmix-bench/host/stereo_upmix_bench --backend hssr-universal   --model models/hssr-q6d-moises-a/hssr_model.bin --mask-model build/hssr_mask.bin   --mask-selftest > /tmp/host_selftest.txt
QEMU_KEEP_LOG=/tmp/qemu_selftest.log tools/stereo-upmix-render/bench/qnx_qemu_bench.sh   --variant latest --backend hssr-universal --model models/hssr-q6d-moises-a/hssr_model.bin   --mask-model build/hssr_mask.bin --mask-selftest --blocks 1 --warmup 1
grep '^mask_selftest' /tmp/qemu_selftest.log | tr -d '\r' > /tmp/qemu_selftest.txt
python3 tools/stereo-upmix-render/bench/mask_selftest_compare.py /tmp/host_selftest.txt /tmp/qemu_selftest.txt
```  The final measured results
are in
[`results/2026-09-07-hssr-selection-v11.md`](results/2026-09-07-hssr-selection-v11.md).
