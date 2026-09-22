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
  --model models/hssr-selection-v11/hssr_model.bin \
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
  --model models/hssr-selection-v11/hssr_model.bin
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
workload.  The final measured results
are in
[`results/2026-09-07-hssr-selection-v11.md`](results/2026-09-07-hssr-selection-v11.md).
